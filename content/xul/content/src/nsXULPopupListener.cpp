/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *   Dean Tessman <dean_tessman@hotmail.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Robert O'Callahan <roc+moz@cs.cmu.edu>
 *
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

/*
  This file provides the implementation for xul popup listener which
  tracks xul popups and context menus
 */

#include "nsCOMPtr.h"
#include "nsXULAtoms.h"
#include "nsIDOMElement.h"
#include "nsIDOMXULElement.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMDocumentXBL.h"
#include "nsIXULPopupListener.h"
#include "nsIDOMMouseListener.h"
#include "nsIDOMContextMenuListener.h"
#include "nsContentCID.h"

#include "nsIScriptGlobalObject.h"
#include "nsIScriptContext.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDOMXULDocument.h"
#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsIDOMMouseEvent.h"
#include "nsIDOMNSUIEvent.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMNSEvent.h"

#include "nsIBoxObject.h"
#include "nsIPopupBoxObject.h"

// for event firing in context menus
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIEventStateManager.h"

#include "nsIFrame.h"
#include "nsIStyleContext.h"

// on win32 and os/2, context menus come up on mouse up. On other platforms,
// they appear on mouse down. Certain bits of code care about this difference.
#ifndef XP_PC
#define NS_CONTEXT_MENU_IS_MOUSEUP 1
#endif


////////////////////////////////////////////////////////////////////////
// PopupListenerImpl
//
//   This is the popup listener implementation for popup menus and context menus.
//
class XULPopupListenerImpl : public nsIXULPopupListener,
                             public nsIDOMMouseListener,
                             public nsIDOMContextMenuListener
{
public:
    XULPopupListenerImpl(void);
    virtual ~XULPopupListenerImpl(void);

public:
    // nsISupports
    NS_DECL_ISUPPORTS

    // nsIXULPopupListener
    NS_IMETHOD Init(nsIDOMElement* aElement, const XULPopupType& popupType);

    // nsIDOMMouseListener
    NS_IMETHOD MouseDown(nsIDOMEvent* aMouseEvent);
    NS_IMETHOD MouseUp(nsIDOMEvent* aMouseEvent) { return NS_OK; };
    NS_IMETHOD MouseClick(nsIDOMEvent* aMouseEvent) { return NS_OK; };
    NS_IMETHOD MouseDblClick(nsIDOMEvent* aMouseEvent) { return NS_OK; };
    NS_IMETHOD MouseOver(nsIDOMEvent* aMouseEvent) { return NS_OK; };
    NS_IMETHOD MouseOut(nsIDOMEvent* aMouseEvent) { return NS_OK; };

    // nsIDOMContextMenuListener
    NS_IMETHOD ContextMenu(nsIDOMEvent* aContextMenuEvent);

    // nsIDOMEventListener
    NS_IMETHOD HandleEvent(nsIDOMEvent* anEvent) { return NS_OK; };

protected:

    virtual nsresult LaunchPopup(nsIDOMEvent* anEvent);
    virtual nsresult LaunchPopup(PRInt32 aClientX, PRInt32 aClientY) ;
    virtual void ClosePopup();

private:

    nsresult PreLaunchPopup(nsIDOMEvent* aMouseEvent);
    nsresult FireFocusOnTargetContent(nsIDOMNode* aTargetNode);

    // |mElement| is the node to which this listener is attached.
    nsIDOMElement* mElement;               // Weak ref. The element will go away first.

    // The popup that is getting shown on top of mElement.
    nsIDOMElement* mPopupContent; 

    // The type of the popup
    XULPopupType popupType;
    
};

////////////////////////////////////////////////////////////////////////
      
XULPopupListenerImpl::XULPopupListenerImpl(void)
  : mElement(nsnull), mPopupContent(nsnull)
{
	NS_INIT_REFCNT();	
}

XULPopupListenerImpl::~XULPopupListenerImpl(void)
{
  ClosePopup();
  
#ifdef DEBUG_REFS
    --gInstanceCount;
    fprintf(stdout, "%d - RDF: XULPopupListenerImpl\n", gInstanceCount);
#endif
}

NS_IMPL_ADDREF(XULPopupListenerImpl)
NS_IMPL_RELEASE(XULPopupListenerImpl)


NS_INTERFACE_MAP_BEGIN(XULPopupListenerImpl)
  NS_INTERFACE_MAP_ENTRY(nsIXULPopupListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMouseListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMContextMenuListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIDOMEventListener, nsIDOMMouseListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIXULPopupListener)
