/*
 * Copyright (c) 2003, Douglas Ryan Richardson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation and/or
 * other materials provided with the distribution.
 * * Neither the name of the organization nor the names of its contributors may be
 * used to endorse or promote products derived from this software without specific
 * prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MAILSERV_POP3_SERVER_H
#define MAILSERV_POP3_SERVER_H

#include "socket.h"
#include "accounts.h"
#include "options.h"
#include "utility.h"

struct MessageInfo
{
	bool bDelete; // If true, delete on update.
	int filesize;
	char filename[MAX_PATH];
};

class MessageInfoArray
{
	size_t m_totalSize; // Total size of the mailbox in octects (is an octect a byte?).
	size_t m_growBy; // The number of MessageInfos to grow by when more memory is needed.
	size_t m_count; // The count of the messages in this list.
	size_t m_maxcount; // The max count before the buffer must be incremented.
	MessageInfo *m_msginfo; // The vector.
	MessageInfo m_bogus; // This is the MessageInfo returned if you give an index that is out of bounds.

public:
	MessageInfoArray();
	~MessageInfoArray();

	size_t getTotalSize() const; // Get the total size of the files this array represents.
	size_t getCount() const; // Get the number of MessageInfo entries.
	MessageInfo & getAt(size_t index);
	bool build_list(const char* mailbox_dir);
};

class Pop3Server
{
	Socket m_sock;
	Log m_log;
	Accounts & m_accounts;
	const Options & m_options;
	char* m_mailbox_dir;
	bool m_bHaveLock;

	enum POP3State {
		P3S_AUTHORIZATION,
		P3S_TRANSACTION,
		P3S_UPDATE
	} m_state;

	class User
	{
		char *user;
		char *domain;

	public:
		User();
		~User();

		const char* getUser() const;
		void setUser(const char* newUser);

		const char* getDomain() const;
		void setDomain(const char* newDomain);

		void clear();
		bool isSet() const;
	} m_user;

	MessageInfoArray m_message_list;

	void ok(const char* msg = NULL);
	void err(const char* msg = NULL);

	void user(char *command);
	void pass(char *command);
	void stat();
	void list(char *command);
	void retr(char *command);
	void dele(char *command);
	void noop();
	void rset();
	void quit_noupdate();
	void quit_update();

	// Returns true if the password is value, false if it is not.
	bool valid_password(const char* password);

	// The assignment operator is made private so that no one uses it. It has no definition.
	const Pop3Server & operator=(const Pop3Server & server);

public:
	Pop3Server(SOCKET sock, Accounts & accounts, const Options & options);
	~Pop3Server();

	int run();
};

#endif
