/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Peter Annema <disttsc@bart.nl>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsWidget.h"
#include "keysym2ucs.h"
#include "nsWindow.h"
#include "nsAppShell.h"

#include "nsScrollbar.h"
#include "nsGUIEvent.h"

#include "nsTextWidget.h"
#include "nsGtkIMEHelper.h"


#include <stdio.h>

#include <gtk/gtk.h>
#include <gtk/gtkprivate.h>
#include "nsGtkEventHandler.h"

#include <gdk/gdkkeysyms.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#ifdef DEBUG_pavlov
//#define DEBUG_EVENTS 1
#endif

//#define DEBUG_MOVE

static void
dispatch_superwin_event(GdkEvent *event, nsWindow *window);

static PRBool
gdk_window_child_of_gdk_window(GdkWindow *window, GdkWindow *ancestor);

// This is a flag that says if we should suppress the next key down
// event.  This is used when that last key release event was
// suppressed and we want the next key press event to not generate the
// key down event since it is part of a key repeat sequence.
static PRBool suppressNextKeyDown = PR_FALSE;

//==============================================================
void InitAllocationEvent(GtkAllocation *aAlloc,
                         gpointer   p,
                         nsSizeEvent &anEvent,
                         PRUint32   aEventType)
{
  anEvent.message = aEventType;
  anEvent.widget  = (nsWidget *) p;

  anEvent.eventStructType = NS_SIZE_EVENT;

  if (aAlloc != nsnull) {
    // HACK
    //    nsRect *foo = new nsRect(aAlloc->x, aAlloc->y, aAlloc->width, aAlloc->height);
    nsRect *foo = new nsRect(0, 0, aAlloc->width, aAlloc->height);
    anEvent.windowSize = foo;
    //    anEvent.point.x = aAlloc->x;
    //    anEvent.point.y = aAlloc->y;
    // HACK
    anEvent.point.x = 0;
    anEvent.point.y = 0;
    anEvent.mWinWidth = aAlloc->width;
    anEvent.mWinHeight = aAlloc->height;
  }

  anEvent.time = PR_IntervalNow();
}

//==============================================================
struct nsKeyConverter {
  int vkCode; // Platform independent key code
  int keysym; // GDK keysym key code
};

//
// Netscape keycodes are defined in widget/public/nsGUIEvent.h
// GTK keycodes are defined in <gdk/gdkkeysyms.h>
//
struct nsKeyConverter nsKeycodes[] = {
  { NS_VK_CANCEL,     GDK_Cancel },
  { NS_VK_BACK,       GDK_BackSpace },
  { NS_VK_TAB,        GDK_Tab },
  { NS_VK_TAB,        GDK_ISO_Left_Tab },
  { NS_VK_CLEAR,      GDK_Clear },
  { NS_VK_RETURN,     GDK_Return },
  { NS_VK_SHIFT,      GDK_Shift_L },
  { NS_VK_SHIFT,      GDK_Shift_R },
  { NS_VK_CONTROL,    GDK_Control_L },
  { NS_VK_CONTROL,    GDK_Control_R },
  { NS_VK_ALT,        GDK_Alt_L },
  { NS_VK_ALT,        GDK_Alt_R },
  { NS_VK_META,       GDK_Meta_L },
  { NS_VK_META,       GDK_Meta_R },
  { NS_VK_PAUSE,      GDK_Pause },
  { NS_VK_CAPS_LOCK,  GDK_Caps_Lock },
  { NS_VK_ESCAPE,     GDK_Escape },
  { NS_VK_SPACE,      GDK_space },
  { NS_VK_PAGE_UP,    GDK_Page_Up },
  { NS_VK_PAGE_DOWN,  GDK_Page_Down },
  { NS_VK_END,        GDK_End },
  { NS_VK_HOME,       GDK_Home },
  { NS_VK_LEFT,       GDK_Left },
  { NS_VK_UP,         GDK_Up },
  { NS_VK_RIGHT,      GDK_Right },
  { NS_VK_DOWN,       GDK_Down },
  { NS_VK_PRINTSCREEN, GDK_Print },
  { NS_VK_INSERT,     GDK_Insert },
  { NS_VK_DELETE,     GDK_Delete },

