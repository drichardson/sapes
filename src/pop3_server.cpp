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

#include "pop3_server.h"
#include "utility.h"
#include "config_file.h"

#ifndef WIN32
#include <glob.h>
#include <sys/stat.h>
#include <stdlib.h>
#endif

//
// MessageInfoArray
//

MessageInfoArray::MessageInfoArray()
: m_totalSize(0),
m_growBy(100),
m_count(0),
m_maxcount(0),
m_msginfo(NULL)
{
	m_bogus.bDelete = false;
	safe_strcpy(m_bogus.filename, "THIS IS A BOGUS FILE NAME", sizeof(m_bogus.filename));
	m_bogus.filesize = 0;
}

MessageInfoArray::~MessageInfoArray()
{
	delete[] m_msginfo;
}

size_t MessageInfoArray::getTotalSize() const
{
	return m_totalSize;
}

size_t MessageInfoArray::getCount() const
{
	return m_count;
}

MessageInfo & MessageInfoArray::getAt(size_t index)
{
	if(index >= m_count)
		return m_bogus;

	return m_msginfo[index];
}

#ifndef WIN32
static int glob_err_func(const char* filename, int error_code)
{
  fprintf(stderr, "Glob error: filename: %s, error code: %d\n", filename, error_code);
  return 0;
}
#endif

bool MessageInfoArray::build_list(const char* mailbox_dir)
{
	m_count = 0;
	m_totalSize = 0;

#ifdef WIN32
	WIN32_FIND_DATA findData;
	char buf[MAX_PATH + 1];

	safe_snprintf(buf, sizeof buf, "%s/MSG*", mailbox_dir);
	HANDLE h = FindFirstFile(buf, &findData);
	if(h != INVALID_HANDLE_VALUE)
	{
		do
		{
			// Make sure this entry isn't a directory.
			if(findData.dwFileAttributes ^ FILE_ATTRIBUTE_DIRECTORY)
			{
				safe_snprintf(buf, sizeof buf, "%s/%s", mailbox_dir, findData.cFileName);

				if(m_count >= m_maxcount)
				{
					m_maxcount += m_growBy;
					MessageInfo *p = new MessageInfo[m_maxcount];
					memcpy(p, m_msginfo, m_count * sizeof(MessageInfo));
					delete[] m_msginfo;
					m_msginfo = p;
				}

				m_msginfo[m_count].bDelete = false;
				// Actual is file size is (nFileSizeHigh * MAXDWORD) + nFileSizeLow,
				// but that is inconvienient to work with.
				m_msginfo[m_count].filesize = findData.nFileSizeLow;
				safe_strcpy(m_msginfo[m_count].filename, buf, sizeof(m_msginfo[m_count].filename));

				m_totalSize += m_msginfo[m_count].filesize;
				++m_count;
			}
		} while(FindNextFile(h, &findData));

		FindClose(h);
	}
#else
	glob_t g;
	char buf[MAX_PATH + 1];
	safe_snprintf(buf, sizeof buf, "%s/MSG*", mailbox_dir);
	memset(&g, 0, sizeof(g));	

	if(glob(buf, 0, glob_err_func, &g) != 0)
		return false;

	for(size_t i = 0; i < g.gl_pathc; ++i)
	{
		struct stat s;
		if(stat(g.gl_pathv[i], &s) == 0 && S_ISREG(s.st_mode))
		{
			realpath(g.gl_pathv[i], buf);

			if(m_count >= m_maxcount)
			{
				m_maxcount += m_growBy;
				MessageInfo *p = new MessageInfo[m_maxcount];
				memcpy(p, m_msginfo, m_count * sizeof(MessageInfo));
				delete[] m_msginfo;
				m_msginfo = p;				
			}

			m_msginfo[m_count].bDelete = false;
			m_msginfo[m_count].filesize = s.st_size;
			safe_strcpy(m_msginfo[m_count].filename, buf, sizeof(m_msginfo[m_count].filename));
		}
	}
	
	globfree(&g);
#endif

	return true;
}


//
// User
//

Pop3Server::User::User()
: user(0),
domain(0)
{
}

Pop3Server::User::~User()
{
	clear();
}


const char* Pop3Server::User::getUser() const
{
	return user;
}

void Pop3Server::User::setUser(const char* newUser)
{
	delete[] user;
	user = strdupnew(newUser);
}

const char* Pop3Server::User::getDomain() const
{
	return domain;
}

void Pop3Server::User::setDomain(const char* newDomain)
{
	delete[] domain;
	domain = strdupnew(newDomain);
}

void Pop3Server::User::clear()
{
	delete[] user;
	user = 0;

	delete[] domain;
	domain = 0;
}

bool Pop3Server::User::isSet() const
{
	return domain && user;
}

