/*
 * Copyright (c) 2003, Douglas Ryan Richardson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * * Neither the name of the organization nor the names of its contributors may
 * be used to endorse or promote products derived from this software without
 * specific prior written permission.
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

#ifndef MAILSERV_SENDER_H
#define MAILSERV_SENDER_H

#include "socket.h"
#include "log.h"
#include "options.h"
#include "accounts.h"
#include "thread.h"

enum REASON_FAILED
{
	RF_MAILBOX_NOT_FOUND,
	RF_HOST_NOT_FOUND,
	RF_COULD_NOT_CONNECT_TO_HOST,
	RF_REJECTED_MAIL_FROM,
	RF_UNKNOWN
};

class Sender
{
	Log m_log;
	Options m_options;
	const Accounts & m_accounts;
	bool m_run;
	SEMAPHORE m_fileListSemaphore;
	MUTEX m_fileListMutex;
	SEMAPHORE m_fileListEmptySemaphore;

	struct FileList
	{
		FileList *next;
		char* filename;

		FileList(FileList * newNext, const char* filename);
		~FileList();
	} *m_pfiles;

	struct Mailbox
	{
		Mailbox *next; // The next mailbox

		// The next remote. Used for convience. You should not call delete on nextRemote.
		// It is the user of this structs responsiblity.
		Mailbox *nextRemote; 

		char *user; // Username
		char *domain; // Domain name
		bool failed; // True if a failure occurred while trying to deliver message.

		Mailbox(Mailbox *newNext, const char* newUser, const char* newDomain);
		~Mailbox();
	};

	// Build a list of files to process. Returns true if it added one or more
	// files to the list and false if it did not.
	bool build_list();

	// The thread routine.
	static THREAD_RETTYPE WINAPI thread_routine(void* pThis);

	// Process a sendDir file and put it in it's mailbox or send it
	// to another SMTP server.
	void process_file(const char* filename);

	bool copyMessageToLocalMailbox(FILE* fp, long endpos, const char* mailbox_dir) const;
	bool sendMessageToRemoteMailbox(FILE* fp, const Mailbox* from, const Mailbox* to, REASON_FAILED & reason) const;
	FILE* createBounceMessage(FILE *fp_original_message, char **pFilename,
								const Mailbox *from, const Mailbox *unreachable, REASON_FAILED reason) const;
	bool sendBounceMessage(FILE *fp, const Mailbox *from, const Mailbox *unreachable, REASON_FAILED reason) const;
	bool sendMessage(Socket & s, FILE* fp, const Mailbox* from, const Mailbox* to, REASON_FAILED & reason) const;

	const Sender & operator=(const Sender &);

public:
	Sender(const Options & options, const Accounts & accounts);
	~Sender();

	// Start the sender monitor.
	void Run();

	// Stop the sender monitor.
	void Stop();
};

#endif
