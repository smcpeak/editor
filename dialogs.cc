// dialogs.cc
// code for dialogs.h, built on Motif

// based on information from:
//   http://csc.lsu.edu/tutorial/Xnotes/subsubsection3_12_6_1.html
//   http://www.landfield.com/faqs/motif-faq/part6/section-47.html

#include <Xm/Xm.h>             // Motif
#include <Xm/MainW.h>
#include <Xm/CascadeB.h>
#include <Xm/MessageB.h>
#include <Xm/PushB.h>
#include <Xm/SelectioB.h>

#include <Xm/RowColumn.h>      // XmCreateMenuBar
#include <stdio.h>             // printf

#include "typ.h"               // bool

bool doMyPopup = false;
Widget myPopupParent, popupDialog;

// prototypes for my stuff
bool stringPromptDialog(Widget parent, char *prompt, char *defaultValue,
                        char *destString, int destLength);
void promptCallback(Widget wt, XtPointer client_data,
                    XmSelectionBoxCallbackStruct *selection);
void messageBox(Widget parent, char *text);

void my_prompt_pop_up(Widget parent, char *text,
                      XmPushButtonCallbackStruct *cbs);
void do_my_prompt_pop_up(Widget parent);
void printStateInfo(char const *context);


// prototypes for existing stuff
void ScrubDial(Widget, int);
void
prompt_pop_up(Widget cascade_button, char *text,
XmPushButtonCallbackStruct *cbs);
void
info_pop_up(Widget cascade_button, char *text,
XmPushButtonCallbackStruct *cbs);
void quit_pop_up(Widget cascade_button, char *text,
XmPushButtonCallbackStruct *cbs);
void prompt_activate(Widget widget, XtPointer client_data,
XmSelectionBoxCallbackStruct *selection);
void
quit_activate(Widget dialog, void*, void*);


Widget top_wid;
XtAppContext app;

int main(int argc, char *argv[])
{
    Widget main_w, menu_bar, info, prompt, quit;


    top_wid = XtVaAppInitialize(&app, "Demos", NULL, 0,
        &argc, argv, NULL, NULL);

    main_w = XtVaCreateManagedWidget("main_window",
        xmMainWindowWidgetClass,   top_wid,
        XmNheight, 300,
        XmNwidth,300,
        NULL);


    menu_bar = XmCreateMenuBar(main_w, "main_list",
        NULL, 0);
    XtManageChild(menu_bar);

    /* create prompt widget + callback */
    prompt = XtVaCreateManagedWidget( "Prompt",
        xmCascadeButtonWidgetClass, menu_bar,
        XmNmnemonic, 'P',
        NULL);

    /* Callback has data passed to */
    XtAddCallback(prompt, XmNactivateCallback,
                  prompt_pop_up, NULL);



    // ----- mine ------
    /* create prompt widget + callback */
    prompt = XtVaCreateManagedWidget( "MyPrompt",
        xmCascadeButtonWidgetClass, menu_bar,
        XmNmnemonic, 'M',
        NULL);

    /* Callback has data passed to */
    XtAddCallback(prompt, XmNactivateCallback,
                  my_prompt_pop_up, NULL);
    // ----- end mine ------


    /* create quit widget + callback */
    quit = XtVaCreateManagedWidget( "Quit",
        xmCascadeButtonWidgetClass, menu_bar,
        XmNmnemonic, 'Q',
        NULL);


    /* Callback has data passed to */
    XtAddCallback(quit, XmNactivateCallback, quit_pop_up,
                  (void*)"Are you sure you want to quit?");



    /* create help widget + callback */
    info = XtVaCreateManagedWidget( "Info",
        xmCascadeButtonWidgetClass, menu_bar,
        XmNmnemonic, 'I',
        NULL);

    XtAddCallback(info, XmNactivateCallback, info_pop_up,
                  (void*)"Select Prompt Option To Get Program Going.");

    XtRealizeWidget(top_wid);

    {
      XEvent event;
      while (1) {
        if (doMyPopup) {
          do_my_prompt_pop_up(myPopupParent);
          doMyPopup = false;
        }
        else {
          XtAppNextEvent(app, &event);
          if (!XtDispatchEvent(&event)) {
            // probably irrelevant
            //printf("warning: unhandled event\n");
          }
        }
      }
    }

    return 0;
}


/* routine to remove a DialButton from a Dialog */

void ScrubDial(Widget wid, int dial)

{  Widget remove;

   remove = XmMessageBoxGetChild(wid, dial);

   XtUnmanageChild(remove);
}


// ------------------ my stuff -------------------------
// called in response to a menu selection
void
my_prompt_pop_up(Widget parent, char *text,
                 XmPushButtonCallbackStruct *cbs)
{
  doMyPopup = true;
  myPopupParent = parent;
}