//
// Pop3Server
//

Pop3Server::Pop3Server(SOCKET sock,
					   Accounts & accounts,
					   const Options & options)
					   : m_sock(sock),
					   m_accounts(accounts),
					   m_options(options),
					   m_mailbox_dir(NULL),
					   m_bHaveLock(false),
					   m_state(P3S_AUTHORIZATION)
{
}

Pop3Server::~Pop3Server()
{
	if(m_bHaveLock)
		m_accounts.releasePOP3lock(m_user.getDomain(), m_user.getUser());
	m_sock.close();
	delete[] m_mailbox_dir;
}

int Pop3Server::run()
{
	try
	{
		bool done = false;
		ok("POP3 server ready");

		while(!done)
		{
			char command_buf[POP3_MAX_RESPONSE_LENGTH];
			char *pcmd_line = command_buf;
			char command[POP3_MAX_RESPONSE_LENGTH];

			if(!m_sock.getLine(command_buf, sizeof command_buf, NULL))
			{
				err("Line too long.");
				continue;
			}

			if(!nexttoken(&pcmd_line, command, sizeof command))
			{
				err("Invalid command");
				continue;
			}

			switch(m_state)
			{
			case P3S_AUTHORIZATION:
				if(strcasecmp("USER", command) == 0)
					user(pcmd_line);
				else if(m_user.isSet() && strcasecmp("PASS", command) == 0)
					pass(pcmd_line);
				else if(strcasecmp("QUIT", command) == 0)
				{
					quit_noupdate();
					done = true;
				}
				else
					err("Invalid command");
				break;

			case P3S_TRANSACTION:
				if(strcasecmp("STAT", command) == 0)
					stat();
				else if(strcasecmp("LIST", command) == 0)
					list(pcmd_line);
				else if(strcasecmp("RETR", command) == 0)
					retr(pcmd_line);
				else if(strcasecmp("DELE", command) == 0)
					dele(pcmd_line);
				else if(strcasecmp("NOOP", command) == 0)
					noop();
				else if(strcasecmp("RSET", command) == 0)
					rset();
				else if(strcasecmp("QUIT", command) == 0)
				{
					quit_update();
					done = true;
				}
				else 
					err("Invalid command");
				break;

			case P3S_UPDATE:
				break;
			}
		}
	}
	catch(SocketError & e)
	{
		m_log.log("POP3 server error: %s", e.errMsg());
		return 1;
	}

	return 0;
}

void Pop3Server::ok(const char* msg)
{
	m_sock.send("+OK ", 4);
	if(msg)
		m_sock.send(msg, strlen(msg));
	m_sock.send(CRLF, 2); // Don't use sizeof because it will send the NULL terminator.
}

void Pop3Server::err(const char* msg)
{
	m_sock.send("-ERR ", 5);
	if(msg)
		m_sock.send(msg, strlen(msg));
	m_sock.send(CRLF, 2); // Don't use sizeof (see above).
}


void Pop3Server::user(char *command)
{
	char *local_part = 0;
	char *domain_part = 0;

	if(*command == 0)
	{
		err("Mailbox does not exist");
		return;
	}

	++command;

	// The command must be user@domain.
	if(!mailbox_ok(command, &local_part, &domain_part))
	{
		err("Mailbox does not exist");
		return;
	}

	delete[] m_mailbox_dir;
	m_mailbox_dir = 0;

	if(m_accounts.isMailboxOk(domain_part, local_part, &m_mailbox_dir) != MS_OK)
	{
		err("Mailbox does not exist");
		return;
	}
	
	m_user.setUser(local_part);
	m_user.setDomain(domain_part);
	delete[] local_part;
	delete[] domain_part;

	ok("That is a valid mailbox");
}

void Pop3Server::pass(char *command)
{
	if(*command == 0)
	{
		err("Invalid password");
		return;
	}

	++command;

	if(!valid_password(command))
	{
		err("Invalid password");
		return;
	}

	if(!m_accounts.acquirePOP3lock(m_user.getDomain(), m_user.getUser()))
	{
		err("Mailbox is already locked");
		return;
	}

	m_bHaveLock = true;

	if(!m_message_list.build_list(m_mailbox_dir))
	{
		err("Unable to build mail list.");
		m_accounts.releasePOP3lock(m_user.getDomain(), m_user.getUser());
		m_bHaveLock = false;
		return;
	}

	char response[POP3_MAX_RESPONSE_LENGTH];
	safe_snprintf(response, sizeof(response),
		"%s@%s's mailbox has %u messages (%u octects)",
		m_user.getUser(), m_user.getDomain(),
		m_message_list.getCount(), m_message_list.getTotalSize());

	m_state = P3S_TRANSACTION;
	ok(response);
}

