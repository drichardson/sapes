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

#include "utility.h"
#include "string.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <ctype.h>

#ifdef WIN32
#define vsnprintf _vsnprintf
#define snprintf _snprintf
#endif

const char CRLF[3] = { CR, LF, '\0' };

char* strdupnew(const char* src)
{
	return strcpy(new char[strlen(src) + 1], src);
}

char* safe_strcpy(char* dest, const char* src, size_t bufsize)
{
	strncpy(dest, src, bufsize);
	dest[bufsize - 1] = 0;
	return dest;
}

int safe_snprintf(char* buf, size_t bufsize, const char* format, ...)
{
	int retval;

	va_list va;
	va_start(va, format);

	retval = safe_vsnprintf(buf, bufsize, format, va);
	va_end(va);

	return retval;
}

int safe_vsnprintf(char* buf, size_t bufsize, const char* format, va_list va)
{
	int rc = 0;

	if(bufsize > 0)
	{
		rc = vsnprintf(buf, bufsize, format, va);
		buf[bufsize - 1] = 0;
		if(rc > 0 && (unsigned)rc == bufsize)
			--rc; // Return what strlen would return (which doesn't include the NULL terminator).
	}

	return rc;
}

#ifdef WIN32

FILE* newfile(const char* path, const char* prefix, char** pNewFileName)
{
	char tmpfile_path[MAX_PATH + 1];

	// For Windows we use GetTempFileName because mktemp and tempname
	// fail after a fix number of calls. This is only a problem with
	// the Windows C Runtime library.

	if(GetTempFileName(path, prefix, 0, tmpfile_path) == 0)
		return NULL;

	FILE* fp = fopen(tmpfile_path, "wb");

	if(fp && pNewFileName)
		*pNewFileName = strdupnew(tmpfile_path);

	return fp;
}

#else
FILE* newfile(const char* path, const char* prefix, char** pNewFileName)
{
  char filename[MAX_PATH + 1];
  safe_snprintf(filename, sizeof filename, "%s/%sXXXXXX", path, prefix);
  int fd = mkstemp(filename);

  if(fd == -1)
	return NULL;

  *pNewFileName = strdupnew(filename);

  // Attach the file descriptor to the stream.
  return fdopen(fd, "w");
}

#endif

bool isdir(const char* path)
{
	struct stat sb;
	
	if(stat(path, &sb) != 0)
		return false;

	return sb.st_mode && S_IFDIR;
}

struct in_addr *atoaddr(const char *address, in_addr * psaddr)
{
   struct hostent *host;

   /* First try it as aaa.bbb.ccc.ddd. */
   psaddr->s_addr = inet_addr(address);
   if ((int)psaddr->s_addr != -1)
    return psaddr;



#ifdef WIN32
   // On Windows one copy of the returned structure is allocated per
   // thread
   host = gethostbyname(address);
#else
   // On Linux, you must use gethostbyname_r.
   hostent host_buf;
   char buf[1024];
   int herr;
   // I don't understand where you get the size of buf. The documentation
   // just says to pass it in, but not how big to make it.
   gethostbyname_r(address, &host_buf, buf, sizeof(buf), &host, &herr);
#endif

   if (host != NULL)
    return (struct in_addr *) *host->h_addr_list;
    
   return NULL;
}

// nexttoken gets the next token from input and sets pstr to the location
// after the token.
bool nexttoken(char** pstr,
			   char* buf,
			   int maxbuf,
			   const char* delim,
			   const char* delim_tokens)
{
	char *s = *pstr;
	char *p = buf;
	const char* DELIM = delim != NULL ? delim : ":<> \t\r\n";
	const char* DELIM_TOKENS = delim_tokens != NULL ? delim_tokens : ":<>";

	// Skip whitespace.
	while(*s && isspace(*s))
		++s;

	if(!*s)
		return false;

	char *found_delim = strchr(DELIM_TOKENS, *s);

	if(found_delim != NULL)
	{
		buf[0] = *s;
		buf[1] = 0;
		++s;
		*pstr = s;
		return true;
	}


	// Copy the next token into buf, but not more than maxbuf - 1 characters.
	while((p - buf) < maxbuf && *s && (found_delim = strchr(DELIM, *s)) == NULL)
	{
		*p = *s;
		++p;
		++s;
	}

	if((p - buf) >= maxbuf)
		return false;

	// NULL terminate p (which points to part of buf).
	*p = 0;

	*pstr = s;

	return true;
}

// A syntactically valid IP address is:
// X.X.X.X, where 255 >= X >= 0.
static bool isIP4addr(const char* addr)
{
	// Read four triplets.
	int index = 0;
	for(int i = 0; i < 4; ++i)
	{
		char triplet[4];

		// Read a single triplet.
		int j;
		for(j = 0; j < 4; ++i)
		{
			triplet[j] = addr[index++];
			if(!isdigit(triplet[j]))
				break;
		}
		if(triplet[j] != '.')
			return false;
		triplet[j] = 0;
		if(strlen(triplet) == 0)
			return false;
		int val = atoi(triplet);
		if(val < 0 || val > 255)
			return false;
	}

	return true;
}

