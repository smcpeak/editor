// clipboard-experiment.cc
// Win32 program to manually drive the clipboard API, e.g., to hold it open.

// Build (Cygwin or MinGW):
//
//   g++ -Wall -o clipboard-experiment.exe clipboard-experiment.cc -luser32 -lgdi32 -lkernel32
//
// This is deliberately built as a console-subsystem program (no
// `-mwindows`) so that stdout is attached to the invoking terminal
// while the program also shows a GUI window.

/*
  Specification:

  This program opens a window that has these buttons, arranged
  vertically:

  * "OpenClipboard": Calls OpenClipboard with the window handle.

  * "EmptyClipboard": Calls EmptyClipboard.

  * "SetClipboardData" Calls SetClipboardData with CF_TEXT and the
    string "Clipboard experiment text data".

  * "CloseClipboard": Calls CloseClipboard.

  * "GetClipboardData": Calls GetClipboardData with CF_TEXT, and if that
    is successful, writes the retrieved string to stdout.  (Don't open
    the clipboard first; the user can press the OpenClipboard button
    if/when desired.)

  * "Quit": Quit the program.

  For all of these, it writes a line to stdout saying what function is
  being called and with what arguments.  Afterward, it writes a line
  with the return value.

  If any function fails, it calls GetLastError() and writes a line with
  the error code in hex and the human-readable error message to stdout.
*/

#include <windows.h>                   // Win32 API

#include <stdarg.h>                    // va_list, va_start, va_end
#include <stdio.h>                     // printf, vprintf, fflush
#include <string.h>                    // strlen, memcpy


// Control IDs for the buttons.
enum {
  ID_OPEN_CLIPBOARD = 101,
  ID_EMPTY_CLIPBOARD,
  ID_SET_CLIPBOARD_DATA,
  ID_CLOSE_CLIPBOARD,
  ID_GET_CLIPBOARD_DATA,
  ID_QUIT,
};


// Text placed on the clipboard by "SetClipboardData".
static char const clipboardText[] = "Clipboard experiment text data";


// Print a line to stdout and flush it so it appears immediately even
// when stdout is a pipe.
static void printLine(char const *format, ...)
{
  va_list args;
  va_start(args, format);
  vprintf(format, args);
  va_end(args);
  printf("\n");
  fflush(stdout);
}


// Call GetLastError and print the code and its message.
static void printLastError(char const *failedFunction)
{
  DWORD code = GetLastError();

  char *message = NULL;
  DWORD len = FormatMessageA(
    FORMAT_MESSAGE_ALLOCATE_BUFFER |
      FORMAT_MESSAGE_FROM_SYSTEM |
      FORMAT_MESSAGE_IGNORE_INSERTS,
    NULL,
    code,
    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
    (LPSTR)&message,
    0,
    NULL);

  // Remove the trailing CR/LF that FormatMessage appends.
  while (len > 0 && (message[len-1] == '\r' || message[len-1] == '\n')) {
    message[--len] = '\0';
  }

  printLine("  %s failed: GetLastError() = 0x%08lX: %s",
            failedFunction,
            (unsigned long)code,
            len > 0? message : "(no message available)");

  if (message) {
    LocalFree(message);
  }
}


static void doOpenClipboard(HWND hwnd)
{
  printLine("Calling OpenClipboard(hWndNewOwner=%p)", (void*)hwnd);
  SetLastError(0);
  BOOL ret = OpenClipboard(hwnd);
  printLine("  OpenClipboard returned %d", (int)ret);
  if (!ret) {
    printLastError("OpenClipboard");
  }
}


static void doEmptyClipboard()
{
  printLine("Calling EmptyClipboard()");
  SetLastError(0);
  BOOL ret = EmptyClipboard();
  printLine("  EmptyClipboard returned %d", (int)ret);
  if (!ret) {
    printLastError("EmptyClipboard");
  }
}


static void doSetClipboardData()
{
  // SetClipboardData requires a movable global memory block containing
  // the NUL-terminated text.
  SIZE_T size = strlen(clipboardText) + 1;

  printLine("Calling GlobalAlloc(uFlags=GMEM_MOVEABLE, dwBytes=%lu)",
            (unsigned long)size);
  SetLastError(0);
  HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, size);
  printLine("  GlobalAlloc returned %p", (void*)hMem);
  if (!hMem) {
    printLastError("GlobalAlloc");
    return;
  }

  printLine("Calling GlobalLock(hMem=%p)", (void*)hMem);
  SetLastError(0);
  void *p = GlobalLock(hMem);
  printLine("  GlobalLock returned %p", p);
  if (!p) {
    printLastError("GlobalLock");
    GlobalFree(hMem);
    return;
  }
  memcpy(p, clipboardText, size);
  GlobalUnlock(hMem);

  printLine("Calling SetClipboardData(uFormat=CF_TEXT, hMem=%p) "
            "with text \"%s\"",
            (void*)hMem, clipboardText);
  SetLastError(0);
  HANDLE ret = SetClipboardData(CF_TEXT, hMem);
  printLine("  SetClipboardData returned %p", (void*)ret);
  if (!ret) {
    printLastError("SetClipboardData");

    // On failure, we still own the memory.
    GlobalFree(hMem);
  }
  else {
    // On success, the system owns the memory.
  }
}