void Pop3Server::stat()
{
	char response[POP3_MAX_RESPONSE_LENGTH];
	safe_snprintf(response, sizeof(response), "%u %u",
		m_message_list.getCount(), m_message_list.getTotalSize());
	ok(response);
}

void Pop3Server::list(char *command)
{
	char buf[POP3_MAX_RESPONSE_LENGTH];

	if(nexttoken(&command, buf, sizeof buf))
	{
		// The user set the message to retrieve.
		int msgnum = atoi(buf);
		
		if(msgnum <= 0 || (unsigned)msgnum > m_message_list.getCount())
		{
			err("No such message");
			return;
		}

		safe_snprintf(buf, sizeof(buf), "%u %u",
			msgnum, m_message_list.getAt(msgnum - 1).filesize);
		ok(buf);
	}
	else
	{
		const char* format;

		if(m_message_list.getCount() == 1)
			format = "%u message (%u octets)";
		else
			format = "%u messages (%u octets)";

		safe_snprintf(buf, sizeof(buf), format,
			m_message_list.getCount(), m_message_list.getTotalSize());

		ok(buf);

		// Get a list of all messages.
		size_t count = m_message_list.getCount();
		for(size_t i = 0; i < count; ++i)
		{
			int len = safe_snprintf(buf, sizeof(buf), "%u %u",
				i + 1, m_message_list.getAt(i).filesize);
			m_sock.send(buf, len);
			m_sock.send(CRLF, 2);
		}
		m_sock.putLine(".");
	}
}

void Pop3Server::retr(char *command)
{
	int msgnum = atoi(command);

	if(msgnum <= 0 || (unsigned)msgnum > m_message_list.getCount())
	{
		err("No such message");
		return;
	}

	MessageInfo & m = m_message_list.getAt(msgnum - 1);

	char buf[POP3_MAX_RESPONSE_LENGTH];
	safe_snprintf(buf, sizeof(buf), "%u octets", m.filesize);

	FILE *fp = fopen(m.filename, "rb");

	if(fp == NULL)
	{
		err("No such message");
		return;
	}

	ok(buf);

	// BUFLEN is the size "chunk" we read from files. The bigger it is
	// the less times we go to disk.
	const size_t BUFLEN = 30000;
	char readBuf[BUFLEN];
	size_t bytesRead;

	while((bytesRead = fread(readBuf, 1, sizeof(readBuf), fp)) > 0)
		m_sock.send(readBuf, bytesRead);

	if(ferror(fp))
		m_log.log("POP Server: Error reading '%s'", m.filename);

	m_sock.putLine("");
	m_sock.putLine("."); // End of data indicator.

	fclose(fp);
}

void Pop3Server::dele(char *command)
{
	int msgnum = atoi(command);
	char buf[POP3_MAX_RESPONSE_LENGTH];

	if(msgnum <= 0 || (unsigned)msgnum > m_message_list.getCount())
	{
		err("No such message");
		return;
	}

	MessageInfo & m = m_message_list.getAt(msgnum - 1);
	if(m.bDelete)
	{
		safe_snprintf(buf, sizeof(buf), "Message %u already deleted", (unsigned)msgnum);
		err(buf);
		return;
	}

	m.bDelete = true;
	safe_snprintf(buf, sizeof(buf), "Message %u deleted", (unsigned)msgnum);
	ok(buf);
}

void Pop3Server::noop()
{
	ok();
}

void Pop3Server::rset()
{
	size_t count = m_message_list.getCount();
	for(size_t i = 0; i < count; ++i)
		m_message_list.getAt(i).bDelete = false;
	ok();
}

void Pop3Server::quit_noupdate()
{
	ok("POP3 server signing off");
}

void Pop3Server::quit_update()
{
	bool allGood = true;
	size_t count = m_message_list.getCount();

	for(size_t i = 0; i < count; ++i)
	{
		MessageInfo & m = m_message_list.getAt(i);
		if(m.bDelete)
		{
			if(unlink(m.filename) != 0)
				allGood = false;
		}
	}

	if(!allGood)
	{
		err("Some messages not removed");
		return;
	}

	ok("POP3 server signing off");
}

// Returns true if the password is value, false if it is not.
bool Pop3Server::valid_password(const char* password)
{
	char actual_password[MAX_CONFIGFILE_LINE_LEN + 1];
	char userconf_filename[MAX_PATH + 1];
	
	safe_snprintf(userconf_filename, sizeof(userconf_filename),
		"%s%cuserconf.txt", m_mailbox_dir, DIR_DELIM);

	ConfigFile cf(userconf_filename);
	
	if(!cf.getValue("password", actual_password, sizeof actual_password))
		return false;

	return strcmp(password, actual_password) == 0;
}
