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
}

Log::Log(char* sFile) {
	//Log::Log(); //Go do whatever is normally done - currently not needed
	file(sFile);
}

Log::~Log() {
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
//	log_opts.file_name = (char *)malloc(strlen(sFile));
	log_opts.file_name = strdup(sFile);
//	strcpy(log_opts.file_name, sFile);

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
		
		va_start(va, format);

	//	if (log_opts.file_name 

		//If we can't open the file (or the file name is null) then set our file
		//stream to stdout (so that we can continue normally and not handle error
		//situations with seperate code)
		if (!log_opts.file_name || (*log_opts.file_name == '\0') || (fLog = fopen(log_opts.file_name, "a")) == NULL) {
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
		
		if (fLog != stdout) 
			fclose(fLog);

		va_end(va);

		release_mutex(log_mutex);
	}
}

/* Log::timestamp()
   =================
   General: Sets the timestamp parameter which govers whether or not the log
            file will prepend it's items with a timestamp
   Usage  : Log::timestamp(0/1)
   Returns: void
   
   Created By: Oren Nachman - Log to file
*/

void Log::timestamp(bool opt) {
	log_opts.timestamp = opt;
}

