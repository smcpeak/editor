// editor.cc
// tiny editor using Xlib only

#include <X11/Xlib.h>        // X library
#include <X11/Xutil.h>       // XComposeStatus

#define XK_MISCELLANY        // why do I have to jump this hoop?
#include <X11/keysymdef.h>   // XK_xxx constants

#include <stdio.h>           // printf
#include <assert.h>          // assert
#include <unistd.h>          // sleep
#include <ctype.h>           // isprint
#include <string.h>          // memmove


// buffer of text being edited
class Buffer {
public:
  char text[80];
  int cursor;

public:
  Buffer() {
    strcpy(text, "initial text");
    cursor = 5;
  }

  int length() const { return strlen(text); }

  void insert(char c);
  void moveCursor(int delta);
  void backspace();
};


void Buffer::insert(char c)
{
  if (length() < 79) {
    // shift everything to the right of the cursor over by 1
    memmove(text+cursor+1, text+cursor, length()-cursor);

    // put the new char at the cursor pos
    text[cursor] = c;

    // move the cursor over 1
    cursor++;
  }
  else {
    printf("buffer is full\n");
  }
}


void Buffer::moveCursor(int delta)
{
  cursor += delta;
  if (cursor < 0) { cursor = 0; }
  if (cursor > length()) { cursor = length(); }
}


void Buffer::backspace()
{
  if (cursor > 0) {
    memmove(text+cursor-1, text+cursor, length()-cursor+1);  // inc null term
    cursor--;
  }
}


// right now, one buffer
Buffer theBuffer;


// info to name a window
class WindowName {
public:
  Display *display;      // which X server we're talking to
  int screen;            // ?
  Window window;         // which window id on that server
  
public:
  WindowName(Display *d, int s)
    : display(d), screen(s) {}  
};


void drawBuffer(WindowName &win, Buffer &buffer)
{
  // create a graphics context (GC)
  GC gc;             // a pointer to an _XGC struct
  {
    // need a struct..
    XGCValues values;
    unsigned long valuesMask = 0;    // nothing is specified in 'values'

    gc = XCreateGC(
      win.display,             // display
      win.window,              // thing to draw upon (window or pixmap id)
      valuesMask, &values      // extra stuff
    );
  }

  // set some colors in the gc
  XSetForeground(win.display, gc, BlackPixel(win.display, win.screen));
  XSetBackground(win.display, gc, WhitePixel(win.display, win.screen));

  // erase left-side cursor if necessary
  XDrawImageString(
    win.display, win.window, gc,       // dest
    3,15, //10,35,                     // lower,base coordinate
    " ", 1                             // text, length
  );

  // and some text *with* erasing background
  XDrawImageString(
    win.display, win.window, gc,       // dest
    5,15, //10,35,                     // lower,base coordinate
    buffer.text, buffer.length()       // text, length
  );


  // try to print a cursor
  XFontStruct *fs = XQueryFont(win.display, XGContextFromGC(gc));
  if (!fs) {
    printf("font does not exist (?!)\n");
  }

  // compute width of text before cursor
  int width = XTextWidth(fs, buffer.text, buffer.cursor);

  // draw the cursor as a line
  XDrawLine(win.display, win.window, gc,
            5+width-1, 16, 5+width-1, 4);

  // calc width of whole string
  width = XTextWidth(fs, buffer.text, buffer.length());

  // overwrite after last char in case of backspace
  XDrawImageString(
    win.display, win.window, gc,       // dest
    5+width,15,                        // lower,base coordinate
    " ", 1                             // text, length
  );

  // free the 'fs' .. the 'names' stuff is confusing..
  XFreeFontInfo(NULL, fs, 0);


  // cue to quit app
  XDrawString(
    win.display, win.window, gc,       // dest
    5,295,                     // left edge, baseline coordinate
    "Click here to quit", 18   // text, length
  );

  // network traffic
  XFlush(win.display);

  XFreeGC(win.display, gc);
}


