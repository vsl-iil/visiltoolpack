#include <Windows.h>
#include <winreg.h>
#include "reg.hpp"

#pragma comment(lib,"advapi32.lib")

int OpenExclusions(HKEY* outKey) {
    // HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows Defender\Exclusions\Paths
    // HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows Defender\Exclusions\Extensions
    // HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows Defender\Exclusions\Processes
    HKEY defenderKey = NULL;
    int status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT("SOFTWARE\\Microsoft\\Windows Defender\\Exclusions"), 0, KEY_READ, &defenderKey);
    if (status != ERROR_SUCCESS) {
        return status;
    }
    *outKey = defenderKey;
    return ERROR_SUCCESS;
}

int OpenExclSubkey(HKEY excl_key, const char* subkey, HKEY* hOutKey) {
    HKEY hsubkey = NULL;
    int status = RegOpenKeyEx(excl_key, TEXT(subkey), 0, KEY_READ, &hsubkey);
    if (status != ERROR_SUCCESS) {
        return status;
    }
    *hOutKey = hsubkey;
    return ERROR_SUCCESS;
}

int QueryKey(HKEY excl_key, exceptionsState* outstate) {
    DWORD cValues = 0;
    FILETIME lastAccess;
    DWORD retCode = RegQueryInfoKey(
        excl_key,                // key handle 
        NULL,                    // buffer for class name 
        NULL,                    // size of class string 
        NULL,                    // reserved 
        NULL,                    // number of subkeys 
        NULL,                    // longest subkey size 
        NULL,                    // longest class string 
        &cValues,                // number of values for this key 
        NULL,                    // longest value name 
        NULL,                    // longest value data 
        NULL,                    // security descriptor 
        &lastAccess              // last write time 
    );

    if (retCode != ERROR_SUCCESS) {
        return retCode;
    } 

    outstate->count = cValues;
    outstate->highTime = lastAccess.dwHighDateTime;
    outstate->lowTime = lastAccess.dwLowDateTime;

    return ERROR_SUCCESS;
} 