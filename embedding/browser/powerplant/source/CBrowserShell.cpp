/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef __CBrowserShell__
#include "CBrowserShell.h"
#endif

#include "nsCWebBrowser.h"
#include "nsIComponentManager.h"
#include "nsWidgetsCID.h"
#include "nsRepeater.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsIWebBrowserChrome.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIWebProgressListener.h"
#include "nsIServiceManager.h"
#include "nsIClipboardCommands.h"
#include "nsIWalletService.h"
#include "nsIDOMWindow.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDOMDocument.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDocument.h"
#include "nsIDOMHTMLCollection.h"
#include "nsIDOMHTMLLinkElement.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsIWebBrowserFind.h"
#include "nsIWebBrowserFocus.h"
#include "nsIWebBrowserPersist.h"
#include "nsIURI.h"
#include "nsWeakPtr.h"
#include "nsRect.h"
#include "nsReadableUtils.h"
#include "nsILocalFile.h"
#include "nsILocalFileMac.h"
#include "nsWeakReference.h"
#include "nsIChannel.h"
#include "nsIWidget.h"
#include "nsIWebBrowserPrint.h"
#include "nsIMacTextInputEventSink.h"
#include "nsCRT.h"

// Local
#include "ApplIDs.h"
#include "CBrowserMsgDefs.h"
#include "CBrowserChrome.h"
#include "CWebBrowserCMAttachment.h"
#include "UMacUnicode.h"

// PowerPlant
#include <UModalDialogs.h>
#include <LStream.h>
#include <UNavServicesDialogs.h>
#include <LEditText.h>
#include <LCheckBox.h>
#include <UEventMgr.h>

// ToolBox
#include <InternetConfig.h>

static NS_DEFINE_IID(kWindowCID, NS_WINDOW_CID);

nsCOMPtr<nsIDragHelperService> CBrowserShell::sDragHelper;

//*****************************************************************************
//***    CBrowserShellProgressListener
//*****************************************************************************

class CBrowserShellProgressListener : public nsIWebProgressListener,
                                      public nsSupportsWeakReference
{
  public:
                                CBrowserShellProgressListener(CBrowserShell* itsOwner);
                                
    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBPROGRESSLISTENER
    
    void                        SetOwner(CBrowserShell* itsOwner)
                                { mpOwner = itsOwner; }
                                
    PRBool                      GetIsLoading()
                                { return mLoading; }
                                
  protected:
    virtual                     ~CBrowserShellProgressListener();
    
  protected:
    CBrowserShell               *mpOwner;
    PRBool                      mLoading;
    PRBool                      mUseRealProgFlag;
    PRInt32                     mFinishedRequests, mTotalRequests;                       
};

NS_IMPL_ISUPPORTS2(CBrowserShellProgressListener, nsIWebProgressListener, nsISupportsWeakReference)

CBrowserShellProgressListener::CBrowserShellProgressListener(CBrowserShell* itsOwner) :
    mpOwner(itsOwner),
    mLoading(PR_FALSE), mUseRealProgFlag(PR_FALSE),
    mFinishedRequests(0), mTotalRequests(0)
{
    NS_INIT_ISUPPORTS();
}

CBrowserShellProgressListener::~CBrowserShellProgressListener()
{
}

NS_IMETHODIMP CBrowserShellProgressListener::OnStateChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRUint32 aStateFlags, nsresult aStatus)
{
    NS_ENSURE_TRUE(mpOwner, NS_ERROR_NULL_POINTER);

    if (aStateFlags & STATE_START)
    {
        if (aStateFlags & STATE_IS_NETWORK)
        {
            MsgNetStartInfo startInfo(mpOwner);
            mpOwner->BroadcastMessage(msg_OnNetStartChange, &startInfo);            
            mLoading = true;
            
            // Init progress vars
            mUseRealProgFlag = false;
            mTotalRequests = 0;
            mFinishedRequests = 0;
        }
        if (aStateFlags & STATE_IS_REQUEST)
            mTotalRequests++;
    }
    else if (aStateFlags & STATE_STOP)
    {
        if (aStateFlags & STATE_IS_REQUEST)
        {
            mFinishedRequests += 1;
        
            if (!mUseRealProgFlag)
            {
                MsgOnProgressChangeInfo progInfo(mpOwner, mFinishedRequests, mTotalRequests);    
                mpOwner->BroadcastMessage(msg_OnProgressChange, &progInfo);
            }
        }
        if (aStateFlags & STATE_IS_NETWORK)
        {
            MsgNetStopInfo stopInfo(mpOwner);
            mpOwner->BroadcastMessage(msg_OnNetStopChange, &stopInfo);
            mLoading = false;
        }
    }
    else if (aStateFlags & STATE_TRANSFERRING)
    {

        if (aStateFlags & STATE_IS_DOCUMENT)
        {
            nsCOMPtr<nsIChannel> channel(do_QueryInterface(aRequest));
            NS_ENSURE_TRUE(channel, NS_ERROR_FAILURE);
            nsCAutoString contentType;
            channel->GetContentType(contentType);
            if (contentType.Equals(NS_LITERAL_CSTRING("text/html")))
                mUseRealProgFlag = true;
        }
        
        if (aStateFlags & STATE_IS_REQUEST)
        {
            if (!mUseRealProgFlag)
            {
                MsgOnProgressChangeInfo progInfo(mpOwner, mFinishedRequests, mTotalRequests);    
                mpOwner->BroadcastMessage(msg_OnProgressChange, &progInfo);
            }
        }
    }
    return NS_OK;
}

NS_IMETHODIMP CBrowserShellProgressListener::OnProgressChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRInt32 aCurSelfProgress, PRInt32 aMaxSelfProgress, PRInt32 aCurTotalProgress, PRInt32 aMaxTotalProgress)
{
    NS_ENSURE_TRUE(mpOwner, NS_ERROR_NULL_POINTER);

    if (!mUseRealProgFlag)
        return NS_OK;
    
    MsgOnProgressChangeInfo progInfo(mpOwner, aCurTotalProgress, aMaxTotalProgress);    
    mpOwner->BroadcastMessage(msg_OnProgressChange, &progInfo);
    
    return NS_OK;
}

NS_IMETHODIMP CBrowserShellProgressListener::OnLocationChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, nsIURI *location)
{
    NS_ENSURE_TRUE(mpOwner, NS_ERROR_NULL_POINTER);
    
    nsCAutoString spec;
    
	if (location)
		location->GetSpec(spec);
		
	MsgLocationChangeInfo info(mpOwner, spec.get());
    mpOwner->BroadcastMessage(msg_OnLocationChange, &info);
    
    return NS_OK;
}