NS_INTERFACE_MAP_END

NS_IMETHODIMP
XULPopupListenerImpl::Init(nsIDOMElement* aElement, const XULPopupType& popup)
{
  mElement = aElement; // Weak reference. Don't addref it.
  popupType = popup;
  return NS_OK;
}

////////////////////////////////////////////////////////////////
// nsIDOMMouseListener

nsresult
XULPopupListenerImpl::MouseDown(nsIDOMEvent* aMouseEvent)
{
  if(popupType != eXULPopupType_context)
    return PreLaunchPopup(aMouseEvent);
  else
    return NS_OK;
}

nsresult
XULPopupListenerImpl::ContextMenu(nsIDOMEvent* aMouseEvent)
{
  if(popupType == eXULPopupType_context)
    return PreLaunchPopup(aMouseEvent);
  else 
    return NS_OK;
}

nsresult
XULPopupListenerImpl::PreLaunchPopup(nsIDOMEvent* aMouseEvent)
{
  PRUint16 button;

  nsCOMPtr<nsIDOMMouseEvent> mouseEvent;
  mouseEvent = do_QueryInterface(aMouseEvent);
  if (!mouseEvent) {
    //non-ui event passed in.  bad things.
    return NS_OK;
  }

  // check if someone has attempted to prevent this action.
  nsCOMPtr<nsIDOMNSUIEvent> nsUIEvent;
  nsUIEvent = do_QueryInterface(mouseEvent);
  if (!nsUIEvent) {
    return NS_OK;
  }

  PRBool preventDefault;
  nsUIEvent->GetPreventDefault(&preventDefault);
  if (preventDefault) {
    // someone called preventDefault. bail.
    return NS_OK;
  }

  // Get the node that was clicked on.
  nsCOMPtr<nsIDOMEventTarget> target;
  mouseEvent->GetTarget( getter_AddRefs( target ) );
  nsCOMPtr<nsIDOMNode> targetNode;
  if (target) targetNode = do_QueryInterface(target);

  // This is a gross hack to deal with a recursive popup situation happening in AIM code. 
  // See http://bugzilla.mozilla.org/show_bug.cgi?id=96920.
  // If a menu item child was clicked on that leads to a popup needing
  // to show, we know (guaranteed) that we're dealing with a menu or
  // submenu of an already-showing popup.  We don't need to do anything at all.
  if (popupType == eXULPopupType_popup) {
    nsCOMPtr<nsIContent> targetContent = do_QueryInterface(target);
    nsCOMPtr<nsIAtom> tag;
    targetContent->GetTag(*getter_AddRefs(tag));
    if (tag && (tag.get() == nsXULAtoms::menu || tag.get() == nsXULAtoms::menuitem))
      return NS_OK;
  }

  // Get the document with the popup.
  nsCOMPtr<nsIDocument> document;
  nsCOMPtr<nsIContent> content = do_QueryInterface(mElement);
  nsresult rv;
  if (NS_FAILED(rv = content->GetDocument(*getter_AddRefs(document)))) {
    NS_ERROR("Unable to retrieve the document.");
    return rv;
  }

  // Turn the document into a XUL document so we can use SetPopupNode.
  nsCOMPtr<nsIDOMXULDocument> xulDocument = do_QueryInterface(document);
  if (xulDocument == nsnull) {
    NS_ERROR("Popup attached to an element that isn't in XUL!");
    return NS_ERROR_FAILURE;
  }

  // Store clicked-on node in xul document for context menus and menu popups.
  xulDocument->SetPopupNode( targetNode );

  nsCOMPtr<nsIDOMNSEvent> nsevent(do_QueryInterface(aMouseEvent));

  switch (popupType) {
    case eXULPopupType_popup:
      // Check for left mouse button down
      mouseEvent->GetButton(&button);
      if (button == 0) {
        // Time to launch a popup menu.
        LaunchPopup(aMouseEvent);

        if (nsevent) {
            nsevent->PreventBubble();
        }

        aMouseEvent->PreventDefault();
      }
      break;
    case eXULPopupType_context:

    // Time to launch a context menu
#ifndef NS_CONTEXT_MENU_IS_MOUSEUP
    // If the context menu launches on mousedown,
    // we have to fire focus on the content we clicked on
    FireFocusOnTargetContent(targetNode);
#endif
    LaunchPopup(aMouseEvent);

    if (nsevent) {
        nsevent->PreventBubble();
    }

    aMouseEvent->PreventDefault();
    break;
  }
  return NS_OK;
}

