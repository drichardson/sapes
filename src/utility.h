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

#ifndef MAILSERV_UTILITY_H
#define MAILSERV_UTILITY_H

#include "socket.h"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

#ifdef WIN32
#define strncasecmp strnicmp
#define strcasecmp stricmp
#define sleep(x) Sleep(1000 * x)
#define DIR_DELIM '\\'
#else
#include <limits.h>
#define MAX_PATH PATH_MAX
#define DIR_DELIM '/'
#endif

#define MAX_HOSTNAME_LEN 500

// command line length - from RFC 2821
//		The maximum total length of a command line including the command
//		word and the <CRLF> is 512 characters.  SMTP extensions may be
//		used to increase this limit.
#define SMTP_MAX_COMMAND_LENGTH 512

// reply line - from RFC 2821
//		The maximum total length of a reply line including the reply code
//		and the <CRLF> is 512 characters.  More information may be
//		conveyed through multiple-line replies.
#define SMTP_MAX_REPLY_LENGTH 512

// text line - from RFC 2821
//		The maximum total length of a text line including the <CRLF> is
//		1000 characters (not counting the leading dot duplicated for
//		transparency).  This number may be increased by the use of SMTP
//		Service Extensions.
#define SMTP_MAX_TEXT_LINE 1000

// response line - from RFC 1939
//		Responses in the POP3 consist of a status indicator and a keyword
//		possibly followed by additional information.  All responses are
//		terminated by a CRLF pair.  Responses may be up to 512 characters
//		long, including the terminating CRLF.  There are currently two status
//		indicators: positive ("+OK") and negative ("-ERR").  Servers MUST
//		send the "+OK" and "-ERR" in upper case.
#define POP3_MAX_RESPONSE_LENGTH 512

// Carraige return
#define CR 0x0D

// Linefeed
#define LF 0x0A

extern const char CRLF[3];

// Duplicate a string using new to allocate memory.
char* strdupnew(const char* src);

// Copy src to dest. Dest has a size of maxlen. This function makes
// sure that dest is NULL terminated (unlike strncpy).
char* safe_strcpy(char* dest, const char* src, size_t bufsize);

// Works just like snprintf execpt it always NULL terminates buf, unless
// bufsize is 0.
int safe_snprintf(char* buf, size_t bufsize, const char* format, ...);

// Works just like vsnprintf execpt it always NULL terminates buf, unless
// bufsize is 0.
int safe_vsnprintf(char* buf, size_t bufsize, const char* format, va_list va);

// Create a file in the given directory and sets
// pNewFileName to the name of the new file, if pNewFileName is not null.
// The function uses new to allocate memory for *pNewFileName. The file
// is created with "wb" passed to fopen. If you don't want a prefix then
// pass NULL as the prefix.
FILE* newfile(const char* path, const char* prefix, char** pNewFileName);

// Returns true if path is a directory and false otherwise.
bool isdir(const char* path);

// Converts ascii text to in_addr struct.  NULL is returned if the 
// address can not be found.
struct in_addr *atoaddr(const char *address, in_addr * psaddr);


// nexttoken gets the next token from input and sets pstr to the location
// after the token.
bool nexttoken(char** pstr, char* buf, int maxbuf);

// If mailbox_ok returns true then it points local_part and domain_part to
// newly allocated memory (with new) that contains the local part and domain part
// of the mailbox. You must free these with delete[]. If you pass in NULL for
// one of these arguments then the local or domain part is not set.
bool mailbox_ok(const char* mailbox, char** plocal_part_out, char** pdomain_part_out);

// Get a RFC 2822 date. If the date is successfully stored in buf, then true is returned.
// Otherwise, false is returned.
bool get_rfc_2822_datetime(time_t t, char *buf, size_t bufSize);


#endif
