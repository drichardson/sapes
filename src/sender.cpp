/*
 * Copyright (c) 2003, Douglas Ryan Richardson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * * Neither the name of the organization nor the names of its contributors may
 *   be used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "utility.h"
#include "sender.h"
#include "dns_resolve.h"

#include <stdio.h>
#include <stdlib.h>

#ifndef WIN32
#include <glob.h>
#include <sys/stat.h>
#endif

Sender::Mailbox::Mailbox(Mailbox *newNext, const char* newUser, const char* newDomain)
: next(newNext),
nextRemote(0),
failed(false)
{
	user = strdupnew(newUser);
	domain = strdupnew(newDomain);
}

Sender::Mailbox::~Mailbox()
{
	delete[] user;
	delete[] domain;
	delete next;
}


Sender::FileList::FileList(Sender::FileList *newNext, const char* newFilename)
: next(newNext)
{
	filename = strdupnew(newFilename);
}

Sender::FileList::~FileList()
{
	delete[] filename;
	delete next;
}

Sender::Sender(const Options & options, const Accounts & accounts)
: m_options(options),
m_accounts(accounts),
m_bFileListSemCreated(false),
m_bFileListMutexCreated(false),
m_bFileListEmptySemCreated(false),
m_pfiles(0)
{
}

Sender::~Sender()
{
	delete_semaphore(m_fileListSemaphore);
	delete_mutex(m_fileListMutex);
	delete_semaphore(m_fileListEmptySemaphore);

	delete m_pfiles;
}

THREAD_RETTYPE WINAPI Sender::thread_routine(void* pData)
{
	Sender *pThis = (Sender*)pData;

	while(pThis->m_run)
	{
		if(wait_semaphore(pThis->m_fileListSemaphore))
		{
			if(wait_mutex(pThis->m_fileListMutex))
			{
				if(pThis->m_pfiles)
				{
					FileList *p = pThis->m_pfiles;
					pThis->m_pfiles = pThis->m_pfiles->next;

					p->next = 0; // Remove this node from the list so that we can delete it
								// without deleting all the nodes.

					release_mutex(pThis->m_fileListMutex);

					pThis->process_file(p->filename);

					// If this is the last file then signal the thread that
					// builds the file list. You must signal after you process the file,
					// otherwise the file may be processed again when the file list builder
					// re-adds it to the list of files to process. This will cause the other
					// threads to sit around until this one finishes.
					if(pThis->m_pfiles == 0)
						signal_semaphore(pThis->m_fileListEmptySemaphore);
					delete p;
				}
				else
				{
					pThis->m_log.log("Sender_thread_routine: Error: m_pfiles is an empty list but the semaphore indicates it shouldn't be.");
					release_mutex(pThis->m_fileListMutex);
				}
			}
		}
	}
	
	return 0;
}

// Retrieves a line from the sender file but first strips off the CRLF.
static bool getLine(FILE *fp, char *buf, const size_t MAXBUFLEN)
{
	int c;
	int last = 0;
	size_t i = 0;

	while((c = getc(fp)) != EOF && i < MAXBUFLEN)
	{
		if(last == CR && c == LF)
		{
			buf[i - 1] = 0;
			return true;
		}

		buf[i] = (char)c;
		++i;

		last = c;
	}

	return false;
}

void Sender::process_file(const char* filename)
{
	Mailbox *from = 0;
	Mailbox *to = 0;
	Mailbox *p = 0;
	Mailbox *remote = 0;
	bool incomplete = false;

	FILE *fp = fopen(filename, "rb");
	char line[SMTP_MAX_TEXT_LINE];
	long pos, endpos;

	if(!fp)
	{
		m_log.log("Sender_process_file: Error opening '%s'", filename);
		goto error;
	}

	// Make sure this is a sender file.
	if(!getLine(fp, line, sizeof line))
	{
		m_log.log("Sender_process_file: Error reading sender file header string from '%s'", filename);
			goto error;
	}

	if(strcmp(line, "MAILSERV SENDER FILE") != 0)
	{
		m_log.log("Sender_process_file: Error: First line of '%s' was not MAILSERV SENDER FILE", filename);
		goto error;
	}

	pos = ftell(fp); // Save the current position.

	// Make sure the file ends with <CRLF>.<CRLF>. Otherwise it may not have been completly written.
	if(fseek(fp, -5, SEEK_END) != 0)
	{
		m_log.log("Sender_process_file: Error seeking to end of '%s'.", filename);
		goto error;
	}

	endpos = ftell(fp);

	if(fread(line, 1, 5, fp) != 5)
	{
		m_log.log("Sender_process_file: Error: Sender file '%s' does not end in <CRLF>.<CRLF>", filename);
		incomplete = true;
		goto error;
	}

	if(!(line[0] == CR && line[1] == LF && line[2] == '.' && line[3] == CR && line[4] == LF))
	{
		m_log.log("Sender_process_file: Error: Sender file '%s' does not end in <CRLF>.<CRLF>", filename);
		goto error;
	}

	if(fseek(fp, pos, SEEK_SET))
	{
		// Move to the saved position (right after MAILSERV SENDER FILE).
		m_log.log("Sender_process_file: Error seeking to position after MAILSERV SEDNER FILE in '%s'", filename);
		goto error;
	}

	// Get the from and to mailboxes.
	while(getLine(fp, line, sizeof line) && strcmp(line, "<END>") != 0)
	{
		char domain[SMTP_MAX_TEXT_LINE];
		if(!getLine(fp, domain, sizeof domain))
		{
			m_log.log("Sender_process_file: Error getting the domain for '%s'", line);
			goto error;
		}

		if(strcmp(domain, "<END>") == 0)
		{
			m_log.log("Sender_process_file: Expecting domain for user '%s' but got <END>", line);
			goto error;
		}

		// The first mailbox is the from mailbox. After the from are the to mailboxes.
		if(!from)
			from = new Mailbox(NULL, line, domain);
		else
			to = new Mailbox(to, line, domain);
	}

	if(!from)
	{
		m_log.log("Sender_process_file: Error: No from in sender file '%s'", filename);
		goto error;
	}

	if(!to)
	{
		m_log.log("Sender_process_file: Error: No to list in sender file '%s'", filename);
		goto error;
	}

	pos = ftell(fp); // Save the current position because it is where the message data starts.

	// Put the message data in local mailboxes first, since this is a fast operation.
	for(p = to; p; p = p->next)
	{
		char* mailbox_dir = NULL;
		switch(m_accounts.isMailboxOk(p->domain, p->user, &mailbox_dir))
		{
		case MS_MAILBOX_NOT_FOUND:
			p->failed = true; // If it is local but not found then mark it as an error.
			break;
		case MS_DOMAIN_NOT_LOCAL:
			if(remote)
				remote = remote->nextRemote = p;
			else
				remote = p;
			break;
		case MS_OK:
			if(!copyMessageToLocalMailbox(fp, endpos, mailbox_dir))
				p->failed = true;
			
			fseek(fp, pos, SEEK_SET);
			break;
		}

		delete[] mailbox_dir;
	}

	// Send the message data to remote mailboxes.
	for(p = remote; p; p = p->next)
	{
		REASON_FAILED reason = RF_UNKNOWN;

		if(!sendMessageToRemoteMailbox(fp, from, p, reason))
		{
			p->failed = true;
			// If we couldn't send the message then bounce the message back to
			// the sender. If we can't do this then log the fact and don't do anything
			// else, because we do not want to get into a loop where we keep bouncing
			// the message.
			fseek(fp, pos, SEEK_SET);
			if(!sendBounceMessage(fp, from, p, reason))
			{
				m_log.log("Sender_process_file: Error sending bounce message to %s@%s (couldn't send to %s@%s)",
					from->user, from->domain, p->user, p->domain);
			}
		}

		fseek(fp, pos, SEEK_SET);
	}


	fclose(fp);
	unlink(filename);
	delete to;
	delete from;
	return;

error:
	if(fp)
		fclose(fp);
	delete to;
	delete from;

	// Don't delete the file if it is incomplete because it may be in 
	// the process of being written to.
	if(!incomplete)
		unlink(filename);
}

bool Sender::copyMessageToLocalMailbox(FILE* fp_sender, long endpos, const char* mailbox_dir) const
{
	// BUFLEN is the size "chunk" we read and write to files. The bigger it is
	// the less times we go to disk.
	const size_t BUFLEN = 30000;
	long bytesRead;
	char buf[BUFLEN];
	bool retval = false;
	bool done = false;

	char *filename = NULL;
	FILE *fp = newfile(mailbox_dir, "NEW", &filename);

	if(!fp)
	{
		m_log.log("Sender_copyMessageToLocalMailbox: Could not create file in mailbox '%s'", mailbox_dir);
		return false;
	}

	while((bytesRead = fread(buf, 1, BUFLEN, fp_sender)) > 0)
	{
		long curpos = ftell(fp_sender);
		if(curpos >= endpos)
		{
			bytesRead -= (curpos - endpos);
			done = true;
		}

		if(bytesRead > 0)
		{
			if(fwrite(buf, 1, bytesRead, fp) != (size_t)bytesRead)
			{
				m_log.log("Sender_copyMessageToLocalMailbox: Error writing to '%s'", filename);
				retval = false;
				break;
			}
		}

		if((size_t)bytesRead != BUFLEN || done)
			break;
	}

	if(ferror(fp_sender))
		m_log.log("Sender_copyMessageToLocalMailbox: Error while reading from sender file.");

	if(ferror(fp))
		m_log.log("Sender_copyMessageToLocalMailbox: Error while writing to '%s'", filename);

	fclose(fp);

	// Rename the file to have a MSG prefix instead of a NEW prefix. This indicates
	// that the file is completely written out.
	char *new_filename = strdupnew(filename);
	char NEW_PREFIX[] = { DIR_DELIM, 'N', 'E', 'W', '\0' };
	size_t len = strlen(new_filename);

	while(!retval && len--)
	{
		if(strncmp(NEW_PREFIX, new_filename + len, 4) == 0)
		{
			memcpy(new_filename + len, "/MSG", 4);
			retval = rename(filename, new_filename) == 0;
			break;
		}
	}

	delete[] filename;
	delete[] new_filename;

	return retval;
}

bool Sender::sendMessageToRemoteMailbox(FILE* fp,
										const Mailbox* from,
										const Mailbox* to,
										REASON_FAILED & reason) const
{
	// Lookup the MX entry for the domain to find the address of the SMTP server.
	char *exchanger = 0;
	if(!dns_resolve_mx_to_addr(to->domain, &exchanger))
	{
		reason = RF_HOST_NOT_FOUND;
		m_log.log("Sender_sendMessageToRemoteMailbox: Could not get MX (mail exchanger) record for '%s'", to->domain);
		return false;
	}

	SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(25);
	in_addr addr_buf;
	in_addr *paddr = atoaddr(exchanger, &addr_buf);

	if(!paddr)
	{
		reason = RF_HOST_NOT_FOUND;
		m_log.log("Sender_sendMessageToRemoteMailbox: Could not get address for '%s'", exchanger);
		delete[] exchanger;
		return false;
	}

	addr.sin_addr = *paddr;

	if(connect(s, (sockaddr*)&addr, sizeof addr) != 0)
	{
		reason = RF_COULD_NOT_CONNECT_TO_HOST;
		m_log.log("Sender_sendMessageToRemoteMailbox: Could not connect to '%s'", exchanger);
		delete[] exchanger;
		return false;
	}

	delete[] exchanger;

	Socket sock(s);

	bool retval = sendMessage(sock, fp, from, to, reason);

	sock.close();

	return retval;
}

// This function sends "bounce" RFC 3462 formatted message.
FILE* Sender::createBounceMessage(FILE *fp_original_message,
								  char **pFilename,
								  const Mailbox *from,
								  const Mailbox *unreachable,
								  REASON_FAILED reason)
								  const
{
	char datetime[200];
	char *filename = NULL;
	const char* boundary = "===========================_ _= 4183769(29875)5809016839";

	int c;
	long endpos;
	long startpos;

	startpos = ftell(fp_original_message);

	if(startpos == -1)
		return NULL;

	if(fseek(fp_original_message, 0, SEEK_END) != 0)
		return NULL;

	endpos = ftell(fp_original_message);

	if(endpos == -1)
		return NULL;

	endpos -= 5; // Move back past the last <CRLF>.<CRLF>.

	if(endpos < 0)
		return NULL;

	if(fseek(fp_original_message, startpos, SEEK_SET) != 0)
		return NULL;

	if(!get_rfc_2822_datetime(time(NULL), datetime, sizeof(datetime)))
		return NULL;

	FILE *fp = newfile(m_options.sendDir(), "BNC", &filename);

	if(!fp)
		return NULL;

	// Write out the header.
	if(fprintf(fp, "From: \"Mail Administrator\" <postmaster@%s>%s",
		from->domain, CRLF) < 0)
		goto write_error;

	if(fprintf(fp, "To: %s@%s%s", from->user, from->domain, CRLF) < 0)
		goto write_error;

	if(fprintf(fp, "Subject: Mail System Error - Returned Mail%s", CRLF) < 0)
		goto write_error;

	if(fprintf(fp, "Date: %s%s", datetime, CRLF) < 0)
		goto write_error;

	if(fprintf(fp, "MIME-Version: 1.0%s", CRLF) < 0)
		goto write_error;

	if(fprintf(fp, "Content-Type: multipart/report;%s\treport-type=delivery-status;%s\tBoundary=\"%s\"%s%s%s",
		CRLF, CRLF, boundary, CRLF, CRLF, CRLF) < 0)
		goto write_error;

	// Write out the error message.
	if(fprintf(fp, "--%s%s", boundary, CRLF) < 0)
		goto write_error;

	if(fprintf(fp, "Content-type: text/plain%s%s", CRLF, CRLF) < 0)
		goto write_error;

	if(fprintf(fp, "This Message was undeliverable due to the following reason:%s%s", CRLF, CRLF) < 0)
		goto write_error;

	switch(reason)
	{
	case RF_MAILBOX_NOT_FOUND:
		if(fprintf(fp,
			"Your message was not delivered because the destination mailbox%s" \
			"was not found.%s%s", CRLF, CRLF, CRLF) < 0)
			goto write_error;
		break;

	case RF_HOST_NOT_FOUND:
		if(fprintf(fp,
			"Your message was not delivered because the destination computer%s" \
			"was not found.%s" \
			"%s" \
			"It is also possible that a network problem caused this situation,%s"\
			"so if you are sure that the address is correct then try to send%s" \
			"the message again.%s%s\tHost %s not found%s%s",
			CRLF, CRLF, CRLF, CRLF, CRLF, CRLF, CRLF, unreachable->domain, CRLF, CRLF) < 0)
			goto write_error;
		break;

	case RF_COULD_NOT_CONNECT_TO_HOST:
		if(fprintf(fp,
			"Your message was not delivered because the destination computer%s" \
			"could not be reached.%s" \
			"%s" \
			"It is also possible that a network problem caused this situation,%s"\
			"so you might want to send this message again.%s%s\tCould not connect host %s%s%s",
			CRLF, CRLF, CRLF, CRLF, CRLF, CRLF, unreachable->domain, CRLF, CRLF) < 0)
			goto write_error;
		break;

	case RF_REJECTED_MAIL_FROM:
		if(fprintf(fp,
			"Your message was rejected by %s%s%s", unreachable->domain, CRLF, CRLF) < 0)
			goto write_error;
		break;

	case RF_UNKNOWN:
		if(fprintf(fp,
			"Your message could not be delivered.%s%s", CRLF, CRLF) < 0)
			goto write_error;
		break;
	}

	if(fprintf(fp,
		"The following recipient did not receive this message:%s" \
		"%s" \
		"\t<%s@%s>%s" \
		"%s" \
		"Please reply to Postmaster@%s%s" \
		"if you believe this message to be in error.%s%s",
		CRLF, CRLF, unreachable->user, unreachable->domain, CRLF,
		CRLF, from->domain, CRLF, CRLF, CRLF) < 0)
		goto write_error;

	// Write the delivery status
	if(fprintf(fp, "--%s%sContent-Type: message/delivery-status%s%s",
		boundary, CRLF, CRLF, CRLF) < 0)
		goto write_error;

	char hostname[MAX_HOSTNAME_LEN + 1];
	gethostname(hostname, MAX_HOSTNAME_LEN);
	if(fprintf(fp, "Reporting-MTA: dns; %s%s", hostname, CRLF) < 0)
		goto write_error;

	if(fprintf(fp, "Arrival-Date: %s%s", datetime, CRLF) < 0)
		goto write_error;

	if(fprintf(fp, "Received-From-MTA: dns; %s (%s)%s%s",
		"HOSTNAME GOES HERE", "IP GOES HERE", CRLF, CRLF) < 0)
		goto write_error;

	if(fprintf(fp, "Final-Recipient: RFC822; <%s@%s>%s",
		unreachable->user, unreachable->domain, CRLF) < 0)
		goto write_error;

	if(fprintf(fp, "Action: failed%sStatus: 5.1.2%sRemote-MTA: dns; %s%s%s",
		CRLF, CRLF, unreachable->domain, CRLF, CRLF) < 0)
		goto write_error;
	
	// Write the message that couldn't be delivered.
	if(fprintf(fp, "--%s%sContent-Type: message/rfc822%s%s",
		boundary, CRLF, CRLF, CRLF) < 0)
		goto write_error;

	while(ftell(fp_original_message) <= endpos && (c = getc(fp_original_message)) != EOF)
	{
		if(putc(c, fp) != c)
			goto write_error;
	}
	
	// End in <CRLF>.<CRLF> since this will be sent by the send message routine.
	if(fprintf(fp, "%s--%s%s.%s", CRLF, boundary, CRLF, CRLF) < 0)
		goto write_error;

	fclose(fp);

	fp = fopen(filename, "rb");

	*pFilename = filename;
	return fp;

write_error:
	if(fp)
		fclose(fp);
	if(filename)
		unlink(filename);
	return NULL;
}

bool Sender::sendBounceMessage(FILE *fp_og_message,
							   const Mailbox *from,
							   const Mailbox *unreachable,
							   REASON_FAILED reason)
							   const
{
	// Create the bounce message.
	char *filename = NULL;
	FILE *fp_bounce_message = createBounceMessage(fp_og_message, &filename, from, unreachable, reason);
	if(!fp_bounce_message)
		return false;

	Mailbox postmaster(NULL, "Postmaster", from->domain);

	REASON_FAILED dont_care;
	if(!sendMessageToRemoteMailbox(fp_bounce_message, &postmaster, from, dont_care))
		goto failed;

	fclose(fp_bounce_message);
	unlink(filename);
	delete[] filename;
	return true;

failed:
	if(fp_bounce_message)
		fclose(fp_bounce_message);
	if(filename)
	{
		unlink(filename);
		delete[] filename;
	}

	return false;
}

bool Sender::sendMessage(Socket & s, FILE *fp, const Mailbox* from, const Mailbox* to, REASON_FAILED &reason) const
{
	try
	{
		char command[SMTP_MAX_TEXT_LINE];

		if(!s.getLine(command, sizeof command, NULL))
			return false;

		if(atoi(command) != 220)
			return false;

		s.putLine("HELO");
		if(!s.getLine(command, sizeof command, NULL))
			return false;

		if(atoi(command) != 250)
			return false;

		safe_snprintf(command, sizeof command, "MAIL FROM: <%s@%s>", from->user, from->domain);
		s.putLine(command);

		if(!s.getLine(command, sizeof command, NULL))
			return false;

		if(atoi(command) != 250)
		{
			reason = RF_REJECTED_MAIL_FROM;
			return false;
		}

		safe_snprintf(command, sizeof command, "RCPT TO: <%s@%s>", to->user, to->domain);
		s.putLine(command);

		if(!s.getLine(command, sizeof command, NULL))
		{
			reason = RF_MAILBOX_NOT_FOUND;
			return false;
		}

		if(atoi(command) != 250)
			return false;

		s.putLine("DATA");
		
		if(!s.getLine(command, sizeof command, NULL))
			return false;

		if(atoi(command) != 354)
			return false;

		// BUFLEN is the size "chunk" we read from files. The bigger it is
		// the less times we go to disk.
		const size_t BUFLEN = 30000;
		char readBuf[BUFLEN];
		size_t bytesRead;

		while((bytesRead = fread(readBuf, 1, sizeof(readBuf), fp)) > 0)
			s.send(readBuf, bytesRead);

		if(!s.getLine(command, sizeof command, NULL))
			return false;

		if(atoi(command) != 250)
			return false;

		s.putLine("QUIT");

		if(!s.getLine(command, sizeof command, NULL))
			return false;

		if(atoi(command) != 221)
			return false;

	}
	catch(SocketError & e)
	{
		m_log.log("Sender_sendMessageToRemoteMailbox: Socket error while sending message: %s", e.errMsg());
		return false;
	}

	return true;
}

void Sender::Run()
{
	// Reinitialize the semaphore and mutex for this Run.
	if(m_bFileListSemCreated)
	{
		delete_semaphore(m_fileListSemaphore);
		m_bFileListSemCreated = false;
	}

	if(m_bFileListMutexCreated)
	{
		delete_mutex(m_fileListMutex);
		m_bFileListMutexCreated = false;
	}

	if(m_bFileListEmptySemCreated)
	{
		delete_semaphore(m_fileListEmptySemaphore);
		m_bFileListEmptySemCreated = false;
	}

	if(create_semaphore(m_fileListSemaphore))
		m_bFileListSemCreated = true;
	else
	{
		m_log.log("Sender_Run: Could not create file list semaphore. Sender exiting.");
		return;
	}

	if(create_mutex(m_fileListMutex))
		m_bFileListMutexCreated = true;
	else
	{
		m_log.log("Sender_Run: Could not create file list mutex. Sender exiting.");
		return;
	}

	if(create_semaphore(m_fileListEmptySemaphore))
		m_bFileListEmptySemCreated = true;
	else
	{
		m_log.log("Sender_Run: Could not create file list empty semaphore. Sender exiting.");
		return;
	}
	
	// Start the Run.
	m_run = true;
	signal_semaphore(m_fileListEmptySemaphore);

	unsigned int count = m_options.senderThreads();
	for(unsigned int i = 0; i < count; ++i)
	{
		if(!create_thread(thread_routine, this))
			m_log.log("Sender_Run: Error creating sending thread #%u", i);
	}

	while(wait_semaphore(m_fileListEmptySemaphore))
	{
		// When the list is empty scan the directory every 5 seconds
		// for new e-mails.
		while(m_run && !build_list())
			sleep(m_options.scanInterval());

		if(!m_run)
			break;
	}
}

void Sender::Stop()
{
	m_run = false;
	signal_semaphore(m_fileListEmptySemaphore);
}

#ifndef WIN32
static int glob_err_func(const char* filename, int error_code)
{
	fprintf(stderr, "Glob error: filename: %s, error code: %d\n", filename, error_code);
	return 0;
}
#endif

bool Sender::build_list()
{
	if(!wait_mutex(m_fileListMutex))
	{
		m_log.log("Sender_build_list: Error while waiting for file list mutex.");
		return false;
	}

	delete m_pfiles;
	m_pfiles = 0;

#ifdef WIN32
	WIN32_FIND_DATA findData;
	char buf[MAX_PATH + 1];

	safe_snprintf(buf, sizeof buf, "%s/MSG*", m_options.sendDir());
	HANDLE h = FindFirstFile(buf, &findData);
	if(h != INVALID_HANDLE_VALUE)
	{
		do
		{
			// Make sure this entry isn't a directory.
			if(findData.dwFileAttributes ^ FILE_ATTRIBUTE_DIRECTORY)
			{
				safe_snprintf(buf, sizeof buf, "%s%c%s",
					m_options.sendDir(), DIR_DELIM, findData.cFileName);
				m_pfiles = new FileList(m_pfiles, buf);
				signal_semaphore(m_fileListSemaphore);
			}
		} while(FindNextFile(h, &findData));

		FindClose(h);
	}
#else
	glob_t g;
	char buf[MAX_PATH + 1];
	
	memset(&g, 0, sizeof(g));
	safe_snprintf(buf, sizeof buf, "%s/MSG*", m_options.sendDir());

	if(glob(buf, 0, glob_err_func, &g) == 0)
	{
		for(size_t i = 0; i < g.gl_pathc; ++i)
		{
			realpath(g.gl_pathv[i], buf);
			struct stat s;
			if(stat(buf, &s) == 0 && !S_ISDIR(s.st_mode))
			{
				m_pfiles = new FileList(m_pfiles, buf);
				signal_semaphore(m_fileListSemaphore);
			}
		}
	}

	globfree(&g);
#endif

	bool rc = m_pfiles != NULL;

	release_mutex(m_fileListMutex);

	return rc;
}
