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

#include "listener.h"
#include "log.h"
#include "options.h"

#include "signal.h"

#ifdef WIN32

#include "service.h"

VOID WINAPI SAPES_ServiceMain();//DWORD dwArgc,LPTSTR* lpszArgv); - add if needed
VOID WINAPI SAPES_ServiceCtrlHandler (DWORD Opcode);
void RemoveExecutable(char* sPath);

SERVICE_STATUS status; 
SERVICE_STATUS_HANDLE statusHandle;

#endif

static Listener *g_pListener = NULL;

void signal_handler(int signum)
{
	switch(signum)
	{
	case SIGTERM:
	case SIGINT:
#ifndef WIN32
	case SIGQUIT:
	case SIGHUP:
#endif
		if(g_pListener)
			g_pListener->Stop();
		break;
	}
}

int main(int argc, char *argv[])
{
	log_init();

	signal(SIGTERM, signal_handler);
	signal(SIGINT, signal_handler);
#ifndef WIN32
	signal(SIGQUIT, signal_handler);
	signal(SIGHUP, signal_handler);
#endif

#ifndef WIN32
	Options opts;
	if(!opts.loadValuesFromFile("sapes.conf"))
	{
		fprintf(stderr, "Error loading values from sapes.conf\n");
		return 1;
	}

	Listener listener(opts);
	g_pListener = &listener;
	int rc = listener.Run();
	g_pListener = NULL;

	log_uninit();

	return rc;

#else
	
	bool bRet;

	//TODO: To many returns - a return var is needed
	//TODO: Usage is printed twice (no param, invalid param)
	if (argc == 1) {
		printf("Usage: mailserv [stop start restart reload start_alone install uninstall help]\n");
		return FALSE;
	}

	if (strcmp(argv[1], "help") == 0) {
		printf("\nUsage: mailserv [start stop restart reload start_alone install uninstall help]\n");
		printf("       start   - starts the server service\n");
		printf("       stop    - stops the server service\n");
		printf("       restart - like running stop and then start\n");
		printf("       reload  - forces a configuration reload, without stopping the server\n");
		printf("       start_alone - starts a stand alone server (console)\n");
		printf("       install   - installs the server service\n");
		printf("       uninstall - uninstalls the server service\n");
		printf("       help      - this screen\n");
		
		return TRUE;
	} else if (strcmp(argv[1], "service_start") == 0) {
		//This starts the main service loop. It is not documented because
		//it is only meant to be executed via the Service manager - and not
		//from the command prompt (which would not benifit the user in any way)

		SERVICE_TABLE_ENTRY ste[]= {
			{SERVICE_NAME,(LPSERVICE_MAIN_FUNCTION)SAPES_ServiceMain},
			{NULL,NULL}
		};

		//TODO: Log!!!
		if (! StartServiceCtrlDispatcher(ste)) {
			printf("SAPES Service: StartServiceCtrlDispatcher (%d)\n", GetLastError()); 
			bRet = false;
		} else {
			bRet = true;
		}

		return bRet;

	} else if (strcmp(argv[1], "start") == 0) {

		if (Start_Service(SERVICE_NAME)) {
			printf("SAPES Service started...\n");
			bRet = true;
		} else {
			printf("Could not start service... (already started?)\n");
			bRet = false;
		}

		return bRet;
		
	} else if (strcmp(argv[1], "stop") == 0) {

		if (StopService(SERVICE_NAME)) {
			printf("SAPES Service stopped...\n");
			bRet = true;
		} else {
			printf("SAPES Service could not be stopped... (stopped already?)\n");
			bRet = false;
		}

		return bRet;

	} else if (strcmp(argv[1], "restart") == 0) {
		
		StopService(SERVICE_NAME); //Might already be stopped - we don't really care if this works
		
		if (Start_Service(SERVICE_NAME)) {
			printf("SAPES Service restarted...\n");
			bRet = true;
		} else {
			printf("Could not restart service...\n");
			bRet = false;
		}

		return bRet;

	} else if (strcmp(argv[1], "reload") == 0) {

		//TODO: Implement custom message that will signal an options reload
	
	} else if (strcmp(argv[1], "start_alone") == 0) {
		char *sPath = (char *)malloc(1024);

		//Determine the real path of the service file (which is where sapes.conf is
		//hopefully sitting)
		GetModuleFileName(NULL, sPath, 1024);
		RemoveExecutable(sPath);
		strcat(sPath, "\\sapes.conf");

		//Load options from sapes.conf (which is now fully pathed in sPath)
		Options opts;

		if(!opts.loadValuesFromFile(sPath)) {
			free(sPath); //dealloc memory before exiting
			return 0;
		}

		free(sPath); //dealloc memory before continuing
	
		Log logger;

		logger.log(LOG_STATUS, "Server starting in stand-alone mode..");

		Listener listener(opts);
		g_pListener = &listener;
		int rc = listener.Run();
		g_pListener = NULL;

		logger.log(LOG_STATUS, "Server shutting down..");

		log_uninit();
		
		return rc;
	} else if (strcmp(argv[1], "install") == 0) {

		char *sExec = (char *)malloc(strlen(_pgmptr) + strlen(" service_start") + 1);

		strcpy(sExec,_pgmptr);
		strcat(sExec, " service_start");

		//if _pgmptr is unusable for some reason use the GetModuleFileName API call
		if (InstallService(sExec, SERVICE_NAME, SERVICE_DISPLAY, SERVICE_DESCRIPTION)) {
			printf("SAPES Service installed...\n");
			bRet = true;
		} else {
			printf("Could not install SAPES Service (already installed?)\n");
			bRet = false;
		}

		free (sExec);

		return bRet;

	} else if (strcmp(argv[1], "uninstall") == 0) {
		if (UnInstallService(SERVICE_NAME)) {
			printf("SAPES Service uninstalled...\n");
			return true;
		} else {
			printf("Could not uninstall SAPES service (running? not installed?)\n");
			return false;
		}
	} else {
		printf("Usage: mailserv [stop start restart reload start_alone install uninstall help]\n");
		return FALSE;
	}

	printf("%d\n", argc);
	printf("%s\n", argv[1]);

	Options opts;
	if(!opts.loadValuesFromFile("sapes.conf"))
	{
		fprintf(stderr, "Error loading values from sapes.conf\n");
		return 1;
	}

	return TRUE;

#endif //win32
}

