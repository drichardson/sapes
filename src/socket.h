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

// socket.h - include the neccessary headers for each platform to support sockets.
// After you include this file you should define your socket as type SOCKET.

#ifndef MAILSERV_SOCKET_H
#define MAILSERV_SOCKET_H

#ifdef WIN32
#include <winsock2.h>
typedef int socklen_t;
#else

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

typedef int SOCKET;
#define INVALID_SOCKET (-1)
#define closesocket close
#define SOCKET_ERROR (-1)

#endif

class SocketError
{
	char *m_errmsg;
	const SocketError & operator=(const SocketError & s);

public:
	SocketError(const char* errmsg);
	SocketError(const SocketError & se);
	~SocketError();
	const char* errMsg() const;
};

class Socket
{
public:
	SOCKET sock;
	Socket(SOCKET newSock);
	~Socket();

	// These functions throw SocketError if the socket is closed while trying to read
	// or write or if some other fatal error occurs.
	bool getLine(char* command_buf, const int BUFLEN, unsigned int *pLength);
	void putLine(const char* command);
	void send(const char* buf, int len, int flags = 0);
	void recv(char *buf, int len, int flags = 0);
	void close();
};

#endif
