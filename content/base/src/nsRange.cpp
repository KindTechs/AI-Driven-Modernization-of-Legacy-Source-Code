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
 * nsRange.cpp: Implementation of the nsIDOMRange object.
 */

#include "nscore.h"
#include "nsRange.h"

#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsIDOMNode.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNSDocument.h"
#include "nsIDOMDocumentFragment.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsVoidArray.h"
#include "nsIDOMText.h"
#include "nsDOMError.h"
#include "nsIContentIterator.h"
#include "nsIDOMNodeList.h"
#include "nsIParser.h"
#include "nsIComponentManager.h"
#include "nsParserCIID.h"
#include "nsIHTMLFragmentContentSink.h"
#include "nsIEnumerator.h"
#include "nsScriptSecurityManager.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptContext.h"
#include "nsIHTMLDocument.h"
#include "nsCRT.h"

#include "nsIJSContextStack.h"
// XXX Temporary inclusion to deal with fragment parsing
#include "nsHTMLParts.h"

#include "nsContentUtils.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_CID(kCParserCID, NS_PARSER_CID);

PRMonitor*   nsRange::mMonitor = nsnull;
nsVoidArray* nsRange::mStartAncestors = nsnull;      
nsVoidArray* nsRange::mEndAncestors = nsnull;        
nsVoidArray* nsRange::mStartAncestorOffsets = nsnull; 
nsVoidArray* nsRange::mEndAncestorOffsets = nsnull;  

nsresult NS_NewContentIterator(nsIContentIterator** aInstancePtrResult);
nsresult NS_NewContentSubtreeIterator(nsIContentIterator** aInstancePtrResult);



#ifdef XP_MAC
#pragma mark -
#pragma mark  utility functions (some exposed through nsRangeUtils, below
#pragma mark -
#endif


/******************************************************
 * stack based utilty class for managing monitor
 ******************************************************/

class nsAutoRangeLock
{
  public:
    nsAutoRangeLock()  { nsRange::Lock(); }
    ~nsAutoRangeLock() { nsRange::Unlock(); }
};


// Returns -1 if point1 < point2, 1, if point1 > point2,
// 0 if error or if point1 == point2. 
PRInt32 ComparePoints(nsIDOMNode* aParent1, PRInt32 aOffset1,
                      nsIDOMNode* aParent2, PRInt32 aOffset2)
{
  if (aParent1 == aParent2 && aOffset1 == aOffset2)
    return 0;
  nsIDOMRange* range;
  if (NS_FAILED(NS_NewRange(&range)))
    return 0;
  nsresult res = range->SetStart(aParent1, aOffset1);
  if (NS_FAILED(res))
    return 0;
  res = range->SetEnd(aParent2, aOffset2);
  NS_RELEASE(range);
  if (NS_SUCCEEDED(res))
    return -1;
  else
    return 1;
}

// Utility routine to detect if a content node intersects a range
PRBool IsNodeIntersectsRange(nsIContent* aNode, nsIDOMRange* aRange)
{
  // create a pair of dom points that expresses location of node:
  //     NODE(start), NODE(end)
  // Let incoming range be:
  //    {RANGE(start), RANGE(end)}
  // if (RANGE(start) < NODE(end))  and (RANGE(end) > NODE(start))
  // then the Node intersect the Range.
  
  if (!aNode) return PR_FALSE;
  nsCOMPtr<nsIDOMNode> parent, rangeStartParent, rangeEndParent;
  PRInt32 nodeStart, nodeEnd, rangeStartOffset, rangeEndOffset; 
  
  // gather up the dom point info
  if (!GetNodeBracketPoints(aNode, address_of(parent), &nodeStart, &nodeEnd))
    return PR_FALSE;
  
  if (NS_FAILED(aRange->GetStartContainer(getter_AddRefs(rangeStartParent))))
    return PR_FALSE;

  if (NS_FAILED(aRange->GetStartOffset(&rangeStartOffset)))
    return PR_FALSE;

  if (NS_FAILED(aRange->GetEndContainer(getter_AddRefs(rangeEndParent))))
    return PR_FALSE;

  if (NS_FAILED(aRange->GetEndOffset(&rangeEndOffset)))
    return PR_FALSE;

  // is RANGE(start) < NODE(end) ?
  PRInt32 comp = ComparePoints(rangeStartParent, rangeStartOffset, parent, nodeEnd);
  if (comp >= 0)
    return PR_FALSE; // range start is after node end
    
  // is RANGE(end) > NODE(start) ?
  comp = ComparePoints(rangeEndParent, rangeEndOffset, parent, nodeStart);
  if (comp <= 0)
    return PR_FALSE; // range end is before node start
    
  // if we got here then the node intersects the range
  return PR_TRUE;
}


// Utility routine to detect if a content node is completely contained in a range
// If outNodeBefore is returned true, then the node starts before the range does.
// If outNodeAfter is returned true, then the node ends after the range does.
// Note that both of the above might be true.
// If neither are true, the node is contained inside of the range.
// XXX - callers responsibility to ensure node in same doc as range! 
nsresult CompareNodeToRange(nsIContent* aNode, 
                        nsIDOMRange* aRange,
                        PRBool *outNodeBefore,
                        PRBool *outNodeAfter)
{
  // create a pair of dom points that expresses location of node:
  //     NODE(start), NODE(end)
  // Let incoming range be:
  //    {RANGE(start), RANGE(end)}
  // if (RANGE(start) <= NODE(start))  and (RANGE(end) => NODE(end))
  // then the Node is contained (completely) by the Range.
  
  if (!aNode) 
    return NS_ERROR_NULL_POINTER;
  if (!aRange) 
    return NS_ERROR_NULL_POINTER;
  if (!outNodeBefore) 
    return NS_ERROR_NULL_POINTER;
  if (!outNodeAfter) 
    return NS_ERROR_NULL_POINTER;
  
  PRBool isPositioned;
  nsresult err = ((nsRange*)aRange)->GetIsPositioned(&isPositioned);
  // Why do I have to cast above?  Because GetIsPositioned() is
  // mysteriously missing from the nsIDOMRange interface.  dunno why.

  if (NS_FAILED(err))
    return err;
    
  if (!isPositioned) 
    return NS_ERROR_UNEXPECTED; 
  
  nsCOMPtr<nsIDOMNode> parent, rangeStartParent, rangeEndParent;
  PRInt32 nodeStart, nodeEnd, rangeStartOffset, rangeEndOffset; 
  
  // gather up the dom point info
  if (!GetNodeBracketPoints(aNode, address_of(parent), &nodeStart, &nodeEnd))
    return NS_ERROR_FAILURE;
  
  if (NS_FAILED(aRange->GetStartContainer(getter_AddRefs(rangeStartParent))))
    return NS_ERROR_FAILURE;

  if (NS_FAILED(aRange->GetStartOffset(&rangeStartOffset)))
    return NS_ERROR_FAILURE;

  if (NS_FAILED(aRange->GetEndContainer(getter_AddRefs(rangeEndParent))))
    return NS_ERROR_FAILURE;

  if (NS_FAILED(aRange->GetEndOffset(&rangeEndOffset)))
    return NS_ERROR_FAILURE;

  *outNodeBefore = PR_FALSE;
  *outNodeAfter = PR_FALSE;
  
  // is RANGE(start) <= NODE(start) ?
  PRInt32 comp = ComparePoints(rangeStartParent, rangeStartOffset, parent, nodeStart);
  if (comp > 0)
    *outNodeBefore = PR_TRUE; // range start is after node start
    
  // is RANGE(end) >= NODE(end) ?
  comp = ComparePoints(rangeEndParent, rangeEndOffset, parent, nodeEnd);
  if (comp < 0)
    *outNodeAfter = PR_TRUE; // range end is before node end
    
  return NS_OK;
}


// Utility routine to create a pair of dom points to represent 
// the start and end locations of a single node.  Return false
// if we dont' succeed.
PRBool GetNodeBracketPoints(nsIContent* aNode, 
                            nsCOMPtr<nsIDOMNode>* outParent,
                            PRInt32* outStartOffset,
                            PRInt32* outEndOffset)
{
  if (!aNode) 
    return PR_FALSE;
  if (!outParent) 
    return PR_FALSE;
  if (!outStartOffset)
    return PR_FALSE;
  if (!outEndOffset)
    return PR_FALSE;
    
  nsCOMPtr<nsIDOMNode> theDOMNode( do_QueryInterface(aNode) );
  PRInt32 indx;
  theDOMNode->GetParentNode(getter_AddRefs(*outParent));
  
  if (!(*outParent)) // special case for root node
  {
    // can't make a parent/offset pair to represent start or 
    // end of the root node, becasue it has no parent.
    // so instead represent it by (node,0) and (node,numChildren)
    *outParent = do_QueryInterface(aNode);
    nsCOMPtr<nsIContent> cN(do_QueryInterface(*outParent));
    if (!cN)
      return PR_FALSE;
    cN->ChildCount(indx);
    if (!indx) 
      return PR_FALSE;
    *outStartOffset = 0;
    *outEndOffset = indx;
  }
  else
  {
    *outStartOffset = nsRange::IndexOf(theDOMNode);
    *outEndOffset = *outStartOffset+1;
  }
  return PR_TRUE;
}



#ifdef XP_MAC
#pragma mark -
#pragma mark  class nsRangeUtils
#pragma mark -
#endif


/******************************************************
 * non members
 ******************************************************/

nsresult
NS_NewRangeUtils(nsIRangeUtils** aResult)
{
  NS_PRECONDITION(aResult != nsnull, "null ptr");
  if (! aResult)
    return NS_ERROR_NULL_POINTER;

  nsRangeUtils* rangeUtil = new nsRangeUtils();
  if (rangeUtil)
    return rangeUtil->QueryInterface(NS_GET_IID(nsIRangeUtils), (void**) aResult);
  return NS_ERROR_OUT_OF_MEMORY;
}

/******************************************************
 * constructor/destructor
 ******************************************************/

nsRangeUtils::nsRangeUtils()
{
  NS_INIT_REFCNT();
} 

nsRangeUtils::~nsRangeUtils() 
{
} 

/******************************************************
 * nsISupports
 ******************************************************/
 
NS_IMPL_ADDREF(nsRangeUtils)
NS_IMPL_RELEASE(nsRangeUtils)

