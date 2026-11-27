// long-name-load.c
// Open a very long file name, then load a DLL; fails under Sandboxie.

// Build (MinGW):
//
//   gcc -Wall -o long-name-load.exe long-name-load.c
//
// Usage:
//
//   long-name-load.exe <len> <method>
//
// It does one file operation on the NT path
// "\??\<cwd>\true #xxx...x", whose last component is <len> characters,
// and then calls LoadLibraryW on iphlpapi.dll (or the DLL named by the
// environment variable LNL_DLL).  The file need not exist.
//
// <method> is the file operation:
//
//   none        no file operation
//   ntopen      NtOpenFile
//   ntqa        NtQueryAttributesFile
//   createfile  CreateFileW with a \\?\ path
//
// Under Sandboxie, with <len> around 2200-3900, LoadLibraryW then
// fails with error 998 (ERROR_NOACCESS), and keeps failing on retry.
// That is the failure bash hits: when a native program starts a Cygwin
// program, Cygwin passes each double-quoted argument word to glob(),
// which stats it as a file name in the current directory.  Bash then
// loads iphlpapi.dll and fails.
//
// Exit codes: 0 if the DLL loaded, 1 if not, 2 for usage errors.

#include <windows.h>                   // Win32 API
#include <winternl.h>                  // UNICODE_STRING, OBJECT_ATTRIBUTES

#include <stdio.h>                     // printf, fprintf
#include <stdlib.h>                    // atoi
#include <string.h>                    // strcmp
#include <wchar.h>                     // swprintf, wcslen, _wgetenv


typedef NTSTATUS (NTAPI *NtOpenFile_t)(PHANDLE, ACCESS_MASK,
  POBJECT_ATTRIBUTES, PIO_STATUS_BLOCK, ULONG, ULONG);
typedef NTSTATUS (NTAPI *NtQueryAttributesFile_t)(POBJECT_ATTRIBUTES,
  PVOID);


int main(int argc, char **argv)
{
  if (argc != 3) {
    fprintf(stderr, "usage: %s <len> <method>\n", argv[0]);
    return 2;
  }
  int len = atoi(argv[1]);
  char const *method = argv[2];

  // Build "\??\<cwd>\true #xxx...x".
  static WCHAR path[40000];
  WCHAR cwd[1000];
  if (!GetCurrentDirectoryW(1000, cwd)) {
    fprintf(stderr, "GetCurrentDirectoryW failed\n");
    return 2;
  }
  int n = swprintf(path, 40000, L"\\??\\%ls\\true #", cwd);
  int nameStart = 4 + (int)wcslen(cwd) + 1;
  if (len < n - nameStart || nameStart + len > 32000) {
    fprintf(stderr, "invalid len: %d\n", len);
    return 2;
  }
  while (n - nameStart < len) {
    path[n++] = L'x';
  }
  path[n] = 0;

  UNICODE_STRING us;
  us.Buffer = path;
  us.Length = (USHORT)(n * sizeof(WCHAR));
  us.MaximumLength = us.Length + sizeof(WCHAR);
  OBJECT_ATTRIBUTES oa;
  InitializeObjectAttributes(&oa, &us, OBJ_CASE_INSENSITIVE, NULL,
                             NULL);

  HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
  long status = 0;
  DWORD err = 0;

  if (!strcmp(method, "none")) {
    // Nothing.
  }
  else if (!strcmp(method, "ntopen")) {
    NtOpenFile_t f = (NtOpenFile_t)GetProcAddress(ntdll, "NtOpenFile");
    HANDLE h;
    IO_STATUS_BLOCK io;
    status = f(&h, FILE_READ_ATTRIBUTES | SYNCHRONIZE, &oa, &io,
               FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
               FILE_OPEN_FOR_BACKUP_INTENT | FILE_OPEN_REPARSE_POINT);
  }
  else if (!strcmp(method, "ntqa")) {
    NtQueryAttributesFile_t f = (NtQueryAttributesFile_t)
      GetProcAddress(ntdll, "NtQueryAttributesFile");
    BYTE info[64];
    status = f(&oa, info);
  }
  else if (!strcmp(method, "createfile")) {
    path[1] = L'\\';                   // "\??\" -> "\\?\"
    HANDLE h = CreateFileW(path, FILE_READ_ATTRIBUTES,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
      OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    err = (h == INVALID_HANDLE_VALUE)? GetLastError() : 0;
  }
  else {
    fprintf(stderr, "unknown method: %s\n", method);
    return 2;
  }

  WCHAR const *dll = _wgetenv(L"LNL_DLL");
  if (!dll) {
    dll = L"C:\\WINDOWS\\system32\\iphlpapi.dll";
  }
  HMODULE h = LoadLibraryW(dll);
  DWORD loadErr = GetLastError();

  printf("long-name-load: len %d, %s: status 0x%lx, error %lu\n"
    "LoadLibraryW %s",
    len, method, (unsigned long)status, (unsigned long)err,
    h? "succeeded\n" : "failed, ");
  if (!h) {
    printf("error %lu\n", (unsigned long)loadErr);
    return 1;
  }
  return 0;
}


// EOF