NS_IMETHODIMP CBrowserShellProgressListener::OnStatusChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, nsresult aStatus, const PRUnichar *aMessage)
{
    NS_ENSURE_TRUE(mpOwner, NS_ERROR_NULL_POINTER);

    MsgStatusChangeInfo info(mpOwner, aStatus, aMessage);
    mpOwner->BroadcastMessage(msg_OnStatusChange, &info);

    return NS_OK;
}

NS_IMETHODIMP CBrowserShellProgressListener::OnSecurityChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRUint32 state)
{
    NS_ENSURE_TRUE(mpOwner, NS_ERROR_NULL_POINTER);

    MsgSecurityChangeInfo info(mpOwner, state);
    mpOwner->BroadcastMessage(msg_OnSecurityChange, &info);
    
    return NS_OK;
}


//*****************************************************************************
//***    CBrowserShell: constructors/destructor
//*****************************************************************************

CBrowserShell::CBrowserShell() :
    mChromeFlags(nsIWebBrowserChrome::CHROME_DEFAULT), mIsMainContent(true),
    mContextMenuContext(nsIContextMenuListener::CONTEXT_NONE), mContextMenuDOMNode(nsnull),
    LDropArea(GetMacWindow())
{
	nsresult rv = CommonConstruct();
	if (rv != NS_OK)
	   Throw_Err(NS_ERROR_GET_CODE(rv));   // If this fails, there's no reason to live anymore :(
}


CBrowserShell::CBrowserShell(const SPaneInfo	&inPaneInfo,
						  	 const SViewInfo	&inViewInfo,
						  	 const UInt32       inChromeFlags,
						  	 const Boolean      inIsMainContent) :
    LView(inPaneInfo, inViewInfo), LDropArea(GetMacWindow()),
    mChromeFlags(inChromeFlags), mIsMainContent(inIsMainContent),
    mContextMenuContext(nsIContextMenuListener::CONTEXT_NONE), mContextMenuDOMNode(nsnull)
    
{
	nsresult rv = CommonConstruct();
	if (rv != NS_OK)
	   Throw_Err(NS_ERROR_GET_CODE(rv));   // If this fails, there's no reason to live anymore :(
}


CBrowserShell::CBrowserShell(LStream*	inStream) :
	LView(inStream), LDropArea(GetMacWindow()),
    mContextMenuContext(nsIContextMenuListener::CONTEXT_NONE), mContextMenuDOMNode(nsnull)
{
	*inStream >> mChromeFlags;
	*inStream >> mIsMainContent;
	
	nsresult rv = CommonConstruct();
	if (rv != NS_OK)
	   Throw_Err(NS_ERROR_GET_CODE(rv));   // If this fails, there's no reason to live anymore :(
}


CBrowserShell::~CBrowserShell()
{
    if (mWebBrowser)
        mWebBrowser->SetContainerWindow(nsnull);
    
    if (mChrome)
    {
        mChrome->SetBrowserShell(nsnull);
        NS_RELEASE(mChrome);
    }
    if (mProgressListener)
    {
        mProgressListener->SetOwner(nsnull);
        NS_RELEASE(mProgressListener);
    }
}


NS_IMETHODIMP CBrowserShell::CommonConstruct()
{
    nsresult  rv;

    mWebBrowser = do_CreateInstance(NS_WEBBROWSER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

    nsCOMPtr<nsIBaseWindow> baseWin(do_QueryInterface(mWebBrowser));
    NS_ENSURE_TRUE(baseWin, NS_ERROR_FAILURE);
    mWebBrowserAsBaseWin = baseWin;

    nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mWebBrowser));
    NS_ENSURE_TRUE(webNav, NS_ERROR_FAILURE);
    mWebBrowserAsWebNav = webNav;
    
    mChrome = new CBrowserChrome(this, mChromeFlags, mIsMainContent);
    NS_ENSURE_TRUE(mChrome, NS_ERROR_FAILURE);
    NS_ADDREF(mChrome);
    AddListener(mChrome);
    mWebBrowser->SetContainerWindow(mChrome);
        
    mProgressListener = new CBrowserShellProgressListener(this);
    NS_ENSURE_TRUE(mProgressListener, NS_ERROR_FAILURE);
    NS_ADDREF(mProgressListener);

    return NS_OK;
}

/**
 * It is a nescesary evil to create a top level window widget in order to
 * have a parent for our nsIBaseWindow. In order to not put that responsibility
 * onto the PowerPlant window which contains us, we do it ourselves here by
 * creating the widget if it does not exist and storing it as a window property.
 */
 
NS_IMETHODIMP CBrowserShell::EnsureTopLevelWidget(nsIWidget **aWidget)
{
    NS_ENSURE_ARG_POINTER(aWidget);
    *aWidget = nsnull;
    
    OSStatus err;
    nsresult rv;
    nsIWidget *widget = nsnull;

    err = ::GetWindowProperty(Compat_GetMacWindow(), 'PPMZ', 'WIDG', sizeof(nsIWidget*), nsnull, (void*)&widget);
    if (err == noErr && widget) {
        *aWidget = widget;
        NS_ADDREF(*aWidget);
        return NS_OK;
    }

	// Create it with huge bounds. The actual bounds that matters is that of the
	// nsIBaseWindow. The bounds of the top level widget clips its children so
	// we just have to make sure it is big enough to always contain the children.
    
    nsCOMPtr<nsIWidget> newWidget(do_CreateInstance(kWindowCID, &rv));
    NS_ENSURE_SUCCESS(rv, rv);
	    
	nsRect r(0, 0, 32000, 32000);
	rv = newWidget->Create(Compat_GetMacWindow(), r, nsnull, nsnull, nsnull, nsnull, nsnull);
	NS_ENSURE_SUCCESS(rv, rv);

	widget = newWidget;
    err = ::SetWindowProperty(Compat_GetMacWindow(), 'PPMZ', 'WIDG', sizeof(nsIWidget*), (void*)&widget);
    if (err == noErr) {
        *aWidget = newWidget;
        NS_ADDREF(*aWidget);
        return NS_OK;
    }
    
    return NS_ERROR_FAILURE;
}


//*****************************************************************************
//***    CBrowserShell: LPane overrides
//*****************************************************************************