nsresult nsRangeUtils::QueryInterface(const nsIID& aIID,
                                     void** aInstancePtrResult)
{
  NS_PRECONDITION(aInstancePtrResult, "null pointer");
  if (!aInstancePtrResult) 
  {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kISupportsIID)) 
  {
    *aInstancePtrResult = (void*)(nsISupports*)(nsIRangeUtils *)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIRangeUtils))) 
  {
    *aInstancePtrResult = (void*)(nsIRangeUtils*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}


/******************************************************
 * nsIRangeUtils methods
 ******************************************************/
 
NS_IMETHODIMP_(PRInt32) 
nsRangeUtils::ComparePoints(nsIDOMNode* aParent1, PRInt32 aOffset1,
                                     nsIDOMNode* aParent2, PRInt32 aOffset2)
{
  return ::ComparePoints(aParent1, aOffset1, aParent2, aOffset2);
}

NS_IMETHODIMP_(PRBool) 
nsRangeUtils::IsNodeIntersectsRange(nsIContent* aNode, nsIDOMRange* aRange)
{
  return ::IsNodeIntersectsRange( aNode,  aRange);
}

NS_IMETHODIMP
nsRangeUtils::CompareNodeToRange(nsIContent* aNode, 
                                nsIDOMRange* aRange,
                                PRBool *outNodeBefore,
                                PRBool *outNodeAfter)
{
  return ::CompareNodeToRange(aNode, aRange, outNodeBefore, outNodeAfter);
}

#ifdef XP_MAC
#pragma mark -
#pragma mark  class nsRange
#pragma mark -
#endif


/******************************************************
 * non members
 ******************************************************/

nsresult
NS_NewRange(nsIDOMRange** aResult)
{
  NS_PRECONDITION(aResult != nsnull, "null ptr");
  if (! aResult)
    return NS_ERROR_NULL_POINTER;

  nsRange * range = new nsRange();
  if (range)
    return range->QueryInterface(NS_GET_IID(nsIDOMRange), (void**) aResult);
  return NS_ERROR_OUT_OF_MEMORY;
}
/******************************************************
 * constructor/destructor
 ******************************************************/

nsRange::nsRange() :
  mIsPositioned(PR_FALSE),
  mIsDetached(PR_FALSE),
  mStartOffset(0),
  mEndOffset(0),
  mStartParent(),
  mEndParent()
{
  NS_INIT_REFCNT();
} 

nsRange::~nsRange() 
{
  DoSetRange(nsCOMPtr<nsIDOMNode>(),0,nsCOMPtr<nsIDOMNode>(),0); 
  // we want the side effects (releases and list removals)
  // note that "nsCOMPtr<nsIDOMmNode>()" is the moral equivalent of null
} 

// for layout module destructor
void nsRange::Shutdown()
{
  if (mMonitor) {
    PR_DestroyMonitor(mMonitor);
    mMonitor = nsnull;
  }

  delete mStartAncestors;      
  mStartAncestors = nsnull;      

  delete mEndAncestors;        
  mEndAncestors = nsnull;        

  delete mStartAncestorOffsets; 
  mStartAncestorOffsets = nsnull; 

  delete mEndAncestorOffsets;  
  mEndAncestorOffsets = nsnull;  
}

/******************************************************
 * nsISupports
 ******************************************************/


// QueryInterface implementation for nsRange
NS_INTERFACE_MAP_BEGIN(nsRange)
  NS_INTERFACE_MAP_ENTRY(nsIDOMRange)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNSRange)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMRange)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(Range)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(nsRange)
NS_IMPL_RELEASE(nsRange)


/********************************************************
 * Utilities for comparing points: API from nsIDOMNSRange
 ********************************************************/
NS_IMETHODIMP
nsRange::IsPointInRange(nsIDOMNode* aParent, PRInt32 aOffset, PRBool* aResult)
{
  PRInt16 compareResult = 0;
  nsresult res;
  res = ComparePoint(aParent, aOffset, &compareResult);
  if (compareResult) 
    *aResult = PR_FALSE;
  else 
    *aResult = PR_TRUE;
  return res;
}
  
// returns -1 if point is before range, 0 if point is in range,
// 1 if point is after range.
NS_IMETHODIMP
nsRange::ComparePoint(nsIDOMNode* aParent, PRInt32 aOffset, PRInt16* aResult)
{
  // check arguments
  if (!aResult) 
    return NS_ERROR_NULL_POINTER;
  
  // no trivial cases please
  if (!aParent) 
    return NS_ERROR_NULL_POINTER;
  
  // our range is in a good state?
  if (!mIsPositioned) 
    return NS_ERROR_NOT_INITIALIZED;
  
  // check common case first
  if ((aParent == mStartParent.get()) && (aParent == mEndParent.get()))
  {
    if (aOffset<mStartOffset)
    {
      *aResult = -1;
      return NS_OK;
    }
    if (aOffset>mEndOffset)
    {
      *aResult = 1;
      return NS_OK;
    }
    *aResult = 0;
    return NS_OK;
  }
  
  // more common cases
  if ((aParent == mStartParent.get()) && (aOffset == mStartOffset)) 
  {
    *aResult = 0;
    return NS_OK;
  }
  if ((aParent == mEndParent.get()) && (aOffset == mEndOffset)) 
  {
    *aResult = 0;
    return NS_OK;
  }
  
  // ok, do it the hard way
  if (IsIncreasing(aParent,aOffset,mStartParent,mStartOffset)) 
    *aResult = -1;
  else if (IsIncreasing(mEndParent,mEndOffset,aParent,aOffset)) 
    *aResult = 1;
  else 
    *aResult = 0;
  
  return NS_OK;
}
  
NS_IMETHODIMP
nsRange::IntersectsNode(nsIDOMNode* aNode, PRBool* aReturn)
{
  if (!aReturn)
    return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsIContent> content (do_QueryInterface(aNode));
  if (!content)
  {
    *aReturn = 0;
    return NS_ERROR_UNEXPECTED;
  }
  *aReturn = IsNodeIntersectsRange(content, this);
  return NS_OK;
}

// HOW does the node intersect the range?
NS_IMETHODIMP
nsRange::CompareNode(nsIDOMNode* aNode, PRUint16* aReturn)
{
  if (!aReturn)
    return NS_ERROR_NULL_POINTER;
  *aReturn = 0;
  PRBool nodeBefore, nodeAfter;
  nsCOMPtr<nsIContent> content (do_QueryInterface(aNode));
  if (!content)
    return NS_ERROR_UNEXPECTED;

  nsresult res = CompareNodeToRange(content, this, &nodeBefore, &nodeAfter);
  if (NS_FAILED(res))
    return res;

  // nodeBefore -> range start after node start, i.e. node starts before range.
  // nodeAfter -> range end before node end, i.e. node ends after range.
  // But I know that I get nodeBefore && !nodeAfter when the node is
  // entirely inside the selection!  This doesn't make sense.
  if (nodeBefore && !nodeAfter)
    *aReturn = nsIDOMNSRange::NODE_BEFORE;  // May or may not intersect
  else if (!nodeBefore && nodeAfter)
    *aReturn = nsIDOMNSRange::NODE_AFTER;   // May or may not intersect
  else if (nodeBefore && nodeAfter)
    *aReturn = nsIDOMNSRange::NODE_BEFORE_AND_AFTER;  // definitely intersects
  else
    *aReturn = nsIDOMNSRange::NODE_INSIDE;            // definitely intersects

  return NS_OK;
}



nsresult
nsRange::NSDetach()
{
  return DoSetRange(nsnull,0,nsnull,0);
}



/******************************************************
 * Private helper routines
 ******************************************************/

PRBool nsRange::InSameDoc(nsIDOMNode* aNode1, nsIDOMNode* aNode2)
{
  nsCOMPtr<nsIContent> cN1;
  nsCOMPtr<nsIContent> cN2;
  nsCOMPtr<nsIDocument> doc1;
  nsCOMPtr<nsIDocument> doc2;
  
  nsresult res = GetContentFromDOMNode(aNode1, address_of(cN1));
  if (NS_FAILED(res)) 
    return PR_FALSE;
  res = GetContentFromDOMNode(aNode2, address_of(cN2));
  if (NS_FAILED(res)) 
    return PR_FALSE;
  res = cN1->GetDocument(*getter_AddRefs(doc1));
  if (NS_FAILED(res)) 
    return PR_FALSE;
  res = cN2->GetDocument(*getter_AddRefs(doc2));
  if (NS_FAILED(res)) 
    return PR_FALSE;
  
  // Now compare the two documents: is direct comparison safe?
  if (doc1 == doc2) 
    return PR_TRUE;

  return PR_FALSE;
}


nsresult nsRange::AddToListOf(nsIDOMNode* aNode)
{
  if (!aNode) 
    return NS_ERROR_NULL_POINTER;
  
  nsCOMPtr<nsIContent> cN;

  nsresult res = aNode->QueryInterface(NS_GET_IID(nsIContent), getter_AddRefs(cN));
  if (NS_FAILED(res)) 
    return res;

  res = cN->RangeAdd(NS_STATIC_CAST(nsIDOMRange*,this));
  return res;
}
  

nsresult nsRange::RemoveFromListOf(nsIDOMNode* aNode)
{
  if (!aNode) 
    return NS_ERROR_NULL_POINTER;
  
  nsCOMPtr<nsIContent> cN;

  nsresult res = aNode->QueryInterface(NS_GET_IID(nsIContent), getter_AddRefs(cN));
  if (NS_FAILED(res)) 
    return res;

  res = cN->RangeRemove(NS_STATIC_CAST(nsIDOMRange*,this));
  return res;
}

// It's important that all setting of the range start/end points 
// go through this function, which will do all the right voodoo
// for content notification of range ownership.  
// Calling DoSetRange with either parent argument null will collapse
// the range to have both endpoints point to the other node
nsresult nsRange::DoSetRange(nsIDOMNode* aStartN, PRInt32 aStartOffset,
                             nsIDOMNode* aEndN, PRInt32 aEndOffset)
{
  //if only one endpoint is null, set it to the other one
  if (aStartN && !aEndN) 
  {
    aEndN = aStartN;
    aEndOffset = aStartOffset;
  }
  if (aEndN && !aStartN)
  {
    aStartN = aEndN;
    aStartOffset = aEndOffset;
  }
  
  if (mStartParent && (mStartParent.get() != aStartN) && (mStartParent.get() != aEndN))
  {
    // if old start parent no longer involved, remove range from that
    // node's range list.
    RemoveFromListOf(mStartParent);
  }
  if (mEndParent && (mEndParent.get() != aStartN) && (mEndParent.get() != aEndN))
  {
    // if old end parent no longer involved, remove range from that
    // node's range list.
    RemoveFromListOf(mEndParent);
  }
 
 
  if (mStartParent.get() != aStartN)
  {
    mStartParent = do_QueryInterface(aStartN);
    if (mStartParent) // if it has a new start node, put it on it's list
    {
      AddToListOf(mStartParent);  // AddToList() detects duplication for us
    }
  }
  mStartOffset = aStartOffset;

  if (mEndParent.get() != aEndN)
  {
    mEndParent = do_QueryInterface(aEndN);
    if (mEndParent) // if it has a new end node, put it on it's list
    {
      AddToListOf(mEndParent);  // AddToList() detects duplication for us
    }
  }
  mEndOffset = aEndOffset;

  if (mStartParent) 
    mIsPositioned = PR_TRUE;
  else
    mIsPositioned = PR_FALSE;

  // FIX ME need to handle error cases 
  // (range lists return error, or setting only one endpoint to null)
  return NS_OK;
}


PRBool nsRange::IsIncreasing(nsIDOMNode* aStartN, PRInt32 aStartOffset,
                             nsIDOMNode* aEndN, PRInt32 aEndOffset)
{
  PRInt32 numStartAncestors = 0;
  PRInt32 numEndAncestors = 0;
  PRInt32 commonNodeStartOffset = 0;
  PRInt32 commonNodeEndOffset = 0;
  
  // no trivial cases please
  if (!aStartN || !aEndN) 
    return PR_FALSE;
  
  // check common case first
  if (aStartN==aEndN)
  {
    if (aStartOffset>aEndOffset) 
      return PR_FALSE;
    else 
      return PR_TRUE;
  }
  
  // thread safety - need locks around here to end of routine to protect use of static members
  nsAutoRangeLock lock;
  
  // lazy allocation of static arrays
  if (!mStartAncestors)
  {
    mStartAncestors = new nsAutoVoidArray();
    if (!mStartAncestors) return NS_ERROR_OUT_OF_MEMORY;
    mStartAncestorOffsets = new nsAutoVoidArray();
    if (!mStartAncestorOffsets) return NS_ERROR_OUT_OF_MEMORY;
    mEndAncestors = new nsAutoVoidArray();
    if (!mEndAncestors) return NS_ERROR_OUT_OF_MEMORY;
    mEndAncestorOffsets = new nsAutoVoidArray();
    if (!mEndAncestorOffsets) return NS_ERROR_OUT_OF_MEMORY;
  }

  // refresh ancestor data
  mStartAncestors->Clear();
  mStartAncestorOffsets->Clear();
  mEndAncestors->Clear();
  mEndAncestorOffsets->Clear();

  numStartAncestors = GetAncestorsAndOffsets(aStartN,aStartOffset,mStartAncestors,mStartAncestorOffsets);
  numEndAncestors = GetAncestorsAndOffsets(aEndN,aEndOffset,mEndAncestors,mEndAncestorOffsets);
  
  --numStartAncestors; // adjusting for 0-based counting 
  --numEndAncestors; 
  // back through the ancestors, starting from the root, until first non-matching ancestor found
  while (numStartAncestors >= 0 && numEndAncestors >= 0 &&
         mStartAncestors->ElementAt(numStartAncestors) == mEndAncestors->ElementAt(numEndAncestors))
  {
    --numStartAncestors;
    --numEndAncestors;
    // numStartAncestors will only be <0 if one endpoint's node is the
    // common ancestor of the other
  }
  // now back up one and that's the last common ancestor from the root,
  // or the first common ancestor from the leaf perspective
  numStartAncestors++;
  numEndAncestors++;
  // both indexes are now >= 0
  commonNodeStartOffset = NS_PTR_TO_INT32(mStartAncestorOffsets->ElementAt(numStartAncestors));
  commonNodeEndOffset   = NS_PTR_TO_INT32(mEndAncestorOffsets->ElementAt(numEndAncestors));
  
  if (commonNodeStartOffset > commonNodeEndOffset) 
    return PR_FALSE;
  else if (commonNodeStartOffset < commonNodeEndOffset) 
    return PR_TRUE;
  else 
  {
    // The offsets are equal.  This can happen when one endpoint parent is the common parent
    // of both endpoints.  In this case, we compare the depth of the ancestor tree to determine
    // the ordering.
    if (numStartAncestors < numEndAncestors)
      return PR_TRUE;
    else if (numStartAncestors > numEndAncestors)
      return PR_FALSE;
    else
    {
      // whoa nelly. shouldn't get here.
      NS_NOTREACHED("nsRange::IsIncreasing");
      return PR_FALSE;
    }
  }
}

