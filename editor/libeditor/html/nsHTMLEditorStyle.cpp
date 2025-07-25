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
 *   Daniel Glazman <glazman@netscape.com>
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
#include "nsICaret.h"

#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"

#include "nsHTMLEditor.h"
#include "nsHTMLEditRules.h"
#include "nsTextEditUtils.h"
#include "nsHTMLEditUtils.h"

#include "nsEditorEventListeners.h"

#include "nsIDOMText.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMDocument.h"
#include "nsIDOMAttr.h"
#include "nsIDocument.h"
#include "nsIDOMEventReceiver.h" 
#include "nsIDOMKeyEvent.h"
#include "nsIDOMKeyListener.h" 
#include "nsIDOMMouseListener.h"
#include "nsIDOMMouseEvent.h"
#include "nsISelection.h"
#include "nsISelectionPrivate.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsISelectionController.h"

#include "nsICSSLoader.h"
#include "nsICSSStyleSheet.h"
#include "nsIHTMLContentContainer.h"
#include "nsIDocumentObserver.h"
#include "nsIDocumentStateListener.h"

#include "TypeInState.h"

#include "nsIEnumerator.h"
#include "nsIContent.h"
#include "nsIContentIterator.h"
#include "nsEditorCID.h"
#include "nsLayoutCID.h"
#include "nsIDOMRange.h"
#include "nsIDOMNSRange.h"
#include "nsISupportsArray.h"
#include "nsVoidArray.h"
#include "nsFileSpec.h"
#include "nsIFile.h"
#include "nsIURL.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsWidgetsCID.h"
#include "nsIDocumentEncoder.h"
#include "nsIDOMDocumentFragment.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIParser.h"
#include "nsParserCIID.h"
#include "nsIImage.h"
#include "nsAOLCiter.h"
#include "nsInternetCiter.h"
#include "nsISupportsPrimitives.h"
#include "InsertTextTxn.h"
#include "ChangeCSSInlineStyleTxn.h"

// Transactionas
#include "PlaceholderTxn.h"
#include "nsStyleSheetTxns.h"

// Misc
#include "nsEditorUtils.h"

static NS_DEFINE_CID(kCContentIteratorCID, NS_CONTENTITERATOR_CID);
static NS_DEFINE_IID(kSubtreeIteratorCID, NS_SUBTREEITERATOR_CID);

#if defined(NS_DEBUG) && defined(DEBUG_buster)
static PRBool gNoisy = PR_FALSE;
#else
static const PRBool gNoisy = PR_FALSE;
#endif

// Add the CSS style corresponding to the HTML inline style defined
// by aProperty aAttribute and aValue to the selection
NS_IMETHODIMP nsHTMLEditor::SetCSSInlineProperty(nsIAtom *aProperty, 
                            const nsAString & aAttribute, 
                            const nsAString & aValue)
{
  nsresult res = NS_OK;
  PRBool useCSS;

  GetIsCSSEnabled(&useCSS);
  if (useCSS) {
    res = SetInlineProperty(aProperty, aAttribute, aValue);
  }
  return res;
}

NS_IMETHODIMP nsHTMLEditor::SetInlineProperty(nsIAtom *aProperty, 
                            const nsAString & aAttribute, 
                            const nsAString & aValue)
{
  if (!aProperty) { return NS_ERROR_NULL_POINTER; }
  if (!mRules) { return NS_ERROR_NOT_INITIALIZED; }
  ForceCompositionEnd();

  nsresult res;
  nsCOMPtr<nsISelection>selection;
  res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsISelectionPrivate> selPriv(do_QueryInterface(selection));

  PRBool isCollapsed;
  selection->GetIsCollapsed(&isCollapsed);
  if (isCollapsed)
  {
    // manipulating text attributes on a collapsed selection only sets state for the next text insertion
    nsString tAttr(aAttribute);//MJUDGE SCC NEED HELP
    nsString tVal(aValue);//MJUDGE SCC NEED HELP
    return mTypeInState->SetProp(aProperty, tAttr, tVal);
  }
  
  nsAutoEditBatch batchIt(this);
  nsAutoRules beginRulesSniffing(this, kOpInsertElement, nsIEditor::eNext);
  nsAutoSelectionReset selectionResetter(selection, this);
  nsAutoTxnsConserveSelection dontSpazMySelection(this);
  
  PRBool cancel, handled;
  nsTextRulesInfo ruleInfo(nsTextEditRules::kSetTextProperty);
  res = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  if (NS_FAILED(res)) return res;
  if (!cancel && !handled)
  {
    // get selection range enumerator
    nsCOMPtr<nsIEnumerator> enumerator;
    res = selPriv->GetEnumerator(getter_AddRefs(enumerator));
    if (NS_FAILED(res)) return res;
    if (!enumerator)    return NS_ERROR_FAILURE;

    // loop thru the ranges in the selection
    enumerator->First(); 
    nsCOMPtr<nsISupports> currentItem;
    while ((NS_ENUMERATOR_FALSE == enumerator->IsDone()))
    {
      res = enumerator->CurrentItem(getter_AddRefs(currentItem));
      if (NS_FAILED(res)) return res;
      if (!currentItem)   return NS_ERROR_FAILURE;
      
      nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );

      // adjust range to include any ancestors who's children are entirely selected
      res = PromoteInlineRange(range);
      if (NS_FAILED(res)) return res;
      
      // check for easy case: both range endpoints in same text node
      nsCOMPtr<nsIDOMNode> startNode, endNode;
      res = range->GetStartContainer(getter_AddRefs(startNode));
      if (NS_FAILED(res)) return res;
      res = range->GetEndContainer(getter_AddRefs(endNode));
      if (NS_FAILED(res)) return res;
      if ((startNode == endNode) && IsTextNode(startNode))
      {
        PRInt32 startOffset, endOffset;
        range->GetStartOffset(&startOffset);
        range->GetEndOffset(&endOffset);
        nsCOMPtr<nsIDOMCharacterData> nodeAsText = do_QueryInterface(startNode);
        res = SetInlinePropertyOnTextNode(nodeAsText, startOffset, endOffset, aProperty, &aAttribute, &aValue);
        if (NS_FAILED(res)) return res;
      }
      else
      {
        // not the easy case.  range not contained in single text node. 
        // there are up to three phases here.  There are all the nodes
        // reported by the subtree iterator to be processed.  And there
        // are potentially a starting textnode and an ending textnode
        // which are only partially contained by the range.
        
        // lets handle the nodes reported by the iterator.  These nodes
        // are entirely contained in the selection range.  We build up
        // a list of them (since doing operations on the document during
        // iteration would perturb the iterator).

        nsCOMPtr<nsIContentIterator> iter;
        res = nsComponentManager::CreateInstance(kSubtreeIteratorCID, nsnull,
                                                  NS_GET_IID(nsIContentIterator), 
                                                  getter_AddRefs(iter));
        if (NS_FAILED(res)) return res;
        if (!iter)          return NS_ERROR_FAILURE;

        nsCOMPtr<nsISupportsArray> arrayOfNodes;
        nsCOMPtr<nsIContent> content;
        nsCOMPtr<nsIDOMNode> node;
        nsCOMPtr<nsISupports> isupports;
        
        // make a array
        res = NS_NewISupportsArray(getter_AddRefs(arrayOfNodes));
        if (NS_FAILED(res)) return res;
        
        // iterate range and build up array
        res = iter->Init(range);
        // init returns an error if no nodes in range.
        // this can easily happen with the subtree 
        // iterator if the selection doesn't contain
        // any *whole* nodes.
        if (NS_SUCCEEDED(res))
        {
          while (NS_ENUMERATOR_FALSE == iter->IsDone())
          {
            res = iter->CurrentNode(getter_AddRefs(content));
            if (NS_FAILED(res)) return res;
            node = do_QueryInterface(content);
            if (!node) return NS_ERROR_FAILURE;
            if (IsEditable(node))
            { 
              isupports = do_QueryInterface(node);
              arrayOfNodes->AppendElement(isupports);
            }
            res = iter->Next();
            if (NS_FAILED(res)) return res;
          }
        }
        // first check the start parent of the range to see if it needs to 
        // be seperately handled (it does if it's a text node, due to how the
        // subtree iterator works - it will not have reported it).
        if (IsTextNode(startNode) && IsEditable(startNode))
        {
          nsCOMPtr<nsIDOMCharacterData> nodeAsText = do_QueryInterface(startNode);
          PRInt32 startOffset;
          PRUint32 textLen;
          range->GetStartOffset(&startOffset);
          nodeAsText->GetLength(&textLen);
          res = SetInlinePropertyOnTextNode(nodeAsText, startOffset, textLen, aProperty, &aAttribute, &aValue);
          if (NS_FAILED(res)) return res;
        }
        
        // then loop through the list, set the property on each node
        PRUint32 listCount;
        PRUint32 j;
        arrayOfNodes->Count(&listCount);
        for (j = 0; j < listCount; j++)
        {
          isupports = dont_AddRef(arrayOfNodes->ElementAt(0));
          node = do_QueryInterface(isupports);
          res = SetInlinePropertyOnNode(node, aProperty, &aAttribute, &aValue);
          if (NS_FAILED(res)) return res;
          arrayOfNodes->RemoveElementAt(0);
        }
        
        // last check the end parent of the range to see if it needs to 
        // be seperately handled (it does if it's a text node, due to how the
        // subtree iterator works - it will not have reported it).
        if (IsTextNode(endNode) && IsEditable(endNode))
        {
          nsCOMPtr<nsIDOMCharacterData> nodeAsText = do_QueryInterface(endNode);
          PRInt32 endOffset;
          range->GetEndOffset(&endOffset);
          res = SetInlinePropertyOnTextNode(nodeAsText, 0, endOffset, aProperty, &aAttribute, &aValue);
          if (NS_FAILED(res)) return res;
        }
      }
      enumerator->Next();
    }
  }
  if (!cancel)
  {
    // post-process
    res = mRules->DidDoAction(selection, &ruleInfo, res);
  }
  return res;
}