void CBrowserShell::FinishCreateSelf()
{
	FocusDraw();
		
	nsCOMPtr<nsIWidget> aWidget;
	ThrowIfError_(EnsureTopLevelWidget(getter_AddRefs(aWidget)));
	
	// the widget is also our avenue for dispatching events into Gecko via
	// nsIEventSink. Save this sink for later.
	mEventSink = do_QueryInterface(aWidget);
	ThrowIfNil_(mEventSink);

	Rect portFrame;
	CalcPortFrameRect(portFrame);
	nsRect   r(portFrame.left, portFrame.top, portFrame.right - portFrame.left, portFrame.bottom - portFrame.top);
	
	nsresult rv;
	
    mWebBrowserAsBaseWin->InitWindow(aWidget->GetNativeData(NS_NATIVE_WIDGET), nsnull, r.x, r.y, r.width, r.height);
    mWebBrowserAsBaseWin->Create();
        
    // Hook up our progress listener
    nsWeakPtr weakling(dont_AddRef(NS_GetWeakReference((nsIWebProgressListener *)mProgressListener)));
    rv = mWebBrowser->AddWebBrowserListener(weakling, NS_GET_IID(nsIWebProgressListener));
    NS_ASSERTION(NS_SUCCEEDED(rv), "Call to AddWebBrowserListener failed");
      
    AdjustFrame();   
	StartRepeating();
}


void CBrowserShell::ResizeFrameBy(SInt16	inWidthDelta,
                				  SInt16	inHeightDelta,
                				  Boolean	inRefresh)
{
	LView::ResizeFrameBy(inWidthDelta, inHeightDelta, inRefresh);
	AdjustFrame();
}


void CBrowserShell::MoveBy(SInt32	inHorizDelta,
				           SInt32	inVertDelta,
						   Boolean	inRefresh)
{
	LView::MoveBy(inHorizDelta, inVertDelta, inRefresh);
	AdjustFrame();
}


void CBrowserShell::ShowSelf()
{
    mWebBrowserAsBaseWin->SetVisibility(PR_TRUE);
}


void CBrowserShell::DrawSelf()
{
    EventRecord osEvent;
    osEvent.what = updateEvt;
    PRBool handled = PR_FALSE;
    mEventSink->DispatchEvent(&osEvent, &handled);
}

	
void CBrowserShell::ClickSelf(const SMouseDownEvent	&inMouseDown)
{
	if (!IsTarget())
		SwitchTarget(this);

	FocusDraw();
  PRBool handled = PR_FALSE;
	mEventSink->DispatchEvent(&const_cast<EventRecord&>(inMouseDown.macEvent), &handled);
}


void CBrowserShell::EventMouseUp(const EventRecord	&inMacEvent)
{
    FocusDraw();
    PRBool handled = PR_FALSE;
    mEventSink->DispatchEvent(&const_cast<EventRecord&>(inMacEvent), &handled);
    
    LEventDispatcher *dispatcher = LEventDispatcher::GetCurrentEventDispatcher();
    if (dispatcher)
        dispatcher->UpdateMenus();
}

#if __PowerPlant__ >= 0x02200000
void CBrowserShell::AdjustMouseSelf(Point				inPortPt,
                                    const EventRecord&	inMacEvent,
                                    RgnHandle			outMouseRgn)
{
    static Point	lastWhere = {0, 0};

    if ((*(long*)&lastWhere != *(long*)&inMacEvent.where))
    {
        HandleMouseMoved(inMacEvent);
        lastWhere = inMacEvent.where;
    }
    Rect cursorRect = { inPortPt.h, inPortPt.v, inPortPt.h + 1, inPortPt.v + 1 };
    ::RectRgn(outMouseRgn, &cursorRect);
}

#else

void CBrowserShell::AdjustCursorSelf(Point				/* inPortPt */,
                                     const EventRecord&	inMacEvent)
{
    static Point	lastWhere = {0, 0};

    if ((*(long*)&lastWhere != *(long*)&inMacEvent.where))
    {
        HandleMouseMoved(inMacEvent);
        lastWhere = inMacEvent.where;
    }
}
#endif

//*****************************************************************************
//***    CBrowserShell: LCommander overrides
//*****************************************************************************

void CBrowserShell::BeTarget()
{
    nsCOMPtr<nsIWebBrowserFocus>  focus(do_GetInterface(mWebBrowser));
    if (focus)
        focus->Activate();
}

void CBrowserShell::DontBeTarget()
{
    nsCOMPtr<nsIWebBrowserFocus>  focus(do_GetInterface(mWebBrowser));
    if (focus)
        focus->Deactivate();
}

Boolean CBrowserShell::HandleKeyPress(const EventRecord	&inKeyEvent)
{
	// set the QuickDraw origin
	FocusDraw();

	// dispatch the event
  PRBool handled = PR_FALSE;
	Boolean keyHandled = mEventSink->DispatchEvent(&const_cast<EventRecord&>(inKeyEvent), &handled);

	return keyHandled;
}

Boolean CBrowserShell::ObeyCommand(PP_PowerPlant::CommandT inCommand, void* ioParam)
{
	Boolean		cmdHandled = true;

    nsresult rv;
    nsCOMPtr<nsIClipboardCommands> clipCmd;

    switch (inCommand)
    {
        case cmd_Back:
            Back();
            break;
 
        case cmd_Forward:
            Forward();
            break;
            
        case cmd_Stop:
            Stop();
            break;
            
        case cmd_Reload:
            Reload();
            break;

        case cmd_SaveAs:
            rv = SaveCurrentURI();
            ThrowIfError_(rv);
            break;
            
        case cmd_SaveAllAs:
            rv = SaveDocument();
            ThrowIfError_(rv);
            break;
            
        case cmd_Cut:
            rv = GetClipboardHandler(getter_AddRefs(clipCmd));
            if (NS_SUCCEEDED(rv))
                clipCmd->CutSelection();
            break;

        case cmd_Copy:
            rv = GetClipboardHandler(getter_AddRefs(clipCmd));
            if (NS_SUCCEEDED(rv))
                clipCmd->CopySelection();
            break;

        case cmd_Paste:
            rv = GetClipboardHandler(getter_AddRefs(clipCmd));
            if (NS_SUCCEEDED(rv))
                clipCmd->Paste();
            break;

        case cmd_SelectAll:
            rv = GetClipboardHandler(getter_AddRefs(clipCmd));
            if (NS_SUCCEEDED(rv))
                clipCmd->SelectAll();
            break;

		case cmd_Find:
			Find();
			break;

		case cmd_FindNext:
			FindNext();
			break;

        case cmd_OpenLinkInNewWindow:
            {               
                // Get the URL from the link
                ThrowIfNil_(mContextMenuDOMNode);
                nsCOMPtr<nsIDOMHTMLAnchorElement> linkElement(do_QueryInterface(mContextMenuDOMNode, &rv));
                ThrowIfError_(rv);
                
                nsAutoString href;
                rv = linkElement->GetHref(href);
                ThrowIfError_(rv);
                
                nsCAutoString urlSpec;
                CopyUCS2toASCII(href, urlSpec);
                PostOpenURLEvent(urlSpec);
            }
            break;
            
		case cmd_SaveFormData:
            {
                nsCOMPtr<nsIDOMWindow> domWindow;
                nsCOMPtr<nsIDOMWindowInternal> domWindowInternal;

                nsCOMPtr<nsIWalletService> walletService = 
                         do_GetService(NS_WALLETSERVICE_CONTRACTID, &rv);
                ThrowIfError_(rv);
                rv = mWebBrowser->GetContentDOMWindow(getter_AddRefs(domWindow));
                ThrowIfError_(rv);
                domWindowInternal = do_QueryInterface(domWindow, &rv);
                ThrowIfError_(rv);
                PRUint32 retval;
                rv = walletService->WALLET_RequestToCapture(domWindowInternal, &retval);
            }
            break;
		  
		case cmd_PrefillForm:
            {
                nsCOMPtr<nsIDOMWindow> domWindow;
                nsCOMPtr<nsIDOMWindowInternal> domWindowInternal;

                nsCOMPtr<nsIWalletService> walletService = 
                         do_GetService(NS_WALLETSERVICE_CONTRACTID, &rv);
                ThrowIfError_(rv);
                rv = mWebBrowser->GetContentDOMWindow(getter_AddRefs(domWindow));
                ThrowIfError_(rv);
                domWindowInternal = do_QueryInterface(domWindow, &rv);
                ThrowIfError_(rv);
                PRBool retval;
                // Don't check the result - A result of NS_ERROR_FAILURE means not to show preview dialog
                rv = walletService->WALLET_Prefill(true, domWindowInternal, &retval);
            }
            break;

        case cmd_ViewPageSource:
            {
                nsCAutoString currentURL;
                rv = GetCurrentURL(currentURL);
                ThrowIfError_(rv);
                currentURL.Insert("view-source:", 0);
                PostOpenURLEvent(currentURL);
            }
            break;

        default:
            cmdHandled = LCommander::ObeyCommand(inCommand, ioParam);
            break;
    }
    return cmdHandled;
}


