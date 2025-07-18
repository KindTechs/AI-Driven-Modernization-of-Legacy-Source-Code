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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Joe Hewitt <hewitt@netscape.com> (original author)
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

#include "inDOMView.h"

#include "inLayoutUtils.h"

#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsISupportsArray.h"
#include "nsIDOMNode.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMAttr.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsIDOMMutationEvent.h"
#include "nsIBindingManager.h"
#include "nsIDocument.h"

////////////////////////////////////////////////////////////////////////
// inDOMViewNode

class inDOMViewNode
{
public:
  inDOMViewNode() {};
  inDOMViewNode(nsIDOMNode* aNode);
  ~inDOMViewNode();

  nsCOMPtr<nsIDOMNode> node;

  inDOMViewNode* parent;
  inDOMViewNode* next;
  inDOMViewNode* previous;

  PRInt32 level;
  PRBool isOpen;
  PRBool isContainer;
  PRBool hasAnonymous;
  PRBool hasSubDocument;
};

inDOMViewNode::inDOMViewNode(nsIDOMNode* aNode) :
  node(aNode),
  parent(nsnull),
  next(nsnull),
  previous(nsnull),
  level(0),
  isOpen(PR_FALSE),
  isContainer(PR_FALSE),
  hasAnonymous(PR_FALSE),
  hasSubDocument(PR_FALSE)
{
  
}

inDOMViewNode::~inDOMViewNode()
{
}

////////////////////////////////////////////////////////////////////////

nsIAtom* inDOMView::kAnonymousAtom = nsnull;
nsIAtom* inDOMView::kElementNodeAtom = nsnull;
nsIAtom* inDOMView::kAttributeNodeAtom = nsnull;
nsIAtom* inDOMView::kTextNodeAtom = nsnull;
nsIAtom* inDOMView::kCDataSectionNodeAtom = nsnull;
nsIAtom* inDOMView::kEntityReferenceNodeAtom = nsnull;
nsIAtom* inDOMView::kEntityNodeAtom = nsnull;
nsIAtom* inDOMView::kProcessingInstructionNodeAtom = nsnull;
nsIAtom* inDOMView::kCommentNodeAtom = nsnull;
nsIAtom* inDOMView::kDocumentNodeAtom = nsnull;
nsIAtom* inDOMView::kDocumentTypeNodeAtom = nsnull;
nsIAtom* inDOMView::kDocumentFragmentNodeAtom = nsnull;
nsIAtom* inDOMView::kNotationNodeAtom = nsnull;

inDOMView::inDOMView() :
  mShowAnonymous(PR_FALSE),
  mShowSubDocuments(PR_FALSE),
  mFilters(65535) // show all node types by default

{
  NS_INIT_REFCNT();

  kAnonymousAtom = NS_NewAtom("anonymous");
  kElementNodeAtom = NS_NewAtom("ELEMENT_NODE");
  kAttributeNodeAtom = NS_NewAtom("ATTRIBUTE_NODE");
  kTextNodeAtom = NS_NewAtom("TEXT_NODE");
  kCDataSectionNodeAtom = NS_NewAtom("CDATA_SECTION_NODE");
  kEntityReferenceNodeAtom = NS_NewAtom("ENTITY_REFERENCE_NODE");
  kEntityNodeAtom = NS_NewAtom("ENTITY_NODE");
  kProcessingInstructionNodeAtom = NS_NewAtom("PROCESSING_INSTRUCTION_NODE");
  kCommentNodeAtom = NS_NewAtom("COMMENT_NODE");
  kDocumentNodeAtom = NS_NewAtom("DOCUMENT_NODE");
  kDocumentTypeNodeAtom = NS_NewAtom("DOCUMENT_TYPE_NODE");
  kDocumentFragmentNodeAtom = NS_NewAtom("DOCUMENT_FRAGMENT_NODE");
  kNotationNodeAtom = NS_NewAtom("NOTATION_NODE");
}

inDOMView::~inDOMView()
{
  SetRootNode(nsnull);
}

////////////////////////////////////////////////////////////////////////
// nsISupports

NS_IMPL_ISUPPORTS3(inDOMView,
                   inIDOMView,
                   nsITreeView,
                   nsIDocumentObserver);

////////////////////////////////////////////////////////////////////////
// inIDOMView

