#include <Windows.h>
#include <cstring>
#include <stdio.h>
#include <TlHelp32.h> 

#define check(result, errortext) if (!result) { printf("%s\nSystem error: %lu", errortext, GetLastError()); exit(-1); }

void createRemoteThread(DWORD pid, const char* dllpath) {
    HANDLE handle = OpenProcess(
        PROCESS_QUERY_INFORMATION   |
        PROCESS_CREATE_THREAD       |
        PROCESS_VM_OPERATION        |
        PROCESS_VM_WRITE, 
        FALSE, 
        pid
    );

    check(handle, "Failed to open process");

    size_t pathsize = strlen(dllpath);
    void* remotepath = VirtualAllocEx(handle, NULL, pathsize+1, MEM_COMMIT, PAGE_READWRITE);

    check(remotepath, "Failed to alloce page in remote process");

    BOOL written = WriteProcessMemory(handle, remotepath, dllpath, pathsize, NULL);

    check(written, "Failed to write memory in remote process");

    PTHREAD_START_ROUTINE pLoadLibraryA = (PTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandle(TEXT("Kernel32.dll")), "LoadLibraryA");

    check(pLoadLibraryA, "Failed to locate LoadLibraryA in remote process");

    HANDLE hwnd = CreateRemoteThread(handle, NULL, 0, pLoadLibraryA, remotepath, 0, NULL);

    check(hwnd, "Failed to create remote thread");

    WaitForSingleObject(hwnd, INFINITE);

    VirtualFreeEx(handle, remotepath, 0, MEM_RELEASE);
    CloseHandle(hwnd);
    CloseHandle(handle);
}

DWORD findPIDbyName(const char* procname) {
    HANDLE h;
    PROCESSENTRY32 singleProcess;
    h = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    singleProcess.dwSize = sizeof(PROCESSENTRY32);

    do {
        if (strcmp(singleProcess.szExeFile, procname) == 0) {
            DWORD pid = singleProcess.th32ProcessID;
            printf("Found PID %lu", pid);
            CloseHandle(h);
            return pid;
        }
    } while (Process32Next(h, &singleProcess));

    CloseHandle(h);

    return 0;
}


int main(int argc, char** argv) {
    if (argc != 3) {
        char* name = strrchr(argv[0], '\\')+1;
        printf("Usage: %s <PROCESS NAME> <DLL PATH>", name);

        return -1;
    }

    createRemoteThread(findPIDbyName(argv[1]), argv[2]);

    return 0;
}