void CBrowserShell::FindCommandStatus(PP_PowerPlant::CommandT inCommand,
            		                  Boolean &outEnabled, Boolean &outUsesMark,
            					      UInt16 &outMark, Str255 outName)
{
    nsresult rv;
    nsCOMPtr<nsIClipboardCommands> clipCmd;
    PRBool haveContent, canDo;
    nsCOMPtr<nsIURI> currURI;

    rv = mWebBrowserAsWebNav->GetCurrentURI(getter_AddRefs(currURI));
    haveContent = NS_SUCCEEDED(rv) && currURI;
    
    switch (inCommand)
    {
        case cmd_Back:
            outEnabled = CanGoBack();
            break;
 
        case cmd_Forward:
            outEnabled = CanGoForward();
            break;
            
        case cmd_Stop:
            outEnabled = IsBusy();
            break;
            
        case cmd_Reload:
            outEnabled = haveContent;
            break;

        case cmd_SaveAs:
        case cmd_SaveAllAs:
            outEnabled = haveContent;
            break;
            
        case cmd_Cut:
            if (haveContent) {
                rv = GetClipboardHandler(getter_AddRefs(clipCmd));
                if (NS_SUCCEEDED(rv)) {
                    rv = clipCmd->CanCutSelection(&canDo);
                    outEnabled = NS_SUCCEEDED(rv) && canDo;
                }
            }
            break;

        case cmd_Copy:
            if (haveContent) {
                rv = GetClipboardHandler(getter_AddRefs(clipCmd));
                if (NS_SUCCEEDED(rv)) {
                    rv = clipCmd->CanCopySelection(&canDo);
                    outEnabled = NS_SUCCEEDED(rv) && canDo;
                }
            }
            break;

        case cmd_Paste:
            if (haveContent) {
                rv = GetClipboardHandler(getter_AddRefs(clipCmd));
                if (NS_SUCCEEDED(rv)) {
                    rv = clipCmd->CanPaste(&canDo);
                    outEnabled = NS_SUCCEEDED(rv) && canDo;
                }
            }
            break;

        case cmd_SelectAll:
            outEnabled = haveContent;
            break;

		case cmd_Find:
			outEnabled = haveContent;
			break;

		case cmd_FindNext:
			outEnabled = haveContent && CanFindNext();
			break;

        case cmd_OpenLinkInNewWindow:
            outEnabled = haveContent && ((mContextMenuContext & nsIContextMenuListener::CONTEXT_LINK) != 0);
            break;
            
        case cmd_ViewPageSource:
            outEnabled = haveContent && ((mChromeFlags & nsIWebBrowserChrome::CHROME_OPENAS_CHROME) == 0);
            break;
            
        case cmd_ViewImage:
        case cmd_CopyImageLocation:
            outEnabled = haveContent && ((mContextMenuContext & nsIContextMenuListener::CONTEXT_IMAGE) != 0);
            break;
            
        case cmd_CopyLinkLocation:
            outEnabled = haveContent && ((mContextMenuContext & nsIContextMenuListener::CONTEXT_LINK) != 0);
            break;
        
		case cmd_SaveFormData:
		    outEnabled = haveContent && HasFormElements();
		    break;

		case cmd_PrefillForm:
		    outEnabled = haveContent && HasFormElements();
		    break;

        default:
            LCommander::FindCommandStatus(inCommand, outEnabled,
        					              outUsesMark, outMark, outName);
            break;
    }
}


//*****************************************************************************
//***    CBrowserShell: LPeriodical overrides
//*****************************************************************************

void CBrowserShell::SpendTime(const EventRecord&		inMacEvent)
{
    switch (inMacEvent.what)
    {
        case osEvt:
        {
            // The event sink will not set the cursor if we are in the background - which is right.
            // We have to feed it suspendResumeMessages for it to know

            unsigned char eventType = ((inMacEvent.message >> 24) & 0x00ff);
            if (eventType == suspendResumeMessage) {
              PRBool handled = PR_FALSE;
              mEventSink->DispatchEvent(&const_cast<EventRecord&>(inMacEvent), &handled);
            }
        }
        break;
    }
}


//*****************************************************************************
//***    CBrowserShell:
//*****************************************************************************

void CBrowserShell::AddAttachments()
{
    // Only add a context menu attachment for full browser windows -
    // not view-source and chrome dialogs.
    if ((mChromeFlags & (nsIWebBrowserChrome::CHROME_TOOLBAR |
                         nsIWebBrowserChrome::CHROME_STATUSBAR)) != 0)
    {
        CWebBrowserCMAttachment *cmAttachment = new CWebBrowserCMAttachment(this);
        ThrowIfNil_(cmAttachment);
        cmAttachment->SetCommandList(mcmd_BrowserShellContextMenuCmds);
        AddAttachment(cmAttachment);
    }
}