nsresult
XULPopupListenerImpl::FireFocusOnTargetContent(nsIDOMNode* aTargetNode)
{
  nsresult rv;
  nsCOMPtr<nsIDOMDocument> domDoc;
  rv = aTargetNode->GetOwnerDocument(getter_AddRefs(domDoc));
  if(NS_SUCCEEDED(rv) && domDoc)
  {
    nsCOMPtr<nsIPresShell> shell;
    nsCOMPtr<nsIPresContext> context;
    nsCOMPtr<nsIDocument> tempdoc = do_QueryInterface(domDoc);
    tempdoc->GetShellAt(0, getter_AddRefs(shell));  // Get nsIDOMElement for targetNode
    if (!shell)
        return NS_ERROR_FAILURE;

    shell->GetPresContext(getter_AddRefs(context));
 
    nsCOMPtr<nsIContent> content = do_QueryInterface(aTargetNode);
    nsIFrame* targetFrame;
    shell->GetPrimaryFrameFor(content, &targetFrame);
    if (!targetFrame) return NS_ERROR_FAILURE;
      
    PRBool suppressBlur = PR_FALSE;
    const nsStyleUserInterface* ui;
    targetFrame->GetStyleData(eStyleStruct_UserInterface, ((const nsStyleStruct*&)ui));
    suppressBlur = (ui->mUserFocus == NS_STYLE_USER_FOCUS_IGNORE);

    nsCOMPtr<nsIDOMElement> element;
    nsCOMPtr<nsIContent> newFocus = do_QueryInterface(content);

    nsIFrame* currFrame = targetFrame;
    // Look for the nearest enclosing focusable frame.
    while (currFrame) {
        const nsStyleUserInterface* ui;
        currFrame->GetStyleData(eStyleStruct_UserInterface, ((const nsStyleStruct*&)ui));
        if ((ui->mUserFocus != NS_STYLE_USER_FOCUS_IGNORE) &&
            (ui->mUserFocus != NS_STYLE_USER_FOCUS_NONE)) 
        {
          currFrame->GetContent(getter_AddRefs(newFocus));
          nsCOMPtr<nsIDOMElement> domElement(do_QueryInterface(newFocus));
          if (domElement) {
            element = domElement;
            break;
          }
        }
        currFrame->GetParent(&currFrame);
    } 
    nsCOMPtr<nsIContent> focusableContent = do_QueryInterface(element);
    nsCOMPtr<nsIEventStateManager> esm;
    context->GetEventStateManager(getter_AddRefs(esm));

    if (focusableContent)
      focusableContent->SetFocus(context);
    else if (!suppressBlur)
      esm->SetContentState(nsnull, NS_EVENT_STATE_FOCUS);

    esm->SetContentState(focusableContent, NS_EVENT_STATE_ACTIVE);
  }
  return rv;
}

//
// ClosePopup
//
// Do everything needed to shut down the popup.
//
// NOTE: This routine is safe to call even if the popup is already closed.
//
void
XULPopupListenerImpl :: ClosePopup ( )
{
  if ( mPopupContent ) {
    nsCOMPtr<nsIDOMXULElement> popupElement(do_QueryInterface(mPopupContent));
    nsCOMPtr<nsIBoxObject> boxObject;
    if (popupElement)
      popupElement->GetBoxObject(getter_AddRefs(boxObject));
    nsCOMPtr<nsIPopupBoxObject> popupObject(do_QueryInterface(boxObject));
    if (popupObject)
      popupObject->HidePopup();

    mPopupContent = nsnull;  // release the popup
  }

} // ClosePopup

//
// LaunchPopup
//
nsresult
XULPopupListenerImpl::LaunchPopup ( nsIDOMEvent* anEvent )
{
  // Retrieve our x and y position.
  nsCOMPtr<nsIDOMMouseEvent> mouseEvent ( do_QueryInterface(anEvent) );
  if (!mouseEvent) {
    //non-ui event passed in.  bad things.
    return NS_OK;
  }

  PRInt32 xPos, yPos;
  mouseEvent->GetClientX(&xPos); 
  mouseEvent->GetClientY(&yPos); 

  return LaunchPopup(xPos, yPos);
}


