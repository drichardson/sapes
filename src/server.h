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

#ifndef MAILSERV_SERVER_H
#define MAILSERV_SERVER_H

#include "log.h"
#include "socket.h"
#include "accounts.h"
#include "options.h"

class Server
{
	Socket m_sock;
	Log m_log;
	const Accounts & m_accounts;
	const Options & m_options;

	// command processing functions
	void helo(char* command);
	void ehlo(char* command);
	void mail(char* command);
	void rcpt(char* command);
	void data(char* command);
	void rset(char* command);
	void vrfy(char* command);

	// commmand/reply functions
	void reply(short code, const char* format = 0, ...); // Send a reply.

	class Mailbox
	{
		char *local_part;
		char *domain_part;

	public:
		Mailbox();
		~Mailbox();

		const char* getLocal() const;
		const char* getDomain() const;

		void setPath(const char* newLocal, const char* newDomain);

		void clear();
		bool isSet();
	};

	struct ToList
	{
		ToList *next;
		Mailbox recipient;

		ToList();
		~ToList();
		bool isSet();
	};

	struct Message
	{
		Mailbox from;
		ToList *to;
		char *data;

		Message();
		~Message();

		void add_recipient(const char* localpart, const char* domainpart);
		void reset();
	} m_message;

	// The assignment operator is made private so that no one uses it. It has no definition.
	const Server & operator=(const Server &);

public:
	Server(SOCKET sock, const Accounts & accounts, const Options & options);
	~Server();

	int run();
};

#endif
