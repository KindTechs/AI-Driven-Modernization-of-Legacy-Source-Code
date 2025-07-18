/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Conrad Carlen <ccarlen@netscape.com>
 */

#include "EmbedEventHandling.h"
#include "CTextInputEventHandler.h"

#include "nsRepeater.h"
#include "prthread.h"
#include "SIOUX.h"

#include "nsIWidget.h"
#include "nsIEventSink.h"
#include "nsCOMPtr.h"

// Static Variables

WindowPtr        CEmbedEventAttachment::mLastAlienWindowClicked;

//*****************************************************************************
//  CEmbedEventAttachment
//
//  Handles events for windows popped up by Gecko which are not LWindows
//
//*****************************************************************************

CEmbedEventAttachment::CEmbedEventAttachment()
{
}

CEmbedEventAttachment::~CEmbedEventAttachment()
{
}


//
// GetTopWidget
//
// We've stashed the nsIWidget for the given windowPtr in the data 
// properties of the window. Fetch it.
//
void
CEmbedEventAttachment::GetTopWidget ( WindowPtr aWindow, nsIWidget** outWidget )
{
  nsIWidget* topLevelWidget = nsnull;
	::GetWindowProperty ( aWindow, 'MOSS', 'GEKO', sizeof(nsIWidget*), nsnull, (void*)&topLevelWidget);
  if ( topLevelWidget ) {
    *outWidget = topLevelWidget;
    NS_ADDREF(*outWidget);
  }
}


//
// GetWindowEventSink
//
// We've stashed the nsIEventSink for the given windowPtr in the data of the
// root control. Fetch it.
//
void
CEmbedEventAttachment::GetWindowEventSink ( WindowPtr aWindow, nsIEventSink** outSink )
{
  *outSink = nsnull;
  
  nsCOMPtr<nsIWidget> topWidget;
  GetTopWidget ( aWindow, getter_AddRefs(topWidget) );
  nsCOMPtr<nsIEventSink> sink ( do_QueryInterface(topWidget) );
  if ( sink ) {
    *outSink = sink;
    NS_ADDREF(*outSink);
  }
}


void CEmbedEventAttachment::ExecuteSelf(MessageT	inMessage,
	                                    void*		ioParam)
{
	SetExecuteHost(true);

    EventRecord *inMacEvent;
    WindowPtr	macWindowP;
	
	// 1. Handle the msg_AdjustCursor message. PowerPlant
	// sends this message to attachments which are attached
	// to views but not those attached to the application.
	// IMPORTANT: For this to be called, the application's
	// AdjustCursor() method must be overridden to send the
	// msg_AdjustCursor to its attachments.
		
    if (inMessage == msg_AdjustCursor) {
        inMacEvent = static_cast<EventRecord*>(ioParam);
        ::MacFindWindow(inMacEvent->where, &macWindowP);
        if (IsAlienGeckoWindow(macWindowP)) {
            nsCOMPtr<nsIEventSink> sink;
            GetWindowEventSink(macWindowP, getter_AddRefs(sink));
            if ( sink ) {
              PRBool handled = PR_FALSE;
              sink->DispatchEvent(inMacEvent, &handled);
              SetExecuteHost(false);
            }
        }
		
	} else if (inMessage == msg_Event) {
        inMacEvent = static_cast<EventRecord*>(ioParam);

#ifdef DEBUG
        // 2. See if the event is for the console window.
        // Limit what types of events we send to it.
        if ((inMacEvent->what == mouseDown ||
            inMacEvent->what == updateEvt ||
            inMacEvent->what == activateEvt ||
            inMacEvent->what == keyDown) &&
            SIOUXHandleOneEvent(inMacEvent))
        {
            SetExecuteHost(false);
            return;
        }
#endif
    	 
    	// 3. Finally, see if we have an event for a Gecko window
    	// which is not an LWindow. These get created by widget to
    	// show pop-up list boxes - grrr. The following events are
    	// sufficient for those.
    	   	  
        switch (inMacEvent->what)
  	    {	
  		    case mouseDown:
  		    {
	            SInt16 thePart = ::MacFindWindow(inMacEvent->where, &macWindowP);
	            if (thePart == inContent && IsAlienGeckoWindow(macWindowP)) {
                  nsCOMPtr<nsIEventSink> sink;
                  GetWindowEventSink(macWindowP, getter_AddRefs(sink));
                  if ( sink ) {
                    PRBool handled = PR_FALSE;
  	                sink->DispatchEvent(inMacEvent, &handled);
  	                mLastAlienWindowClicked = macWindowP;
  	                SetExecuteHost(false);
  	              }
	            }
	            else
	                mLastAlienWindowClicked = nil;
	        }
  			break;
  			
  		    case mouseUp:
  		    {
  		        if (mLastAlienWindowClicked) {
                nsCOMPtr<nsIEventSink> sink;
                GetWindowEventSink(mLastAlienWindowClicked, getter_AddRefs(sink));
                if ( sink ) {
                  PRBool handled = PR_FALSE;
  		            sink->DispatchEvent(inMacEvent, &handled);
  		            mLastAlienWindowClicked = nil;
  		            SetExecuteHost(false);
  		          }
  		        }
  		    }
  			break;
  			  	
  		    case updateEvt:
  		    case activateEvt:
  		    {
  		      macWindowP = (WindowPtr)inMacEvent->message;
  			    if (IsAlienGeckoWindow(macWindowP)) {
                nsCOMPtr<nsIEventSink> sink;
                GetWindowEventSink(macWindowP, getter_AddRefs(sink));
                if ( sink ) {
                  PRBool handled = PR_FALSE;
  		            sink->DispatchEvent(inMacEvent, &handled);
  		            SetExecuteHost(false);
  		          }
  			    }
  			}
  			break;  			
  	    }
	}
}


