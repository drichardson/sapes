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

#ifndef MAILSERV_ACCOUNTS_H
#define MAILSERV_ACCOUNTS_H

#include <stdio.h>
#include "log.h"
#include "thread.h"

enum MAILBOX_STATUS
{
	MS_OK,
	MS_DOMAIN_NOT_LOCAL,
	MS_MAILBOX_NOT_FOUND
};

class Accounts
{
	Log m_log;

	// DomainList represents a list of domains that the server is handling
	// mail requests for.
	struct DomainList
	{
		DomainList *next;
		char* domain;
		char* mailbox_directory;

		DomainList(DomainList *next, const char* domain, const char* directory);
		~DomainList();
	} *m_domain_list;

	// LockTree is a binary search tree that has an entry for each lock
	// acquired. Once the lock has been release the entry will be deleted.
	struct LockTree
	{
		LockTree *left;
		LockTree *right;
		char* user; // The fully qualified username (i.e. user@domain).

		LockTree(const char* newUser);
		~LockTree();
	} *m_lock_tree; // Only access m_lock_tree after acquiring m_LockTreeMutex.

	MUTEX m_LockTreeMutex;

public:
	Accounts();
	~Accounts();

	// Return the status of a mailbox.If MS_OK and pmailbox_dir != NULL, then pmailbox_dir
	// is set to point to the mailbox directory. A strdupnew is used to allocate the
	// memory *pmilbox_dir points to, so make sure you free it with delete[].
	MAILBOX_STATUS isMailboxOk(const char* domain,
		const char* mailbox,
		char** pmailbox_dir = NULL) const;

	// Add a domain for the server to be responsible for. The mailbox directory
	// contains all of the user mailboxes.
	void addDomain(const char* domain, const char* mailbox_directory);

	// Create a new message for the given (domain, mailbox) pair. The returned
	// FILE pointer is the responsiblity of the calling function. A NULL is returned
	// if the domain or mailbox does not exist or if fopen fails. *pNewMessageFile is set
	// to point to a string allocated with new that contains the full path to the newly
	// created file. The file is created with "wb" passed to fopen.
	FILE* newMessage(const char* domain, const char* mailbox, char** pNewMessageFile) const;

	// The POP3 server uses the following two functions to acquire and release
	// mailbox locks.
	bool acquirePOP3lock(const char* domain, const char* mailbox);
	bool releasePOP3lock(const char* domain, const char* mailbox);
};

#endif
