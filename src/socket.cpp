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

#include "socket.h"
#include "utility.h"

//
// SocketError
//

SocketError::SocketError(const char* errmsg)
{
	m_errmsg = strdupnew(errmsg);
}

SocketError::SocketError(const SocketError & se)
{
	m_errmsg = strdupnew(se.m_errmsg);
}

SocketError::~SocketError()
{
	delete[] m_errmsg;
}

const char* SocketError::errMsg() const
{
	return m_errmsg;
}

//
// Socket
//

Socket::Socket(SOCKET newSock)
: sock(newSock)
{
}

Socket::~Socket()
{
	if(sock != INVALID_SOCKET)
		close();
}

bool Socket::getLine(char* command_buf, const int BUFLEN, unsigned int* pLength)
{
	int i;

	for(i = 0; i < BUFLEN; ++i)
	{
		recv(command_buf + i, 1);
		if(i > 0 && command_buf[i - 1] == CR && command_buf[i] == LF)
		{
			// Remove the CRLF.
			command_buf[i - 1] = 0;
			break;
		}
	}

	// If we read MAXLEN characters but never got a CRLF then
	// eat all the characters until we get a CRLF.
	if(i == BUFLEN)
	{
		char cur = command_buf[BUFLEN - 1];
		char last = 0;
		
		while(last != CR || cur != LF)
		{
			last = cur;
			recv(&cur, 1);
		}

		command_buf[BUFLEN - 1] = 0;
	}

	if(i < BUFLEN && i > 0 && pLength)
		*pLength = (unsigned int)i - 1;

	return i < BUFLEN;
}

void Socket::putLine(const char* command)
{
	send(command, strlen(command));
	send(CRLF, 2);
}

void Socket::send(const char* buf, int len, int flags)
{
	int bytesSent = 0;
	int totalSent = 0;

	while(totalSent < len)
	{
		bytesSent = ::send(sock, buf, len, flags);
		if(bytesSent == SOCKET_ERROR)
			throw SocketError("Error sending data");
		totalSent += bytesSent;
	}
}

void Socket::recv(char* buf, int len, int flags)
{
	int bytesReceived = 0;
	int totalReceived = 0;

	while(totalReceived < len)
	{
		bytesReceived = ::recv(sock, buf, len, flags);
		if(bytesReceived == SOCKET_ERROR)
			throw SocketError("Error receiving data");
		if(bytesReceived == 0)
			throw SocketError("The connection has been closed.");
		totalReceived += bytesReceived;
	}
}

void Socket::close()
{
	::closesocket(sock);
	sock = INVALID_SOCKET;
}