NS_METHOD CBrowserShell::GetWebBrowser(nsIWebBrowser** aBrowser)
{
    NS_ENSURE_ARG_POINTER(aBrowser);

    *aBrowser = mWebBrowser;
    NS_IF_ADDREF(*aBrowser);
    return NS_OK;
}


NS_METHOD CBrowserShell::SetWebBrowser(nsIWebBrowser* aBrowser)
{
    NS_ENSURE_ARG(aBrowser);

    FocusDraw();

    /*
    CBrowserWindow *ourWindow = dynamic_cast<CBrowserWindow*>(LWindow::FetchWindowObject(Compat_GetMacWindow()));
    NS_ENSURE_TRUE(ourWindow, NS_ERROR_FAILURE);

    nsCOMPtr<nsIWidget>  aWidget;
    ourWindow->GetWidget(getter_AddRefs(aWidget));
    NS_ENSURE_TRUE(aWidget, NS_ERROR_FAILURE);
    */
    
	nsCOMPtr<nsIWidget> aWidget;
	ThrowIfError_(EnsureTopLevelWidget(getter_AddRefs(aWidget)));

    mWebBrowser = aBrowser;

    nsCOMPtr<nsIBaseWindow> baseWin(do_QueryInterface(mWebBrowser));
    NS_ENSURE_TRUE(baseWin, NS_ERROR_FAILURE);
    mWebBrowserAsBaseWin = baseWin;

    nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mWebBrowser));
    NS_ENSURE_TRUE(webNav, NS_ERROR_FAILURE);
    mWebBrowserAsWebNav = webNav;

    Rect portFrame;
    CalcPortFrameRect(portFrame);
    nsRect   r(portFrame.left, portFrame.top, portFrame.right - portFrame.left, portFrame.bottom - portFrame.top);
    	
    mWebBrowserAsBaseWin->InitWindow(aWidget->GetNativeData(NS_NATIVE_WIDGET), nsnull, r.x, r.y, r.width, r.height);
    mWebBrowserAsBaseWin->Create();

    AdjustFrame();   

    return NS_OK;
}

NS_METHOD CBrowserShell::GetWebBrowserChrome(nsIWebBrowserChrome** aChrome)
{
    NS_ENSURE_ARG_POINTER(aChrome);
    return mChrome->QueryInterface(NS_GET_IID(nsIWebBrowserChrome), (void **)aChrome);
}

NS_METHOD CBrowserShell::GetContentViewer(nsIContentViewer** aViewer)
{
    nsCOMPtr<nsIDocShell> ourDocShell(do_GetInterface(mWebBrowser));
    NS_ENSURE_TRUE(ourDocShell, NS_ERROR_FAILURE);
    return ourDocShell->GetContentViewer(aViewer);
}


NS_METHOD CBrowserShell::GetPrintSettings(nsIPrintSettings** aSettings)
{
    NS_ENSURE_ARG_POINTER(aSettings);
    *aSettings = nsnull;
    
    if (!mPrintSettings) {
        // If we don't have print settings yet, make new ones.
        nsCOMPtr<nsIWebBrowserPrint> wbPrint(do_GetInterface(mWebBrowser));
        NS_ENSURE_TRUE(wbPrint, NS_ERROR_NO_INTERFACE);
        wbPrint->GetNewPrintSettings(getter_AddRefs(mPrintSettings));
    }
    if (mPrintSettings) {
        *aSettings = mPrintSettings;
        NS_ADDREF(*aSettings);
        return NS_OK;
    }
    return NS_ERROR_FAILURE;
}

//*****************************************************************************
//***    CBrowserShell: Navigation
//*****************************************************************************

Boolean CBrowserShell::IsBusy()
{
    return mProgressListener->GetIsLoading();
}


Boolean CBrowserShell::CanGoBack()
{
    PRBool      canDo;
    nsresult    rv;

    rv = mWebBrowserAsWebNav->GetCanGoBack(&canDo);
    return (NS_SUCCEEDED(rv) && canDo);
}


Boolean CBrowserShell::CanGoForward()
{
    PRBool      canDo;
    nsresult    rv;

    rv = mWebBrowserAsWebNav->GetCanGoForward(&canDo);
    return (NS_SUCCEEDED(rv) && canDo);
}


NS_METHOD CBrowserShell::Back()
{
   nsresult rv;
    
    if (CanGoBack())
      rv = mWebBrowserAsWebNav->GoBack();
   else
   {
      ::SysBeep(5);
      rv = NS_ERROR_FAILURE;
   }
   return rv;
}

NS_METHOD CBrowserShell::Forward()
{
   nsresult rv;

    if (CanGoForward())
      rv = mWebBrowserAsWebNav->GoForward();
   else
   {
      ::SysBeep(5);
      rv = NS_ERROR_FAILURE;
   }
   return rv;
}

NS_METHOD CBrowserShell::Stop()
{
   return mWebBrowserAsWebNav->Stop(nsIWebNavigation::STOP_ALL);
}

NS_METHOD CBrowserShell::Reload()
{
    return mWebBrowserAsWebNav->Reload(nsIWebNavigation::LOAD_FLAGS_NONE);
}

//*****************************************************************************
//***    CBrowserShell: URL Loading
//*****************************************************************************


NS_METHOD CBrowserShell::LoadURL(const nsACString& urlText)
{
    nsAutoString unicodeURL;
    CopyASCIItoUCS2(urlText, unicodeURL);
    return mWebBrowserAsWebNav->LoadURI(unicodeURL.get(),
                                        nsIWebNavigation::LOAD_FLAGS_NONE,
                                        nsnull,
                                        nsnull,
                                        nsnull);
}

NS_METHOD CBrowserShell::GetCurrentURL(nsACString& urlText)
{
    nsresult rv;
    nsCOMPtr<nsIURI> currentURI;
    rv = mWebBrowserAsWebNav->GetCurrentURI(getter_AddRefs(currentURI));
    if (NS_FAILED(rv)) return rv;
    rv = currentURI->GetSpec(urlText);
    if (NS_FAILED(rv)) return rv;
    
    return NS_OK;
}

//*****************************************************************************
//***    CBrowserShell: URI Saving
//*****************************************************************************

NS_METHOD CBrowserShell::SaveDocument()
{
    FSSpec      fileSpec;
    Boolean     isReplacing;
    nsresult    rv = NS_OK;
    
    if (DoSaveFileDialog(fileSpec, isReplacing)) {
        if (isReplacing) {
            OSErr err = ::FSpDelete(&fileSpec);
            if (err) return NS_ERROR_FAILURE;
        }
        rv = SaveDocument(fileSpec);
    }
    return rv;
}