  // keypad keys
  { NS_VK_LEFT,       GDK_KP_Left },
  { NS_VK_RIGHT,      GDK_KP_Right },
  { NS_VK_UP,         GDK_KP_Up },
  { NS_VK_DOWN,       GDK_KP_Down },
  { NS_VK_PAGE_UP,    GDK_KP_Page_Up },
    // Not sure what these are
    //{ NS_VK_,       GDK_KP_Prior },
    //{ NS_VK_,        GDK_KP_Next },
    // GDK_KP_Begin is the 5 on the non-numlock keypad
    //{ NS_VK_,        GDK_KP_Begin },
  { NS_VK_PAGE_DOWN,  GDK_KP_Page_Down },
  { NS_VK_HOME,       GDK_KP_Home },
  { NS_VK_END,        GDK_KP_End },
  { NS_VK_INSERT,     GDK_KP_Insert },
  { NS_VK_DELETE,     GDK_KP_Delete },

  { NS_VK_MULTIPLY,   GDK_KP_Multiply },
  { NS_VK_ADD,        GDK_KP_Add },
  { NS_VK_SEPARATOR,  GDK_KP_Separator },
  { NS_VK_SUBTRACT,   GDK_KP_Subtract },
  { NS_VK_DECIMAL,    GDK_KP_Decimal },
  { NS_VK_DIVIDE,     GDK_KP_Divide },
  { NS_VK_RETURN,     GDK_KP_Enter },

  { NS_VK_COMMA,      GDK_comma },
  { NS_VK_PERIOD,     GDK_period },
  { NS_VK_SLASH,      GDK_slash },
  { NS_VK_BACK_SLASH, GDK_backslash },
  { NS_VK_BACK_QUOTE, GDK_grave },
  { NS_VK_OPEN_BRACKET, GDK_bracketleft },
  { NS_VK_CLOSE_BRACKET, GDK_bracketright },
  { NS_VK_SEMICOLON, GDK_colon },
  { NS_VK_QUOTE, GDK_apostrophe },

  // NS doesn't have dash or equals distinct from the numeric keypad ones,
  // so we'll use those for now.  See bug 17008:
  { NS_VK_SUBTRACT, GDK_minus },
  { NS_VK_EQUALS, GDK_equal },

  // Some shifted keys, see bug 15463 as well as 17008.
  // These should be subject to different keyboard mappings.
  { NS_VK_QUOTE, GDK_quotedbl },
  { NS_VK_OPEN_BRACKET, GDK_braceleft },
  { NS_VK_CLOSE_BRACKET, GDK_braceright },
  { NS_VK_BACK_SLASH, GDK_bar },
  { NS_VK_SEMICOLON, GDK_semicolon },
  { NS_VK_BACK_QUOTE, GDK_asciitilde },
  { NS_VK_COMMA, GDK_less },
  { NS_VK_PERIOD, GDK_greater },
  { NS_VK_SLASH,      GDK_question },
  { NS_VK_1, GDK_exclam },
  { NS_VK_2, GDK_at },
  { NS_VK_3, GDK_numbersign },
  { NS_VK_4, GDK_dollar },
  { NS_VK_5, GDK_percent },
  { NS_VK_6, GDK_asciicircum },
  { NS_VK_7, GDK_ampersand },
  { NS_VK_8, GDK_asterisk },
  { NS_VK_9, GDK_parenleft },
  { NS_VK_0, GDK_parenright },
  { NS_VK_SUBTRACT, GDK_underscore },
  { NS_VK_EQUALS, GDK_plus }
};

//==============================================================