nsresult
nsHTMLEditor::SetInlinePropertyOnTextNode( nsIDOMCharacterData *aTextNode, 
                                            PRInt32 aStartOffset,
                                            PRInt32 aEndOffset,
                                            nsIAtom *aProperty, 
                                            const nsAString *aAttribute,
                                            const nsAString *aValue)
{
  if (!aTextNode) return NS_ERROR_NULL_POINTER;
  nsresult res = NS_OK;
  nsCOMPtr<nsIDOMNode> parent;
  res = aTextNode->GetParentNode(getter_AddRefs(parent));
  if (NS_FAILED(res)) return res;
  nsAutoString tagString;
  aProperty->ToString(tagString);
  if (!CanContainTag(parent, tagString)) return NS_OK;
  
  // dont need to do anything if no characters actually selected
  if (aStartOffset == aEndOffset) return NS_OK;
  
  nsCOMPtr<nsIDOMNode> tmp, node = do_QueryInterface(aTextNode);
  
  // dont need to do anything if property already set on node
  PRBool bHasProp;
  nsCOMPtr<nsIDOMNode> styleNode;
  IsTextPropertySetByContent(node, aProperty, aAttribute, aValue, bHasProp, getter_AddRefs(styleNode));
  if (bHasProp) return NS_OK;
  
  // do we need to split the text node?
  PRUint32 textLen;
  aTextNode->GetLength(&textLen);
  
  if ( (PRUint32)aEndOffset != textLen )
  {
    // we need to split off back of text node
    res = SplitNode(node, aEndOffset, getter_AddRefs(tmp));
    if (NS_FAILED(res)) return res;
    node = tmp;  // remember left node
  }
  if ( aStartOffset )
  {
    // we need to split off front of text node
    res = SplitNode(node, aStartOffset, getter_AddRefs(tmp));
    if (NS_FAILED(res)) return res;
  }
  
  // look for siblings that are correct type of node
  nsCOMPtr<nsIDOMNode> sibling;
  GetPriorHTMLSibling(node, address_of(sibling));
  if (sibling && NodeIsType(sibling, aProperty) &&         
      HasAttrVal(sibling, aAttribute, aValue) &&
      IsOnlyAttribute(sibling, aAttribute) )
  {
    // previous sib is already right kind of inline node; slide this over into it
    res = MoveNode(node, sibling, -1);
    return res;
  }
  sibling = nsnull;
  GetNextHTMLSibling(node, address_of(sibling));
  if (sibling && NodeIsType(sibling, aProperty) &&         
      HasAttrVal(sibling, aAttribute, aValue) &&
      IsOnlyAttribute(sibling, aAttribute) )
  {
    // following sib is already right kind of inline node; slide this over into it
    res = MoveNode(node, sibling, 0);
    return res;
  }
  
  // reparent the node inside inline node with appropriate {attribute,value}
  res = SetInlinePropertyOnNode(node, aProperty, aAttribute, aValue);
  return res;
}


nsresult
nsHTMLEditor::SetInlinePropertyOnNode( nsIDOMNode *aNode,
                                       nsIAtom *aProperty, 
                                       const nsAString *aAttribute,
                                       const nsAString *aValue)
{
  if (!aNode || !aProperty) return NS_ERROR_NULL_POINTER;

  nsresult res = NS_OK;
  nsCOMPtr<nsIDOMNode> tmp;
  nsAutoString tag;
  aProperty->ToString(tag);
  ToLowerCase(tag);
  
  PRBool useCSS;
  GetIsCSSEnabled(&useCSS);

  if (useCSS) {
    // we are in CSS mode
    if (mHTMLCSSUtils->IsCSSEditableProperty(aNode, aProperty, aAttribute)) {
      // the HTML style defined by aProperty/aAttribute has a CSS equivalence
      // in this implementation for the node aNode
      nsCOMPtr<nsIDOMNode> tmp = aNode;
      if (IsTextNode(tmp)) {
        // we are working on a text node and need to create a span container
        // that will carry the styles
        InsertContainerAbove( aNode, 
                              address_of(tmp), 
                              NS_LITERAL_STRING("span"),
                              nsnull,
                              nsnull);
      }
      nsCOMPtr<nsIDOMElement>element;
      element = do_QueryInterface(tmp);
      // first we have to remove occurences of the same style hint in the
      // children of the aNode
      res = RemoveStyleInside(tmp, aProperty, aAttribute, PR_TRUE);
      if (NS_FAILED(res)) return res;
      PRInt32 count;
      // then we add the css styles corresponding to the HTML style request
      res = mHTMLCSSUtils->SetCSSEquivalentToHTMLStyle(element, aProperty, aAttribute, aValue, &count);
      if (NS_FAILED(res)) return res;
      return res;
    }
  }
  
  // dont need to do anything if property already set on node
  PRBool bHasProp;
  nsCOMPtr<nsIDOMNode> styleNode;
  IsTextPropertySetByContent(aNode, aProperty, aAttribute, aValue, bHasProp, getter_AddRefs(styleNode));
  if (bHasProp) return NS_OK;

  // is it already the right kind of node, but with wrong attribute?
  if (NodeIsType(aNode, aProperty))
  {
    // just set the attribute on it.
    // but first remove any contrary style in it's children.
    res = RemoveStyleInside(aNode, aProperty, aAttribute, PR_TRUE);
    if (NS_FAILED(res)) return res;
    nsCOMPtr<nsIDOMElement> elem = do_QueryInterface(aNode);
    return SetAttribute(elem, *aAttribute, *aValue);
  }
  
  // can it be put inside inline node?
  if (TagCanContain(tag, aNode))
  {
    nsCOMPtr<nsIDOMNode> priorNode, nextNode;
    // is either of it's neighbors the right kind of node?
    GetPriorHTMLSibling(aNode, address_of(priorNode));
    GetNextHTMLSibling(aNode, address_of(nextNode));
    if (priorNode && NodeIsType(priorNode, aProperty) && 
        HasAttrVal(priorNode, aAttribute, aValue)     &&
        IsOnlyAttribute(priorNode, aAttribute) )
    {
      // previous sib is already right kind of inline node; slide this over into it
      res = MoveNode(aNode, priorNode, -1);
    }
    else if (nextNode && NodeIsType(nextNode, aProperty) && 
             HasAttrVal(nextNode, aAttribute, aValue)    &&
             IsOnlyAttribute(priorNode, aAttribute) )
    {
      // following sib is already right kind of inline node; slide this over into it
      res = MoveNode(aNode, nextNode, 0);
    }
    else
    {
      // ok, chuck it in it's very own container
      res = InsertContainerAbove(aNode, address_of(tmp), tag, aAttribute, aValue);
    }
    if (NS_FAILED(res)) return res;
    return RemoveStyleInside(aNode, aProperty, aAttribute);
  }
  // none of the above?  then cycle through the children.
  nsCOMPtr<nsIDOMNodeList> childNodes;
  res = aNode->GetChildNodes(getter_AddRefs(childNodes));
  if (NS_FAILED(res)) return res;
  if (childNodes)
  {
    PRInt32 j;
    PRUint32 childCount;
    childNodes->GetLength(&childCount);
    if (childCount)
    {
      nsCOMPtr<nsISupportsArray> arrayOfNodes;
      nsCOMPtr<nsIDOMNode> node;
      nsCOMPtr<nsISupports> isupports;
      
      // make a array
      res = NS_NewISupportsArray(getter_AddRefs(arrayOfNodes));
      if (NS_FAILED(res)) return res;
      
      // populate the list
      for (j=0 ; j < (PRInt32)childCount; j++)
      {
        nsCOMPtr<nsIDOMNode> childNode;
        res = childNodes->Item(j, getter_AddRefs(childNode));
        if ((NS_SUCCEEDED(res)) && (childNode) && IsEditable(childNode))
        {
          isupports = do_QueryInterface(childNode);
          arrayOfNodes->AppendElement(isupports);
        }
      }
      
      // then loop through the list, set the property on each node
      PRUint32 listCount;
      arrayOfNodes->Count(&listCount);
      for (j = 0; j < (PRInt32)listCount; j++)
      {
        isupports = dont_AddRef(arrayOfNodes->ElementAt(0));
        node = do_QueryInterface(isupports);
        res = SetInlinePropertyOnNode(node, aProperty, aAttribute, aValue);
        if (NS_FAILED(res)) return res;
        arrayOfNodes->RemoveElementAt(0);
      }
    }
  }
  return res;
}


