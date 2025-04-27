#include <Windows.h>
#include <stdio.h>
#include <winnt.h>
#include <winuser.h>

INT APIENTRY DllMain(HMODULE hDll, DWORD Reason, LPVOID Reserved) {
    printf("SUCCESS!");
    MessageBox(NULL, TEXT("Hurrah!"), TEXT("!!!"), 0);

    return TRUE;
}