// Input keysym is in gtk format; output is in NS_VK format
int nsPlatformToDOMKeyCode(GdkEventKey *aGEK)
{
  int i;
  int length = sizeof(nsKeycodes) / sizeof(struct nsKeyConverter);

  int keysym = aGEK->keyval;

  // First, try to handle alphanumeric input, not listed in nsKeycodes:
  // most likely, more letters will be getting typed in than things in
  // the key list, so we will look through these first.

  // since X has different key symbols for upper and lowercase letters and
  // mozilla does not, convert gdk's to mozilla's
  if (keysym >= GDK_a && keysym <= GDK_z)
    return keysym - GDK_a + NS_VK_A;
  if (keysym >= GDK_A && keysym <= GDK_Z)
    return keysym - GDK_A + NS_VK_A;

  // numbers
  if (keysym >= GDK_0 && keysym <= GDK_9)
    return keysym - GDK_0 + NS_VK_0;

  // keypad numbers
  if (keysym >= GDK_KP_0 && keysym <= GDK_KP_9)
    return keysym - GDK_KP_0 + NS_VK_NUMPAD0;

  // misc other things
  for (i = 0; i < length; i++) {
    if (nsKeycodes[i].keysym == keysym)
      return(nsKeycodes[i].vkCode);
  }

  // function keys
  // See bug 57262, Sun's Stop key generates F11 keysym code, in order to
  // make Stop key stops the page loading while not sabotaging F11 key in
  // all platforms, this code catches F11 keysym, then changes it into ESCAPE,
  // which stops the page loading on all platforms.
  // Now, Stop key has the same function as ESC in Solaris
#if defined(SUNOS4) || defined(SOLARIS)
  if (keysym == GDK_F11)
    return NS_VK_ESCAPE;
#endif
  if (keysym >= GDK_F1 && keysym <= GDK_F24)
    return keysym - GDK_F1 + NS_VK_F1;

#if defined(DEBUG_akkana) || defined(DEBUG_ftang)
  printf("No match in nsPlatformToDOMKeyCode: keysym is 0x%x, string is '%s', keyval = %d\n", keysym, aGEK->string, aGEK->keyval);
#endif

  return((int)0);
}

//==============================================================

// Convert gdk key event keyvals to char codes if printable, 0 otherwise
PRUint32 nsConvertCharCodeToUnicode(GdkEventKey* aGEK)
{
  // Anything above 0xf000 is considered a non-printable
  if (aGEK->keyval > 0xf000) {

    // Keypad keys are an exception: they return a value different
    // from their non-keypad equivalents, but mozilla doesn't distinguish.
    switch (aGEK->keyval)
    {
      case GDK_KP_Space:
        return ' ';
      case GDK_KP_Equal:
        return '=';
      case GDK_KP_Multiply:
        return '*';
      case GDK_KP_Add:
        return '+';
      case GDK_KP_Separator:
        return '|';
      case GDK_KP_Subtract:
        return '-';
      case GDK_KP_Decimal:
        return '.';
      case GDK_KP_Divide:
        return '/';
      case GDK_KP_0:
        return '0';
      case GDK_KP_1:
        return '1';
      case GDK_KP_2:
        return '2';
      case GDK_KP_3:
        return '3';
      case GDK_KP_4:
        return '4';
      case GDK_KP_5:
        return '5';
      case GDK_KP_6:
        return '6';
      case GDK_KP_7:
        return '7';
      case GDK_KP_8:
        return '8';
      case GDK_KP_9:
        return '9';
    }

    // non-printables
    return 0;
  }

  // we're supposedly printable, let's try to convert
  long ucs = keysym2ucs(aGEK->keyval);
  if ((ucs != -1) && (ucs < 0x10000))
    return ucs;

  // I guess we couldn't convert
  return 0;
}

//==============================================================
void InitKeyEvent(GdkEventKey *aGEK,
                            gpointer   p,
                            nsKeyEvent &anEvent,
                            PRUint32   aEventType)
{
  anEvent.message = aEventType;
  anEvent.widget  = (nsWidget *) p;

  anEvent.eventStructType = NS_KEY_EVENT;

  if (aGEK != nsnull) {
    anEvent.keyCode = nsPlatformToDOMKeyCode(aGEK);
    anEvent.charCode = 0;
    anEvent.time = aGEK->time;
    anEvent.isShift = (aGEK->state & GDK_SHIFT_MASK) ? PR_TRUE : PR_FALSE;
    anEvent.isControl = (aGEK->state & GDK_CONTROL_MASK) ? PR_TRUE : PR_FALSE;
    anEvent.isAlt = (aGEK->state & GDK_MOD1_MASK) ? PR_TRUE : PR_FALSE;
    anEvent.isMeta = (aGEK->state & GDK_MOD4_MASK) ? PR_TRUE : PR_FALSE;
    anEvent.point.x = 0;
    anEvent.point.y = 0;
  }
}