static void
GetImmediateChild(nsIContent* aContent, nsIAtom *aTag, nsIContent** aResult) 
{
  *aResult = nsnull;
  PRInt32 childCount;
  aContent->ChildCount(childCount);
  for (PRInt32 i = 0; i < childCount; i++) {
    nsCOMPtr<nsIContent> child;
    aContent->ChildAt(i, *getter_AddRefs(child));
    nsCOMPtr<nsIAtom> tag;
    child->GetTag(*getter_AddRefs(tag));
    if (aTag == tag.get()) {
      *aResult = child;
      NS_ADDREF(*aResult);
      return;
    }
  }

  return;
}

static void ConvertPosition(nsIDOMElement* aPopupElt, nsString& aAnchor, nsString& aAlign, PRInt32& aY)
{
  nsAutoString position;
  aPopupElt->GetAttribute(NS_LITERAL_STRING("position"), position);
  if (position.IsEmpty())
    return;

  if (position.Equals(NS_LITERAL_STRING("before_start"))) {
    aAnchor.Assign(NS_LITERAL_STRING("topleft"));
    aAlign.Assign(NS_LITERAL_STRING("bottomleft"));
  }
  else if (position.Equals(NS_LITERAL_STRING("before_end"))) {
    aAnchor.Assign(NS_LITERAL_STRING("topright"));
    aAlign.Assign(NS_LITERAL_STRING("bottomright"));
  }
  else if (position.Equals(NS_LITERAL_STRING("after_start"))) {
    aAnchor.Assign(NS_LITERAL_STRING("bottomleft"));
    aAlign.Assign(NS_LITERAL_STRING("topleft"));
  }
  else if (position.Equals(NS_LITERAL_STRING("after_end"))) {
    aAnchor.Assign(NS_LITERAL_STRING("bottomright"));
    aAlign.Assign(NS_LITERAL_STRING("topright"));
  }
  else if (position.Equals(NS_LITERAL_STRING("start_before"))) {
    aAnchor.Assign(NS_LITERAL_STRING("topleft"));
    aAlign.Assign(NS_LITERAL_STRING("topright"));
  }
  else if (position.Equals(NS_LITERAL_STRING("start_after"))) {
    aAnchor.Assign(NS_LITERAL_STRING("bottomleft"));
    aAlign.Assign(NS_LITERAL_STRING("bottomright"));
  }
  else if (position.Equals(NS_LITERAL_STRING("end_before"))) {
    aAnchor.Assign(NS_LITERAL_STRING("topright"));
    aAlign.Assign(NS_LITERAL_STRING("topleft"));
  }
  else if (position.Equals(NS_LITERAL_STRING("end_after"))) {
    aAnchor.Assign(NS_LITERAL_STRING("bottomright"));
    aAlign.Assign(NS_LITERAL_STRING("bottomleft"));
  }
  else if (position.Equals(NS_LITERAL_STRING("overlap"))) {
    aAnchor.Assign(NS_LITERAL_STRING("topleft"));
    aAlign.Assign(NS_LITERAL_STRING("topleft"));
  }
  else if (position.Equals(NS_LITERAL_STRING("after_pointer")))
    aY += 21;
}

