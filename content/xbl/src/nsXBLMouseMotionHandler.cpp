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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Original Author: David W. Hyatt (hyatt@netscape.com)
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

#include "nsCOMPtr.h"
#include "nsIXBLPrototypeHandler.h"
#include "nsXBLMouseMotionHandler.h"
#include "nsIContent.h"
#include "nsIAtom.h"
#include "nsIDOMMouseEvent.h"
#include "nsINameSpaceManager.h"
#include "nsIScriptContext.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIJSEventListener.h"
#include "nsIController.h"
#include "nsIControllers.h"
#include "nsIDOMXULElement.h"
#include "nsIDOMNSHTMLTextAreaElement.h"
#include "nsIDOMNSHTMLInputElement.h"
#include "nsIDOMText.h"
#include "nsIEventListenerManager.h"
#include "nsIDOMEventReceiver.h"
#include "nsXBLBinding.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIDOMWindowInternal.h"
#include "nsIPref.h"
#include "nsIServiceManager.h"
#include "nsIURI.h"
#include "nsXPIDLString.h"

PRUint32 nsXBLMouseMotionHandler::gRefCnt = 0;
nsIAtom* nsXBLMouseMotionHandler::kMouseMoveAtom = nsnull;

nsXBLMouseMotionHandler::nsXBLMouseMotionHandler(nsIDOMEventReceiver* aReceiver, nsIXBLPrototypeHandler* aHandler)
:nsXBLEventHandler(aReceiver,aHandler)
{
  gRefCnt++;
  if (gRefCnt == 1) {
    kMouseMoveAtom = NS_NewAtom("mousemove");
  }
}

nsXBLMouseMotionHandler::~nsXBLMouseMotionHandler()
{
  gRefCnt--;
  if (gRefCnt == 0) {
    NS_RELEASE(kMouseMoveAtom);
  }
}

NS_IMPL_ISUPPORTS_INHERITED1(nsXBLMouseMotionHandler, nsXBLEventHandler, nsIDOMMouseMotionListener)

static inline nsresult DoMouse(nsIAtom* aEventType, nsIXBLPrototypeHandler* aHandler, nsIDOMEvent* aMouseEvent,
                               nsIDOMEventReceiver* aReceiver)
{
  if (!aHandler)
    return NS_OK;

  PRBool matched = PR_FALSE;
  nsCOMPtr<nsIDOMMouseEvent> mouse(do_QueryInterface(aMouseEvent));
  aHandler->MouseEventMatched(aEventType, mouse, &matched);

  if (matched)
    aHandler->ExecuteHandler(aReceiver, aMouseEvent);
  
  return NS_OK;
}

nsresult nsXBLMouseMotionHandler::MouseMove(nsIDOMEvent* aMouseEvent)
{
  return DoMouse(kMouseMoveAtom, mProtoHandler, aMouseEvent, mEventReceiver);
}


///////////////////////////////////////////////////////////////////////////////////

nsresult
NS_NewXBLMouseMotionHandler(nsIDOMEventReceiver* aRec, nsIXBLPrototypeHandler* aHandler, 
                            nsXBLMouseMotionHandler** aResult)
{
  *aResult = new nsXBLMouseMotionHandler(aRec, aHandler);
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}