PRInt32 nsRange::IndexOf(nsIDOMNode* aChildNode)
{
  nsCOMPtr<nsIDOMNode> parentNode;
  nsCOMPtr<nsIContent> contentChild;
  nsCOMPtr<nsIContent> contentParent;
  PRInt32    theIndex = nsnull;
  
  if (!aChildNode) 
    return 0;

  // get the parent node
  nsresult res = aChildNode->GetParentNode(getter_AddRefs(parentNode));
  if (NS_FAILED(res)) 
    return 0;
  
  // convert node and parent to nsIContent, so that we can find the child index
  res = parentNode->QueryInterface(NS_GET_IID(nsIContent), getter_AddRefs(contentParent));
  if (NS_FAILED(res)) 
    return 0;

  res = aChildNode->QueryInterface(NS_GET_IID(nsIContent), getter_AddRefs(contentChild));
  if (NS_FAILED(res)) 
    return 0;
  
  // finally we get the index
  res = contentParent->IndexOf(contentChild,theIndex); 
  if (NS_FAILED(res)) 
    return 0;

  return theIndex;
}

PRInt32 nsRange::FillArrayWithAncestors(nsVoidArray* aArray, nsIDOMNode* aNode)
{
  PRInt32    i=0;
  nsCOMPtr<nsIDOMNode> node(aNode);
  nsCOMPtr<nsIDOMNode> parent;
  // callers responsibility to make sure args are non-null and proper type
  
  // insert node itself
  aArray->InsertElementAt((void*)node,i);
  
  // insert all the ancestors
  // not doing all the inserts at location 0, that would make for lots of memcopys in the voidArray::InsertElementAt implementation
  node->GetParentNode(getter_AddRefs(parent));  
  while(parent)
  {
    node = parent;
    ++i;
    aArray->InsertElementAt(NS_STATIC_CAST(void*,node),i);
    node->GetParentNode(getter_AddRefs(parent));
  }
  
  return i;
}

PRInt32 nsRange::GetAncestorsAndOffsets(nsIDOMNode* aNode, PRInt32 aOffset,
                        nsVoidArray* aAncestorNodes, nsVoidArray* aAncestorOffsets)
{
  PRInt32    i=0;
  PRInt32    nodeOffset;
  nsresult   res;
  nsCOMPtr<nsIContent> contentNode;
  nsCOMPtr<nsIContent> contentParent;
  
  // callers responsibility to make sure args are non-null and proper type

  res = aNode->QueryInterface(NS_GET_IID(nsIContent),getter_AddRefs(contentNode));
  if (NS_FAILED(res)) 
  {
    NS_NOTREACHED("nsRange::GetAncestorsAndOffsets");
    return -1;  // poor man's error code
  }
  
  // insert node itself
  aAncestorNodes->InsertElementAt((void*)contentNode,i);
  aAncestorOffsets->InsertElementAt((void*)aOffset,i);
  
  // insert all the ancestors
  // not doing all the inserts at location 0, that would make for lots of memcopys in the voidArray::InsertElementAt implementation
  contentNode->GetParent(*getter_AddRefs(contentParent));
  while(contentParent)
  {
    contentParent->IndexOf(contentNode, nodeOffset);
    ++i;
    aAncestorNodes->InsertElementAt((void*)contentParent,i);
    aAncestorOffsets->InsertElementAt((void*)nodeOffset,i);
    contentNode = contentParent;
    contentNode->GetParent(*getter_AddRefs(contentParent));
  }
  
  return i;
}

nsCOMPtr<nsIDOMNode> nsRange::CommonParent(nsIDOMNode* aNode1, nsIDOMNode* aNode2)
{
  nsCOMPtr<nsIDOMNode> theParent;
  
  // no null nodes please
  if (!aNode1 || !aNode2) 
    return theParent;  // moral equiv of nsnull
  
  // shortcut for common case - both nodes are the same
  if (aNode1 == aNode2)
  {
    theParent = aNode1; // this forces an AddRef
    return theParent;
  }

  // otherwise traverse the tree for the common ancestor
  // For now, a pretty dumb hack on computing this
  nsAutoVoidArray array1;
  nsAutoVoidArray array2;
  PRInt32     i=0, j=0;
  
  // get ancestors of each node
  i = FillArrayWithAncestors(&array1,aNode1);
  j = FillArrayWithAncestors(&array2,aNode2);
  
  // sanity test (for now) - FillArrayWithAncestors succeeded
  if ((i==-1) || (j==-1))
  {
    NS_NOTREACHED("nsRange::CommonParent");
    return theParent; // moral equiv of nsnull
  }
  
  // sanity test (for now) - the end of each array
  // should match and be the root
  if (array1.ElementAt(i) != array2.ElementAt(j))
  {
    NS_NOTREACHED("nsRange::CommonParent");
    return theParent; // moral equiv of nsnull
  }
  
  // back through the ancestors, starting from the root, until
  // first different ancestor found.  
  while (i >= 0 && j >= 0 && array1.ElementAt(i) == array2.ElementAt(j))
  {
    --i;
    --j;
    // i < 0 will only happen if one endpoint's node is the common ancestor
    // of the other
  }
  // now back up one and that's the last common ancestor from the root,
  // or the first common ancestor from the leaf perspective
  i++;
  // i >= 0 now
  nsIDOMNode *node = NS_STATIC_CAST(nsIDOMNode*, array1.ElementAt(i));
  theParent = do_QueryInterface(node);
  return theParent;  
}

nsresult nsRange::GetDOMNodeFromContent(nsIContent* inContentNode, nsCOMPtr<nsIDOMNode>* outDomNode)
{
  if (!outDomNode) 
    return NS_ERROR_NULL_POINTER;
  nsresult res = inContentNode->QueryInterface(NS_GET_IID(nsIDOMNode), getter_AddRefs(*outDomNode));
  if (NS_FAILED(res)) 
    return res;
  return NS_OK;
}

nsresult nsRange::GetContentFromDOMNode(nsIDOMNode* inDomNode, nsCOMPtr<nsIContent>* outContentNode)
{
  if (!outContentNode) 
    return NS_ERROR_NULL_POINTER;
  nsresult res = inDomNode->QueryInterface(NS_GET_IID(nsIContent), getter_AddRefs(*outContentNode));
  if (NS_FAILED(res)) 
    return res;
  return NS_OK;
}

nsresult nsRange::PopRanges(nsIDOMNode* aDestNode, PRInt32 aOffset, nsIContent* aSourceNode)
{
  // utility routine to pop all the range endpoints inside the content subtree defined by 
  // aSourceNode, into the node/offset represented by aDestNode/aOffset.
  
  nsCOMPtr<nsIContentIterator> iter;
  nsresult res = NS_NewContentIterator(getter_AddRefs(iter));
  iter->Init(aSourceNode);

  nsCOMPtr<nsIContent> cN;
  nsVoidArray* theRangeList;
  
  iter->CurrentNode(getter_AddRefs(cN));
  while (cN && (NS_ENUMERATOR_FALSE == iter->IsDone()))
  {
    cN->GetRangeList(theRangeList);
    if (theRangeList)
    {
       nsRange* theRange;
       PRInt32  theCount = theRangeList->Count();
       while (theCount)
       {
          theRange = NS_STATIC_CAST(nsRange*, (theRangeList->ElementAt(0)));
          if (theRange)
          {
            nsCOMPtr<nsIDOMNode> domNode;
            res = GetDOMNodeFromContent(cN, address_of(domNode));
            NS_POSTCONDITION(NS_SUCCEEDED(res), "error updating range list");
            NS_POSTCONDITION(domNode, "error updating range list");
            // sanity check - do range and content agree over ownership?
            res = theRange->ContentOwnsUs(domNode);
            NS_POSTCONDITION(NS_SUCCEEDED(res), "range and content disagree over range ownership");

            if (theRange->mStartParent == domNode)
            {
              // promote start point up to replacement point
              res = theRange->SetStart(aDestNode, aOffset);
              NS_POSTCONDITION(NS_SUCCEEDED(res), "nsRange::PopRanges() got error from SetStart()");
              if (NS_FAILED(res)) return res;
            }
            if (theRange->mEndParent == domNode)
            {
              // promote end point up to replacement point
              res = theRange->SetEnd(aDestNode, aOffset);
              NS_POSTCONDITION(NS_SUCCEEDED(res), "nsRange::PopRanges() got error from SetEnd()");
              if (NS_FAILED(res)) return res;
            }          
          }
          // must refresh theRangeList - it might have gone away!
          cN->GetRangeList(theRangeList);
          if (theRangeList)
            theCount = theRangeList->Count();
          else
            theCount = 0;
       } 
    }
    res = iter->Next();
    if (NS_FAILED(res)) // a little noise here to catch bugs
    {
      NS_NOTREACHED("nsRange::PopRanges() : iterator failed to advance");
      return res;
    }
    iter->CurrentNode(getter_AddRefs(cN));
  }
  
  return NS_OK;
}

// sanity check routine for content helpers.  confirms that given 
// node owns one or both range endpoints.
nsresult nsRange::ContentOwnsUs(nsIDOMNode* domNode)
{
  NS_PRECONDITION(domNode, "null pointer");
  if ((mStartParent.get() != domNode) && (mEndParent.get() != domNode))
  {
    NS_NOTREACHED("nsRange::ContentOwnsUs");
    return NS_ERROR_UNEXPECTED;
  }
  return NS_OK;
}

/******************************************************
 * public functionality
 ******************************************************/

nsresult nsRange::GetIsPositioned(PRBool* aIsPositioned)
{
  *aIsPositioned = mIsPositioned;
  return NS_OK;
}

nsresult nsRange::GetStartContainer(nsIDOMNode** aStartParent)
{
  if (!mIsPositioned)
    return NS_ERROR_NOT_INITIALIZED;
  if (!aStartParent)
    return NS_ERROR_NULL_POINTER;
  //NS_IF_RELEASE(*aStartParent); don't think we should be doing this
  *aStartParent = mStartParent;
  NS_IF_ADDREF(*aStartParent);
  return NS_OK;
}

nsresult nsRange::GetStartOffset(PRInt32* aStartOffset)
{
  if (!mIsPositioned)
    return NS_ERROR_NOT_INITIALIZED;
  if (!aStartOffset)
    return NS_ERROR_NULL_POINTER;
  *aStartOffset = mStartOffset;
  return NS_OK;
}

