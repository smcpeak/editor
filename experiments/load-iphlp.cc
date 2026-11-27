#include <windows.h>
#include <stdio.h>

int main(void) {
    // Explicitly load iphlpapi.dll at runtime
    HMODULE hIphlpapi = LoadLibraryA("iphlpapi.dll");

    if (hIphlpapi != NULL) {
        printf("Successfully loaded iphlpapi.dll at address: %p\n", (void*)hIphlpapi);

        // Unload the DLL when finished
        FreeLibrary(hIphlpapi);
    } else {
        DWORD err = GetLastError();
        printf("Failed to load iphlpapi.dll. Windows Error Code: %lu\n", err);
    }

    return 0;
}
