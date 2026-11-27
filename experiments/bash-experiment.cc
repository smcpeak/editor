// bash-experiment.cc
// Minimal program that makes bash crash loading iphlpapi.dll.

// See bash-experiment.txt for the original specification and
// bash-crash.txt for the background.
//
// Build (MinGW, so it is a native program):
//
//   g++ -Wall -o bash-experiment.exe bash-experiment.cc
//
// Usage:
//
//   bash-experiment.exe <bash> <size>
//
// It runs `<bash> -c "true #xxx...x"`, where the command line is
// padded with 'x' to be <size> characters long.
//
// Under Sandboxie, with the current directory inside an "Open for
// All" path, bash fails with:
//
//   bash.exe: *** fatal error - unable to load iphlpapi.dll, Win32
//   error 998
//
// for some sizes (for example, 2500-3500).  Everything that
// bash-experiment.cc used to do beyond this turned out not to matter:
// pipes, the CRT handle table in lpReserved2, the job object,
// STARTF_USESHOWWINDOW, CREATE_UNICODE_ENVIRONMENT, `-l`, Claude's
// wrapper text, and the heredoc.
//
// What does matter is the double quotes: the same length unquoted
// (`-c true#xxx...x`) never crashes.  Even `<bash> "xxx...x"`, with
// no `-c`, crashes.

#include <windows.h>                   // Win32 API

#include <stdio.h>                     // printf, fprintf
#include <stdlib.h>                    // strtol

#include <string>                      // std::string, std::wstring


// Exit codes.
enum {
  EXIT_BASH_OK = 0,
  EXIT_BASH_FAILED = 1,
  EXIT_ERROR = 2,
};


int main(int argc, char **argv)
{
  if (argc != 3) {
    fprintf(stderr, "usage: %s <bash> <size>\n", argv[0]);
    return EXIT_ERROR;
  }

  std::string cmdLine = std::string(argv[1]) + " -c \"true #";
  char *end;
  long size = strtol(argv[2], &end, 10);
  if (*argv[2] == '\0' || *end != '\0' ||
      size < (long)cmdLine.size() + 1 || size > 30000) {
    fprintf(stderr, "bash-experiment: invalid size: %s; "
      "the minimum is %d\n", argv[2], (int)cmdLine.size() + 1);
    return EXIT_ERROR;
  }
  cmdLine += std::string(size - cmdLine.size() - 1, 'x') + "\"";

  // Bash is ASCII here, so widening each char is enough.
  std::wstring wCmdLine(cmdLine.begin(), cmdLine.end());

  STARTUPINFOW si = {};
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi = {};

  // Inheriting handles lets bash write its error message to our
  // stdout and stderr when they are pipes rather than a console.
  if (!CreateProcessW(nullptr, &wCmdLine[0], nullptr, nullptr,
                      TRUE /*bInheritHandles*/, 0, nullptr, nullptr,
                      &si, &pi)) {
    fprintf(stderr, "bash-experiment: CreateProcessW failed: "
      "code %lu\n", (unsigned long)GetLastError());
    return EXIT_ERROR;
  }

  WaitForSingleObject(pi.hProcess, INFINITE);
  DWORD exitCode = 1;
  GetExitCodeProcess(pi.hProcess, &exitCode);
  printf("bash-experiment: size %ld, bash exit code %lu\n",
    size, (unsigned long)exitCode);

  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  return exitCode == 0? EXIT_BASH_OK : EXIT_BASH_FAILED;
}


// EOF
