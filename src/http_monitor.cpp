/*
 * Copyright (c) 2004, Douglas Ryan Richardson
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

#include "http_monitor.h"
#include "utility.h"

static const char* msgFromCode(HTTP_RESPONSE_CODE code)
{
	const char* msg = NULL;
	
	switch(code)
	{
	case HTTP_OK:
		msg = "OK";
		break;
	case HTTP_BADREQUEST:
		msg = "Bad request.";
		break;
	case HTTP_NOTFOUND:
		msg = "Not found";
		break;
	case HTTP_REQUESTTOLARGE:
		msg = "Request too large.";
		break;
	case HTTP_SERVERERROR:
		msg = "Internal server error.";
		break;
	default:
		msg = "Unknown error.";
		break;
	}

	return msg;
}

class DataList
{
	DataList *m_next;
	const void *m_data;
	unsigned int m_data_len;
public:
	DataList(const void* pdata, unsigned int len)
		: m_next(NULL), m_data(pdata), m_data_len(len) {}
	~DataList() { delete m_next; }
	void setNext(DataList *next) { m_next = next; }
	const DataList* getNext() const { return m_next; }
	const void* getData() const { return m_data; }
	unsigned int getDataLen() const { return m_data_len; }
};

class DataObject
{
	DataList *m_pFront;
	DataList *m_pBack;
public:
	DataObject() : m_pFront(NULL), m_pBack(NULL) {}
	~DataObject() { delete m_pFront; }
	void addData(const void *data, unsigned int len);
	const DataList* getFirst() const { return m_pFront; }
};

void DataObject::addData(const void *data, unsigned int len)
{
	if(m_pFront == NULL)
	{
		m_pBack = m_pFront = new DataList(data, len);
		if(m_pFront == NULL)
			throw RuntimeException("DataObject::addData - Could not create new DataList (1).");
	}
	else if(m_pBack != NULL)
	{
		DataList *next = new DataList(data, len);
		if(next == NULL)
			throw RuntimeException("DataObject::addData - Could not create new DataList (2).");
		m_pBack->setNext(next);
		m_pBack = next;
	}
	else
		throw RuntimeException("DataObject::addData - Program Error. Unexpected execution path.");
} 
	
class HttpResponse
{
	HTTP_RESPONSE_CODE m_response_code;
	DataObject m_header;
	DataObject m_data;
	unsigned int m_data_len;

public:
	HttpResponse(HTTP_RESPONSE_CODE code);

	// HttpResponse automatically comptes the content-length header, so
	// there is no need to set it here. addHeader is for other headers.
	void addHeader(const char* header);

	// Add the data to send.
	void addData(const void* data, unsigned int len);
	void addData(const char* str);

	// Send the response.
	void send(Socket & s);
};

HttpResponse::HttpResponse(HTTP_RESPONSE_CODE code)
	: m_response_code(code),
	  m_data_len(0)
{
}

void HttpResponse::addHeader(const char* headerval)
{
	m_header.addData(headerval, strlen(headerval));
	m_header.addData(CRLF, 2);
}

// appendData relies on data being around until the HttpResponse
// object is destroyed.
void HttpResponse::addData(const void *data, unsigned int len)
{
	m_data.addData(data, len);
	m_data_len += len;	
}

void HttpResponse::addData(const char* str)
{
	if(str)
		addData(str, strlen(str));
}

void HttpResponse::send(Socket & s)
{
	// Send the initial response.
	char buf[200];
	safe_snprintf(buf, sizeof(buf), "HTTP/1.0 %d %s",
				  m_response_code, msgFromCode(m_response_code));
	s.putLine(buf);

	// Send the headers.
	const DataList *p = NULL;

	for(p = m_header.getFirst(); p; p = p->getNext())
		s.send(p->getData(), p->getDataLen());

	// Send the content length;
	char content_length[50];
	safe_snprintf(content_length, sizeof(content_length), "Content-Length: %u", m_data_len);
	s.putLine(content_length);
	s.send(CRLF, 2);

	// Send the data.
	for(p = m_data.getFirst(); p; p = p->getNext())
		s.send(p->getData(), p->getDataLen());
}

HttpMonitor::HttpMonitor(SOCKET sock,
						 Accounts & accounts,
						 const Options & options)
	: m_sock(sock),
	  m_accounts(accounts),
	  m_options(options)
{
}

HttpMonitor::~HttpMonitor()
{
	m_sock.close();
}

enum HTTP_COMMAND
{
	HTTP_NOTSET,
	HTTP_GET
};

int HttpMonitor::run()
{
	try
	{
		bool got_command = false;
		
		do
		{
			char command_buf[HTTP_MAX_LINE_LENGTH];
			char *pcmd_line = command_buf;
			char command[HTTP_MAX_LINE_LENGTH];
			unsigned int len = 0;

			if(!m_sock.getLine(command_buf, sizeof command_buf, &len))
			{

				err(HTTP_REQUESTTOLARGE);
				return 1;
			}

			if(nexttoken(&pcmd_line, command, sizeof(command), " \t", ""))
			{
				if(strcmp("GET", command) == 0)
				{
					get(pcmd_line);
					got_command = true;
				}
			}			
		}
		while(!got_command);
	}
	catch(Exception & e)
	{
		m_log.log("HTTP monitor error: %s", e.errMsg());
		return 1;
	}
	
	return 0;
}

void HttpMonitor::err(HTTP_RESPONSE_CODE errcode, const char* msg)
{
	if(msg == NULL)
		msg = msgFromCode(errcode);

	char buf[200];
	safe_snprintf(buf, sizeof(buf), "HTTP/1.0 %d %s", errcode, msg);
	m_sock.putLine(buf);
}

void HttpMonitor::get(char* get_command)
{
	try
	{
		char path[HTTP_MAX_LINE_LENGTH];
		if(!nexttoken(&get_command, path, sizeof(path), " \t", ""))
		{
			err(HTTP_BADREQUEST);
			return;
		}

		if(strcmp(path, "/") == 0)
			get_main();
		else
			err(HTTP_NOTFOUND);
	}
	catch(SocketError & e)
	{
		m_log.log("HTTP monitor error while processing GET request: %s", e.errMsg());
	}
}

#define ADDHTML(html) { const char d[] = html; r.addData(d, sizeof(d) - 1); }

void HttpMonitor::get_main()
{
	// Get the main status page.
	HttpResponse r(HTTP_OK);
	r.addHeader("Content-Type: text/html");

	ADDHTML("<html><head><title>sapes monitor</title></head>\n"
			"<body>\n"
			"<h1>sapes monitor</h1></body></html>\n"
			"<p>\n"
			"This monitor allows you to view sapes stats. Here is a domain list.\n"
			"<ol>");

	for(const DomainList *pDL = m_options.domains(); pDL; pDL = pDL->next)
	{
		ADDHTML("\n<li>");
		r.addData(pDL->domain);
	}
	
	ADDHTML("\n</ol>\n</body>\n</html>");
	r.send(m_sock);
}