NS_METHOD CBrowserShell::SaveCurrentURI()
{
    FSSpec      fileSpec;
    Boolean     isReplacing;
    nsresult    rv = NS_OK;
    
    if (DoSaveFileDialog(fileSpec, isReplacing)) {
        if (isReplacing) {
            OSErr err = ::FSpDelete(&fileSpec);
            if (err) return NS_ERROR_FAILURE;
        }
        rv = SaveCurrentURI(fileSpec);
    }
    return rv;
}

NS_METHOD CBrowserShell::SaveDocument(const FSSpec& outSpec)
{
    nsresult    rv;
    
    nsCOMPtr<nsIWebBrowserPersist> wbPersist(do_GetInterface(mWebBrowser, &rv));
    if (NS_FAILED(rv)) return rv;
    nsCOMPtr<nsIDOMDocument> domDoc;
    rv = mWebBrowserAsWebNav->GetDocument(getter_AddRefs(domDoc));
    if (NS_FAILED(rv)) return rv;

    FSSpec nonConstOutSpec = outSpec;    
    nsCOMPtr<nsILocalFileMac> localFile;
    rv = NS_NewLocalFileWithFSSpec(&nonConstOutSpec, PR_FALSE, getter_AddRefs(localFile));
    if (NS_FAILED(rv)) return rv;
    nsCOMPtr<nsIFile> parentDir;
    rv = localFile->GetParent(getter_AddRefs(parentDir));
    if (NS_FAILED(rv)) return rv;
    nsCOMPtr<nsILocalFile> parentDirAsLocal(do_QueryInterface(parentDir, &rv));
    if (NS_FAILED(rv)) return rv;

    rv = wbPersist->SaveDocument(domDoc, localFile, parentDirAsLocal, nsnull, 0, 0);
    
    return rv;
}

NS_METHOD CBrowserShell::SaveCurrentURI(const FSSpec& outSpec)
{
    nsresult    rv;
    
    nsCOMPtr<nsIWebBrowserPersist> wbPersist(do_GetInterface(mWebBrowser, &rv));
    if (NS_FAILED(rv)) return rv;

    FSSpec nonConstOutSpec = outSpec;    
    nsCOMPtr<nsILocalFileMac> localFile;
    rv = NS_NewLocalFileWithFSSpec(&nonConstOutSpec, PR_FALSE, getter_AddRefs(localFile));
    if (NS_FAILED(rv)) return rv;

    rv = wbPersist->SaveURI(nsnull, nsnull, localFile);
    
    return rv;
}

Boolean CBrowserShell::DoSaveFileDialog(FSSpec& outSpec, Boolean& outIsReplacing)
{
	UNavServicesDialogs::LFileDesignator	designator;

    nsresult rv;
    nsAutoString docTitle;
    Str255       defaultName;

    nsCOMPtr<nsIDOMDocument> domDoc;
    rv = mWebBrowserAsWebNav->GetDocument(getter_AddRefs(domDoc));
    if (NS_SUCCEEDED(rv)) {
        nsCOMPtr<nsIDOMHTMLDocument> htmlDoc(do_QueryInterface(domDoc, &rv));
        if (NS_SUCCEEDED(rv))
            htmlDoc->GetTitle(docTitle);
    }
    
    // For now, we'll assume that we're saving HTML
    NS_NAMED_LITERAL_STRING(htmlSuffix, ".html");
    if (docTitle.IsEmpty())
        docTitle.Assign(NS_LITERAL_STRING("untitled"));
    else {
        if (docTitle.Length() > 31 - htmlSuffix.Length())
            docTitle.Truncate(31 - htmlSuffix.Length());
    }
    docTitle.Append(htmlSuffix);
    CPlatformUCSConversion::GetInstance()->UCSToPlatform(docTitle, defaultName);

    Boolean     result;
    
    result = designator.AskDesignateFile(defaultName);
    if (result) {
        designator.GetFileSpec(outSpec);
        outIsReplacing = designator.IsReplacing();
    }
    
    return result;
}

//*****************************************************************************
//***    CBrowserShell: Text Searching
//*****************************************************************************

Boolean CBrowserShell::Find()
{
    nsresult rv;
    nsXPIDLString stringBuf;
    PRBool  findBackwards;
    PRBool  wrapFind;
    PRBool  entireWord;
    PRBool  matchCase;

    nsCOMPtr<nsIWebBrowserFind> finder(do_GetInterface(mWebBrowser));
    if (!finder) return FALSE;
    finder->GetSearchString(getter_Copies(stringBuf));
    finder->GetFindBackwards(&findBackwards);    
    finder->GetWrapFind(&wrapFind);    
    finder->GetEntireWord(&entireWord);    
    finder->GetMatchCase(&matchCase);    

    Boolean     result = FALSE;
    nsString    searchString(stringBuf.get());

    if (DoFindDialog(searchString, findBackwards, wrapFind, entireWord, matchCase))
    {
        PRBool  didFind;

        finder->SetSearchString(searchString.get());
        finder->SetFindBackwards(findBackwards);    
        finder->SetWrapFind(wrapFind);    
        finder->SetEntireWord(entireWord);    
        finder->SetMatchCase(matchCase);    
        rv = finder->FindNext(&didFind);
        result = (NS_SUCCEEDED(rv) && didFind);
        if (!result)
          ::SysBeep(1);
    }
    return result;
}

Boolean CBrowserShell::Find(const nsAString& searchString,
                            Boolean findBackwards,
                            Boolean wrapFind,
                            Boolean entireWord,
                            Boolean matchCase)
{
    nsresult    rv;
    Boolean     result;
    PRBool      didFind;

    nsCOMPtr<nsIWebBrowserFind> finder(do_GetInterface(mWebBrowser));
    if (!finder) return FALSE;

    finder->SetSearchString(PromiseFlatString(searchString).get());
    finder->SetFindBackwards(findBackwards);    
    finder->SetWrapFind(wrapFind);    
    finder->SetEntireWord(entireWord);    
    finder->SetMatchCase(matchCase);    

    rv = finder->FindNext(&didFind);
    result = (NS_SUCCEEDED(rv) && didFind);
    if (!result)
        ::SysBeep(1);

    return result;
}

Boolean CBrowserShell::CanFindNext()
{
    nsresult    rv;

    nsCOMPtr<nsIWebBrowserFind> finder(do_GetInterface(mWebBrowser));
    if (!finder) return FALSE;
    
    nsXPIDLString searchStr;
    rv = finder->GetSearchString(getter_Copies(searchStr));
    return (NS_SUCCEEDED(rv) && nsCRT::strlen(searchStr) != 0);
}


