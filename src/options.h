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

#ifndef MAILSERV_OPTIONS_H
#define MAILSERV_OPTIONS_H

#include "log.h"

struct DomainList
{
	DomainList *next;
	char* domain;
	char* mailbox_dir;

	DomainList(DomainList* next, const char* domainName, const char* mailboxDir);
	~DomainList();
};

class Options
{
	Log m_log;
	short m_smtp_listen_port;
	short m_pop3_listen_port;
	char* m_send_dir;
	DomainList *m_domains;
	unsigned int m_scan_interval;
	unsigned int m_sender_threads;

	void set_default_values();

	void init();
	void copy(const Options & opt);
	const Options & operator=(const Options & opt);
public:
	Options();
	Options(const Options & opt);
	~Options();

	bool loadValuesFromFile(const char* filename);

	short smtpListenPort() const;
	short pop3ListenPort() const;
	const char* sendDir() const;
	const DomainList * domains() const;
	unsigned int scanInterval() const;
	unsigned int senderThreads() const;
};

#endif