nsresult nsHTMLEditor::SplitStyleAboveRange(nsIDOMRange *inRange, 
                                            nsIAtom *aProperty, 
                                            const nsAString *aAttribute)
{
  if (!inRange) return NS_ERROR_NULL_POINTER;
  nsresult res;
  nsCOMPtr<nsIDOMNode> startNode, endNode, origStartNode;
  PRInt32 startOffset, endOffset, origStartOffset;
  
  res = inRange->GetStartContainer(getter_AddRefs(startNode));
  if (NS_FAILED(res)) return res;
  res = inRange->GetStartOffset(&startOffset);
  if (NS_FAILED(res)) return res;
  res = inRange->GetEndContainer(getter_AddRefs(endNode));
  if (NS_FAILED(res)) return res;
  res = inRange->GetEndOffset(&endOffset);
  if (NS_FAILED(res)) return res;
  
  origStartNode = startNode;
  origStartOffset = startOffset;
  PRBool sameNode = (startNode==endNode);
  
  // split any matching style nodes above the start of range
  res = SplitStyleAbovePoint(address_of(startNode), &startOffset, aProperty, aAttribute);
  if (NS_FAILED(res)) return res;

  if (sameNode && (startNode != origStartNode))
  {
    // our startNode got split.  This changes the offset of the end of our range.
    endOffset -= origStartOffset;
  }
  
  // second verse, same as the first...
  res = SplitStyleAbovePoint(address_of(endNode), &endOffset, aProperty, aAttribute);
  if (NS_FAILED(res)) return res;
  
  // reset the range
  res = inRange->SetStart(startNode, startOffset);
  if (NS_FAILED(res)) return res;
  res = inRange->SetEnd(endNode, endOffset);
  return res;
}

nsresult nsHTMLEditor::SplitStyleAbovePoint(nsCOMPtr<nsIDOMNode> *aNode,
                                           PRInt32 *aOffset,
                                           nsIAtom *aProperty,          // null here means we split all properties
                                           const nsAString *aAttribute,
                                           nsCOMPtr<nsIDOMNode> *outLeftNode,
                                           nsCOMPtr<nsIDOMNode> *outRightNode)
{
  if (!aNode || !*aNode || !aOffset) return NS_ERROR_NULL_POINTER;
  if (outLeftNode)  *outLeftNode  = nsnull;
  if (outRightNode) *outRightNode = nsnull;
  // split any matching style nodes above the node/offset
  nsCOMPtr<nsIDOMNode> parent, tmp = *aNode;
  PRInt32 offset;

  PRBool useCSS;
  GetIsCSSEnabled(&useCSS);

  PRBool isSet;
  while (tmp && !IsBlockNode(tmp))
  {
    isSet = PR_FALSE;
    if (useCSS && mHTMLCSSUtils->IsCSSEditableProperty(tmp, aProperty, aAttribute)) {
      // the HTML style defined by aProperty/aAttribute has a CSS equivalence
      // in this implementation for the node tmp; let's check if it carries those css styles
      nsAutoString firstValue;
      mHTMLCSSUtils->IsCSSEquivalentToHTMLInlineStyleSet(tmp, aProperty, aAttribute,
                                                         isSet, firstValue,
                                                         SPECIFIED_STYLE_TYPE);
    }
    if ( (aProperty && NodeIsType(tmp, aProperty)) ||   // node is the correct inline prop
         (aProperty == nsIEditProperty::href && nsHTMLEditUtils::IsLink(tmp)) ||
                                                        // node is href - test if really <a href=...
         (!aProperty && NodeIsProperty(tmp)) ||         // or node is any prop, and we asked to split them all
         isSet)                                         // or the style is specified in the style attribute
    {
      // found a style node we need to split
      SplitNodeDeep(tmp, *aNode, *aOffset, &offset, PR_FALSE, outLeftNode, outRightNode);
      // reset startNode/startOffset
      tmp->GetParentNode(getter_AddRefs(*aNode));
      *aOffset = offset;
    }
    tmp->GetParentNode(getter_AddRefs(parent));
    tmp = parent;
  }
  return NS_OK;
}

PRBool nsHTMLEditor::NodeIsProperty(nsIDOMNode *aNode)
{
  if (!aNode)               return PR_FALSE;
  if (!IsContainer(aNode))  return PR_FALSE;
  if (!IsEditable(aNode))   return PR_FALSE;
  if (IsBlockNode(aNode))   return PR_FALSE;
  if (NodeIsType(aNode, nsIEditProperty::a)) return PR_FALSE;
  return PR_TRUE;
}

nsresult nsHTMLEditor::RemoveStyleInside(nsIDOMNode *aNode, 
                                   nsIAtom *aProperty,   // null here means remove all properties
                                   const nsAString *aAttribute, 
                                   PRBool aChildrenOnly)
{
  if (!aNode) return NS_ERROR_NULL_POINTER;
  if (IsTextNode(aNode)) return NS_OK;
  nsresult res = NS_OK;

  // first process the children
  nsCOMPtr<nsIDOMNode> child, tmp;
  aNode->GetFirstChild(getter_AddRefs(child));
  while (child)
  {
    // cache next sibling since we might remove child
    child->GetNextSibling(getter_AddRefs(tmp));
    res = RemoveStyleInside(child, aProperty, aAttribute);
    if (NS_FAILED(res)) return res;
    child = tmp;
  }

  // then process the node itself
  if ( !aChildrenOnly && 
        (aProperty && NodeIsType(aNode, aProperty) || // node is prop we asked for
        (aProperty == nsIEditProperty::href && nsHTMLEditUtils::IsLink(aNode)) || // but check for link (<a href=...)
        (aProperty == nsIEditProperty::name && nsHTMLEditUtils::IsNamedAnchor(aNode))) || // and for named anchors
        (!aProperty && NodeIsProperty(aNode)))  // or node is any prop and we asked for that
  {
    // if we weren't passed an attribute, then we want to 
    // remove any matching inlinestyles entirely
    if (!aAttribute || aAttribute->IsEmpty())
    {
      res = RemoveContainer(aNode);
    }
    // otherwise we just want to eliminate the attribute
    else
    {
      if (HasAttr(aNode, aAttribute))
      {
        // if this matching attribute is the ONLY one on the node,
        // then remove the whole node.  Otherwise just nix the attribute.
        if (IsOnlyAttribute(aNode, aAttribute))
        {
          res = RemoveContainer(aNode);
        }
        else
        {
          nsCOMPtr<nsIDOMElement> elem = do_QueryInterface(aNode);
          if (!elem) return NS_ERROR_NULL_POINTER;
          res = RemoveAttribute(elem, *aAttribute);
        }
      }
    }
  }
  else {
    PRBool useCSS;
    GetIsCSSEnabled(&useCSS);

    if (!aChildrenOnly
        && useCSS && mHTMLCSSUtils->IsCSSEditableProperty(aNode, aProperty, aAttribute)) {
      // the HTML style defined by aProperty/aAttribute has a CSS equivalence
      // in this implementation for the node aNode; let's check if it carries those css styles
      nsAutoString propertyValue;
      PRBool isSet;
      mHTMLCSSUtils->IsCSSEquivalentToHTMLInlineStyleSet(aNode, aProperty, aAttribute,
                                                         isSet, propertyValue,
                                                         SPECIFIED_STYLE_TYPE);
      if (isSet) {
        // yes, tmp has the corresponding css declarations in its style attribute
        // let's remove them
        mHTMLCSSUtils->RemoveCSSEquivalentToHTMLStyle(aNode,
                                                      aProperty,
                                                      aAttribute,
                                                      &propertyValue);
        // remove the node if it is a span, if its style attribute is empty or absent,
        // and if it does not have a class nor an id
        nsCOMPtr<nsIDOMElement> element = do_QueryInterface(aNode);
        res = RemoveElementIfNoStyleOrIdOrClass(element, nsIEditProperty::span);
      }
    }
  }  
  if ( aProperty == nsIEditProperty::font &&    // or node is big or small and we are setting font size
       (NodeIsType(aNode, nsIEditProperty::big) || NodeIsType(aNode, nsIEditProperty::small)) &&
       aAttribute->Equals(NS_LITERAL_STRING("size"),nsCaseInsensitiveStringComparator()))       
  {
    res = RemoveContainer(aNode);  // if we are setting font size, remove any nested bigs and smalls
  }
  return res;
}