static void doCloseClipboard()
{
  printLine("Calling CloseClipboard()");
  SetLastError(0);
  BOOL ret = CloseClipboard();
  printLine("  CloseClipboard returned %d", (int)ret);
  if (!ret) {
    printLastError("CloseClipboard");
  }
}


static void doGetClipboardData()
{
  printLine("Calling GetClipboardData(uFormat=CF_TEXT)");
  SetLastError(0);
  HANDLE hData = GetClipboardData(CF_TEXT);
  printLine("  GetClipboardData returned %p", (void*)hData);
  if (!hData) {
    printLastError("GetClipboardData");
    return;
  }

  printLine("Calling GlobalLock(hMem=%p)", (void*)hData);
  SetLastError(0);
  char const *text = (char const *)GlobalLock(hData);
  printLine("  GlobalLock returned %p", (void const*)text);
  if (!text) {
    printLastError("GlobalLock");
    return;
  }

  printLine("  Clipboard text: \"%s\"", text);

  GlobalUnlock(hData);
}


static LRESULT CALLBACK windowProc(
  HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  switch (msg) {
    case WM_COMMAND:
      if (HIWORD(wParam) == BN_CLICKED) {
        switch (LOWORD(wParam)) {
          case ID_OPEN_CLIPBOARD:     doOpenClipboard(hwnd); return 0;
          case ID_EMPTY_CLIPBOARD:    doEmptyClipboard();    return 0;
          case ID_SET_CLIPBOARD_DATA: doSetClipboardData();  return 0;
          case ID_CLOSE_CLIPBOARD:    doCloseClipboard();    return 0;
          case ID_GET_CLIPBOARD_DATA: doGetClipboardData();  return 0;

          case ID_QUIT:
            printLine("Calling DestroyWindow(hWnd=%p)", (void*)hwnd);
            {
              SetLastError(0);
              BOOL ret = DestroyWindow(hwnd);
              printLine("  DestroyWindow returned %d", (int)ret);
              if (!ret) {
                printLastError("DestroyWindow");
              }
            }
            return 0;
        }
      }
      break;

    case WM_DESTROY:
      printLine("Calling PostQuitMessage(nExitCode=0)");
      PostQuitMessage(0);
      printLine("  PostQuitMessage returned (void)");
      return 0;
  }

  return DefWindowProcA(hwnd, msg, wParam, lParam);
}


int main()
{
  HINSTANCE hInstance = GetModuleHandleA(NULL);

  WNDCLASSA wc;
  memset(&wc, 0, sizeof(wc));
  wc.lpfnWndProc = windowProc;
  wc.hInstance = hInstance;
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
  wc.lpszClassName = "ClipboardExperimentWindow";
  if (!RegisterClassA(&wc)) {
    printLastError("RegisterClassA");
    return 2;
  }

  // Button layout.
  int const margin = 10;
  int const buttonWidth = 200;
  int const buttonHeight = 30;
  int const spacing = 8;

  struct ButtonSpec {
    int m_id;
    char const *m_label;
  };
  static ButtonSpec const buttons[] = {
    { ID_OPEN_CLIPBOARD,     "OpenClipboard" },
    { ID_EMPTY_CLIPBOARD,    "EmptyClipboard" },
    { ID_SET_CLIPBOARD_DATA, "SetClipboardData" },
    { ID_CLOSE_CLIPBOARD,    "CloseClipboard" },
    { ID_GET_CLIPBOARD_DATA, "GetClipboardData" },
    { ID_QUIT,               "Quit" },
  };
  int const numButtons = sizeof(buttons) / sizeof(buttons[0]);

  // Compute the outer window size from the desired client area.
  RECT rect = { 0, 0,
    margin*2 + buttonWidth,
    margin*2 + numButtons*buttonHeight + (numButtons-1)*spacing };
  DWORD const style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
  AdjustWindowRect(&rect, style, FALSE);

  HWND hwnd = CreateWindowA(
    wc.lpszClassName,
    "Clipboard Experiment",
    style,
    CW_USEDEFAULT, CW_USEDEFAULT,
    rect.right - rect.left, rect.bottom - rect.top,
    NULL, NULL, hInstance, NULL);
  if (!hwnd) {
    printLastError("CreateWindowA");
    return 2;
  }
  printLine("Main window hwnd=%p", (void*)hwnd);

  for (int i = 0; i < numButtons; i++) {
    HWND b = CreateWindowA(
      "BUTTON",
      buttons[i].m_label,
      WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
      margin, margin + i*(buttonHeight + spacing),
      buttonWidth, buttonHeight,
      hwnd,
      (HMENU)(INT_PTR)buttons[i].m_id,
      hInstance,
      NULL);
    if (!b) {
      printLastError("CreateWindowA(BUTTON)");
      return 2;
    }

    // Use the normal GUI font rather than the old bold system font.
    SendMessageA(b, WM_SETFONT,
                 (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
  }

  ShowWindow(hwnd, SW_SHOWNORMAL);
  UpdateWindow(hwnd);

  MSG msg;
  while (GetMessageA(&msg, NULL, 0, 0) > 0) {
    if (!IsDialogMessageA(hwnd, &msg)) {
      TranslateMessage(&msg);
      DispatchMessageA(&msg);
    }
  }

  return (int)msg.wParam;
}


// EOF
