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

#include "socket.h"
#include "listener.h"
#include "server.h"
#include "pop3_server.h"
#include "thread.h"

struct StartupData
{
	SOCKET sock;
	Accounts & accounts;
	const Options & options;

	StartupData(SOCKET s, Accounts & accnts, const Options & opt)
		: sock(s),
		accounts(accnts),
		options(opt)
	{
	}

private:
	const StartupData & operator=(const StartupData &);
};

static THREAD_RETTYPE WINAPI run_smpt_server(void *pData)
{
	StartupData *pSD = (StartupData*)pData;
	Server server(pSD->sock, pSD->accounts, pSD->options);
	delete pSD; // pSD was allocated by a listener routine.

	return (THREAD_RETTYPE)server.run();
}

static THREAD_RETTYPE WINAPI run_sender(void *pData)
{
	Sender *p = (Sender*)pData;
	p->Run();
	delete p;

	return 0;
}

static THREAD_RETTYPE WINAPI run_pop3_server(void *pData)
{
	StartupData *pSD = (StartupData*)pData;
	Pop3Server server(pSD->sock, pSD->accounts, pSD->options);
	delete pSD;

	return (THREAD_RETTYPE)server.run();
}

Listener::Listener(const Options & opts)
: m_options(opts),
m_pSender(0),
m_run(true)
{
#ifdef WIN32
	WORD wVersionRequested;
	WSADATA wsaData;
	wVersionRequested = MAKEWORD( 2, 2 );
	WSAStartup(wVersionRequested, &wsaData);
#endif

}

Listener::~Listener()
{
	if(m_pSender)
	{
		m_pSender->Stop();
		delete m_pSender;
	}

#ifdef WIN32
	WSACleanup();
#endif
}

#ifdef WIN32
// If you don't do this pragma then you can't compile with warning level 4
// with MS VC++ because FD_SET is a macro that has a while(0) in it.
#pragma warning(disable : 4127)
#endif

int Listener::Run()
{
	// Initialize the accounts.
	for(const DomainList *pDL = m_options.domains(); pDL; pDL = pDL->next)
		m_accounts.addDomain(pDL->domain, pDL->mailbox_dir);

	// Startup the sender monitor.
	if(!m_pSender)
	{
		m_pSender = new Sender(m_options, m_accounts);
		if(!create_thread(run_sender, m_pSender))
		{
			delete m_pSender;
			m_pSender = 0;
			m_log.log("Error creating sender thread.");
			return 1;
		}
	}
	
	// Setup the SMTP listening socket.
	SOCKET smtp_listen_socket = socket(AF_INET, SOCK_STREAM, 0);

	if(smtp_listen_socket == INVALID_SOCKET)
	{
		m_log.log("Could not create SMTP listen socket.");
		return 1;
	}

	sockaddr_in listen_addr;
	listen_addr.sin_family = AF_INET;
	listen_addr.sin_port = htons(m_options.smtpListenPort());
	listen_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if(bind(smtp_listen_socket, (sockaddr*)&listen_addr, sizeof listen_addr) != 0)
	{
		m_log.log("Could not bind to address.");
		closesocket(smtp_listen_socket);
		return 1;
	}

	if(listen(smtp_listen_socket, SOMAXCONN) != 0)
	{
		m_log.log("Could not set socket to listen socket.");
		closesocket(smtp_listen_socket);
		return 1;
	}
	
	// Setup the POP3 listening socket.
	SOCKET pop3_listen_socket = socket(AF_INET, SOCK_STREAM, 0);

	if(pop3_listen_socket == INVALID_SOCKET)
	{
		m_log.log("Could not create POP3 listen socket.");
		closesocket(smtp_listen_socket);
		return 1;
	}

	listen_addr.sin_family = AF_INET;
	listen_addr.sin_port = htons(m_options.pop3ListenPort());
	listen_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if(bind(pop3_listen_socket, (sockaddr*)&listen_addr, sizeof listen_addr) != 0)
	{
		m_log.log("Could not bind POP3 listen socket to address.");
		closesocket(smtp_listen_socket);
		closesocket(pop3_listen_socket);
		return 1;
	}

	if(listen(pop3_listen_socket, SOMAXCONN) != 0)
	{
		m_log.log("Could not set POP3 socket to listen socket.");
		closesocket(smtp_listen_socket);
		closesocket(pop3_listen_socket);
		return 1;
	}

	while(m_run)
	{
		int ready;
		fd_set set;
		FD_ZERO(&set);
		FD_SET(smtp_listen_socket, &set);
		FD_SET(pop3_listen_socket, &set);

		ready = select(FD_SETSIZE, &set, NULL, NULL, NULL);

		// This shouldn't happen since we have no timeout.
		if(ready == 0)
		{
			m_log.log("Listener::Run - ready == 0. Doesn't make sense");
			continue;
		}

		// This will happen on signals or bad arguments to select.
		if(ready <= 0)
		{
			// This is what I want to do but I don't know if I can in a
			// multi-threaded application. I need to research this.fs
			// if(errno != EINTR)
			//	m_log.log("select failed: %s", strerror(errno));
			continue;
		}

		if(FD_ISSET(smtp_listen_socket, &set))
		{
			sockaddr_in addr;
			socklen_t addr_len = sizeof addr;
			SOCKET sock = accept(smtp_listen_socket, (sockaddr*)&addr, &addr_len);

			if(sock == INVALID_SOCKET)
			{
				m_log.log("Error accepting SMTP connection.");
				continue;
			}

			// pSD will be freed by the thread routine.
			StartupData *pSD = new StartupData(sock, m_accounts, m_options);

			if(!create_thread(run_smpt_server, pSD))
			{
				m_log.log("Could not create SMTP server thread.");
				continue;
			}
		}

		if(FD_ISSET(pop3_listen_socket, &set))
		{
			sockaddr_in addr;
			socklen_t addr_len = sizeof addr;
			SOCKET sock = accept(pop3_listen_socket, (sockaddr*)&addr, &addr_len);

			if(sock == INVALID_SOCKET)
			{
				m_log.log("Error accepting POP3 connection.");
				continue;
			}

			// pSD will be freed by the thread routine.
			StartupData *pSD = new StartupData(sock, m_accounts, m_options);

			if(!create_thread(run_pop3_server, pSD))
			{
				m_log.log("Could not create POP3 server thread.");
				continue;
			}
		}

	}

	return 0;
}