PRBool nsHTMLEditor::IsOnlyAttribute(nsIDOMNode *aNode, 
                                     const nsAString *aAttribute)
{
  if (!aNode || !aAttribute) return PR_FALSE;  // ooops
  nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);
  if (!content) return PR_FALSE;  // ooops
  
  PRInt32 attrCount, i, nameSpaceID;
  nsCOMPtr<nsIAtom> attrName, prefix;
  content->GetAttrCount(attrCount);
  
  for (i=0; i<attrCount; i++)
  {
    content->GetAttrNameAt(i, nameSpaceID, *getter_AddRefs(attrName),
                           *getter_AddRefs(prefix));
    nsAutoString attrString, tmp;
    if (!attrName) continue;  // ooops
    attrName->ToString(attrString);
    // if it's the attribute we know about, keep looking
    if (attrString.Equals(*aAttribute,nsCaseInsensitiveStringComparator())) continue;
    // if it's a special _moz... attribute, keep looking
    attrString.Left(tmp,4);
    if (tmp.Equals(NS_LITERAL_STRING("_moz"),nsCaseInsensitiveStringComparator())) continue;
    // otherwise, it's another attribute, so return false
    return PR_FALSE;
  }
  // if we made it through all of them without finding a real attribute
  // other than aAttribute, then return PR_TRUE
  return PR_TRUE;
}

PRBool nsHTMLEditor::HasAttr(nsIDOMNode *aNode, 
                             const nsAString *aAttribute)
{
  if (!aNode) return PR_FALSE;
  if (!aAttribute || aAttribute->IsEmpty()) return PR_TRUE;  // everybody has the 'null' attribute
  
  // get element
  nsCOMPtr<nsIDOMElement> elem = do_QueryInterface(aNode);
  if (!elem) return PR_FALSE;
  
  // get attribute node
  nsCOMPtr<nsIDOMAttr> attNode;
  nsresult res = elem->GetAttributeNode(*aAttribute, getter_AddRefs(attNode));
  if ((NS_FAILED(res)) || !attNode) return PR_FALSE;
  return PR_TRUE;
}


PRBool nsHTMLEditor::HasAttrVal(nsIDOMNode *aNode, 
                                const nsAString *aAttribute, 
                                const nsAString *aValue)
{
  if (!aNode) return PR_FALSE;
  if (!aAttribute || aAttribute->IsEmpty()) return PR_TRUE;  // everybody has the 'null' attribute
  
  // get element
  nsCOMPtr<nsIDOMElement> elem = do_QueryInterface(aNode);
  if (!elem) return PR_FALSE;
  
  // get attribute node
  nsCOMPtr<nsIDOMAttr> attNode;
  nsresult res = elem->GetAttributeNode(*aAttribute, getter_AddRefs(attNode));
  if ((NS_FAILED(res)) || !attNode) return PR_FALSE;
  
  // check if attribute has a value
  PRBool isSet;
  attNode->GetSpecified(&isSet);
  // if no value, and that's what we wanted, then return true
  if (!isSet && (!aValue || aValue->IsEmpty())) return PR_TRUE; 
  
  // get attribute value
  nsAutoString attrVal;
  attNode->GetValue(attrVal);
  
  // do values match?
  if (attrVal.Equals(*aValue,nsCaseInsensitiveStringComparator())) return PR_TRUE;
  return PR_FALSE;
}

nsresult nsHTMLEditor::PromoteRangeIfStartsOrEndsInNamedAnchor(nsIDOMRange *inRange)
{
  if (!inRange) return NS_ERROR_NULL_POINTER;
  nsresult res;
  nsCOMPtr<nsIDOMNode> startNode, endNode, parent, tmp;
  PRInt32 startOffset, endOffset, tmpOffset;
  
  res = inRange->GetStartContainer(getter_AddRefs(startNode));
  if (NS_FAILED(res)) return res;
  res = inRange->GetStartOffset(&startOffset);
  if (NS_FAILED(res)) return res;
  res = inRange->GetEndContainer(getter_AddRefs(endNode));
  if (NS_FAILED(res)) return res;
  res = inRange->GetEndOffset(&endOffset);
  if (NS_FAILED(res)) return res;

  tmp = startNode;
  while ( tmp && 
          !nsTextEditUtils::IsBody(tmp) &&
          !nsHTMLEditUtils::IsNamedAnchor(tmp))
  {
    res = GetNodeLocation(tmp, address_of(parent), &tmpOffset);
    if (NS_FAILED(res)) return res;
    tmp = parent;
  }
  if (!tmp) return NS_ERROR_NULL_POINTER;
  if (nsHTMLEditUtils::IsNamedAnchor(tmp))
  {
    res = GetNodeLocation(tmp, address_of(parent), &tmpOffset);
    if (NS_FAILED(res)) return res;
    startNode = parent;
    startOffset = tmpOffset;
  }

  tmp = endNode;
  while ( tmp && 
          !nsTextEditUtils::IsBody(tmp) &&
          !nsHTMLEditUtils::IsNamedAnchor(tmp))
  {
    res = GetNodeLocation(tmp, address_of(parent), &tmpOffset);
    if (NS_FAILED(res)) return res;
    tmp = parent;
  }
  if (!tmp) return NS_ERROR_NULL_POINTER;
  if (nsHTMLEditUtils::IsNamedAnchor(tmp))
  {
    res = GetNodeLocation(tmp, address_of(parent), &tmpOffset);
    if (NS_FAILED(res)) return res;
    endNode = parent;
    endOffset = tmpOffset + 1;
  }

  res = inRange->SetStart(startNode, startOffset);
  if (NS_FAILED(res)) return res;
  res = inRange->SetEnd(endNode, endOffset);
  return res;
}

nsresult nsHTMLEditor::PromoteInlineRange(nsIDOMRange *inRange)
{
  if (!inRange) return NS_ERROR_NULL_POINTER;
  nsresult res;
  nsCOMPtr<nsIDOMNode> startNode, endNode, parent;
  PRInt32 startOffset, endOffset;
  
  res = inRange->GetStartContainer(getter_AddRefs(startNode));
  if (NS_FAILED(res)) return res;
  res = inRange->GetStartOffset(&startOffset);
  if (NS_FAILED(res)) return res;
  res = inRange->GetEndContainer(getter_AddRefs(endNode));
  if (NS_FAILED(res)) return res;
  res = inRange->GetEndOffset(&endOffset);
  if (NS_FAILED(res)) return res;
  
  while ( startNode && 
          !nsTextEditUtils::IsBody(startNode) && 
          IsAtFrontOfNode(startNode, startOffset) )
  {
    res = GetNodeLocation(startNode, address_of(parent), &startOffset);
    if (NS_FAILED(res)) return res;
    startNode = parent;
  }
  if (!startNode) return NS_ERROR_NULL_POINTER;
  
  while ( endNode && 
          !nsTextEditUtils::IsBody(endNode) && 
          IsAtEndOfNode(endNode, endOffset) )
  {
    res = GetNodeLocation(endNode, address_of(parent), &endOffset);
    if (NS_FAILED(res)) return res;
    endNode = parent;
    endOffset++;  // we are AFTER this node
  }
  if (!endNode) return NS_ERROR_NULL_POINTER;
  
  res = inRange->SetStart(startNode, startOffset);
  if (NS_FAILED(res)) return res;
  res = inRange->SetEnd(endNode, endOffset);
  return res;
}

PRBool nsHTMLEditor::IsAtFrontOfNode(nsIDOMNode *aNode, PRInt32 aOffset)
{
  if (!aNode) return PR_FALSE;  // oops
  if (!aOffset) return PR_TRUE;
  
  if (IsTextNode(aNode))
  {
    return PR_FALSE;
  }
  else
  {
    nsCOMPtr<nsIDOMNode> firstNode;
    GetFirstEditableChild(aNode, address_of(firstNode));
    if (!firstNode) return PR_TRUE; 
    PRInt32 offset;
    nsEditor::GetChildOffset(firstNode, aNode, offset);
    if (offset < aOffset) return PR_FALSE;
    return PR_TRUE;
  }
}

PRBool nsHTMLEditor::IsAtEndOfNode(nsIDOMNode *aNode, PRInt32 aOffset)
{
  if (!aNode) return PR_FALSE;  // oops
  PRUint32 len;
  GetLengthOfDOMNode(aNode, len);
  if (aOffset == (PRInt32)len) return PR_TRUE;
  
  if (IsTextNode(aNode))
  {
    return PR_FALSE;
  }
  else
  {
    nsCOMPtr<nsIDOMNode> lastNode;
    GetLastEditableChild(aNode, address_of(lastNode));
    if (!lastNode) return PR_TRUE; 
    PRInt32 offset;
    nsEditor::GetChildOffset(lastNode, aNode, offset);
    if (offset < aOffset) return PR_TRUE;
    return PR_FALSE;
  }
}


