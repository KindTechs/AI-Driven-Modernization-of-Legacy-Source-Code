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
 *   David W. Hyatt (hyatt@netscape.com) (Original Author)
 *   Joe Hewitt (hewitt@netscape.com)
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
#include "nsIListBoxObject.h"
#include "nsBoxObject.h"
#include "nsIFrame.h"
#include "nsIDocument.h"
#include "nsIBindingManager.h"
#include "nsIDOMElement.h"
#include "nsIDOMNodeList.h"
#include "nsXULAtoms.h"

class nsListBoxObject : public nsIListBoxObject, public nsBoxObject
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSILISTBOXOBJECT

  nsListBoxObject();
  virtual ~nsListBoxObject();

  nsIListBoxObject* GetListBoxBody();
  
protected:
};

NS_IMPL_ADDREF(nsListBoxObject)
NS_IMPL_RELEASE(nsListBoxObject)

NS_IMETHODIMP 
nsListBoxObject::QueryInterface(REFNSIID iid, void** aResult)
{
  if (!aResult)
    return NS_ERROR_NULL_POINTER;
  
  if (iid.Equals(NS_GET_IID(nsIListBoxObject))) {
    *aResult = (nsIListBoxObject*)this;
    NS_ADDREF(this);
    return NS_OK;
  }

  return nsBoxObject::QueryInterface(iid, aResult);
}
  
nsListBoxObject::nsListBoxObject()
{
  NS_INIT_ISUPPORTS();
}

nsListBoxObject::~nsListBoxObject()
{
}


//////////////////////////////////////////////////////////////////////////
//// nsIListBoxObject

NS_IMETHODIMP
nsListBoxObject::GetListboxBody(nsIListBoxObject * *aListboxBody)
{
  *aListboxBody = GetListBoxBody();
  NS_IF_ADDREF(*aListboxBody);
  return NS_OK;
}

NS_IMETHODIMP
nsListBoxObject::GetRowCount(PRInt32 *aResult)
{
  nsIListBoxObject* body = GetListBoxBody();
  if (body)
    return body->GetRowCount(aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsListBoxObject::GetNumberOfVisibleRows(PRInt32 *aResult)
{
  nsIListBoxObject* body = GetListBoxBody();
  if (body)
    return body->GetNumberOfVisibleRows(aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsListBoxObject::GetIndexOfFirstVisibleRow(PRInt32 *aResult)
{
  nsIListBoxObject* body = GetListBoxBody();
  if (body)
    return body->GetIndexOfFirstVisibleRow(aResult);
  return NS_OK;
}

NS_IMETHODIMP nsListBoxObject::EnsureIndexIsVisible(PRInt32 aRowIndex)
{
  nsIListBoxObject* body = GetListBoxBody();
  if (body)
    return body->EnsureIndexIsVisible(aRowIndex);
  return NS_OK;
}

NS_IMETHODIMP
nsListBoxObject::ScrollToIndex(PRInt32 aRowIndex)
{
  nsIListBoxObject* body = GetListBoxBody();
  if (body)
    return body->ScrollToIndex(aRowIndex);
  return NS_OK;
}

NS_IMETHODIMP
nsListBoxObject::ScrollByLines(PRInt32 aNumLines)
{
  nsIListBoxObject* body = GetListBoxBody();
  if (body)
    return body->ScrollByLines(aNumLines);
  return NS_OK;
}

NS_IMETHODIMP
nsListBoxObject::GetItemAtIndex(PRInt32 index, nsIDOMElement **_retval)
{
  nsIListBoxObject* body = GetListBoxBody();
  if (body)
    return body->GetItemAtIndex(index, _retval);
  return NS_OK;
}

NS_IMETHODIMP
nsListBoxObject::GetIndexOfItem(nsIDOMElement* aElement, PRInt32 *aResult)
{
  *aResult = 0;

  nsIListBoxObject* body = GetListBoxBody();
  if (body)
    return body->GetIndexOfItem(aElement, aResult);
  return NS_OK;
}

//////////////////////

static void
FindBodyContent(nsIContent* aParent, nsIContent** aResult)
{
  nsCOMPtr<nsIAtom> tag;
  aParent->GetTag(*getter_AddRefs(tag));
  if (tag.get() == nsXULAtoms::listboxbody) {
    *aResult = aParent;
    NS_IF_ADDREF(*aResult);
  }
  else {
    nsCOMPtr<nsIDocument> doc;
    aParent->GetDocument(*getter_AddRefs(doc));
    nsCOMPtr<nsIBindingManager> bindingManager;
    doc->GetBindingManager(getter_AddRefs(bindingManager));
    nsCOMPtr<nsIDOMNodeList> kids;
    bindingManager->GetXBLChildNodesFor(aParent, getter_AddRefs(kids));
    if (!kids) return;

    PRUint32 count;
    kids->GetLength(&count);
    // start from the end, cuz we're smart and we know the listboxbody is probably at the end
    for (PRUint32 i = count-1; i >= 0; --i) {
      nsCOMPtr<nsIDOMNode> childNode;
      kids->Item(i, getter_AddRefs(childNode));
      nsCOMPtr<nsIContent> childContent(do_QueryInterface(childNode));
      FindBodyContent(childContent, aResult);
      if (*aResult)
        break;
    }
  }
}

nsIListBoxObject*
nsListBoxObject::GetListBoxBody()
{
  nsAutoString listboxbody;
  listboxbody.AssignWithConversion("listboxbody");

  nsCOMPtr<nsISupports> supp;
  GetPropertyAsSupports(listboxbody.get(), getter_AddRefs(supp));

  if (supp) {
    nsCOMPtr<nsIListBoxObject> body(do_QueryInterface(supp));
    return body;
  }

  nsIFrame* frame = GetFrame();
  if (!frame)
    return nsnull;

  // Iterate over our content model children looking for the body.
  nsCOMPtr<nsIContent> startContent;
  frame->GetContent(getter_AddRefs(startContent));

  nsCOMPtr<nsIContent> content;
  FindBodyContent(startContent, getter_AddRefs(content));

  // this frame will be a nsGFXScrollFrame
  mPresShell->GetPrimaryFrameFor(content, &frame);
  if (!frame)
     return nsnull;

  // this frame will be a nsListBoxScrollPortFrame
  nsIFrame* scrollPort = nsnull;
  frame->FirstChild(nsnull, nsnull, &scrollPort);
  if (!scrollPort)
     return nsnull;

  // this frame will be the one we want
  nsIFrame* yeahBaby = nsnull;
  scrollPort->FirstChild(nsnull, nsnull, &yeahBaby);
  if (!yeahBaby)
     return nsnull;

  // It's a frame. Refcounts are irrelevant.
  nsCOMPtr<nsIListBoxObject> body;
  yeahBaby->QueryInterface(NS_GET_IID(nsIListBoxObject), getter_AddRefs(body));
  SetPropertyAsSupports(listboxbody.get(), body);
  return body;
}

// Creation Routine ///////////////////////////////////////////////////////////////////////

nsresult
NS_NewListBoxObject(nsIBoxObject** aResult)
{
  *aResult = new nsListBoxObject;
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}