nsresult nsRange::GetEndContainer(nsIDOMNode** aEndParent)
{
  if (!mIsPositioned)
    return NS_ERROR_NOT_INITIALIZED;
  if (!aEndParent)
    return NS_ERROR_NULL_POINTER;
  //NS_IF_RELEASE(*aEndParent); don't think we should be doing this
  *aEndParent = mEndParent;
  NS_IF_ADDREF(*aEndParent);
  return NS_OK;
}

nsresult nsRange::GetEndOffset(PRInt32* aEndOffset)
{
  if (!mIsPositioned)
    return NS_ERROR_NOT_INITIALIZED;
  if (!aEndOffset)
    return NS_ERROR_NULL_POINTER;
  *aEndOffset = mEndOffset;
  return NS_OK;
}

nsresult nsRange::GetCollapsed(PRBool* aIsCollapsed)
{
  if(IsDetached())
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  if (!mIsPositioned)
    return NS_ERROR_NOT_INITIALIZED;

  if (mEndParent == 0 ||
      (mStartParent == mEndParent && mStartOffset == mEndOffset))
    *aIsCollapsed = PR_TRUE;
  else
    *aIsCollapsed = PR_FALSE;
  return NS_OK;
}

nsresult nsRange::GetCommonAncestorContainer(nsIDOMNode** aCommonParent)
{ 
  if(IsDetached())
    return NS_ERROR_DOM_INVALID_STATE_ERR;

  *aCommonParent = CommonParent(mStartParent,mEndParent);
  NS_IF_ADDREF(*aCommonParent);
  return NS_OK;
}

nsresult nsRange::SetStart(nsIDOMNode* aParent, PRInt32 aOffset)
{
  if(IsDetached())
    return NS_ERROR_DOM_INVALID_STATE_ERR;

  nsresult res;
  
  if (!aParent) return NS_ERROR_NULL_POINTER;
  
  nsCOMPtr<nsIDOMNode>theParent( do_QueryInterface(aParent) );
    
  // must be in same document as endpoint, else 
  // endpoint is collapsed to new start.
  if (mIsPositioned && !InSameDoc(theParent,mEndParent))
  {
    res = DoSetRange(theParent,aOffset,theParent,aOffset);
    return res;
  }
  // start must be before end
  if (mIsPositioned && !IsIncreasing(theParent,aOffset,mEndParent,mEndOffset))
    return NS_ERROR_ILLEGAL_VALUE;
  // if it's in an attribute node, end must be in or descended from same node
  // (haven't done this one yet)
  
  res = DoSetRange(theParent,aOffset,mEndParent,mEndOffset);
  return res;
}

nsresult nsRange::SetStartBefore(nsIDOMNode* aSibling)
{
  if(IsDetached())
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  if (nsnull == aSibling)// Not the correct one to throw, but spec doesn't say what is
    return NS_ERROR_DOM_NOT_OBJECT_ERR;

  nsCOMPtr<nsIDOMNode> nParent;
  nsresult res = aSibling->GetParentNode(getter_AddRefs(nParent));
  if (NS_FAILED(res) || !nParent) return NS_ERROR_DOM_RANGE_INVALID_NODE_TYPE_ERR;
  PRInt32 indx = IndexOf(aSibling);
  return SetStart(nParent,indx);
}

nsresult nsRange::SetStartAfter(nsIDOMNode* aSibling)
{
  if(IsDetached())
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  if (nsnull == aSibling)// Not the correct one to throw, but spec doesn't say what is
    return NS_ERROR_DOM_NOT_OBJECT_ERR;

  nsCOMPtr<nsIDOMNode> nParent;
  nsresult res = aSibling->GetParentNode(getter_AddRefs(nParent));
  if (NS_FAILED(res) || !nParent) return NS_ERROR_DOM_RANGE_INVALID_NODE_TYPE_ERR;
  PRInt32 indx = IndexOf(aSibling) + 1;
  return SetStart(nParent,indx);
}

nsresult nsRange::SetEnd(nsIDOMNode* aParent, PRInt32 aOffset)
{
  if(IsDetached())
    return NS_ERROR_DOM_INVALID_STATE_ERR;

  nsresult res;
  
  if (!aParent) return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIDOMNode>theParent( do_QueryInterface(aParent) );
  
  // must be in same document as startpoint, else 
  // endpoint is collapsed to new end.
  if (mIsPositioned && !InSameDoc(theParent,mStartParent))
  {
    res = DoSetRange(theParent,aOffset,theParent,aOffset);
    return res;
  }
  // start must be before end
  if (mIsPositioned && !IsIncreasing(mStartParent,mStartOffset,theParent,aOffset))
    return NS_ERROR_ILLEGAL_VALUE;
  // if it's in an attribute node, start must be in or descended from same node
  // (haven't done this one yet)
  
  res = DoSetRange(mStartParent,mStartOffset,theParent,aOffset);
  return res;
}

nsresult nsRange::SetEndBefore(nsIDOMNode* aSibling)
{
  if(IsDetached())
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  if (nsnull == aSibling)// Not the correct one to throw, but spec doesn't say what is
    return NS_ERROR_DOM_NOT_OBJECT_ERR;

  nsCOMPtr<nsIDOMNode> nParent;
  nsresult res = aSibling->GetParentNode(getter_AddRefs(nParent));
  if (NS_FAILED(res) || !nParent) return NS_ERROR_DOM_RANGE_INVALID_NODE_TYPE_ERR;
  PRInt32 indx = IndexOf(aSibling);
  return SetEnd(nParent,indx);
}

nsresult nsRange::SetEndAfter(nsIDOMNode* aSibling)
{
  if(IsDetached())
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  if (nsnull == aSibling)// Not the correct one to throw, but spec doesn't say what is
    return NS_ERROR_DOM_NOT_OBJECT_ERR;

  nsCOMPtr<nsIDOMNode> nParent;
  nsresult res = aSibling->GetParentNode(getter_AddRefs(nParent));
  if (NS_FAILED(res) || !nParent) return NS_ERROR_DOM_RANGE_INVALID_NODE_TYPE_ERR;
  PRInt32 indx = IndexOf(aSibling) + 1;
  return SetEnd(nParent,indx);
}

nsresult nsRange::Collapse(PRBool aToStart)
{
  if(IsDetached())
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  if (!mIsPositioned)
    return NS_ERROR_NOT_INITIALIZED;

  if (aToStart)
    return DoSetRange(mStartParent,mStartOffset,mStartParent,mStartOffset);
  else
    return DoSetRange(mEndParent,mEndOffset,mEndParent,mEndOffset);
}

nsresult nsRange::Unposition()
{
  return DoSetRange(nsCOMPtr<nsIDOMNode>(),0,nsCOMPtr<nsIDOMNode>(),0); 
  // note that "nsCOMPtr<nsIDOMmNode>()" is the moral equivalent of null
}

nsresult nsRange::SelectNode(nsIDOMNode* aN)
{
  if(IsDetached())
    return NS_ERROR_DOM_INVALID_STATE_ERR;

  if (!aN) return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsIDOMNode> parent;
  PRInt32 start, end;

  PRUint16 type = 0;
  aN->GetNodeType(&type);

  switch (type) {
    case nsIDOMNode::ATTRIBUTE_NODE :
    case nsIDOMNode::ENTITY_NODE :
    case nsIDOMNode::DOCUMENT_NODE :
    case nsIDOMNode::DOCUMENT_FRAGMENT_NODE :
    case nsIDOMNode::NOTATION_NODE :
      return NS_ERROR_DOM_RANGE_INVALID_NODE_TYPE_ERR;
  }

  nsresult res;
  res = aN->GetParentNode(getter_AddRefs(parent));
  if(NS_SUCCEEDED(res) && parent)
  {
    nsCOMPtr<nsIDOMDocument> doc(do_QueryInterface(parent));
    if(doc)
    {
      nsCOMPtr<nsIContent>content(do_QueryInterface(aN));
      if(!content)
        return NS_ERROR_DOM_RANGE_INVALID_NODE_TYPE_ERR;
      parent = aN;//parent is now equal to the node you passed in
      // which is the root.  start is zero, end is the number of children
      start = 0;
      res = content->ChildCount(end);
      if (NS_FAILED(res))
        return NS_ERROR_DOM_RANGE_INVALID_NODE_TYPE_ERR;
    }
    else
    {
      start = IndexOf(aN);
      end = start + 1;
    }
    return DoSetRange(parent,start,parent,end);
  }
  return NS_ERROR_DOM_RANGE_INVALID_NODE_TYPE_ERR;
}

nsresult nsRange::SelectNodeContents(nsIDOMNode* aN)
{
  if(IsDetached())
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  NS_ENSURE_ARG_POINTER(aN);

  nsCOMPtr<nsIDOMNode> theNode( do_QueryInterface(aN) );
  nsCOMPtr<nsIDOMNodeList> aChildNodes;
  
  nsresult res = aN->GetChildNodes(getter_AddRefs(aChildNodes));
  if (NS_FAILED(res))
    return res;
  if (!aChildNodes)
    return NS_ERROR_UNEXPECTED;
  PRUint32 indx;
  res = aChildNodes->GetLength(&indx);
  if (NS_FAILED(res))
    return res;
  return DoSetRange(theNode,0,theNode,indx);
}

#ifdef XP_MAC
#pragma mark -
#pragma mark  class RangeSubtreeIterator
#pragma mark -
#endif

// The Subtree Content Iterator only returns subtrees that are
// completely within a given range. It doesn't return a CharacterData
// node that contains either the start or end point of the range.
// We need an iterator that will also include these start/end points
// so that our methods/algorithms aren't cluttered with special
// case code that tries to include these points while iterating.
//
// The RangeSubtreeIterator class mimics the nsIContentIterator
// methods we need, so should the Content Iterator support the
// start/end points in the future, we can switchover relatively
// easy.

class RangeSubtreeIterator
{
private:

  enum RangeSubtreeIterState { eDone=0,
                               eUseStartCData,
                               eUseIterator,
                               eUseEndCData };

  nsCOMPtr<nsIContentIterator>  mIter;
  RangeSubtreeIterState         mIterState;

  nsCOMPtr<nsIDOMNode>          mStartCData;
  nsCOMPtr<nsIDOMNode>          mEndCData;

public:

  RangeSubtreeIterator() : mIterState(eDone) {}
  ~RangeSubtreeIterator() {}

  nsresult Init(nsIDOMRange *aRange)
  {
    NS_ENSURE_ARG_POINTER(aRange);

    mIterState = eDone;

    nsCOMPtr<nsIDOMNode> node;

    // Grab the start point of the range and QI it to
    // a CharacterData pointer. If it is CharacterData store
    // a pointer to the node.

    nsresult res = aRange->GetStartContainer(getter_AddRefs(node));
    if (NS_FAILED(res)) return res;
    if (!node) return NS_ERROR_FAILURE;

    nsCOMPtr<nsIDOMCharacterData> cData = do_QueryInterface(node);
    if (cData)
      mStartCData = node;

    // Grab the end point of the range and QI it to
    // a CharacterData pointer. If it is CharacterData store
    // a pointer to the node.

    res = aRange->GetEndContainer(getter_AddRefs(node));
    if (NS_FAILED(res)) return res;
    if (!node) return NS_ERROR_FAILURE;

    cData = do_QueryInterface(node);
    if (cData)
      mEndCData = node;

    if (mStartCData && mStartCData == mEndCData)
    {
      // The range starts and stops in the same CharacterData
      // node. Null out the end pointer so we only visit the
      // node once!

      mEndCData = nsnull;
    }
    else
    {
      // Now create a Content Subtree Iterator to be used
      // for the subtrees between the end points!

      res = NS_NewContentSubtreeIterator(getter_AddRefs(mIter));

      if (NS_FAILED(res)) return res;
      if (!mIter) return NS_ERROR_FAILURE;

      res = mIter->Init(aRange);
      if (NS_FAILED(res)) return res;

      if (mIter->IsDone() != NS_ENUMERATOR_FALSE)
      {
        // The subtree iterator thinks there's nothing
        // to iterate over, so just free it up so we
        // don't accidentally call into it.

        mIter = nsnull;
      }
    }

    // Initialize the iterator by calling First().
    // Note that we are ignoring the return value on purpose!

    (void)First();

    return NS_OK;
  }