void
do_my_prompt_pop_up(Widget parent)
{
  char result[80];
  strcpy(result, "(no value was ever put into 'result')");

  printf("my_prompt_pop_up: start\n");

  if (stringPromptDialog(parent, "Enter some text:", "(default value)",
                         result, 80)) {
    printf("result: ok\n");
  }
  else {
    printf("result: cancel\n");
  }

  printf("my_prompt_pop_up: middle\n");

  messageBox(parent, result);

  printf("my_prompt_pop_up: end\n");
}


// carries info through a callback
struct PromptInfo {
  char *dest;    // user's string
  int len;       // user-supplied length
  bool ok;       // true if user pressed Ok
};


// a general-purpose string prompter
bool stringPromptDialog(Widget parent, char *prompt, char *defaultValue,
                        char *destString, int destLength)
{
  Widget dialog, remove;
  XmString xm_string1, xm_string2, xm_string3;
  void prompt_activate();
  Arg args[4];
  struct PromptInfo info;

  // fill in the 'info' struct for the callback
  info.dest = destString;
  info.len = destLength;
  info.ok = false;

  /* label the dialog */
  xm_string1 = XmStringCreateSimple(prompt);
  XtSetArg(args[0], XmNselectionLabelString, xm_string1);

  /* default text string  */
  xm_string2 = XmStringCreateSimple(defaultValue);
  XtSetArg(args[1], XmNtextString, xm_string2);

  /* set up default button for cancel callback */
  XtSetArg(args[2], XmNdefaultButtonType,
                    XmDIALOG_CANCEL_BUTTON);

  // specify the dialog caption
  xm_string3 = XmStringCreateSimple("my caption");
  XtSetArg(args[3], XmNdialogTitle, xm_string3);

  /* Create the dialog */
  dialog = XmCreatePromptDialog(parent, "what_is_this",
                                args, 4);
  printf("dialog is %p; parent is %p\n",
         dialog, parent);

  XmStringFree(xm_string1);
  XmStringFree(xm_string2);
  XmStringFree(xm_string3);

  XtAddCallback(dialog, XmNokCallback,
                promptCallback, &info);
  //XtAddCallback(dialog, XmNcancelCallback,
  //              (void (*)())promptCallback, &info);
  //XtAddCallback(dialog, XmNpopdownCallback,
  //              (void (*)())promptCallback, &info);
  //XtAddCallback(XtParent(dialog), XmNpopdownCallback,
  //              (void (*)())promptCallback, &info);

  /* Scrub Prompt Help Button */
  remove = XmSelectionBoxGetChild(dialog, XmDIALOG_HELP_BUTTON);
  XtUnmanageChild(remove);  /* scrub HELP button */

  XtManageChild(dialog);
  XtPopup(XtParent(dialog), XtGrabNone);

  popupDialog = dialog;
  printStateInfo("before");

  // modal loop!
  {
    XEvent event;
    while (XtIsManaged(dialog) /*|| XtAppPending(app)*/) {
      XtAppNextEvent(app, &event);
      if (!XtDispatchEvent(&event)) {
        // probably irrelevant
        //printf("warning: unhandled event\n");
      }
    }
  }

  printStateInfo("after");

  return info.ok;
}


// callback for responding when the user closes the
// prompt dialog, above
void promptCallback(Widget wt, XtPointer client_data,
                    XmSelectionBoxCallbackStruct *selection)
{
  struct PromptInfo *info = (struct PromptInfo*)client_data;
  char *userString;

  printf("promptCallback: wt=%p, sel=%p, len=%d, str=%s\n",
         wt, selection, info->len, info->dest);

  if (selection != NULL) {
    printf("promptCallback: reason=%d (XmCR_OK=%d, XmCR_CANCEL=%d)\n",
           selection->reason, XmCR_OK, XmCR_CANCEL);

    if (selection->reason == XmCR_OK) {
      // grab the user's entry; Motif allocates a string for me,
      // how nice of them
      XmStringGetLtoR(selection->value, XmSTRING_DEFAULT_CHARSET,
                      &userString);

      // get that into my buffer
      strncpy(info->dest, userString, info->len);
      info->dest[info->len-1] = 0;    // force null termination

      // release the thing it allocated for me
      XFree(userString);

      // tell our caller that 'ok' was pressed
      info->ok = true;
    }
  }
}


// print state info about my popup dialog
void printStateInfo(char const *context)
{
  printf("context: XtIsObject=%d XtIsManaged=%d\n",
         XtIsObject(popupDialog),
         XtIsManaged(popupDialog));
}


// general-purpose message box
void messageBox(Widget parent, char *text)
{
  printf("messageBox: %s\n", text);

  #if 0
  Widget dialog;
  Arg args[2];
  XmString xm_string;

  /* compose InformationDialog output string */
  xm_string = XmStringCreateSimple(text);
  XtSetArg(args[0], XmNmessageString, xm_string);

  /* set up default button for OK callback */
  XtSetArg(args[1], XmNdefaultButtonType, XmDIALOG_OK_BUTTON);

  /* Create the InformationDialog */
  dialog = XmCreateInformationDialog(parent, "prompt_message",
                                     args, 2);

  // remove some buttons we don't want
  ScrubDial(dialog, XmDIALOG_CANCEL_BUTTON);
  ScrubDial(dialog, XmDIALOG_HELP_BUTTON);

  XtManageChild(dialog);
  XtPopup(XtParent(dialog), XtGrabNone);
  #endif // 0
}