void InitKeyPressEvent(GdkEventKey *aGEK,
                       gpointer p,
                       nsKeyEvent &anEvent)
{
  //
  // init the basic event fields
  //
  anEvent.eventStructType = NS_KEY_EVENT;
  anEvent.message = NS_KEY_PRESS;
  anEvent.widget = (nsWidget*)p;

  if (aGEK!=nsnull) {
    anEvent.isShift = (aGEK->state & GDK_SHIFT_MASK) ? PR_TRUE : PR_FALSE;
    anEvent.isControl = (aGEK->state & GDK_CONTROL_MASK) ? PR_TRUE : PR_FALSE;
    anEvent.isAlt = (aGEK->state & GDK_MOD1_MASK) ? PR_TRUE : PR_FALSE;
    anEvent.isMeta = (aGEK->state & GDK_MOD4_MASK) ? PR_TRUE : PR_FALSE;

    anEvent.charCode = nsConvertCharCodeToUnicode(aGEK);
    if (anEvent.charCode) {
      anEvent.keyCode = 0;
      
      // if the control, meta, or alt key is down, then we should leave
      // the isShift flag alone (probably not a printable character)
      // if none of the other modifier keys are pressed then we need to
      // clear isShift so the character can be inserted in the editor

      if ( anEvent.isControl || anEvent.isAlt || anEvent.isMeta ) {
         // make Ctrl+uppercase functional as same as Ctrl+lowercase
         // when Ctrl+uppercase(eg.Ctrl+C) is pressed,convert the charCode
         // from uppercase to lowercase(eg.Ctrl+c),so do Alt and Meta Key
         // It is hack code for bug 61355, there is same code snip for
         // Windows platform in widget/src/windows/nsWindow.cpp: See bug 16486
         // Note: if Shift is pressed at the same time, do not to_lower()
         // Because Ctrl+Shift has different function with Ctrl
         if ( !anEvent.isShift &&
              anEvent.charCode >= GDK_A &&
              anEvent.charCode <= GDK_Z )
           anEvent.charCode = gdk_keyval_to_lower(anEvent.charCode);
      } else
        anEvent.isShift = PR_FALSE;
    } else {
      anEvent.keyCode = nsPlatformToDOMKeyCode(aGEK);
    }

#if defined(DEBUG_akkana_not) || defined (DEBUG_ftang)
    if (!aGEK->length) printf("!length, ");
    printf("Key Press event: gtk string = '%s', keyval = '%c' = %d,\n",
           aGEK->string, aGEK->keyval, aGEK->keyval);
    printf("    --> keyCode = 0x%x, char code = '%c'",
           anEvent.keyCode, anEvent.charCode);
    if (anEvent.isShift)
      printf(" [shift]");
    if (anEvent.isControl)
      printf(" [ctrl]");
    if (anEvent.isAlt)
      printf(" [alt]");
    if (anEvent.isMeta)
      printf(" [meta]");
    printf("\n");
#endif

    anEvent.time = aGEK->time;
    anEvent.point.x = 0;
    anEvent.point.y = 0;
  }
}

/*==============================================================
  ==============================================================
  =============================================================
  ==============================================================*/

void handle_size_allocate(GtkWidget *w, GtkAllocation *alloc, gpointer p)
{
  nsWindow *widget = (nsWindow *)p;
  nsSizeEvent event;

  InitAllocationEvent(alloc, p, event, NS_SIZE);
  NS_ADDREF(widget);
  widget->OnResize(&event);
  NS_RELEASE(widget);

  delete event.windowSize;
}

//==============================================================
void handle_scrollbar_value_changed(GtkAdjustment *adj, gpointer p)
{
  nsScrollbar *widget = (nsScrollbar*) p;
  nsScrollbarEvent sevent;

  sevent.message = NS_SCROLLBAR_POS;
  sevent.widget  = (nsWidget *) p;
  sevent.eventStructType = NS_SCROLLBAR_EVENT;

  GdkWindow *win = (GdkWindow *)widget->GetNativeData(NS_NATIVE_WINDOW);
  gdk_window_get_pointer(win, &sevent.point.x, &sevent.point.y, nsnull);

  NS_ADDREF(widget);
  widget->OnScroll(sevent, adj->value);
  NS_RELEASE(widget);

/* FIXME we need to set point.* from the event stuff. */
#if 0
  nsWindow * widgetWindow = (nsWindow *) p ;
  XmScrollBarCallbackStruct * cbs = (XmScrollBarCallbackStruct*) call_data;
  sevent.widget  = (nsWindow *) p;
  if (cbs->event != nsnull) {
    sevent.point.x = cbs->event->xbutton.x;
    sevent.point.y = cbs->event->xbutton.y;
  } else {
    sevent.point.x = 0;
    sevent.point.y = 0;
  }
  sevent.time    = 0; //XXX Implement this

  switch (cbs->reason) {

    case XmCR_INCREMENT:
      sevent.message = NS_SCROLLBAR_LINE_NEXT;
      break;

    case XmCR_DECREMENT:
      sevent.message = NS_SCROLLBAR_LINE_PREV;
      break;

    case XmCR_PAGE_INCREMENT:
      sevent.message = NS_SCROLLBAR_PAGE_NEXT;
      break;

    case XmCR_PAGE_DECREMENT:
      sevent.message = NS_SCROLLBAR_PAGE_PREV;
      break;

    case XmCR_DRAG:
      sevent.message = NS_SCROLLBAR_POS;
      break;

    case XmCR_VALUE_CHANGED:
      sevent.message = NS_SCROLLBAR_POS;
      break;

    default:
      break;
  }
#endif
}