  nsresult CurrentNode(nsIDOMNode **aNode)
  {
    NS_ENSURE_ARG_POINTER(aNode);

    *aNode = nsnull;

    nsresult res = NS_OK;

    if (mIterState == eUseStartCData && mStartCData)
      *aNode = mStartCData;
    else if (mIterState == eUseEndCData && mEndCData)
      *aNode = mEndCData;
    else if (mIterState == eUseIterator && mIter)
    {
      nsCOMPtr<nsIContent> content;
      res = mIter->CurrentNode(getter_AddRefs(content));
      if (NS_FAILED(res)) return res;
      if (!content) return NS_ERROR_FAILURE;
      nsCOMPtr<nsIDOMNode> node(do_QueryInterface(content));
      if (!node) return NS_ERROR_FAILURE;
      *aNode = node;
    }
    else
      res = NS_ERROR_FAILURE;

    NS_IF_ADDREF(*aNode);

    return res;
  }

  nsresult First()
  {
    nsresult res = NS_OK;

    if (mStartCData)
      mIterState = eUseStartCData;
    else if (mIter)
    {
      res = mIter->First();
      if (NS_FAILED(res)) return res;
      mIterState = eUseIterator;
    }
    else if (mEndCData)
      mIterState = eUseEndCData;
    else
      res = NS_ERROR_FAILURE;

    return res;
  }

  nsresult Last()
  {
    nsresult res = NS_OK;

    if (mEndCData)
      mIterState = eUseEndCData;
    else if (mIter)
    {
      res = mIter->Last();
      if (NS_FAILED(res)) return res;
      mIterState = eUseIterator;
    }
    else if (mStartCData)
      mIterState = eUseStartCData;
    else
      res = NS_ERROR_FAILURE;

    return res;
  }

  nsresult Next()
  {
    nsresult res = NS_OK;

    if (mIterState == eUseStartCData)
    {
      if (mIter)
      {
        res = mIter->First();
        if (NS_FAILED(res)) return res;
        mIterState = eUseIterator;
      }
      else if (mEndCData)
        mIterState = eUseEndCData;
      else
        mIterState = eDone;
    }
    else if (mIterState == eUseIterator)
    {
      res = mIter->Next();
      if (NS_FAILED(res)) return res;

      if (mIter->IsDone() != NS_ENUMERATOR_FALSE)
      {
        if (mEndCData)
          mIterState = eUseEndCData;
        else
          mIterState = eDone;
      }
    }
    else if (mIterState == eUseEndCData)
      mIterState = eDone;
    else
      res = NS_ERROR_FAILURE;

    return res;
  }

  nsresult Prev()
  {
    nsresult res = NS_OK;

    if (mIterState == eUseEndCData)
    {
      if (mIter)
      {
        res = mIter->Last();
        if (NS_FAILED(res)) return res;
        mIterState = eUseIterator;
      }
      else if (mStartCData)
        mIterState = eUseStartCData;
      else
        mIterState = eDone;
    }
    else if (mIterState == eUseIterator)
    {
      res = mIter->Prev();
      if (NS_FAILED(res)) return res;

      if (mIter->IsDone() != NS_ENUMERATOR_FALSE)
      {
        if (mStartCData)
          mIterState = eUseStartCData;
        else
          mIterState = eDone;
      }
    }
    else if (mIterState == eUseStartCData)
      mIterState = eDone;
    else
      res = NS_ERROR_FAILURE;

    return res;
  }

  PRBool IsDone()
  {
    return mIterState == eDone;
  }
};

// CollapseRangeAfterDelete() is a utiltiy method that is used by
// DeleteContents() and ExtractContents() to collapse the range
// in the correct place, under the range's root container (the
// range end points common container) as outlined by the Range spec:
//
// http://www.w3.org/TR/2000/REC-DOM-Level-2-Traversal-Range-20001113/ranges.html
// The assumption made by this method is that the delete or extract
// has been done already, and left the range in a state where there is
// no content between the 2 end points.

nsresult nsRange::CollapseRangeAfterDelete(nsIDOMRange *aRange)
{
  NS_ENSURE_ARG_POINTER(aRange);

  // Check if range gravity took care of collapsing the range for us!

  PRBool isCollapsed = PR_FALSE;
  nsresult res = aRange->GetCollapsed(&isCollapsed);
  if (NS_FAILED(res)) return res;

  if (isCollapsed)
  {
    // aRange is collapsed so there's nothing for us to do.
    //
    // There are 2 possible scenarios here:
    //
    // 1. aRange could've been collapsed prior to the delete/extract,
    //    which would've resulted in nothing being removed, so aRange
    //    is already where it should be.
    //
    // 2. Prior to the delete/extract, aRange's start and end were in
    //    the same container which would mean everything between them
    //    was removed, causing range gravity to collapse the range.

    return NS_OK;
  }

  // aRange isn't collapsed so figure out the appropriate place to collapse!
  // First get both end points and their common ancestor.

  nsCOMPtr<nsIDOMNode> commonAncestor;
  res = aRange->GetCommonAncestorContainer(getter_AddRefs(commonAncestor));
  if(NS_FAILED(res)) return res;

  nsCOMPtr<nsIDOMNode> startContainer, endContainer;

  res = aRange->GetStartContainer(getter_AddRefs(startContainer));
  if (NS_FAILED(res)) return res;

  res = aRange->GetEndContainer(getter_AddRefs(endContainer));
  if (NS_FAILED(res)) return res;

  // Collapse to one of the end points if they are already in the
  // commonAncestor. This should work ok since this method is called
  // immediately after a delete or extract that leaves no content
  // between the 2 end points!

  if (startContainer == commonAncestor)
    return aRange->Collapse(PR_TRUE);
  if (endContainer == commonAncestor)
    return aRange->Collapse(PR_FALSE);

  // End points are at differing levels. We want to collapse to the
  // point that is between the 2 subtrees that contain each point,
  // under the common ancestor.

  nsCOMPtr<nsIDOMNode> nodeToSelect(startContainer), parent;

  while (nodeToSelect)
  {
    nsresult res = nodeToSelect->GetParentNode(getter_AddRefs(parent));
    if (NS_FAILED(res)) return res;

    if (parent == commonAncestor)
      break; // We found the nodeToSelect!

    nodeToSelect = parent;
  }

  if (!nodeToSelect)
    return NS_ERROR_FAILURE; // This should never happen!

  res = aRange->SelectNode(nodeToSelect);
  if (NS_FAILED(res)) return res;

  return aRange->Collapse(PR_FALSE);
}

nsresult nsRange::DeleteContents()
{ 
  if(IsDetached())
    return NS_ERROR_DOM_INVALID_STATE_ERR;

  // Save the range end points locally to avoid interference
  // of Range gravity during our edits!

  nsCOMPtr<nsIDOMNode> startContainer(mStartParent);
  PRInt32              startOffset = mStartOffset;
  nsCOMPtr<nsIDOMNode> endContainer(mEndParent);
  PRInt32              endOffset = mEndOffset;

  // Create and initialize a subtree iterator that will give
  // us all the subtrees within the range.

  RangeSubtreeIterator iter;

  nsresult res = iter.Init(this);
  if (NS_FAILED(res)) return res;

  if (iter.IsDone())
  {
    // There's nothing for us to delete.
    return CollapseRangeAfterDelete(this);
  }

  // We delete backwards to avoid iterator problems!

  res = iter.Last();
  if (NS_FAILED(res)) return res;

  nsCOMPtr<nsIDOMNode> node;
  res = iter.CurrentNode(getter_AddRefs(node));
  if (NS_FAILED(res)) return res;
  if (!node) return NS_ERROR_FAILURE;

  PRBool handled = PR_FALSE;

  // With the exception of text nodes that contain one of the range
  // end points, the subtree iterator should only give us back subtrees
  // that are completely contained between the range's end points.

  while (node)
  {
    // Before we delete anything, advance the iterator to the
    // next subtree.

    res = iter.Prev();
    if (NS_FAILED(res)) return res;

    handled = PR_FALSE;

    // If it's CharacterData, make sure we might need to delete
    // part of the data, instead of removing the whole node.
    //
    // XXX_kin: We need to also handle ProcessingInstruction
    // XXX_kin: according to the spec.

    nsCOMPtr<nsIDOMCharacterData> charData(do_QueryInterface(node));

    if (charData)
    {
      PRUint32 dataLength = 0;

      if (node == startContainer)
      {
        if (node == endContainer)
        {
          // This range is completely contained within a single text node.
          // Delete the data between startOffset and endOffset.

          if (endOffset > startOffset)
          {
            res = charData->DeleteData(startOffset, endOffset - startOffset);
            if (NS_FAILED(res)) return res;
          }

          handled = PR_TRUE;
        }
        else
        {
          // Delete everything after startOffset.

          res = charData->GetLength(&dataLength);
          if (NS_FAILED(res)) return res;

          if (dataLength > (PRUint32)startOffset)
          {
            res = charData->DeleteData(startOffset, dataLength - startOffset);
            if (NS_FAILED(res)) return res;
          }

          handled = PR_TRUE;
        }
      }
      else if (node == endContainer)
      {
        // Delete the data between 0 and endOffset.

        if (endOffset > 0)
        {
          res = charData->DeleteData(0, endOffset);
          if (NS_FAILED(res)) return res;
        }

        handled = PR_TRUE;
      }       
    }

    if (!handled)
    {
      // node was not handled above, so it must be completely contained
      // within the range. Just remove it from the tree!

      nsCOMPtr<nsIDOMNode> parent, tmpNode;

      res = node->GetParentNode(getter_AddRefs(parent));
      if (NS_FAILED(res)) return res;

      res = parent->RemoveChild(node, getter_AddRefs(tmpNode));
      if (NS_FAILED(res)) return res;
    }

    if (iter.IsDone())
      break; // We must be done!

    res = iter.CurrentNode(getter_AddRefs(node));
    if (NS_FAILED(res)) return res;
    if (!node) return NS_ERROR_FAILURE;
  }

  // XXX_kin: At this point we should be checking for the case
  // XXX_kin: where we have 2 adjacent text nodes left, each
  // XXX_kin: containing one of the range end points. The spec
  // XXX_kin: says the 2 nodes should be merged in that case,
  // XXX_kin: and to use Normalize() to do the merging, but
  // XXX_kin: calling Normalize() on the common parent to accomplish
  // XXX_kin: this might also normalize nodes that are outside the
  // XXX_kin: range but under the common parent. Need to verify
  // XXX_kin: with the range commitee members that this was the
  // XXX_kin: desired behavior. For now we don't merge anything!

  return CollapseRangeAfterDelete(this);
}