int main()
{
  // open a network connection to host specified in DISPLAY
  // environment variable
  // the Display structure is defined around line 546 in Xlib.h
  Display *display = XOpenDisplay(NULL);
  printf("display: %p\n", display);
  assert(display);

  // figure out which screen we are (already?) connected to
  int screen = DefaultScreen(display);
  printf("screen: %d\n", screen);

  // print some info about the screen
  int screenWidth = DisplayWidth(display, screen);
  int screenHeight = DisplayHeight(display, screen);
  int screenDepth = DefaultDepth(display, screen);
  printf("width: %d   height: %d   depth: %d\n",
         screenWidth, screenHeight, screenDepth);

  // create a window
  WindowName win(display, screen);
  {
    // some of the parameters go in this structure
    XSetWindowAttributes attr;
    attr.background_pixel = WhitePixel(display, screen);     // what about xrdb?
    attr.border_pixel = BlackPixel(display, screen);
    attr.event_mask =                                        // events to notice
      ExposureMask |                                           // window-revealed
      //PointerMotionMask |                                      // mouse movement
      ButtonPressMask |                                        // mouse button press
      ButtonReleaseMask |                                      // mouse button release
      KeyPressMask |                                           // keyboard key press
      KeyReleaseMask |                                         // keyboard key release
      0;
    attr.override_redirect = False;                          // don't override window mgr

    // then we have to say which ones we set
    unsigned long attrMask =
      CWBackPixel | CWBorderPixel | CWEventMask | CWOverrideRedirect;

    // then create the window itself
    win.window = XCreateWindow(
      display,                         // display on which to create the window (which screen?)
      RootWindow(display, screen),     // parent window
      0,0,                             // x,y of upper left (usually ignored by WM?)
      300,300,                         // (interior?) width,height (usually respected by WM?)
      10,                              // border width
      screenDepth,                     // depth (?)
      InputOutput,                     // interaction class
      CopyFromParent,                  // visual class (?)
      attrMask, &attr                  // structure params above
    );
    printf("window: %ld\n", win.window);
  }

  // give some display hints to the WM
  #if 0     // this fn doesn't exist, and it's not clear what the right one is
  {
    XSetAttributes attr;
    attr.x = 50;
    attr.y = 50;
    attr.width = 500;
    attr.height = 500;

    // and say which we set
    attr.flags = USPosition | USSize;

    // send the hints
    XSetNormalSize(display, win.window, &attr);
  }
  #endif // 0

  // side note: X fns call a user-supplied error handler (or a default)
  // when they fail; one generally need not check the return values
  // (questionable design decision, but tolerable in the current context)

  // set the window's caption
  XStoreName(display, win.window, "Hello, world!");

  // display the window
  XMapWindow(display, win.window);      // make it visible
  //XMapRaised(display, window);      // make it visible, and on top

  // cause network traffic so window appears
  XFlush(display);

  // draw some stuff (this is actually unnecessary, since the
  // window system sends an 'expose' initially)
  drawBuffer(win, theBuffer);

  // event loop
  int quit = False;
  while (!quit) {
    XEvent event;
    XNextEvent(display, &event);
    switch (event.type) {
      case Expose:
        printf("expose\n");
        drawBuffer(win, theBuffer);
        break;

      case MotionNotify:
        printf("pointer motion at %d, %d\n",
               event.xmotion.x, event.xmotion.y);
        break;

      case ButtonPress:
        printf("button press at %d, %d\n",
               event.xbutton.x, event.xbutton.y);
        if (event.xbutton.x < 100 && event.xbutton.y > 285) {
          // click in lower-left; quit
          quit = True;
        }
        break;

      case ButtonRelease:
        printf("button release at %d, %d\n",
               event.xbutton.x, event.xbutton.y);
        break;

      case KeyPress:
      case KeyRelease: {
        // this is a fairly limited way of getting key information.. it
        // doesn't return anything useful for keys other than the ascii
        // keys..
       	char buf[20];			  // holds string generated by key
        XComposeStatus composeStatus;     // ?
        KeySym keysym;
        int length = XLookupString(
          &event.xkey,                    // XKeyEvent
          buf, 18,                        // buffer to store string
          &keysym, &composeStatus	  // ?
        );
        buf[length] = 0;                  // terminating null

        printf("Key %s, length=%d, text=",
               event.type==KeyPress? "pressed" : "released",
               length);
        for (int i=0; i<length; i++) {
          if (isprint(buf[i])) {
            printf("%c", buf[i]);
          }
          else {
            printf("\\x%02X", buf[i]);
          }
        }

        // here's a more low-level way of interrogating keystrokes
        printf(" keysym=%ld", keysym);
        if (keysym == XK_Shift_L) {
          printf(" (it's left-shift)");
        }

        if (event.type == KeyPress) {
          if (length==1 && isprint(buf[0])) {
            // insert this character at the cursor
            theBuffer.insert(buf[0]);
          }

          switch (keysym) {
            case XK_Left:   theBuffer.moveCursor(-1);  break;
            case XK_Right:  theBuffer.moveCursor(+1);  break;
            case XK_BackSpace:  theBuffer.backspace(); break;
          }

          // redraw
          drawBuffer(win, theBuffer);
        }

        printf("\n");
        break;
      }

      default:
        printf("unknown event!\n");
        break;
    }
  }

  // clean up
  XDestroyWindow(display, win.window);
  XCloseDisplay(display);

  printf("returning from main\n");
  return 0;
}