// GTK's text widget already does XIM, so we don't want to do this again
gint handle_key_press_event_for_text(GtkObject *w, GdkEventKey* event,
                                     gpointer p)
{
  nsKeyEvent kevent;
  nsTextWidget* win = (nsTextWidget*)p;

  // work around for annoying things.
  if (event->keyval == GDK_Tab)
    if (event->state & GDK_CONTROL_MASK)
      if (event->state & GDK_MOD1_MASK)
        return PR_FALSE;

  // Don't pass shift, control and alt as key press events
  if (event->keyval == GDK_Shift_L
      || event->keyval == GDK_Shift_R
      || event->keyval == GDK_Control_L
      || event->keyval == GDK_Control_R
      || event->keyval == GDK_Alt_L
      || event->keyval == GDK_Alt_R
     )
    return PR_TRUE;

  NS_ADDREF(win);
  InitKeyEvent(event, p, kevent, NS_KEY_DOWN);
  win->OnKey(kevent);

  //
  // Second, dispatch the Key event as a key press event w/ a Unicode
  //  character code.  Note we have to check for modifier keys, since
  // gtk returns a character value for them
  //
  InitKeyPressEvent(event,p, kevent);
  win->OnKey(kevent);

  NS_RELEASE(win);
  if (w)
  {
    gtk_signal_emit_stop_by_name (GTK_OBJECT(w), "key_press_event");
  }

  return PR_TRUE;
}

// GTK's text widget already does XIM, so we don't want to do this again
gint handle_key_release_event_for_text(GtkObject *w, GdkEventKey* event,
                                       gpointer p)
{
  nsKeyEvent kevent;
  nsTextWidget* win = (nsTextWidget*)p;

  // Don't pass shift, control and alt as key release events
  if (event->keyval == GDK_Shift_L
      || event->keyval == GDK_Shift_R
      || event->keyval == GDK_Control_L
      || event->keyval == GDK_Control_R
      || event->keyval == GDK_Alt_L
      || event->keyval == GDK_Alt_R
     )
    return PR_TRUE;

  InitKeyEvent(event, p, kevent, NS_KEY_UP);
  NS_ADDREF(win);
  win->OnKey(kevent);
  NS_RELEASE(win);
  
  if (w)
  {
    gtk_signal_emit_stop_by_name (GTK_OBJECT(w), "key_release_event");
  }

  return PR_TRUE;
}


