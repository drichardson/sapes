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

#include "server.h"
#include "utility.h"
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>

static bool matchtoken(char** str, const char* strtomatch)
{
	char buf[SMTP_MAX_TEXT_LINE + 1];
	if(!nexttoken(str, buf, sizeof buf))
		return false;

	return strcasecmp(buf, strtomatch) == 0;
}

static bool mailpath(char **ppath, char **local_part, char **domain_part)
{
	if(!ppath)
		return false;

	char* path = *ppath;
	char sender[SMTP_MAX_TEXT_LINE + 1];

	// Get all characters until the next '>'.
	int i;
	for(i = 0; i < SMTP_MAX_TEXT_LINE && path[i] && path[i] != '>'; ++i)
	{
		sender[i] = path[i];
	}

	sender[i] = 0;

	*ppath = path + i;

	// Remove the source routes from the sender address.
	char *mailbox = strrchr(sender, ':');
	if(mailbox == NULL)
		mailbox = sender;
	else
		++mailbox;

	// Make sure the mailbox is syntaxtically correct.
	return mailbox_ok(mailbox, local_part, domain_part);
}


//
// Server
//

Server::Mailbox::Mailbox()
: local_part(0),
domain_part(0)
{
}

Server::Mailbox::~Mailbox()
{
	clear();
}

const char* Server::Mailbox::getLocal() const
{
	return local_part;
}

const char* Server::Mailbox::getDomain() const
{
	return domain_part;
}

void Server::Mailbox::setPath(const char* newLocal, const char* newDomain)
{
	delete[] local_part;
	local_part = strdupnew(newLocal);

	delete[] domain_part;
	domain_part = strdupnew(newDomain);
}

void Server::Mailbox::clear()
{
	delete[] local_part;
	local_part = 0;

	delete[] domain_part;
	domain_part = 0;
}

bool Server::Mailbox::isSet()
{
	return local_part != NULL && domain_part != NULL;
}

Server::ToList::ToList()
: next(0)
{
}

Server::ToList::~ToList()
{
	delete next;
}

bool Server::ToList::isSet()
{
	return this != NULL;
}

Server::Message::Message()
:to(0),
data(0)
{
}

Server::Message::~Message()
{
	reset();
}

void Server::Message::add_recipient(const char* localpart, const char* domainpart)
{
	if(!to)
		to = new ToList();
	else
	{
		ToList *n = new ToList();
		n->next = to;
		to = n;
	}

	to->recipient.setPath(localpart, domainpart);
}

void Server::Message::reset()
{
	from.clear();

	delete[] data;
	delete to;

	data = 0;
	to = 0;
}

Server::Server(SOCKET s, const Accounts & accounts, const Options & options)
: m_sock(s),
m_accounts(accounts),
m_options(options)
{
}

Server::~Server()
{
	m_sock.close();
}

int Server::run()
{
	try
	{
		bool done = false;
		reply(220);

		while(!done)
		{
			char command_line[SMTP_MAX_COMMAND_LENGTH];
			char *pcmd_line = command_line;
			char command[SMTP_MAX_COMMAND_LENGTH];

			if(!m_sock.getLine(pcmd_line, sizeof command_line, NULL))
			{
				reply(500, "Line too long");
				continue;
			}

			if(!nexttoken(&pcmd_line, command, sizeof command))
			{
				reply(500);
				continue;
			}

			if(strcasecmp("HELO", command) == 0)
			{
				helo(pcmd_line);
			}
			else if(strcasecmp("EHLO", command) == 0)
			{
				ehlo(pcmd_line);
			}
			else if(strcasecmp("MAIL", command) == 0)
			{
				mail(pcmd_line);
			}
			else if(strcasecmp("RCPT", command) == 0)
			{
				rcpt(pcmd_line);
			}
			else if(strcasecmp("DATA", command) == 0)
			{
				data(pcmd_line);
			}
			else if(strcasecmp("QUIT", command) == 0)
			{
				reply(221, "%s Service closing transmission channel", "MY DOMAIN HERE!!!");
				done = true;
			}
			else if(strcasecmp("RSET", command) == 0)
			{
				rset(pcmd_line);
			}
			else if(strcasecmp("NOOP", command) == 0)
			{
				reply(250);
			}
			else if(strcasecmp("VRFY", command) == 0)
			{
				vrfy(command);
			}
			else
			{
				reply(500, "Command unknown: '%s'", command);
			}

		}
	}
	catch(SocketError & e)
	{
		m_log.log(LOG_WARN, "Server::run(): SMTP server error: %s", e.errMsg());
		return 1;
	}

	return 0;
}

