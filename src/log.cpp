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

#include "log.h"
#include "thread.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <io.h>
#include <fcntl.h>

static bool log_mutex_set = false;
static MUTEX log_mutex;

static log_opts_struct log_opts;

void log_init()
{
	if(!log_mutex_set)
	{
		log_mutex_set = create_mutex(log_mutex);
		if(!log_mutex_set)
		{
			fprintf(stderr, "Fatal Error: Could not create log mutex.\n");
			exit(1);
		}
	}

	log_opts.loglevel = 1;
	log_opts.timestamp = 0;
	log_opts.max_file_size = LOG_MAX_SIZE;
}

void log_uninit()
{
	if(log_mutex_set)
	{
		delete_mutex(log_mutex);
		log_mutex_set = false;
	}
	
	free(log_opts.file_name);
}

/* Log::Log() ( - Overloaded)
   ==========
   General: Log class constructor
   Usage  : Log <var name>(<log file name>);
   Returns: void

   Created By: Douglas Ryan Richardson
   Updates   : Oren Nachman - Receive file name
*/

Log::Log() {
	m_usepvt = false;
}

Log::Log(char* sFile) {
	m_usepvt = false;
	file(sFile);
}

Log::~Log() {
	if (m_usepvt) 
		free(m_pvtfile);
}

/* Log::file()
   ===========
   General: Sets the path to the log file
   Usage  : Log::file(<filename>)
   Returns: The file name

   Created By: Oren Nachman
*/
char* Log::file(char *sFile) {
	
	free(log_opts.file_name);

	log_opts.file_name = strdup(sFile);

	return log_opts.file_name;
}

/* Log::log()
   ========== 
   General: Logs a message
   Usage  : As with printf etc. ->log("<string format>", params);
   Returns: void
   Note   : Updated to log to a file.

   Created By: Douglas Ryan Richardson
   Updates   : Oren Nachman - Log to file
*/
void Log::log(const char* format, ...) const {

	if (wait_mutex(log_mutex)) {
		va_list va;
		FILE* fLog;
		char* filename;


		va_start(va, format);

		//I was considering putting in fall back code - where if m_pvtfile can not be opened then 
		//logs fall back to the main log - but this would be counterproductive (think of wading through
		//giant logs)
		if (m_usepvt)
			filename = m_pvtfile;
		else 
			filename = log_opts.file_name;
		
		//If we can't open the file (or the file name is null) then set our file
		//stream to stdout (so that we can continue normally and not handle error
		//situations with seperate code)
		if (!filename || (*filename == '\0') || (fLog = fopen(filename, "a")) == NULL) {
			fLog = stdout;
		}
			
		if (log_opts.timestamp) {
			time_t cur;
			char time_string[26];
			
			time(&cur);
			
			strcpy(time_string, ctime(&cur));
			time_string[24] = '\0';
			
			fprintf(fLog, "%s : ", time_string);
		}

		vfprintf(fLog, format, va);
		fputc('\n', fLog);
		

		if (fLog != stdout) {
			fclose(fLog);
			CheckLogSize(filename);
		}

		va_end(va);

		release_mutex(log_mutex);
	}
}

/* Log::CheckLogSize() - Overloaded
   ===================
   General: Checks the size of the log file and moves it to an archive if
            needed - this forces the next log() call to create a new archive
   Usage  : CheckLogSize(filename) - can also be called externally (not sure why you
            would want to though)
   Returns: void
   Note   : function is const because it is called by log() which is also const
   
   Created By: Oren Nachman
*/
void Log::CheckLogSize(char* filename) const {
	int log_file = open(filename, _O_RDONLY);

	if ((unsigned long)filelength(log_file) > log_opts.max_file_size) {
		
		time_t tLocal;
		tm *tmLocal;
		
		char* archive = (char *)malloc(2056);
		int iFiles = 1;

		time(&tLocal);
		tmLocal = localtime(&tLocal);
		
		sprintf(archive, "%s.%d%d%d", filename, tmLocal->tm_mday, tmLocal->tm_mon, tmLocal->tm_year);
		
		close(log_file); //close so that we can move the file

		while(rename(log_opts.file_name, archive) && iFiles < 50) {
			sprintf(archive, "%s.%d%d%d.%d", filename, tmLocal->tm_mday, tmLocal->tm_mon, tmLocal->tm_year, iFiles++);
		}

		if (iFiles >= 50) {
			log("Log::CheckLogSize: Current log exceeds max size - but can not be moved to an archive (after 50 tries)! - %s", archive);
		} else {
			log("Log::CheckLogSize: Log exceeded maximum size and was archived to: %s", archive);
		}

		free(archive);
	}
	
	close(log_file);
}

/* Log::timestamp() - Overloaded
   ================
   General: Sets the timestamp parameter which govers whether or not the log
            file will prepend it's items with a timestamp
   Usage  : Log::timestamp(true / false)
   Returns: void / timestamp (true / false)
   
   Created By: Oren Nachman
*/

void Log::timestamp(bool opt) {
	log_opts.timestamp = opt;
}

bool Log::timestamp() {
	return log_opts.timestamp;
}

/* Log::loglevel() - Overloaded
   ===============
   General: Determines the level of logging
            1 - Errors
			2 - Warnings
			3 - Status Messages
			4 - Server interactions
			5 - Full
   Usage  : Log::loglevel() or Log::loglevel(1->5)
   Returns: current log level / 0 - invalid log level or the log level that was specified
   
   Created By: Oren Nachman
*/
int Log::loglevel(int level) {

	if (level < 1 || level > 5) {
		return 0; //Invalid log level, leave log_level at default
	}

	return (log_opts.loglevel = level);
}

int Log::loglevel() {
	return log_opts.loglevel;
}

/* Log::PrivateLogFile() - Overloaded
   =====================
   General: Sets a log file only for this instance of the Log class
            Good for logging a server session to a seperate file instead
			of clogging the main log file
   Usage  : Log::PrivateLogFile(<filename>)
   Returns: the log file
   
   Created By: Oren Nachman
*/
char* Log::PrivateLogFile(char* filename) {
	
	if (filename != NULL) 
		if (m_usepvt) 
			free(m_pvtfile);
		else
			m_usepvt = true;

	m_pvtfile = strdup(filename);

	return m_pvtfile;
}

char* Log::PrivateLogFile() {
	return m_pvtfile;
}