NS_IMETHODIMP
inDOMView::GetRootNode(nsIDOMNode** aNode)
{
  *aNode = mRootNode;
  NS_IF_ADDREF(*aNode);
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::SetRootNode(nsIDOMNode* aNode)
{
  if (mRootDocument) {
    // remove previous document observer
    nsCOMPtr<nsIDocument> doc(do_QueryInterface(mRootDocument));
    if (doc)
      doc->RemoveObserver(this);
  }
    
  RemoveAllNodes();

  mRootNode = aNode;

  if (aNode) {
    PRBool filtered;
    // If we are able to show element nodes, then start with the root node
    // as the first node in the buffer
    IsFiltered(nsIDOMNode::ELEMENT_NODE, &filtered);
    if (filtered) {
      // allocate new node array
      AppendNode(CreateNode(aNode, nsnull));
    } else {
      // place only the children of the root node in the buffer
      ExpandNode(-1);
    }

    // store an owning reference to document so that it isn't
    // destroyed before we are
    nsCOMPtr<nsIDOMDocument> domDoc;
    aNode->GetOwnerDocument(getter_AddRefs(domDoc));
    mRootDocument = domDoc;

    // add document observer
    nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDoc));
    if (doc)
      doc->AddObserver(this);
  } else {
    mRootDocument = nsnull;
  }

  return NS_OK;
}

NS_IMETHODIMP
inDOMView::GetNodeFromRowIndex(PRInt32 rowIndex, nsIDOMNode **_retval)
{
  inDOMViewNode* viewNode = nsnull;
  RowToNode(rowIndex, &viewNode);
  if (!viewNode) return NS_ERROR_FAILURE;
  *_retval = viewNode->node;
  NS_IF_ADDREF(*_retval);

  return NS_OK;
}

NS_IMETHODIMP
inDOMView::GetRowIndexFromNode(nsIDOMNode *node, PRInt32 *_retval)
{
  NodeToRow(node, _retval);
  return NS_OK;
}


NS_IMETHODIMP 
inDOMView::GetShowAnonymousContent(PRBool *aShowAnonymousContent)
{
  *aShowAnonymousContent = mShowAnonymous;
  return NS_OK;
}