nsresult nsRange::CompareBoundaryPoints(PRUint16 how, nsIDOMRange* srcRange,
                                   PRInt32* aCmpRet)
{
  if(IsDetached())
    return NS_ERROR_DOM_INVALID_STATE_ERR;

  nsresult res;
  if (aCmpRet == 0)
    return NS_ERROR_NULL_POINTER;
  if (srcRange == 0)
    return NS_ERROR_INVALID_ARG;

  nsCOMPtr<nsIDOMNode> boundaryNode;  // the Invoking range
  nsCOMPtr<nsIDOMNode> sourceNode;    // the sourceRange
  PRInt32 boundaryOffset, sourceOffset;

  switch (how)
  {
  case nsIDOMRange::START_TO_START: // where is the start point of boundary range
    boundaryNode = mStartParent;    // relative to the start point of the source range?
    boundaryOffset = mStartOffset;
    res = srcRange->GetStartContainer(getter_AddRefs(sourceNode));
    if (NS_SUCCEEDED(res))
      res = srcRange->GetStartOffset(&sourceOffset);
    break;
  case nsIDOMRange::START_TO_END: // where is the end point of the boundary range
    boundaryNode = mEndParent;  // relative to the start point of source range?
    boundaryOffset = mEndOffset;
    res = srcRange->GetStartContainer(getter_AddRefs(sourceNode));
    if (NS_SUCCEEDED(res))
      res = srcRange->GetStartOffset(&sourceOffset);
    break;
  case nsIDOMRange::END_TO_START: // where is the the start point of the boundary range
    boundaryNode = mStartParent;    // relative to end point of source range?
    boundaryOffset = mStartOffset;
    res = srcRange->GetEndContainer(getter_AddRefs(sourceNode));
    if (NS_SUCCEEDED(res))
      res = srcRange->GetEndOffset(&sourceOffset);
    break;
  case nsIDOMRange::END_TO_END: // where is the end point of boundary range
    boundaryNode = mEndParent;  // relative to the end point of the source range?
    boundaryOffset = mEndOffset;
    res = srcRange->GetEndContainer(getter_AddRefs(sourceNode));
    if (NS_SUCCEEDED(res))
      res = srcRange->GetEndOffset(&sourceOffset);
    break;

  default:  // shouldn't get here
    return NS_ERROR_ILLEGAL_VALUE;
  }

  if (NS_FAILED(res))
    return res;

  if ((boundaryNode == sourceNode) && (boundaryOffset == sourceOffset))
    *aCmpRet = 0;//then the points are equal
  else if (IsIncreasing(boundaryNode, boundaryOffset, sourceNode, sourceOffset))
      *aCmpRet = -1;//then boundary point is before source point
  else
      *aCmpRet = 1;//then boundary point is after source point

  return NS_OK;
}

nsresult nsRange::ExtractContents(nsIDOMDocumentFragment** aReturn)
{ 
  if(IsDetached())
    return NS_ERROR_DOM_INVALID_STATE_ERR;

  // XXX_kin: The spec says that nodes that are completely in the
  // XXX_kin: range should be moved into the document fragment, not
  // XXX_kin: copied. This method will have to be rewritten using
  // XXX_kin: DeleteContents() as a template, with the charData cloning
  // XXX_kin: code from CloneContents() merged in.

  nsresult res = CloneContents(aReturn);
  if (NS_FAILED(res))
    return res;
  res = DeleteContents();
  return res; 
}

nsresult nsRange::CloneParentsBetween(nsIDOMNode *aAncestor,
                             nsIDOMNode *aNode,
                             nsIDOMNode **aClosestAncestor,
                             nsIDOMNode **aFarthestAncestor)
{
  NS_ENSURE_ARG_POINTER((aAncestor && aNode && aClosestAncestor && aFarthestAncestor));

  *aClosestAncestor  = nsnull;
  *aFarthestAncestor = nsnull;

  if (aAncestor == aNode)
    return NS_OK;

  nsCOMPtr<nsIDOMNode> parent, firstParent, lastParent;

  nsresult res = aNode->GetParentNode(getter_AddRefs(parent));

  while(parent && parent != aAncestor)
  {
    nsCOMPtr<nsIDOMNode> clone, tmpNode;

    res = parent->CloneNode(PR_FALSE, getter_AddRefs(clone));

    if (NS_FAILED(res)) return res;
    if (!clone)         return NS_ERROR_FAILURE;

    if (! firstParent)
      firstParent = lastParent = clone;
    else
    {
      res = clone->AppendChild(lastParent, getter_AddRefs(tmpNode));

      if (NS_FAILED(res)) return res;

      lastParent = clone;
    }

    tmpNode = parent;
    res = tmpNode->GetParentNode(getter_AddRefs(parent));
  }

  *aClosestAncestor  = firstParent;
  NS_IF_ADDREF(*aClosestAncestor);

  *aFarthestAncestor = lastParent;
  NS_IF_ADDREF(*aFarthestAncestor);

  return NS_OK;
}

nsresult nsRange::CloneContents(nsIDOMDocumentFragment** aReturn)
{
  if (IsDetached())
    return NS_ERROR_DOM_INVALID_STATE_ERR;

  nsresult res;
  nsCOMPtr<nsIDOMNode> commonAncestor;
  res = GetCommonAncestorContainer(getter_AddRefs(commonAncestor));
  if (NS_FAILED(res)) return res;

  nsCOMPtr<nsIDOMDocument> document;
  res = mStartParent->GetOwnerDocument(getter_AddRefs(document));
  if (NS_FAILED(res)) return res;

  // Create a new document fragment in the context of this document,
  // which might be null

  nsCOMPtr<nsIDOMDocumentFragment> clonedFrag;

  nsCOMPtr<nsIDocument> doc(do_QueryInterface(document));

  res = NS_NewDocumentFragment(getter_AddRefs(clonedFrag), doc);
  if (NS_FAILED(res)) return res;

  nsCOMPtr<nsIDOMNode> commonCloneAncestor(do_QueryInterface(clonedFrag));
  if (!commonCloneAncestor) return NS_ERROR_FAILURE;

  // Create and initialize a subtree iterator that will give
  // us all the subtrees within the range.

  RangeSubtreeIterator iter;

  res = iter.Init(this);
  if (NS_FAILED(res)) return res;

  if (iter.IsDone())
  {
    // There's nothing to add to the doc frag, we must be done!

    *aReturn = clonedFrag;
    NS_IF_ADDREF(*aReturn);
    return NS_OK;
  }

  res = iter.First();
  if (NS_FAILED(res)) return res;

  nsCOMPtr<nsIDOMNode> node;
  res = iter.CurrentNode(getter_AddRefs(node));
  if (NS_FAILED(res)) return res;
  if (!node) return NS_ERROR_FAILURE;


  // With the exception of text nodes that contain one of the range
  // end points, the subtree iterator should only give us back subtrees
  // that are completely contained between the range's end points.
  //
  // Unfortunately these subtrees don't contain the parent hierarchy/context
  // that the Range spec requires us to return. This loop clones the
  // parent hierarchy, adds a cloned version of the subtree, to it, then
  // correctly places this new subtree into the doc fragment.

  while (node)
  {
    // Clone the current subtree!

    nsCOMPtr<nsIDOMNode> clone;
    res = node->CloneNode(PR_TRUE, getter_AddRefs(clone));

    if (NS_FAILED(res)) return res;
    if (!clone) return NS_ERROR_FAILURE;

    // If it's CharacterData, make sure we only clone what
    // is in the range.
    //
    // XXX_kin: We need to also handle ProcessingInstruction
    // XXX_kin: according to the spec.

    nsCOMPtr<nsIDOMCharacterData> charData(do_QueryInterface(clone));

    if (charData)
    {
      if (node == mEndParent)
      {
        // We only need the data before mEndOffset, so get rid of any
        // data after it.

        PRUint32 dataLength = 0;
        res = charData->GetLength(&dataLength);
        if (NS_FAILED(res)) return res;

        if (dataLength > (PRUint32)mEndOffset)
        {
          res = charData->DeleteData(mEndOffset, dataLength - mEndOffset);
          if (NS_FAILED(res)) return res;
        }
      }       

      if (node == mStartParent)
      {
        // We don't need any data before mStartOffset, so just
        // delete it!

        if (mStartOffset > 0)
        {
          res = charData->DeleteData(0, mStartOffset);
          if (NS_FAILED(res)) return res;
        }
      }
    }

    // Clone the parent hierarchy between commonAncestor and node.

    nsCOMPtr<nsIDOMNode> closestAncestor, farthestAncestor;

    res = CloneParentsBetween(commonAncestor, node,
                              getter_AddRefs(closestAncestor),
                              getter_AddRefs(farthestAncestor));

    if (NS_FAILED(res)) return res;

    // Hook the parent hierarchy/context of the subtree into the clone tree.

    nsCOMPtr<nsIDOMNode> tmpNode;

    if (farthestAncestor)
    {
      res = commonCloneAncestor->AppendChild(farthestAncestor,
                                             getter_AddRefs(tmpNode));

      if (NS_FAILED(res)) return res;
    }

    // Place the cloned subtree into the cloned doc frag tree!

    if (closestAncestor)
    {
      // Append the subtree under closestAncestor since it is the
      // immediate parent of the subtree.

      res = closestAncestor->AppendChild(clone, getter_AddRefs(tmpNode));

      if (NS_FAILED(res)) return res;
    }
    else
    {
      // If we get here, there is no missing parent hierarchy between 
      // commonAncestor and node, so just append clone to commonCloneAncestor.

      res = commonCloneAncestor->AppendChild(clone, getter_AddRefs(tmpNode));

      if (NS_FAILED(res)) return res;
    }

    // Get the next subtree to be processed. The idea here is to setup
    // the parameters for the next iteration of the loop.

    res = iter.Next();

    if (iter.IsDone())
      break; // We must be done!

    nsCOMPtr<nsIDOMNode> nextNode;
    res = iter.CurrentNode(getter_AddRefs(nextNode));
    if (NS_FAILED(res)) return res;
    if (!nextNode) return NS_ERROR_FAILURE;

    // Get node and nextNode's common parent.

    commonAncestor = CommonParent(node, nextNode);

    if (!commonAncestor)
      return NS_ERROR_FAILURE;

    // Find the equivalent of commonAncestor in the cloned tree!

    while (node && node != commonAncestor)
    {
      tmpNode = node;
      res = tmpNode->GetParentNode(getter_AddRefs(node));
      if (NS_FAILED(res)) return res;
      if (!node) return NS_ERROR_FAILURE;

      tmpNode = clone;
      res = tmpNode->GetParentNode(getter_AddRefs(clone));
      if (NS_FAILED(res)) return res;
      if (!node) return NS_ERROR_FAILURE;
    }

    commonCloneAncestor = clone;
    node = nextNode;
  }

  *aReturn = clonedFrag;
  NS_IF_ADDREF(*aReturn);

  return NS_OK;
}

nsresult nsRange::CloneRange(nsIDOMRange** aReturn)
{
  if(IsDetached())
    return NS_ERROR_DOM_INVALID_STATE_ERR;

  if (aReturn == 0)
    return NS_ERROR_NULL_POINTER;

  nsresult res = NS_NewRange(aReturn);
  if (NS_FAILED(res))
    return res;

  res = (*aReturn)->SetStart(mStartParent, mStartOffset);
  if (NS_FAILED(res))
    return res;
  
  res = (*aReturn)->SetEnd(mEndParent, mEndOffset);
  return res;
}

nsresult nsRange::InsertNode(nsIDOMNode* aN)
{
  if(IsDetached())
    return NS_ERROR_DOM_INVALID_STATE_ERR;

  NS_ENSURE_ARG_POINTER(aN);
 
  nsresult res;
  PRInt32 tStartOffset;
  this->GetStartOffset(&tStartOffset);

  nsCOMPtr<nsIDOMNode> tStartContainer;
  res = this->GetStartContainer(getter_AddRefs(tStartContainer));
  if(NS_FAILED(res)) return res;
  
  PRUint16 tNodeType;
  aN->GetNodeType(&tNodeType);
  if( (nsIDOMNode::CDATA_SECTION_NODE == tNodeType) ||
      (nsIDOMNode::TEXT_NODE == tNodeType) )
  {
    nsCOMPtr<nsIDOMNode> tSCParentNode;
    res = tStartContainer->GetParentNode(getter_AddRefs(tSCParentNode));
    if(NS_FAILED(res)) return res;

    nsCOMPtr<nsIDOMNode> tResultNode;
    return tSCParentNode->InsertBefore(aN, tSCParentNode, getter_AddRefs(tResultNode));
  }  

  nsCOMPtr<nsIDOMNodeList>tChildList;
  res = tStartContainer->GetChildNodes(getter_AddRefs(tChildList));
  if(NS_FAILED(res)) return res;
  PRUint32 tChildListLength;
  res = tChildList->GetLength(&tChildListLength);
  if(NS_FAILED(res)) return res;

  // find the insertion point in the DOM and insert the Node
  if(tStartOffset == (PRInt32)tChildListLength)
  {
    nsCOMPtr<nsIDOMNode> tNode;
    return tStartContainer->AppendChild(aN, getter_AddRefs(tNode));
  }
  else
  {
    nsCOMPtr<nsIDOMNode>tChildNode;
    res = tChildList->Item(tStartOffset, getter_AddRefs(tChildNode));
    if(NS_FAILED(res)) return res;

    nsCOMPtr<nsIDOMNode> tResultNode;
    return tStartContainer->InsertBefore(aN, tChildNode, getter_AddRefs(tResultNode));
  }
}