#ifdef WIN32
/*
//Niftly little debug tool for suddenly crashing services :)
void out(char* text) {
	FILE *stream = fopen("c:\\temp.txt", "a");
	fprintf(stream, "%s", text);
	fclose(stream);
}*/

/* RemoveExecutable
   ================
   General: Returns only the path part of a windows path to an executable
   Usage  : RemoveExecutable(path to a file ex: c:\temp.txt)
   Returns: Nothing (puts a \0 char in the right part of the string though... c:\temp.txt -> c:\\0emp.txt
            which is read as c:\)
*/
//TODO: Shouldn't this be in utility.cpp??
void RemoveExecutable(char* sPath) {
	char* found = strrchr(sPath, '\\');
	if (found != NULL) {
		sPath[(int)(found - sPath + 1)] = '\0';
	}
}

void WINAPI SAPES_ServiceMain() { //DWORD dwArgc,LPTSTR* lpszArgv) { - add if needed
	
	char *sPath = (char *)malloc(1024);

    status.dwServiceType        = SERVICE_WIN32; 
    status.dwCurrentState       = SERVICE_START_PENDING; 
    status.dwControlsAccepted   = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_PAUSE_CONTINUE; 
    status.dwWin32ExitCode      = 0; 
    status.dwServiceSpecificExitCode = 0; 
    status.dwCheckPoint         = 0; 
    status.dwWaitHint           = 0; 
 
	statusHandle = RegisterServiceCtrlHandler(SERVICE_NAME, SAPES_ServiceCtrlHandler);

	if (statusHandle == (SERVICE_STATUS_HANDLE)0) {
        printf(" SAPES Service: RegisterServiceCtrlHandler failed %d\n", GetLastError()); 
        return; 
    } 

	//TODO: Server init goes here. Nothing heavy - can not exceed 30 seconds (not that
	//      it should take that long to start anyway (currently: loads opts)

	
	//Determine the real path of the service file (which is where sapes.conf is
	//hopefully sitting)
	GetModuleFileName(NULL, sPath, 1024);
	RemoveExecutable(sPath);
	strcat(sPath, "\\sapes.conf");

	//Load options from sapes.conf (which is now fully pathed in sPath)
	Options opts;
	if(!opts.loadValuesFromFile(sPath)) {
		//fprintf(stderr, "Error loading values from sapes.conf\n");
		free(sPath); //dealloc memory before exiting
		return;
	}

	free(sPath); //dealloc memory before continuing
	
	//Finished init - set state to running...
	status.dwCurrentState       = SERVICE_RUNNING; 
    status.dwCheckPoint         = 0; 
    status.dwWaitHint           = 0; 
	
    if (!SetServiceStatus (statusHandle, &status)) {
        printf(" SAPES Service: SetServiceStatus error %ld\n",GetLastError()); 
    } 

	//At last! Service can start to do some actual work - ashame it isn't as easy as main() {<work>};
	
	Listener listener(opts);
	g_pListener = &listener;

	listener.Run(); //removed: int rc = ; currently the value is not used - should be logged
	g_pListener = NULL;

	log_uninit();

	//TODO: Function does not return a value - the value should be logged
	return;

}

VOID WINAPI SAPES_ServiceCtrlHandler (DWORD dwOpCode) {
  
   switch(dwOpCode) 
   { 
      case SERVICE_CONTROL_PAUSE: 
      // Do whatever it takes to pause here.
         status.dwCurrentState = SERVICE_PAUSED; 
         break; 
 
      case SERVICE_CONTROL_CONTINUE: 
      // Do whatever it takes to continue here. 
         status.dwCurrentState = SERVICE_RUNNING; 
         break; 
 
      case SERVICE_CONTROL_STOP: 
      // Do whatever it takes to stop here. 
         status.dwWin32ExitCode = 0; 
         status.dwCurrentState  = SERVICE_STOPPED; 
         status.dwCheckPoint    = 0; 
         status.dwWaitHint      = 0; 

         if (!SetServiceStatus (statusHandle, 
           &status))
         { 
            printf(" SAPES Service: SetServiceStatus error %ld\n", 
               GetLastError()); 
         } 
 
         printf(" SAPES Service: Leaving MyService \n",0); 
         return; 
 
      case SERVICE_CONTROL_INTERROGATE: 
      // Fall through to send current status. 
         break; 
 
      default: 
         printf(" SAPES Service: Unrecognized opcode %ld\n", 
             dwOpCode); 
   } 
 
   // Send current status. 
   if (!SetServiceStatus (statusHandle,  &status)) 
   { 
      printf(" SAPES Service: SetServiceStatus error %ld\n", GetLastError()); 
   } 

   return; 

}

#endif
