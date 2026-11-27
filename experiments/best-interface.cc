#include <winsock2.h>
#include <iphlpapi.h>
#include <stdio.h>
#include <unistd.h>           // sleep

// Automatically instructs MSVC to link iphlpapi.lib
#ifdef _MSC_VER
#pragma comment(lib, "iphlpapi.lib")
#endif

int main(void) {
    DWORD bestIfIndex = 0;

    // Calling GetBestInterface forces the linker to add iphlpapi.dll to the IAT.
    // Even if this code branch were never reached at runtime, the DLL still loads at startup.
    DWORD result = GetBestInterface(0x08080808, &bestIfIndex); // 8.8.8.8

    if (result == NO_ERROR) {
        printf("iphlpapi.dll loaded. Best interface index: %lu\n", bestIfIndex);
    } else {
        printf("iphlpapi.dll loaded. GetBestInterface returned error: %lu\n", result);
    }

    //sleep(10);

    return 0;
}