Boolean CBrowserShell::FindNext()
{
    nsresult    rv;
    Boolean     result;
    PRBool      didFind;

    nsCOMPtr<nsIWebBrowserFind> finder(do_GetInterface(mWebBrowser));
    if (!finder) return FALSE;
    rv = finder->FindNext(&didFind);
    result = (NS_SUCCEEDED(rv) && didFind);
    if (!result)
        ::SysBeep(1);

    return result;
}


NS_METHOD CBrowserShell::OnShowContextMenu(PRUint32 aContextFlags,
                                           nsIDOMEvent *aEvent,
                                           nsIDOMNode *aNode)
{
    // Find our CWebBrowserCMAttachment, if any
    CWebBrowserCMAttachment *aCMAttachment = nsnull;
    const TArray<LAttachment*>* theAttachments = GetAttachmentsList();
    
    if (theAttachments) {
        TArrayIterator<LAttachment*> iterate(*theAttachments);
        
        LAttachment*    theAttach;
        while (iterate.Next(theAttach)) {
            aCMAttachment = dynamic_cast<CWebBrowserCMAttachment*>(theAttach);
            if (aCMAttachment != nil)
                break;
        }
    }
    if (!aCMAttachment) {
        NS_ASSERTION(PR_FALSE, "No CWebBrowserCMAttachment");
        return NS_OK;
    }
                
    EventRecord macEvent;        
    UEventMgr::GetMouseAndModifiers(macEvent);
    
    mContextMenuContext = aContextFlags;
    mContextMenuDOMNode = aNode;
    aCMAttachment->DoContextMenuClick(macEvent);
    mContextMenuContext = 0;
    mContextMenuDOMNode = nsnull;

    return NS_OK;
}
                                          
NS_METHOD CBrowserShell::OnShowTooltip(PRInt32 aXCoords,
                                       PRInt32 aYCoords,
                                       const PRUnichar *aTipText)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD CBrowserShell::OnHideTooltip()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


void CBrowserShell::HandleMouseMoved(const EventRecord& inMacEvent)
{
    if (IsActive())
    {
        FocusDraw();
        PRBool handled = PR_FALSE;
        mEventSink->DispatchEvent(&const_cast<EventRecord&>(inMacEvent), &handled);
    }
}


void CBrowserShell::AdjustFrame()
{
    FocusDraw();
    	
    Rect portFrame;
    CalcPortFrameRect(portFrame);
    nsRect r(portFrame.left, portFrame.top, portFrame.right - portFrame.left, portFrame.bottom - portFrame.top);
    		
    mWebBrowserAsBaseWin->SetPositionAndSize(r.x, r.y, r.width, r.height, PR_TRUE);
}


Boolean CBrowserShell::DoFindDialog(nsAString& searchText,
                                     PRBool& findBackwards,
                                     PRBool& wrapFind,
                                     PRBool& entireWord,
                                     PRBool& caseSensitive)
{
    enum
    {
        kSearchTextEdit       = FOUR_CHAR_CODE('Text'),
        kCaseSensitiveCheck   = FOUR_CHAR_CODE('Case'),
        kWrapAroundCheck      = FOUR_CHAR_CODE('Wrap'),
        kSearchBackwardsCheck = FOUR_CHAR_CODE('Back'),
        kEntireWordCheck      = FOUR_CHAR_CODE('Entr')
    };

    Boolean     result;

    try
    {
        // Create stack-based object for handling the dialog box

        StDialogHandler	theHandler(dlog_FindDialog, this);
        LWindow			*theDialog = theHandler.GetDialog();
        Str255          aStr;

        // Set initial text for string in dialog box

        CPlatformUCSConversion::GetInstance()->UCSToPlatform(searchText, aStr);

        LEditText	*editField = dynamic_cast<LEditText*>(theDialog->FindPaneByID(kSearchTextEdit));
        editField->SetDescriptor(aStr);
        theDialog->SetLatentSub(editField);

        LCheckBox   *caseSensCheck, *entireWordCheck, *wrapAroundCheck, *backwardsCheck;

        caseSensCheck = dynamic_cast<LCheckBox*>(theDialog->FindPaneByID(kCaseSensitiveCheck));
        ThrowIfNot_(caseSensCheck);
        caseSensCheck->SetValue(caseSensitive ? 1 : 0);
        entireWordCheck = dynamic_cast<LCheckBox*>(theDialog->FindPaneByID(kEntireWordCheck));
        ThrowIfNot_(entireWordCheck);
        entireWordCheck->SetValue(entireWord ? 1 : 0);
        wrapAroundCheck = dynamic_cast<LCheckBox*>(theDialog->FindPaneByID(kWrapAroundCheck));
        ThrowIfNot_(wrapAroundCheck);
        wrapAroundCheck->SetValue(wrapFind ? 1 : 0);
        backwardsCheck = dynamic_cast<LCheckBox*>(theDialog->FindPaneByID(kSearchBackwardsCheck));
        ThrowIfNot_(backwardsCheck);
        backwardsCheck->SetValue(findBackwards ? 1 : 0);

        theDialog->Show();

        while (true)  // This is our modal dialog event loop
        {				
            MessageT	hitMessage = theHandler.DoDialog();

            if (hitMessage == msg_Cancel)
            {
                result = FALSE;
                break;
   			
            }
            else if (hitMessage == msg_OK)
            {
                editField->GetDescriptor(aStr);
                CPlatformUCSConversion::GetInstance()->PlatformToUCS(aStr, searchText);

                caseSensitive = caseSensCheck->GetValue() ? TRUE : FALSE;
                entireWord = entireWordCheck->GetValue() ? TRUE : FALSE;
                wrapFind = wrapAroundCheck->GetValue() ? TRUE : FALSE;
                findBackwards = backwardsCheck->GetValue() ? TRUE : FALSE;

                result = TRUE;
                break;
            }
        }
    }
	catch (...)
	{
	   result = FALSE;
	   // Don't propagate the error.
	}
	
	return result;
}

nsresult CBrowserShell::GetClipboardHandler(nsIClipboardCommands **aCommand)
{
    NS_ENSURE_ARG_POINTER(aCommand);

    nsCOMPtr<nsIClipboardCommands> clipCmd(do_GetInterface(mWebBrowser));
    NS_ENSURE_TRUE(clipCmd, NS_ERROR_FAILURE);

    *aCommand = clipCmd;
    NS_ADDREF(*aCommand);
    return NS_OK;
}