Boolean CEmbedEventAttachment::IsAlienGeckoWindow(WindowPtr inMacWindow)
{
    PRBool isAlien = PR_FALSE;
    
    // it's an 'alien' window if there's no LWindow object and there is
    // an nsIEventSink stashed in the window's root control.
    if (inMacWindow && !LWindow::FetchWindowObject(inMacWindow)) {
      nsCOMPtr<nsIEventSink> sink;
      GetWindowEventSink ( inMacWindow, getter_AddRefs(sink) );
      if ( sink )
        isAlien = PR_TRUE;   
    }
    
    return isAlien;
}


//*****************************************************************************
//  CEmbedIdler
//
//
//*****************************************************************************

CEmbedIdler::CEmbedIdler()
{
}


CEmbedIdler::~CEmbedIdler()
{
}
  
void CEmbedIdler::SpendTime(const EventRecord&	inMacEvent)
{
    Repeater::DoIdlers(inMacEvent);
    ::PR_Sleep(PR_INTERVAL_NO_WAIT);
}


//*****************************************************************************
//  CEmbedRepeater
//
//
//*****************************************************************************

CEmbedRepeater::CEmbedRepeater()
{
}


CEmbedRepeater::~CEmbedRepeater()
{
}
  
void CEmbedRepeater::SpendTime(const EventRecord&	inMacEvent)
{
	Repeater::DoRepeaters(inMacEvent);
}


//*****************************************************************************
// Initialization Function - Call at application startup. 
//*****************************************************************************

void InitializeEmbedEventHandling(LApplication* theApplication)
{
    CEmbedEventAttachment *windowAttachment = new CEmbedEventAttachment;
    ThrowIfNil_(windowAttachment);
    theApplication->AddAttachment(windowAttachment);

    CEmbedIdler *embedIdler = new CEmbedIdler;
    ThrowIfNil_(embedIdler);
    embedIdler->StartIdling();

    CEmbedRepeater *embedRepeater = new CEmbedRepeater;
    ThrowIfNil_(embedRepeater);
    embedRepeater->StartRepeating();
    
#if TARGET_CARBON
    InitializeTextInputEventHandling();
#endif

}
   