void Server::helo(char* /*command*/)
{
	reply(250, "Either my machine or my domain");
}

void Server::ehlo(char* /*command*/)
{
	reply(250, "Either my machine or my domain.");
}

void Server::mail(char* command)
{
	if(!matchtoken(&command, "from"))
	{
		reply(501);
		return;
	}

	if(!matchtoken(&command, ":"))
	{
		reply(501);
		return;
	}

	if(!matchtoken(&command, "<"))
	{
		reply(501);
		return;
	}

	char *local_part, *domain_part;

	if(!mailpath(&command, &local_part, &domain_part))
	{
		reply(553);
		return;
	}

	if(!matchtoken(&command, ">"))
	{
		delete[] local_part;
		delete[] domain_part;
		reply(501);
		return;
	}
	
	m_message.from.setPath(local_part, domain_part);

	delete[] local_part;
	delete[] domain_part;

	reply(250);
}

void Server::rcpt(char* command)
{
	if(!matchtoken(&command, "to"))
	{
		reply(501);
		return;
	}

	if(!matchtoken(&command, ":"))
	{
		reply(501);
		return;
	}

	if(!matchtoken(&command, "<"))
	{
		reply(501);
		return;
	}

	char *local_part, *domain_part;
	if(!mailpath(&command, &local_part, &domain_part))
	{
		reply(553);
		return;
	}

	if(!matchtoken(&command, ">"))
	{
		delete[] local_part;
		delete[] domain_part;
		reply(501);
		return;
	}

	switch(m_accounts.isMailboxOk(domain_part, local_part))
	{
	case MS_OK:
	case MS_DOMAIN_NOT_LOCAL:
		break;

	case MS_MAILBOX_NOT_FOUND:
		delete[] local_part;
		delete[] domain_part;
		reply(550);
		return;
	}

	m_message.add_recipient(local_part, domain_part);

	delete[] local_part;
	delete[] domain_part;

	reply(250);
}

void Server::data(char* /*command*/)
{
	if(!m_message.from.isSet())
	{
		reply(503);
		return;
	}

	if(!m_message.to->isSet())
	{
		reply(554, "No valid recipients");
		return;
	}

	reply(354);

	// Name this file NEWxxx so that the sender thread doesn't pick it
	// up until we have completely written it. When we have completed,
	// we will rename it to MSGxxx.
	char NEW_PREFIX[] = { DIR_DELIM, 'N', 'E', 'W', '\0' };
	size_t new_filename_len = 0;
	char *filename = 0;
	char *new_filename = 0;
	FILE *fp = newfile(m_options.sendDir(), "NEW", &filename);

	if(!fp)
	{
		reply(452);
		return;
	}

	int rc = fprintf(fp, "MAILSERV SENDER FILE%s", CRLF);
	if(rc >= 0)
		rc = fprintf(fp, "%s%s", m_message.from.getLocal(), CRLF);
	if(rc >= 0)
		rc = fprintf(fp, "%s%s", m_message.from.getDomain(), CRLF);
	if(rc >= 0)
	{
		for(ToList *p = m_message.to; p && rc >= 0; p = p->next)
		{
			rc = fprintf(fp, "%s%s", p->recipient.getLocal(), CRLF);
			if(rc >= 0)
				rc = fprintf(fp, "%s%s", p->recipient.getDomain(), CRLF);
		}
	}

	if(rc >= 0)
		rc = fprintf(fp, "<END>%s", CRLF);

	if(rc < 0)
	{
		m_log.log(LOG_SERVER, "Server::data(): Error writing header to send file.");
		reply(452);
		goto cleanup;
	}

	try
	{
		char buf[SMTP_MAX_TEXT_LINE];
		bool got_line;
		unsigned int len;

		while((got_line = m_sock.getLine(buf, sizeof buf, &len)) != false && strcmp(buf, ".") != 0)
		{
			if(fwrite(buf, 1, len, fp) != len ||
				fwrite(CRLF, 1, 2, fp) != 2)
			{
				reply(452);
				m_log.log(LOG_SERVER, "Server::data(): Error writing user data to send file.");
				goto cleanup;
			}
		}

		if(!got_line)
		{
			reply(500);
			m_log.log(LOG_SERVER, "Server::data(): Error while receiving DATA from client.");
			goto cleanup;
		}
	}
	catch(...)
	{
		fclose(fp);
		unlink(filename);
		delete[] filename;
		throw;
	}

	if(fprintf(fp, ".%s", CRLF) < 0)
	{
		m_log.log(LOG_SERVER, "Server::data(): Error writing '.' terminator to send file.");
		reply(452);
		goto cleanup;
	}

	fclose(fp);

	// Rename the file to have a MSG prefix instead of a NEW prefix. This indicates
	// that the file is completely written out.
	new_filename = strdupnew(filename);
	new_filename_len = strlen(new_filename);

	while(new_filename_len--)
	{
		if(strncmp(NEW_PREFIX, new_filename + new_filename_len, 4) == 0)
		{
			memcpy(new_filename + new_filename_len, "/MSG", 4);
			if(rename(filename, new_filename) != 0)
			{
				reply(452, "Error renaming file.");
				goto cleanup;
			}
		}
	}

	delete[] filename;

	reply(250);
	return;

cleanup:
	if(fp)
		fclose(fp);
	if(filename)
		unlink(filename);
	delete[] filename;
}

