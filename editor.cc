// editor.cc
// tiny editor using Xlib only

#if 0
#include <X11/Xlib.h>        // X library
#include <X11/Xutil.h>       // XComposeStatus
#include <X11/Xatom.h>       // XA_FONT

#define XK_MISCELLANY        // why do I have to jump this hoop?
#include <X11/keysymdef.h>   // XK_xxx constants
#endif // 0

#include "editor.h"          // this module

#include <qapplication.h>    // QApplication
#include <qpushbutton.h>     // QPushButton

#include <stdio.h>           // printf
#include <assert.h>          // assert
#include <unistd.h>          // sleep
#include <ctype.h>           // isprint
#include <string.h>          // memmove

#include "buffer.h"          // Buffer
#include "position.h"        // Position
#include "textline.h"        // TextLine


// right now, one buffer and one cursor
Buffer buffer;
Position cursor(&buffer);


#if 0
// the font I want to use
char const *myFontName = "-*-editor-medium-r-*-*-14-*-*-*-*-*-*-*";
Atom myFontAtom;
Font myFontId;
XFontStruct *myFontStruct;
#endif // 0


#if 0
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
#endif // 0


void Editor::paintEvent(QPaintEvent *ev)
{
  #if 0
  static char const *lotsOfSpaces = "                                                                      ";

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

  // use my font!
  XSetFont(win.display, gc, myFontId);

  XFontStruct *fs = myFontStruct;
  int fontHeight = fs->ascent + fs->descent;
  int fontWidth = fs->max_bounds.width;        // ok?

  int lastBotDescent = 0;

  int leftMargin = 5;
  int topMargin = 5;

  for (int line=0; line < buffer.totLines(); line++) {
    int baseLine = topMargin + (line+1)*fontHeight;
    //int topAscent = baseLine - fs->ascent;
    int botDescent = baseLine + fs->descent;
    lastBotDescent = botDescent;

    // erase left margin
    XDrawImageString(
      win.display, win.window, gc,       // dest
      0, baseLine,                       // lower,base coordinate
      " ", 1                             // text, length
    );

    // draw line's text
    TextLine const *tl = buffer.getLineC(line);
    XDrawImageString(
      win.display, win.window, gc,       // dest
      leftMargin, baseLine,              // lower,base coordinate
      tl->getText(), tl->getLength()     // text, length
    );

    // calc width of whole string
    int width = XTextWidth(fs, tl->getText(), tl->getLength());

    // write spaces to right edge of the window
    XDrawImageString(
      win.display, win.window, gc,       // dest
      leftMargin+width, baseLine,        // lower,base coordinate
      lotsOfSpaces, strlen(lotsOfSpaces) // text, length
    );

    #if 0
    if (cursor.line() == line) {
      // try to print a cursor

      // compute width of text before cursor
      width = XTextWidth(fs, tl->getText(), cursor.col());

      // draw the cursor as a line
      XDrawLine(win.display, win.window, gc,
                leftMargin+width-1, botDescent,     // bottom of descent
                leftMargin+width-1, topAscent);     // top of ascent
    }
    #endif // 0
  }

  // fill the remainder with white
  XClearArea(win.display, win.window,
             0,lastBotDescent,       // x1,y1
             500,500,                // width,height
             False);                 // gen exposures?

  // draw the cursor as a line
  {
    int line = cursor.line();
    int baseLine = topMargin + (line+1)*fontHeight;
    int topAscent = baseLine - fs->ascent;
    int botDescent = baseLine + fs->descent;
    int width = fontWidth * cursor.col();
    XDrawLine(win.display, win.window, gc,
              leftMargin+width-1, botDescent,     // bottom of descent
              leftMargin+width-1, topAscent);     // top of ascent
  }            

  // cue to quit app
  XDrawString(
    win.display, win.window, gc,       // dest
    5,295,                     // left edge, baseline coordinate
    "Click here to quit", 18   // text, length
  );

  // network traffic
  XFlush(win.display);

  XFreeGC(win.display, gc);
  #endif // 0
}


