#define SERVICE_NAME "SAPES"
#define SERVICE_DISPLAY "SAPES Mail Server"
#define SERVICE_DESCRIPTION "SAPES Mail Server System"

bool StopService(LPCSTR lpszService);
bool Start_Service(LPCSTR lpszService);
bool InstallService(LPCSTR lpszBin, LPCSTR lpszService, LPCSTR lpszDisplayName, LPCSTR lpszDescription);
bool UnInstallService(LPCSTR lpszService);