//
// LaunchPopup
//
// Given the element on which the event was triggered and the mouse locations in
// Client and widget coordinates, popup a new window showing the appropriate 
// content.
//
// This looks for an attribute on |aElement| of the appropriate popup type 
// (popup, context) and uses that attribute's value as an ID for
// the popup content in the document.
//
nsresult
XULPopupListenerImpl::LaunchPopup(PRInt32 aClientX, PRInt32 aClientY)
{
  nsresult rv = NS_OK;

  nsAutoString type(NS_LITERAL_STRING("popup"));
  if ( popupType == eXULPopupType_context ) {
    type.Assign(NS_LITERAL_STRING("context"));
    
    // position the menu two pixels down and to the right from the current
    // mouse position. This makes it easier to dismiss the menu by just
    // clicking.
    aClientX += 2;
    aClientY += 2;
  }

  nsAutoString identifier;
  mElement->GetAttribute(type, identifier);

  if (identifier.IsEmpty()) {
    if (type.EqualsIgnoreCase("popup"))
      mElement->GetAttribute(NS_LITERAL_STRING("menu"), identifier);
    else if (type.EqualsIgnoreCase("context"))
      mElement->GetAttribute(NS_LITERAL_STRING("contextmenu"), identifier);
    if (identifier.IsEmpty())
      return rv;
  }

  // Try to find the popup content and the document.
  nsCOMPtr<nsIDocument> document;
  nsCOMPtr<nsIContent> content = do_QueryInterface(mElement);
  if (NS_FAILED(rv = content->GetDocument(*getter_AddRefs(document)))) {
    NS_ERROR("Unable to retrieve the document.");
    return rv;
  }

  // Turn the document into a XUL document so we can use getElementById
  nsCOMPtr<nsIDOMXULDocument> xulDocument = do_QueryInterface(document);
  if (xulDocument == nsnull) {
    NS_ERROR("Popup attached to an element that isn't in XUL!");
    return NS_ERROR_FAILURE;
  }

  // Handle the _child case for popups and context menus
  nsCOMPtr<nsIDOMElement> popupContent;

  if (identifier == NS_LITERAL_STRING("_child")) {
    nsCOMPtr<nsIContent> popup;
    
    nsIAtom* tag = nsXULAtoms::menupopup;

    GetImmediateChild(content, tag, getter_AddRefs(popup));
    if (popup)
      popupContent = do_QueryInterface(popup);
    else {
      nsCOMPtr<nsIDOMDocumentXBL> nsDoc(do_QueryInterface(xulDocument));
      nsCOMPtr<nsIDOMNodeList> list;
      nsDoc->GetAnonymousNodes(mElement, getter_AddRefs(list));
      if (list) {
        PRUint32 ctr,listLength;
        nsCOMPtr<nsIDOMNode> node;
        list->GetLength(&listLength);
        for (ctr = 0; ctr < listLength; ctr++) {
          list->Item(ctr, getter_AddRefs(node));
          nsCOMPtr<nsIContent> childContent(do_QueryInterface(node));
          nsCOMPtr<nsIAtom> childTag;
          childContent->GetTag(*getter_AddRefs(childTag));
          if (childTag.get() == tag) {
            popupContent = do_QueryInterface(childContent);
            break;
          }
        }
      }
    }
  }
  else if (NS_FAILED(rv = xulDocument->GetElementById(identifier, getter_AddRefs(popupContent)))) {
    // Use getElementById to obtain the popup content and gracefully fail if 
    // we didn't find any popup content in the document. 
    NS_ERROR("GetElementById had some kind of spasm.");
    return rv;
  }
  if ( !popupContent )
    return NS_OK;

  // We have some popup content. Obtain our window.
  nsCOMPtr<nsIScriptContext> context;
  nsCOMPtr<nsIScriptGlobalObject> global;
  document->GetScriptGlobalObject(getter_AddRefs(global));
  if (global) {
    if ((NS_OK == global->GetContext(getter_AddRefs(context))) && context) {
      // Get the DOM window
      nsCOMPtr<nsIDOMWindowInternal> domWindow = do_QueryInterface(global);
      if (domWindow != nsnull) {
        // Find out if we're anchored.
        mPopupContent = popupContent.get();

        nsAutoString anchorAlignment;
        mPopupContent->GetAttribute(NS_LITERAL_STRING("popupanchor"), anchorAlignment);

        nsAutoString popupAlignment;
        mPopupContent->GetAttribute(NS_LITERAL_STRING("popupalign"), popupAlignment);

        PRInt32 xPos = aClientX, yPos = aClientY;
        
        ConvertPosition(mPopupContent, anchorAlignment, popupAlignment, yPos);
        if (!anchorAlignment.IsEmpty() && !popupAlignment.IsEmpty())
          xPos = yPos = -1;

        nsCOMPtr<nsIBoxObject> popupBox;
        nsCOMPtr<nsIDOMXULElement> xulPopupElt(do_QueryInterface(mPopupContent));
        xulPopupElt->GetBoxObject(getter_AddRefs(popupBox));
        nsCOMPtr<nsIPopupBoxObject> popupBoxObject(do_QueryInterface(popupBox));
        if (popupBoxObject)
          popupBoxObject->ShowPopup(mElement, mPopupContent, xPos, yPos, 
                                    type.get(), anchorAlignment.get(), 
                                    popupAlignment.get());
      }
    }
  }

  return NS_OK;
}

////////////////////////////////////////////////////////////////
nsresult
NS_NewXULPopupListener(nsIXULPopupListener** pop)
{
    XULPopupListenerImpl* popup = new XULPopupListenerImpl();
    if (!popup)
      return NS_ERROR_OUT_OF_MEMORY;
    
    NS_ADDREF(popup);
    *pop = popup;
    return NS_OK;
}