nsresult
nsHTMLEditor::GetInlinePropertyBase(nsIAtom *aProperty, 
                             const nsAString *aAttribute,
                             const nsAString *aValue,
                             PRBool *aFirst, 
                             PRBool *aAny, 
                             PRBool *aAll,
                             nsAString *outValue)
{
  if (!aProperty)
    return NS_ERROR_NULL_POINTER;
/*
  if (gNoisy) 
  { 
    nsAutoString propString;
    aProperty->ToString(propString);
    char *propCString = ToNewCString(propString);
    if (gNoisy) { printf("nsTextEditor::GetTextProperty %s\n", propCString); }
    nsCRT::free(propCString);
  }
*/
  nsresult result;
  *aAny=PR_FALSE;
  *aAll=PR_TRUE;
  *aFirst=PR_FALSE;
  PRBool first=PR_TRUE;

  PRBool useCSS;
  GetIsCSSEnabled(&useCSS);

  nsCOMPtr<nsISelection>selection;
  result = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(result)) return result;
  if (!selection) return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsISelectionPrivate> selPriv(do_QueryInterface(selection));

  PRBool isCollapsed;
  selection->GetIsCollapsed(&isCollapsed);
  nsCOMPtr<nsIDOMNode> collapsedNode;
  nsCOMPtr<nsIEnumerator> enumerator;
  result = selPriv->GetEnumerator(getter_AddRefs(enumerator));
  if (NS_FAILED(result)) return result;
  if (!enumerator) return NS_ERROR_NULL_POINTER;

  enumerator->First(); 
  nsCOMPtr<nsISupports> currentItem;
  result = enumerator->CurrentItem(getter_AddRefs(currentItem));
  // XXX: should be a while loop, to get each separate range
  // XXX: ERROR_HANDLING can currentItem be null?
  if ((NS_SUCCEEDED(result)) && currentItem)
  {
    PRBool firstNodeInRange = PR_TRUE; // for each range, set a flag 
    nsCOMPtr<nsIDOMRange> range(do_QueryInterface(currentItem));

    if (isCollapsed)
    {
      range->GetStartContainer(getter_AddRefs(collapsedNode));
      if (!collapsedNode) return NS_ERROR_FAILURE;
      PRBool isSet, theSetting;
      if (aAttribute)
      {
        nsString tString(*aAttribute); //MJUDGE SCC NEED HELP
        nsString tOutString;//MJUDGE SCC NEED HELP
        nsString *tPassString=nsnull;
        if (outValue)
            tPassString = &tOutString;
        mTypeInState->GetTypingState(isSet, theSetting, aProperty, tString, &tOutString);
        if (outValue)
          outValue->Assign(tOutString);
      }
      else
        mTypeInState->GetTypingState(isSet, theSetting, aProperty);
      if (isSet) 
      {
        *aFirst = *aAny = *aAll = theSetting;
        return NS_OK;
      }
      if (!useCSS) {
        nsCOMPtr<nsIDOMNode> resultNode;
        IsTextPropertySetByContent(collapsedNode, aProperty, aAttribute, aValue,
                                   isSet, getter_AddRefs(resultNode), outValue);
        *aFirst = *aAny = *aAll = isSet;
        return NS_OK;
      }
    }

    // non-collapsed selection
    nsCOMPtr<nsIContentIterator> iter;
    iter = do_CreateInstance(kCContentIteratorCID);
    if (!iter) return NS_ERROR_NULL_POINTER;

    iter->Init(range);
    nsCOMPtr<nsIContent> content;
    nsAutoString firstValue, theValue;
    iter->CurrentNode(getter_AddRefs(content));
    while (NS_ENUMERATOR_FALSE == iter->IsDone())
    {
      nsCOMPtr<nsIDOMNode> node = do_QueryInterface(content);

      if (node && nsTextEditUtils::IsBody(node))
        break;

      //if (gNoisy) { printf("  checking node %p\n", content.get()); }
      nsCOMPtr<nsIDOMCharacterData>text;
      text = do_QueryInterface(content);
      
      PRBool skipNode = PR_FALSE;
      
      // just ignore any non-editable nodes
      if (text && !IsEditable(text))
      {
        skipNode = PR_TRUE;
      }
      else if (text)
      {
        if (!isCollapsed && first && firstNodeInRange)
        {
          firstNodeInRange = PR_FALSE;
          PRInt32 startOffset;
          range->GetStartOffset(&startOffset);
          PRUint32 count;
          text->GetLength(&count);
          if (startOffset==(PRInt32)count) 
          {
            //if (gNoisy) { printf("  skipping node %p\n", content.get()); }
            skipNode = PR_TRUE;
          }
        }
      }
      else
      { // handle non-text leaf nodes here
        PRBool canContainChildren;
        content->CanContainChildren(canContainChildren);
        if (canContainChildren)
        {
          //if (gNoisy) { printf("  skipping non-leaf node %p\n", content.get()); }
          skipNode = PR_TRUE;
        }
        else {
          //if (gNoisy) { printf("  testing non-text leaf node %p\n", content.get()); }
        }
      }
      if (!skipNode)
      {
        if (node)
        {
          PRBool isSet = PR_FALSE;
          nsCOMPtr<nsIDOMNode>resultNode;
          if (first)
          {
            if (useCSS &&
                mHTMLCSSUtils->IsCSSEditableProperty(node, aProperty, aAttribute)) {
              // the HTML styles defined by aProperty/aAttribute has a CSS equivalence
              // in this implementation for node; let's check if it carries those css styles
              if (aValue) firstValue.Assign(*aValue);
              mHTMLCSSUtils->IsCSSEquivalentToHTMLInlineStyleSet(node, aProperty, aAttribute,
                                                                 isSet, firstValue,
                                                                 COMPUTED_STYLE_TYPE);
            }
            else {
              IsTextPropertySetByContent(node, aProperty, aAttribute, aValue, isSet,
                                         getter_AddRefs(resultNode), &firstValue);
            }
            *aFirst = isSet;
            first = PR_FALSE;
            if (outValue) *outValue = firstValue;
          }
          else
          {
            if (useCSS &&
                mHTMLCSSUtils->IsCSSEditableProperty(node, aProperty, aAttribute)) {
              // the HTML styles defined by aProperty/aAttribute has a CSS equivalence
              // in this implementation for node; let's check if it carries those css styles
              if (aValue) firstValue.Assign(*aValue);
              mHTMLCSSUtils->IsCSSEquivalentToHTMLInlineStyleSet(node, aProperty, aAttribute,
                                                                 isSet, theValue,
                                                                 COMPUTED_STYLE_TYPE);
            }
            else {
              IsTextPropertySetByContent(node, aProperty, aAttribute, aValue, isSet,
                                         getter_AddRefs(resultNode), &theValue);
            }
            if (firstValue != theValue)
              *aAll = PR_FALSE;
          }
          
          if (isSet) {
            *aAny = PR_TRUE;
          }
          else {
            *aAll = PR_FALSE;
          }
        }
      }
      result = iter->Next();
      if (NS_FAILED(result))  
        break;
      iter->CurrentNode(getter_AddRefs(content));
    }
  }
  if (!*aAny) 
  { // make sure that if none of the selection is set, we don't report all is set
    *aAll = PR_FALSE;
  }
  //if (gNoisy) { printf("  returning first=%d any=%d all=%d\n", *aFirst, *aAny, *aAll); }
  return result;
}


NS_IMETHODIMP nsHTMLEditor::GetInlineProperty(nsIAtom *aProperty, 
                                              const nsAString &aAttribute, 
                                              const nsAString &aValue,
                                              PRBool *aFirst, 
                                              PRBool *aAny, 
                                              PRBool *aAll)
{
  if (!aProperty || !aFirst || !aAny || !aAll)
    return NS_ERROR_NULL_POINTER;
  const nsAString *att = nsnull;
  if (aAttribute.Length())
    att = &aAttribute;
  const nsAString *val = nsnull;
  if (aValue.Length())
    val = &aValue;
  return GetInlinePropertyBase( aProperty, att, val, aFirst, aAny, aAll, nsnull);
}


NS_IMETHODIMP nsHTMLEditor::GetInlinePropertyWithAttrValue(nsIAtom *aProperty, 
                                              const nsAString &aAttribute, 
                                              const nsAString &aValue,
                                              PRBool *aFirst, 
                                              PRBool *aAny, 
                                              PRBool *aAll,
                                              nsAString &outValue)
{
  if (!aProperty || !aFirst || !aAny || !aAll)
    return NS_ERROR_NULL_POINTER;
  const nsAString *att = nsnull;
  if (aAttribute.Length())
    att = &aAttribute;
  const nsAString *val = nsnull;
  if (aValue.Length())
    val = &aValue;
  return GetInlinePropertyBase( aProperty, att, val, aFirst, aAny, aAll, &outValue);
}


NS_IMETHODIMP nsHTMLEditor::RemoveAllInlineProperties()
{
  return RemoveInlinePropertyImpl(nsnull, nsnull);
}

NS_IMETHODIMP nsHTMLEditor::RemoveInlineProperty(nsIAtom *aProperty, const nsAString &aAttribute)
{
  return RemoveInlinePropertyImpl(aProperty, &aAttribute);
}