//==============================================================
gint handle_key_press_event(GtkObject *w, GdkEventKey* event, gpointer p)
{
  nsKeyEvent kevent;
  nsWidget *win = (nsWidget*)p;

  // if there's a focused window rewrite the event to use that window.
  if (nsWidget::sFocusWindow)
    win = nsWidget::sFocusWindow;

  // work around for annoying things.
  if (event->keyval == GDK_Tab)
    if (event->state & GDK_CONTROL_MASK)
      if (event->state & GDK_MOD1_MASK)
        return PR_FALSE;

  // Don't pass shift and control as key press events
  if (event->keyval == GDK_Shift_L
      || event->keyval == GDK_Shift_R
      || event->keyval == GDK_Control_L
      || event->keyval == GDK_Control_R)
    return PR_TRUE;

  NS_ADDREF(win);

  //
  // First, dispatch the Key event as a virtual key down event
  //   but lie about where it came from and say it is from the
  //   window that currently has focus inside our app...
  //
  InitKeyEvent(event, win, kevent, NS_KEY_DOWN);
  // if we need to suppress this NS_KEY_DOWN event, reset the flag
  if (suppressNextKeyDown == PR_TRUE)
    suppressNextKeyDown = PR_FALSE;
  else
    win->OnKey(kevent);


  //
  // Second, dispatch the Key event as a key press event w/ a Unicode
  //  character code.  Note we have to check for modifier keys, since
  // gtk returns a character value for them
  //

  // Call nsConvertCharCodeToUnicode() here to get kevent.charCode 
  InitKeyPressEvent(event, win, kevent);

  if (event->length) {
    if (kevent.charCode || kevent.keyCode) {
      // kevent.charCode or kevent.keyCode is valid, just pass to OnKey()
      win->OnKey(kevent);
    } else if (nsGtkIMEHelper::GetSingleton()) {
      // commit request from IME
      win->IMECommitEvent(event);
    }
  } else { // for Home/End/Up/Down/Left/Right/PageUp/PageDown key
    win->OnKey(kevent);
  }

  NS_RELEASE(win);

  if (w)
  {
    gtk_signal_emit_stop_by_name (GTK_OBJECT(w), "key_press_event");
  }

  return PR_TRUE;
}

//==============================================================
gint handle_key_release_event(GtkObject *w, GdkEventKey* event, gpointer p)
{
  XEvent nextEvent;
  PRBool    shouldDrop = PR_FALSE;
  // According to the DOM spec if this is the result of a key repeat
  // event we don't let it through since the DOM wants the event
  // stream to look like this: press press press press release.  The
  // way that X does things is to do press release press release, etc.
  // We check to see if this is a key release by checking to see if
  // the next event in the queue is a key press event and it has the
  // exact same timestamp as the current event.

  // have a look in the X queue to see if there's another event in the
  // queue.

  if (XPending(GDK_DISPLAY())) {
    // get a copy of the next event
    XPeekEvent(GDK_DISPLAY(), &nextEvent);
    
    // see if it's a key press event and if it has the same time as
    // the last event.
    if ((nextEvent.xany.type == KeyPress) &&
        (nextEvent.xkey.time == event->time))
    {
      shouldDrop = PR_TRUE;
      // the next key press event shouldn't generate a key down event.
      // this is a global variable
      suppressNextKeyDown = PR_TRUE;
    }
  }

  // should we drop this event?
  if (shouldDrop)
    return PR_TRUE;

  // Don't pass shift, control and alt as key release events
  if (event->keyval == GDK_Shift_L
      || event->keyval == GDK_Shift_R
      || event->keyval == GDK_Control_L
      || event->keyval == GDK_Control_R)
    return PR_TRUE;

  nsWidget *win = (nsWidget *)p;
  if (nsWidget::sFocusWindow)
    win = nsWidget::sFocusWindow;

  nsKeyEvent kevent;
  InitKeyEvent(event, win, kevent, NS_KEY_UP);

  NS_ADDREF(win);
  win->OnKey(kevent);
  NS_RELEASE(win);

  if (w)
  {
    gtk_signal_emit_stop_by_name (GTK_OBJECT(w), "key_release_event");
  }

  return PR_TRUE;
}