nsresult nsRange::SurroundContents(nsIDOMNode* aN)
{
  if(IsDetached())
    return NS_ERROR_DOM_INVALID_STATE_ERR;

  NS_ENSURE_ARG_POINTER(aN);
  nsresult res;

  //get start offset, and start container
  PRInt32 tStartOffset;
  this->GetStartOffset(&tStartOffset);
  nsCOMPtr<nsIDOMNode> tStartContainer;
  res = GetStartContainer(getter_AddRefs(tStartContainer));
  if(NS_FAILED(res)) return res;

  //get end offset, and end container
  PRInt32 tEndOffset;
  this->GetEndOffset(&tEndOffset);
  nsCOMPtr<nsIDOMNode> tEndContainer;
  res = GetEndContainer(getter_AddRefs(tEndContainer));
  if(NS_FAILED(res)) return res;

  //prep start
  PRUint16 tStartNodeType;
  tStartContainer->GetNodeType(&tStartNodeType);
  if( (nsIDOMNode::CDATA_SECTION_NODE == tStartNodeType) ||
      (nsIDOMNode::TEXT_NODE == tStartNodeType) )
  {
    nsCOMPtr<nsIDOMText> tStartContainerText = do_QueryInterface(tStartContainer);
    nsCOMPtr<nsIDOMText> tTempText;
    res = tStartContainerText->SplitText(tStartOffset, getter_AddRefs(tTempText));
    if(NS_FAILED(res)) return res;
    tStartOffset = 0;
    tStartContainer = do_QueryInterface(tTempText);
  }

  //prep end
  PRUint16 tEndNodeType;
  tEndContainer->GetNodeType(&tEndNodeType);
  if( (nsIDOMNode::CDATA_SECTION_NODE == tEndNodeType) ||
      (nsIDOMNode::TEXT_NODE == tEndNodeType) )
  {
    nsCOMPtr<nsIDOMText> tEndContainerText = do_QueryInterface(tEndContainer);
    nsCOMPtr<nsIDOMText> tTempText;
    res = tEndContainerText->SplitText(tEndOffset, getter_AddRefs(tTempText));
    if(NS_FAILED(res)) return res;

    tEndContainer = do_QueryInterface(tTempText);
  }

  //get ancestor info
  nsCOMPtr<nsIDOMNode> tAncestorContainer;
  this->GetCommonAncestorContainer(getter_AddRefs(tAncestorContainer));

  PRUint16 tCommonAncestorType;
  tAncestorContainer->GetNodeType(&tCommonAncestorType);

  nsCOMPtr<nsIDOMNode>tempNode;
  nsCOMPtr<nsIDOMNode>tRangeContentsNode;
  nsCOMPtr<nsIDOMDocument> document;
  res = mStartParent->GetOwnerDocument(getter_AddRefs(document));
  if (NS_FAILED(res)) return res;

  // Create a new document fragment in the context of this document,
  // which might be null
  nsCOMPtr<nsIDOMDocumentFragment> docfrag;

  nsCOMPtr<nsIDocument> doc(do_QueryInterface(document));

  res = NS_NewDocumentFragment(getter_AddRefs(docfrag), doc);
  if (NS_FAILED(res)) return res;

  res = this->ExtractContents(getter_AddRefs(docfrag));
  if (NS_FAILED(res)) return res;

  tRangeContentsNode = do_QueryInterface(docfrag);
  aN->AppendChild(tRangeContentsNode, getter_AddRefs(tempNode));

  if( (nsIDOMNode::CDATA_SECTION_NODE == tCommonAncestorType) ||
      (nsIDOMNode::TEXT_NODE == tCommonAncestorType) )
  {//easy stuff here
    this->InsertNode(aN);
  }
  else
  {//hard stuff here
    nsCOMPtr<nsIDOMNodeList>tChildList;
    res = tAncestorContainer->GetChildNodes(getter_AddRefs(tChildList));
    PRUint32 i,tNumChildren;
    tChildList->GetLength(&tNumChildren);

    PRBool tFound = PR_FALSE;
    PRInt16 tResult;
    for(i = 0; (i < tNumChildren && !tFound); i++)
    {
      ComparePoint(tAncestorContainer, i, &tResult);
      if(tResult == 0)
      {
        tFound = PR_TRUE;
        break;
      }
    }

    if(tFound)
    {
      nsCOMPtr<nsIDOMNode> tChild;
      tChildList->Item(i-1, getter_AddRefs(tChild));
      tAncestorContainer->InsertBefore(aN, tChild, getter_AddRefs(tempNode));
    }
    else // there is an error this may need to be updated later
      this->InsertNode(aN);

    // re-define the range so that it contains the same content as it did before
    tEndContainer->GetNodeType(&tEndNodeType);
    if( (nsIDOMNode::CDATA_SECTION_NODE == tEndNodeType) ||
       (nsIDOMNode::TEXT_NODE == tEndNodeType) )
    {
      nsCOMPtr<nsIDOMText> tEndContainerText = do_QueryInterface(tEndContainer);
      PRUint32 tInt;
      tEndContainerText->GetLength(&tInt);
      tEndOffset = tInt;
    }
    else
    {
      nsCOMPtr<nsIDOMNodeList>tChildList;
      res = tEndContainer->GetChildNodes(getter_AddRefs(tChildList));
      PRUint32 tInt;
      tChildList->GetLength(&tInt);
      tEndOffset = tInt;
    }
    this->DoSetRange(tStartContainer, 0, tEndContainer, tEndOffset);
  }
  this->SelectNode(aN);
  return NS_OK;
}

nsresult nsRange::ToString(nsAString& aReturn)
{ 
  if(IsDetached())
    return NS_ERROR_DOM_INVALID_STATE_ERR;

  nsCOMPtr<nsIContent> cStart( do_QueryInterface(mStartParent) );
  nsCOMPtr<nsIContent> cEnd( do_QueryInterface(mEndParent) );
  
  // clear the string
  aReturn.Truncate();
  
  // If we're unpositioned, return the empty string
  if (!cStart || !cEnd) {
    return NS_OK;
  }

#ifdef DEBUG_range
      printf("Range dump: -----------------------\n");
#endif /* DEBUG */
    
  // effeciency hack for simple case
  if (cStart == cEnd)
  {
    nsCOMPtr<nsIDOMText> textNode( do_QueryInterface(mStartParent) );
    
    if (textNode)
    {
#ifdef DEBUG_range
      // If debug, dump it:
      nsCOMPtr<nsIContent> cN (do_QueryInterface(mStartParent));
      if (cN) cN->List(stdout);
      printf("End Range dump: -----------------------\n");
#endif /* DEBUG */

      // grab the text
      if (NS_FAILED(textNode->SubstringData(mStartOffset,mEndOffset-mStartOffset,aReturn)))
        return NS_ERROR_UNEXPECTED;
      return NS_OK;
    }
  } 
  
  /* complex case: cStart != cEnd, or cStart not a text node
     revisit - there are potential optimizations here and also tradeoffs.
  */

  nsCOMPtr<nsIContentIterator> iter;
  NS_NewContentIterator(getter_AddRefs(iter));
  iter->Init(this);
  
  nsString tempString;
  nsCOMPtr<nsIContent> cN;
 
  // loop through the content iterator, which returns nodes in the range in 
  // close tag order, and grab the text from any text node
  iter->CurrentNode(getter_AddRefs(cN));
  while (cN && (NS_ENUMERATOR_FALSE == iter->IsDone()))
  {
#ifdef DEBUG_range
    // If debug, dump it:
    cN->List(stdout);
#endif /* DEBUG */
    nsCOMPtr<nsIDOMText> textNode( do_QueryInterface(cN) );
    if (textNode) // if it's a text node, get the text
    {
      if (cN == cStart) // only include text past start offset
      {
        PRUint32 strLength;
        textNode->GetLength(&strLength);
        textNode->SubstringData(mStartOffset,strLength-mStartOffset,tempString);
        aReturn += tempString;
      }
      else if (cN == cEnd)  // only include text before end offset
      {
        textNode->SubstringData(0,mEndOffset,tempString);
        aReturn += tempString;
      }
      else  // grab the whole kit-n-kaboodle
      {
        textNode->GetData(tempString);
        aReturn += tempString;
      }
    }
    nsresult res = iter->Next();
    if (NS_FAILED(res)) // a little noise here to catch bugs
    {
      NS_NOTREACHED("nsRange::ToString() : iterator failed to advance");
      return res;
    }
    iter->CurrentNode(getter_AddRefs(cN));
  }

#ifdef DEBUG_range
  printf("End Range dump: -----------------------\n");
#endif /* DEBUG */
  return NS_OK;
}



nsresult
nsRange::Detach()
{
  if(IsDetached())
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  mIsDetached = PR_TRUE;
  return DoSetRange(nsnull,0,nsnull,0);
}



nsresult nsRange::OwnerGone(nsIContent* aDyingNode)
{
  // nothing for now - should be impossible to getter here
  // No node should be deleted if it holds a range endpoint,
  // since the range endpoint addrefs the node.
  NS_ASSERTION(PR_FALSE,"Deleted content holds a range endpoint");  
  return NS_OK;
}
  

nsresult nsRange::OwnerChildInserted(nsIContent* aParentNode, PRInt32 aOffset)
{
  // sanity check - null nodes shouldn't have enclosed ranges
  if (!aParentNode) return NS_ERROR_UNEXPECTED;

  nsCOMPtr<nsIContent> parent( do_QueryInterface(aParentNode) );
  // quick return if no range list
  nsVoidArray *theRangeList;
  parent->GetRangeList(theRangeList);
  if (!theRangeList) return NS_OK;
  
  nsCOMPtr<nsIDOMNode> domNode;
  nsresult res;
  
  res = GetDOMNodeFromContent(parent, address_of(domNode));
  if (NS_FAILED(res))  return res;
  if (!domNode) return NS_ERROR_UNEXPECTED;


  PRInt32   count = theRangeList->Count();
  for (PRInt32 loop = 0; loop < count; loop++)
  {
    nsRange* theRange = NS_STATIC_CAST(nsRange*, (theRangeList->ElementAt(loop))); 
    NS_ASSERTION(theRange, "oops, no range");

    // sanity check - do range and content agree over ownership?
    res = theRange->ContentOwnsUs(domNode);
    NS_PRECONDITION(NS_SUCCEEDED(res), "range and content disagree over range ownership");
    if (NS_SUCCEEDED(res))
    {
      if (theRange->mStartParent == domNode)
      {
        // if child inserted before start, move start offset right one
        if (aOffset < theRange->mStartOffset) theRange->mStartOffset++;
      }
      if (theRange->mEndParent == domNode)
      {
        // if child inserted before end, move end offset right one
        if (aOffset < theRange->mEndOffset) theRange->mEndOffset++;
      }
      NS_PRECONDITION(NS_SUCCEEDED(res), "error updating range list");
    }
  }
  return NS_OK;
}
  