nsresult nsHTMLEditor::RemoveInlinePropertyImpl(nsIAtom *aProperty, const nsAString *aAttribute)
{
  if (!mRules)    return NS_ERROR_NOT_INITIALIZED;
  ForceCompositionEnd();

  nsresult res;
  nsCOMPtr<nsISelection>selection;
  res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsISelectionPrivate> selPriv(do_QueryInterface(selection));

  PRBool isCollapsed;
  selection->GetIsCollapsed(&isCollapsed);

  PRBool useCSS;
  GetIsCSSEnabled(&useCSS);

  if (isCollapsed)
  {
    // manipulating text attributes on a collapsed selection only sets state for the next text insertion

    // For links, aProperty uses "href", use "a" instead
    if (aProperty == nsIEditProperty::href ||
        aProperty == nsIEditProperty::name)
      aProperty = nsIEditProperty::a;

    if (aProperty) return mTypeInState->ClearProp(aProperty, nsAutoString(*aAttribute));
    else return mTypeInState->ClearAllProps();
  }
  nsAutoEditBatch batchIt(this);
  nsAutoRules beginRulesSniffing(this, kOpRemoveTextProperty, nsIEditor::eNext);
  nsAutoSelectionReset selectionResetter(selection, this);
  nsAutoTxnsConserveSelection dontSpazMySelection(this);
  
  PRBool cancel, handled;
  nsTextRulesInfo ruleInfo(nsTextEditRules::kRemoveTextProperty);
  res = mRules->WillDoAction(selection, &ruleInfo, &cancel, &handled);
  if (NS_FAILED(res)) return res;
  if (!cancel && !handled)
  {
    // get selection range enumerator
    nsCOMPtr<nsIEnumerator> enumerator;
    res = selPriv->GetEnumerator(getter_AddRefs(enumerator));
    if (NS_FAILED(res)) return res;
    if (!enumerator)    return NS_ERROR_FAILURE;

    // loop thru the ranges in the selection
    enumerator->First(); 
    nsCOMPtr<nsISupports> currentItem;
    while ((NS_ENUMERATOR_FALSE == enumerator->IsDone()))
    {
      res = enumerator->CurrentItem(getter_AddRefs(currentItem));
      if (NS_FAILED(res)) return res;
      if (!currentItem)   return NS_ERROR_FAILURE;
      
      nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );

      if (aProperty == nsIEditProperty::name)
      {
        // promote range if it starts or end in a named anchor and we
        // want to remove named anchors
        res = PromoteRangeIfStartsOrEndsInNamedAnchor(range);
      }
      else {
        // adjust range to include any ancestors who's children are entirely selected
        res = PromoteInlineRange(range);
      }
      if (NS_FAILED(res)) return res;

      // remove this style from ancestors of our range endpoints, 
      // splitting them as appropriate
      res = SplitStyleAboveRange(range, aProperty, aAttribute);
      if (NS_FAILED(res)) return res;
      
      // check for easy case: both range endpoints in same text node
      nsCOMPtr<nsIDOMNode> startNode, endNode;
      res = range->GetStartContainer(getter_AddRefs(startNode));
      if (NS_FAILED(res)) return res;
      res = range->GetEndContainer(getter_AddRefs(endNode));
      if (NS_FAILED(res)) return res;
      if ((startNode == endNode) && IsTextNode(startNode))
      {
        // we're done with this range!
        if (useCSS && mHTMLCSSUtils->IsCSSEditableProperty(startNode, aProperty, aAttribute)) {
          // the HTML style defined by aProperty/aAttribute has a CSS equivalence
          // in this implementation for startNode
          nsAutoString cssValue;
          PRBool isSet = PR_FALSE;
          mHTMLCSSUtils->IsCSSEquivalentToHTMLInlineStyleSet(startNode,
                                                    aProperty,
                                                    aAttribute,
                                                    isSet ,
                                                    cssValue,
                                                    COMPUTED_STYLE_TYPE);
          if (isSet) {
            // startNode's computed style indicates the CSS equivalence to the HTML style to
            // remove is applied; but we found no element in the ancestors of startNode
            // carrying specified styles; assume it comes from a rule and let's try to
            // insert a span "inverting" the style
            nsAutoString value; value.AssignWithConversion("-moz-editor-invert-value");
            PRInt32 startOffset, endOffset;
            range->GetStartOffset(&startOffset);
            range->GetEndOffset(&endOffset);
            nsCOMPtr<nsIDOMCharacterData> nodeAsText = do_QueryInterface(startNode);
            if (mHTMLCSSUtils->IsCSSInvertable(aProperty, aAttribute)) {
              SetInlinePropertyOnTextNode(nodeAsText, startOffset, endOffset, aProperty, aAttribute, &value);
            }
          }
        }
      }
      else
      {
        // not the easy case.  range not contained in single text node. 
        nsCOMPtr<nsIContentIterator> iter;
        res = nsComponentManager::CreateInstance(kSubtreeIteratorCID, nsnull,
                                                  NS_GET_IID(nsIContentIterator), 
                                                  getter_AddRefs(iter));
        if (NS_FAILED(res)) return res;
        if (!iter)          return NS_ERROR_FAILURE;

        nsCOMPtr<nsISupportsArray> arrayOfNodes;
        nsCOMPtr<nsIContent> content;
        nsCOMPtr<nsIDOMNode> node;
        nsCOMPtr<nsISupports> isupports;
        
        // make a array
        res = NS_NewISupportsArray(getter_AddRefs(arrayOfNodes));
        if (NS_FAILED(res)) return res;
        
        // iterate range and build up array
        iter->Init(range);
        while (NS_ENUMERATOR_FALSE == iter->IsDone())
        {
          res = iter->CurrentNode(getter_AddRefs(content));
          if (NS_FAILED(res)) return res;
          node = do_QueryInterface(content);
          if (!node) return NS_ERROR_FAILURE;
          if (IsEditable(node))
          { 
            isupports = do_QueryInterface(node);
            arrayOfNodes->AppendElement(isupports);
          }
          res = iter->Next();
          if (NS_FAILED(res)) return res;
        }
        
        // loop through the list, remove the property on each node
        PRUint32 listCount;
        PRUint32 j;
        arrayOfNodes->Count(&listCount);
        for (j = 0; j < listCount; j++)
        {
          isupports = dont_AddRef(arrayOfNodes->ElementAt(0));
          node = do_QueryInterface(isupports);
          res = RemoveStyleInside(node, aProperty, aAttribute);
          if (NS_FAILED(res)) return res;
          if (useCSS && mHTMLCSSUtils->IsCSSEditableProperty(node, aProperty, aAttribute)) {
            // the HTML style defined by aProperty/aAttribute has a CSS equivalence
            // in this implementation for node
            nsAutoString cssValue;
            PRBool isSet = PR_FALSE;
            mHTMLCSSUtils->IsCSSEquivalentToHTMLInlineStyleSet(node,
                                                               aProperty,
                                                               aAttribute,
                                                               isSet ,
                                                               cssValue,
                                                               COMPUTED_STYLE_TYPE);
            if (isSet) {
              // startNode's computed style indicates the CSS equivalence to the HTML style to
              // remove is applied; but we found no element in the ancestors of startNode
              // carrying specified styles; assume it comes from a rule and let's try to
              // insert a span "inverting" the style
              if (mHTMLCSSUtils->IsCSSInvertable(aProperty, aAttribute)) {
                nsAutoString value; value.AssignWithConversion("-moz-editor-invert-value");
                SetInlinePropertyOnNode(node, aProperty, aAttribute, &value);
              }
            }
          }
          arrayOfNodes->RemoveElementAt(0);
        }
      }
      enumerator->Next();
    }
  }
  if (!cancel)
  {
    // post-process 
    res = mRules->DidDoAction(selection, &ruleInfo, res);
  }
  return res;
}

NS_IMETHODIMP nsHTMLEditor::IncreaseFontSize()
{
  return RelativeFontChange(1);
}

NS_IMETHODIMP nsHTMLEditor::DecreaseFontSize()
{
  return RelativeFontChange(-1);
}