int main(int argc, char **argv)
{
  QApplication a(argc, argv);

  Editor editor(&buffer, NULL /*parent*/, "An Editor");
  editor.resize(300,300);

  //QPushButton hello("Hello world!", NULL /*parent*/);
  //hello.resize(100, 30);

  a.setMainWidget(&editor);
  editor.show();
  return a.exec();

  #if 0
  // make an atom for the font name I want to use
  myFontAtom = XInternAtom(display, myFontName, False /*must_exist*/);
  printf("myFontAtom: %ld\n", myFontAtom);

  // make an id for it (doesn't actually need the Atom ..)
  myFontId = XLoadFont(display, myFontName);
  printf("myFontId: %ld\n", myFontId);

  // query its info
  myFontStruct = XQueryFont(display, myFontId);
  XFontStruct *fs = myFontStruct;
  if (!myFontStruct) {
    printf("font does not exist (?!)\n");
  }

  // try to get font's name
  {
    Atom nameAtom;
    if (!XGetFontProperty(fs, XA_FONT, &nameAtom)) {
      printf("XA_FONT isn't defined?\n");
    }
    else {
      printf("XA_FONT: %ld\n", nameAtom);

      // how to handle errors?
      char *name = XGetAtomName(display, nameAtom);
      printf("font name: %s\n", name);
      XFree(name);
    }
  }    
  #endif // 0

  #if 0
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
      StructureNotifyMask |                                    // window destruction?
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
  #endif // 0

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

  #if 0
  // set the window's caption
  XStoreName(display, win.window, "El Editor Suprémo");
    // this character is decimal 233,            ^
    // an accented 'e'

  // display the window
  XMapWindow(display, win.window);      // make it visible
  //XMapRaised(display, window);      // make it visible, and on top

  // cause network traffic so window appears
  XFlush(display);

  // draw some stuff (this is actually unnecessary, since the
  // window system sends an 'expose' initially)
  drawBuffer(win, buffer);

  // event loop
  int quit = False;
  while (!quit) {
    XEvent event;
    XNextEvent(display, &event);
    switch (event.type) {
      case Expose:
        printf("expose\n");
        drawBuffer(win, buffer);
        break;

      #define WATCH_FOR(type)              \
      case type:                           \
        printf("saw %s event\n", #type);   \
        break;                             
      WATCH_FOR(CirculateNotify)
      WATCH_FOR(ConfigureNotify)
      WATCH_FOR(CreateNotify)
      WATCH_FOR(DestroyNotify)
      WATCH_FOR(GravityNotify)
      WATCH_FOR(MapNotify)
      WATCH_FOR(MappingNotify)
      WATCH_FOR(ReparentNotify)
      WATCH_FOR(UnmapNotify)
      WATCH_FOR(VisibilityNotify)
      #undef WATCH_FOR

      case MotionNotify:
        printf("pointer motion at %d, %d\n",
               event.xmotion.x, event.xmotion.y);
        break;

      case ButtonPress:
        printf("button press at %d, %d\n",
               event.xbutton.x, event.xbutton.y);
        if (event.xbutton.x < 165 && event.xbutton.y > 285) {
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

        if (event.type == KeyPress) {
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
          printf(" keysym=%ld (0x%lX)", keysym, keysym);
          if (keysym == XK_Shift_L) {
            printf(" (it's left-shift)");
          }

          printf("\n");

          if (length==1 && buf[0] == 'u'-'a'+1) {  // ctrl-u
            buffer.dumpRepresentation();
          }

          if (length==1 && buf[0] == 'c'-'a'+1) {  // ctrl-c
            quit = true;
          }

          if (length==1 && isprint(buf[0])) {
            // insert this character at the cursor
            buffer.insertText(&cursor, buf, 1);
          }

          switch (keysym) {
            case XK_Left:       cursor.move(0, -1);   break;
            case XK_Right:      cursor.move(0, +1);   break;
            case XK_Home:       cursor.setCol(0);     break;
            case XK_End:
              cursor.setCol(
                buffer.getLineC(cursor.line())->getLength());
              break;
            case XK_Up:         cursor.move(-1,0);    break;
            case XK_Down:       cursor.move(+1,0);    break;  // allows cursor past EOF..

            case XK_BackSpace: {
              Position oneLeft(cursor);
              oneLeft.move(0,-1);
              buffer.deleteText(&oneLeft, &cursor);
              break;
            }
            
            case XK_Return: {
              buffer.insertText(&cursor, "\n", 1);
              break;
            }
          }

          // redraw
          drawBuffer(win, buffer);
        }
        break;
      }

      default:
        printf("unknown event!\n");
        break;
    }
  }
  #endif // 0

  // clean up

  #if 0
  // free the 'fs' .. the 'names' stuff is confusing..
  // (does *not* use XFreeFont, b/c of using the GC id above..)
  //XFreeFontInfo(NULL, fs, 0);
  XFreeFont(display, myFontStruct);    // now?

  XDestroyWindow(display, win.window);
  XCloseDisplay(display);
  #endif // 0

  printf("returning from main\n");
  return 0;
}
