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

#include "config_file.h"
#include "utility.h"

ConfigFile::ConfigFile(const char* config_filename)
{
	m_filename = strdupnew(config_filename);
}

ConfigFile::~ConfigFile()
{
	delete[] m_filename;
}

// getLine returns true if it got the entire line. Otherwise it returns false.
bool ConfigFile::getLine(FILE *fp, char* buf, size_t bufSize)
{
	int c;
	size_t cur = 0;

	if(bufSize == 0)
		return false;

	while((c = getc(fp)) != EOF && c != '\n' && cur < bufSize - 1)
	{
		buf[cur] = (char)c;
		++cur;
	}

	if(cur > 0)
		buf[cur] = 0;

	return c == EOF || c == '\n';
}

bool ConfigFile::getValue(const char* valuename, char* value, size_t valueBufSize)
{
	FILE *fp = fopen(m_filename, "rt");
	char line[MAX_CONFIGFILE_LINE_LEN + 1];
	bool found = false;

	if(!fp)
		return false;

	// Go through the lines until we find valuename.
	size_t len = strlen(valuename);
	while(!found && !feof(fp) && getLine(fp, line, sizeof line))
	{
		if(strncasecmp(valuename, line, len) == 0)
		{
			if(line[len] == ':')
			{
				safe_strcpy(value, line + len + 1, valueBufSize);
				found = true;
			}
		}
	}

	fclose(fp);
	return found;
}