nsresult
nsHTMLEditor::RelativeFontChange( PRInt32 aSizeChange)
{
  // Can only change font size by + or - 1
  if ( !( (aSizeChange==1) || (aSizeChange==-1) ) )
    return NS_ERROR_ILLEGAL_VALUE;
  
  ForceCompositionEnd();

  // Get the selection 
  nsCOMPtr<nsISelection>selection;
  nsresult res = GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_FAILURE;
  nsCOMPtr<nsISelectionPrivate> selPriv(do_QueryInterface(selection));  
  // Is the selection collapsed?
  PRBool bCollapsed;
  res = selection->GetIsCollapsed(&bCollapsed);
  if (NS_FAILED(res)) return res;
  
  // if it's collapsed set typing state
  if (bCollapsed)
  {
    nsCOMPtr<nsIAtom> atom;
    if (aSizeChange==1) atom = nsIEditProperty::big;
    else                atom = nsIEditProperty::small;

    // Let's see in what kind of element the selection is
    PRInt32 offset;
    nsCOMPtr<nsIDOMNode> selectedNode;
    res = GetStartNodeAndOffset(selection, address_of(selectedNode), &offset);
    if (IsTextNode(selectedNode)) {
      nsCOMPtr<nsIDOMNode> parent;
      res = selectedNode->GetParentNode(getter_AddRefs(parent));
      if (NS_FAILED(res)) return res;
      selectedNode = parent;
    }
    nsAutoString tag;
    atom->ToString(tag);
    if (!CanContainTag(selectedNode, tag)) return NS_OK;

    // manipulating text attributes on a collapsed selection only sets state for the next text insertion
    return mTypeInState->SetProp(atom, nsAutoString(), nsAutoString());
  }
  
  // wrap with txn batching, rules sniffing, and selection preservation code
  nsAutoEditBatch batchIt(this);
  nsAutoRules beginRulesSniffing(this, kOpSetTextProperty, nsIEditor::eNext);
  nsAutoSelectionReset selectionResetter(selection, this);
  nsAutoTxnsConserveSelection dontSpazMySelection(this);

  // get selection range enumerator
  nsCOMPtr<nsIEnumerator> enumerator;
  res = selPriv->GetEnumerator(getter_AddRefs(enumerator));
  if (NS_FAILED(res)) return res;
  if (!enumerator)    return NS_ERROR_FAILURE;

  // loop thru the ranges in the selection
  enumerator->First(); 
  nsCOMPtr<nsISupports> currentItem;
  while ((NS_ENUMERATOR_FALSE == enumerator->IsDone()))
  {
    res = enumerator->CurrentItem(getter_AddRefs(currentItem));
    if (NS_FAILED(res)) return res;
    if (!currentItem)   return NS_ERROR_FAILURE;
    
    nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );

    // adjust range to include any ancestors who's children are entirely selected
    res = PromoteInlineRange(range);
    if (NS_FAILED(res)) return res;
    
    // check for easy case: both range endpoints in same text node
    nsCOMPtr<nsIDOMNode> startNode, endNode;
    res = range->GetStartContainer(getter_AddRefs(startNode));
    if (NS_FAILED(res)) return res;
    res = range->GetEndContainer(getter_AddRefs(endNode));
    if (NS_FAILED(res)) return res;
    if ((startNode == endNode) && IsTextNode(startNode))
    {
      PRInt32 startOffset, endOffset;
      range->GetStartOffset(&startOffset);
      range->GetEndOffset(&endOffset);
      nsCOMPtr<nsIDOMCharacterData> nodeAsText = do_QueryInterface(startNode);
      res = RelativeFontChangeOnTextNode(aSizeChange, nodeAsText, startOffset, endOffset);
      if (NS_FAILED(res)) return res;
    }
    else
    {
      // not the easy case.  range not contained in single text node. 
      // there are up to three phases here.  There are all the nodes
      // reported by the subtree iterator to be processed.  And there
      // are potentially a starting textnode and an ending textnode
      // which are only partially contained by the range.
      
      // lets handle the nodes reported by the iterator.  These nodes
      // are entirely contained in the selection range.  We build up
      // a list of them (since doing operations on the document during
      // iteration would perturb the iterator).

      nsCOMPtr<nsIContentIterator> iter;
      res = nsComponentManager::CreateInstance(kSubtreeIteratorCID, nsnull,
                                                NS_GET_IID(nsIContentIterator), 
                                                getter_AddRefs(iter));
      if (NS_FAILED(res)) return res;
      if (!iter)          return NS_ERROR_FAILURE;

      nsCOMPtr<nsISupportsArray> arrayOfNodes;
      nsCOMPtr<nsIContent> content;
      nsCOMPtr<nsIDOMNode> node;
      nsCOMPtr<nsISupports> isupports;
      
      // make a array
      res = NS_NewISupportsArray(getter_AddRefs(arrayOfNodes));
      if (NS_FAILED(res)) return res;
      
      // iterate range and build up array
      res = iter->Init(range);
      if (NS_SUCCEEDED(res))
      {
        while (NS_ENUMERATOR_FALSE == iter->IsDone())
        {
          res = iter->CurrentNode(getter_AddRefs(content));
          if (NS_FAILED(res)) return res;
          node = do_QueryInterface(content);
          if (!node) return NS_ERROR_FAILURE;
          if (IsEditable(node))
          { 
            isupports = do_QueryInterface(node);
            arrayOfNodes->AppendElement(isupports);
          }
          iter->Next();
        }
        
        // now that we have the list, do the font size change on each node
        PRUint32 listCount;
        PRUint32 j;
        arrayOfNodes->Count(&listCount);
        for (j = 0; j < listCount; j++)
        {
          isupports = dont_AddRef(arrayOfNodes->ElementAt(0));
          node = do_QueryInterface(isupports);
          res = RelativeFontChangeOnNode(aSizeChange, node);
          if (NS_FAILED(res)) return res;
          arrayOfNodes->RemoveElementAt(0);
        }
      }
      // now check the start and end parents of the range to see if they need to 
      // be seperately handled (they do if they are text nodes, due to how the
      // subtree iterator works - it will not have reported them).
      if (IsTextNode(startNode) && IsEditable(startNode))
      {
        nsCOMPtr<nsIDOMCharacterData> nodeAsText = do_QueryInterface(startNode);
        PRInt32 startOffset;
        PRUint32 textLen;
        range->GetStartOffset(&startOffset);
        nodeAsText->GetLength(&textLen);
        res = RelativeFontChangeOnTextNode(aSizeChange, nodeAsText, startOffset, textLen);
        if (NS_FAILED(res)) return res;
      }
      if (IsTextNode(endNode) && IsEditable(endNode))
      {
        nsCOMPtr<nsIDOMCharacterData> nodeAsText = do_QueryInterface(endNode);
        PRInt32 endOffset;
        range->GetEndOffset(&endOffset);
        res = RelativeFontChangeOnTextNode(aSizeChange, nodeAsText, 0, endOffset);
        if (NS_FAILED(res)) return res;
      }
    }
    enumerator->Next();
  }
  
  return res;  
}

nsresult
nsHTMLEditor::RelativeFontChangeOnTextNode( PRInt32 aSizeChange, 
                                            nsIDOMCharacterData *aTextNode, 
                                            PRInt32 aStartOffset,
                                            PRInt32 aEndOffset)
{
  // Can only change font size by + or - 1
  if ( !( (aSizeChange==1) || (aSizeChange==-1) ) )
    return NS_ERROR_ILLEGAL_VALUE;
  if (!aTextNode) return NS_ERROR_NULL_POINTER;
  
  // dont need to do anything if no characters actually selected
  if (aStartOffset == aEndOffset) return NS_OK;
  
  nsresult res = NS_OK;
  nsCOMPtr<nsIDOMNode> parent;
  res = aTextNode->GetParentNode(getter_AddRefs(parent));
  if (NS_FAILED(res)) return res;
  if (!CanContainTag(parent, NS_LITERAL_STRING("big"))) return NS_OK;

  nsCOMPtr<nsIDOMNode> tmp, node = do_QueryInterface(aTextNode);

  // do we need to split the text node?
  PRUint32 textLen;
  aTextNode->GetLength(&textLen);
  
  // -1 is a magic value meaning to the end of node
  if (aEndOffset == -1) aEndOffset = textLen;
  
  if ( (PRUint32)aEndOffset != textLen )
  {
    // we need to split off back of text node
    res = SplitNode(node, aEndOffset, getter_AddRefs(tmp));
    if (NS_FAILED(res)) return res;
    node = tmp;  // remember left node
  }
  if ( aStartOffset )
  {
    // we need to split off front of text node
    res = SplitNode(node, aStartOffset, getter_AddRefs(tmp));
    if (NS_FAILED(res)) return res;
  }

  NS_NAMED_LITERAL_STRING(bigSize, "big");
  NS_NAMED_LITERAL_STRING(smallSize, "small");
  const nsAString& nodeType = (aSizeChange==1) ? NS_STATIC_CAST(const nsAString&, bigSize) : NS_STATIC_CAST(const nsAString&, smallSize);
  // look for siblings that are correct type of node
  nsCOMPtr<nsIDOMNode> sibling;
  GetPriorHTMLSibling(node, address_of(sibling));
  if (sibling && NodeIsType(sibling, nodeType))
  {
    // previous sib is already right kind of inline node; slide this over into it
    res = MoveNode(node, sibling, -1);
    return res;
  }
  sibling = nsnull;
  GetNextHTMLSibling(node, address_of(sibling));
  if (sibling && NodeIsType(sibling, nodeType))
  {
    // following sib is already right kind of inline node; slide this over into it
    res = MoveNode(node, sibling, 0);
    return res;
  }
  
  // else reparent the node inside font node with appropriate relative size
  res = InsertContainerAbove(node, address_of(tmp), nodeType);
  return res;
}


