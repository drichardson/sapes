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
  
#include "accounts.h"
#include "utility.h"

Accounts::DomainList::DomainList(Accounts::DomainList *newNext, const char* newDomain, const char* newDirectory)
: next(newNext)
{
	domain = strdupnew(newDomain);
	mailbox_directory = strdupnew(newDirectory);
}

Accounts::DomainList::~DomainList()
{
	delete[] domain;
	delete[] mailbox_directory;
	delete next;
}

Accounts::LockTree::LockTree(const char* newUser)
: left(NULL),
right(NULL)
{
	user = strdupnew(newUser);
}

Accounts::LockTree::~LockTree()
{
	delete[] user;
	delete left;
	delete right;
}

Accounts::Accounts()
: m_domain_list(0),
m_lock_tree(0)
{
	if(!create_mutex(m_LockTreeMutex))
		m_log.log(LOG_WARN, "Accounts: Could not create lock tree mutex.");
}

Accounts::~Accounts()
{
	delete_mutex(m_LockTreeMutex);

	delete m_domain_list;
	delete m_lock_tree;
}

MAILBOX_STATUS Accounts::isMailboxOk(const char* domain,
									 const char* mailbox,
									 char** pmailbox_dir) const
{
	const char* mailbox_dir = 0;

	for(DomainList* p = m_domain_list; p && !mailbox_dir; p = p->next)
	{
		if(strcasecmp(domain, p->domain) == 0)
			mailbox_dir = p->mailbox_directory;
	}

	if(!mailbox_dir)
		return MS_DOMAIN_NOT_LOCAL;

	char buf[MAX_PATH + 1];
	safe_snprintf(buf, sizeof buf, "%s/%s", mailbox_dir, mailbox);

	if(!isdir(buf))
		return MS_MAILBOX_NOT_FOUND;

	if(pmailbox_dir)
		*pmailbox_dir = strdupnew(buf);

	return MS_OK;
}

void Accounts::addDomain(const char* domain, const char* mailbox_directory)
{
	m_domain_list = new DomainList(m_domain_list, domain, mailbox_directory);
}

FILE* Accounts::newMessage(const char* domain, const char* mailbox, char** pNewMessageFile) const
{
	const char* mailbox_dir = 0;

	for(DomainList* p = m_domain_list; p && !mailbox_dir; p = p->next)
	{
		if(strcasecmp(domain, p->domain) == 0)
			mailbox_dir = p->mailbox_directory;
	}

	if(!mailbox_dir)
	{
		m_log.log(LOG_ERROR, "newMessage: Mailbox directory not set for the %s domain.", domain);
		return NULL;
	}

	char buf[MAX_PATH + 1];
	safe_snprintf(buf, sizeof buf, "%s/%s", mailbox_dir, mailbox);

	FILE *fp = newfile(mailbox_dir, "MSG", pNewMessageFile);

	if(!fp)
	{
		m_log.log(LOG_ERROR, "Accounts::newMessage(): Could not create message file in '%s' for %s domain.", mailbox_dir, domain);
		return NULL;
	}

	return fp;
}

bool Accounts::acquirePOP3lock(const char* domain, const char* mailbox)
{
	size_t bufsize = strlen(domain) + strlen(mailbox) + 2;
	char *fully_qualified_username = new char[bufsize];
	safe_snprintf(fully_qualified_username, bufsize, "%s@%s", mailbox, domain);

	if(!wait_mutex(m_LockTreeMutex))
	{
		m_log.log(LOG_WARN, "acquirePOP2lock: Can't acquire POP3 lock because I cannot acquire the lock tree mutex.");
		delete[] fully_qualified_username;
		return false;
	}

	LockTree *parent = NULL;
	LockTree *p = m_lock_tree;
	bool addOnLeft = false;

	if(m_lock_tree == NULL)
	{
		m_lock_tree = new LockTree(fully_qualified_username);
		delete[] fully_qualified_username;
		release_mutex(m_LockTreeMutex);
		return true;
	}

	while(p)
	{
		int cmp = strcasecmp(fully_qualified_username, p->user);

		if(cmp == 0)
			break;
		else if(cmp > 0)
		{
			addOnLeft = false;
			parent = p;
			p = p->right;
		}
		else
		{
			addOnLeft = true;
			parent = p;
			p = p->left;
		}
	}

	bool rc = p == NULL;

	if(!p)
	{
		// Add the lock to the tree.
		p = new LockTree(fully_qualified_username);
		if(addOnLeft)
			parent->left = p;
		else
			parent->right = p;

		delete[] fully_qualified_username;
	}

	release_mutex(m_LockTreeMutex);
	return rc;
}

bool Accounts::releasePOP3lock(const char* domain, const char* mailbox)
{
	size_t bufsize = strlen(domain) + strlen(mailbox) + 2;
	char *fully_qualified_username = new char[bufsize];
	safe_snprintf(fully_qualified_username, bufsize, "%s@%s", mailbox, domain);

	if(!wait_mutex(m_LockTreeMutex))
	{
		m_log.log(LOG_WARN, "releasePOP3lock: Could not release POP3 lock because I couldn't acquire the lock tree mutex.");
		delete[] fully_qualified_username;
		return false;
	}

	LockTree *parent = NULL;
	LockTree *p = m_lock_tree;
	bool parentsLeft = false;

	if(m_lock_tree == NULL)
	{
		// No lock to release.
		delete[] fully_qualified_username;
		release_mutex(m_LockTreeMutex);
		return false;
	}

	while(p)
	{
		int cmp = strcasecmp(fully_qualified_username, p->user);

		if(cmp == 0)
			break;
		else if(cmp > 0)
		{
			parentsLeft = false;
			parent = p;
			p = p->right;
		}
		else
		{
			parentsLeft = true;
			parent = p;
			p = p->left;
		}
	}

	bool rc = p != NULL;

	// Remove the lock from the tree.
	if(p)
	{
		if(p == m_lock_tree)
		{
			delete m_lock_tree;
			m_lock_tree = NULL;
		}
		else
		{
			if(parentsLeft)
				parent->left = p->left == NULL ? p->right : p->left;
			else
				parent->right = p->left == NULL ? p->right : p->left;

			// NULL the child nodes so that they don't get recusively deleted
			// by ~LockTree.
			p->left = NULL;
			p->right = NULL;
			delete p;
		}
	}

	delete[] fully_qualified_username;
	release_mutex(m_LockTreeMutex);
	return rc;
}