//==============================================================
void 
handle_gdk_event (GdkEvent *event, gpointer data)
{
  GtkObject *eventObject = nsnull;

  // set the last time that we got an event.  we need this for drag
  // and drop if you can believe that.
  guint32 event_time = gdk_event_get_time(event);
  if (event_time)
    nsWidget::SetLastEventTime(event_time);

  // Get the next X event serial ID and save it for later for event
  // processing.  If it stays zero then events won't be processed
  // later.  We're using this as a flag too but with a number this big
  // we're probably not going to overflow and even if we did there
  // wouldn't be any harm.
  unsigned long serial = 0;

  if (XPending(GDK_DISPLAY())) {
    XEvent temp_event;
    XPeekEvent(GDK_DISPLAY(), &temp_event);
    serial = temp_event.xany.serial - 1;
  }

  // try to get the user data for the event window.
  if (event->any.window)
    gdk_window_get_user_data (event->any.window, (void **)&eventObject);

  // If there is an event object and it's a superwin then we need to
  // make sure that the event either is handled locally or is passed
  // on with a legitimate GtkWidget object associated with it so the
  // Gtk mainloop doesn't go spastic.

  if (eventObject && GDK_IS_SUPERWIN(eventObject)) {
    // try to find the nsWindow object associated with this superwin
    nsWindow *window = (nsWindow *)gtk_object_get_data (eventObject,
                                                        "nsWindow");

    // If we don't have a window here anymore, we are probably in
    // the process of being or have been destroyed.  give up now.
    if (!window)
      goto end;

    // Find out if there's a grabbing widget.  If there is and it's a
    // regular gtk widget, and our current event window is not the
    // child of that grab widget, we need to rewrite the event to that
    // gtk grabbing widget.
    PRBool rewriteEvent = PR_FALSE;
    GtkWidget *grabWidget = gtk_grab_get_current();
    GtkWidget *owningWidget = window->GetOwningWidget();

    if (grabWidget &&
        !GTK_IS_MOZAREA(grabWidget) &&
        !gdk_window_child_of_gdk_window(owningWidget->window,
                                        grabWidget->window)) {
      rewriteEvent = PR_TRUE;
    }


    // There are a lot of events that are always dispatched to our
    // internal handler, no matter if there is a grab or not.
    switch(event->type)
      {
      case GDK_NOTHING:
        break;

      case GDK_DESTROY:
      case GDK_DELETE:
      case GDK_PROPERTY_NOTIFY:
      case GDK_EXPOSE:
      case GDK_NO_EXPOSE:
      case GDK_FOCUS_CHANGE:
      case GDK_CONFIGURE:
      case GDK_MAP:
      case GDK_UNMAP:
      case GDK_SELECTION_CLEAR:
      case GDK_SELECTION_REQUEST:
      case GDK_SELECTION_NOTIFY:
      case GDK_CLIENT_EVENT:
      case GDK_VISIBILITY_NOTIFY:
        dispatch_superwin_event(event, window);
        break;

      case GDK_BUTTON_PRESS:
      case GDK_2BUTTON_PRESS:
      case GDK_3BUTTON_PRESS:
      case GDK_KEY_PRESS:
      case GDK_KEY_RELEASE:
        // If we need to rewrite this to the nearest real gtk widget,
        // do it here.
        if (rewriteEvent) {
          gdk_window_unref(event->any.window);
          event->any.window = owningWidget->window;
          gdk_window_ref(event->any.window);
          gtk_main_do_event(event);
          break;
        }

        // Otherwise, just send it to our event handler
        if (GTK_WIDGET_IS_SENSITIVE(owningWidget))
          dispatch_superwin_event(event, window);
        break;

      case GDK_MOTION_NOTIFY:
      case GDK_BUTTON_RELEASE:
      case GDK_PROXIMITY_IN:
      case GDK_PROXIMITY_OUT:
        // See above.
        if (rewriteEvent) {
          gdk_window_unref(event->any.window);
          event->any.window = owningWidget->window;
          gdk_window_ref(event->any.window);
          gtk_propagate_event(grabWidget, event);
          break;
        }

        if (GTK_WIDGET_IS_SENSITIVE(owningWidget))
          dispatch_superwin_event(event, window);
        break;

      case GDK_ENTER_NOTIFY:
      case GDK_LEAVE_NOTIFY:
        // Always dispatch enter and leave notify events to the
        // windows that they happened on so that state can be properly
        // tracked.  The code that handles the enter and leave events
        // tracks sensitivity as well.
        dispatch_superwin_event(event, window);
        break;

      default:
        // XXX lots of DND events not handled?  I don't think that we have to.
        NS_WARNING("Odd, hit default case in handle_gdk_event()\n");
        break;
      }
  }
  else {
    nsWindow  *grabbingWindow = nsWindow::GetGrabWindow();
    
    nsCOMPtr<nsIWidget> grabbingWindowGuard(grabbingWindow);
    GtkWidget *tempWidget = NULL;
    
    if (grabbingWindow) {
      // get the GdkWindow that we are grabbing on
      GdkWindow *grabbingGdkWindow =
        NS_STATIC_CAST(GdkWindow *,
                       grabbingWindow->GetNativeData(NS_NATIVE_WINDOW));
      
      // If the object getting the event in question is a GtkWidget
      // and it's a child of the grabbing window, we need to put a
      // grab on that window before dispatching the event.  We do this
      // so that the gtk mainloop will recognize the widget in
      // question as having the grab.  If we didn't the mainloop
      // wouldn't think that the widget that got the event is the
      // child of the grabbing window, due to the superwin(s) in the
      // middle.
      if (GTK_IS_WIDGET(eventObject)) {
        tempWidget = GTK_WIDGET(eventObject);
        if (gdk_window_child_of_gdk_window(tempWidget->window,
                                           grabbingGdkWindow)) {
          // this is so awesome.  gtk_grab_add/gtk_grab_remove don't
          // have exact push and pop semantics.  if we call grab_add
          // on the object and as part of the dispatching of this
          // event another grab_add is called on it, we can end up
          // releasing the grab early because we always call
          // grab_remove on the object after dispatching the event.
          // we can end up with a widget that doesn't have a grab on
          // it, even though it should.  the scrollbar does this.
          //
          // so, what we do here is to check and see if the widget in
          // question has a parent.  if it does and it's a mozbox
          // object we slam a grab on that instead of the widget
          // itself.  we know that no one is doing grabs on those.
          if (tempWidget->parent) {
            if (GTK_IS_MOZBOX(tempWidget->parent)) {
              tempWidget = tempWidget->parent;
            }
          }
          gtk_grab_add(tempWidget);
        }
        else  {
          // if the gtk widget in question wasn't the child of the
          // grabbing window then the grabbing window gets it.
          dispatch_superwin_event(event, grabbingWindow);
          goto end;
        }
      }
    }

    gtk_main_do_event(event);

    if (tempWidget)
      gtk_grab_remove(tempWidget);

    if (event->type == GDK_BUTTON_RELEASE) {
      // Always clear the button motion target when sending a
      // button release event to a real gtk widget, otherwise
      // mozilla will still think it has the grab.  This happens
      // when there's a native gtk widget popup over a Mozilla
      // area.
      nsWidget::DropMotionTarget();
    }
  }

 end:

  // use the saved serial to process any pending events, now that all
  // the window events have been processed
  if (serial)
    nsAppShell::ProcessBeforeID(serial);

}