// A syntactically valid domain name is
// x[.x]*, where x is a digit, letter, or hyphen.
static bool isDomain(const char* addr)
{
	if(!addr)
		return false;

	bool first = true;
	int c;
	while((c = *(addr++)) != '\0')
	{
		if(isalnum(c))
			first = false;
		else if(c == '-' || c == '.')
		{
			if(first)
				return false; // Can't start a subdomain with a '-' or '.'.

			if(c == '.')
				first = true;
		}
		else
			return false; // Invalid character.
	}

	return true;
}

// A syntactially valid local part is either
// "quoted content"
// or
// str[.str]*, where str is any alphanumeric character.
static bool isLocalPart(const char* local_part)
{
	if(!local_part)
		return false;

	if(*local_part == '\"')
	{
		// This is a quoted string.
		int c;
		bool found_end_quote = false;
		while(!found_end_quote && (c = *(local_part++)) != '\0')
		{
			if(c == '\"')
				found_end_quote = true;
		}

		if(found_end_quote && *local_part) // If the quote wasn't the last character.
			return false;
	}
	else
	{
		// This is a dot string.
		bool first = true;
		int c;
		while((c = *(local_part++)) != '\0')
		{
			if(isalnum(c))
				first = false;
			else if(c == '.')
			{
				if(first)
					return false;
				first = true;
			}
		}
	}

	return true;
}


// Mailbox BNF from RFC 2821.
//	Mailbox = Local-part "@" Domain
//
//	Local-part = Dot-string / Quoted-string
//		; MAY be case-sensitive
//
//	Dot-string = Atom *("." Atom)
//
//	Atom = 1*atext
//
//	Quoted-string = DQUOTE *qcontent DQUOTE
//
//	String = Atom / Quoted-string
//
//	Domain = (sub-domain 1*("." sub-domain)) / address-literal
//
//	sub-domain = Let-dig [Ldh-str]
//
//	address-literal = "[" IPv4-address-literal /
//                        IPv6-address-literal /
//                        General-address-literal "]"

// If mailbox_ok returns true then it points local_part and domain_part to
// newly allocated memory (with new) that contains the local part and domain part
// of the mailbox. You must free these with delete[]. If you pass in NULL for
// one of these arguments then the local or domain part is not set.
bool mailbox_ok(const char* mailbox, char** plocal_part_out, char** pdomain_part_out)
{
	if(!mailbox)
		return false;

	char *tmp_mailbox = strdupnew(mailbox);
	char *s = tmp_mailbox;

	// Skip white space at beginning of mailbox.
	while(*s && isspace(*s))
		++s;

	if(!*s)
		return false;

	// Strip white space from end of mailbox.
	size_t len = strlen(s);
	char *end = s + len;
	while(end > s && isspace(*(end - 1)))
		--end;

	// Make sure this isn't the empty string.
	if(end == s)
		return false;

	// Chop off the tailing white space.
	*end = 0;

	bool rc = false;
	char *local_part = s;
	char *domain = strchr(s, '@');

	if(local_part && *local_part != '@' && domain && domain[1])
	{
		// At this point we know that there is a local part and domain part.
		// Now we need to make sure they are valid.

		*domain = 0; // Replace '@' with NULL to break up the mailbox into two strings.
		++domain;

		if(isLocalPart(local_part) && (isDomain(domain) || isIP4addr(domain)))
		{
			if(plocal_part_out)
				*plocal_part_out = strdupnew(local_part);
			if(pdomain_part_out)
				*pdomain_part_out = strdupnew(domain);
			rc = true;
		}
	}

	delete[] tmp_mailbox;
	return rc;
}

static char *Days[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
static char *Months[] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

bool get_rfc_2822_datetime(time_t t, char *buf, size_t bufSize)
{
	int minutes_offset = (-timezone) / 60;
	char sign = minutes_offset >= 0 ? '+' : '-';
	int hours_offset = minutes_offset / 60;
	minutes_offset -= (hours_offset * 60);

#ifdef WIN32	
	struct tm * ptime = localtime(&t);
#else
	struct tm tm_buf;
	struct tm * ptime = localtime_r(&t, &tm_buf);
#endif

	minutes_offset = minutes_offset > 0 ? minutes_offset : -minutes_offset;
	hours_offset = hours_offset > 0 ? hours_offset : -hours_offset;

	int rc = safe_snprintf(buf, bufSize, "%s, %02u %s %u %02u:%02u:%02u %c%02d%02d",
		Days[ptime->tm_wday],
		ptime->tm_mday,
		Months[ptime->tm_mon],
		ptime->tm_year + 1900,
		ptime->tm_hour,
		ptime->tm_min,
		ptime->tm_sec,
		sign,
		hours_offset,
		minutes_offset);

	return rc >= 0;
}