NS_IMETHODIMP 
inDOMView::SetShowAnonymousContent(PRBool aShowAnonymousContent)
{
  mShowAnonymous = aShowAnonymousContent;
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::GetShowSubDocuments(PRBool *aShowSubDocuments)
{
  *aShowSubDocuments = mShowSubDocuments;
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::SetShowSubDocuments(PRBool aShowSubDocuments)
{
  mShowSubDocuments = aShowSubDocuments;
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::AddFilterByType(PRUint16 aType, PRBool aExclusive)
{
  PRUint16 key = GetNodeTypeKey(aType);

  if (aExclusive) {
    mFilters = key;
  } else {
    mFilters |= key;
  }

  return NS_OK;
}

NS_IMETHODIMP
inDOMView::RemoveFilterByType(PRUint16 aType)
{
  PRUint16 key = GetNodeTypeKey(aType);
  mFilters -= key;
  return NS_OK;
}


NS_IMETHODIMP
inDOMView::IsFiltered(PRUint16 aType, PRBool* _retval)
{
  PRUint16 key = GetNodeTypeKey(aType);
  *_retval = mFilters & key;

  return NS_OK;
}

NS_IMETHODIMP
inDOMView::Rebuild()
{
  nsCOMPtr<nsIDOMNode> root;
  GetRootNode(getter_AddRefs(root));
  SetRootNode(root);
  mTree->Invalidate();
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// nsITreeView

NS_IMETHODIMP
inDOMView::GetRowCount(PRInt32 *aRowCount)
{
  *aRowCount = GetRowCount();
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::GetRowProperties(PRInt32 index, nsISupportsArray *properties)
{
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::GetCellProperties(PRInt32 row, const PRUnichar *colID, nsISupportsArray *properties)
{
  inDOMViewNode* node = nsnull;
  RowToNode(row, &node);
  if (!node) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIContent> content = do_QueryInterface(node->node);
  if (content) {
    nsCOMPtr<nsIContent> bparent;
    content->GetBindingParent(getter_AddRefs(bparent));
    if (bparent)
      properties->AppendElement(kAnonymousAtom);
  }

  PRUint16 nodeType;
  node->node->GetNodeType(&nodeType);
  switch (nodeType) {
    case nsIDOMNode::ELEMENT_NODE:
      properties->AppendElement(kElementNodeAtom);
      break;
    case nsIDOMNode::ATTRIBUTE_NODE:
      properties->AppendElement(kAttributeNodeAtom);
      break;
    case nsIDOMNode::TEXT_NODE:
      properties->AppendElement(kTextNodeAtom);
      break;
    case nsIDOMNode::CDATA_SECTION_NODE:
      properties->AppendElement(kCDataSectionNodeAtom);
      break;
    case nsIDOMNode::ENTITY_REFERENCE_NODE:
      properties->AppendElement(kEntityReferenceNodeAtom);
      break;
    case nsIDOMNode::ENTITY_NODE:
      properties->AppendElement(kEntityNodeAtom);
      break;
    case nsIDOMNode::PROCESSING_INSTRUCTION_NODE:
      properties->AppendElement(kProcessingInstructionNodeAtom);
      break;
    case nsIDOMNode::COMMENT_NODE:
      properties->AppendElement(kCommentNodeAtom);
      break;
    case nsIDOMNode::DOCUMENT_NODE:
      properties->AppendElement(kDocumentNodeAtom);
      break;
    case nsIDOMNode::DOCUMENT_TYPE_NODE:
      properties->AppendElement(kDocumentTypeNodeAtom);
      break;
    case nsIDOMNode::DOCUMENT_FRAGMENT_NODE:
      properties->AppendElement(kDocumentFragmentNodeAtom);
      break;
    case nsIDOMNode::NOTATION_NODE:
      properties->AppendElement(kNotationNodeAtom);
      break;
  }
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::GetColumnProperties(const PRUnichar *colID, nsIDOMElement *colElt, nsISupportsArray *properties)
{
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::GetImageSrc(PRInt32 row, const PRUnichar *colID, nsAString& _retval)
{
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::GetProgressMode(PRInt32 row, const PRUnichar *colID, PRInt32* _retval)
{
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::GetCellValue(PRInt32 row, const PRUnichar *colID, nsAString& _retval)
{
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::GetCellText(PRInt32 row, const PRUnichar *colID, nsAString& _retval)
{
  inDOMViewNode* node = nsnull;
  RowToNode(row, &node);
  if (!node) return NS_ERROR_FAILURE;
  
  nsIDOMNode* domNode = node->node;

  nsAutoString col(colID);
  if (col.Equals(NS_LITERAL_STRING("colNodeName")))
    domNode->GetNodeName(_retval);
  else if (col.Equals(NS_LITERAL_STRING("colLocalName")))
    domNode->GetLocalName(_retval);
  else if (col.Equals(NS_LITERAL_STRING("colPrefix")))
    domNode->GetPrefix(_retval);
  else if (col.Equals(NS_LITERAL_STRING("colNamespaceURI")))
    domNode->GetNamespaceURI(_retval);
  else if (col.Equals(NS_LITERAL_STRING("colNodeType"))) {
    PRUint16 nodeType;
    domNode->GetNodeType(&nodeType);
    nsAutoString temp;
    temp.AppendInt(PRInt32(nodeType));
    _retval = temp;
  } else if (col.Equals(NS_LITERAL_STRING("colNodeValue")))
    domNode->GetNodeValue(_retval);
  else {
    if (Substring(col, 0, 4).Equals(NS_LITERAL_STRING("col@"))) {
      nsCOMPtr<nsIDOMElement> el = do_QueryInterface(node->node);
      if (el) {
        nsAutoString attr;
        col.Right(attr, col.Length()-4); // have to use this because Substring is crashing on me!
        el->GetAttribute(attr, _retval);
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
inDOMView::IsContainer(PRInt32 index, PRBool *_retval)
{
  inDOMViewNode* node = nsnull;
  RowToNode(index, &node);
  if (!node) return NS_ERROR_FAILURE;

  *_retval = node->isContainer;
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::IsContainerOpen(PRInt32 index, PRBool *_retval)
{
  inDOMViewNode* node = nsnull;
  RowToNode(index, &node);
  if (!node) return NS_ERROR_FAILURE;

  *_retval = node->isOpen;
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::IsContainerEmpty(PRInt32 index, PRBool *_retval)
{
  inDOMViewNode* node = nsnull;
  RowToNode(index, &node);
  if (!node) return NS_ERROR_FAILURE;

  *_retval = node->isContainer ? PR_FALSE : PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::GetLevel(PRInt32 index, PRInt32 *_retval)
{
  inDOMViewNode* node = nsnull;
  RowToNode(index, &node);
  if (!node) return NS_ERROR_FAILURE;

  *_retval = node->level;
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::GetParentIndex(PRInt32 rowIndex, PRInt32 *_retval)
{
  inDOMViewNode* node = nsnull;
  RowToNode(rowIndex, &node);
  if (!node) return NS_ERROR_FAILURE;
  
  inDOMViewNode* checkNode = nsnull;
  PRUint32 i = rowIndex - 1;
  do {
    RowToNode(i, &checkNode);
    if (checkNode == node->parent) {
      *_retval = i;
      return NS_OK;
    }
    --i;
  } while (checkNode);

  return NS_OK;
}

NS_IMETHODIMP
inDOMView::HasNextSibling(PRInt32 rowIndex, PRInt32 afterIndex, PRBool *_retval)
{
  inDOMViewNode* node = nsnull;
  RowToNode(rowIndex, &node);
  if (!node) return NS_ERROR_FAILURE;
  
  *_retval = node->next != nsnull;

  return NS_OK;
}

NS_IMETHODIMP
inDOMView::ToggleOpenState(PRInt32 index)
{
  inDOMViewNode* node = nsnull;
  RowToNode(index, &node);
  if (!node) return NS_ERROR_FAILURE;

  PRInt32 oldCount = GetRowCount();
  if (node->isOpen)
    CollapseNode(index);
  else 
    ExpandNode(index);

  // Update the twisty.
  mTree->InvalidateRow(index);

  mTree->RowCountChanged(index+1, GetRowCount() - oldCount);
    
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::SetTree(nsITreeBoxObject *tree)
{
  mTree = tree;
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::GetSelection(nsITreeSelection * *aSelection)
{
  *aSelection = mSelection;
  NS_IF_ADDREF(*aSelection);
  return NS_OK;
}

NS_IMETHODIMP inDOMView::SetSelection(nsITreeSelection * aSelection)
{
  mSelection = aSelection;
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::SelectionChanged()
{
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::SetCellText(PRInt32 row, const PRUnichar *colID, const PRUnichar *value)
{
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::CycleHeader(const PRUnichar *colID, nsIDOMElement *elt)
{
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::CycleCell(PRInt32 row, const PRUnichar *colID)
{
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::IsEditable(PRInt32 row, const PRUnichar *colID, PRBool *_retval)
{
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::IsSeparator(PRInt32 index, PRBool *_retval)
{
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::IsSorted(PRBool *_retval)
{
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::CanDropOn(PRInt32 index, PRBool *_retval)
{
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::CanDropBeforeAfter(PRInt32 index, PRBool before, PRBool *_retval)
{
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::Drop(PRInt32 row, PRInt32 orientation)
{
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::PerformAction(const PRUnichar *action)
{
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::PerformActionOnRow(const PRUnichar *action, PRInt32 row)
{
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::PerformActionOnCell(const PRUnichar *action, PRInt32 row, const PRUnichar *colID)
{
  return NS_OK;
}

///////////////////////////////////////////////////////////////////////
// nsIDocumentObserver

NS_IMETHODIMP
inDOMView::AttributeChanged(nsIDocument *aDocument, nsIContent* aContent, PRInt32 aNameSpaceID,
                            nsIAtom* aAttribute, PRInt32 aModType, PRInt32 aHint)
{
  if (!mTree)
    return NS_ERROR_FAILURE;

  PRBool filtered;
  IsFiltered(nsIDOMNode::ATTRIBUTE_NODE, &filtered);
  if (!filtered)
    return NS_OK;

  // get the dom attribute node, if there is any
  nsCOMPtr<nsIDOMNode> node(do_QueryInterface(aContent));
  nsCOMPtr<nsIDOMElement> el(do_QueryInterface(aContent));
  nsCOMPtr<nsIDOMAttr> domAttr;
  nsAutoString attrStr;
  aAttribute->ToString(attrStr);
  el->GetAttributeNode(attrStr, getter_AddRefs(domAttr)); 

  if (aModType == nsIDOMMutationEvent::MODIFICATION) {
    // No fancy stuff here, just invalidate the changed row
    PRInt32 row = 0;
    NodeToRow(domAttr, &row);
    mTree->InvalidateRange(row, row);
  } else if (aModType == nsIDOMMutationEvent::ADDITION) {
    // get the number of attributes on this content node
    nsCOMPtr<nsIDOMNamedNodeMap> attrs;
    node->GetAttributes(getter_AddRefs(attrs));
    PRUint32 attrCount;
    attrs->GetLength(&attrCount);
  
    nsCOMPtr<nsIDOMNode> content(do_QueryInterface(aContent));
    inDOMViewNode* contentNode = nsnull;
    PRInt32 contentRow;
    PRInt32 attrRow;
    if (NS_SUCCEEDED(NodeToRow(content, &contentRow))) {
      // If there is a node for this content in the view, and it is open,
      // we will insert the attr node after the index of the last attr
      RowToNode(contentRow, &contentNode);
      if (!contentNode->isOpen)
        return NS_OK;
      attrRow = contentRow + attrCount;
    } else {
      // if this view has a root node but is not display it, it is ok
      // to act here if the changed attribute is on the root.  If not,
      // then the changed attribute's parent is not in the view
      if (mRootNode != node)
        return NS_OK;
      attrRow = attrCount - 1;
    }

    inDOMViewNode* newNode = CreateNode(domAttr, contentNode);
    inDOMViewNode* insertNode = nsnull;
    RowToNode(attrRow, &insertNode);
    if (insertNode) {
      if (insertNode->level <= contentNode->level) {
        RowToNode(attrRow-1, &insertNode);
        InsertLinkAfter(newNode, insertNode);
      } else 
        InsertLinkBefore(newNode, insertNode);
    }
    InsertNode(newNode, attrRow);
    mTree->RowCountChanged(attrRow, 1);
  } else if (aModType == nsIDOMMutationEvent::REMOVAL) {
    // At this point, the attribute is already gone from the DOM, but is still represented 
    // in our mRows array.  Search through the content node's children for the corresponding
    // node and remove it.

    // get the row of the content node
    nsCOMPtr<nsIDOMNode> content(do_QueryInterface(aContent));
    inDOMViewNode* contentNode = nsnull;
    PRInt32 contentRow;
    PRInt32 baseLevel;
    if (NS_SUCCEEDED(NodeToRow(content, &contentRow))) {
      RowToNode(contentRow, &contentNode);
      baseLevel = contentNode->level;
    } else {
      if (mRootNode == node) {
        contentRow = -1;
        baseLevel = -1;      
      } else
        return NS_OK;
    }

    // search for the attribute node that was removed
    inDOMViewNode* checkNode = nsnull;
    PRInt32 row = 0;
    for (row = contentRow+1; row < GetRowCount(); ++row) {
      checkNode = GetNodeAt(row);
      if (checkNode->level == baseLevel+1) {
        domAttr = do_QueryInterface(checkNode->node);
        if (domAttr) {
          nsAutoString attrName;
          domAttr->GetNodeName(attrName);
          if (attrName.Equals(attrStr)) {
            // we have found the row for the attribute that was removed
            RemoveLink(checkNode);
            RemoveNode(row);
            mTree->RowCountChanged(row, -1);
            break;
          }
        }        
      }
      if (checkNode->level <= baseLevel)
        break;
    }

 }
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::ContentAppended(nsIDocument *aDocument, nsIContent* aContainer, PRInt32 aNewIndexInContainer)
{
  if (!mTree)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIContent> child;
  aContainer->ChildAt(aNewIndexInContainer, *getter_AddRefs(child));
  return ContentInserted(aDocument, aContainer, child, aNewIndexInContainer);
}

NS_IMETHODIMP
inDOMView::ContentInserted(nsIDocument *aDocument, nsIContent* aContainer, nsIContent* aChild, PRInt32 aIndexInContainer)
{
  if (!mTree)
    return NS_ERROR_FAILURE;

  nsresult rv;
  nsCOMPtr<nsIDOMNode> childDOMNode(do_QueryInterface(aChild));
  nsCOMPtr<nsIDOMNode> parent;
  GetRealParent(childDOMNode, getter_AddRefs(parent));
  
  // find the inDOMViewNode for the parent of the inserted content
  PRInt32 parentRow = 0;
  if (NS_FAILED(rv = NodeToRow(parent, &parentRow)))
    return rv;
  inDOMViewNode* parentNode = nsnull;
  if (NS_FAILED(rv = RowToNode(parentRow, &parentNode)))
    return rv;

  // get the previous sibling of the inserted content
  nsCOMPtr<nsIDOMNode> previous;
  GetRealPreviousSibling(childDOMNode, parent, getter_AddRefs(previous));
  inDOMViewNode* previousNode = nsnull;

  PRInt32 row = 0;
  if (previous) {
    // find the inDOMViewNode for the previous sibling of the inserted content
    PRInt32 previousRow = 0;
    if (NS_FAILED(rv = NodeToRow(previous, &previousRow)))
      return rv;
    if (NS_FAILED(rv = RowToNode(previousRow, &previousNode)))
      return rv;

    // get the last descendant of the previous row, which is the row
    // after which to insert this new row
    GetLastDescendantOf(previousNode, previousRow, &row);
    ++row;
  } else {
    // there is no previous sibling, so the new row will be inserted after the parent
    row = parentRow+1;
  }

  inDOMViewNode* newNode = CreateNode(childDOMNode, parentNode);

  if (previous) {
    InsertLinkAfter(newNode, previousNode);
  } else {
    PRInt32 firstChildRow;
    if (NS_SUCCEEDED(GetFirstDescendantOf(parentNode, parentRow, &firstChildRow))) {
      inDOMViewNode* firstChild;
      RowToNode(firstChildRow, &firstChild);
      InsertLinkBefore(newNode, firstChild);
    }
  }

  // insert new node
  InsertNode(newNode, row);

  mTree->RowCountChanged(row, 1);

  return NS_OK;
}

NS_IMETHODIMP
inDOMView::ContentReplaced(nsIDocument *aDocument, nsIContent* aContainer, nsIContent* aOldChild, nsIContent* aNewChild, PRInt32 aIndexInContainer)
{
  if (!mTree)
    return NS_ERROR_FAILURE;

  nsresult rv;

  // find the inDOMViewNode for the old child
  nsCOMPtr<nsIDOMNode> oldDOMNode(do_QueryInterface(aOldChild));
  nsCOMPtr<nsIDOMNode> newDOMNode(do_QueryInterface(aNewChild));
  PRInt32 row = 0;
  if (NS_FAILED(rv = NodeToRow(oldDOMNode, &row)))
    return rv;
  inDOMViewNode* oldNode;
  if (NS_FAILED(rv = RowToNode(row, &oldNode)))
    return rv;

  PRInt32 oldRowCount = GetRowCount();
  if (oldNode->isOpen)
    CollapseNode(row);

  inDOMViewNode* newNode = CreateNode(newDOMNode, oldNode->parent);
  ReplaceLink(newNode, oldNode);

  ReplaceNode(newNode, row);

  // XXX can this go into ReplaceNode?
  mTree->InvalidateRange(row, oldRowCount-1);

  return NS_OK;
}

NS_IMETHODIMP
inDOMView::ContentRemoved(nsIDocument *aDocument, nsIContent* aContainer, nsIContent* aChild, PRInt32 aIndexInContainer)
{
  if (!mTree)
    return NS_ERROR_FAILURE;

  nsresult rv;

  // find the inDOMViewNode for the old child
  nsCOMPtr<nsIDOMNode> oldDOMNode(do_QueryInterface(aChild));
  PRInt32 row = 0;
  if (NS_FAILED(rv = NodeToRow(oldDOMNode, &row)))
    return rv;
  inDOMViewNode* oldNode;
  if (NS_FAILED(rv = RowToNode(row, &oldNode)))
    return rv;

  if (oldNode->isOpen)
    CollapseNode(row);

  RemoveLink(oldNode);
  RemoveNode(row);

  mTree->RowCountChanged(row, -1);

  return NS_OK;
}

///////////////////////////////////////////////////////////////////////
// inDOMView

//////// NODE MANAGEMENT

inDOMViewNode*
inDOMView::GetNodeAt(PRInt32 aRow)
{
  return (inDOMViewNode*)mNodes.ElementAt(aRow);
}

PRInt32
inDOMView::GetRowCount()
{
  return mNodes.Count();
}

inDOMViewNode*
inDOMView::CreateNode(nsIDOMNode* aNode, inDOMViewNode* aParent)
{
  inDOMViewNode* viewNode = new inDOMViewNode(aNode);
  viewNode->level = aParent ? aParent->level+1 : 0;
  viewNode->parent = aParent;

  nsCOMPtr<nsISupportsArray> grandKids;
  GetChildNodesFor(aNode, getter_AddRefs(grandKids));
  PRUint32 grandKidLength;
  grandKids->Count(&grandKidLength);
  viewNode->isContainer = grandKidLength > 0;
  return viewNode;
}

PRBool
inDOMView::RowOutOfBounds(PRInt32 aRow, PRInt32 aCount)
{
  return aRow < 0 || aRow >= GetRowCount() || aCount+aRow > GetRowCount();
}

void
inDOMView::AppendNode(inDOMViewNode* aNode)
{
  mNodes.AppendElement(aNode);
}

void
inDOMView::InsertNode(inDOMViewNode* aNode, PRInt32 aRow)
{
  if (RowOutOfBounds(aRow, 1))
    AppendNode(aNode);
  else
    mNodes.InsertElementAt(aNode, aRow);
}

void
inDOMView::RemoveNode(PRInt32 aRow)
{
  if (RowOutOfBounds(aRow, 1))
    return;

  delete GetNodeAt(aRow);
  mNodes.RemoveElementAt(aRow);
}

void
inDOMView::ReplaceNode(inDOMViewNode* aNode, PRInt32 aRow)
{
  if (RowOutOfBounds(aRow, 1))
    return;

  delete GetNodeAt(aRow);
  mNodes.ReplaceElementAt(aNode, aRow);
}

void
inDOMView::InsertNodes(nsVoidArray& aNodes, PRInt32 aRow)
{
  if (aRow < 0 || aRow > GetRowCount())
    return;

  mNodes.InsertElementsAt(aNodes, aRow);
}

void
inDOMView::RemoveNodes(PRInt32 aRow, PRInt32 aCount)
{
  if (aRow < 0)
    return;

  PRInt32 rowCount = GetRowCount();
  for (PRInt32 i = aRow; i < aRow+aCount && i < rowCount; ++i) {
    delete GetNodeAt(i);
  }

  mNodes.RemoveElementsAt(aRow, aCount);
}

void
inDOMView::RemoveAllNodes()
{
  PRInt32 rowCount = GetRowCount();
  for (PRInt32 i = 0; i < rowCount; ++i) {
    delete GetNodeAt(i);
  }

  mNodes.Clear();
}

void
inDOMView::ExpandNode(PRInt32 aRow)
{
  inDOMViewNode* node = nsnull;
  RowToNode(aRow, &node);

  nsCOMPtr<nsISupportsArray> kids;
  inDOMView::GetChildNodesFor(node ? node->node : mRootNode, getter_AddRefs(kids));
  PRUint32 kidCount;
  kids->Count(&kidCount);

  nsVoidArray list(kidCount);

  nsCOMPtr<nsIDOMNode> kid;
  inDOMViewNode* newNode = nsnull;
  inDOMViewNode* prevNode = nsnull;

  for (PRUint32 i = 0; i < kidCount; ++i) {
    kids->GetElementAt(i, getter_AddRefs(kid));
    newNode = CreateNode(kid, node);
    list.ReplaceElementAt(newNode, i);

    if (prevNode)
      prevNode->next = newNode;
    newNode->previous = prevNode;
    prevNode = newNode;
  }

  InsertNodes(list, aRow+1);

  if (node)
    node->isOpen = PR_TRUE;
}

void
inDOMView::CollapseNode(PRInt32 aRow)
{
  inDOMViewNode* node = nsnull;
  RowToNode(aRow, &node);
  
  PRInt32 row = 0;
  GetLastDescendantOf(node, aRow, &row);

  RemoveNodes(aRow+1, row-aRow);

  node->isOpen = PR_FALSE;
}

//////// NODE AND ROW CONVERSION

nsresult
inDOMView::RowToNode(PRInt32 aRow, inDOMViewNode** aNode)
{
  if (aRow < 0 || aRow >= GetRowCount())
    return NS_ERROR_FAILURE;

  *aNode = GetNodeAt(aRow);
  return NS_OK;
}

nsresult
inDOMView::NodeToRow(nsIDOMNode* aNode, PRInt32* aRow)
{
  PRInt32 rowCount = GetRowCount();
  for (PRInt32 i = 0; i < rowCount; ++i) {
    if (GetNodeAt(i)->node == aNode) {
      *aRow = i;
      return NS_OK;
    }
  }

  *aRow = -1;
  return NS_ERROR_FAILURE;
}

//////// NODE HIERARCHY MUTATION

void 
inDOMView::InsertLinkAfter(inDOMViewNode* aNode, inDOMViewNode* aInsertAfter)
{
  if (aInsertAfter->next)
    aInsertAfter->next->previous = aNode;
  aNode->next = aInsertAfter->next;
  aInsertAfter->next = aNode;
  aNode->previous = aInsertAfter;
}

void 
inDOMView::InsertLinkBefore(inDOMViewNode* aNode, inDOMViewNode* aInsertBefore)
{
  if (aInsertBefore->previous)
    aInsertBefore->previous->next = aNode;
  aNode->previous = aInsertBefore->previous;
  aInsertBefore->previous = aNode;
  aNode->next = aInsertBefore;
}

void
inDOMView::RemoveLink(inDOMViewNode* aNode)
{
  if (aNode->previous)
    aNode->previous->next = aNode->next;
  if (aNode->next)
    aNode->next->previous = aNode->previous;
}

void
inDOMView::ReplaceLink(inDOMViewNode* aNewNode, inDOMViewNode* aOldNode)
{
  if (aOldNode->previous)
    aOldNode->previous->next = aNewNode;
  if (aOldNode->next)
    aOldNode->next->previous = aNewNode;
  aNewNode->next = aOldNode->next;
  aNewNode->previous = aOldNode->previous;
}

//////// NODE HIERARCHY UTILITIES

nsresult
inDOMView::GetFirstDescendantOf(inDOMViewNode* aNode, PRInt32 aRow, PRInt32* aResult)
{
  // get the first node that is a descendant of the previous sibling
  PRInt32 row = 0;
  inDOMViewNode* node;
  for (row = aRow+1; row < GetRowCount(); ++row) {
    node = GetNodeAt(row);
    if (node->parent == aNode) {
      *aResult = row;
      return NS_OK;
    }
    if (node->level <= aNode->level)
      break;
  }
  return NS_ERROR_FAILURE;
}

nsresult
inDOMView::GetLastDescendantOf(inDOMViewNode* aNode, PRInt32 aRow, PRInt32* aResult)
{
  // get the last node that is a descendant of the previous sibling
  PRInt32 row = 0;
  for (row = aRow+1; row < GetRowCount(); ++row) {
    if (GetNodeAt(row)->level <= aNode->level)
      break;
  }
  *aResult = row-1;
  return NS_OK;
}

//////// DOM UTILITIES

nsresult
inDOMView::GetChildNodesFor(nsIDOMNode* aNode, nsISupportsArray **aResult)
{
  nsresult rv;
  nsISupportsArray* result;
  if (NS_FAILED(rv = NS_NewISupportsArray(&result)))
    return rv;

  // Need to do this test to prevent unfortunate NYI assertion 
  // on nsXULAttribute::GetChildNodes
  nsCOMPtr<nsIDOMAttr> attr = do_QueryInterface(aNode, &rv);
  if (NS_FAILED(rv)) {
    // attribute nodes
    PRBool filtered = PR_FALSE;
    IsFiltered(nsIDOMNode::ATTRIBUTE_NODE, &filtered);
    if (filtered) {
      nsCOMPtr<nsIDOMNamedNodeMap> attrs;
      rv = aNode->GetAttributes(getter_AddRefs(attrs));
      if (attrs)
        AppendAttrsToArray(attrs, result);
    }

    if (mShowSubDocuments) {
      nsCOMPtr<nsIDOMDocument> domdoc = inLayoutUtils::GetSubDocumentFor(aNode);
      if (domdoc) {
        nsCOMPtr<nsIDOMNodeList> kids;
        rv = domdoc->GetChildNodes(getter_AddRefs(kids));
        if (NS_SUCCEEDED(rv))
          AppendKidsToArray(kids, result);
      }
    }

    
    IsFiltered(nsIDOMNode::ELEMENT_NODE, &filtered);
    if (filtered) {
      // try to get the anonymous content
      nsCOMPtr<nsIDOMNodeList> kids;
      if (mShowAnonymous) {
        nsCOMPtr<nsIContent> content = do_QueryInterface(aNode, &rv);
        if (content) {
          nsCOMPtr<nsIBindingManager> bindingManager = inLayoutUtils::GetBindingManagerFor(aNode);
          if (bindingManager) {
            bindingManager->GetAnonymousNodesFor(content, getter_AddRefs(kids));
            if (!kids)
              bindingManager->GetContentListFor(content, getter_AddRefs(kids));
          }
        } 
      }

      if (!kids)
        rv = aNode->GetChildNodes(getter_AddRefs(kids));
      if (NS_SUCCEEDED(rv))
        AppendKidsToArray(kids, result);
    }
  }

  *aResult = result;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

nsresult
inDOMView::GetRealParent(nsIDOMNode* aNode, nsIDOMNode** aParent)
{
  if (mShowSubDocuments && inLayoutUtils::IsDocumentElement(aNode)) {
    nsCOMPtr<nsIDOMDocument> doc;
    aNode->GetOwnerDocument(getter_AddRefs(doc));
    nsCOMPtr<nsIDOMNode> node = inLayoutUtils::GetContainerFor(doc);
    if (node)
      *aParent = node;
  }

  if (mShowAnonymous && !*aParent) {
    nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);
    nsCOMPtr<nsIContent> bparent;
    nsCOMPtr<nsIBindingManager> bindingManager = inLayoutUtils::GetBindingManagerFor(aNode);
    if (bindingManager)
      bindingManager->GetInsertionParent(content, getter_AddRefs(bparent));
      
    if (bparent) {
      nsCOMPtr<nsIDOMNode> parent = do_QueryInterface(bparent);
      *aParent = parent;
    }
  }
  
  if (!*aParent) {
    nsCOMPtr<nsIDOMNode> node;
    aNode->GetParentNode(getter_AddRefs(node));
    *aParent = node;
  }

  NS_IF_ADDREF(*aParent);
  return NS_OK;
}

nsresult
inDOMView::GetRealPreviousSibling(nsIDOMNode* aNode, nsIDOMNode* aRealParent, nsIDOMNode** aSibling)
{
  // XXXjrh: This won't work for some cases during some situations where XBL insertion points
  // are involved.  Fix me!
  aNode->GetPreviousSibling(aSibling);
  return NS_OK;
}

nsresult
inDOMView::AppendKidsToArray(nsIDOMNodeList* aKids, nsISupportsArray* aArray)
{
  PRUint32 l = 0;
  aKids->GetLength(&l);
  nsCOMPtr<nsIDOMNode> kid;
  PRUint16 nodeType = 0;
  PRBool filtered = PR_FALSE;
  for (PRUint32 i = 0; i < l; ++i) {
    aKids->Item(i, getter_AddRefs(kid));
    kid->GetNodeType(&nodeType);
    IsFiltered(nodeType, &filtered);
    if (filtered)
      aArray->AppendElement(kid);
  }
  return NS_OK;
}

nsresult
inDOMView::AppendAttrsToArray(nsIDOMNamedNodeMap* aKids, nsISupportsArray* aArray)
{
  PRUint32 l = 0;
  aKids->GetLength(&l);
  nsCOMPtr<nsIDOMNode> kid;
  for (PRUint32 i = 0; i < l; ++i) {
    aKids->Item(i, getter_AddRefs(kid));
    aArray->AppendElement(kid);
  }
  return NS_OK;
}

PRUint16
inDOMView::GetNodeTypeKey(PRUint16 aType)
{
  PRUint16 key = 1;
  for (PRUint32 i = 0; i < aType; ++i)
    key = key * 2;
  
  return key;
}