void
dispatch_superwin_event(GdkEvent *event, nsWindow *window)
{
  if (event->type == GDK_KEY_PRESS || event->type == GDK_KEY_RELEASE)
  {
    // Check to see whether or not we need to send this to the
    // toplevel window to get passed to the GtkWidget with focus.
    // This happens in the embedding case.
    if (!nsWidget::sFocusWindow)
    {
      GtkWidget *mozArea = window->GetOwningWidget();
      NS_ASSERTION(mozArea, "Failed to get GtkMozArea for superwin event!\n");
      // get the toplevel window for that widget
      GtkWidget *toplevel = gtk_widget_get_toplevel(mozArea);
      // pass it off to gtk's event system and return
      gboolean handled = gtk_widget_event(toplevel, event);
      if (handled)
        return;
    }
  }

  switch (event->type)
  {
  case GDK_KEY_PRESS:
    handle_key_press_event (NULL, &event->key, window);
    break;
  case GDK_KEY_RELEASE:
    handle_key_release_event (NULL, &event->key, window);
    break;
  default:
    window->HandleGDKEvent (event);
    break;
  }
}

PRBool
gdk_window_child_of_gdk_window(GdkWindow *window, GdkWindow *ancestor)
{
  GdkWindow *this_window = window;
  while (this_window)
  {
    if (this_window == ancestor)
      return PR_TRUE;
    this_window = gdk_window_get_parent(this_window);
  }
  return PR_FALSE;
}


//==============================================================
void
handle_xlib_shell_event(GdkSuperWin *superwin, XEvent *event, gpointer p)
{
  nsWindow *window = (nsWindow *)p;
  switch(event->xany.type) {
  case ConfigureNotify:
    window->HandleXlibConfigureNotifyEvent(event);
    break;
  default:
    break;
  }
}

//==============================================================
void
handle_superwin_paint(gint aX, gint aY,
                      gint aWidth, gint aHeight, gpointer aData)
{
  nsWindow *window = (nsWindow *)aData;
  nsRect    rect;
  rect.x = aX;
  rect.y = aY;
  rect.width = aWidth;
  rect.height = aHeight;
  window->Invalidate(rect, PR_FALSE);
}

void
handle_superwin_flush(gpointer aData)
{
  nsWindow *window = (nsWindow *)aData;
  window->Update();
}