Boolean CBrowserShell::HasFormElements()
{
    nsresult rv;
    
    nsCOMPtr<nsIDOMWindow> domWindow;
    rv = mWebBrowser->GetContentDOMWindow(getter_AddRefs(domWindow));
    if (NS_SUCCEEDED(rv)) {
        nsCOMPtr<nsIDOMDocument> domDoc;
        rv = domWindow->GetDocument(getter_AddRefs(domDoc));
        if (NS_SUCCEEDED(rv)) {
            nsCOMPtr<nsIDOMHTMLDocument> htmlDoc = do_QueryInterface(domDoc);
            if (htmlDoc) {
                nsCOMPtr<nsIDOMHTMLCollection> forms;
                htmlDoc->GetForms(getter_AddRefs(forms));
                if (forms) {
                    PRUint32 numForms;
                    forms->GetLength(&numForms);
                    return numForms > 0;
                }
            }
        }
    }
    return false;
}

void CBrowserShell::PostOpenURLEvent(const nsACString& url)
{    
    // Send an AppleEvent to ourselves to open a new window with the given URL
    
    // IMPORTANT: We need to make our target address using a ProcessSerialNumber
    // from GetCurrentProcess. This will cause our AppleEvent to be handled from
    // the event loop. Creating and showing a new window before the context menu
    // click is done being processed is fatal. If we make the target address with a
    // ProcessSerialNumber in which highLongOfPSN == 0 && lowLongOfPSN == kCurrentProcess,
    // the event will be dispatched to us directly and we die.
    
    OSErr err;
    ProcessSerialNumber currProcess;
    StAEDescriptor selfAddrDesc;
    
    err = ::GetCurrentProcess(&currProcess);
    ThrowIfOSErr_(err);
    err = ::AECreateDesc(typeProcessSerialNumber, (Ptr) &currProcess,
                         sizeof(currProcess), selfAddrDesc);
    ThrowIfOSErr_(err);

    AppleEvent  getURLEvent;
    err = ::AECreateAppleEvent(kInternetEventClass, kAEGetURL,
                            selfAddrDesc,
                            kAutoGenerateReturnID,
                            kAnyTransactionID,
                            &getURLEvent);
    ThrowIfOSErr_(err);

    const nsPromiseFlatCString& flatURL = PromiseFlatCString(url);  
    StAEDescriptor urlDesc(typeChar, flatURL.get(), flatURL.Length());
    
    err = ::AEPutParamDesc(&getURLEvent, keyDirectObject, urlDesc);
    if (err) {
        ::AEDisposeDesc(&getURLEvent);
        Throw_(err);
    }
    UAppleEventsMgr::SendAppleEvent(getURLEvent);
}


void
CBrowserShell::InsideDropArea(DragReference inDragRef)
{
  if ( sDragHelper ) {
    PRBool dropAllowed = PR_FALSE;
    sDragHelper->Tracking ( inDragRef, mEventSink, &dropAllowed );
  }
}


void
CBrowserShell::EnterDropArea( DragReference inDragRef, Boolean inDragHasLeftSender)
{
  sDragHelper = do_GetService ( "@mozilla.org/widget/draghelperservice;1" );
  NS_ASSERTION ( sDragHelper, "Couldn't get a drag service, we're in biiig trouble" );
  if ( sDragHelper )
    sDragHelper->Enter ( inDragRef, mEventSink );
}


void
CBrowserShell::LeaveDropArea( DragReference inDragRef )
{
  if ( sDragHelper ) {
    sDragHelper->Leave ( inDragRef, mEventSink );
    sDragHelper = nsnull;      
  }
}


Boolean
CBrowserShell::PointInDropArea(Point inGlobalPt)
{
  return true;
}

Boolean
CBrowserShell::DragIsAcceptable( DragReference inDragRef )
{
  return true;
}

void
CBrowserShell::DoDragReceive( DragReference inDragRef )
{
  if ( sDragHelper ) {
    PRBool dragAccepted = PR_FALSE;
    sDragHelper->Drop ( inDragRef, mEventSink, &dragAccepted );
  }
}

#pragma mark -

OSStatus CBrowserShell::HandleUpdateActiveInputArea(
  const nsAString& text, 
  PRInt16 script,  
  PRInt16 language, 
  PRInt32 fixLen, 
  const TextRangeArray * hiliteRng
)
{
  nsCOMPtr<nsIMacTextInputEventSink> tieSink;
  tieSink = do_QueryInterface(mEventSink);
  if (!tieSink)
    return eventNotHandledErr;
    
  OSStatus err = noErr;
  nsresult res = tieSink->HandleUpdateActiveInputArea( text,
                                                       script, language, 
                                                       fixLen, (void*)hiliteRng, 
                                                       &err);
  if (noErr != err)
    return err;
  return NS_FAILED(res) ? eventNotHandledErr : noErr;
}

OSStatus CBrowserShell::HandleUnicodeForKeyEvent(
  const nsAString& text, 
  PRInt16 script,
  PRInt16 language, 
  const EventRecord * keyboardEvent)
{
  nsCOMPtr<nsIMacTextInputEventSink> tieSink;
  tieSink = do_QueryInterface(mEventSink);
  if (!tieSink)
    return eventNotHandledErr;
    
  OSStatus err = noErr;
  nsresult res = tieSink->HandleUnicodeForKeyEvent( text,
                                                    script, language, 
                                                    (void*)keyboardEvent, 
                                                    &err);
  if (noErr != err)
    return err;
  return NS_FAILED(res) ? eventNotHandledErr : noErr;
}

OSStatus CBrowserShell::HandleOffsetToPos(
  PRInt32 offset, 
  PRInt16 *pointX, 
  PRInt16 *pointY)
{
  nsCOMPtr<nsIMacTextInputEventSink> tieSink;
  tieSink = do_QueryInterface(mEventSink);
  if (!tieSink)
    return eventNotHandledErr;
    
  OSStatus err = noErr;
  nsresult res = tieSink->HandleOffsetToPos( offset, pointX, pointY, &err);
  if (noErr != err)
    return err;
  return NS_FAILED(res) ? eventNotHandledErr : noErr;
}

OSStatus CBrowserShell::HandlePosToOffset(
  PRInt16 currentPointX, 
  PRInt16 currentPointY, 
  PRInt32 *offset,
  PRInt16 *regionClass
)
{
  nsCOMPtr<nsIMacTextInputEventSink> tieSink;
  tieSink = do_QueryInterface(mEventSink);
  if (!tieSink)
    return eventNotHandledErr;
    
  OSStatus err = noErr;
  nsresult res = tieSink->HandlePosToOffset( currentPointX, currentPointY, 
                                             offset, regionClass, &err);
  if (noErr != err)
    return err;
  return NS_FAILED(res) ? eventNotHandledErr : noErr;
}