nsresult
nsHTMLEditor::RelativeFontChangeHelper( PRInt32 aSizeChange, 
                                        nsIDOMNode *aNode)
{
  /*  This routine looks for all the font nodes in the tree rooted by aNode,
      including aNode itself, looking for font nodes that have the size attr
      set.  Any such nodes need to have big or small put inside them, since
      they override any big/small that are above them.
  */
  
  // Can only change font size by + or - 1
  if ( !( (aSizeChange==1) || (aSizeChange==-1) ) )
    return NS_ERROR_ILLEGAL_VALUE;
  if (!aNode) return NS_ERROR_NULL_POINTER;

  nsresult res = NS_OK;
  nsAutoString tag;
  if (aSizeChange == 1) tag.Assign(NS_LITERAL_STRING("big"));
  else tag.Assign(NS_LITERAL_STRING("small"));
  nsCOMPtr<nsIDOMNodeList> childNodes;
  PRInt32 j;
  PRUint32 childCount;
  nsCOMPtr<nsIDOMNode> childNode;
  
  // if this is a font node with size, put big/small inside it
  NS_NAMED_LITERAL_STRING(attr, "size");
  if (NodeIsType(aNode, nsIEditProperty::font) && HasAttr(aNode, &attr))
  {
    // cycle through children and adjust relative font size
    res = aNode->GetChildNodes(getter_AddRefs(childNodes));
    if (NS_FAILED(res)) return res;
    if (childNodes)
    {
      childNodes->GetLength(&childCount);
      for (j=childCount-1; j>=0; j--)
      {
        res = childNodes->Item(j, getter_AddRefs(childNode));
        if ((NS_SUCCEEDED(res)) && (childNode))
        {
          res = RelativeFontChangeOnNode(aSizeChange, childNode);
          if (NS_FAILED(res)) return res;
        }
      }
    }
  }

  childNodes = nsnull;
  // now cycle through the children.
  res = aNode->GetChildNodes(getter_AddRefs(childNodes));
  if (NS_FAILED(res)) return res;
  if (childNodes)
  {
    childNodes->GetLength(&childCount);
    for (j=childCount-1; j>=0; j--)
    {
      res = childNodes->Item(j, getter_AddRefs(childNode));
      if ((NS_SUCCEEDED(res)) && (childNode))
      {
        res = RelativeFontChangeHelper(aSizeChange, childNode);
        if (NS_FAILED(res)) return res;
      }
    }
  }

  return res;
}


nsresult
nsHTMLEditor::RelativeFontChangeOnNode( PRInt32 aSizeChange, 
                                        nsIDOMNode *aNode)
{
  // Can only change font size by + or - 1
  if ( !( (aSizeChange==1) || (aSizeChange==-1) ) )
    return NS_ERROR_ILLEGAL_VALUE;
  if (!aNode) return NS_ERROR_NULL_POINTER;

  nsresult res = NS_OK;
  nsCOMPtr<nsIDOMNode> tmp;
  nsAutoString tag;
  if (aSizeChange == 1) tag.Assign(NS_LITERAL_STRING("big"));
  else tag.Assign(NS_LITERAL_STRING("small"));
  
  // is it the opposite of what we want?  
  if ( ((aSizeChange == 1) && nsHTMLEditUtils::IsSmall(aNode)) || 
       ((aSizeChange == -1) &&  nsHTMLEditUtils::IsBig(aNode)) )
  {
    // first populate any nested font tags that have the size attr set
    res = RelativeFontChangeHelper(aSizeChange, aNode);
    if (NS_FAILED(res)) return res;
    // in that case, just remove this node and pull up the children
    res = RemoveContainer(aNode);
    return res;
  }
  // can it be put inside a "big" or "small"?
  if (TagCanContain(tag, aNode))
  {
    // first populate any nested font tags that have the size attr set
    res = RelativeFontChangeHelper(aSizeChange, aNode);
    if (NS_FAILED(res)) return res;
    // ok, chuck it in.
    // first look at siblings of aNode for matching bigs or smalls.
    // if we find one, move aNode into it.
    nsCOMPtr<nsIDOMNode> sibling;
    GetPriorHTMLSibling(aNode, address_of(sibling));
    if (sibling && NodeIsType(sibling, NS_ConvertASCIItoUCS2(aSizeChange==1 ? "big" : "small")))
    {
      // previous sib is already right kind of inline node; slide this over into it
      res = MoveNode(aNode, sibling, -1);
      return res;
    }
    sibling = nsnull;
    GetNextHTMLSibling(aNode, address_of(sibling));
    if (sibling && NodeIsType(sibling, NS_ConvertASCIItoUCS2(aSizeChange==1 ? "big" : "small")))
    {
      // following sib is already right kind of inline node; slide this over into it
      res = MoveNode(aNode, sibling, 0);
      return res;
    }
    // else insert it above aNode
    res = InsertContainerAbove(aNode, address_of(tmp), tag);
    return res;
  }
  // none of the above?  then cycle through the children.
  // MOOSE: we should group the children together if possible
  // into a single "big" or "small".  For the moment they are
  // each getting their own.  
  nsCOMPtr<nsIDOMNodeList> childNodes;
  res = aNode->GetChildNodes(getter_AddRefs(childNodes));
  if (NS_FAILED(res)) return res;
  if (childNodes)
  {
    PRInt32 j;
    PRUint32 childCount;
    childNodes->GetLength(&childCount);
    for (j=childCount-1; j>=0; j--)
    {
      nsCOMPtr<nsIDOMNode> childNode;
      res = childNodes->Item(j, getter_AddRefs(childNode));
      if ((NS_SUCCEEDED(res)) && (childNode))
      {
        res = RelativeFontChangeOnNode(aSizeChange, childNode);
        if (NS_FAILED(res)) return res;
      }
    }
  }
  return res;
}

NS_IMETHODIMP 
nsHTMLEditor::GetFontFaceState(PRBool *aMixed, nsAString &outFace)
{
  if (!aMixed)
      return NS_ERROR_FAILURE;
  *aMixed = PR_TRUE;
  outFace.SetLength(0);

  nsresult res;
  PRBool first, any, all;
  
  NS_NAMED_LITERAL_STRING(attr, "face");
  res = GetInlinePropertyBase(nsIEditProperty::font, &attr, nsnull, &first, &any, &all, &outFace);
  if (NS_FAILED(res)) return res;
  if (any && !all) return res; // mixed
  if (all)
  {
    *aMixed = PR_FALSE;
    return res;
  }
  
  res = GetInlinePropertyBase(nsIEditProperty::tt, nsnull, nsnull, &first, &any, &all,nsnull);
  if (NS_FAILED(res)) return res;
  if (any && !all) return res; // mixed
  if (all)
  {
    *aMixed = PR_FALSE;
    nsIEditProperty::tt->ToString(outFace);
  }
  
  if (!any)
  {
    // there was no font face attrs of any kind.  We are in normal font.
    outFace.SetLength(0);
    *aMixed = PR_FALSE;
  }
  return res;
}

NS_IMETHODIMP 
nsHTMLEditor::GetFontColorState(PRBool *aMixed, nsAString &aOutColor)
{
  if (!aMixed)
      return NS_ERROR_NULL_POINTER;
  *aMixed = PR_TRUE;
  aOutColor.SetLength(0);
  
  nsresult res;
  NS_NAMED_LITERAL_STRING(colorStr, "color");
  PRBool first, any, all;
  
  res = GetInlinePropertyBase(nsIEditProperty::font, &colorStr, nsnull, &first, &any, &all, &aOutColor);
  if (NS_FAILED(res)) return res;
  if (any && !all) return res; // mixed
  if (all)
  {
    *aMixed = PR_FALSE;
    return res;
  }
  
  if (!any)
  {
    // there was no font color attrs of any kind..
    aOutColor.SetLength(0);
    *aMixed = PR_FALSE;
  }
  return res;
}

// the return value is true only if the instance of the HTML editor we created
// can handle CSS styles (for instance, Composer can, Messenger can't) and if
// the CSS preference is checked
nsresult
nsHTMLEditor::GetIsCSSEnabled(PRBool *aIsCSSEnabled)
{
  *aIsCSSEnabled = PR_FALSE;
  if (mCSSAware) {
    // TBD later : removal of mCSSAware and use only the presence of mHTMLCSSUtils
    if (mHTMLCSSUtils) {
      *aIsCSSEnabled = mHTMLCSSUtils->IsCSSPrefChecked();
    }
  }
  return NS_OK;
}

nsresult
nsHTMLEditor::HasStyleOrIdOrClass(nsIDOMElement * aElement, PRBool *aHasStyleOrIdOrClass)
{
  NS_ENSURE_TRUE(aElement, NS_ERROR_NULL_POINTER);
  nsCOMPtr<nsIDOMNode> node  = do_QueryInterface(aElement);


  // remove the node if its style attribute is empty or absent,
  // and if it does not have a class nor an id
  nsAutoString styleVal;
  PRBool isStyleSet;
  *aHasStyleOrIdOrClass = PR_TRUE;
  nsresult res = GetAttributeValue(aElement,  NS_LITERAL_STRING("style"), styleVal, &isStyleSet);
  if (NS_FAILED(res)) return res;
  if (!isStyleSet || (0 == styleVal.Length())) {
    res = mHTMLCSSUtils->HasClassOrID(aElement, *aHasStyleOrIdOrClass);
    if (NS_FAILED(res)) return res;
  }
  return res;
}

nsresult
nsHTMLEditor::RemoveElementIfNoStyleOrIdOrClass(nsIDOMElement * aElement, nsIAtom * aTag)
{
  NS_ENSURE_TRUE(aElement, NS_ERROR_NULL_POINTER);
  nsCOMPtr<nsIDOMNode> node  = do_QueryInterface(aElement);

  // early way out if node is not the right kind of element
  if (!NodeIsType(node, aTag)) {
    return NS_OK;
  }
  PRBool hasStyleOrIdOrClass;
  nsresult res = HasStyleOrIdOrClass(aElement, &hasStyleOrIdOrClass);
  if (!hasStyleOrIdOrClass) {
    res = RemoveContainer(node);
  }
  return res;
}