// --------------------- his junk ----------------
void
prompt_pop_up(Widget cascade_button, char *text,
XmPushButtonCallbackStruct *cbs)

{   Widget dialog, remove;
    XmString xm_string1, xm_string2;
    Arg args[3];

    /* label the dialog */
    xm_string1 = XmStringCreateSimple("Enter Text Here:");
    XtSetArg(args[0], XmNselectionLabelString, xm_string1);
    /* default text string  */
    xm_string2 = XmStringCreateSimple("Default String");


    XtSetArg(args[1], XmNtextString, xm_string2);
   /* set up default button for cancel callback */
    XtSetArg(args[2], XmNdefaultButtonType,
                    XmDIALOG_CANCEL_BUTTON);

    /* Create the WarningDialog */
    dialog = XmCreatePromptDialog(cascade_button, "prompt",
                                  args, 3);

    XmStringFree(xm_string1);
    XmStringFree(xm_string2);


    XtAddCallback(dialog, XmNokCallback, prompt_activate,
                  NULL);
   /* Scrub Prompt Help Button */
    remove = XmSelectionBoxGetChild(dialog,
                      XmDIALOG_HELP_BUTTON);

    XtUnmanageChild(remove);  /* scrub HELP button */

    XtManageChild(dialog);
    XtPopup(XtParent(dialog), XtGrabNone);
}


void
info_pop_up(Widget cascade_button, char *text,
XmPushButtonCallbackStruct *cbs)

{   Widget dialog;
    XmString xm_string;
    Arg args[2];

    printStateInfo("info");

    /* label the dialog */
    xm_string = XmStringCreateSimple(text);
    XtSetArg(args[0], XmNmessageString, xm_string);
    /* set up default button for OK callback */
    XtSetArg(args[1], XmNdefaultButtonType,
             XmDIALOG_OK_BUTTON);


    /* Create the InformationDialog as child of
       cascade_button passed in */
    dialog = XmCreateInformationDialog(cascade_button,
                                       "info", args, 2);

    ScrubDial(dialog, XmDIALOG_CANCEL_BUTTON);
    ScrubDial(dialog, XmDIALOG_HELP_BUTTON);

    XmStringFree(xm_string);

    XtManageChild(dialog);
    XtPopup(XtParent(dialog), XtGrabNone);
}


void quit_pop_up(Widget cascade_button, char *text,
XmPushButtonCallbackStruct *cbs)
{   Widget dialog;
    XmString xm_string;
    Arg args[1];

    /* label the dialog */
    xm_string = XmStringCreateSimple(text);
    XtSetArg(args[0], XmNmessageString, xm_string);
    /* set up default button for cancel callback */
    XtSetArg(args[1], XmNdefaultButtonType,
                   XmDIALOG_CANCEL_BUTTON);


    /* Create the WarningDialog */
    dialog = XmCreateWarningDialog(cascade_button, "quit",
                                   args, 1);

    ScrubDial(dialog, XmDIALOG_HELP_BUTTON);

    XmStringFree(xm_string);

    XtAddCallback(dialog, XmNokCallback, quit_activate,
                  NULL);

    XtManageChild(dialog);
    XtPopup(XtParent(dialog), XtGrabNone);
}


/* callback function for Prompt activate */

void prompt_activate(Widget widget, XtPointer client_data,
XmSelectionBoxCallbackStruct *selection)

{   Widget dialog;
    Arg args[2];
    XmString xm_string;

    /* compose InformationDialog output string */
    /* selection->value holds XmString entered to prompt */

    xm_string = XmStringCreateSimple("You typed: ");
    xm_string = XmStringConcat(xm_string,selection->value);


    XtSetArg(args[0], XmNmessageString, xm_string);
    /* set up default button for OK callback */
    XtSetArg(args[1], XmNdefaultButtonType,
             XmDIALOG_OK_BUTTON);

    /* Create the InformationDialog to echo
       string grabbed from prompt */
    dialog = XmCreateInformationDialog(top_wid,
           "prompt_message", args, 2);

    ScrubDial(dialog, XmDIALOG_CANCEL_BUTTON);
    ScrubDial(dialog, XmDIALOG_HELP_BUTTON);

    XtManageChild(dialog);
    XtPopup(XtParent(dialog), XtGrabNone);
}



/* callback routines for quit ok dialog */


void
quit_activate(Widget dialog, void*, void*)
{
    printf("Quit Ok was pressed.\n");
    exit(0);
}