nsresult nsRange::OwnerChildRemoved(nsIContent* aParentNode, PRInt32 aOffset, nsIContent* aRemovedNode)
{
  // sanity check - null nodes shouldn't have enclosed ranges
  if (!aParentNode) return NS_ERROR_UNEXPECTED;

  nsCOMPtr<nsIContent> parent( do_QueryInterface(aParentNode) );
  nsCOMPtr<nsIContent> removed( do_QueryInterface(aRemovedNode) );

  // any ranges in the content subtree rooted by aRemovedNode need to
  // have the enclosed endpoints promoted up to the parentNode/offset
  nsCOMPtr<nsIDOMNode> domNode;
  nsresult res = GetDOMNodeFromContent(parent, address_of(domNode));
  if (NS_FAILED(res))  return res;
  if (!domNode) return NS_ERROR_UNEXPECTED;
  res = PopRanges(domNode, aOffset, removed);

  // quick return if no range list
  nsVoidArray *theRangeList;
  parent->GetRangeList(theRangeList);
  if (!theRangeList) return NS_OK;
  
  PRInt32   count = theRangeList->Count();
  for (PRInt32 loop = 0; loop < count; loop++)
  {
    nsRange* theRange = NS_STATIC_CAST(nsRange*, (theRangeList->ElementAt(loop))); 
    NS_ASSERTION(theRange, "oops, no range");

    // sanity check - do range and content agree over ownership?
    res = theRange->ContentOwnsUs(domNode);
    NS_PRECONDITION(NS_SUCCEEDED(res), "range and content disagree over range ownership");
    if (NS_SUCCEEDED(res))
    {
      if (theRange->mStartParent == domNode)
      {
        // if child deleted before start, move start offset left one
        if (aOffset < theRange->mStartOffset) theRange->mStartOffset--;
      }
      if (theRange->mEndParent == domNode)
      {
        // if child deleted before end, move end offset left one
        if (aOffset < theRange->mEndOffset) 
        {
          if (theRange->mEndOffset>0) theRange->mEndOffset--;
        }
      }
    }
  }
  
  return NS_OK;
}
  

nsresult nsRange::OwnerChildReplaced(nsIContent* aParentNode, PRInt32 aOffset, nsIContent* aReplacedNode)
{
  // sanity check - null nodes shouldn't have enclosed ranges
  if (!aParentNode) return NS_ERROR_UNEXPECTED;

  // don't need to adjust ranges whose endpoints are in this parent,
  // but we do need to pop out any range endpoints inside the subtree
  // rooted by aReplacedNode.
  
  nsCOMPtr<nsIContent> parent( do_QueryInterface(aParentNode) );
  nsCOMPtr<nsIContent> replaced( do_QueryInterface(aReplacedNode) );
  nsCOMPtr<nsIDOMNode> parentDomNode; 
  nsresult res;
  
  res = GetDOMNodeFromContent(parent, address_of(parentDomNode));
  if (NS_FAILED(res))  return res;
  if (!parentDomNode) return NS_ERROR_UNEXPECTED;
  
  res = PopRanges(parentDomNode, aOffset, replaced);
  
  return res;
}
  

nsresult nsRange::TextOwnerChanged(nsIContent* aTextNode, PRInt32 aStartChanged, PRInt32 aEndChanged, PRInt32 aReplaceLength)
{
  // sanity check - null nodes shouldn't have enclosed ranges
  if (!aTextNode) return NS_ERROR_UNEXPECTED;

  nsCOMPtr<nsIContent> textNode( do_QueryInterface(aTextNode) );
  nsVoidArray *theRangeList;
  aTextNode->GetRangeList(theRangeList);
  // the caller already checked to see if there was a range list
  
  nsCOMPtr<nsIDOMNode> domNode;
  nsresult res;
  
  res = GetDOMNodeFromContent(textNode, address_of(domNode));
  if (NS_FAILED(res))  return res;
  if (!domNode) return NS_ERROR_UNEXPECTED;

  PRInt32   count = theRangeList->Count();
  for (PRInt32 loop = 0; loop < count; loop++)
  {
    nsRange* theRange = NS_STATIC_CAST(nsRange*, (theRangeList->ElementAt(loop))); 
    NS_ASSERTION(theRange, "oops, no range");

    // sanity check - do range and content agree over ownership?
    res = theRange->ContentOwnsUs(domNode);
    NS_PRECONDITION(NS_SUCCEEDED(res), "range and content disagree over range ownership");
    if (NS_SUCCEEDED(res))
    { 
      PRBool bStartPointInChangedText = PR_FALSE;
      
      if (theRange->mStartParent == domNode)
      {
        // if range start is inside changed text, position it after change
        if ((aStartChanged <= theRange->mStartOffset) && (aEndChanged >= theRange->mStartOffset))
        { 
          theRange->mStartOffset = aStartChanged+aReplaceLength;
          bStartPointInChangedText = PR_TRUE;
        }
        // else if text changed before start, adjust start offset
        else if (aEndChanged <= theRange->mStartOffset) 
          theRange->mStartOffset += aStartChanged + aReplaceLength - aEndChanged;
      }
      if (theRange->mEndParent == domNode)
      {
        // if range end is inside changed text, position it before change
        if ((aStartChanged <= theRange->mEndOffset) && (aEndChanged >= theRange->mEndOffset)) 
        {
          theRange->mEndOffset = aStartChanged;
          // hack: if BOTH range endpoints were inside the change, then they
          // both get collapsed to the beginning of the change.  
          if (bStartPointInChangedText) theRange->mStartOffset = aStartChanged;
        }
        // else if text changed before end, adjust end offset
        else if (aEndChanged <= theRange->mEndOffset) 
          theRange->mEndOffset += aStartChanged + aReplaceLength - aEndChanged;
      }
    }
  }
  
  return NS_OK;
}


// nsIDOMNSRange interface
NS_IMETHODIMP    
nsRange::CreateContextualFragment(const nsAString& aFragment, 
                                  nsIDOMDocumentFragment** aReturn)
{
  nsresult result = NS_OK;
  nsCOMPtr<nsIParser> parser;
  nsVoidArray tagStack;

  if (!mIsPositioned) {
    return NS_ERROR_FAILURE;
  }

  // Create a new parser for this entire operation
  result = nsComponentManager::CreateInstance(kCParserCID, 
                                              nsnull, 
                                              NS_GET_IID(nsIParser), 
                                              (void **)getter_AddRefs(parser));
  if (NS_SUCCEEDED(result)) {

    nsCOMPtr<nsIDOMNode> parent;
    nsCOMPtr<nsIContent> content(do_QueryInterface(mStartParent, &result));

    if (NS_SUCCEEDED(result)) {
      nsCOMPtr<nsIDocument> document;
      nsCOMPtr<nsIDOMDocument> domDocument;
      
      result = content->GetDocument(*getter_AddRefs(document));
      if (document && NS_SUCCEEDED(result)) {
        domDocument = do_QueryInterface(document, &result);
      }

      parent = mStartParent;
      while (parent && 
             (parent != domDocument) && 
             NS_SUCCEEDED(result)) {
        nsCOMPtr<nsIDOMNode> temp;
        nsAutoString tagName;
        PRUnichar* name = nsnull;
        PRUint16 nodeType;
        
        parent->GetNodeType(&nodeType);
        if (nsIDOMNode::ELEMENT_NODE == nodeType) {
          parent->GetNodeName(tagName);
          // XXX Wish we didn't have to allocate here
          name = ToNewUnicode(tagName);
          if (name) {
            tagStack.AppendElement(name);
            temp = parent;
            result = temp->GetParentNode(getter_AddRefs(parent));
          }
          else {
            result = NS_ERROR_OUT_OF_MEMORY;
          }
        }
        else {
          temp = parent;
          result = temp->GetParentNode(getter_AddRefs(parent));
        }
      }
      
      if (NS_SUCCEEDED(result)) {
        nsCAutoString contentType;
        nsIHTMLFragmentContentSink* sink;
        
        result = NS_NewHTMLFragmentContentSink(&sink);
        if (NS_SUCCEEDED(result)) {
          parser->SetContentSink(sink);
          nsCOMPtr<nsIDOMNSDocument> domnsDocument(do_QueryInterface(document));
          if (domnsDocument) {
            nsAutoString buf;
            domnsDocument->GetContentType(buf);
            CopyUCS2toASCII(buf, contentType);
          }
          else {
            // Who're we kidding. This only works for html.
            contentType = NS_LITERAL_CSTRING("text/html");
          }

          // If there's no JS or system JS running,
          // push the current document's context on the JS context stack
          // so that event handlers in the fragment do not get 
          // compiled with the system principal.
          nsCOMPtr<nsIJSContextStack> ContextStack;
          nsCOMPtr<nsIScriptSecurityManager> secMan;
          secMan = do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &result);
          if (document && NS_SUCCEEDED(result)) {
            nsCOMPtr<nsIPrincipal> sysPrin;
            nsCOMPtr<nsIPrincipal> subjectPrin;

            // Just to compare, not to use!
            result = secMan->GetSystemPrincipal(getter_AddRefs(sysPrin));
            if (NS_SUCCEEDED(result))
                result = secMan->GetSubjectPrincipal(getter_AddRefs(subjectPrin));
            // If there's no subject principal, there's no JS running, so we're in system code.
            // (just in case...null subject principal will probably never happen)
            if (NS_SUCCEEDED(result) &&
               (!subjectPrin || sysPrin.get() == subjectPrin.get())) {
              nsCOMPtr<nsIScriptGlobalObject> globalObj;
              result = document->GetScriptGlobalObject(getter_AddRefs(globalObj));

              nsCOMPtr<nsIScriptContext> scriptContext;
              if (NS_SUCCEEDED(result) && globalObj) {
                result = globalObj->GetContext(getter_AddRefs(scriptContext));
              }

              JSContext* cx = nsnull;
              if (NS_SUCCEEDED(result) && scriptContext) {
                cx = (JSContext*)scriptContext->GetNativeContext();
              }

              if(cx) {
                ContextStack = do_GetService("@mozilla.org/js/xpc/ContextStack;1", &result);
                if(NS_SUCCEEDED(result)) {
                  result = ContextStack->Push(cx);
                }
              }
            }
          }
          
          nsDTDMode mode = eDTDMode_autodetect;
          nsCOMPtr<nsIDOMDocument> ownerDoc;
          mStartParent->GetOwnerDocument(getter_AddRefs(ownerDoc));
          nsCOMPtr<nsIHTMLDocument> htmlDoc(do_QueryInterface(ownerDoc));
          if (htmlDoc) {
            htmlDoc->GetDTDMode(mode);
          }
          result = parser->ParseFragment(aFragment, (void*)0,
                                         tagStack,
                                         0, contentType, mode);

          if (ContextStack) {
            JSContext *notused;
            ContextStack->Pop(&notused);
          }
          
          if (NS_SUCCEEDED(result)) {
            sink->GetFragment(aReturn);
          }
          
          NS_RELEASE(sink);
        }
      }
    }
      
    // XXX Ick! Delete strings we allocated above.
    PRInt32 count = tagStack.Count();
    for (PRInt32 i = 0; i < count; i++) {
      PRUnichar* str = (PRUnichar*)tagStack.ElementAt(i);
      if (str) {
        nsCRT::free(str);
      }
    }
  }

  return result;
}

NS_IMETHODIMP
nsRange::GetHasGeneratedBefore(PRBool *aBool)
{
  NS_ENSURE_ARG_POINTER(aBool);
  *aBool = mBeforeGenContent;
  return NS_OK;
}

NS_IMETHODIMP    
nsRange::GetHasGeneratedAfter(PRBool *aBool)
{
  NS_ENSURE_ARG_POINTER(aBool);
  *aBool = mAfterGenContent;
  return NS_OK;
}

NS_IMETHODIMP    
nsRange::SetHasGeneratedBefore(PRBool aBool)
{
  mBeforeGenContent = aBool;
  return NS_OK;
}

NS_IMETHODIMP    
nsRange::SetHasGeneratedAfter(PRBool aBool)
{
  mAfterGenContent = aBool;
  return NS_OK;
}

NS_IMETHODIMP    
nsRange::SetBeforeAndAfter(PRBool aBefore, PRBool aAfter)
{
  mBeforeGenContent = aBefore;
  mBeforeGenContent = aAfter;
  return NS_OK;
}

nsresult
nsRange::Lock()
{
  if (!mMonitor)
    mMonitor = ::PR_NewMonitor();

  if (mMonitor)
    PR_EnterMonitor(mMonitor);

  return NS_OK;
}

nsresult
nsRange::Unlock()
{
  if (mMonitor)
    PR_ExitMonitor(mMonitor);

  return NS_OK;
}

