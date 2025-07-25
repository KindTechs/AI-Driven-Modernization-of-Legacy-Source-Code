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

#include "nsDOMDocumentType.h"
#include "nsDOMAttributeMap.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsLayoutAtoms.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"


nsresult
NS_NewDOMDocumentType(nsIDOMDocumentType** aDocType,
                      const nsAString& aName,
                      nsIDOMNamedNodeMap *aEntities,
                      nsIDOMNamedNodeMap *aNotations,
                      const nsAString& aPublicId,
                      const nsAString& aSystemId,
                      const nsAString& aInternalSubset)
{
  NS_ENSURE_ARG_POINTER(aDocType);

  *aDocType = new nsDOMDocumentType(aName, aEntities, aNotations, aPublicId,
                                    aSystemId, aInternalSubset);
  if (!*aDocType) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(*aDocType);

  return NS_OK;
}

nsDOMDocumentType::nsDOMDocumentType(const nsAString& aName,
                                     nsIDOMNamedNodeMap *aEntities,
                                     nsIDOMNamedNodeMap *aNotations,
                                     const nsAString& aPublicId,
                                     const nsAString& aSystemId,
                                     const nsAString& aInternalSubset) :
  mName(aName),
  mPublicId(aPublicId),
  mSystemId(aSystemId),
  mInternalSubset(aInternalSubset)
{
  NS_INIT_REFCNT();

  mEntities = aEntities;
  mNotations = aNotations;

  NS_IF_ADDREF(mEntities);
  NS_IF_ADDREF(mNotations);
}

nsDOMDocumentType::~nsDOMDocumentType()
{
  NS_IF_RELEASE(mEntities);
  NS_IF_RELEASE(mNotations);
}


// QueryInterface implementation for nsDOMDocumentType
NS_INTERFACE_MAP_BEGIN(nsDOMDocumentType)
  NS_INTERFACE_MAP_ENTRY(nsIContent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDocumentType)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIContent)
  NS_INTERFACE_MAP_ENTRY_TEAROFF(nsIDOM3Node, nsNode3Tearoff(this))
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(DocumentType)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(nsDOMDocumentType)
NS_IMPL_RELEASE(nsDOMDocumentType)


NS_IMETHODIMP    
nsDOMDocumentType::GetName(nsAString& aName)
{
  aName=mName;

  return NS_OK;
}

NS_IMETHODIMP    
nsDOMDocumentType::GetEntities(nsIDOMNamedNodeMap** aEntities)
{
  NS_ENSURE_ARG_POINTER(aEntities);

  *aEntities = mEntities;

  NS_IF_ADDREF(*aEntities);

  return NS_OK;
}

NS_IMETHODIMP    
nsDOMDocumentType::GetNotations(nsIDOMNamedNodeMap** aNotations)
{
  NS_ENSURE_ARG_POINTER(aNotations);

  *aNotations = mNotations;

  NS_IF_ADDREF(*aNotations);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMDocumentType::GetPublicId(nsAString& aPublicId)
{
  aPublicId = mPublicId;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMDocumentType::GetSystemId(nsAString& aSystemId)
{
  aSystemId = mSystemId;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMDocumentType::GetInternalSubset(nsAString& aInternalSubset)
{
  // XXX: null string
  aInternalSubset = mInternalSubset;

  return NS_OK;
}

NS_IMETHODIMP 
nsDOMDocumentType::GetTag(nsIAtom*& aResult) const
{
  aResult = NS_NewAtom(mName.get());

  return NS_OK;
}

NS_IMETHODIMP
nsDOMDocumentType::GetNodeName(nsAString& aNodeName)
{
  aNodeName=mName;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMDocumentType::GetNodeType(PRUint16* aNodeType)
{
  *aNodeType = nsIDOMNode::DOCUMENT_TYPE_NODE;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMDocumentType::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  nsDOMDocumentType* it = new nsDOMDocumentType(mName,
                                                mEntities,
                                                mNotations,
                                                mPublicId,
                                                mSystemId,
                                                mInternalSubset);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(NS_GET_IID(nsIDOMNode), (void**) aReturn);
}

#ifdef DEBUG
NS_IMETHODIMP
nsDOMDocumentType::SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const
{
  NS_ENSURE_ARG_POINTER(aResult);

  PRUint32 sum;
  nsGenericDOMDataNode::SizeOf(aSizer, &sum);
  PRUint32 ssize;
  mName.SizeOf(aSizer, &ssize);
  sum = sum - sizeof(mName) + ssize;
  if (mEntities) {
    PRBool recorded;
    aSizer->RecordObject((void*) mEntities, &recorded);
    if (!recorded) {
      PRUint32 size;
      nsDOMAttributeMap::SizeOfNamedNodeMap(mEntities, aSizer, &size);
      aSizer->AddSize(nsLayoutAtoms::xml_document_entities, size);
    }
  }
  if (mNotations) {
    PRBool recorded;
    aSizer->RecordObject((void*) mNotations, &recorded);
    if (!recorded) {
      PRUint32 size;
      nsDOMAttributeMap::SizeOfNamedNodeMap(mNotations, aSizer, &size);
      aSizer->AddSize(nsLayoutAtoms::xml_document_notations, size);
    }
  }
  return NS_OK;
}
#endif

