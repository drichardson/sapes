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

#include "dns_resolve.h"
#include "utility.h"

#ifdef WIN32

// This implementation for Windows uses DnsQuery, which is only valid for
// Windows 2000 Professional and XP.

#include <windows.h>
#include <windns.h>

bool dns_resolve_mx_to_addr(const char* domain, char **phostaddr)
{
	PDNS_RECORD results = 0;
	
	if(DnsQuery(domain, DNS_TYPE_MX, DNS_QUERY_STANDARD, NULL, &results, NULL) != ERROR_SUCCESS)
		return false;

	PDNS_RECORD lowest = 0;
	for(PDNS_RECORD p = results; p; p = p->pNext)
	{
		if(p->wType == DNS_TYPE_MX)
		{
			if(!lowest)
				lowest = p;
			else if(p->Data.MX.wPreference < lowest->Data.MX.wPreference)
				lowest = p;
		}
	}

	if(!lowest)
	{
		DnsRecordListFree(results, DnsFreeRecordList);
		return false;
	}

	*phostaddr = strdupnew(lowest->Data.MX.pNameExchange);

	DnsRecordListFree(results, DnsFreeRecordList);

	return true;
}

#else
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>

bool dns_resolve_mx_to_addr(const char* domain_name, char **mx_name)
{
  char *bp, *min_pref_name;
  unsigned char answer[PACKETSZ], *cp, *eom;
  int n, ancount, qdcount, buflen, type, pref, ind, min_pref, first_time;
  char MXHostBuf[PACKETSZ - HFIXEDSZ];
  HEADER *hp;

  first_time = 1;
  min_pref_name = NULL;

  if(mx_name == 0)
	return false;

  *mx_name = NULL;
  n = res_search(domain_name, C_IN, T_MX, (unsigned char*)&answer, sizeof(answer));
  if(n == -1)
	return false;

  if(n > (int)sizeof(answer))
	n = sizeof(answer);

  hp = (HEADER*)&answer;
  cp = (unsigned char*) (answer + HFIXEDSZ);
  eom = (unsigned char*)(answer + n);
  for(qdcount = ntohs(hp->qdcount); qdcount--; cp += n + QFIXEDSZ) {
	n = dn_skipname(cp, eom);
	if(n < 0)
	  return false;
  }
  buflen = sizeof(MXHostBuf) - 1;
  bp = MXHostBuf;
  ind = 0;
  ancount = ntohs(hp->ancount);
  while(--ancount >= 0 && cp < eom) {
	n = dn_expand(answer, eom, cp, bp, buflen);
	if(n < 0)
	  break;
	cp += n;
	GETSHORT(type, cp);
	cp += INT16SZ + INT32SZ;
	GETSHORT(n, cp);
	if(type != T_MX) {
	  cp += n;
	  continue;
	}
	GETSHORT(pref, cp);
	n = dn_expand(answer, eom, cp, bp, buflen);
	if(n < 0)
	  break;
	cp += n;

	if(pref < min_pref || first_time) {
	  first_time = 0;
	  min_pref = pref;
	  min_pref_name = bp;
	}

	n = strlen(bp);
	bp += n;
	*bp++ = '\0';

	buflen -= n + 1;
  }

  if(min_pref_name == NULL)
	return false;

  *mx_name = strdupnew(min_pref_name);
  
  return true;
}
#endif