void Server::rset(char* /*command*/)
{
	m_message.reset();
	reply(250);
}

void Server::vrfy(char* /*command*/)
{
	reply(502);
}

void Server::reply(short code, const char* format, ...)
{
	// Send the reply code
	assert(code >= 0 && code < 1000);
	char buf[5];
	safe_snprintf(buf, sizeof buf, "%d ", code);
	m_sock.send(buf, 4);

	// Send the message
	if(format)
	{
		char buf[SMTP_MAX_REPLY_LENGTH];
		va_list va;
		va_start(va, format);
		safe_vsnprintf(buf, sizeof buf, format, va);
		va_end(va);

		// Make sure buf is NULL terminated.
		buf[sizeof(buf) - 1] = 0;

		int len = strlen(buf);
		m_sock.send(buf, len);
	}
	else
	{
		char *msg = 0;

		switch(code)
		{
		case 211:
			msg = "System status, or system help reply";
			break;
		case 214:
			msg = "Help message";
			break;
		case 220:
			msg = "<domain> Service ready";
			break;
		case 221:
			msg = "<domain> Service closing transmission channel";
			break;
		case 250:
			msg = "Requested mail action okay, completed";
			break;
		case 251:
			msg = "User not local; will forward to <forward-path>";
			break;
		case 252:
			msg = "Cannot VRFY user, but will accept message and attempt delivery";
			break;
		case 354:
			msg = "Start mail input; end with <CRLF>.<CRLF>";
			break;
		case 421:
			msg = "<domain> Service not available, closing transmission channel";
			break;
		case 450:
			msg = "Requested mail action not taken: mailbox unavailable";
			break;
		case 451:
			msg = "Requested action aborted: local error in processing";
			break;
		case 452:
			msg = "Requested action not taken: insufficient system storage";
			break;
		case 500:
			msg = "Syntax error, command unrecognized";
			break;
		case 501:
			msg = "Syntax error in parameters or arguments";
			break;
		case 502:
			msg = "Command not implemented";
			break;
		case 503:
			msg = "Bad sequence of commands";
			break;
		case 504:
			msg = "Command parameter not implemented";
			break;
		case 550:
			msg = "Requested action not taken: mailbox unavailable";
			break;
		case 551:
			msg = "User not local; please try <forward-path>";
			break;
		case 552:
			msg = "Requested mail action aborted: exceeded storage allocation";
			break;
		case 553:
			msg = "Requested action not taken: mailbox name not allowed";
			break;
		case 554:
			msg = "Transaction failed";
			break;
		}

		if(msg)
			m_sock.send(msg, strlen(msg));
	}

	m_sock.send(CRLF, 2);
}

