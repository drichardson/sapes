#include <windows.h>
#include "service.h"

/* service.cpp - Service Handling Interface
   ========================================

   Handles:
    1. Installing / Deleteing a service
	2. Start / Stop service
	3. Run service (ServiceMain)
	4. Service settings (?)
*/

/* Start_Service()
   ================
   General: Tells the service manager to start the specified service
   Usage  : Start_Service(service name)
   Returns: True (success) / False
   Note   : Function has underscore (_) because there is a real windows function
            called StartService
*/
bool Start_Service(LPCSTR lpszService) {
	bool bRet;
	
	SC_HANDLE scManager = OpenSCManager( 
							NULL,                    
							NULL,                    
							SC_MANAGER_CONNECT);

    SC_HANDLE scService = OpenService( 
        scManager,       
        lpszService,       
        SERVICE_START);            

	if (scService == NULL) 
		return false;
	
	//Simple start - with no args
	if (! StartService(scService, 0, NULL)) {
		bRet = false;
	} else {
		bRet = true;
	}
	
	CloseServiceHandle(scService);
	CloseServiceHandle(scManager);

	return bRet;
}

/* StopService()
   ================
   General: Stops the specified service
   Usage  : StopService(service name)
   Returns: True (success) / False
*/
bool StopService(LPCSTR lpszService) {
	
	bool bRet;
	SERVICE_STATUS ss;
	SC_HANDLE scManager = OpenSCManager( 
							NULL,                    
							NULL,                    
							SC_MANAGER_CONNECT);

    SC_HANDLE scService = OpenService( 
        scManager,       
        lpszService,       
        SERVICE_STOP);            

	if (scService == NULL) 
		return false;

	if (! ControlService(scService,SERVICE_CONTROL_STOP,&ss)) {
		bRet = false;
	} else {
		bRet = true;
	}

	CloseServiceHandle(scService);
	CloseServiceHandle(scManager);

	return bRet;
}

/* InstallService()
   ================
   General: Installs the specified service (lpszeBin) using the provided name (lpszService) (and display name).
   Usage  : InstallService(path to .exe file, Service Name for service manager, 
						   Service Displayed name, Service Description);
   Returns: True (success) / False
   Future : Currently the function creates a basic service with hard coded options, maybe these options should
            be part of the parameters?
			Add a call to 
*/
bool InstallService(LPCSTR lpszBin, LPCSTR lpszService, LPCSTR lpszDisplayName, LPCSTR lpszDescription) {

	bool bRet = false;

	//Connect to the Service Control Manager on the local machine with service create rights
	SC_HANDLE scManager = OpenSCManager( 
								NULL,                    
								NULL,                    
								SC_MANAGER_CREATE_SERVICE);

	//TODO: scManager didn't open
	//Create the service entry in the database
	SC_HANDLE scService = CreateService( 
								scManager,                 // SCManager database 
								lpszService,	           // name of service 
								lpszDisplayName,           // service name to display 
								SERVICE_ALL_ACCESS,        // desired access 
								SERVICE_WIN32_OWN_PROCESS, // service type 
								SERVICE_AUTO_START,        // start type 
								SERVICE_ERROR_NORMAL,      // error control type 
								lpszBin,                   // service's binary 
								NULL,                      // no load ordering group 
								NULL,                      // no tag identifier 
								NULL,                      // no dependencies 
								NULL,                      // LocalSystem account 
								NULL);                     // no password 

	(scService == NULL) ? (bRet = false) : (bRet = true);

	ChangeServiceConfig2(scService, SERVICE_CONFIG_DESCRIPTION, &lpszDescription);

	CloseServiceHandle(scService); //Close Service Handle
	CloseServiceHandle(scManager); //Close Service Manager Handle

	return bRet;
}
/* UninstallService()
   ==================
   General: Delete a service according to the service name (real name, not display name)
   Usage  : UninstallService(service name);
   Returns: true / false
*/
bool UnInstallService(LPCSTR lpszService) {

	//Connect to the Service Control Manager on the local machine with service create (delete) rights
	SC_HANDLE scManager = OpenSCManager( 
								NULL,       
								NULL,                    
								SC_MANAGER_CREATE_SERVICE);

	//Open a handle to the service - so that it can be deleted
    SC_HANDLE scService = OpenService( 
        scManager,       
        lpszService,       
        DELETE);            
 
	if (scService == NULL) //Service is open and can not be deleted
		return FALSE;

    if (! DeleteService(scService) ) //Error Occurred trying to delete the service
		return FALSE;
 
    CloseServiceHandle(scService); 
	return TRUE;
}
