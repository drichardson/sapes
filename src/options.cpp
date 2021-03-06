/*
 * Copyright (c) 2003,2004 Douglas Ryan Richardson
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

#include "options.h"
#include "utility.h"
#include "config_file.h"
#include <stdlib.h>

//
// DomainList
//
DomainList::DomainList(DomainList *newNext, const char* domainName, const char* mailboxDir)
: next(newNext)
{
	domain = strdupnew(domainName);
	mailbox_dir = strdupnew(mailboxDir);
}

DomainList::~DomainList()
{
	delete[] domain;
	delete[] mailbox_dir;
	delete next;
}

//
// Options
//

Options::Options()
{
	init();
	set_default_values();
}

Options::Options(const Options & opt)
{
	init();
	copy(opt);
}

Options::~Options()
{
	delete[] m_send_dir;
	delete m_domains;
	delete m_resource_dir;
}

void Options::init()
{
	m_send_dir = NULL;
	m_domains = NULL;
	m_resource_dir = NULL;
}

void Options::copy(const Options & opt)
{
	delete[] m_send_dir;
	if(opt.m_send_dir)
		m_send_dir = strdupnew(opt.m_send_dir);

	m_smtp_listen_port = opt.m_smtp_listen_port;
	m_pop3_listen_port = opt.m_pop3_listen_port;
	m_http_listen_port = opt.m_http_listen_port;

	delete m_domains;
	for(const DomainList *p = opt.m_domains; p; p = p->next)
		m_domains = new DomainList(m_domains, p->domain, p->mailbox_dir);

	m_scan_interval = opt.m_scan_interval;
	m_sender_threads = opt.m_sender_threads;

	m_use_http_monitor = opt.m_use_http_monitor;

	delete[] m_resource_dir;
	if(opt.m_resource_dir)
		m_resource_dir = strdupnew(opt.m_resource_dir);
}

void Options::set_default_values()
{
	m_sender_threads = 5;
	m_scan_interval = 1;
	m_smtp_listen_port = 25;
	m_pop3_listen_port = 110;
	m_http_listen_port = 80;

	m_send_dir = NULL;
	m_domains = NULL;

	m_use_http_monitor = true;

	m_resource_dir = NULL;
}

bool Options::loadValuesFromFile(const char* filename)
{
	char buf[MAX_CONFIGFILE_LINE_LEN + 1];
	ConfigFile cf(filename);

	//The first options we need to check are the logging options - so
	//that we can properly log anything that is needed

	//Check the log_file option and update the logger with the file name
	if (cf.getValue("log_file", buf, sizeof(buf))) {
		m_log.GlobalLogFile(buf);
	} else {
		m_log.log(LOG_WARN, "Options::loadValuesFromFile(): No log file defined in config file");
	}

	//Timestamp log messages?
	if (cf.getValue("log_timestamp", buf, sizeof(buf))) {
		int stamp = atoi(buf);

		if (stamp != 0) 
			m_log.timestamp(true);
		else
			m_log.log(LOG_WARN, "Options::loadValuesFromFile(): Invalid timestamp option (%d)", stamp);
	}

	//Check the log level
	if (cf.getValue("log_level", buf, sizeof(buf))) {
		int lvl = atoi(buf);

		//make sure the received level is within bounds (see log.h)
		if (lvl >= LOG_LEVEL_LOW && lvl <= LOG_LEVEL_HIGH) 
			m_log.loglevel(lvl);
		else
			m_log.log(LOG_WARN, "Options::loadValuesFromFile(): Invalid log level (%d). Range is from %d to %d", 
								lvl, LOG_LEVEL_LOW, LOG_LEVEL_HIGH);
	}

	// send_dir is required. Return false if it is not found.
	if(cf.getValue("send_dir", buf, sizeof(buf)))
	{
		delete[] m_send_dir;
		m_send_dir = strdupnew(buf);
	}
	else
	{
		m_log.log(LOG_ERROR, "Options::loadValuesFromFile(): Required field 'send_dir' not found.");
		return false;
	}


	if(cf.getValue("domain_count", buf, sizeof(buf)))
	{
		int count = atoi(buf);

		if(count < 0)
			m_log.log(LOG_WARN, "Options::loadValuesFromFile(): Invalid domain_count value (%d, which is less than 0). No default used.");
		else
		{
			for(unsigned int i = 1; i <= (unsigned int)count; ++i)
			{
				char domain[MAX_CONFIGFILE_LINE_LEN + 1];
				char mailbox[MAX_CONFIGFILE_LINE_LEN + 1];

				safe_snprintf(buf, sizeof(buf), "domain%u", i);
				if(!cf.getValue(buf, domain, sizeof(domain)))
				{
					m_log.log(LOG_WARN, "Options::loadValuesFromFile(): Could not read value for %s", buf);
					return false;
				}

				safe_snprintf(buf, sizeof(buf), "domain%u_mailboxes", i);
				if(!cf.getValue(buf, mailbox, sizeof(mailbox)))
				{
					m_log.log(LOG_WARN, "Options::loadValuesFromFile(): Could not read value for %s", buf);
					return false;
				}
				else
					m_domains = new DomainList(m_domains, domain, mailbox);
			}
		}
	}
	else
	{
		m_log.log(LOG_ERROR, "Options::loadValuesFromFile(): Required field 'domain_count' not found.");
		return false;
	}

	if(cf.getValue("smtp_port", buf, sizeof(buf)))
	{
		short tmp = (short)atoi(buf);
		if(tmp < 1)
		{
			m_log.log(LOG_WARN, "Options::loadValuesFromFile(): Invalid smtp_port value (%d, which is less than 1). Default (%d) used.",
					  tmp, m_smtp_listen_port);
		}
		else
			m_smtp_listen_port = tmp;
	}

	if(cf.getValue("pop3_port", buf, sizeof(buf)))
	{
		short tmp = (short)atoi(buf);
		if(tmp < 1)
		{
			m_log.log(LOG_WARN, "Options::loadValuesFromFile(): Invalid pop3_port value (%d, which is less than 1). Default (%d) used.",
					  tmp, m_pop3_listen_port);
		}
		else
			m_pop3_listen_port = tmp;
	}

	if(cf.getValue("http_port", buf, sizeof(buf)))
	{
		short tmp = (short)atoi(buf);
		if(tmp < 1)
		{
			m_log.log(LOG_WARN, "Options::loadValuesFromFile(): Invalid http_port value (%d, which is less than 1). Default (%d) used.",
					  tmp, m_http_listen_port);
		}
		else
			m_http_listen_port = tmp;
	}	

	if(cf.getValue("scan_interval", buf, sizeof(buf)))
	{
		int tmp = atoi(buf);
		if(tmp < 1)
			m_log.log(LOG_WARN, "Options::loadValuesFromFile(): Invalid scan_interval value (%d, which is less than 1). Default (%d) used.", tmp);
		else
			m_scan_interval = tmp;
	}

	if(cf.getValue("sender_threads", buf, sizeof(buf)))
	{
		int tmp = atoi(buf);
		if(tmp < 1)
			m_log.log(LOG_WARN, "Options::loadValuesFromFile(): Invalid sender_thread value (%d, which is less than 1). Default (%d) used.", tmp);
		else
			m_sender_threads = tmp;
	}

	if(cf.getValue("use_http_monitor", buf, sizeof(buf)))
		m_use_http_monitor = atoi(buf) != 0;

	if(cf.getValue("resource_dir", buf, sizeof(buf)))
	{
		delete[] m_resource_dir;
		m_resource_dir = strdupnew(buf);
	}

	return true;
}

short Options::smtpListenPort() const
{
	return m_smtp_listen_port;
}

short Options::pop3ListenPort() const
{
	return m_pop3_listen_port;
}

short Options::httpListenPort() const
{
	return m_http_listen_port;
}

const char* Options::sendDir() const
{
	return m_send_dir;
}

const DomainList* Options::domains() const
{
	return m_domains;
}

unsigned int Options::scanInterval() const
{
	return m_scan_interval;
}

unsigned int Options::senderThreads() const
{
	return m_sender_threads;
}

bool Options::useHttpMonitor() const
{
	return m_use_http_monitor;
}

FILE* Options::getResourceFile(const char* filename) const
{
	FILE *fp = NULL;
	
	if(m_resource_dir)
	{
		char path[MAX_PATH + 1];
		safe_snprintf(path, sizeof(path), "%s%c%s",
					  m_resource_dir, DIR_DELIM, filename);
		fp = fopen(path, "r");
	}

	return fp;	
}
