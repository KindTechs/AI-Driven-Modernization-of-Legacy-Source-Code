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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

/* build on macs with low memory */
#if defined(XP_MAC) && defined(MOZ_MAC_LOWMEM)
#pragma optimization_level 1
#endif

#include "nsHTMLEditRules.h"

#include "nsEditor.h"
#include "nsTextEditUtils.h"
#include "nsHTMLEditUtils.h"
#include "nsHTMLEditor.h"
#include "TypeInState.h"

#include "nsIServiceManager.h"

#include "nsIContent.h"
#include "nsIContentIterator.h"
#include "nsIDOMNode.h"
#include "nsIDOMText.h"
#include "nsIDOMElement.h"
#include "nsIDOMNodeList.h"
#include "nsISelection.h"
#include "nsISelectionPrivate.h"
#include "nsISelectionController.h"
#include "nsIDOMRange.h"
#include "nsIDOMNSRange.h"
#include "nsIDOMCharacterData.h"
#include "nsIEnumerator.h"
#include "nsIStyleContext.h"
#include "nsIPresShell.h"
#include "nsLayoutCID.h"
#include "nsIPref.h"
#include "nsIDOMNamedNodeMap.h"

#include "nsEditorUtils.h"
#include "nsWSRunObject.h"

#include "InsertTextTxn.h"
#include "DeleteTextTxn.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"


//const static char* kMOZEditorBogusNodeAttr="MOZ_EDITOR_BOGUS_NODE";
//const static char* kMOZEditorBogusNodeValue="TRUE";
const static PRUnichar nbsp = 160;

static NS_DEFINE_IID(kContentIteratorCID, NS_CONTENTITERATOR_CID);
static NS_DEFINE_IID(kRangeCID, NS_RANGE_CID);
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);

enum
{
  kLonely = 0,
  kPrevSib = 1,
  kNextSib = 2,
  kBothSibs = 3
};

/********************************************************
 *  first some helpful funcotrs we will use
 ********************************************************/

static PRBool IsBlockNode(nsIDOMNode* node)
{
  PRBool isBlock (PR_FALSE);
  nsHTMLEditor::NodeIsBlockStatic(node, &isBlock);
  return isBlock;
}

static PRBool IsInlineNode(nsIDOMNode* node)
{
  return !IsBlockNode(node);
}
 
class nsTableCellAndListItemFunctor : public nsBoolDomIterFunctor
{
  public:
    virtual PRBool operator()(nsIDOMNode* aNode)  // used to build list of all li's, td's & th's iterator covers
    {
      if (nsHTMLEditUtils::IsTableCell(aNode)) return PR_TRUE;
      if (nsHTMLEditUtils::IsListItem(aNode)) return PR_TRUE;
      return PR_FALSE;
    }
};

class nsBRNodeFunctor : public nsBoolDomIterFunctor
{
  public:
    virtual PRBool operator()(nsIDOMNode* aNode)  // used to build list of all td's & th's iterator covers
    {
      if (nsTextEditUtils::IsBreak(aNode)) return PR_TRUE;
      return PR_FALSE;
    }
};

class nsEmptyFunctor : public nsBoolDomIterFunctor
{
  public:
    nsEmptyFunctor(nsHTMLEditor* editor) : mHTMLEditor(editor) {}
    virtual PRBool operator()(nsIDOMNode* aNode)  // used to build list of empty li's and td's
    {
      if (nsHTMLEditUtils::IsListItem(aNode) || nsHTMLEditUtils::IsTableCellOrCaption(aNode))
      {
        PRBool bIsEmptyNode;
        nsresult res = mHTMLEditor->IsEmptyNode(aNode, &bIsEmptyNode, PR_FALSE, PR_FALSE);
        if (NS_FAILED(res)) return PR_FALSE;
        if (bIsEmptyNode) 
          return PR_TRUE;
      }
      return PR_FALSE;
    }
  protected:
    nsHTMLEditor* mHTMLEditor;
};

class nsEditableTextFunctor : public nsBoolDomIterFunctor
{
  public:
    nsEditableTextFunctor(nsHTMLEditor* editor) : mHTMLEditor(editor) {}
    virtual PRBool operator()(nsIDOMNode* aNode)  // used to build list of empty li's and td's
    {
      if (nsEditor::IsTextNode(aNode) && mHTMLEditor->IsEditable(aNode)) 
      {
        return PR_TRUE;
      }
      return PR_FALSE;
    }
  protected:
    nsHTMLEditor* mHTMLEditor;
};


/********************************************************
 *  routine for making new rules instance
 ********************************************************/

nsresult
NS_NewHTMLEditRules(nsIEditRules** aInstancePtrResult)
{
  nsHTMLEditRules * rules = new nsHTMLEditRules();
  if (rules)
    return rules->QueryInterface(NS_GET_IID(nsIEditRules), (void**) aInstancePtrResult);
  return NS_ERROR_OUT_OF_MEMORY;
}

/********************************************************
 *  Constructor/Destructor 
 ********************************************************/

nsHTMLEditRules::nsHTMLEditRules() : 
mDocChangeRange(nsnull)
,mListenerEnabled(PR_TRUE)
,mReturnInEmptyLIKillsList(PR_TRUE)
,mUtilRange(nsnull)
,mJoinOffset(0)
{
}

nsHTMLEditRules::~nsHTMLEditRules()
{
  // remove ourselves as a listener to edit actions
  // In the normal case, we have already been removed by 
  // ~nsHTMLEditor, in which case we will get an error here
  // which we ignore.  But this allows us to add the ability to
  // switch rule sets on the fly if we want.
  mHTMLEditor->RemoveEditActionListener(this);
}

/********************************************************
 *  XPCOM Cruft
 ********************************************************/

NS_IMPL_ADDREF_INHERITED(nsHTMLEditRules, nsTextEditRules)
NS_IMPL_RELEASE_INHERITED(nsHTMLEditRules, nsTextEditRules)
NS_IMPL_QUERY_INTERFACE3(nsHTMLEditRules, nsIHTMLEditRules, nsIEditRules, nsIEditActionListener)


/********************************************************
 *  Public methods 
 ********************************************************/

NS_IMETHODIMP
nsHTMLEditRules::Init(nsPlaintextEditor *aEditor, PRUint32 aFlags)
{
  mHTMLEditor = NS_STATIC_CAST(nsHTMLEditor*, aEditor);
  nsresult res;
  
  // call through to base class Init 
  res = nsTextEditRules::Init(aEditor, aFlags);
  if (NS_FAILED(res)) return res;

  // cache any prefs we care about
  nsCOMPtr<nsIPref> prefs(do_GetService(kPrefServiceCID, &res));
  if (NS_FAILED(res)) return res;

  char *returnInEmptyLIKillsList = 0;
  res = prefs->CopyCharPref("editor.html.typing.returnInEmptyListItemClosesList",
                               &returnInEmptyLIKillsList);
                          
  if (NS_SUCCEEDED(res) && returnInEmptyLIKillsList)
  {
    if (!strncmp(returnInEmptyLIKillsList, "false", 5))
      mReturnInEmptyLIKillsList = PR_FALSE; 
    else
      mReturnInEmptyLIKillsList = PR_TRUE; 
  }
  else
  {
    mReturnInEmptyLIKillsList = PR_TRUE; 
  }
  
  // make a utility range for use by the listenter
  mUtilRange = do_CreateInstance(kRangeCID);
  if (!mUtilRange) return NS_ERROR_NULL_POINTER;
   
  // set up mDocChangeRange to be whole doc
  nsCOMPtr<nsIDOMElement> bodyElem;
  nsCOMPtr<nsIDOMNode> bodyNode;
  mHTMLEditor->GetRootElement(getter_AddRefs(bodyElem));
  bodyNode = do_QueryInterface(bodyElem);
  if (bodyNode)
  {
    // temporarily turn off rules sniffing
    nsAutoLockRulesSniffing lockIt((nsTextEditRules*)this);
    if (!mDocChangeRange)
    {
      mDocChangeRange = do_CreateInstance(kRangeCID);
      if (!mDocChangeRange) return NS_ERROR_NULL_POINTER;
    }
    mDocChangeRange->SelectNode(bodyNode);
    res = AdjustSpecialBreaks();
    if (NS_FAILED(res)) return res;
  }

  // add ourselves as a listener to edit actions
  res = mHTMLEditor->AddEditActionListener(this);

  return res;
}


NS_IMETHODIMP
nsHTMLEditRules::BeforeEdit(PRInt32 action, nsIEditor::EDirection aDirection)
{
  if (mLockRulesSniffing) return NS_OK;
  
  nsAutoLockRulesSniffing lockIt((nsTextEditRules*)this);
  
  if (!mActionNesting)
  {
    nsCOMPtr<nsIDOMNSRange> nsrange;
    if(mDocChangeRange)
    {
      nsrange = do_QueryInterface(mDocChangeRange);
      if (!nsrange)
        return NS_ERROR_FAILURE;
      nsrange->NSDetach();  // clear out our accounting of what changed
    }
    if(mUtilRange)
    {
      nsrange = do_QueryInterface(mUtilRange);
      if (!nsrange)
        return NS_ERROR_FAILURE;
      nsrange->NSDetach();  // ditto for mUtilRange.  
    }
    // turn off caret
    nsCOMPtr<nsISelectionController> selCon;
    mHTMLEditor->GetSelectionController(getter_AddRefs(selCon));
    if (selCon) selCon->SetCaretEnabled(PR_FALSE);
    // check that selection is in subtree defined by body node
    ConfirmSelectionInBody();
    // let rules remember the top level action
    mTheAction = action;
  }
  mActionNesting++;
  return NS_OK;
}


NS_IMETHODIMP
nsHTMLEditRules::AfterEdit(PRInt32 action, nsIEditor::EDirection aDirection)
{
  if (mLockRulesSniffing) return NS_OK;

  nsAutoLockRulesSniffing lockIt(this);

  NS_PRECONDITION(mActionNesting>0, "bad action nesting!");
  nsresult res = NS_OK;
  if (!--mActionNesting)
  {
    // do all the tricky stuff
    res = AfterEditInner(action, aDirection);
    // turn on caret
    nsCOMPtr<nsISelectionController> selCon;
    mHTMLEditor->GetSelectionController(getter_AddRefs(selCon));
    if (selCon) selCon->SetCaretEnabled(PR_TRUE);
#ifdef IBMBIDI
    /* After inserting text the cursor Bidi level must be set to the level of the inserted text.
     * This is difficult, because we cannot know what the level is until after the Bidi algorithm
     * is applied to the whole paragraph.
     *
     * So we set the cursor Bidi level to UNDEFINED here, and the caret code will set it correctly later
     */
    if (action == nsEditor::kOpInsertText) {
      nsCOMPtr<nsIPresShell> shell;
      mEditor->GetPresShell(getter_AddRefs(shell));
      if (shell) {
        shell->UndefineCaretBidiLevel();
      }
    }
#endif
  }

  return res;
}


nsresult
nsHTMLEditRules::AfterEditInner(PRInt32 action, nsIEditor::EDirection aDirection)
{
  ConfirmSelectionInBody();
  if (action == nsEditor::kOpIgnore) return NS_OK;
  
  nsCOMPtr<nsISelection>selection;
  nsresult res = mHTMLEditor->GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  
  // do we have a real range to act on?
  PRBool bDamagedRange = PR_FALSE;  
  if (mDocChangeRange)
  {  
    nsCOMPtr<nsIDOMNode> rangeStartParent, rangeEndParent;
    mDocChangeRange->GetStartContainer(getter_AddRefs(rangeStartParent));
    mDocChangeRange->GetEndContainer(getter_AddRefs(rangeEndParent));
    if (rangeStartParent && rangeEndParent) 
      bDamagedRange = PR_TRUE; 
  }
  
  if (bDamagedRange && !((action == nsEditor::kOpUndo) || (action == nsEditor::kOpRedo)))
  {
    // dont let any txns in here move the selection around behind our back.
    // Note that this won't prevent explicit selection setting from working.
    nsAutoTxnsConserveSelection dontSpazMySelection(mHTMLEditor);
   
    // expand the "changed doc range" as needed
    res = PromoteRange(mDocChangeRange, action);
    if (NS_FAILED(res)) return res;
    
    // add in any needed <br>s, and remove any unneeded ones.
    res = AdjustSpecialBreaks();
    if (NS_FAILED(res)) return res;
    
    // merge any adjacent text nodes
    if ( (action != nsEditor::kOpInsertText &&
         action != nsEditor::kOpInsertIMEText) )
    {
      res = mHTMLEditor->CollapseAdjacentTextNodes(mDocChangeRange);
      if (NS_FAILED(res)) return res;
    }
    
    // replace newlines with breaks.
    // MOOSE:  This is buttUgly.  A better way to 
    // organize the action enum is in order.
    if (// (action == nsEditor::kOpInsertText) || 
        // (action == nsEditor::kOpInsertIMEText) ||
        (action == nsHTMLEditor::kOpInsertElement) ||
        (action == nsHTMLEditor::kOpInsertQuotation) ||
        (action == nsEditor::kOpInsertNode) ||
        (action == nsHTMLEditor::kOpHTMLPaste ||
        (action == nsHTMLEditor::kOpLoadHTML)))
    {
      res = ReplaceNewlines(mDocChangeRange);
    }
    
    // clean up any empty nodes in the selection
    res = RemoveEmptyNodes();
    if (NS_FAILED(res)) return res;

    // attempt to transform any uneeded nbsp's into spaces after doing deletions
    if (action == nsEditor::kOpDeleteSelection)
    {
      res = AdjustWhitespace(selection);
      if (NS_FAILED(res)) return res;
    }
    
    // if we created a new block, make sure selection lands in it
    if (mNewBlock)
    {
      res = PinSelectionToNewBlock(selection);
      mNewBlock = 0;
    }

    // adjust selection for insert text, html paste, and delete actions
    if ((action == nsEditor::kOpInsertText) || 
        (action == nsEditor::kOpInsertIMEText) ||
        (action == nsEditor::kOpDeleteSelection) ||
        (action == nsEditor::kOpInsertBreak) || 
        (action == nsHTMLEditor::kOpHTMLPaste ||
        (action == nsHTMLEditor::kOpLoadHTML)))
    {
      res = AdjustSelection(selection, aDirection);
      if (NS_FAILED(res)) return res;
    }
    
  }

  // detect empty doc
  res = CreateBogusNodeIfNeeded(selection);
  
  // adjust selection HINT if needed
  if (NS_FAILED(res)) return res;
  res = CheckInterlinePosition(selection);
  return res;
}


NS_IMETHODIMP 
nsHTMLEditRules::WillDoAction(nsISelection *aSelection, 
                              nsRulesInfo *aInfo, 
                              PRBool *aCancel, 
                              PRBool *aHandled)
{
  if (!aInfo || !aCancel || !aHandled) 
    return NS_ERROR_NULL_POINTER;
#if defined(DEBUG_ftang)
  printf("nsHTMLEditRules::WillDoAction action = %d\n", aInfo->action);
#endif

  *aCancel = PR_FALSE;
  *aHandled = PR_FALSE;
    
  // my kingdom for dynamic cast
  nsTextRulesInfo *info = NS_STATIC_CAST(nsTextRulesInfo*, aInfo);
    
  switch (info->action)
  {
    case kInsertText:
    case kInsertTextIME:
      return WillInsertText(info->action,
                            aSelection, 
                            aCancel, 
                            aHandled,
                            info->inString,
                            info->outString,
                            info->maxLength);
    case kLoadHTML:
      return WillLoadHTML(aSelection, aCancel);
    case kInsertBreak:
      return WillInsertBreak(aSelection, aCancel, aHandled);
    case kDeleteSelection:
      return WillDeleteSelection(aSelection, info->collapsedAction, aCancel, aHandled);
    case kMakeList:
      return WillMakeList(aSelection, info->blockType, info->entireList, info->bulletType, aCancel, aHandled);
    case kIndent:
      return WillIndent(aSelection, aCancel, aHandled);
    case kOutdent:
      return WillOutdent(aSelection, aCancel, aHandled);
    case kAlign:
      return WillAlign(aSelection, info->alignType, aCancel, aHandled);
    case kMakeBasicBlock:
      return WillMakeBasicBlock(aSelection, info->blockType, aCancel, aHandled);
    case kRemoveList:
      return WillRemoveList(aSelection, info->bOrdered, aCancel, aHandled);
    case kMakeDefListItem:
      return WillMakeDefListItem(aSelection, info->blockType, info->entireList, aCancel, aHandled);
    case kInsertElement:
      return WillInsert(aSelection, aCancel);
  }
  return nsTextEditRules::WillDoAction(aSelection, aInfo, aCancel, aHandled);
}
  
  
NS_IMETHODIMP 
nsHTMLEditRules::DidDoAction(nsISelection *aSelection,
                             nsRulesInfo *aInfo, nsresult aResult)
{
  nsTextRulesInfo *info = NS_STATIC_CAST(nsTextRulesInfo*, aInfo);
  switch (info->action)
  {
    case kInsertBreak:
      return DidInsertBreak(aSelection, aResult);
    case kMakeBasicBlock:
    case kIndent:
    case kOutdent:
    case kAlign:
      return DidMakeBasicBlock(aSelection, aInfo, aResult);
  }
  
  // default: pass thru to nsTextEditRules
  return nsTextEditRules::DidDoAction(aSelection, aInfo, aResult);
}
  
/********************************************************
 *  nsIHTMLEditRules methods
 ********************************************************/

NS_IMETHODIMP 
nsHTMLEditRules::GetListState(PRBool *aMixed, PRBool *aOL, PRBool *aUL, PRBool *aDL)
{
  if (!aMixed || !aOL || !aUL || !aDL)
    return NS_ERROR_NULL_POINTER;
  *aMixed = PR_FALSE;
  *aOL = PR_FALSE;
  *aUL = PR_FALSE;
  *aDL = PR_FALSE;
  PRBool bNonList = PR_FALSE;
  
  nsCOMPtr<nsISupportsArray> arrayOfNodes;
  nsresult res = GetListActionNodes(address_of(arrayOfNodes), PR_FALSE, PR_TRUE);
  if (NS_FAILED(res)) return res;

  // examine list type for nodes in selection
  PRUint32 listCount;
  PRInt32 i;
  arrayOfNodes->Count(&listCount);
  for (i=(PRInt32)listCount-1; i>=0; i--)
  {
    nsCOMPtr<nsISupports> isupports = dont_AddRef(arrayOfNodes->ElementAt(i));
    nsCOMPtr<nsIDOMNode> curNode( do_QueryInterface(isupports) );
    
    if (mHTMLEditor->NodeIsType(curNode,nsIEditProperty::ul))
      *aUL = PR_TRUE;
    else if (mHTMLEditor->NodeIsType(curNode,nsIEditProperty::ol))
      *aOL = PR_TRUE;
    else if (mHTMLEditor->NodeIsType(curNode,nsIEditProperty::li))
    {
      nsCOMPtr<nsIDOMNode> parent;
      PRInt32 offset;
      res = nsEditor::GetNodeLocation(curNode, address_of(parent), &offset);
      if (NS_FAILED(res)) return res;
      if (nsHTMLEditUtils::IsUnorderedList(parent))
        *aUL = PR_TRUE;
      else if (nsHTMLEditUtils::IsOrderedList(parent))
        *aOL = PR_TRUE;
    }
    else if (mHTMLEditor->NodeIsType(curNode,nsIEditProperty::dl) ||
             mHTMLEditor->NodeIsType(curNode,nsIEditProperty::dt) ||
             mHTMLEditor->NodeIsType(curNode,nsIEditProperty::dd) )
    {
      *aDL = PR_TRUE;
    }
    else bNonList = PR_TRUE;
  }  
  
  // hokey arithmetic with booleans
  if ( (*aUL + *aOL + *aDL + bNonList) > 1) *aMixed = PR_TRUE;
  
  return res;
}

NS_IMETHODIMP 
nsHTMLEditRules::GetListItemState(PRBool *aMixed, PRBool *aLI, PRBool *aDT, PRBool *aDD)
{
  if (!aMixed || !aLI || !aDT || !aDD)
    return NS_ERROR_NULL_POINTER;
  *aMixed = PR_FALSE;
  *aLI = PR_FALSE;
  *aDT = PR_FALSE;
  *aDD = PR_FALSE;
  PRBool bNonList = PR_FALSE;
  
  nsCOMPtr<nsISupportsArray> arrayOfNodes;
  nsresult res = GetListActionNodes(address_of(arrayOfNodes), PR_FALSE, PR_TRUE);
  if (NS_FAILED(res)) return res;

  // examine list type for nodes in selection
  PRUint32 listCount;
  PRInt32 i;
  arrayOfNodes->Count(&listCount);
  for (i=(PRInt32)listCount-1; i>=0; i--)
  {
    nsCOMPtr<nsISupports> isupports = dont_AddRef(arrayOfNodes->ElementAt(i));
    nsCOMPtr<nsIDOMNode> curNode( do_QueryInterface(isupports) );
    
    if (mHTMLEditor->NodeIsType(curNode,nsIEditProperty::ul) ||
        mHTMLEditor->NodeIsType(curNode,nsIEditProperty::ol) ||
        mHTMLEditor->NodeIsType(curNode,nsIEditProperty::li) )
    {
      *aLI = PR_TRUE;
    }
    else if (mHTMLEditor->NodeIsType(curNode,nsIEditProperty::dt))
    {
      *aDT = PR_TRUE;
    }
    else if (mHTMLEditor->NodeIsType(curNode,nsIEditProperty::dd))
    {
      *aDD = PR_TRUE;
    }
    else if (mHTMLEditor->NodeIsType(curNode,nsIEditProperty::dl))
    {
      // need to look inside dl and see which types of items it has
      PRBool bDT, bDD;
      res = GetDefinitionListItemTypes(curNode, bDT, bDD);
      if (NS_FAILED(res)) return res;
      *aDT |= bDT;
      *aDD |= bDD;
    }
    else bNonList = PR_TRUE;
  }  
  
  // hokey arithmetic with booleans
  if ( (*aDT + *aDD + bNonList) > 1) *aMixed = PR_TRUE;
  
  return res;
}

NS_IMETHODIMP 
nsHTMLEditRules::GetAlignment(PRBool *aMixed, nsIHTMLEditor::EAlignment *aAlign)
{
  // for now, just return first alignment.  we'll lie about
  // if it's mixed.  This is for efficiency
  // given that our current ui doesn't care if it's mixed.
  // cmanske: NOT TRUE! We would like to pay attention to mixed state
  //  in Format | Align submenu!

  // this routine assumes that alignment is done ONLY via divs

  // default alignment is left
  if (!aMixed || !aAlign)
    return NS_ERROR_NULL_POINTER;
  *aMixed = PR_FALSE;
  *aAlign = nsIHTMLEditor::eLeft;

  // get selection
  nsCOMPtr<nsISelection>selection;
  nsresult res = mHTMLEditor->GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;

  // get selection location
  nsCOMPtr<nsIDOMNode> parent;
  nsCOMPtr<nsIDOMElement> rootElem;
  PRInt32 offset, rootOffset;
  res = mHTMLEditor->GetRootElement(getter_AddRefs(rootElem));
  if (NS_FAILED(res)) return res;
  res = nsEditor::GetNodeLocation(rootElem, address_of(parent), &rootOffset);
  if (NS_FAILED(res)) return res;
  res = mHTMLEditor->GetStartNodeAndOffset(selection, address_of(parent), &offset);
  if (NS_FAILED(res)) return res;

  // is the selection collapsed?
  PRBool bCollapsed;
  res = selection->GetIsCollapsed(&bCollapsed);
  if (NS_FAILED(res)) return res;
  nsCOMPtr<nsIDOMNode> nodeToExamine;
  nsCOMPtr<nsISupports> isupports;
  if (bCollapsed)
  {
    // if it is, we want to look at 'parent' and it's ancestors
    // for divs with alignment on them
    nodeToExamine = parent;
  }
  else if (mHTMLEditor->IsTextNode(parent)) 
  {
    // if we are in a text node, then that is the node of interest
    nodeToExamine = parent;
  }
  else if (nsTextEditUtils::NodeIsType(parent,NS_LITERAL_STRING("html")) &&
           offset == rootOffset)
  {
    // if we have selected the body, let's look at the first editable node
    mHTMLEditor->GetNextNode(parent, offset, PR_TRUE, address_of(nodeToExamine));
  }
  else
  {
    nsCOMPtr<nsISupportsArray> arrayOfRanges;
    res = GetPromotedRanges(selection, address_of(arrayOfRanges), kAlign);
    if (NS_FAILED(res)) return res;

    // use these ranges to contruct a list of nodes to act on.
    nsCOMPtr<nsISupportsArray> arrayOfNodes;
    res = GetNodesForOperation(arrayOfRanges, address_of(arrayOfNodes), kAlign, PR_TRUE);
    if (NS_FAILED(res)) return res;                                 
    isupports = dont_AddRef(arrayOfNodes->ElementAt(0));
    nodeToExamine = do_QueryInterface(isupports );
  }

  if (!nodeToExamine) return NS_ERROR_NULL_POINTER;

  PRBool useCSS;
  mHTMLEditor->GetIsCSSEnabled(&useCSS);
  NS_NAMED_LITERAL_STRING(typeAttrName, "align");
  nsIAtom  *dummyProperty = nsnull;
  if (useCSS && mHTMLEditor->mHTMLCSSUtils->IsCSSEditableProperty(nodeToExamine, dummyProperty, &typeAttrName))
  {
    // we are in CSS mode and we know how to align this element with CSS
    nsAutoString value;
    // let's get the value(s) of text-align or margin-left/margin-right
    mHTMLEditor->mHTMLCSSUtils->GetCSSEquivalentToHTMLInlineStyleSet(nodeToExamine,
                                                     dummyProperty,
                                                     &typeAttrName,
                                                     value,
                                                     COMPUTED_STYLE_TYPE);
    if (value.Equals(NS_LITERAL_STRING("center")) ||
        value.Equals(NS_LITERAL_STRING("-moz-center")) ||
        value.Equals(NS_LITERAL_STRING("auto auto")))
    {
      *aAlign = nsIHTMLEditor::eCenter;
      return NS_OK;
    }
    else if (value.Equals(NS_LITERAL_STRING("right")) ||
             value.Equals(NS_LITERAL_STRING("-moz-right")) ||
             value.Equals(NS_LITERAL_STRING("auto 0px")))
    {
      *aAlign = nsIHTMLEditor::eRight;
      return NS_OK;
    }
    else if (value.Equals(NS_LITERAL_STRING("justify")))
    {
      *aAlign = nsIHTMLEditor::eJustify;
      return NS_OK;
    }
    else    {
      *aAlign = nsIHTMLEditor::eLeft;
      return NS_OK;
    }
  }

  // check up the ladder for divs with alignment
  nsCOMPtr<nsIDOMNode> temp = nodeToExamine;
  PRBool isFirstNodeToExamine = PR_TRUE;
  while (nodeToExamine)
  {
    if (!isFirstNodeToExamine && nsHTMLEditUtils::IsTable(nodeToExamine))
    {
      // the node to examine is a table and this is not the first node
      // we examine; let's break here to materialize the 'inline-block'
      // behaviour of html tables regarding to text alignment
      return NS_OK;
    }
    if (nsHTMLEditUtils::SupportsAlignAttr(nodeToExamine))
    {
      // check for alignment
      nsCOMPtr<nsIDOMElement> elem = do_QueryInterface(nodeToExamine);
      if (elem)
      {
        nsAutoString typeAttrVal;
        res = elem->GetAttribute(NS_LITERAL_STRING("align"), typeAttrVal);
        ToLowerCase(typeAttrVal);
        if (NS_SUCCEEDED(res) && typeAttrVal.Length())
        {
          if (typeAttrVal.Equals(NS_LITERAL_STRING("center")))
            *aAlign = nsIHTMLEditor::eCenter;
          else if (typeAttrVal.Equals(NS_LITERAL_STRING("right")))
            *aAlign = nsIHTMLEditor::eRight;
          else if (typeAttrVal.Equals(NS_LITERAL_STRING("justify")))
            *aAlign = nsIHTMLEditor::eJustify;
          else
            *aAlign = nsIHTMLEditor::eLeft;
          return res;
        }
      }
    }
    isFirstNodeToExamine = PR_FALSE;
    res = nodeToExamine->GetParentNode(getter_AddRefs(temp));
    if (NS_FAILED(res)) temp = nsnull;
    nodeToExamine = temp; 
  }
  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLEditRules::GetIndentState(PRBool *aCanIndent, PRBool *aCanOutdent)
{
  if (!aCanIndent || !aCanOutdent)
      return NS_ERROR_FAILURE;
  *aCanIndent = PR_TRUE;    
  *aCanOutdent = PR_FALSE;

  // get selection
  nsCOMPtr<nsISelection>selection;
  nsresult res = mHTMLEditor->GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  nsCOMPtr<nsISelectionPrivate> selPriv(do_QueryInterface(selection));
  if (!selPriv)
    return NS_ERROR_FAILURE;

  // contruct a list of nodes to act on.
  nsCOMPtr<nsISupportsArray> arrayOfNodes;
  res = GetNodesFromSelection(selection, kIndent, address_of(arrayOfNodes), PR_TRUE);
  if (NS_FAILED(res)) return res;

  // examine nodes in selection for blockquotes or list elements;
  // these we can outdent.  Note that we return true for canOutdent
  // if *any* of the selection is outdentable, rather than all of it.
  PRUint32 listCount;
  PRInt32 i;
  arrayOfNodes->Count(&listCount);
  PRBool useCSS;
  mHTMLEditor->GetIsCSSEnabled(&useCSS);
  for (i=(PRInt32)listCount-1; i>=0; i--)
  {
    nsCOMPtr<nsISupports> isupports = dont_AddRef(arrayOfNodes->ElementAt(i));
    nsCOMPtr<nsIDOMNode> curNode( do_QueryInterface(isupports) );
    
    if (nsHTMLEditUtils::IsList(curNode)     || 
        nsHTMLEditUtils::IsListItem(curNode) ||
        nsHTMLEditUtils::IsBlockquote(curNode))
    {
      *aCanOutdent = PR_TRUE;
      break;
    }
    else if (useCSS) {
      // we are in CSS mode, indentation is done using the margin-left property
      nsAutoString value;
      // retrieve its specified value
      mHTMLEditor->mHTMLCSSUtils->GetSpecifiedProperty(curNode, nsIEditProperty::cssMarginLeft, value);
      float f;
      nsIAtom * unit;
      // get its number part and its unit
      mHTMLEditor->mHTMLCSSUtils->ParseLength(value, &f, &unit);
      NS_IF_RELEASE(unit);
      // if the number part is strictly positive, outdent is possible
      if (0 < f) {
        *aCanOutdent = PR_TRUE;
        break;
      }
    }
  }  
  
  if (!*aCanOutdent)
  {
    // if we haven't found something to outdent yet, also check the parents
    // of selection endpoints.  We might have a blockquote or list item 
    // in the parent heirarchy.
    
    // gather up info we need for test
    nsCOMPtr<nsIDOMNode> parent, tmp, root;
    nsCOMPtr<nsIDOMElement> rootElem;
    nsCOMPtr<nsISelection> selection;
    PRInt32 selOffset;
    res = mHTMLEditor->GetRootElement(getter_AddRefs(rootElem));
    if (NS_FAILED(res)) return res;
    if (!rootElem) return NS_ERROR_NULL_POINTER;
    root = do_QueryInterface(rootElem);
    if (!root) return NS_ERROR_NO_INTERFACE;
    res = mHTMLEditor->GetSelection(getter_AddRefs(selection));
    if (NS_FAILED(res)) return res;
    if (!selection) return NS_ERROR_NULL_POINTER;
    
    // test start parent heirachy
    res = mHTMLEditor->GetStartNodeAndOffset(selection, address_of(parent), &selOffset);
    if (NS_FAILED(res)) return res;
    while (parent && (parent!=root))
    {
      if (nsHTMLEditUtils::IsList(parent)     || 
          nsHTMLEditUtils::IsListItem(parent) ||
          nsHTMLEditUtils::IsBlockquote(parent))
      {
        *aCanOutdent = PR_TRUE;
        break;
      }
      tmp=parent;
      tmp->GetParentNode(getter_AddRefs(parent));
    }

    // test end parent heirachy
    res = mHTMLEditor->GetEndNodeAndOffset(selection, address_of(parent), &selOffset);
    if (NS_FAILED(res)) return res;
    while (parent && (parent!=root))
    {
      if (nsHTMLEditUtils::IsList(parent)     || 
          nsHTMLEditUtils::IsListItem(parent) ||
          nsHTMLEditUtils::IsBlockquote(parent))
      {
        *aCanOutdent = PR_TRUE;
        break;
      }
      tmp=parent;
      tmp->GetParentNode(getter_AddRefs(parent));
    }
  }
  return res;
}


NS_IMETHODIMP 
nsHTMLEditRules::GetParagraphState(PRBool *aMixed, nsAString &outFormat)
{
  // This routine is *heavily* tied to our ui choices in the paragraph
  // style popup.  I cant see a way around that.
  if (!aMixed)
    return NS_ERROR_NULL_POINTER;
  *aMixed = PR_TRUE;
  outFormat.Truncate(0);
  
  PRBool bMixed = PR_FALSE;
  // using "x" as an uninitialized value, since "" is meaningful
  nsAutoString formatStr(NS_LITERAL_STRING("x")); 
  
  nsCOMPtr<nsISupportsArray> arrayOfNodes;
  nsresult res = GetParagraphFormatNodes(address_of(arrayOfNodes), PR_TRUE);
  if (NS_FAILED(res)) return res;

  // post process list.  We need to replace any block nodes that are not format
  // nodes with their content.  This is so we only have to look "up" the heirarchy
  // to find format nodes, instead of both up and down.
  nsCOMPtr<nsISupports> isupports;
  PRUint32 listCount;
  arrayOfNodes->Count(&listCount);
  PRInt32 i;
  for (i=(PRInt32)listCount-1; i>=0; i--)
  {
    isupports = dont_AddRef(arrayOfNodes->ElementAt(i));
    nsCOMPtr<nsIDOMNode> curNode( do_QueryInterface(isupports) );
    // if it is a known format node we have it easy
    if (IsBlockNode(curNode) && !IsFormatNode(curNode))
    {
      // Otherwise we have to add it's content to the list.
      // As a performance opt, we dont remove the node itself from list
      // (which would memcpy everything beyond that in the list).
      // arrayOfNodes->RemoveElement(isupports);
      res = AppendInnerFormatNodes(arrayOfNodes, curNode);
      if (NS_FAILED(res)) return res;
    }
  }
  
  // we might have an empty node list.  if so, find selection parent
  // and put that on the list
  arrayOfNodes->Count(&listCount);
  if (!listCount)
  {
    nsCOMPtr<nsIDOMNode> selNode;
    PRInt32 selOffset;
    nsCOMPtr<nsISelection>selection;
    res = mHTMLEditor->GetSelection(getter_AddRefs(selection));
    if (NS_FAILED(res)) return res;
    res = mHTMLEditor->GetStartNodeAndOffset(selection, address_of(selNode), &selOffset);
    if (NS_FAILED(res)) return res;
    isupports = do_QueryInterface(selNode);
    if (!isupports) return NS_ERROR_NULL_POINTER;
    arrayOfNodes->AppendElement(isupports);
    listCount = 1;
  }

  // remember root node
  nsCOMPtr<nsIDOMElement> rootElem;
  res = mHTMLEditor->GetRootElement(getter_AddRefs(rootElem));
  if (NS_FAILED(res)) return res;
  if (!rootElem) return NS_ERROR_NULL_POINTER;

  // loop through the nodes in selection and examine their paragraph format
  for (i=(PRInt32)listCount-1; i>=0; i--)
  {
    isupports = dont_AddRef(arrayOfNodes->ElementAt(i));
    nsCOMPtr<nsIDOMNode> curNode( do_QueryInterface(isupports) );
    if (!curNode) return NS_ERROR_NULL_POINTER;
    nsAutoString format;
    // if it is a known format node we have it easy
    if (IsFormatNode(curNode))
      GetFormatString(curNode, format);
    else if (IsBlockNode(curNode))
    {
      // this is a div or some other non-format block.
      // we should ignore it.  It's children were appended to this list
      // by AppendInnerFormatNodes() call above.  We will get needed
      // info when we examine them instead.
      continue;
    }
    else
    {
      nsCOMPtr<nsIDOMNode> node, tmp = curNode;
      tmp->GetParentNode(getter_AddRefs(node));
      while (node)
      {
        if (node == rootElem)
        {
          format.Truncate(0);
          break;
        }
        else if (IsFormatNode(node))
        {
          GetFormatString(node, format);
          break;
        }
        // else keep looking up
        tmp = node;
        tmp->GetParentNode(getter_AddRefs(node));
      }
    }
    
    // if this is the first node, we've found, remember it as the format
    if (formatStr.Equals(NS_LITERAL_STRING("x")))
      formatStr = format;
    // else make sure it matches previously found format
    else if (format != formatStr) 
    {
      bMixed = PR_TRUE;
      break; 
    }
  }  
  
  *aMixed = bMixed;
  outFormat = formatStr;
  return res;
}

PRBool 
nsHTMLEditRules::IsFormatNode(nsIDOMNode *aNode)
{
  if (!aNode) return NS_ERROR_NULL_POINTER;
  if (nsHTMLEditUtils::IsParagraph(aNode)  ||
      nsHTMLEditUtils::IsPre(aNode)  ||
      nsHTMLEditUtils::IsHeader(aNode)  ||
      nsHTMLEditUtils::IsAddress(aNode) )
    return PR_TRUE;
  return PR_FALSE;
}

nsresult 
nsHTMLEditRules::AppendInnerFormatNodes(nsISupportsArray *aArray, nsIDOMNode *aNode)
{
  if (!aArray || !aNode) return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIDOMNodeList> childList;
  nsCOMPtr<nsIDOMNode> child;
  nsCOMPtr<nsISupports> isupports;

  aNode->GetChildNodes(getter_AddRefs(childList));
  if (!childList)  return NS_OK;
  PRUint32 len, j=0;
  childList->GetLength(&len);

  // we only need to place any one inline inside this node onto 
  // the list.  They are all the same for purposes of determining
  // paragraph style.  We use foundInline to track this as we are 
  // going through the children in the loop below.
  PRBool foundInline = PR_FALSE;
  while (j < len)
  {
    childList->Item(j, getter_AddRefs(child));
    PRBool isBlock = IsBlockNode(child);
    PRBool isFormat = IsFormatNode(child);
    if (isBlock && !isFormat)  // if it's a div, etc, recurse
      AppendInnerFormatNodes(aArray, child);
    else if (isFormat)
    {
      isupports = do_QueryInterface(child);
      aArray->AppendElement(isupports);
    }
    else if (!foundInline)  // if this is the first inline we've found, use it
    {
      foundInline = PR_TRUE;      
      isupports = do_QueryInterface(child);
      aArray->AppendElement(isupports);
    }
    j++;
  }
  return NS_OK;
}

nsresult 
nsHTMLEditRules::GetFormatString(nsIDOMNode *aNode, nsAString &outFormat)
{
  if (!aNode) return NS_ERROR_NULL_POINTER;
  
  nsCOMPtr<nsIAtom> atom = mHTMLEditor->GetTag(aNode);
  
  if ( nsIEditProperty::p == atom.get()           ||
       nsIEditProperty::address == atom.get()     ||
       nsIEditProperty::pre == atom.get()           )
  {
    atom->ToString(outFormat);
  }
  else if (nsHTMLEditUtils::IsHeader(aNode))
  {
    nsEditor::GetTagString(aNode,outFormat);
    ToLowerCase(outFormat);
  }
  else
  {
    outFormat.Truncate(0);
  }
  
  return NS_OK;
}    

/********************************************************
 *  Protected rules methods 
 ********************************************************/

nsresult
nsHTMLEditRules::WillInsert(nsISelection *aSelection, PRBool *aCancel)
{
  nsresult res = nsTextEditRules::WillInsert(aSelection, aCancel);
  if (NS_FAILED(res)) return res; 
  
  // Adjust selection to prevent insertion after a moz-BR.
  // this next only works for collapsed selections right now,
  // because selection is a pain to work with when not collapsed.
  // (no good way to extend start or end of selection)
  PRBool bCollapsed;
  res = aSelection->GetIsCollapsed(&bCollapsed);
  if (NS_FAILED(res)) return res;
  if (!bCollapsed) return NS_OK;

  // if we are after a mozBR in the same block, then move selection
  // to be before it
  nsCOMPtr<nsIDOMNode> selNode, priorNode;
  PRInt32 selOffset;
  // get the (collapsed) selection location
  res = mHTMLEditor->GetStartNodeAndOffset(aSelection, address_of(selNode),
                                           &selOffset);
  if (NS_FAILED(res)) return res;
  // get prior node
  res = mHTMLEditor->GetPriorHTMLNode(selNode, selOffset,
                                      address_of(priorNode));
  if (NS_SUCCEEDED(res) && priorNode && nsTextEditUtils::IsMozBR(priorNode))
  {
    nsCOMPtr<nsIDOMNode> block1, block2;
    if (IsBlockNode(selNode)) block1 = selNode;
    else block1 = mHTMLEditor->GetBlockNodeParent(selNode);
    block2 = mHTMLEditor->GetBlockNodeParent(priorNode);
  
    if (block1 == block2)
    {
      // if we are here then the selection is right after a mozBR
      // that is in the same block as the selection.  We need to move
      // the selection start to be before the mozBR.
      res = nsEditor::GetNodeLocation(priorNode, address_of(selNode), &selOffset);
      if (NS_FAILED(res)) return res;
      res = aSelection->Collapse(selNode,selOffset);
      if (NS_FAILED(res)) return res;
    }
  }

  // we need to get the doc
  nsCOMPtr<nsIDOMDocument>doc;
  res = mHTMLEditor->GetDocument(getter_AddRefs(doc));
  if (NS_FAILED(res)) return res;
  if (!doc) return NS_ERROR_NULL_POINTER;
    
  // for every property that is set, insert a new inline style node
  return CreateStyleForInsertText(aSelection, doc);
}    

#ifdef XXX_DEAD_CODE
nsresult
nsHTMLEditRules::DidInsert(nsISelection *aSelection, nsresult aResult)
{
  return nsTextEditRules::DidInsert(aSelection, aResult);
}
#endif

nsresult
nsHTMLEditRules::WillInsertText(PRInt32          aAction,
                                nsISelection *aSelection, 
                                PRBool          *aCancel,
                                PRBool          *aHandled,
                                const nsAString *inString,
                                nsAString       *outString,
                                PRInt32          aMaxLength)
{  
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }



  if (inString->IsEmpty() && (aAction != kInsertTextIME))
  {
    // HACK: this is a fix for bug 19395
    // I can't outlaw all empty insertions
    // because IME transaction depend on them
    // There is more work to do to make the 
    // world safe for IME.
    *aCancel = PR_TRUE;
    *aHandled = PR_FALSE;
    return NS_OK;
  }
  
  // initialize out param
  *aCancel = PR_FALSE;
  *aHandled = PR_TRUE;
  nsresult res;
  nsCOMPtr<nsIDOMNode> selNode;
  PRInt32 selOffset;

  PRBool bPlaintext = mFlags & nsIPlaintextEditor::eEditorPlaintextMask;

  // if the selection isn't collapsed, delete it.
  PRBool bCollapsed;
  res = aSelection->GetIsCollapsed(&bCollapsed);
  if (NS_FAILED(res)) return res;
  if (!bCollapsed)
  {
    res = mHTMLEditor->DeleteSelection(nsIEditor::eNone);
    if (NS_FAILED(res)) return res;
  }

  res = WillInsert(aSelection, aCancel);
  if (NS_FAILED(res)) return res;
  // initialize out param
  // we want to ignore result of WillInsert()
  *aCancel = PR_FALSE;
  
  // get the (collapsed) selection location
  res = mHTMLEditor->GetStartNodeAndOffset(aSelection, address_of(selNode), &selOffset);
  if (NS_FAILED(res)) return res;

  // dont put text in places that cant have it
  if (!mHTMLEditor->IsTextNode(selNode) && !mHTMLEditor->CanContainTag(selNode, NS_LITERAL_STRING("__moz_text")))
    return NS_ERROR_FAILURE;

  // we need to get the doc
  nsCOMPtr<nsIDOMDocument>doc;
  res = mHTMLEditor->GetDocument(getter_AddRefs(doc));
  if (NS_FAILED(res)) return res;
  if (!doc) return NS_ERROR_NULL_POINTER;
    
  if (aAction == kInsertTextIME) 
  { 
    // Right now the nsWSRunObject code bails on empty strings, but IME needs 
    // the InsertTextImpl() call to still happen since empty strings are meaningful there.
    if (inString->IsEmpty())
    {
      res = mHTMLEditor->InsertTextImpl(*inString, address_of(selNode), &selOffset, doc);
    }
    else
    {
      nsWSRunObject wsObj(mHTMLEditor, selNode, selOffset);
      res = wsObj.InsertText(*inString, address_of(selNode), &selOffset, doc);
    }
    if (NS_FAILED(res)) return res;
  }
  else // aAction == kInsertText
  {
    // find where we are
    nsCOMPtr<nsIDOMNode> curNode = selNode;
    PRInt32 curOffset = selOffset;
    
    // is our text going to be PREformatted?  
    // We remember this so that we know how to handle tabs.
    PRBool isPRE;
    res = mHTMLEditor->IsPreformatted(selNode, &isPRE);
    if (NS_FAILED(res)) return res;    
    
    // turn off the edit listener: we know how to
    // build the "doc changed range" ourselves, and it's
    // must faster to do it once here than to track all
    // the changes one at a time.
    nsAutoLockListener lockit(&mListenerEnabled); 
    
    // dont spaz my selection in subtransactions
    nsAutoTxnsConserveSelection dontSpazMySelection(mHTMLEditor);
    nsAutoString tString(*inString);
    const PRUnichar *unicodeBuf = tString.get();
    nsCOMPtr<nsIDOMNode> unused;
    PRInt32 pos = 0;
    NS_NAMED_LITERAL_STRING(newlineStr, "\n");
        
    // for efficiency, break out the pre case seperately.  This is because
    // its a lot cheaper to search the input string for only newlines than
    // it is to search for both tabs and newlines.
    if (isPRE || bPlaintext)
    {
      char newlineChar = '\n';
      while (unicodeBuf && (pos != -1) && (pos < (PRInt32)(*inString).Length()))
      {
        PRInt32 oldPos = pos;
        PRInt32 subStrLen;
        pos = tString.FindChar(newlineChar, oldPos);

        if (pos != -1) 
        {
          subStrLen = pos - oldPos;
          // if first char is newline, then use just it
          if (subStrLen == 0)
            subStrLen = 1;
        }
        else
        {
          subStrLen = tString.Length() - oldPos;
          pos = tString.Length();
        }

        nsDependentSubstring subStr(tString, oldPos, subStrLen);
        
        // is it a return?
        if (subStr.Equals(newlineStr))
        {
          res = mHTMLEditor->CreateBRImpl(address_of(curNode), &curOffset, address_of(unused), nsIEditor::eNone);
          pos++;
        }
        else
        {
          res = mHTMLEditor->InsertTextImpl(subStr, address_of(curNode), &curOffset, doc);
        }
        if (NS_FAILED(res)) return res;
      }
    }
    else
    {
      NS_NAMED_LITERAL_STRING(tabStr, "\t");
      NS_NAMED_LITERAL_STRING(spacesStr, "    ");
      char specialChars[] = {'\t','\n',0};
      while (unicodeBuf && (pos != -1) && (pos < (PRInt32)inString->Length()))
      {
        PRInt32 oldPos = pos;
        PRInt32 subStrLen;
        pos = tString.FindCharInSet(specialChars, oldPos);
        
        if (pos != -1) 
        {
          subStrLen = pos - oldPos;
          // if first char is newline, then use just it
          if (subStrLen == 0)
            subStrLen = 1;
        }
        else
        {
          subStrLen = tString.Length() - oldPos;
          pos = tString.Length();
        }

        nsDependentSubstring subStr(tString, oldPos, subStrLen);
        nsWSRunObject wsObj(mHTMLEditor, curNode, curOffset);

        // is it a tab?
        if (subStr.Equals(tabStr))
        {
          res = wsObj.InsertText(spacesStr, address_of(curNode), &curOffset, doc);
          if (NS_FAILED(res)) return res;
          pos++;
        }
        // is it a return?
        else if (subStr.Equals(newlineStr))
        {
          res = wsObj.InsertBreak(address_of(curNode), &curOffset, address_of(unused), nsIEditor::eNone);
          if (NS_FAILED(res)) return res;
          pos++;
        }
        else
        {
          res = wsObj.InsertText(subStr, address_of(curNode), &curOffset, doc);
          if (NS_FAILED(res)) return res;
        }
        if (NS_FAILED(res)) return res;
      }
    }
#ifdef IBMBIDI
    nsCOMPtr<nsISelection> selection(aSelection);
    nsCOMPtr<nsISelectionPrivate> selPriv(do_QueryInterface(selection));
    selPriv->SetInterlinePosition(PR_FALSE);
#endif
    if (curNode) aSelection->Collapse(curNode, curOffset);
    // manually update the doc changed range so that AfterEdit will clean up
    // the correct portion of the document.
    if (!mDocChangeRange)
    {
      mDocChangeRange = do_CreateInstance(kRangeCID);
      if (!mDocChangeRange) return NS_ERROR_NULL_POINTER;
    }
    res = mDocChangeRange->SetStart(selNode, selOffset);
    if (NS_FAILED(res)) return res;
    if (curNode)
      res = mDocChangeRange->SetEnd(curNode, curOffset);
    else
      res = mDocChangeRange->SetEnd(selNode, selOffset);
    if (NS_FAILED(res)) return res;
  }
  return res;
}

nsresult
nsHTMLEditRules::WillLoadHTML(nsISelection *aSelection, PRBool *aCancel)
{
  if (!aSelection || !aCancel) return NS_ERROR_NULL_POINTER;

  *aCancel = PR_FALSE;

  // Delete mBogusNode if it exists. If we really need one,
  // it will be added during post-processing in AfterEditInner().

  if (mBogusNode)
  {
    mEditor->DeleteNode(mBogusNode);
    mBogusNode = nsnull;
  }

  return NS_OK;
}

nsresult
nsHTMLEditRules::WillInsertBreak(nsISelection *aSelection, PRBool *aCancel, PRBool *aHandled)
{
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }
  nsCOMPtr<nsISelection> selection(aSelection);
  nsCOMPtr<nsISelectionPrivate> selPriv(do_QueryInterface(selection));
  // initialize out param
  *aCancel = PR_FALSE;
  *aHandled = PR_FALSE;
  
  PRBool bPlaintext = mFlags & nsIPlaintextEditor::eEditorPlaintextMask;

  // if the selection isn't collapsed, delete it.
  PRBool bCollapsed;
  nsresult res = aSelection->GetIsCollapsed(&bCollapsed);
  if (NS_FAILED(res)) return res;
  if (!bCollapsed)
  {
    res = mHTMLEditor->DeleteSelection(nsIEditor::eNone);
    if (NS_FAILED(res)) return res;
  }
  
  res = WillInsert(aSelection, aCancel);
  if (NS_FAILED(res)) return res;
  
  // initialize out param
  // we want to ignore result of WillInsert()
  *aCancel = PR_FALSE;
  
  // split any mailcites in the way.
  // should we abort this if we encounter table cell boundaries?
  if (mFlags & nsIPlaintextEditor::eEditorMailMask)
  {
    nsCOMPtr<nsIDOMNode> citeNode, selNode, leftCite, rightCite;
    PRInt32 selOffset, newOffset;
    res = mHTMLEditor->GetStartNodeAndOffset(aSelection, address_of(selNode), &selOffset);
    if (NS_FAILED(res)) return res;
    res = GetTopEnclosingMailCite(selNode, address_of(citeNode), bPlaintext);
    if (NS_FAILED(res)) return res;
    if (citeNode)
    {
      nsCOMPtr<nsIDOMNode> brNode;
      res = mHTMLEditor->SplitNodeDeep(citeNode, selNode, selOffset, &newOffset, 
                         PR_TRUE, address_of(leftCite), address_of(rightCite));
      if (NS_FAILED(res)) return res;
      res = citeNode->GetParentNode(getter_AddRefs(selNode));
      if (NS_FAILED(res)) return res;
      res = mHTMLEditor->CreateBR(selNode, newOffset, address_of(brNode));
      if (NS_FAILED(res)) return res;
      // want selection before the break, and on same line
      selPriv->SetInterlinePosition(PR_TRUE);
      res = aSelection->Collapse(selNode, newOffset);
      if (NS_FAILED(res)) return res;
      // if citeNode wasn't a block, we also want another break before it
      if (IsInlineNode(citeNode))
      {
        res = mHTMLEditor->CreateBR(selNode, newOffset, address_of(brNode));
        if (NS_FAILED(res)) return res;
      }
      // delete any empty cites
      PRBool bEmptyCite = PR_FALSE;
      if (leftCite)
      {
        res = mHTMLEditor->IsEmptyNode(leftCite, &bEmptyCite, PR_TRUE, PR_FALSE);
        if (NS_SUCCEEDED(res) && bEmptyCite)
          res = mHTMLEditor->DeleteNode(leftCite);
        if (NS_FAILED(res)) return res;
      }
      if (rightCite)
      {
        res = mHTMLEditor->IsEmptyNode(rightCite, &bEmptyCite, PR_TRUE, PR_FALSE);
        if (NS_SUCCEEDED(res) && bEmptyCite)
          res = mHTMLEditor->DeleteNode(rightCite);
        if (NS_FAILED(res)) return res;
      }
      *aHandled = PR_TRUE;
      return NS_OK;
    }
  }

  // smart splitting rules
  nsCOMPtr<nsIDOMNode> node;
  PRInt32 offset;
  
  res = mHTMLEditor->GetStartNodeAndOffset(aSelection, address_of(node), &offset);
  if (NS_FAILED(res)) return res;
  if (!node) return NS_ERROR_FAILURE;
    
  // identify the block
  nsCOMPtr<nsIDOMNode> blockParent;
  
  if (IsBlockNode(node)) 
    blockParent = node;
  else 
    blockParent = mHTMLEditor->GetBlockNodeParent(node);
    
  if (!blockParent) return NS_ERROR_FAILURE;
  
  nsCOMPtr<nsIDOMNode> listItem = IsInListItem(blockParent);
  if (listItem)
  {
    res = ReturnInListItem(aSelection, listItem, node, offset);
    *aHandled = PR_TRUE;
    return NS_OK;
  }
  
  // headers: close (or split) header
  else if (nsHTMLEditUtils::IsHeader(blockParent))
  {
    res = ReturnInHeader(aSelection, blockParent, node, offset);
    *aHandled = PR_TRUE;
    return NS_OK;
  }
  
  // paragraphs: special rules to look for <br>s
  else if (nsHTMLEditUtils::IsParagraph(blockParent))
  {
    res = ReturnInParagraph(aSelection, blockParent, node, offset, aCancel, aHandled);
    return NS_OK;
  }
  
  // its something else (body, div, td, ...): insert a normal br
  else
  {
    nsCOMPtr<nsIDOMNode> brNode;
    if (bPlaintext)
    {
      res = mHTMLEditor->CreateBR(node, offset, address_of(brNode));
    }
    else
    {
      nsWSRunObject wsObj(mHTMLEditor, node, offset);
      res = wsObj.InsertBreak(address_of(node), &offset, address_of(brNode), nsIEditor::eNone);
    }
    if (NS_FAILED(res)) return res;
    res = nsEditor::GetNodeLocation(brNode, address_of(node), &offset);
    if (NS_FAILED(res)) return res;
    // SetInterlinePosition(PR_TRUE) means we want the caret to stick to the content on the "right".
    // We want the caret to stick to whatever is past the break.  This is
    // because the break is on the same line we were on, but the next content
    // will be on the following line.
    
    // An exception to this is if the break has a next sibling that is a block node.
    // Then we stick to the left to aviod an uber caret.
    nsCOMPtr<nsIDOMNode> siblingNode;
    brNode->GetNextSibling(getter_AddRefs(siblingNode));
    if (siblingNode && IsBlockNode(siblingNode))
      selPriv->SetInterlinePosition(PR_FALSE);
    else 
      selPriv->SetInterlinePosition(PR_TRUE);
    res = aSelection->Collapse(node, offset+1);
    if (NS_FAILED(res)) return res;
    *aHandled = PR_TRUE;
  }
  
  return res;
}


nsresult
nsHTMLEditRules::DidInsertBreak(nsISelection *aSelection, nsresult aResult)
{
  return NS_OK;
}


nsresult
nsHTMLEditRules::WillDeleteSelection(nsISelection *aSelection, 
                                     nsIEditor::EDirection aAction, 
                                     PRBool *aCancel,
                                     PRBool *aHandled)
{
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }
  // initialize out param
  *aCancel = PR_FALSE;
  *aHandled = PR_FALSE;
  
  // if there is only bogus content, cancel the operation
  if (mBogusNode) 
  {
    *aCancel = PR_TRUE;
    return NS_OK;
  }

  nsresult res = NS_OK;
  PRBool bPlaintext = mFlags & nsIPlaintextEditor::eEditorPlaintextMask;
  
  PRBool bCollapsed;
  res = aSelection->GetIsCollapsed(&bCollapsed);
  if (NS_FAILED(res)) return res;
  
  nsCOMPtr<nsIDOMNode> startNode, selNode;
  PRInt32 startOffset, selOffset;
  
  // first check for table selection mode.  If so,
  // hand off to table editor.
  {
    nsCOMPtr<nsIDOMElement> cell;
    res = mHTMLEditor->GetFirstSelectedCell(getter_AddRefs(cell), nsnull);
    if (NS_SUCCEEDED(res) && cell)
    {
      res = mHTMLEditor->DeleteTableCellContents();
      *aHandled = PR_TRUE;
      return res;
    }
  }
  
  res = mHTMLEditor->GetStartNodeAndOffset(aSelection, address_of(startNode), &startOffset);
  if (NS_FAILED(res)) return res;
  if (!startNode) return NS_ERROR_FAILURE;
    
  // get the root element  
  nsCOMPtr<nsIDOMNode> bodyNode; 
  {
    nsCOMPtr<nsIDOMElement> bodyElement;   
    res = mHTMLEditor->GetRootElement(getter_AddRefs(bodyElement));
    if (NS_FAILED(res)) return res;
    if (!bodyElement) return NS_ERROR_UNEXPECTED;
    bodyNode = do_QueryInterface(bodyElement);
  }

  if (bCollapsed)
  {
    // if we are inside an empty block, delete it.
    res = CheckForEmptyBlock(startNode, bodyNode, aSelection, aHandled);
    if (NS_FAILED(res)) return res;
    if (*aHandled) return NS_OK;
        
#ifdef IBMBIDI
    // Test for distance between caret and text that will be deleted
    res = CheckBidiLevelForDeletion(startNode, startOffset, aAction, aCancel);
    if (NS_FAILED(res)) return res;
    if (*aCancel) return NS_OK;
#endif // IBMBIDI

    if (!bPlaintext)
    {
      // Check for needed whitespace adjustments.  If we need to delete
      // just whitespace, that is handled here.  
      // CheckForWhitespaceDeletion also adjusts it's first two args to
      // skip over invisible ws.  So we need to check that we didn't cross a table
      // element boundary afterwards.
      nsCOMPtr<nsIDOMNode> visNode = startNode;
      PRInt32 visOffset = startOffset;
      res = CheckForWhitespaceDeletion(address_of(visNode), &visOffset, 
                                       aAction, aHandled);
      if (NS_FAILED(res)) return res;
      if (*aHandled) return NS_OK;
      // dont cross any table elements
      PRBool bInDifTblElems;
      res = InDifferentTableElements(visNode, startNode, &bInDifTblElems);
      if (NS_FAILED(res)) return res;
      if (bInDifTblElems) 
      {
        *aCancel = PR_TRUE;
        return NS_OK;
      }
      // else reset startNode/startOffset
      startNode = visNode;  startOffset = visOffset;
    }
    // in a text node:
    if (mHTMLEditor->IsTextNode(startNode))
    {
      nsCOMPtr<nsIDOMText> textNode = do_QueryInterface(startNode);
      PRUint32 strLength;
      res = textNode->GetLength(&strLength);
      if (NS_FAILED(res)) return res;

      // at beginning of text node and backspaced?
      if (!startOffset && (aAction == nsIEditor::ePrevious))
      {
        nsCOMPtr<nsIDOMNode> priorNode;
        res = mHTMLEditor->GetPriorHTMLNode(startNode, address_of(priorNode));
        if (NS_FAILED(res)) return res;
        
        // if there is no prior node then cancel the deletion
        if (!priorNode || !nsTextEditUtils::InBody(priorNode, mHTMLEditor))
        {
          *aCancel = PR_TRUE;
          return res;
        }
        
        // block parents the same?  

        if (mHTMLEditor->HasSameBlockNodeParent(startNode, priorNode)) 
        {
          // is prior node a text node?
          if ( mHTMLEditor->IsTextNode(priorNode) )
          {
            // delete last character
            PRUint32 offset;
            nsCOMPtr<nsIDOMCharacterData>nodeAsText;
            nodeAsText = do_QueryInterface(priorNode);
            nodeAsText->GetLength((PRUint32*)&offset);
            NS_ENSURE_TRUE(offset, NS_ERROR_FAILURE);
            res = aSelection->Collapse(priorNode,offset);
            if (NS_FAILED(res)) return res;
            if (!bPlaintext)
            {
              PRInt32 so = offset-1;
              PRInt32 eo = offset;
              res = nsWSRunObject::PrepareToDeleteRange(mHTMLEditor, 
                                                      address_of(priorNode), &so, 
                                                      address_of(priorNode), &eo);
            }
            // just return without setting handled to true.
            // default code will take care of actual deletion
            return res;
          }
          // is prior node not a container?  (ie, a br, hr, image...)
          else if (!mHTMLEditor->IsContainer(priorNode))   // MOOSE: anchors not handled
          {
            if (!bPlaintext)
            {
              res = nsWSRunObject::PrepareToDeleteNode(mHTMLEditor, priorNode);
              if (NS_FAILED(res)) return res;
            }
            // remember prior sibling to prior node, if any
            nsCOMPtr<nsIDOMNode> sibling, stepbrother;  
            mHTMLEditor->GetPriorHTMLSibling(priorNode, address_of(sibling));
            // delete the break, and join like nodes if appropriate
            res = mHTMLEditor->DeleteNode(priorNode);
            if (NS_FAILED(res)) return res;
            // we did something, so lets say so.
            *aHandled = PR_TRUE;
            // is there a prior node and are they siblings?
            if (sibling)
              sibling->GetNextSibling(getter_AddRefs(stepbrother));
            if (startNode == stepbrother) 
            {
              // are they both text nodes?
              if (mHTMLEditor->IsTextNode(startNode) && mHTMLEditor->IsTextNode(sibling))
              {
                // if so, join them!
                res = JoinNodesSmart(sibling, startNode, address_of(selNode), &selOffset);
                if (NS_FAILED(res)) return res;
                // fix up selection
                res = aSelection->Collapse(selNode, selOffset);
              }
            }
            return res;
          }
          else if ( IsInlineNode(priorNode) )
          {
            if (!bPlaintext)
            {
              res = nsWSRunObject::PrepareToDeleteNode(mHTMLEditor, priorNode);
              if (NS_FAILED(res)) return res;
            }
            // remember where we are
            PRInt32 offset;
            nsCOMPtr<nsIDOMNode> node;
            res = mHTMLEditor->GetNodeLocation(priorNode, address_of(node), &offset);
            if (NS_FAILED(res)) return res;
            // delete it
            res = mHTMLEditor->DeleteNode(priorNode);
            if (NS_FAILED(res)) return res;
            // we did something, so lets say so.
            *aHandled = PR_TRUE;
            // fix up selection
            res = aSelection->Collapse(node,offset);
            return res;
          }
          else return NS_OK; // punt to default
        }
        
        // ---- deleting across blocks ---------------------------------------------
        
        // dont cross any table elements
        PRBool bInDifTblElems;
        res = InDifferentTableElements(startNode, priorNode, &bInDifTblElems);
        if (NS_FAILED(res)) return res;
        if (bInDifTblElems) 
        {
          *aCancel = PR_TRUE;
          return NS_OK;
        }

        nsCOMPtr<nsIDOMNode> leftParent;
        nsCOMPtr<nsIDOMNode> rightParent;
        if (IsBlockNode(priorNode))
          leftParent = priorNode;
        else
          leftParent = mHTMLEditor->GetBlockNodeParent(priorNode);
        if (IsBlockNode(startNode))
          rightParent = startNode;
        else
          rightParent = mHTMLEditor->GetBlockNodeParent(startNode);
        
        if (!leftParent || !rightParent)
          return NS_ERROR_NULL_POINTER;  // should not happen
        
        nsCOMPtr<nsIDOMNode> selPointNode = startNode;
        PRInt32 selPointOffset = startOffset;
        {
          nsAutoTrackDOMPoint tracker(mHTMLEditor->mRangeUpdater, address_of(selPointNode), &selPointOffset);
          res = JoinBlocks(aSelection, address_of(leftParent), address_of(rightParent), aCancel);
          *aHandled = PR_TRUE;
        }
        aSelection->Collapse(selPointNode, selPointOffset);
        return res;
      }
    
      // at end of text node and deleted?
      if ((startOffset == (PRInt32)strLength)
          && (aAction == nsIEditor::eNext))
      {
        nsCOMPtr<nsIDOMNode> nextNode;
        res = mHTMLEditor->GetNextHTMLNode(startNode, address_of(nextNode));
        if (NS_FAILED(res)) return res;
         
        // if there is no next node, or it's not in the body, then cancel the deletion
        if (!nextNode || !nsTextEditUtils::InBody(nextNode, mHTMLEditor))
        {
          *aCancel = PR_TRUE;
          return res;
        }
       
        // block parents the same?  
        if (mHTMLEditor->HasSameBlockNodeParent(startNode, nextNode)) 
        {
          // is next node a text node?
          if ( mHTMLEditor->IsTextNode(nextNode) )
          {
            // delete first character
            nsCOMPtr<nsIDOMCharacterData>nodeAsText;
            nodeAsText = do_QueryInterface(nextNode);
            res = aSelection->Collapse(nextNode,0);
            if (NS_FAILED(res)) return res;
            if (!bPlaintext)
            {
              PRInt32 so = 0;
              PRInt32 eo = 1;
              res = nsWSRunObject::PrepareToDeleteRange(mHTMLEditor, 
                                                      address_of(nextNode), &so, 
                                                      address_of(nextNode), &eo);
            }
            // just return without setting handled to true.
            // default code will take care of actual deletion
            return res;
          }
          // is next node not a container?  (ie, a br, hr, image...)
          else if (!mHTMLEditor->IsContainer(nextNode))  // MOOSE: anchors not handled
          {
            if (!bPlaintext)
            {
              res = nsWSRunObject::PrepareToDeleteNode(mHTMLEditor, nextNode);
              if (NS_FAILED(res)) return res;
            }
            // remember next sibling after nextNode, if any
            nsCOMPtr<nsIDOMNode> sibling, stepbrother;  
            mHTMLEditor->GetNextHTMLSibling(nextNode, address_of(sibling));
            // delete the break, and join like nodes if appropriate
            res = mHTMLEditor->DeleteNode(nextNode);
            if (NS_FAILED(res)) return res;
            // we did something, so lets say so.
            *aHandled = PR_TRUE;
            // is there a prior node and are they siblings?
            if (sibling)
              sibling->GetPreviousSibling(getter_AddRefs(stepbrother));
            if (startNode == stepbrother) 
            {
              // are they both text nodes?
              if (mHTMLEditor->IsTextNode(startNode) && mHTMLEditor->IsTextNode(sibling))
              {
                // if so, join them!
                res = JoinNodesSmart(startNode, sibling, address_of(selNode), &selOffset);
                if (NS_FAILED(res)) return res;
                // fix up selection
                res = aSelection->Collapse(selNode, selOffset);
              }
            }
            return res;
          }
          else if ( IsInlineNode(nextNode) )
          {
            if (!bPlaintext)
            {
              res = nsWSRunObject::PrepareToDeleteNode(mHTMLEditor, nextNode);
              if (NS_FAILED(res)) return res;
            }
            // remember where we are
            PRInt32 offset;
            nsCOMPtr<nsIDOMNode> node;
            res = mHTMLEditor->GetNodeLocation(nextNode, address_of(node), &offset);
            if (NS_FAILED(res)) return res;
            // delete it
            res = mHTMLEditor->DeleteNode(nextNode);
            if (NS_FAILED(res)) return res;
            // we did something, so lets say so.
            *aHandled = PR_TRUE;
            // fix up selection
            res = aSelection->Collapse(node,offset);
            return res;
          }
          else return NS_OK; // punt to default
        }
                
        // ---- deleting across blocks ---------------------------------------------
        
        // dont cross any table elements
        PRBool bInDifTblElems;
        res = InDifferentTableElements(startNode, nextNode, &bInDifTblElems);
        if (NS_FAILED(res)) return res;
        if (bInDifTblElems) 
        {
          *aCancel = PR_TRUE;
          return NS_OK;
        }

        nsCOMPtr<nsIDOMNode> leftParent;
        nsCOMPtr<nsIDOMNode> rightParent;
        if (IsBlockNode(startNode))
          leftParent = startNode;
        else
          leftParent = mHTMLEditor->GetBlockNodeParent(startNode);
        if (IsBlockNode(nextNode) || nsEditor::IsTextNode(nextNode))
        {
          rightParent = nextNode;
          aSelection->Collapse(nextNode,0);
          startNode = nextNode; startOffset = 0;
        }
        else
        {
          rightParent = mHTMLEditor->GetBlockNodeParent(nextNode);
          nsEditor::GetNodeLocation(nextNode, address_of(startNode), &startOffset);
          aSelection->Collapse(startNode, startOffset);
        }
        if (!leftParent || !rightParent)
          return NS_ERROR_NULL_POINTER;  // should not happen
        
        nsCOMPtr<nsIDOMNode> selPointNode = startNode;
        PRInt32 selPointOffset = startOffset;
        {
          nsAutoTrackDOMPoint tracker(mHTMLEditor->mRangeUpdater, address_of(selPointNode), &selPointOffset);
          res = JoinBlocks(aSelection, address_of(leftParent), address_of(rightParent), aCancel);
          *aHandled = PR_TRUE;
        }
        aSelection->Collapse(selPointNode, selPointOffset);
        return res;
      }
      
      // else in middle of text node.  default will do right thing.
      nsCOMPtr<nsIDOMCharacterData>nodeAsText;
      nodeAsText = do_QueryInterface(startNode);
      if (!nodeAsText) return NS_ERROR_NULL_POINTER;
      res = aSelection->Collapse(startNode,startOffset);
      if (NS_FAILED(res)) return res;
      if (!bPlaintext)
      {
        PRInt32 so = startOffset;
        PRInt32 eo = startOffset;
        if (aAction == nsIEditor::ePrevious)
          so--;  // we know so not zero - that case handled above
        else
          eo++;  // we know eo not at end of text node - that case handled above
        res = nsWSRunObject::PrepareToDeleteRange(mHTMLEditor, 
                                                address_of(startNode), &so, 
                                                address_of(startNode), &eo);
      }
      // just return without setting handled to true.
      // default code will take care of actual deletion
      return res;
    }
    // else not in text node; we need to find right place to act on
    else
    {
      nsCOMPtr<nsIDOMNode> nodeToDelete;
      
      // first note that the right node to delete might be the one we
      // are in.  For example, if a list item is deleted one character at a time,
      // eventually it will be empty (except for a moz-br).  If the user hits 
      // backspace again, they expect the item itself to go away.  Check to
      // see if we are in an "empty" node.
      // Note: do NOT delete table elements this way.
      PRBool bIsEmptyNode;
      res = mHTMLEditor->IsEmptyNode(startNode, &bIsEmptyNode, PR_TRUE, PR_FALSE);
      if (bIsEmptyNode && !nsHTMLEditUtils::IsTableElement(startNode))
        nodeToDelete = startNode;
      else 
      {
        // see if we are on empty line and need to delete it.  This is true
        // when a break is after a block and we are deleting backwards; or
        // when a break is before a block and we are delting forwards.  In
        // these cases, we want to delete the break when we are between it
        // and the block element, even though the break is on the "wrong"
        // side of us.
        nsCOMPtr<nsIDOMNode> maybeBreak;
        nsCOMPtr<nsIDOMNode> maybeBlock;
        
        if (aAction == nsIEditor::ePrevious)
        {
          res = mHTMLEditor->GetPriorHTMLSibling(startNode, startOffset, address_of(maybeBlock));
          if (NS_FAILED(res)) return res;
          res = mHTMLEditor->GetNextHTMLSibling(startNode, startOffset, address_of(maybeBreak));
          if (NS_FAILED(res)) return res;
        }
        else if (aAction == nsIEditor::eNext)
        {
          res = mHTMLEditor->GetPriorHTMLSibling(startNode, startOffset, address_of(maybeBreak));
          if (NS_FAILED(res)) return res;
          res = mHTMLEditor->GetNextHTMLSibling(startNode, startOffset, address_of(maybeBlock));
          if (NS_FAILED(res)) return res;
        }
        
        if (maybeBreak && maybeBlock &&
            nsTextEditUtils::IsBreak(maybeBreak) && IsBlockNode(maybeBlock))
          nodeToDelete = maybeBreak;
        else if (aAction == nsIEditor::ePrevious)
          res = mHTMLEditor->GetPriorHTMLNode(startNode, startOffset, address_of(nodeToDelete));
        else if (aAction == nsIEditor::eNext)
          res = mHTMLEditor->GetNextHTMLNode(startNode, startOffset, address_of(nodeToDelete));
        else
          return NS_OK;
      } 
      
      // don't delete the root element!
      if (NS_FAILED(res)) return res;
      if (!nodeToDelete) return NS_ERROR_NULL_POINTER;
      if (mBody == nodeToDelete) 
      {
        *aCancel = PR_TRUE;
        return res;
      }
      
      // dont cross any table elements
      PRBool bInDifTblElems;
      res = InDifferentTableElements(startNode, nodeToDelete, &bInDifTblElems);
      if (NS_FAILED(res)) return res;
      if (bInDifTblElems) 
      {
        *aCancel = PR_TRUE;
        return NS_OK;
      }
      
      // if this node is text node, adjust selection
      if (nsEditor::IsTextNode(nodeToDelete))
      {
        PRUint32 selPoint = 0;
        nsCOMPtr<nsIDOMCharacterData>nodeAsText;
        nodeAsText = do_QueryInterface(nodeToDelete);
        if (aAction == nsIEditor::ePrevious)
          nodeAsText->GetLength(&selPoint);
        res = aSelection->Collapse(nodeToDelete,selPoint);
        return res;
      }
      else
      {
        // editable leaf node is not text; delete it.
        // that's the default behavior
        // EXCEPTION: if it's a mozBR, we have to check and see if
        // there is a br in front of it. If so, we must delete both.
        // else you get this: deletion code deletes mozBR, then selection
        // adjusting code puts it back in.  doh
        if (nsTextEditUtils::IsMozBR(nodeToDelete))
        {
          nsCOMPtr<nsIDOMNode> brNode;
          res = mHTMLEditor->GetPriorHTMLNode(nodeToDelete, address_of(brNode));
          if (brNode && nsTextEditUtils::IsBreak(brNode))
          {
            // is brNode also a descendant of same block?
            nsCOMPtr<nsIDOMNode> block, brBlock;
            block = mHTMLEditor->GetBlockNodeParent(nodeToDelete);
            brBlock = mHTMLEditor->GetBlockNodeParent(brNode);
            if (block == brBlock)
            {
              // delete both breaks
              if (!bPlaintext)
              {
                res = nsWSRunObject::PrepareToDeleteNode(mHTMLEditor, brNode);
                if (NS_FAILED(res)) return res;
              }
              res = mHTMLEditor->DeleteNode(brNode);
              if (NS_FAILED(res)) return res;
              // fall through to delete other br
            }
            // else fall through
          }
          // else fall through
        }
        if (!bPlaintext)
        {
          res = nsWSRunObject::PrepareToDeleteNode(mHTMLEditor, nodeToDelete);
          if (NS_FAILED(res)) return res;
        }
        PRInt32 offset;
        nsCOMPtr<nsIDOMNode> node;
        res = nsEditor::GetNodeLocation(nodeToDelete, address_of(node), &offset);
        if (NS_FAILED(res)) return res;
        // adjust selection to be right after it
        res = aSelection->Collapse(node, offset+1);
        if (NS_FAILED(res)) return res;
        res = mHTMLEditor->DeleteNode(nodeToDelete);
        *aHandled = PR_TRUE;
        return res;
      }
    }
    
    return NS_OK;
  }
  
  // else we have a non collapsed selection
  // figure out if the enpoints are in nodes that can be merged
  nsCOMPtr<nsIDOMNode> endNode;
  PRInt32 endOffset;
  res = mHTMLEditor->GetEndNodeAndOffset(aSelection, address_of(endNode), &endOffset);
  if (NS_FAILED(res)) return res; 
  
  // adjust surrounding whitespace in preperation to delete selection
  if (!bPlaintext)
  {
    nsAutoTxnsConserveSelection dontSpazMySelection(mHTMLEditor);
    res = nsWSRunObject::PrepareToDeleteRange(mHTMLEditor,
                                            address_of(startNode), &startOffset, 
                                            address_of(endNode), &endOffset);
    if (NS_FAILED(res)) return res; 
  }
  if (endNode.get() != startNode.get())
  {
    {
      // track end location of where we are deleting
      nsAutoTrackDOMPoint tracker(mHTMLEditor->mRangeUpdater, address_of(endNode), &endOffset);
    
      // block parents the same?  use default deletion
      nsCOMPtr<nsIDOMNode> leftParent;
      nsCOMPtr<nsIDOMNode> rightParent;
      if (IsBlockNode(startNode))
        leftParent = startNode;
      else
        leftParent = mHTMLEditor->GetBlockNodeParent(startNode);
      if (IsBlockNode(endNode))
        rightParent = endNode;
      else
        rightParent = mHTMLEditor->GetBlockNodeParent(endNode);
      if (leftParent.get() == rightParent.get()) return NS_OK;
      
      // deleting across blocks
      // are the blocks of same type?
      
      // are the blocks siblings?
      nsCOMPtr<nsIDOMNode> leftBlockParent;
      nsCOMPtr<nsIDOMNode> rightBlockParent;
      leftParent->GetParentNode(getter_AddRefs(leftBlockParent));
      rightParent->GetParentNode(getter_AddRefs(rightBlockParent));

      // MOOSE: this could conceivably screw up a table.. fix me.
      if (   (leftBlockParent.get() == rightBlockParent.get())
          && (mHTMLEditor->NodesSameType(leftParent, rightParent))  )
      {
        if (nsHTMLEditUtils::IsParagraph(leftParent))
        {
          // first delete the selection
          *aHandled = PR_TRUE;
          res = mHTMLEditor->DeleteSelectionImpl(aAction);
          if (NS_FAILED(res)) return res;
          // then join para's, insert break
          res = mHTMLEditor->JoinNodeDeep(leftParent,rightParent,address_of(selNode),&selOffset);
          if (NS_FAILED(res)) return res;
          // fix up selection
          res = aSelection->Collapse(selNode,selOffset);
          return res;
        }
        if (nsHTMLEditUtils::IsListItem(leftParent)
            || nsHTMLEditUtils::IsHeader(leftParent))
        {
          // first delete the selection
          *aHandled = PR_TRUE;
          res = mHTMLEditor->DeleteSelectionImpl(aAction);
          if (NS_FAILED(res)) return res;
          // join blocks
          res = mHTMLEditor->JoinNodeDeep(leftParent,rightParent,address_of(selNode),&selOffset);
          if (NS_FAILED(res)) return res;
          // fix up selection
          res = aSelection->Collapse(selNode,selOffset);
          return res;
        }
      }
      
      // else blocks not same type, or not siblings.  Delete everything except
      // table elements.
      *aHandled = PR_TRUE;
      nsCOMPtr<nsIEnumerator> enumerator;
      nsCOMPtr<nsISelectionPrivate> selPriv(do_QueryInterface(aSelection));
      res = selPriv->GetEnumerator(getter_AddRefs(enumerator));
      if (NS_FAILED(res)) return res;
      if (!enumerator) return NS_ERROR_UNEXPECTED;

      for (enumerator->First(); NS_OK!=enumerator->IsDone(); enumerator->Next())
      {
        nsCOMPtr<nsISupports> currentItem;
        res = enumerator->CurrentItem(getter_AddRefs(currentItem));
        if (NS_FAILED(res)) return res;
        if (!currentItem) return NS_ERROR_UNEXPECTED;

        // build a list of nodes in the range
        nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
        nsCOMPtr<nsISupportsArray> arrayOfNodes;
        nsTrivialFunctor functor;
        nsDOMSubtreeIterator iter;
        res = iter.Init(range);
        if (NS_FAILED(res)) return res;
        res = iter.MakeList(functor, address_of(arrayOfNodes));
        if (NS_FAILED(res)) return res;
    
        // now that we have the list, delete non table elements
        PRUint32 listCount;
        PRUint32 j;
        nsCOMPtr<nsIDOMNode> somenode;
        nsCOMPtr<nsISupports> isupports;

        arrayOfNodes->Count(&listCount);
        for (j = 0; j < listCount; j++)
        {
          isupports = dont_AddRef(arrayOfNodes->ElementAt(0));
          somenode = do_QueryInterface(isupports);
          res = DeleteNonTableElements(somenode);
          arrayOfNodes->RemoveElementAt(0);
        }
      }
      
      // check endopints for possible text deletion.
      // we can assume that if text node is found, we can
      // delete to end or to begining as appropriate,
      // since the case where both sel endpoints in same
      // text node was already handled (we wouldn't be here)
      if ( mHTMLEditor->IsTextNode(startNode) )
      {
        // delete to last character
        nsCOMPtr<nsIDOMCharacterData>nodeAsText;
        PRUint32 len;
        nodeAsText = do_QueryInterface(startNode);
        nodeAsText->GetLength(&len);
        if (len > (PRUint32)startOffset)
        {
          res = mHTMLEditor->DeleteText(nodeAsText,startOffset,len-startOffset);
          if (NS_FAILED(res)) return res;
        }
      }
      if ( mHTMLEditor->IsTextNode(endNode) )
      {
        // delete to first character
        nsCOMPtr<nsIDOMCharacterData>nodeAsText;
        nodeAsText = do_QueryInterface(endNode);
        if (endOffset)
        {
          res = mHTMLEditor->DeleteText(nodeAsText,0,endOffset);
          if (NS_FAILED(res)) return res;
        }
      }
    }
    if (aAction == nsIEditor::eNext)
    {
      res = aSelection->Collapse(endNode,endOffset);
    }
    else
    {
      res = aSelection->Collapse(startNode,startOffset);
    }
    return NS_OK;
  }

  return res;
}  

/*****************************************************************************************************
*    JoinBlocks: this method is used to join two block elements.  The right element is always joined
*    to the left element.  If the elements are the same type and not nested within each other, 
*    JoinNodesSmart is called (example, joining two list items together into one).  If the elements
*    are not the same type, or one is a descendant of the other, we instead destroy the right block
*    placing it's children into leftblock.  DTD containment rules are followed throughout.
*         nsISelection *aSelection                 the selection.  
*         nsCOMPtr<nsIDOMNode> *aLeftBlock         pointer to the left block
*         nsCOMPtr<nsIDOMNode> *aRightBlock        pointer to the right block; will have contents moved to left block
*         PRBool *aCanceled                        return TRUE if we had to cancel operation
*/
nsresult
nsHTMLEditRules::JoinBlocks(nsISelection *aSelection, nsCOMPtr<nsIDOMNode> *aLeftBlock, 
                            nsCOMPtr<nsIDOMNode> *aRightBlock, PRBool *aCanceled)
{
  if (!aLeftBlock || !aRightBlock || !*aLeftBlock || !*aRightBlock) return NS_ERROR_NULL_POINTER;
  if (nsHTMLEditUtils::IsTableElement(*aLeftBlock) || nsHTMLEditUtils::IsTableElement(*aRightBlock))
  {
    // do not try to merge table elements
    *aCanceled = PR_TRUE;
    return NS_OK;
  }

  // special rule here: if we are trying to join list items, and they are in different lists,
  // join the lists instead.
  PRBool bMergeLists = PR_FALSE;
  nsAutoString existingListStr;
  PRInt32 theOffset;
  nsCOMPtr<nsIDOMNode> leftList, rightList;
  if (nsHTMLEditUtils::IsListItem(*aLeftBlock) && nsHTMLEditUtils::IsListItem(*aRightBlock))
  {
    (*aLeftBlock)->GetParentNode(getter_AddRefs(leftList));
    (*aRightBlock)->GetParentNode(getter_AddRefs(rightList));
    if (leftList && rightList && (leftList!=rightList))
    {
      // there are some special complications if the lists are descendants of
      // the other lists' items.  Note that it is ok for them to be descendants
      // of the other lists themselves, which is the usual case for sublists
      // in our impllementation.
      if (!nsHTMLEditUtils::IsDescendantOf(leftList, *aRightBlock, &theOffset) &&
          !nsHTMLEditUtils::IsDescendantOf(rightList, *aLeftBlock, &theOffset))
      {
        *aLeftBlock = leftList;
        *aRightBlock = rightList;
        bMergeLists = PR_TRUE;
        mHTMLEditor->GetTagString(leftList, existingListStr);
        ToLowerCase(existingListStr);
      }
    }
  }
  
  nsAutoTxnsConserveSelection dontSpazMySelection(mHTMLEditor);
  
  nsresult res = NS_OK;

  // theOffset below is where you find yourself in aRightBlock when you traverse upwards
  // from aLeftBlock
  if (nsHTMLEditUtils::IsDescendantOf(*aLeftBlock, *aRightBlock, &theOffset))
  {
    // tricky case.  left block is inside right block.
    // Do ws adjustment.  This just destroys non-visible ws at boundaries we will be joining.
    theOffset++;
    res = nsWSRunObject::ScrubBlockBoundary(mHTMLEditor, aLeftBlock, nsWSRunObject::kBlockEnd);
    if (NS_FAILED(res)) return res;
    res = nsWSRunObject::ScrubBlockBoundary(mHTMLEditor, aRightBlock, nsWSRunObject::kAfterBlock, &theOffset);
    if (NS_FAILED(res)) return res;
    // Do br adjustment.
    nsCOMPtr<nsIDOMNode> brNode;
    res = CheckForInvisibleBR(*aLeftBlock, kBlockEnd, address_of(brNode));
    if (NS_FAILED(res)) return res;
    if (bMergeLists)
    {
      // idea here is to take all list items and sublists in rightList that are past
      // theOffset, and pull them into leftlist.
      nsCOMPtr<nsIDOMNode> childToMove;
      nsCOMPtr<nsIContent> parent(do_QueryInterface(rightList)), child;
      if (!parent) return NS_ERROR_NULL_POINTER;
      parent->ChildAt(theOffset, *getter_AddRefs(child));
      while (child)
      {
        childToMove = do_QueryInterface(child);
        res = mHTMLEditor->MoveNode(childToMove, leftList, -1);
        if (NS_FAILED(res)) return res;
        parent->ChildAt(theOffset, *getter_AddRefs(child));
      }
    }
    else
    {
      res = MoveBlock(aSelection, *aLeftBlock, -1);
    }
    if (brNode) mHTMLEditor->DeleteNode(brNode);
  }
  // theOffset below is where you find yourself in aLeftBlock when you traverse upwards
  // from aRightBlock
  else if (nsHTMLEditUtils::IsDescendantOf(*aRightBlock, *aLeftBlock, &theOffset))
  {
    // tricky case.  right block is inside left block.
    // Do ws adjustment.  This just destroys non-visible ws at boundaries we will be joining.
    res = nsWSRunObject::ScrubBlockBoundary(mHTMLEditor, aRightBlock, nsWSRunObject::kBlockStart);
    if (NS_FAILED(res)) return res;
    res = nsWSRunObject::ScrubBlockBoundary(mHTMLEditor, aLeftBlock, nsWSRunObject::kBeforeBlock, &theOffset);
    if (NS_FAILED(res)) return res;
    // Do br adjustment.
    nsCOMPtr<nsIDOMNode> brNode;
    res = CheckForInvisibleBR(*aLeftBlock, kBeforeBlock, address_of(brNode), theOffset);
    if (NS_FAILED(res)) return res;
    if (bMergeLists)
    {
      res = MoveContents(rightList, leftList, &theOffset);
    }
    else
    {
      res = MoveBlock(aSelection, *aLeftBlock, theOffset);
    }
    if (brNode) mHTMLEditor->DeleteNode(brNode);
  }
  else
  {
    // normal case.  blocks are siblings, or at least close enough to siblings.  An example
    // of the latter is a <p>paragraph</p><ul><li>one<li>two<li>three</ul>.  The first
    // li and the p are not true siblings, but we still want to join them if you backspace
    // from li into p.
    
    // adjust whitespace at block boundaries
    res = nsWSRunObject::PrepareToJoinBlocks(mHTMLEditor, *aLeftBlock, *aRightBlock);
    if (NS_FAILED(res)) return res;
    // Do br adjustment.
    nsCOMPtr<nsIDOMNode> brNode;
    res = CheckForInvisibleBR(*aLeftBlock, kBlockEnd, address_of(brNode));
    if (NS_FAILED(res)) return res;
    if (bMergeLists || mHTMLEditor->NodesSameType(*aLeftBlock, *aRightBlock))
    {
      // nodes are same type.  merge them.
      nsCOMPtr<nsIDOMNode> parent;
      PRInt32 offset;
      res = JoinNodesSmart(*aLeftBlock, *aRightBlock, address_of(parent), &offset);
      if (NS_FAILED(res)) return res;
      nsCOMPtr<nsIDOMNode> newBlock;
      res = ConvertListType(*aRightBlock, address_of(newBlock), existingListStr, NS_LITERAL_STRING("li"));
    }
    else
    {
      // nodes are disimilar types. 
      res = MoveBlock(aSelection, *aLeftBlock);
    }
    if (brNode) mHTMLEditor->DeleteNode(brNode);
  }
  return res;
}


/*****************************************************************************************************
*    MoveBlock: this method is used to move the "block" implied by the selection into aNewParent.
*    Note that the "block" might merely be inline nodes between <br>s, or between blocks, etc.
*    DTD containment rules are followed throughout.
*         nsISelection *aSelection       the selection.  
*         nsIDOMNode *aNewParent         parent to receive moved content
*         PRInt32 aOffset                offset in aNewParent to move content to
*/
nsresult
nsHTMLEditRules::MoveBlock(nsISelection *aSelection, nsIDOMNode *aNewParent, PRInt32 aOffset)
{
  if (!aSelection) return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsISupportsArray> arrayOfNodes;
  nsCOMPtr<nsISupports> isupports;
  nsCOMPtr<nsIDOMNode> curNode;
  // GetNodesFromSelection is the workhorse that figures out what we wnat to move.
  // TODO: it is bogus to have to pass selectin to JoinBlocks() and thru to MoveBlock.  We only
  // need it because of GetNodesFromSelection().  I need to write other versions of GetNodesFromSelection()
  // that accept a nsIDOMRange or a DOMPoint.
  nsresult res = GetNodesFromSelection(aSelection, kMakeList, address_of(arrayOfNodes), PR_TRUE);
  if (NS_FAILED(res)) return res;
  PRUint32 listCount;
  arrayOfNodes->Count(&listCount);
  PRUint32 i;
  for (i=0; i<listCount; i++)
  {
    // get the node to act on
    isupports  = dont_AddRef(arrayOfNodes->ElementAt(i));
    curNode = do_QueryInterface(isupports);
    if (IsBlockNode(curNode))
    {
      // For block nodes, move their contents only, then delete block.
      res = MoveContents(curNode, aNewParent, &aOffset); 
      if (NS_FAILED(res)) return res;
      res = mHTMLEditor->DeleteNode(curNode);
    }
    else
    {
      // otherwise move the content as is, checking against the dtd.
      res = MoveNodeSmart(curNode, aNewParent, &aOffset);
    }
  }
  return res;
}

/*****************************************************************************************************
*    MoveNodeSmart: this method is used to move node aSource to (aDest,aOffset).
*    DTD containment rules are followed throughout.  aOffset is updated to point _after_
*    inserted content.
*         nsIDOMNode *aSource       the selection.  
*         nsIDOMNode *aDest         parent to receive moved content
*         PRInt32 *aOffset          offset in aNewParent to move content to
*/
nsresult
nsHTMLEditRules::MoveNodeSmart(nsIDOMNode *aSource, nsIDOMNode *aDest, PRInt32 *aOffset)
{
  if (!aSource || !aDest || !aOffset) return NS_ERROR_NULL_POINTER;

  nsAutoString tag;
  nsresult res;
  res = mHTMLEditor->GetTagString(aSource, tag);
  if (NS_FAILED(res)) return res;
  ToLowerCase(tag);
  // check if this node can go into the destination node
  if (mHTMLEditor->CanContainTag(aDest, tag))
  {
    // if it can, move it there
    res = mHTMLEditor->MoveNode(aSource, aDest, *aOffset);
    if (NS_FAILED(res)) return res;
    if (*aOffset != -1) ++(*aOffset);
  }
  else
  {
    // if it can't, move it's children, and then delete it.
    res = MoveContents(aSource, aDest, aOffset);
    if (NS_FAILED(res)) return res;
    res = mHTMLEditor->DeleteNode(aSource);
    if (NS_FAILED(res)) return res;
  }
  return NS_OK;
}

/*****************************************************************************************************
*    MoveContents: this method is used to move node the _contents_ of aSource to (aDest,aOffset).
*    DTD containment rules are followed throughout.  aOffset is updated to point _after_
*    inserted content.  aSource is deleted.
*         nsIDOMNode *aSource       the selection.  
*         nsIDOMNode *aDest         parent to receive moved content
*         PRInt32 *aOffset          offset in aNewParent to move content to
*/
nsresult
nsHTMLEditRules::MoveContents(nsIDOMNode *aSource, nsIDOMNode *aDest, PRInt32 *aOffset)
{
  if (!aSource || !aDest || !aOffset) return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIDOMNode> child;
  nsAutoString tag;
  nsresult res;
  aSource->GetFirstChild(getter_AddRefs(child));
  while (child)
  {
    res = MoveNodeSmart(child, aDest, aOffset);
    if (NS_FAILED(res)) return res;
    aSource->GetFirstChild(getter_AddRefs(child));
  }
  return NS_OK;
}


nsresult
nsHTMLEditRules::DeleteNonTableElements(nsIDOMNode *aNode)
{
  if (!aNode) return NS_ERROR_NULL_POINTER;
  nsresult res = NS_OK;
  if (nsHTMLEditUtils::IsTableElement(aNode) && !nsHTMLEditUtils::IsTable(aNode))
  {
    nsCOMPtr<nsIDOMNodeList> children;
    aNode->GetChildNodes(getter_AddRefs(children));
    if (children)
    {
      PRUint32 len;
      children->GetLength(&len);
      if (!len) return NS_OK;
      PRInt32 j;
      for (j=len-1; j>=0; j--)
      {
        nsCOMPtr<nsIDOMNode> node;
        children->Item(j,getter_AddRefs(node));
        res = DeleteNonTableElements(node);
        if (NS_FAILED(res)) return res;

      }
    }
  }
  else
  {
    res = mHTMLEditor->DeleteNode(aNode);
    if (NS_FAILED(res)) return res;
  }
  return res;
}

nsresult
nsHTMLEditRules::WillMakeList(nsISelection *aSelection, 
                              const nsAString *aListType, 
                              PRBool aEntireList,
                              const nsAString *aBulletType,
                              PRBool *aCancel,
                              PRBool *aHandled,
                              const nsAString *aItemType)
{
  if (!aSelection || !aListType || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }

  nsresult res = WillInsert(aSelection, aCancel);
  if (NS_FAILED(res)) return res;

  // initialize out param
  // we want to ignore result of WillInsert()
  *aCancel = PR_FALSE;
  *aHandled = PR_FALSE;

  // deduce what tag to use for list items
  nsAutoString itemType;
  if (aItemType) 
    itemType = *aItemType;
  else if (aListType->Equals(NS_LITERAL_STRING("dl"),nsCaseInsensitiveStringComparator()))
    itemType.Assign(NS_LITERAL_STRING("dd"));
  else
    itemType.Assign(NS_LITERAL_STRING("li"));
    
  // convert the selection ranges into "promoted" selection ranges:
  // this basically just expands the range to include the immediate
  // block parent, and then further expands to include any ancestors
  // whose children are all in the range
  
  *aHandled = PR_TRUE;

  res = NormalizeSelection(aSelection);
  if (NS_FAILED(res)) return res;
  nsAutoSelectionReset selectionResetter(aSelection, mHTMLEditor);
  
  nsCOMPtr<nsISupportsArray> arrayOfNodes;
  res = GetListActionNodes(address_of(arrayOfNodes), aEntireList);
  if (NS_FAILED(res)) return res;
  
  PRUint32 listCount;
  arrayOfNodes->Count(&listCount);
  
  // check if all our nodes are <br>s, or empty inlines
  PRBool bOnlyBreaks = PR_TRUE;
  PRInt32 j;
  for (j=0; j<(PRInt32)listCount; j++)
  {
    nsCOMPtr<nsISupports> isupports = dont_AddRef(arrayOfNodes->ElementAt(j));
    nsCOMPtr<nsIDOMNode> curNode( do_QueryInterface(isupports ) );
    // if curNode is not a Break or empty inline, we're done
    if ( (!nsTextEditUtils::IsBreak(curNode)) && (!IsEmptyInline(curNode)) )
    {
      bOnlyBreaks = PR_FALSE;
      break;
    }
  }
  
  // if no nodes, we make empty list.  Ditto if the user tried to make a list of some # of breaks.
  if (!listCount || bOnlyBreaks) 
  {
    nsCOMPtr<nsIDOMNode> parent, theList, theListItem;
    PRInt32 offset;

    // if only breaks, delete them
    if (bOnlyBreaks)
    {
      for (j=0; j<(PRInt32)listCount; j++)
      {
        nsCOMPtr<nsISupports> isupports = dont_AddRef(arrayOfNodes->ElementAt(j));
        nsCOMPtr<nsIDOMNode> curNode( do_QueryInterface(isupports ) );
        res = mHTMLEditor->DeleteNode(curNode);
        if (NS_FAILED(res)) return res;
      }
    }
    
    // get selection location
    res = mHTMLEditor->GetStartNodeAndOffset(aSelection, address_of(parent), &offset);
    if (NS_FAILED(res)) return res;
    
    // make sure we can put a list here
    res = SplitAsNeeded(aListType, address_of(parent), &offset);
    if (NS_FAILED(res)) return res;
    res = mHTMLEditor->CreateNode(*aListType, parent, offset, getter_AddRefs(theList));
    if (NS_FAILED(res)) return res;
    res = mHTMLEditor->CreateNode(itemType, theList, 0, getter_AddRefs(theListItem));
    if (NS_FAILED(res)) return res;
    // remember our new block for postprocessing
    mNewBlock = theListItem;
    // put selection in new list item
    res = aSelection->Collapse(theListItem,0);
    selectionResetter.Abort();  // to prevent selection reseter from overriding us.
    *aHandled = PR_TRUE;
    return res;
  }

  // if there is only one node in the array, and it is a list, div, or blockquote,
  // then look inside of it until we find inner list or content.

  res = LookInsideDivBQandList(arrayOfNodes);
  if (NS_FAILED(res)) return res;                                 

  // Next we detect all the transitions in the array, where a transition
  // means that adjacent nodes in the array don't have the same parent.
  
  nsVoidArray transitionList;
  res = MakeTransitionList(arrayOfNodes, &transitionList);
  if (NS_FAILED(res)) return res;                                 

  // Ok, now go through all the nodes and put then in the list, 
  // or whatever is approriate.  Wohoo!

  arrayOfNodes->Count(&listCount);
  nsCOMPtr<nsIDOMNode> curParent;
  nsCOMPtr<nsIDOMNode> curList;
  nsCOMPtr<nsIDOMNode> prevListItem;
  
  PRInt32 i;
  for (i=0; i<(PRInt32)listCount; i++)
  {
    // here's where we actually figure out what to do
    nsCOMPtr<nsIDOMNode> newBlock;
    nsCOMPtr<nsISupports> isupports = dont_AddRef(arrayOfNodes->ElementAt(i));
    nsCOMPtr<nsIDOMNode> curNode( do_QueryInterface(isupports ) );
    PRInt32 offset;
    res = nsEditor::GetNodeLocation(curNode, address_of(curParent), &offset);
    if (NS_FAILED(res)) return res;
  
    // if curNode is a Break, delete it, and quit remembering prev list item
    if (nsTextEditUtils::IsBreak(curNode)) 
    {
      res = mHTMLEditor->DeleteNode(curNode);
      if (NS_FAILED(res)) return res;
      prevListItem = 0;
      continue;
    }
    // if curNode is an empty inline container, delete it
    else if (IsEmptyInline(curNode)) 
    {
      res = mHTMLEditor->DeleteNode(curNode);
      if (NS_FAILED(res)) return res;
      continue;
    }
    
    if (nsHTMLEditUtils::IsList(curNode))
    {
      nsAutoString existingListStr;
      res = mHTMLEditor->GetTagString(curNode, existingListStr);
      ToLowerCase(existingListStr);
      // do we have a curList already?
      if (curList && !nsHTMLEditUtils::IsDescendantOf(curNode, curList))
      {
        // move all of our children into curList.
        // cheezy way to do it: move whole list and then
        // RemoveContainer() on the list.
        // ConvertListType first: that routine
        // handles converting the list item types, if needed
        res = mHTMLEditor->MoveNode(curNode, curList, -1);
        if (NS_FAILED(res)) return res;
        res = ConvertListType(curNode, address_of(newBlock), *aListType, itemType);
        if (NS_FAILED(res)) return res;
        res = mHTMLEditor->RemoveBlockContainer(newBlock);
        if (NS_FAILED(res)) return res;
      }
      else
      {
        // replace list with new list type
        res = ConvertListType(curNode, address_of(newBlock), *aListType, itemType);
        if (NS_FAILED(res)) return res;
        curList = newBlock;
      }
      prevListItem = 0;
      continue;
    }

    if (nsHTMLEditUtils::IsListItem(curNode))
    {
      nsAutoString existingListStr;
      res = mHTMLEditor->GetTagString(curParent, existingListStr);
      ToLowerCase(existingListStr);
      if ( existingListStr != *aListType )
      {
        // list item is in wrong type of list.  
        // if we dont have a curList, split the old list
        // and make a new list of correct type.
        if (!curList || nsHTMLEditUtils::IsDescendantOf(curNode, curList))
        {
          res = mHTMLEditor->SplitNode(curParent, offset, getter_AddRefs(newBlock));
          if (NS_FAILED(res)) return res;
          nsCOMPtr<nsIDOMNode> p;
          PRInt32 o;
          res = nsEditor::GetNodeLocation(curParent, address_of(p), &o);
          if (NS_FAILED(res)) return res;
          res = mHTMLEditor->CreateNode(*aListType, p, o, getter_AddRefs(curList));
          if (NS_FAILED(res)) return res;
        }
        // move list item to new list
        res = mHTMLEditor->MoveNode(curNode, curList, -1);
        if (NS_FAILED(res)) return res;
        // convert list item type if needed
        if (!mHTMLEditor->NodeIsType(curNode,itemType))
        {
          res = mHTMLEditor->ReplaceContainer(curNode, address_of(newBlock), itemType);
          if (NS_FAILED(res)) return res;
        }
      }
      else
      {
        // item is in right type of list.  But we might still have to move it.
        // and we might need to convert list item types.
        if (!curList)
          curList = curParent;
        else
        {
          if (curParent != curList)
          {
            // move list item to new list
            res = mHTMLEditor->MoveNode(curNode, curList, -1);
            if (NS_FAILED(res)) return res;
          }
        }
        if (!mHTMLEditor->NodeIsType(curNode,itemType))
        {
          res = mHTMLEditor->ReplaceContainer(curNode, address_of(newBlock), itemType);
          if (NS_FAILED(res)) return res;
        }
      }
      nsCOMPtr<nsIDOMElement> curElement = do_QueryInterface(curNode);
      NS_NAMED_LITERAL_STRING(typestr, "type");
      if (aBulletType && !aBulletType->IsEmpty()) {
        res = mHTMLEditor->SetAttribute(curElement, typestr, *aBulletType);
      }
      else {
        res = mHTMLEditor->RemoveAttribute(curElement, typestr);
      }
      if (NS_FAILED(res)) return res;
      continue;
    }
    
    // need to make a list to put things in if we haven't already,
    // or if this node doesn't go in list we used earlier.
    if (!curList) // || transitionList[i])
    {
      res = SplitAsNeeded(aListType, address_of(curParent), &offset);
      if (NS_FAILED(res)) return res;
      res = mHTMLEditor->CreateNode(*aListType, curParent, offset, getter_AddRefs(curList));
      if (NS_FAILED(res)) return res;
      // remember our new block for postprocessing
      mNewBlock = curList;
      // curList is now the correct thing to put curNode in
      prevListItem = 0;
    }
  
    // if curNode isn't a list item, we must wrap it in one
    nsCOMPtr<nsIDOMNode> listItem;
    if (!nsHTMLEditUtils::IsListItem(curNode))
    {
      if (IsInlineNode(curNode) && prevListItem)
      {
        // this is a continuation of some inline nodes that belong together in
        // the same list item.  use prevListItem
        res = mHTMLEditor->MoveNode(curNode, prevListItem, -1);
        if (NS_FAILED(res)) return res;
      }
      else
      {
        // don't wrap li around a paragraph.  instead replace paragraph with li
        if (nsHTMLEditUtils::IsParagraph(curNode))
        {
          res = mHTMLEditor->ReplaceContainer(curNode, address_of(listItem), itemType);
        }
        else
        {
          res = mHTMLEditor->InsertContainerAbove(curNode, address_of(listItem), itemType);
        }
        if (NS_FAILED(res)) return res;
        if (IsInlineNode(curNode)) 
          prevListItem = listItem;
        else
          prevListItem = nsnull;
      }
    }
    else
    {
      listItem = curNode;
    }
  
    if (listItem)  // if we made a new list item, deal with it
    {
      // tuck the listItem into the end of the active list
      res = mHTMLEditor->MoveNode(listItem, curList, -1);
      if (NS_FAILED(res)) return res;
    }
  }

  return res;
}


nsresult
nsHTMLEditRules::WillRemoveList(nsISelection *aSelection, 
                                PRBool aOrdered, 
                                PRBool *aCancel,
                                PRBool *aHandled)
{
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }
  // initialize out param
  *aCancel = PR_FALSE;
  *aHandled = PR_TRUE;
  
  nsresult res = NormalizeSelection(aSelection);
  if (NS_FAILED(res)) return res;
  nsAutoSelectionReset selectionResetter(aSelection, mHTMLEditor);
  
  nsCOMPtr<nsISupportsArray> arrayOfRanges;
  res = GetPromotedRanges(aSelection, address_of(arrayOfRanges), kMakeList);
  if (NS_FAILED(res)) return res;
  
  // use these ranges to contruct a list of nodes to act on.
  nsCOMPtr<nsISupportsArray> arrayOfNodes;
  res = GetListActionNodes(address_of(arrayOfNodes), PR_FALSE);
  if (NS_FAILED(res)) return res;                                 
                                     
  // Remove all non-editable nodes.  Leave them be.
  
  PRUint32 listCount;
  PRInt32 i;
  arrayOfNodes->Count(&listCount);
  for (i=listCount-1; i>=0; i--)
  {
    nsCOMPtr<nsISupports> isupports = dont_AddRef(arrayOfNodes->ElementAt(i));
    nsCOMPtr<nsIDOMNode> testNode( do_QueryInterface(isupports ) );
    if (!mHTMLEditor->IsEditable(testNode))
    {
      arrayOfNodes->RemoveElementAt(i);
    }
  }
  
  // Only act on lists or list items in the array
  nsCOMPtr<nsIDOMNode> curParent;
  for (i=0; i<(PRInt32)listCount; i++)
  {
    // here's where we actually figure out what to do
    nsCOMPtr<nsISupports> isupports = dont_AddRef(arrayOfNodes->ElementAt(i));
    nsCOMPtr<nsIDOMNode> curNode( do_QueryInterface(isupports ) );
    PRInt32 offset;
    res = nsEditor::GetNodeLocation(curNode, address_of(curParent), &offset);
    if (NS_FAILED(res)) return res;
    
    if (nsHTMLEditUtils::IsListItem(curNode))  // unlist this listitem
    {
      PRBool bOutOfList;
      do
      {
        res = PopListItem(curNode, &bOutOfList);
        if (NS_FAILED(res)) return res;
      } while (!bOutOfList); // keep popping it out until it's not in a list anymore
    }
    else if (nsHTMLEditUtils::IsList(curNode)) // node is a list, move list items out
    {
      res = RemoveListStructure(curNode);
      if (NS_FAILED(res)) return res;
    }
  }
  return res;
}


nsresult
nsHTMLEditRules::WillMakeDefListItem(nsISelection *aSelection, 
                                     const nsAString *aItemType, 
                                     PRBool aEntireList, 
                                     PRBool *aCancel,
                                     PRBool *aHandled)
{
  // for now we let WillMakeList handle this
  NS_NAMED_LITERAL_STRING(listType, "dl");
  return WillMakeList(aSelection, &listType, aEntireList, nsnull, aCancel, aHandled, aItemType);
}

nsresult
nsHTMLEditRules::WillMakeBasicBlock(nsISelection *aSelection, 
                                    const nsAString *aBlockType, 
                                    PRBool *aCancel,
                                    PRBool *aHandled)
{
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }
  // initialize out param
  *aCancel = PR_FALSE;
  *aHandled = PR_FALSE;
  
  nsresult res = WillInsert(aSelection, aCancel);
  if (NS_FAILED(res)) return res;
  // initialize out param
  // we want to ignore result of WillInsert()
  *aCancel = PR_FALSE;
  res = NormalizeSelection(aSelection);
  if (NS_FAILED(res)) return res;
  nsAutoSelectionReset selectionResetter(aSelection, mHTMLEditor);
  nsAutoTxnsConserveSelection dontSpazMySelection(mHTMLEditor);
  *aHandled = PR_TRUE;

  // contruct a list of nodes to act on.
  nsCOMPtr<nsISupportsArray> arrayOfNodes;
  res = GetNodesFromSelection(aSelection, kMakeBasicBlock, address_of(arrayOfNodes));
  if (NS_FAILED(res)) return res;
  
  nsString tString(*aBlockType);

  // if nothing visible in list, make an empty block
  if (ListIsEmptyLine(arrayOfNodes))
  {
    nsCOMPtr<nsIDOMNode> parent, theBlock;
    PRInt32 offset;
    
    // get selection location
    res = mHTMLEditor->GetStartNodeAndOffset(aSelection, address_of(parent), &offset);
    if (NS_FAILED(res)) return res;
    if (tString.Equals(NS_LITERAL_STRING("normal")) ||
             tString.IsEmpty() ) // we are removing blocks (going to "body text")
    {
      nsCOMPtr<nsIDOMNode> curBlock = parent;
      if (!IsBlockNode(curBlock))
        curBlock = mHTMLEditor->GetBlockNodeParent(parent);
      nsCOMPtr<nsIDOMNode> curBlockPar;
      if (!curBlock) return NS_ERROR_NULL_POINTER;
      curBlock->GetParentNode(getter_AddRefs(curBlockPar));
      nsAutoString curBlockTag;
      nsEditor::GetTagString(curBlock, curBlockTag);
      ToLowerCase(curBlockTag);
      if ((curBlockTag.Equals(NS_LITERAL_STRING("pre"))) || 
          (curBlockTag.Equals(NS_LITERAL_STRING("p")))   ||
          (curBlockTag.Equals(NS_LITERAL_STRING("h1")))  ||
          (curBlockTag.Equals(NS_LITERAL_STRING("h2")))  ||
          (curBlockTag.Equals(NS_LITERAL_STRING("h3")))  ||
          (curBlockTag.Equals(NS_LITERAL_STRING("h4")))  ||
          (curBlockTag.Equals(NS_LITERAL_STRING("h5")))  ||
          (curBlockTag.Equals(NS_LITERAL_STRING("h6")))  ||
          (curBlockTag.Equals(NS_LITERAL_STRING("address"))))
      {
        // if the first editable node after selection is a br, consume it.  Otherwise
        // it gets pushed into a following block after the split, which is visually bad.
        nsCOMPtr<nsIDOMNode> brNode;
        res = mHTMLEditor->GetNextHTMLNode(parent, offset, address_of(brNode));
        if (NS_FAILED(res)) return res;        
        if (brNode && nsTextEditUtils::IsBreak(brNode))
        {
          res = mHTMLEditor->DeleteNode(brNode);
          if (NS_FAILED(res)) return res; 
        }
        // do the splits!
        res = mHTMLEditor->SplitNodeDeep(curBlock, parent, offset, &offset, PR_TRUE);
        if (NS_FAILED(res)) return res;
        // put a br at the split point
        res = mHTMLEditor->CreateBR(curBlockPar, offset, address_of(brNode));
        if (NS_FAILED(res)) return res;
        // put selection at the split point
        res = aSelection->Collapse(curBlockPar, offset);
        selectionResetter.Abort();  // to prevent selection reseter from overriding us.
        *aHandled = PR_TRUE;
      }
      // else nothing to do!
    }
    else  // we are making a block
    {   
      // consume a br, if needed
      nsCOMPtr<nsIDOMNode> brNode;
      res = mHTMLEditor->GetNextHTMLNode(parent, offset, address_of(brNode), PR_TRUE);
      if (NS_FAILED(res)) return res;
      if (brNode && nsTextEditUtils::IsBreak(brNode))
      {
        res = mHTMLEditor->DeleteNode(brNode);
        if (NS_FAILED(res)) return res;
      }
      // make sure we can put a block here
      res = SplitAsNeeded(aBlockType, address_of(parent), &offset);
      if (NS_FAILED(res)) return res;
      res = mHTMLEditor->CreateNode(*aBlockType, parent, offset, getter_AddRefs(theBlock));
      if (NS_FAILED(res)) return res;
      // remember our new block for postprocessing
      mNewBlock = theBlock;
      // delete anything that was in the list of nodes
      nsCOMPtr<nsISupports> isupports = dont_AddRef(arrayOfNodes->ElementAt(0));
      nsCOMPtr<nsIDOMNode> curNode;
      while (isupports)
      {
        curNode = do_QueryInterface(isupports);
        res = mHTMLEditor->DeleteNode(curNode);
        if (NS_FAILED(res)) return res;
        res = arrayOfNodes->RemoveElementAt(0);
        if (NS_FAILED(res)) return res;
        isupports = dont_AddRef(arrayOfNodes->ElementAt(0));
      }
      // put selection in new block
      res = aSelection->Collapse(theBlock,0);
      selectionResetter.Abort();  // to prevent selection reseter from overriding us.
      *aHandled = PR_TRUE;
    }
    return res;    
  }
  else
  {
    // Ok, now go through all the nodes and make the right kind of blocks, 
    // or whatever is approriate.  Wohoo! 
    // Note: blockquote is handled a little differently
    if (tString.Equals(NS_LITERAL_STRING("blockquote")))
      res = MakeBlockquote(arrayOfNodes);
    else if (tString.Equals(NS_LITERAL_STRING("normal")) ||
             tString.IsEmpty() )
      res = RemoveBlockStyle(arrayOfNodes);
    else
      res = ApplyBlockStyle(arrayOfNodes, aBlockType);
    return res;
  }
  return res;
}

nsresult 
nsHTMLEditRules::DidMakeBasicBlock(nsISelection *aSelection,
                                   nsRulesInfo *aInfo, nsresult aResult)
{
  if (!aSelection) return NS_ERROR_NULL_POINTER;
  // check for empty block.  if so, put a moz br in it.
  PRBool isCollapsed;
  nsresult res = aSelection->GetIsCollapsed(&isCollapsed);
  if (NS_FAILED(res)) return res;
  if (!isCollapsed) return NS_OK;

  nsCOMPtr<nsIDOMNode> parent;
  PRInt32 offset;
  res = nsEditor::GetStartNodeAndOffset(aSelection, address_of(parent), &offset);
  if (NS_FAILED(res)) return res;
  res = InsertMozBRIfNeeded(parent);
  return res;
}

nsresult
nsHTMLEditRules::WillIndent(nsISelection *aSelection, PRBool *aCancel, PRBool * aHandled)
{
  PRBool useCSS;
  nsresult res;
  mHTMLEditor->GetIsCSSEnabled(&useCSS);
  
  if (useCSS) {
    res = WillCSSIndent(aSelection, aCancel, aHandled);
  }
  else {
    res = WillHTMLIndent(aSelection, aCancel, aHandled);
  }
  return res;
}

nsresult
nsHTMLEditRules::WillCSSIndent(nsISelection *aSelection, PRBool *aCancel, PRBool * aHandled)
{
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }
  
  nsresult res = WillInsert(aSelection, aCancel);
  if (NS_FAILED(res)) return res;

  // initialize out param
  // we want to ignore result of WillInsert()
  *aCancel = PR_FALSE;
  *aHandled = PR_TRUE;

  res = NormalizeSelection(aSelection);
  if (NS_FAILED(res)) return res;
  nsAutoSelectionReset selectionResetter(aSelection, mHTMLEditor);
  nsCOMPtr<nsISupportsArray> arrayOfRanges;
  nsCOMPtr<nsISupportsArray> arrayOfNodes;
  
  // short circuit: detect case of collapsed selection inside an <li>.
  // just sublist that <li>.  This prevents bug 97797.
  
  PRBool bCollapsed;
  nsCOMPtr<nsIDOMNode> liNode;
  res = aSelection->GetIsCollapsed(&bCollapsed);
  if (NS_FAILED(res)) return res;
  if (bCollapsed) 
  {
    nsCOMPtr<nsIDOMNode> node, block;
    PRInt32 offset;
    nsresult res = mHTMLEditor->GetStartNodeAndOffset(aSelection, address_of(node), &offset);
    if (NS_FAILED(res)) return res;
    if (IsBlockNode(node)) 
      block = node;
    else
      block = mHTMLEditor->GetBlockNodeParent(node);
    if (block && nsHTMLEditUtils::IsListItem(block))
      liNode = block;
  }
  
  if (liNode)
  {
    res = NS_NewISupportsArray(getter_AddRefs(arrayOfNodes));
    if (NS_FAILED(res)) return res;
    nsCOMPtr<nsISupports> isupports = do_QueryInterface(liNode);
    arrayOfNodes->AppendElement(isupports);
  }
  else
  {
    // convert the selection ranges into "promoted" selection ranges:
    // this basically just expands the range to include the immediate
    // block parent, and then further expands to include any ancestors
    // whose children are all in the range
    res = GetNodesFromSelection(aSelection, kIndent, address_of(arrayOfNodes));
    if (NS_FAILED(res)) return res;
  }
  
  NS_NAMED_LITERAL_STRING(quoteType, "blockquote");
  // if nothing visible in list, make an empty block
  if (ListIsEmptyLine(arrayOfNodes))
  {
    nsCOMPtr<nsIDOMNode> parent, theBlock;
    PRInt32 offset;
    nsAutoString quoteType(NS_LITERAL_STRING("div"));
    // get selection location
    res = mHTMLEditor->GetStartNodeAndOffset(aSelection, address_of(parent), &offset);
    if (NS_FAILED(res)) return res;
    // make sure we can put a block here
    res = SplitAsNeeded(&quoteType, address_of(parent), &offset);
    if (NS_FAILED(res)) return res;
    res = mHTMLEditor->CreateNode(quoteType, parent, offset, getter_AddRefs(theBlock));
    if (NS_FAILED(res)) return res;
    // remember our new block for postprocessing
    mNewBlock = theBlock;
    RelativeChangeIndentation(theBlock, +1);
    // delete anything that was in the list of nodes
    nsCOMPtr<nsISupports> isupports = dont_AddRef(arrayOfNodes->ElementAt(0));
    nsCOMPtr<nsIDOMNode> curNode;
    while (isupports)
    {
      curNode = do_QueryInterface(isupports);
      res = mHTMLEditor->DeleteNode(curNode);
      if (NS_FAILED(res)) return res;
      res = arrayOfNodes->RemoveElementAt(0);
      if (NS_FAILED(res)) return res;
      isupports = dont_AddRef(arrayOfNodes->ElementAt(0));
    }
    // put selection in new block
    res = aSelection->Collapse(theBlock,0);
    selectionResetter.Abort();  // to prevent selection reseter from overriding us.
    *aHandled = PR_TRUE;
    return res;
  }
  // Next we detect all the transitions in the array, where a transition
  // means that adjacent nodes in the array don't have the same parent.
  
  nsVoidArray transitionList;
  res = MakeTransitionList(arrayOfNodes, &transitionList);
  if (NS_FAILED(res)) return res;                                 
  
  // Ok, now go through all the nodes and put them in a blockquote, 
  // or whatever is appropriate.  Wohoo!
  PRInt32 i;
  nsCOMPtr<nsIDOMNode> curParent;
  nsCOMPtr<nsIDOMNode> curQuote;
  nsCOMPtr<nsIDOMNode> curList;
  PRUint32 listCount;
  arrayOfNodes->Count(&listCount);
  for (i=0; i<(PRInt32)listCount; i++)
  {
    // here's where we actually figure out what to do
    nsCOMPtr<nsISupports> isupports = dont_AddRef(arrayOfNodes->ElementAt(i));
    nsCOMPtr<nsIDOMNode> curNode( do_QueryInterface(isupports ) );
    PRInt32 offset;
    res = nsEditor::GetNodeLocation(curNode, address_of(curParent), &offset);
    if (NS_FAILED(res)) return res;
    
    // some logic for putting list items into nested lists...
    if (nsHTMLEditUtils::IsList(curParent))
    {
      if (!curList || transitionList[i])
      {
        nsAutoString listTag;
        nsEditor::GetTagString(curParent,listTag);
        ToLowerCase(listTag);
        // create a new nested list of correct type
        res = SplitAsNeeded(&listTag, address_of(curParent), &offset);
        if (NS_FAILED(res)) return res;
        res = mHTMLEditor->CreateNode(listTag, curParent, offset, getter_AddRefs(curList));
        if (NS_FAILED(res)) return res;
        // curList is now the correct thing to put curNode in
        // remember our new block for postprocessing
        mNewBlock = curList;
      }
      // tuck the node into the end of the active list
      PRUint32 listLen;
      res = mHTMLEditor->GetLengthOfDOMNode(curList, listLen);
      if (NS_FAILED(res)) return res;
      res = mHTMLEditor->MoveNode(curNode, curList, listLen);
      if (NS_FAILED(res)) return res;
    }
    
    else // not a list item
    {
      if (IsBlockNode(curNode)) {
        RelativeChangeIndentation(curNode, +1);
        curQuote = nsnull;
      }
      else {
        if (!curQuote) // || transitionList[i])
        {
          NS_NAMED_LITERAL_STRING(divquoteType, "div");
          res = SplitAsNeeded(&divquoteType, address_of(curParent), &offset);
          if (NS_FAILED(res)) return res;
          res = mHTMLEditor->CreateNode(divquoteType, curParent, offset, getter_AddRefs(curQuote));
          if (NS_FAILED(res)) return res;
          RelativeChangeIndentation(curQuote, +1);
          // remember our new block for postprocessing
          mNewBlock = curQuote;
          // curQuote is now the correct thing to put curNode in
        }
        
        // tuck the node into the end of the active blockquote
        PRUint32 quoteLen;
        res = mHTMLEditor->GetLengthOfDOMNode(curQuote, quoteLen);
        if (NS_FAILED(res)) return res;
        res = mHTMLEditor->MoveNode(curNode, curQuote, quoteLen);
        if (NS_FAILED(res)) return res;
      }
    }
  }
  return res;
}

nsresult
nsHTMLEditRules::WillHTMLIndent(nsISelection *aSelection, PRBool *aCancel, PRBool * aHandled)
{
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }
  nsresult res = WillInsert(aSelection, aCancel);
  if (NS_FAILED(res)) return res;

  // initialize out param
  // we want to ignore result of WillInsert()
  *aCancel = PR_FALSE;
  *aHandled = PR_TRUE;

  res = NormalizeSelection(aSelection);
  if (NS_FAILED(res)) return res;
  nsAutoSelectionReset selectionResetter(aSelection, mHTMLEditor);
  
  // convert the selection ranges into "promoted" selection ranges:
  // this basically just expands the range to include the immediate
  // block parent, and then further expands to include any ancestors
  // whose children are all in the range
  
  nsCOMPtr<nsISupportsArray> arrayOfRanges;
  res = GetPromotedRanges(aSelection, address_of(arrayOfRanges), kIndent);
  if (NS_FAILED(res)) return res;
  
  // use these ranges to contruct a list of nodes to act on.
  nsCOMPtr<nsISupportsArray> arrayOfNodes;
  res = GetNodesForOperation(arrayOfRanges, address_of(arrayOfNodes), kIndent);
  if (NS_FAILED(res)) return res;                                 
                                     
  NS_NAMED_LITERAL_STRING(quoteType, "blockquote");

  // if nothing visible in list, make an empty block
  if (ListIsEmptyLine(arrayOfNodes))
  {
    nsCOMPtr<nsIDOMNode> parent, theBlock;
    PRInt32 offset;
    
    // get selection location
    res = mHTMLEditor->GetStartNodeAndOffset(aSelection, address_of(parent), &offset);
    if (NS_FAILED(res)) return res;
    // make sure we can put a block here
    res = SplitAsNeeded(&quoteType, address_of(parent), &offset);
    if (NS_FAILED(res)) return res;
    res = mHTMLEditor->CreateNode(quoteType, parent, offset, getter_AddRefs(theBlock));
    if (NS_FAILED(res)) return res;
    // remember our new block for postprocessing
    mNewBlock = theBlock;
    // delete anything that was in the list of nodes
    nsCOMPtr<nsISupports> isupports = dont_AddRef(arrayOfNodes->ElementAt(0));
    nsCOMPtr<nsIDOMNode> curNode;
    while (isupports)
    {
      curNode = do_QueryInterface(isupports);
      res = mHTMLEditor->DeleteNode(curNode);
      if (NS_FAILED(res)) return res;
      res = arrayOfNodes->RemoveElementAt(0);
      if (NS_FAILED(res)) return res;
      isupports = dont_AddRef(arrayOfNodes->ElementAt(0));
    }
    // put selection in new block
    res = aSelection->Collapse(theBlock,0);
    selectionResetter.Abort();  // to prevent selection reseter from overriding us.
    *aHandled = PR_TRUE;
    return res;
  }

  // Ok, now go through all the nodes and put them in a blockquote, 
  // or whatever is appropriate.  Wohoo!
  PRInt32 i;
  nsCOMPtr<nsIDOMNode> curParent, curQuote, curList, indentedLI, sibling;
  PRUint32 listCount;
  arrayOfNodes->Count(&listCount);
  for (i=0; i<(PRInt32)listCount; i++)
  {
    // here's where we actually figure out what to do
    nsCOMPtr<nsISupports> isupports = dont_AddRef(arrayOfNodes->ElementAt(i));
    nsCOMPtr<nsIDOMNode> curNode( do_QueryInterface(isupports ) );

    // Ignore all non-editable nodes.  Leave them be.
    if (!mHTMLEditor->IsEditable(curNode)) continue;

    PRInt32 offset;
    res = nsEditor::GetNodeLocation(curNode, address_of(curParent), &offset);
    if (NS_FAILED(res)) return res;
     
    // some logic for putting list items into nested lists...
    if (nsHTMLEditUtils::IsList(curParent))
    {
      // check to see if curList is still appropriate.  Which it is if
      // curNode is still right after it in the same list.
      if (curList)
      {
        sibling = nsnull;
        mHTMLEditor->GetPriorHTMLSibling(curNode, address_of(sibling));
      }
      
      if (!curList || (sibling && sibling != curList) )
      {
        nsAutoString listTag;
        nsEditor::GetTagString(curParent,listTag);
        ToLowerCase(listTag);
        // create a new nested list of correct type
        res = SplitAsNeeded(&listTag, address_of(curParent), &offset);
        if (NS_FAILED(res)) return res;
        res = mHTMLEditor->CreateNode(listTag, curParent, offset, getter_AddRefs(curList));
        if (NS_FAILED(res)) return res;
        // curList is now the correct thing to put curNode in
        // remember our new block for postprocessing
        mNewBlock = curList;
      }
      // tuck the node into the end of the active list
      res = mHTMLEditor->MoveNode(curNode, curList, -1);
      if (NS_FAILED(res)) return res;
      // forget curQuote, if any
      curQuote = nsnull;
    }
    
    else // not a list item, use blockquote?
    {
      // if we are inside a list item, we dont want to blockquote, we want
      // to sublist the list item.  We may have several nodes listed in the
      // array of nodes to act on, that are in the same list item.  Since
      // we only want to indent that li once, we must keep track of the most
      // recent indented list item, and not indent it if we find another node
      // to act on that is still inside the same li.
      nsCOMPtr<nsIDOMNode> listitem=IsInListItem(curNode);
      if (listitem)
      {
        if (indentedLI == listitem) continue;  // already indented this list item
        res = nsEditor::GetNodeLocation(listitem, address_of(curParent), &offset);
        if (NS_FAILED(res)) return res;
        // check to see if curList is still appropriate.  Which it is if
        // curNode is still right after it in the same list.
        if (curList)
        {
          sibling = nsnull;
          mHTMLEditor->GetPriorHTMLSibling(curNode, address_of(sibling));
        }
         
        if (!curList || (sibling && sibling != curList) )
        {
          nsAutoString listTag;
          nsEditor::GetTagString(curParent,listTag);
          ToLowerCase(listTag);
          // create a new nested list of correct type
          res = SplitAsNeeded(&listTag, address_of(curParent), &offset);
          if (NS_FAILED(res)) return res;
          res = mHTMLEditor->CreateNode(listTag, curParent, offset, getter_AddRefs(curList));
          if (NS_FAILED(res)) return res;
        }
        res = mHTMLEditor->MoveNode(listitem, curList, -1);
        if (NS_FAILED(res)) return res;
        // remember we indented this li
        indentedLI = listitem;
      }
      
      else
      {
        // need to make a blockquote to put things in if we haven't already,
        // or if this node doesn't go in blockquote we used earlier.
        // One reason it might not go in prio blockquote is if we are now
        // in a different table cell. 
        if (curQuote)
        {
          PRBool bInDifTblElems;
          res = InDifferentTableElements(curQuote, curNode, &bInDifTblElems);
          if (NS_FAILED(res)) return res;
          if (bInDifTblElems)
            curQuote = nsnull;
        }
        
        if (!curQuote) 
        {
          res = SplitAsNeeded(&quoteType, address_of(curParent), &offset);
          if (NS_FAILED(res)) return res;
          res = mHTMLEditor->CreateNode(quoteType, curParent, offset, getter_AddRefs(curQuote));
          if (NS_FAILED(res)) return res;
          // remember our new block for postprocessing
          mNewBlock = curQuote;
          // curQuote is now the correct thing to put curNode in
        }
          
        // tuck the node into the end of the active blockquote
        res = mHTMLEditor->MoveNode(curNode, curQuote, -1);
        if (NS_FAILED(res)) return res;
        // forget curList, if any
        curList = nsnull;
      }
    }
  }
  return res;
}


nsresult
nsHTMLEditRules::WillOutdent(nsISelection *aSelection, PRBool *aCancel, PRBool *aHandled)
{
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }
  // initialize out param
  *aCancel = PR_FALSE;
  *aHandled = PR_TRUE;
  nsresult res = NS_OK;
  nsCOMPtr<nsIDOMNode> rememberedLeftBQ, rememberedRightBQ;
  PRBool useCSS;
  mHTMLEditor->GetIsCSSEnabled(&useCSS);

  res = NormalizeSelection(aSelection);
  if (NS_FAILED(res)) return res;
  // some scoping for selection resetting - we may need to tweak it
  {
    nsAutoSelectionReset selectionResetter(aSelection, mHTMLEditor);
    
    // convert the selection ranges into "promoted" selection ranges:
    // this basically just expands the range to include the immediate
    // block parent, and then further expands to include any ancestors
    // whose children are all in the range
    nsCOMPtr<nsISupportsArray> arrayOfNodes;
    res = GetNodesFromSelection(aSelection, kOutdent, address_of(arrayOfNodes));
    if (NS_FAILED(res)) return res;

    // Ok, now go through all the nodes and remove a level of blockquoting, 
    // or whatever is appropriate.  Wohoo!

    nsCOMPtr<nsIDOMNode> curBlockQuote, firstBQChild, lastBQChild;
    PRUint32 listCount;
    PRInt32 i;
    arrayOfNodes->Count(&listCount);
    nsCOMPtr<nsIDOMNode> curParent;
    for (i=0; i<(PRInt32)listCount; i++)
    {
      // here's where we actually figure out what to do
      nsCOMPtr<nsISupports> isupports = dont_AddRef(arrayOfNodes->ElementAt(i));
      nsCOMPtr<nsIDOMNode> curNode( do_QueryInterface(isupports ) );
      PRInt32 offset;
      res = nsEditor::GetNodeLocation(curNode, address_of(curParent), &offset);
      if (NS_FAILED(res)) return res;
      
      // is it a blockquote?
      if (nsHTMLEditUtils::IsBlockquote(curNode)) 
      {
        // if it is a blockquote, remove it.
        // So we need to finish up dealng with any curBlockQuote first.
        if (curBlockQuote)
        {
          res = RemovePartOfBlock(curBlockQuote, firstBQChild, lastBQChild, 
                                  address_of(rememberedLeftBQ), address_of(rememberedRightBQ));
          if (NS_FAILED(res)) return res;
          curBlockQuote = 0;  firstBQChild = 0;  lastBQChild = 0;
        }
        res = mHTMLEditor->RemoveBlockContainer(curNode);
        if (NS_FAILED(res)) return res;
        continue;
      }
      // is it a list item?
      if (nsHTMLEditUtils::IsListItem(curNode)) 
      {
        // if it is a list item, that means we are not outdenting whole list.
        // So we need to finish up dealng with any curBlockQuote, and then
        // pop this list item.
        if (curBlockQuote)
        {
          res = RemovePartOfBlock(curBlockQuote, firstBQChild, lastBQChild,
                                  address_of(rememberedLeftBQ), address_of(rememberedRightBQ));
          if (NS_FAILED(res)) return res;
          curBlockQuote = 0;  firstBQChild = 0;  lastBQChild = 0;
        }
        PRBool bOutOfList;
        res = PopListItem(curNode, &bOutOfList);
        if (NS_FAILED(res)) return res;
        continue;
      }
      // do we have a blockquote that we are already committed to removing?
      if (curBlockQuote)
      {
        // if so, is this node a descendant?
        if (nsHTMLEditUtils::IsDescendantOf(curNode, curBlockQuote))
        {
          lastBQChild = curNode;
          continue;  // then we dont need to do anything different for this node
        }
        else
        {
          // otherwise, we have progressed beyond end of curBlockQuote,
          // so lets handle it now.  We need to remove the portion of 
          // curBlockQuote that contains [firstBQChild - lastBQChild].
          res = RemovePartOfBlock(curBlockQuote, firstBQChild, lastBQChild,
                                  address_of(rememberedLeftBQ), address_of(rememberedRightBQ));
          if (NS_FAILED(res)) return res;
          curBlockQuote = 0;  firstBQChild = 0;  lastBQChild = 0;
          // fall out and handle curNode
        }
      }
      
      // are we inside a blockquote?
      nsCOMPtr<nsIDOMNode> n = curNode;
      nsCOMPtr<nsIDOMNode> tmp;
      // keep looking up the heirarchy as long as we dont hit the body or a table element
      // (other than an entire table)
      while (!nsTextEditUtils::IsBody(n) &&   
             (nsHTMLEditUtils::IsTable(n) || !nsHTMLEditUtils::IsTableElement(n)))
      {
        n->GetParentNode(getter_AddRefs(tmp));
        n = tmp;
        if (nsHTMLEditUtils::IsBlockquote(n))
        {
          // if so, remember it, and remember first node we are taking out of it.
          curBlockQuote = n;
          firstBQChild  = curNode;
          lastBQChild   = curNode;
          break;
        }
      }

      if (!curBlockQuote)
      {
        // could not find an enclosing blockquote for this node.  handle list cases.
        if (nsHTMLEditUtils::IsList(curParent))  // move node out of list
        {
          if (nsHTMLEditUtils::IsList(curNode))  // just unwrap this sublist
          {
            res = mHTMLEditor->RemoveBlockContainer(curNode);
            if (NS_FAILED(res)) return res;
          }
          // handled list item case above
        }
        else if (nsHTMLEditUtils::IsList(curNode)) // node is a list, but parent is non-list: move list items out
        {
          nsCOMPtr<nsIDOMNode> child;
          curNode->GetLastChild(getter_AddRefs(child));
          while (child)
          {
            if (nsHTMLEditUtils::IsListItem(child))
            {
              PRBool bOutOfList;
              res = PopListItem(child, &bOutOfList);
              if (NS_FAILED(res)) return res;
            }
            else if (nsHTMLEditUtils::IsList(child))
            {
              // We have an embedded list, so move it out from under the
              // parent list. Be sure to put it after the parent list
              // because this loop iterates backwards through the parent's
              // list of children.

              res = mHTMLEditor->MoveNode(child, curParent, offset + 1);
              if (NS_FAILED(res)) return res;
            }
            else
            {
              // delete any non- list items for now
              res = mHTMLEditor->DeleteNode(child);
              if (NS_FAILED(res)) return res;
            }
            curNode->GetLastChild(getter_AddRefs(child));
          }
          // delete the now-empty list
          res = mHTMLEditor->RemoveBlockContainer(curNode);
          if (NS_FAILED(res)) return res;
        }
        else if (useCSS) {
          RelativeChangeIndentation(curNode, -1);
        }
      }
    }
    if (curBlockQuote)
    {
      // we have a blockquote we haven't finished handling
      res = RemovePartOfBlock(curBlockQuote, firstBQChild, lastBQChild, 
                              address_of(rememberedLeftBQ), address_of(rememberedRightBQ));
      if (NS_FAILED(res)) return res;
    }
  }
  // make sure selection didn't stick to last piece of content in old bq
  // (only a roblem for collapsed selections)
  if (rememberedLeftBQ || rememberedRightBQ)
  {
    PRBool bCollapsed;
    res = aSelection->GetIsCollapsed(&bCollapsed);
    if (bCollapsed)
    {
      // push selection past end of rememberedLeftBQ
      nsCOMPtr<nsIDOMNode> sNode;
      PRInt32 sOffset;
      mHTMLEditor->GetStartNodeAndOffset(aSelection, address_of(sNode), &sOffset);
      if ((sNode == rememberedLeftBQ) || nsHTMLEditUtils::IsDescendantOf(sNode, rememberedLeftBQ))
      {
        // selection is inside rememberedLeftBQ - push it past it.
        nsEditor::GetNodeLocation(rememberedLeftBQ, address_of(sNode), &sOffset);
        sOffset++;
        aSelection->Collapse(sNode, sOffset);
      }
      // and pull selection before beginning of rememberedRightBQ
      mHTMLEditor->GetStartNodeAndOffset(aSelection, address_of(sNode), &sOffset);
      if ((sNode == rememberedRightBQ) || nsHTMLEditUtils::IsDescendantOf(sNode, rememberedRightBQ))
      {
        // selection is inside rememberedRightBQ - push it before it.
        nsEditor::GetNodeLocation(rememberedRightBQ, address_of(sNode), &sOffset);
        aSelection->Collapse(sNode, sOffset);
      }
    }
  }
  return res;
}


///////////////////////////////////////////////////////////////////////////
// RemovePartOfBlock:  split aBlock and move aStartChild to aEndChild out
//                     of aBlock.  return left side of block (if any) in
//                     aLeftNode.  return right side of block (if any) in
//                     aRightNode.  
//                  
nsresult 
nsHTMLEditRules::RemovePartOfBlock(nsIDOMNode *aBlock, 
                                   nsIDOMNode *aStartChild, 
                                   nsIDOMNode *aEndChild,
                                   nsCOMPtr<nsIDOMNode> *aLeftNode,
                                   nsCOMPtr<nsIDOMNode> *aRightNode)
{
  if (!aBlock || !aStartChild || !aEndChild)
    return NS_ERROR_NULL_POINTER;
  
  nsCOMPtr<nsIDOMNode> startParent, endParent, leftNode, rightNode;
  PRInt32 startOffset, endOffset, offset;
  nsresult res;

  // get split point location
  res = nsEditor::GetNodeLocation(aStartChild, address_of(startParent), &startOffset);
  if (NS_FAILED(res)) return res;
  
  // do the splits!
  res = mHTMLEditor->SplitNodeDeep(aBlock, startParent, startOffset, &offset, 
                                   PR_TRUE, address_of(leftNode), address_of(rightNode));
  if (NS_FAILED(res)) return res;
  if (rightNode)  aBlock = rightNode;

  // remember left portion of block if caller requested
  if (aLeftNode) 
    *aLeftNode = leftNode;

  // get split point location
  res = nsEditor::GetNodeLocation(aEndChild, address_of(endParent), &endOffset);
  if (NS_FAILED(res)) return res;
  endOffset++;  // want to be after lastBQChild

  // do the splits!
  res = mHTMLEditor->SplitNodeDeep(aBlock, endParent, endOffset, &offset, 
                                   PR_TRUE, address_of(leftNode), address_of(rightNode));
  if (NS_FAILED(res)) return res;
  if (leftNode)  aBlock = leftNode;
  
  // remember right portion of block if caller requested
  if (aRightNode) 
    *aRightNode = rightNode;

  // get rid of part of blockquote we are outdenting
  res = mHTMLEditor->RemoveBlockContainer(aBlock);
  
  return res;
}

///////////////////////////////////////////////////////////////////////////
// ConvertListType:  convert list type and list item type.
//                
//                  
nsresult 
nsHTMLEditRules::ConvertListType(nsIDOMNode *aList, 
                                 nsCOMPtr<nsIDOMNode> *outList,
                                 const nsAString& aListType, 
                                 const nsAString& aItemType) 
{
  if (!aList || !outList) return NS_ERROR_NULL_POINTER;
  *outList = aList;  // we might not need to change the list
  nsresult res = NS_OK;
  nsCOMPtr<nsIDOMNode> child, temp;
  aList->GetFirstChild(getter_AddRefs(child));
  while (child)
  {
    if (nsHTMLEditUtils::IsListItem(child) && !mHTMLEditor->NodeIsType(child, aItemType))
    {
      res = mHTMLEditor->ReplaceContainer(child, address_of(temp), aItemType);
      if (NS_FAILED(res)) return res;
      child = temp;
    }
    else if (nsHTMLEditUtils::IsList(child) && !mHTMLEditor->NodeIsType(child, aListType))
    {
      res = ConvertListType(child, address_of(temp), aListType, aItemType);
      if (NS_FAILED(res)) return res;
      child = temp;
    }
    child->GetNextSibling(getter_AddRefs(temp));
    child = temp;
  }
  if (!mHTMLEditor->NodeIsType(aList, aListType))
  {
    res = mHTMLEditor->ReplaceContainer(aList, outList, aListType);
  }
  return res;
}


///////////////////////////////////////////////////////////////////////////
// CreateStyleForInsertText:  take care of clearing and setting appropriate
//                            style nodes for text insertion.
//                
//                  
nsresult 
nsHTMLEditRules::CreateStyleForInsertText(nsISelection *aSelection, nsIDOMDocument *aDoc) 
{
  if (!aSelection || !aDoc) return NS_ERROR_NULL_POINTER;
  if (!mHTMLEditor->mTypeInState) return NS_ERROR_NULL_POINTER;
  
  PRBool weDidSometing = PR_FALSE;
  nsCOMPtr<nsIDOMNode> node, tmp;
  PRInt32 offset;
  nsresult res = mHTMLEditor->GetStartNodeAndOffset(aSelection, address_of(node), &offset);
  if (NS_FAILED(res)) return res;
  PropItem *item = nsnull;
  
  // process clearing any styles first
  mHTMLEditor->mTypeInState->TakeClearProperty(&item);
  while (item)
  {
    nsCOMPtr<nsIDOMNode> leftNode, rightNode, secondSplitParent, newSelParent, savedBR;
    res = mHTMLEditor->SplitStyleAbovePoint(address_of(node), &offset, item->tag, &item->attr, address_of(leftNode), address_of(rightNode));
    if (NS_FAILED(res)) return res;
    PRBool bIsEmptyNode;
    if (leftNode)
    {
      mHTMLEditor->IsEmptyNode(leftNode, &bIsEmptyNode, PR_FALSE, PR_TRUE);
      if (bIsEmptyNode)
      {
        // delete leftNode if it became empty
        res = mEditor->DeleteNode(leftNode);
        if (NS_FAILED(res)) return res;
      }
    }
    if (rightNode)
    {
      secondSplitParent = mHTMLEditor->GetLeftmostChild(rightNode);
      // don't try to split br's...
      // note: probably should only split containers, but being more conservative in changes for now.
      if (!secondSplitParent) secondSplitParent = rightNode;
      if (nsTextEditUtils::IsBreak(secondSplitParent))
      {
        savedBR = secondSplitParent;
        savedBR->GetParentNode(getter_AddRefs(tmp));
        secondSplitParent = tmp;
      }
      offset = 0;
      res = mHTMLEditor->SplitStyleAbovePoint(address_of(secondSplitParent), &offset, item->tag, &(item->attr), address_of(leftNode), address_of(rightNode));
      if (NS_FAILED(res)) return res;
      // should be impossible to not get a new leftnode here
      if (!leftNode) return NS_ERROR_FAILURE;
      newSelParent = mHTMLEditor->GetLeftmostChild(leftNode);
      if (!newSelParent) newSelParent = leftNode;
      // if rightNode starts with a br, suck it out of right node and into leftNode.
      // This is so we you don't revert back to the previous style if you happen to click at the end of a line.
      if (savedBR)
      {
        res = mEditor->MoveNode(savedBR, newSelParent, 0);
        if (NS_FAILED(res)) return res;
      }
      mHTMLEditor->IsEmptyNode(rightNode, &bIsEmptyNode, PR_FALSE, PR_TRUE);
      if (bIsEmptyNode)
      {
        // delete rightNode if it became empty
        res = mEditor->DeleteNode(rightNode);
        if (NS_FAILED(res)) return res;
      }
      // remove the style on this new heirarchy
      PRInt32 newSelOffset = 0;
      {
        // track the point at the new heirarchy.
        // This is so we can know where to put the selection after we call
        // RemoveStyleInside().  RemoveStyleInside() could remove any and all of those nodes,
        // so I have to use the range tracking system to find the right spot to put selection.
        nsAutoTrackDOMPoint tracker(mHTMLEditor->mRangeUpdater, address_of(newSelParent), &newSelOffset);
        res = mHTMLEditor->RemoveStyleInside(leftNode, item->tag, &(item->attr));
        if (NS_FAILED(res)) return res;
      }
      // reset our node offset values to the resulting new sel point
      node = newSelParent;
      offset = newSelOffset;
    }
    // we own item now (TakeClearProperty hands ownership to us)
    delete item;
    mHTMLEditor->mTypeInState->TakeClearProperty(&item);
    weDidSometing = PR_TRUE;
  }
  
  // then process setting any styles
  PRInt32 relFontSize;
  
  res = mHTMLEditor->mTypeInState->TakeRelativeFontSize(&relFontSize);
  if (NS_FAILED(res)) return res;
  res = mHTMLEditor->mTypeInState->TakeSetProperty(&item);
  if (NS_FAILED(res)) return res;
  
  if (item || relFontSize) // we have at least one style to add; make a
  {                        // new text node to insert style nodes above.
    if (mHTMLEditor->IsTextNode(node))
    {
      // if we are in a text node, split it
      res = mHTMLEditor->SplitNodeDeep(node, node, offset, &offset);
      if (NS_FAILED(res)) return res;
      node->GetParentNode(getter_AddRefs(tmp));
      node = tmp;
    }
    nsCOMPtr<nsIDOMNode> newNode;
    nsCOMPtr<nsIDOMText> nodeAsText;
    res = aDoc->CreateTextNode(nsAutoString(), getter_AddRefs(nodeAsText));
    if (NS_FAILED(res)) return res;
    if (!nodeAsText) return NS_ERROR_NULL_POINTER;
    newNode = do_QueryInterface(nodeAsText);
    res = mHTMLEditor->InsertNode(newNode, node, offset);
    if (NS_FAILED(res)) return res;
    node = newNode;
    offset = 0;
    weDidSometing = PR_TRUE;

    if (relFontSize)
    {
      PRInt32 j, dir;
      // dir indicated bigger versus smaller.  1 = bigger, -1 = smaller
      if (relFontSize > 0) dir=1;
      else dir = -1;
      for (j=0; j<abs(relFontSize); j++)
      {
        res = mHTMLEditor->RelativeFontChangeOnTextNode(dir, nodeAsText, 0, -1);
        if (NS_FAILED(res)) return res;
      }
    }
    
    while (item)
    {
      res = mHTMLEditor->SetInlinePropertyOnNode(node, item->tag, &item->attr, &item->value);
      if (NS_FAILED(res)) return res;
      // we own item now (TakeSetProperty hands ownership to us)
      delete item;
      mHTMLEditor->mTypeInState->TakeSetProperty(&item);
    }
  }
  if (weDidSometing)
    return aSelection->Collapse(node, offset);
    
  return res;
}


///////////////////////////////////////////////////////////////////////////
// IsEmptyBlock: figure out if aNode is (or is inside) an empty block.
//               A block can have children and still be considered empty,
//               if the children are empty or non-editable.
//                  
nsresult 
nsHTMLEditRules::IsEmptyBlock(nsIDOMNode *aNode, 
                              PRBool *outIsEmptyBlock, 
                              PRBool aMozBRDoesntCount,
                              PRBool aListItemsNotEmpty) 
{
  if (!aNode || !outIsEmptyBlock) return NS_ERROR_NULL_POINTER;
  *outIsEmptyBlock = PR_TRUE;
  
//  nsresult res = NS_OK;
  nsCOMPtr<nsIDOMNode> nodeToTest;
  if (IsBlockNode(aNode)) nodeToTest = do_QueryInterface(aNode);
//  else nsCOMPtr<nsIDOMElement> block;
//  looks like I forgot to finish this.  Wonder what I was going to do?

  if (!nodeToTest) return NS_ERROR_NULL_POINTER;
  return mHTMLEditor->IsEmptyNode(nodeToTest, outIsEmptyBlock,
                     aMozBRDoesntCount, aListItemsNotEmpty);
}


nsresult
nsHTMLEditRules::WillAlign(nsISelection *aSelection, 
                           const nsAString *alignType, 
                           PRBool *aCancel,
                           PRBool *aHandled)
{
  if (!aSelection || !aCancel || !aHandled) { return NS_ERROR_NULL_POINTER; }

  nsresult res = WillInsert(aSelection, aCancel);
  if (NS_FAILED(res)) return res;

  // initialize out param
  // we want to ignore result of WillInsert()
  *aCancel = PR_FALSE;
  *aHandled = PR_FALSE;

  res = NormalizeSelection(aSelection);
  if (NS_FAILED(res)) return res;
  nsAutoSelectionReset selectionResetter(aSelection, mHTMLEditor);

  // convert the selection ranges into "promoted" selection ranges:
  // this basically just expands the range to include the immediate
  // block parent, and then further expands to include any ancestors
  // whose children are all in the range
  *aHandled = PR_TRUE;
  nsCOMPtr<nsISupportsArray> arrayOfNodes;
  res = GetNodesFromSelection(aSelection, kAlign, address_of(arrayOfNodes));
  if (NS_FAILED(res)) return res;

  // if we don't have any nodes, or we have only a single br, then we are
  // creating an empty alignment div.  We have to do some different things for these.
  PRBool emptyDiv = PR_FALSE;
  PRUint32 listCount;
  arrayOfNodes->Count(&listCount);
  if (!listCount) emptyDiv = PR_TRUE;
  if (listCount == 1)
  {
    nsCOMPtr<nsISupports> isupports = dont_AddRef(arrayOfNodes->ElementAt(0));
    nsCOMPtr<nsIDOMNode> theNode( do_QueryInterface(isupports ) );

    if (nsHTMLEditUtils::SupportsAlignAttr(theNode))
    {
      // the node is a table element, an horiz rule, a paragraph, a div
      // or a section header; in HTML 4, it can directly carry the ALIGN
      // attribute and we don't need to make a div! If we are in CSS mode,
      // all the work is done in AlignBlock
      nsCOMPtr<nsIDOMElement> theElem = do_QueryInterface(theNode);
      res = AlignBlock(theElem, alignType, PR_TRUE);
      if (NS_FAILED(res)) return res;
      return NS_OK;
    }

    if (nsTextEditUtils::IsBreak(theNode))
    {
      // The special case emptyDiv code (below) that consumes BRs can
      // cause tables to split if the start node of the selection is
      // not in a table cell or caption, for example parent is a <tr>.
      // Avoid this unnecessary splitting if possible by leaving emptyDiv
      // FALSE so that we fall through to the normal case alignment code.
      //
      // XXX: It seems a little error prone for the emptyDiv special
      //      case code to assume that the start node of the selection
      //      is the parent of the single node in the arrayOfNodes, as
      //      the paragraph above points out. Do we rely on the selection
      //      start node because of the fact that arrayOfNodes can be empty?
      //      We should probably revisit this issue. - kin

      nsCOMPtr<nsIDOMNode> parent;
      PRInt32 offset;
      res = mHTMLEditor->GetStartNodeAndOffset(aSelection, address_of(parent), &offset);

      if (!nsHTMLEditUtils::IsTableElement(parent) || nsHTMLEditUtils::IsTableCellOrCaption(parent))
        emptyDiv = PR_TRUE;
    }
  }
  if (emptyDiv)
  {
    PRInt32 offset;
    nsCOMPtr<nsIDOMNode> brNode, parent, theDiv, sib;
    NS_NAMED_LITERAL_STRING(divType, "div");
    res = mHTMLEditor->GetStartNodeAndOffset(aSelection, address_of(parent), &offset);
    if (NS_FAILED(res)) return res;
    res = SplitAsNeeded(&divType, address_of(parent), &offset);
    if (NS_FAILED(res)) return res;
    // consume a trailing br, if any.  This is to keep an alignment from
    // creating extra lines, if possible.
    res = mHTMLEditor->GetNextHTMLNode(parent, offset, address_of(brNode));
    if (NS_FAILED(res)) return res;
    if (brNode && nsTextEditUtils::IsBreak(brNode))
    {
      // making use of html structure... if next node after where
      // we are putting our div is not a block, then the br we 
      // found is in same block we are, so its safe to consume it.
      res = mHTMLEditor->GetNextHTMLSibling(parent, offset, address_of(sib));
      if (NS_FAILED(res)) return res;
      if (!IsBlockNode(sib))
      {
        res = mHTMLEditor->DeleteNode(brNode);
        if (NS_FAILED(res)) return res;
      }
    }
    res = mHTMLEditor->CreateNode(divType, parent, offset, getter_AddRefs(theDiv));
    if (NS_FAILED(res)) return res;
    // remember our new block for postprocessing
    mNewBlock = theDiv;
    // set up the alignment on the div, using HTML or CSS
    nsCOMPtr<nsIDOMElement> divElem = do_QueryInterface(theDiv);
    res = AlignBlock(divElem, alignType, PR_TRUE);
    if (NS_FAILED(res)) return res;
    *aHandled = PR_TRUE;
    // put in a moz-br so that it won't get deleted
    res = CreateMozBR(theDiv, 0, address_of(brNode));
    if (NS_FAILED(res)) return res;
    res = aSelection->Collapse(theDiv, 0);
    selectionResetter.Abort();  // dont reset our selection in this case.
    return res;
  }

  // Next we detect all the transitions in the array, where a transition
  // means that adjacent nodes in the array don't have the same parent.

  nsVoidArray transitionList;
  res = MakeTransitionList(arrayOfNodes, &transitionList);
  if (NS_FAILED(res)) return res;                                 

  // Ok, now go through all the nodes and give them an align attrib or put them in a div, 
  // or whatever is appropriate.  Wohoo!

  PRInt32 i;
  nsCOMPtr<nsIDOMNode> curParent;
  nsCOMPtr<nsIDOMNode> curDiv;
  PRBool useCSS;
  mHTMLEditor->GetIsCSSEnabled(&useCSS);
  for (i=0; i<(PRInt32)listCount; i++)
  {
    // here's where we actually figure out what to do
    nsCOMPtr<nsISupports> isupports = dont_AddRef(arrayOfNodes->ElementAt(i));
    nsCOMPtr<nsIDOMNode> curNode( do_QueryInterface(isupports ) );
    PRInt32 offset;
    res = nsEditor::GetNodeLocation(curNode, address_of(curParent), &offset);
    if (NS_FAILED(res)) return res;

    // the node is a table element, an horiz rule, a paragraph, a div
    // or a section header; in HTML 4, it can directly carry the ALIGN
    // attribute and we don't need to nest it, just set the alignment.
    // In CSS, assign the corresponding CSS styles in AlignBlock
    if (nsHTMLEditUtils::SupportsAlignAttr(curNode))
    {
      nsCOMPtr<nsIDOMElement> curElem = do_QueryInterface(curNode);
      res = AlignBlock(curElem, alignType, PR_FALSE);
      if (NS_FAILED(res)) return res;
      // clear out curDiv so that we don't put nodes after this one into it
      curDiv = 0;
      continue;
    }

    // Skip insignificant formatting text nodes to prevent
    // unnecessary structure splitting!
    if (nsEditor::IsTextNode(curNode) &&
       ((nsHTMLEditUtils::IsTableElement(curParent) && !nsHTMLEditUtils::IsTableCellOrCaption(curParent)) ||
        nsHTMLEditUtils::IsList(curParent)))
      continue;

    // if it's a list item, or a list
    // inside a list, forget any "current" div, and instead put divs inside
    // the appropriate block (td, li, etc)
    if ( nsHTMLEditUtils::IsListItem(curNode)
         || nsHTMLEditUtils::IsList(curNode))
    {
      res = RemoveAlignment(curNode, *alignType, PR_TRUE);
      if (NS_FAILED(res)) return res;
      if (useCSS) {
        nsCOMPtr<nsIDOMElement> curElem = do_QueryInterface(curNode);
        NS_NAMED_LITERAL_STRING(attrName, "align");
        PRInt32 count;
        mHTMLEditor->mHTMLCSSUtils->SetCSSEquivalentToHTMLStyle(curNode, nsnull,
                                                   &attrName, alignType, &count);
        curDiv = 0;
        continue;
      }
      else if (nsHTMLEditUtils::IsList(curParent)) {
        // if we don't use CSS, add a contraint to list element : they have
        // to be inside another list, ie >= second level of nesting
        res = AlignInnerBlocks(curNode, alignType);
        if (NS_FAILED(res)) return res;
        curDiv = 0;
        continue;
      }
      // clear out curDiv so that we don't put nodes after this one into it
    }      

    // need to make a div to put things in if we haven't already,
    // or if this node doesn't go in div we used earlier.
    if (!curDiv || transitionList[i])
    {
      NS_NAMED_LITERAL_STRING(divType, "div");
      res = SplitAsNeeded(&divType, address_of(curParent), &offset);
      if (NS_FAILED(res)) return res;
      res = mHTMLEditor->CreateNode(divType, curParent, offset, getter_AddRefs(curDiv));
      if (NS_FAILED(res)) return res;
      // remember our new block for postprocessing
      mNewBlock = curDiv;
      // set up the alignment on the div
      nsCOMPtr<nsIDOMElement> divElem = do_QueryInterface(curDiv);
      res = AlignBlock(divElem, alignType, PR_TRUE);
//      nsAutoString attr(NS_LITERAL_STRING("align"));
//      res = mHTMLEditor->SetAttribute(divElem, attr, *alignType);
//      if (NS_FAILED(res)) return res;
      // curDiv is now the correct thing to put curNode in
    }

    // tuck the node into the end of the active div
    res = mHTMLEditor->MoveNode(curNode, curDiv, -1);
    if (NS_FAILED(res)) return res;
  }

  return res;
}


///////////////////////////////////////////////////////////////////////////
// AlignInnerBlocks: align inside table cells or list items
//       
nsresult
nsHTMLEditRules::AlignInnerBlocks(nsIDOMNode *aNode, const nsAString *alignType)
{
  if (!aNode || !alignType) return NS_ERROR_NULL_POINTER;
  nsresult res;
  
  // gather list of table cells or list items
  nsCOMPtr<nsISupportsArray> arrayOfNodes;
  nsTableCellAndListItemFunctor functor;
  nsDOMIterator iter;
  res = iter.Init(aNode);
  if (NS_FAILED(res)) return res;
  res = iter.MakeList(functor, address_of(arrayOfNodes));
  if (NS_FAILED(res)) return res;
  
  // now that we have the list, align their contents as requested
  PRUint32 listCount;
  PRUint32 j;
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsISupports> isupports;

  arrayOfNodes->Count(&listCount);
  for (j = 0; j < listCount; j++)
  {
    isupports = dont_AddRef(arrayOfNodes->ElementAt(0));
    node = do_QueryInterface(isupports);
    res = AlignBlockContents(node, alignType);
    if (NS_FAILED(res)) return res;
    arrayOfNodes->RemoveElementAt(0);
  }

  return res;  
}


///////////////////////////////////////////////////////////////////////////
// AlignBlockContents: align contents of a block element
//                  
nsresult
nsHTMLEditRules::AlignBlockContents(nsIDOMNode *aNode, const nsAString *alignType)
{
  if (!aNode || !alignType) return NS_ERROR_NULL_POINTER;
  nsresult res;
  nsCOMPtr <nsIDOMNode> firstChild, lastChild, divNode;
  
  PRBool useCSS;
  mHTMLEditor->GetIsCSSEnabled(&useCSS);

  res = mHTMLEditor->GetFirstEditableChild(aNode, address_of(firstChild));
  if (NS_FAILED(res)) return res;
  res = mHTMLEditor->GetLastEditableChild(aNode, address_of(lastChild));
  if (NS_FAILED(res)) return res;
  NS_NAMED_LITERAL_STRING(attr, "align");
  if (!firstChild)
  {
    // this cell has no content, nothing to align
  }
  else if ((firstChild==lastChild) && nsHTMLEditUtils::IsDiv(firstChild))
  {
    // the cell already has a div containing all of it's content: just
    // act on this div.
    nsCOMPtr<nsIDOMElement> divElem = do_QueryInterface(firstChild);
    if (useCSS) {
      res = mHTMLEditor->SetAttributeOrEquivalent(divElem, attr, *alignType); 
    }
    else {
      res = mHTMLEditor->SetAttribute(divElem, attr, *alignType);
    }
    if (NS_FAILED(res)) return res;
  }
  else
  {
    // else we need to put in a div, set the alignment, and toss in all the children
    res = mHTMLEditor->CreateNode(NS_LITERAL_STRING("div"), aNode, 0, getter_AddRefs(divNode));
    if (NS_FAILED(res)) return res;
    // set up the alignment on the div
    nsCOMPtr<nsIDOMElement> divElem = do_QueryInterface(divNode);
    if (useCSS) {
      res = mHTMLEditor->SetAttributeOrEquivalent(divElem, attr, *alignType); 
    }
    else {
      res = mHTMLEditor->SetAttribute(divElem, attr, *alignType);
    }
    if (NS_FAILED(res)) return res;
    // tuck the children into the end of the active div
    while (lastChild && (lastChild != divNode))
    {
      res = mHTMLEditor->MoveNode(lastChild, divNode, 0);
      if (NS_FAILED(res)) return res;
      res = mHTMLEditor->GetLastEditableChild(aNode, address_of(lastChild));
      if (NS_FAILED(res)) return res;
    }
  }
  return res;
}

///////////////////////////////////////////////////////////////////////////
// CheckForEmptyBlock: Called by WillDeleteSelection to detect and handle
//                     case of deleting from inside an empty block.
//                  
nsresult
nsHTMLEditRules::CheckForEmptyBlock(nsIDOMNode *aStartNode, 
                                    nsIDOMNode *aBodyNode,
                                    nsISelection *aSelection,
                                    PRBool *aHandled)
{
  // if we are inside an empty block, delete it.
  // Note: do NOT delete table elements this way.
  nsresult res = NS_OK;
  nsCOMPtr<nsIDOMNode> block;
  if (IsBlockNode(aStartNode)) 
    block = aStartNode;
  else
    block = mHTMLEditor->GetBlockNodeParent(aStartNode);
  PRBool bIsEmptyNode;
  if (block != aBodyNode)
  {
    res = mHTMLEditor->IsEmptyNode(block, &bIsEmptyNode, PR_TRUE, PR_FALSE);
    if (NS_FAILED(res)) return res;
    if (bIsEmptyNode && !nsHTMLEditUtils::IsTableElement(aStartNode))
    {
      // adjust selection to be right after it
      nsCOMPtr<nsIDOMNode> blockParent;
      PRInt32 offset;
      res = nsEditor::GetNodeLocation(block, address_of(blockParent), &offset);
      if (NS_FAILED(res)) return res;
      if (!blockParent || offset < 0) return NS_ERROR_FAILURE;
      res = aSelection->Collapse(blockParent, offset+1);
      if (NS_FAILED(res)) return res;
      res = mHTMLEditor->DeleteNode(block);
      *aHandled = PR_TRUE;
    }
  }
  return res;
}

///////////////////////////////////////////////////////////////////////////
// CheckForWhitespaceDeletion: Called by WillDeleteSelection to detect and handle
//                     case of deleting whitespace.  If we need to skip over
//                     whitespace, visNode and visOffset are updated to new
//                     location to handle deletion.
//                  
nsresult
nsHTMLEditRules::CheckForWhitespaceDeletion(nsCOMPtr<nsIDOMNode> *ioStartNode,
                                            PRInt32 *ioStartOffset,
                                            PRInt32 aAction,
                                            PRBool *aHandled)
{
  nsresult res = NS_OK;
  // gather up ws data here.  We may be next to non-significant ws.
  nsWSRunObject wsObj(mHTMLEditor, *ioStartNode, *ioStartOffset);
  nsCOMPtr<nsIDOMNode> visNode;
  PRInt32 visOffset;
  PRInt16 wsType;
  if (aAction == nsIEditor::ePrevious)
  {
    res = wsObj.PriorVisibleNode(*ioStartNode, *ioStartOffset, address_of(visNode), &visOffset, &wsType);
    // note that visOffset is _after_ what we are about to delete.
  }
  else if (aAction == nsIEditor::eNext)
  {
    res = wsObj.NextVisibleNode(*ioStartNode, *ioStartOffset, address_of(visNode), &visOffset, &wsType);
    // note that visOffset is _before_ what we are about to delete.
  }
  if (NS_SUCCEEDED(res))
  {
    if (wsType==nsWSRunObject::eNormalWS)
    {
      // we found some visible ws to delete.  Let ws code handle it.
      if (aAction == nsIEditor::ePrevious)
        res = wsObj.DeleteWSBackward();
      else if (aAction == nsIEditor::eNext)
        res = wsObj.DeleteWSForward();
      *aHandled = PR_TRUE;
      return res;
    } 
    else if (visNode)
    {
      // reposition startNode and startOffset so that we skip over any non-significant ws
      *ioStartNode = visNode;
      *ioStartOffset = visOffset;
    }
  }
  return res;
}

nsresult
nsHTMLEditRules::CheckForInvisibleBR(nsIDOMNode *aBlock, 
                                     BRLocation aWhere, 
                                     nsCOMPtr<nsIDOMNode> *outBRNode,
                                     PRInt32 aOffset)
{
  // for now I'm relying on fact that user has scrubbed any invisible whitespace
  // inthe vicinity, so we don't need morecomplicated code here to check for that.
  if (!aBlock || !outBRNode) return NS_ERROR_NULL_POINTER;
  *outBRNode = nsnull;
  if (aWhere == kBlockEnd)
  {
    nsCOMPtr<nsIDOMNode> node = mHTMLEditor->GetRightmostChild(aBlock, PR_TRUE);
    if (nsTextEditUtils::IsBreak(node))
      *outBRNode = node;
  }
  else if (aOffset)
  {
    nsCOMPtr<nsIDOMNode> prevItem;
    mHTMLEditor->GetPriorHTMLNode(aBlock, aOffset, address_of(prevItem), PR_TRUE);
    if (nsTextEditUtils::IsBreak(prevItem))
      *outBRNode = prevItem;
  }
  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////
// GetInnerContent: aList and aTbl allow the caller to specify what kind 
//                  of content to "look inside".  If aTbl is true, look inside
//                  any table content, and insert the inner content into the
//                  supplied issupportsarray at offset aIndex.  
//                  Similarly with aList and list content.
//                  aIndex is updated to point past inserted elements.
//                  
nsresult
nsHTMLEditRules::GetInnerContent(nsIDOMNode *aNode, nsISupportsArray *outArrayOfNodes, 
                                 PRInt32 *aIndex, PRBool aList, PRBool aTbl)
{
  if (!aNode || !outArrayOfNodes || !aIndex) return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsISupports> isupports;
  
  nsresult res = mHTMLEditor->GetFirstEditableChild(aNode, address_of(node));
  while (NS_SUCCEEDED(res) && node)
  {
    if (  ( aList && (nsHTMLEditUtils::IsList(node)     || 
                      nsHTMLEditUtils::IsListItem(node) ) )
       || ( aTbl && nsHTMLEditUtils::IsTableElement(node) )  )
    {
      res = GetInnerContent(node, outArrayOfNodes, aIndex, aList, aTbl);
      if (NS_FAILED(res)) return res;
    }
    else
    {
      isupports = do_QueryInterface(node);
      outArrayOfNodes->InsertElementAt(isupports, *aIndex);
      (*aIndex)++;
    }
    nsCOMPtr<nsIDOMNode> tmp;
    res = node->GetNextSibling(getter_AddRefs(tmp));
    node = tmp;
  }

  return res;
}

///////////////////////////////////////////////////////////////////////////
// IsFirstNode: Are we the first editable node in our parent?
//                  
PRBool
nsHTMLEditRules::IsFirstNode(nsIDOMNode *aNode)
{
  nsCOMPtr<nsIDOMNode> parent;
  PRInt32 offset, j=0;
  nsresult res = nsEditor::GetNodeLocation(aNode, address_of(parent), &offset);
  if (NS_FAILED(res)) 
  {
    NS_NOTREACHED("failure in nsHTMLEditRules::IsFirstNode");
    return PR_FALSE;
  }
  if (!offset)  // easy case, we are first dom child
    return PR_TRUE;
  if (!parent)  
    return PR_TRUE;
  
  // ok, so there are earlier children.  But are they editable???
  nsCOMPtr<nsIDOMNodeList> childList;
  nsCOMPtr<nsIDOMNode> child;

  res = parent->GetChildNodes(getter_AddRefs(childList));
  if (NS_FAILED(res) || !childList) 
  {
    NS_NOTREACHED("failure in nsHTMLEditUtils::IsFirstNode");
    return PR_TRUE;
  }
  while (j < offset)
  {
    childList->Item(j, getter_AddRefs(child));
    if (mHTMLEditor->IsEditable(child)) 
      return PR_FALSE;
    j++;
  }
  return PR_TRUE;
}


///////////////////////////////////////////////////////////////////////////
// IsLastNode: Are we the last editable node in our parent?
//                  
PRBool
nsHTMLEditRules::IsLastNode(nsIDOMNode *aNode)
{
  nsCOMPtr<nsIDOMNode> parent;
  PRInt32 offset, j;
  PRUint32 numChildren;
  nsresult res = nsEditor::GetNodeLocation(aNode, address_of(parent), &offset);
  if (NS_FAILED(res)) 
  {
    NS_NOTREACHED("failure in nsHTMLEditUtils::IsLastNode");
    return PR_FALSE;
  }
  nsEditor::GetLengthOfDOMNode(parent, numChildren); 
  if (offset+1 == (PRInt32)numChildren) // easy case, we are last dom child
    return PR_TRUE;
  if (!parent)
    return PR_TRUE;
    
  // ok, so there are later children.  But are they editable???
  j = offset+1;
  nsCOMPtr<nsIDOMNodeList>childList;
  nsCOMPtr<nsIDOMNode> child;
  res = parent->GetChildNodes(getter_AddRefs(childList));
  if (NS_FAILED(res) || !childList) 
  {
    NS_NOTREACHED("failure in nsHTMLEditRules::IsLastNode");
    return PR_TRUE;
  }
  while (j < (PRInt32)numChildren)
  {
    childList->Item(j, getter_AddRefs(child));
    if (mHTMLEditor->IsEditable(child)) 
      return PR_FALSE;
    j++;
  }
  return PR_TRUE;
}


#ifdef XXX_DEAD_CODE
///////////////////////////////////////////////////////////////////////////
// AtStartOfBlock: is node/offset at the start of the editable material in this block?
//                  
PRBool
nsHTMLEditRules::AtStartOfBlock(nsIDOMNode *aNode, PRInt32 aOffset, nsIDOMNode *aBlock)
{
  nsCOMPtr<nsIDOMCharacterData> nodeAsText = do_QueryInterface(aNode);
  if (nodeAsText && aOffset) return PR_FALSE;  // there are chars in front of us
  
  nsCOMPtr<nsIDOMNode> priorNode;
  nsresult  res = mHTMLEditor->GetPriorHTMLNode(aNode, aOffset, address_of(priorNode));
  if (NS_FAILED(res)) return PR_TRUE;
  if (!priorNode) return PR_TRUE;
  nsCOMPtr<nsIDOMNode> blockParent = mHTMLEditor->GetBlockNodeParent(priorNode);
  if (blockParent && (blockParent.get() == aBlock)) return PR_FALSE;
  return PR_TRUE;
}


///////////////////////////////////////////////////////////////////////////
// AtEndOfBlock: is node/offset at the end of the editable material in this block?
//                  
PRBool
nsHTMLEditRules::AtEndOfBlock(nsIDOMNode *aNode, PRInt32 aOffset, nsIDOMNode *aBlock)
{
  nsCOMPtr<nsIDOMCharacterData> nodeAsText = do_QueryInterface(aNode);
  if (nodeAsText)   
  {
    PRUint32 strLength;
    nodeAsText->GetLength(&strLength);
    if ((PRInt32)strLength > aOffset) return PR_FALSE;  // there are chars in after us
  }
  nsCOMPtr<nsIDOMNode> nextNode;
  nsresult  res = mHTMLEditor->GetNextHTMLNode(aNode, aOffset, address_of(nextNode));
  if (NS_FAILED(res)) return PR_TRUE;
  if (!nextNode) return PR_TRUE;
  nsCOMPtr<nsIDOMNode> blockParent = mHTMLEditor->GetBlockNodeParent(nextNode);
  if (blockParent && (blockParent.get() == aBlock)) return PR_FALSE;
  return PR_TRUE;
}


///////////////////////////////////////////////////////////////////////////
// CreateMozDiv: makes a div with type = _moz
//                       
nsresult
nsHTMLEditRules::CreateMozDiv(nsIDOMNode *inParent, PRInt32 inOffset, nsCOMPtr<nsIDOMNode> *outDiv)
{
  if (!inParent || !outDiv) return NS_ERROR_NULL_POINTER;
  nsAutoString divType= "div";
  *outDiv = nsnull;
  nsresult res = mHTMLEditor->CreateNode(divType, inParent, inOffset, getter_AddRefs(*outDiv));
  if (NS_FAILED(res)) return res;
  // give it special moz attr
  nsCOMPtr<nsIDOMElement> mozDivElem = do_QueryInterface(*outDiv);
  res = mHTMLEditor->SetAttribute(mozDivElem, "type", "_moz");
  if (NS_FAILED(res)) return res;
  res = AddTrailerBR(*outDiv);
  return res;
}
#endif    


///////////////////////////////////////////////////////////////////////////
// NormalizeSelection:  tweak non-collapsed selections to be more "natural".
//    Idea here is to adjust selection endpoint so that they do not cross
//    breaks or block boundaries unless something editable beyond that boundary
//    is also selected.  This adjustment makes it much easier for the various
//    block operations to determine what nodes to act on.
//                       
nsresult 
nsHTMLEditRules::NormalizeSelection(nsISelection *inSelection)
{
  if (!inSelection) return NS_ERROR_NULL_POINTER;

  // don't need to touch collapsed selections
  PRBool bCollapsed;
  nsresult res = inSelection->GetIsCollapsed(&bCollapsed);
  if (NS_FAILED(res)) return res;
  if (bCollapsed) return res;

  PRInt32 rangeCount;
  res = inSelection->GetRangeCount(&rangeCount);
  if (NS_FAILED(res)) return res;
  
  // we don't need to mess with cell selections, and we assume multirange selections are those.
  if (rangeCount != 1) return NS_OK;
  
  nsCOMPtr<nsIDOMRange> range;
  res = inSelection->GetRangeAt(0, getter_AddRefs(range));
  if (NS_FAILED(res)) return res;
  if (!range) return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsIDOMNode> startNode, endNode;
  PRInt32 startOffset, endOffset;
  nsCOMPtr<nsIDOMNode> newStartNode, newEndNode;
  PRInt32 newStartOffset, newEndOffset;
  
  res = range->GetStartContainer(getter_AddRefs(startNode));
  if (NS_FAILED(res)) return res;
  res = range->GetStartOffset(&startOffset);
  if (NS_FAILED(res)) return res;
  res = range->GetEndContainer(getter_AddRefs(endNode));
  if (NS_FAILED(res)) return res;
  res = range->GetEndOffset(&endOffset);
  if (NS_FAILED(res)) return res;
  
  // adjusted values default to original values
  newStartNode = startNode; 
  newStartOffset = startOffset;
  newEndNode = endNode; 
  newEndOffset = endOffset;
  
  // some locals we need for whitespace code
  nsCOMPtr<nsIDOMNode> someNode;
  PRInt32 offset;
  PRInt16 wsType;

  // let the whitespace code do the heavy lifting
  nsWSRunObject wsEndObj(mHTMLEditor, endNode, endOffset);
  // is there any intervening visible whitespace?  if so we can't push selection past that,
  // it would visibly change maening of users selection
  res = wsEndObj.PriorVisibleNode(endNode, endOffset, address_of(someNode), &offset, &wsType);
  if (NS_FAILED(res)) return res;
  if ((wsType != nsWSRunObject::eText) && (wsType != nsWSRunObject::eNormalWS))
  {
    // eThisBlock and eOtherBlock conveniently distinquish cases
    // of going "down" into a block and "up" out of a block.
    if (wsEndObj.mStartReason == nsWSRunObject::eOtherBlock) 
    {
      // endpoint is just after the close of a block.
      nsCOMPtr<nsIDOMNode> child = mHTMLEditor->GetRightmostChild(wsEndObj.mStartReasonNode, PR_TRUE);
      if (child)
      {
        res = nsEditor::GetNodeLocation(child, address_of(newEndNode), &newEndOffset);
        if (NS_FAILED(res)) return res;
        ++newEndOffset; // offset *after* child
      }
      // else block is empty - we can leave selection alone here, i think.
    }
    else if (wsEndObj.mStartReason == nsWSRunObject::eThisBlock)
    {
      // endpoint is just after start of this block
      nsCOMPtr<nsIDOMNode> child;
      res = mHTMLEditor->GetPriorHTMLNode(endNode, endOffset, address_of(child));
      if (child)
      {
        res = nsEditor::GetNodeLocation(child, address_of(newEndNode), &newEndOffset);
        if (NS_FAILED(res)) return res;
        ++newEndOffset; // offset *after* child
      }
      // else block is empty - we can leave selection alone here, i think.
    }
    else if (wsEndObj.mStartReason == nsWSRunObject::eBreak)
    {                                       
      // endpoint is just after break.  lets adjust it to before it.
      res = nsEditor::GetNodeLocation(wsEndObj.mStartReasonNode, address_of(newEndNode), &newEndOffset);
      if (NS_FAILED(res)) return res;
    }
  }
  
  
  // similar dealio for start of range
  nsWSRunObject wsStartObj(mHTMLEditor, startNode, startOffset);
  // is there any intervening visible whitespace?  if so we can't push selection past that,
  // it would visibly change maening of users selection
  res = wsStartObj.NextVisibleNode(startNode, startOffset, address_of(someNode), &offset, &wsType);
  if (NS_FAILED(res)) return res;
  if ((wsType != nsWSRunObject::eText) && (wsType != nsWSRunObject::eNormalWS))
  {
    // eThisBlock and eOtherBlock conveniently distinquish cases
    // of going "down" into a block and "up" out of a block.
    if (wsStartObj.mEndReason == nsWSRunObject::eOtherBlock) 
    {
      // startpoint is just before the start of a block.
      nsCOMPtr<nsIDOMNode> child = mHTMLEditor->GetLeftmostChild(wsStartObj.mEndReasonNode, PR_TRUE);
      if (child)
      {
        res = nsEditor::GetNodeLocation(child, address_of(newStartNode), &newStartOffset);
        if (NS_FAILED(res)) return res;
      }
      // else block is empty - we can leave selection alone here, i think.
    }
    else if (wsStartObj.mEndReason == nsWSRunObject::eThisBlock)
    {
      // startpoint is just before end of this block
      nsCOMPtr<nsIDOMNode> child;
      res = mHTMLEditor->GetNextHTMLNode(startNode, startOffset, address_of(child));
      if (child)
      {
        res = nsEditor::GetNodeLocation(child, address_of(newStartNode), &newStartOffset);
        if (NS_FAILED(res)) return res;
      }
      // else block is empty - we can leave selection alone here, i think.
    }
    else if (wsStartObj.mEndReason == nsWSRunObject::eBreak)
    {                                       
      // startpoint is just before a break.  lets adjust it to after it.
      res = nsEditor::GetNodeLocation(wsStartObj.mEndReasonNode, address_of(newStartNode), &newStartOffset);
      if (NS_FAILED(res)) return res;
      ++newStartOffset; // offset *after* break
    }
  }
  
  // there is a demented possiblity we have to check for.  We might have a very strange selection
  // that is not collapsed and yet does not contain any editable content, and satisfies some of the
  // above conditions that cause tweaking.  In this case we don't want to tweak the selection into
  // a block it was never in, etc.  There are a variety of strategies one might use to try to
  // detect these cases, but I think the most straightforward is to see if the adjusted locations
  // "cross" the old values: ie, new end before old start, or new start after old end.  If so 
  // then just leave things alone.
  
  PRInt16 comp;
  comp = mHTMLEditor->mRangeHelper->ComparePoints(startNode, startOffset, newEndNode, newEndOffset);
  if (comp == 1) return NS_OK;  // new end before old start
  comp = mHTMLEditor->mRangeHelper->ComparePoints(newStartNode, newStartOffset, endNode, endOffset);
  if (comp == 1) return NS_OK;  // new start after old end
  
  // otherwise set selection to new values.  
  inSelection->Collapse(newStartNode, newStartOffset);
  inSelection->Extend(newEndNode, newEndOffset);
  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////
// GetPromotedPoint: figure out where a start or end point for a block
//                   operation really is
nsresult
nsHTMLEditRules::GetPromotedPoint(RulesEndpoint aWhere, nsIDOMNode *aNode, PRInt32 aOffset, 
                                  PRInt32 actionID, nsCOMPtr<nsIDOMNode> *outNode, PRInt32 *outOffset)
{
  nsresult res = NS_OK;
  nsCOMPtr<nsIDOMNode> node = aNode;
  nsCOMPtr<nsIDOMNode> parent = aNode;
  PRInt32 offset = aOffset;
  
  // defualt values
  *outNode = node;
  *outOffset = offset;

  // we do one thing for InsertText actions, something else entirely for other actions
  if (actionID == kInsertText)
  {
    PRBool isSpace, isNBSP; 
    nsCOMPtr<nsIDOMNode> temp;   
    // for insert text or delete actions, we want to look backwards (or forwards, as appropriate)
    // for additional whitespace or nbsp's.  We may have to act on these later even though
    // they are outside of the initial selection.  Even if they are in another node!
    if (aWhere == kStart)
    {
      do
      {
        res = mHTMLEditor->IsPrevCharWhitespace(node, offset, &isSpace, &isNBSP, address_of(temp), &offset);
        if (NS_FAILED(res)) return res;
        if (isSpace || isNBSP) node = temp;
        else break;
      } while (node);
  
      *outNode = node;
      *outOffset = offset;
    }
    else if (aWhere == kEnd)
    {
      do
      {
        res = mHTMLEditor->IsNextCharWhitespace(node, offset, &isSpace, &isNBSP, address_of(temp), &offset);
        if (NS_FAILED(res)) return res;
        if (isSpace || isNBSP) node = temp;
        else break;
      } while (node);
  
      *outNode = node;
      *outOffset = offset;
    }
    return res;
  }
  
  // else not kInsertText.  In this case we want to see if we should
  // grab any adjacent inline nodes and/or parents and other ancestors
  if (aWhere == kStart)
  {
    // some special casing for text nodes
    if (nsEditor::IsTextNode(aNode))  
    {
      res = nsEditor::GetNodeLocation(aNode, address_of(parent), &offset);
      if (NS_FAILED(res)) return res;
    }
    else
    {
      node = nsEditor::GetChildAt(parent,offset);
    }
    if (!node) node = parent;
    
    
    // if this is an inline node,
    // back up through any prior inline nodes that
    // aren't across a <br> from us.
    
    if (!IsBlockNode(node))
    {
      nsCOMPtr<nsIDOMNode> block = nsHTMLEditor::GetBlockNodeParent(node);
      nsCOMPtr<nsIDOMNode> prevNode, prevNodeBlock;
      res = mHTMLEditor->GetPriorHTMLNode(node, address_of(prevNode));
      
      while (prevNode && NS_SUCCEEDED(res))
      {
        prevNodeBlock = nsHTMLEditor::GetBlockNodeParent(prevNode);
        if (prevNodeBlock != block)
          break;
        if (nsTextEditUtils::IsBreak(prevNode))
          break;
        if (IsBlockNode(prevNode))
          break;
        node = prevNode;
        res = mHTMLEditor->GetPriorHTMLNode(node, address_of(prevNode));
      }
    }
    
    // finding the real start for this point.  look up the tree for as long as we are the 
    // first node in the container, and as long as we haven't hit the body node.
    if (!nsTextEditUtils::IsBody(node))
    {
      res = nsEditor::GetNodeLocation(node, address_of(parent), &offset);
      if (NS_FAILED(res)) return res;
      while ((IsFirstNode(node)) && (!nsTextEditUtils::IsBody(parent)))
      {
        // some cutoffs are here: we don't need to also include them in the aWhere == kEnd case.
        // as long as they are in one or the other it will work.
        
        // dont cross table cell boundaries
        // if (nsHTMLEditUtils::IsTableCell(parent)) break;
        // special case for outdent: don't keep looking up 
        // if we have found a blockquote element to act on
        if ((actionID == kOutdent) && nsHTMLEditUtils::IsBlockquote(parent))
          break;
        node = parent;
        res = nsEditor::GetNodeLocation(node, address_of(parent), &offset);
        if (NS_FAILED(res)) return res;
      } 
      *outNode = parent;
      *outOffset = offset;
      return res;
    }
  }
  
  if (aWhere == kEnd)
  {
    // some special casing for text nodes
    if (nsEditor::IsTextNode(aNode))  
    {
      res = nsEditor::GetNodeLocation(aNode, address_of(parent), &offset);
      if (NS_FAILED(res)) return res;
    }
    else
    {
      if (offset) offset--; // we want node _before_ offset
      else
      {  // XXX HACK MOOSE
        // The logic in this routine, while it works almost all the time, is fundamentally flawed.
        // I need to rewrite it using something like nsWSRunObject::GetPreviousWSNode().  Til then 
        // I need this hack to correct the most commmon error here.
        
        // we couldn't get a prior node in this parent.  If next node is a br, just skip
        // the promotion altogether.  We have a br after us.  If we fell through to code below,
        // it would think br was before us and would proceed merrily along.
        node = nsEditor::GetChildAt(parent,offset);
        if (node && nsTextEditUtils::IsBreak(node))
          return NS_OK;  // default values used.
      }
      node = nsEditor::GetChildAt(parent,offset);
    }
    if (!node) node = parent;
    
    // if this is an inline node, 
    // look ahead through any further inline nodes that
    // aren't across a <br> from us, and that are enclosed in the same block.
    
    if (!IsBlockNode(node))
    {
      nsCOMPtr<nsIDOMNode> block = nsHTMLEditor::GetBlockNodeParent(node);
      nsCOMPtr<nsIDOMNode> nextNode, nextNodeBlock;
      res = mHTMLEditor->GetNextHTMLNode(node, address_of(nextNode));
      
      while (nextNode && NS_SUCCEEDED(res))
      {
        nextNodeBlock = nsHTMLEditor::GetBlockNodeParent(nextNode);
        if (nextNodeBlock != block)
          break;
        if (nsTextEditUtils::IsBreak(nextNode))
        {
          node = nextNode;
          break;
        }
        if (IsBlockNode(nextNode))
          break;
        node = nextNode;
        res = mHTMLEditor->GetNextHTMLNode(node, address_of(nextNode));
      }
    }
    
    // finding the real end for this point.  look up the tree for as long as we are the 
    // last node in the container, and as long as we haven't hit the body node.
    if (!nsTextEditUtils::IsBody(node))
    {
      res = nsEditor::GetNodeLocation(node, address_of(parent), &offset);
      if (NS_FAILED(res)) return res;
      while ((IsLastNode(node)) && (!nsTextEditUtils::IsBody(parent)))
      {
        node = parent;
        res = nsEditor::GetNodeLocation(node, address_of(parent), &offset);
        if (NS_FAILED(res)) return res;
      } 
      *outNode = parent;
      offset++;  // add one since this in an endpoint - want to be AFTER node.
      *outOffset = offset;
      return res;
    }
  }
  
  return res;
}


///////////////////////////////////////////////////////////////////////////
// GetPromotedRanges: run all the selection range endpoint through 
//                    GetPromotedPoint()
//                       
nsresult 
nsHTMLEditRules::GetPromotedRanges(nsISelection *inSelection, 
                                   nsCOMPtr<nsISupportsArray> *outArrayOfRanges, 
                                   PRInt32 inOperationType)
{
  if (!inSelection || !outArrayOfRanges) return NS_ERROR_NULL_POINTER;

  nsresult res = NS_NewISupportsArray(getter_AddRefs(*outArrayOfRanges));
  if (NS_FAILED(res)) return res;
  
  PRInt32 rangeCount;
  res = inSelection->GetRangeCount(&rangeCount);
  if (NS_FAILED(res)) return res;
  
  PRInt32 i;
  nsCOMPtr<nsIDOMRange> selectionRange;
  nsCOMPtr<nsIDOMRange> opRange;

  for (i = 0; i < rangeCount; i++)
  {
    res = inSelection->GetRangeAt(i, getter_AddRefs(selectionRange));
    if (NS_FAILED(res)) return res;

    // clone range so we dont muck with actual selection ranges
    res = selectionRange->CloneRange(getter_AddRefs(opRange));
    if (NS_FAILED(res)) return res;

    // make a new adjusted range to represent the appropriate block content.
    // The basic idea is to push out the range endpoints
    // to truly enclose the blocks that we will affect.
    // This call alters opRange.
    res = PromoteRange(opRange, inOperationType);
    if (NS_FAILED(res)) return res;
      
    // stuff new opRange into nsISupportsArray
    nsCOMPtr<nsISupports> isupports = do_QueryInterface(opRange);
    (*outArrayOfRanges)->AppendElement(isupports);
  }
  return res;
}


///////////////////////////////////////////////////////////////////////////
// PromoteRange: expand a range to include any parents for which all
//               editable children are already in range. 
//                       
nsresult 
nsHTMLEditRules::PromoteRange(nsIDOMRange *inRange, 
                              PRInt32 inOperationType)
{
  if (!inRange) return NS_ERROR_NULL_POINTER;
  nsresult res;
  nsCOMPtr<nsIDOMNode> startNode, endNode;
  PRInt32 startOffset, endOffset;
  
  res = inRange->GetStartContainer(getter_AddRefs(startNode));
  if (NS_FAILED(res)) return res;
  res = inRange->GetStartOffset(&startOffset);
  if (NS_FAILED(res)) return res;
  res = inRange->GetEndContainer(getter_AddRefs(endNode));
  if (NS_FAILED(res)) return res;
  res = inRange->GetEndOffset(&endOffset);
  if (NS_FAILED(res)) return res;
  
  // MOOSE major hack:
  // GetPromotedPoint doesn't really do the right thing for collapsed ranges
  // inside block elements that contain nothing but a solo <br>.  It's easier
  // to put a workaround here than to revamp GetPromotedPoint.  :-(
  if ( (startNode == endNode) && (startOffset == endOffset))
  {
    nsCOMPtr<nsIDOMNode> block;
    if (IsBlockNode(startNode)) 
      block = startNode;
    else
      block = mHTMLEditor->GetBlockNodeParent(startNode);
    if (block)
    {
      PRBool bIsEmptyNode = PR_FALSE;
      // check for body
      nsCOMPtr<nsIDOMElement> bodyElement;
      nsCOMPtr<nsIDOMNode> bodyNode;
      res = mHTMLEditor->GetRootElement(getter_AddRefs(bodyElement));
      if (NS_FAILED(res)) return res;
      if (!bodyElement) return NS_ERROR_UNEXPECTED;
      bodyNode = do_QueryInterface(bodyElement);
      if (block != bodyNode)
      {
        // ok, not body, check if empty
        res = mHTMLEditor->IsEmptyNode(block, &bIsEmptyNode, PR_TRUE, PR_FALSE);
      }
      if (bIsEmptyNode)
      {
        PRUint32 numChildren;
        nsEditor::GetLengthOfDOMNode(block, numChildren); 
        startNode = block;
        endNode = block;
        startOffset = 0;
        endOffset = numChildren;
      }
    }
  }

  // make a new adjusted range to represent the appropriate block content.
  // this is tricky.  the basic idea is to push out the range endpoints
  // to truly enclose the blocks that we will affect
  
  nsCOMPtr<nsIDOMNode> opStartNode;
  nsCOMPtr<nsIDOMNode> opEndNode;
  PRInt32 opStartOffset, opEndOffset;
  nsCOMPtr<nsIDOMRange> opRange;
  
  res = GetPromotedPoint( kStart, startNode, startOffset, inOperationType, address_of(opStartNode), &opStartOffset);
  if (NS_FAILED(res)) return res;
  res = GetPromotedPoint( kEnd, endNode, endOffset, inOperationType, address_of(opEndNode), &opEndOffset);
  if (NS_FAILED(res)) return res;
  res = inRange->SetStart(opStartNode, opStartOffset);
  if (NS_FAILED(res)) return res;
  res = inRange->SetEnd(opEndNode, opEndOffset);
  return res;
} 

///////////////////////////////////////////////////////////////////////////
// GetNodesForOperation: run through the ranges in the array and construct 
//                       a new array of nodes to be acted on.
//                       
nsresult 
nsHTMLEditRules::GetNodesForOperation(nsISupportsArray *inArrayOfRanges, 
                                   nsCOMPtr<nsISupportsArray> *outArrayOfNodes, 
                                   PRInt32 inOperationType,
                                   PRBool aDontTouchContent)
{
  if (!inArrayOfRanges || !outArrayOfNodes) return NS_ERROR_NULL_POINTER;

  // make a array
  nsresult res = NS_NewISupportsArray(getter_AddRefs(*outArrayOfNodes));
  if (NS_FAILED(res)) return res;
  
  PRUint32 rangeCount;
  res = inArrayOfRanges->Count(&rangeCount);
  if (NS_FAILED(res)) return res;
  
  PRInt32 i;
  nsCOMPtr<nsIDOMRange> opRange;
  nsCOMPtr<nsISupports> isupports;

  PRBool useCSS;
  mHTMLEditor->GetIsCSSEnabled(&useCSS);

  // bust up any inlines that cross our range endpoints,
  // but only if we are allowed to touch content.
  
  if (!aDontTouchContent)
  {
    nsVoidArray rangeItemArray;
    // first register ranges for special editor gravity
    for (i = 0; i < (PRInt32)rangeCount; i++)
    {
      isupports = dont_AddRef(inArrayOfRanges->ElementAt(0));
      opRange = do_QueryInterface(isupports);
      nsRangeStore *item = new nsRangeStore();
      if (!item) return NS_ERROR_NULL_POINTER;
      item->StoreRange(opRange);
      mHTMLEditor->mRangeUpdater.RegisterRangeItem(item);
      rangeItemArray.AppendElement((void*)item);
      inArrayOfRanges->RemoveElementAt(0);
    }    
    // now bust up inlines
    for (i = (PRInt32)rangeCount-1; i >= 0; i--)
    {
      nsRangeStore *item = (nsRangeStore*)rangeItemArray.ElementAt(i);
      res = BustUpInlinesAtRangeEndpoints(*item);
      if (NS_FAILED(res)) return res;    
    } 
    // then unregister the ranges
    for (i = 0; i < (PRInt32)rangeCount; i++)
    {
      nsRangeStore *item = (nsRangeStore*)rangeItemArray.ElementAt(0);
      if (!item) return NS_ERROR_NULL_POINTER;
      rangeItemArray.RemoveElementAt(0);
      mHTMLEditor->mRangeUpdater.DropRangeItem(item);
      res = item->GetRange(address_of(opRange));
      if (NS_FAILED(res)) return res;
      delete item;
      isupports = do_QueryInterface(opRange);
      inArrayOfRanges->AppendElement(isupports);
    }    
  }
  // gather up a list of all the nodes
  for (i = 0; i < (PRInt32)rangeCount; i++)
  {
    isupports = dont_AddRef(inArrayOfRanges->ElementAt(i));
    opRange = do_QueryInterface(isupports);
    
    nsTrivialFunctor functor;
    nsDOMSubtreeIterator iter;
    res = iter.Init(opRange);
    if (NS_FAILED(res)) return res;
    res = iter.AppendList(functor, *outArrayOfNodes);
    if (NS_FAILED(res)) return res;    
  }    

  // certain operations should not act on li's and td's, but rather inside 
  // them.  alter the list as needed
  if (inOperationType == kMakeBasicBlock)
  {
    PRUint32 listCount;
    (*outArrayOfNodes)->Count(&listCount);
    for (i=(PRInt32)listCount-1; i>=0; i--)
    {
      isupports = dont_AddRef((*outArrayOfNodes)->ElementAt(i));
      nsCOMPtr<nsIDOMNode> node( do_QueryInterface(isupports) );
      if (nsHTMLEditUtils::IsListItem(node))
      {
        PRInt32 j=i;
        (*outArrayOfNodes)->RemoveElementAt(i);
        res = GetInnerContent(node, *outArrayOfNodes, &j);
        if (NS_FAILED(res)) return res;
      }
    }
  }
  // indent/outdent already do something special for list items, but
  // we still need to make sure we dont act on table elements
  else if ( (inOperationType == kOutdent)  ||
            (inOperationType == kIndent) )
  {
    PRUint32 listCount;
    (*outArrayOfNodes)->Count(&listCount);
    for (i=(PRInt32)listCount-1; i>=0; i--)
    {
      isupports = dont_AddRef((*outArrayOfNodes)->ElementAt(i));
      nsCOMPtr<nsIDOMNode> node( do_QueryInterface(isupports) );
      if ( (nsHTMLEditUtils::IsTableElement(node) && !nsHTMLEditUtils::IsTable(node)) )
      {
        PRInt32 j=i;
        (*outArrayOfNodes)->RemoveElementAt(i);
        res = GetInnerContent(node, *outArrayOfNodes, &j);
        if (NS_FAILED(res)) return res;
      }
    }
  }
  // outdent should look inside of divs.
  if (inOperationType == kOutdent && !useCSS) 
  {
    PRUint32 listCount;
    (*outArrayOfNodes)->Count(&listCount);
    for (i=(PRInt32)listCount-1; i>=0; i--)
    {
      isupports = dont_AddRef((*outArrayOfNodes)->ElementAt(i));
      nsCOMPtr<nsIDOMNode> node( do_QueryInterface(isupports) );
      if (nsHTMLEditUtils::IsDiv(node))
      {
        PRInt32 j=i;
        (*outArrayOfNodes)->RemoveElementAt(i);
        res = GetInnerContent(node, *outArrayOfNodes, &j, PR_FALSE, PR_FALSE);
        if (NS_FAILED(res)) return res;
      }
    }
  }


  // post process the list to break up inline containers that contain br's.
  // but only for operations that might care, like making lists or para's...
  if ( (inOperationType == kMakeBasicBlock)  ||
       (inOperationType == kMakeList)        ||
       (inOperationType == kAlign)           ||
       (inOperationType == kIndent)          ||
       (inOperationType == kOutdent) )
  {
    PRUint32 listCount;
    (*outArrayOfNodes)->Count(&listCount);
    for (i=(PRInt32)listCount-1; i>=0; i--)
    {
      isupports = dont_AddRef((*outArrayOfNodes)->ElementAt(i));
      nsCOMPtr<nsIDOMNode> node( do_QueryInterface(isupports) );
      if (!aDontTouchContent && IsInlineNode(node) 
           && mHTMLEditor->IsContainer(node) && !mHTMLEditor->IsTextNode(node))
      {
        nsCOMPtr<nsISupportsArray> arrayOfInlines;
        res = BustUpInlinesAtBRs(node, address_of(arrayOfInlines));
        if (NS_FAILED(res)) return res;
        // put these nodes in outArrayOfNodes, replacing the current node
        (*outArrayOfNodes)->RemoveElementAt((PRUint32)i);
        // i sure wish nsISupportsArray had an IsEmpty() and DeleteLastElement()
        // calls.  For that matter, if I could just insert one nsISupportsArray
        // into another at a given position, that would do everything I need here.
        PRUint32 iCount;
        arrayOfInlines->Count(&iCount);
        while (iCount)
        {
          iCount--;
          isupports = dont_AddRef(arrayOfInlines->ElementAt(iCount));
          (*outArrayOfNodes)->InsertElementAt(isupports, i);
        }
      }
    }
  }
  return res;
}



///////////////////////////////////////////////////////////////////////////
// GetChildNodesForOperation: 
//                       
nsresult 
nsHTMLEditRules::GetChildNodesForOperation(nsIDOMNode *inNode, 
                                   nsCOMPtr<nsISupportsArray> *outArrayOfNodes)
{
  if (!inNode || !outArrayOfNodes) return NS_ERROR_NULL_POINTER;

  nsresult res = NS_NewISupportsArray(getter_AddRefs(*outArrayOfNodes));
  if (NS_FAILED(res)) return res;
  
  nsCOMPtr<nsIDOMNodeList> childNodes;
  res = inNode->GetChildNodes(getter_AddRefs(childNodes));
  if (NS_FAILED(res)) return res;
  if (!childNodes) return NS_ERROR_NULL_POINTER;
  PRUint32 childCount;
  res = childNodes->GetLength(&childCount);
  if (NS_FAILED(res)) return res;
  
  PRUint32 i;
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsISupports> isupports;
  for (i = 0; i < childCount; i++)
  {
    res = childNodes->Item( i, getter_AddRefs(node));
    if (!node) return NS_ERROR_FAILURE;
    isupports = do_QueryInterface(node);
    (*outArrayOfNodes)->AppendElement(isupports);
    if (NS_FAILED(res)) return res;
  }
  return res;
}



///////////////////////////////////////////////////////////////////////////
// GetListActionNodes: 
//                       
nsresult 
nsHTMLEditRules::GetListActionNodes(nsCOMPtr<nsISupportsArray> *outArrayOfNodes, 
                                    PRBool aEntireList,
                                    PRBool aDontTouchContent)
{
  if (!outArrayOfNodes) return NS_ERROR_NULL_POINTER;
  nsresult res = NS_OK;
  
  nsCOMPtr<nsISelection>selection;
  res = mHTMLEditor->GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  nsCOMPtr<nsISelectionPrivate> selPriv(do_QueryInterface(selection));
  if (!selPriv)
    return NS_ERROR_FAILURE;
  // added this in so that ui code can ask to change an entire list, even if selection
  // is only in part of it.  used by list item dialog.
  if (aEntireList)
  {       
    res = NS_NewISupportsArray(getter_AddRefs(*outArrayOfNodes));
    if (NS_FAILED(res)) return res;
    nsCOMPtr<nsIEnumerator> enumerator;
    res = selPriv->GetEnumerator(getter_AddRefs(enumerator));
    if (NS_FAILED(res)) return res;
    if (!enumerator) return NS_ERROR_UNEXPECTED;

    for (enumerator->First(); NS_OK!=enumerator->IsDone(); enumerator->Next())
    {
      nsCOMPtr<nsISupports> currentItem;
      res = enumerator->CurrentItem(getter_AddRefs(currentItem));
      if (NS_FAILED(res)) return res;
      if (!currentItem) return NS_ERROR_UNEXPECTED;

      nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
      nsCOMPtr<nsIDOMNode> commonParent, parent, tmp;
      range->GetCommonAncestorContainer(getter_AddRefs(commonParent));
      if (commonParent)
      {
        parent = commonParent;
        while (parent)
        {
          if (nsHTMLEditUtils::IsList(parent))
          {
            nsCOMPtr<nsISupports> isupports = do_QueryInterface(parent);
            (*outArrayOfNodes)->AppendElement(isupports);
            break;
          }
          parent->GetParentNode(getter_AddRefs(tmp));
          parent = tmp;
        }
      }
    }
    // if we didn't find any nodes this way, then try the normal way.  perhaps the
    // selection spans multiple lists but with no common list parent.
    if (*outArrayOfNodes)
    {
      PRUint32 nodeCount;
      (*outArrayOfNodes)->Count(&nodeCount);
      if (nodeCount) return NS_OK;
    }
  }
  
  // contruct a list of nodes to act on.
  res = GetNodesFromSelection(selection, kMakeList, outArrayOfNodes, aDontTouchContent);
  if (NS_FAILED(res)) return res;                                 
               
  // pre process our list of nodes...                      
  PRUint32 listCount;
  PRInt32 i;
  (*outArrayOfNodes)->Count(&listCount);
  for (i=(PRInt32)listCount-1; i>=0; i--)
  {
    nsCOMPtr<nsISupports> isupports = dont_AddRef((*outArrayOfNodes)->ElementAt(i));
    nsCOMPtr<nsIDOMNode> testNode( do_QueryInterface(isupports ) );

    // Remove all non-editable nodes.  Leave them be.
    if (!mHTMLEditor->IsEditable(testNode))
    {
      (*outArrayOfNodes)->RemoveElementAt(i);
    }
    
    // scan for table elements and divs.  If we find table elements other than table,
    // replace it with a list of any editable non-table content.
    if ( (nsHTMLEditUtils::IsTableElement(testNode) && !nsHTMLEditUtils::IsTable(testNode))
         || nsHTMLEditUtils::IsDiv(testNode) )
    {
      PRInt32 j=i;
      (*outArrayOfNodes)->RemoveElementAt(i);
      res = GetInnerContent(testNode, *outArrayOfNodes, &j, PR_FALSE);
      if (NS_FAILED(res)) return res;
    }
  }

  // if there is only one node in the array, and it is a list, div, or blockquote,
  // then look inside of it until we find inner list or content.
  res = LookInsideDivBQandList(*outArrayOfNodes);
  return res;
}


///////////////////////////////////////////////////////////////////////////
// LookInsideDivBQandList: 
//                       
nsresult 
nsHTMLEditRules::LookInsideDivBQandList(nsISupportsArray *aNodeArray)
{
  // if there is only one node in the array, and it is a list, div, or blockquote,
  // then look inside of it until we find inner list or content.
  nsresult res = NS_OK;
  PRUint32 listCount;
  aNodeArray->Count(&listCount);
  if (listCount == 1)
  {
    nsCOMPtr<nsISupports> isupports = dont_AddRef(aNodeArray->ElementAt(0));
    nsCOMPtr<nsIDOMNode> curNode( do_QueryInterface(isupports) );
    
    while (nsHTMLEditUtils::IsDiv(curNode)
           || nsHTMLEditUtils::IsList(curNode)
           || nsHTMLEditUtils::IsBlockquote(curNode))
    {
      // dive as long as there is only one child, and it is a list, div, blockquote
      PRUint32 numChildren;
      res = mHTMLEditor->CountEditableChildren(curNode, numChildren);
      if (NS_FAILED(res)) return res;
      
      if (numChildren == 1)
      {
        // keep diving
        nsCOMPtr <nsIDOMNode> tmpNode = nsEditor::GetChildAt(curNode, 0);
        if (nsHTMLEditUtils::IsDiv(tmpNode)
            || nsHTMLEditUtils::IsList(tmpNode)
            || nsHTMLEditUtils::IsBlockquote(tmpNode))
        {
          // check editablility XXX floppy moose
          curNode = tmpNode;
        }
        else break;
      }
      else break;
    }
    // we've found innermost list/blockquote/div: 
    // replace the one node in the array with these nodes
    aNodeArray->RemoveElementAt(0);
    if ((nsHTMLEditUtils::IsDiv(curNode) || nsHTMLEditUtils::IsBlockquote(curNode)))
    {
      PRInt32 j=0;
      res = GetInnerContent(curNode, aNodeArray, &j, PR_FALSE, PR_FALSE);
    }
    else
    {
      nsCOMPtr<nsISupports> isupports (do_QueryInterface(curNode));
      res = aNodeArray->AppendElement(isupports);
    }
  }
  return res;
}


///////////////////////////////////////////////////////////////////////////
// GetDefinitionListItemTypes: 
//                       
nsresult 
nsHTMLEditRules::GetDefinitionListItemTypes(nsIDOMNode *aNode, PRBool &aDT, PRBool &aDD)
{
  if (!aNode) return NS_ERROR_NULL_POINTER;
  aDT = aDD = PR_FALSE;
  nsresult res = NS_OK;
  nsCOMPtr<nsIDOMNode> child, temp;
  res = aNode->GetFirstChild(getter_AddRefs(child));
  while (child && NS_SUCCEEDED(res))
  {
    if (mHTMLEditor->NodeIsType(child,nsIEditProperty::dt)) aDT = PR_TRUE;
    else if (mHTMLEditor->NodeIsType(child,nsIEditProperty::dd)) aDD = PR_TRUE;
    res = child->GetNextSibling(getter_AddRefs(temp));
    child = temp;
  }
  return res;
}

///////////////////////////////////////////////////////////////////////////
// GetParagraphFormatNodes: 
//                       
nsresult 
nsHTMLEditRules::GetParagraphFormatNodes(nsCOMPtr<nsISupportsArray> *outArrayOfNodes,
                                         PRBool aDontTouchContent)
{
  if (!outArrayOfNodes) return NS_ERROR_NULL_POINTER;
  
  nsCOMPtr<nsISelection>selection;
  nsresult res = mHTMLEditor->GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;

  // contruct a list of nodes to act on.
  res = GetNodesFromSelection(selection, kMakeBasicBlock, outArrayOfNodes, aDontTouchContent);
  if (NS_FAILED(res)) return res;

  // pre process our list of nodes...                      
  PRUint32 listCount;
  PRInt32 i;
  (*outArrayOfNodes)->Count(&listCount);
  for (i=(PRInt32)listCount-1; i>=0; i--)
  {
    nsCOMPtr<nsISupports> isupports = dont_AddRef((*outArrayOfNodes)->ElementAt(i));
    nsCOMPtr<nsIDOMNode> testNode( do_QueryInterface(isupports ) );

    // Remove all non-editable nodes.  Leave them be.
    if (!mHTMLEditor->IsEditable(testNode))
    {
      (*outArrayOfNodes)->RemoveElementAt(i);
    }
    
    // scan for table elements.  If we find table elements other than table,
    // replace it with a list of any editable non-table content.  Ditto for list elements.
    if (nsHTMLEditUtils::IsTableElement(testNode) ||
        nsHTMLEditUtils::IsList(testNode) || 
        nsHTMLEditUtils::IsListItem(testNode) )
    {
      PRInt32 j=i;
      (*outArrayOfNodes)->RemoveElementAt(i);
      res = GetInnerContent(testNode, *outArrayOfNodes, &j);
      if (NS_FAILED(res)) return res;
    }
  }
  return res;
}


///////////////////////////////////////////////////////////////////////////
// BustUpInlinesAtRangeEndpoints: 
//                       
nsresult 
nsHTMLEditRules::BustUpInlinesAtRangeEndpoints(nsRangeStore &item)
{
  nsresult res = NS_OK;
  PRBool isCollapsed = ((item.startNode == item.endNode) && (item.startOffset == item.endOffset));

  nsCOMPtr<nsIDOMNode> endInline = GetHighestInlineParent(item.endNode);
  
  // if we have inline parents above range endpoints, split them
  if (endInline && !isCollapsed)
  {
    nsCOMPtr<nsIDOMNode> resultEndNode;
    PRInt32 resultEndOffset;
    endInline->GetParentNode(getter_AddRefs(resultEndNode));
    res = mHTMLEditor->SplitNodeDeep(endInline, item.endNode, item.endOffset,
                          &resultEndOffset, PR_TRUE);
    if (NS_FAILED(res)) return res;
    // reset range
    item.endNode = resultEndNode; item.endOffset = resultEndOffset;
  }

  nsCOMPtr<nsIDOMNode> startInline = GetHighestInlineParent(item.startNode);

  if (startInline)
  {
    nsCOMPtr<nsIDOMNode> resultStartNode;
    PRInt32 resultStartOffset;
    startInline->GetParentNode(getter_AddRefs(resultStartNode));
    res = mHTMLEditor->SplitNodeDeep(startInline, item.startNode, item.startOffset,
                          &resultStartOffset, PR_TRUE);
    if (NS_FAILED(res)) return res;
    // reset range
    item.startNode = resultStartNode; item.startOffset = resultStartOffset;
  }
  
  return res;
}



///////////////////////////////////////////////////////////////////////////
// BustUpInlinesAtBRs: 
//                       
nsresult 
nsHTMLEditRules::BustUpInlinesAtBRs(nsIDOMNode *inNode, 
                                    nsCOMPtr<nsISupportsArray> *outArrayOfNodes)
{
  if (!inNode || !outArrayOfNodes) return NS_ERROR_NULL_POINTER;

  nsresult res = NS_NewISupportsArray(getter_AddRefs(*outArrayOfNodes));
  if (NS_FAILED(res)) return res;
  
  // first step is to build up a list of all the break nodes inside 
  // the inline container.
  nsCOMPtr<nsISupportsArray> arrayOfBreaks;
  nsBRNodeFunctor functor;
  nsDOMIterator iter;
  res = iter.Init(inNode);
  if (NS_FAILED(res)) return res;
  res = iter.MakeList(functor, address_of(arrayOfBreaks));
  if (NS_FAILED(res)) return res;
  
  // if there aren't any breaks, just put inNode itself in the array
  nsCOMPtr<nsISupports> isupports;
  PRUint32 listCount;
  arrayOfBreaks->Count(&listCount);
  if (!listCount)
  {
    isupports = do_QueryInterface(inNode);
    (*outArrayOfNodes)->AppendElement(isupports);
    if (NS_FAILED(res)) return res;
  }
  else
  {
    // else we need to bust up inNode along all the breaks
    nsCOMPtr<nsIDOMNode> breakNode;
    nsCOMPtr<nsIDOMNode> inlineParentNode;
    nsCOMPtr<nsIDOMNode> leftNode;
    nsCOMPtr<nsIDOMNode> rightNode;
    nsCOMPtr<nsIDOMNode> splitDeepNode = inNode;
    nsCOMPtr<nsIDOMNode> splitParentNode;
    PRInt32 splitOffset, resultOffset, i;
    inNode->GetParentNode(getter_AddRefs(inlineParentNode));
    
    for (i=0; i< (PRInt32)listCount; i++)
    {
      isupports = dont_AddRef(arrayOfBreaks->ElementAt(i));
      breakNode = do_QueryInterface(isupports);
      if (!breakNode) return NS_ERROR_NULL_POINTER;
      if (!splitDeepNode) return NS_ERROR_NULL_POINTER;
      res = nsEditor::GetNodeLocation(breakNode, address_of(splitParentNode), &splitOffset);
      if (NS_FAILED(res)) return res;
      res = mHTMLEditor->SplitNodeDeep(splitDeepNode, splitParentNode, splitOffset,
                          &resultOffset, PR_FALSE, address_of(leftNode), address_of(rightNode));
      if (NS_FAILED(res)) return res;
      // put left node in node list
      if (leftNode)
      {
        // might not be a left node.  a break might have been at the very
        // beginning of inline container, in which case splitnodedeep
        // would not actually split anything
        isupports = do_QueryInterface(leftNode);
        (*outArrayOfNodes)->AppendElement(isupports);
        if (NS_FAILED(res)) return res;
      }
      // move break outside of container and also put in node list
      res = mHTMLEditor->MoveNode(breakNode, inlineParentNode, resultOffset);
      if (NS_FAILED(res)) return res;
      isupports = do_QueryInterface(breakNode);
      (*outArrayOfNodes)->AppendElement(isupports);
      if (NS_FAILED(res)) return res;
      // now rightNode becomes the new node to split
      splitDeepNode = rightNode;
    }
    // now tack on remaining rightNode, if any, to the list
    if (rightNode)
    {
      isupports = do_QueryInterface(rightNode);
      (*outArrayOfNodes)->AppendElement(isupports);
      if (NS_FAILED(res)) return res;
    }
  }
  return res;
}


nsCOMPtr<nsIDOMNode> 
nsHTMLEditRules::GetHighestInlineParent(nsIDOMNode* aNode)
{
  if (!aNode) return nsnull;
  if (IsBlockNode(aNode)) return nsnull;
  nsCOMPtr<nsIDOMNode> inlineNode, node=aNode;

  while (node && IsInlineNode(node))
  {
    inlineNode = node;
    inlineNode->GetParentNode(getter_AddRefs(node));
  }
  return inlineNode;
}


///////////////////////////////////////////////////////////////////////////
// GetNodesFromSelection: given a particular operation, construct a list  
//                     of nodes from the selection that will be operated on. 
//                       
nsresult 
nsHTMLEditRules::GetNodesFromSelection(nsISelection *selection,
                                       PRInt32 operation,
                                       nsCOMPtr<nsISupportsArray> *arrayOfNodes,
                                       PRBool dontTouchContent)
{
  if (!selection || !arrayOfNodes) return NS_ERROR_NULL_POINTER;
  nsresult res;
  
  // promote selection ranges
  nsCOMPtr<nsISupportsArray> arrayOfRanges;
  res = GetPromotedRanges(selection, address_of(arrayOfRanges), operation);
  if (NS_FAILED(res)) return res;
  
  // use these ranges to contruct a list of nodes to act on.
  res = GetNodesForOperation(arrayOfRanges, arrayOfNodes, operation, dontTouchContent); 
  return res;
}


///////////////////////////////////////////////////////////////////////////
// MakeTransitionList: detect all the transitions in the array, where a 
//                     transition means that adjacent nodes in the array 
//                     don't have the same parent.
//                       
nsresult 
nsHTMLEditRules::MakeTransitionList(nsISupportsArray *inArrayOfNodes, 
                                   nsVoidArray *inTransitionArray)
{
  if (!inArrayOfNodes || !inTransitionArray) return NS_ERROR_NULL_POINTER;

  PRUint32 listCount;
  PRUint32 i;
  inArrayOfNodes->Count(&listCount);
  nsVoidArray transitionList;
  nsCOMPtr<nsIDOMNode> prevElementParent;
  nsCOMPtr<nsIDOMNode> curElementParent;
  
  for (i=0; i<listCount; i++)
  {
    nsCOMPtr<nsISupports> isupports  = dont_AddRef(inArrayOfNodes->ElementAt(i));
    nsCOMPtr<nsIDOMNode> transNode( do_QueryInterface(isupports ) );
    transNode->GetParentNode(getter_AddRefs(curElementParent));
    if (curElementParent != prevElementParent)
    {
      // different parents, or seperated by <br>: transition point
      inTransitionArray->InsertElementAt((void*)PR_TRUE,i);  
    }
    else
    {
      // same parents: these nodes grew up together
      inTransitionArray->InsertElementAt((void*)PR_FALSE,i); 
    }
    prevElementParent = curElementParent;
  }
  return NS_OK;
}



/********************************************************
 *  main implementation methods 
 ********************************************************/
 
///////////////////////////////////////////////////////////////////////////
// IsInListItem: if aNode is the descendant of a listitem, return that li.
//               But table element boundaries are stoppers on the search.
//               Also test if aNode is an li itself.
//                       
nsCOMPtr<nsIDOMNode> 
nsHTMLEditRules::IsInListItem(nsIDOMNode *aNode)
{
  if (!aNode) return nsnull;  
  if (nsHTMLEditUtils::IsListItem(aNode)) return aNode;
  
  nsCOMPtr<nsIDOMNode> parent, tmp;
  aNode->GetParentNode(getter_AddRefs(parent));
  
  while (parent)
  {
    if (nsHTMLEditUtils::IsTableElement(parent)) return nsnull;
    if (nsHTMLEditUtils::IsListItem(parent)) return parent;
    tmp=parent; tmp->GetParentNode(getter_AddRefs(parent));
  }
  return nsnull;
}


///////////////////////////////////////////////////////////////////////////
// ReturnInHeader: do the right thing for returns pressed in headers
//                       
nsresult 
nsHTMLEditRules::ReturnInHeader(nsISelection *aSelection, 
                                nsIDOMNode *aHeader, 
                                nsIDOMNode *aNode, 
                                PRInt32 aOffset)
{
  if (!aSelection || !aHeader || !aNode) return NS_ERROR_NULL_POINTER;  
  
  // remeber where the header is
  nsCOMPtr<nsIDOMNode> headerParent;
  PRInt32 offset;
  nsresult res = nsEditor::GetNodeLocation(aHeader, address_of(headerParent), &offset);
  if (NS_FAILED(res)) return res;

  // get ws code to adjust any ws
  nsCOMPtr<nsIDOMNode> selNode = aNode;
  res = nsWSRunObject::PrepareToSplitAcrossBlocks(mHTMLEditor, address_of(selNode), &aOffset);
  if (NS_FAILED(res)) return res;

  // split the header
  PRInt32 newOffset;
  res = mHTMLEditor->SplitNodeDeep( aHeader, selNode, aOffset, &newOffset);
  if (NS_FAILED(res)) return res;

  // if the leftand heading is empty, put a mozbr in it
  nsCOMPtr<nsIDOMNode> prevItem;
  mHTMLEditor->GetPriorHTMLSibling(aHeader, address_of(prevItem));
  if (prevItem && nsHTMLEditUtils::IsHeader(prevItem))
  {
    PRBool bIsEmptyNode;
    res = mHTMLEditor->IsEmptyNode(prevItem, &bIsEmptyNode);
    if (NS_FAILED(res)) return res;
    if (bIsEmptyNode)
    {
      nsCOMPtr<nsIDOMNode> brNode;
      res = CreateMozBR(prevItem, 0, address_of(brNode));
      if (NS_FAILED(res)) return res;
    }
  }
  
  // if the new (righthand) header node is empty, delete it
  PRBool isEmpty;
  res = IsEmptyBlock(aHeader, &isEmpty, PR_TRUE);
  if (NS_FAILED(res)) return res;
  if (isEmpty)
  {
    res = mHTMLEditor->DeleteNode(aHeader);
    if (NS_FAILED(res)) return res;
    // layout tells the caret to blink in a weird place
    // if we dont place a break after the header.
    nsCOMPtr<nsIDOMNode> sibling;
    res = mHTMLEditor->GetNextHTMLSibling(headerParent, offset+1, address_of(sibling));
    if (NS_FAILED(res)) return res;
    if (!sibling || !nsTextEditUtils::IsBreak(sibling))
    {
      res = CreateMozBR(headerParent, offset+1, address_of(sibling));
      if (NS_FAILED(res)) return res;
    }
    res = nsEditor::GetNodeLocation(sibling, address_of(headerParent), &offset);
    if (NS_FAILED(res)) return res;
    // put selection after break
    res = aSelection->Collapse(headerParent,offset+1);
  }
  else
  {
    // put selection at front of righthand heading
    res = aSelection->Collapse(aHeader,0);
  }
  return res;
}


///////////////////////////////////////////////////////////////////////////
// ReturnInParagraph: do the right thing for returns pressed in paragraphs
//                       
nsresult 
nsHTMLEditRules::ReturnInParagraph(nsISelection *aSelection, 
                                   nsIDOMNode *aPara, 
                                   nsIDOMNode *aNode, 
                                   PRInt32 aOffset,
                                   PRBool *aCancel,
                                   PRBool *aHandled)
{
  if (!aSelection || !aPara || !aNode || !aCancel || !aHandled) 
    { return NS_ERROR_NULL_POINTER; }
  *aCancel = PR_FALSE;
  *aHandled = PR_FALSE;

  nsCOMPtr<nsIDOMNode> sibling;
  nsresult res = NS_OK;

  // easy case, in a text node:
  if (mHTMLEditor->IsTextNode(aNode))
  {
    nsCOMPtr<nsIDOMText> textNode = do_QueryInterface(aNode);
    PRUint32 strLength;
    res = textNode->GetLength(&strLength);
    if (NS_FAILED(res)) return res;
    
    // at beginning of text node?
    if (!aOffset)
    {
      // is there a BR prior to it?
      mHTMLEditor->GetPriorHTMLSibling(aNode, address_of(sibling));
      if (!sibling) 
      {
        // no previous sib, so
        // just fall out to default of inserting a BR
        return res;
      }
      if (IsVisBreak(sibling) && !nsTextEditUtils::HasMozAttr(sibling))
      {
        // split para
        nsCOMPtr<nsIDOMNode> selNode = aNode;
        *aHandled = PR_TRUE;
        res = SplitParagraph(aPara, sibling, aSelection, address_of(selNode), &aOffset);
      }
      // else just fall out to default of inserting a BR
      return res;
    }
    // at end of text node?
    if (aOffset == (PRInt32)strLength)
    {
      // is there a BR after to it?
      res = mHTMLEditor->GetNextHTMLSibling(aNode, address_of(sibling));
      if (!sibling) 
      {
        // no next sib, so
        // just fall out to default of inserting a BR
        return res;
      }
      if (IsVisBreak(sibling) && !nsTextEditUtils::HasMozAttr(sibling))
      {
        // split para
        nsCOMPtr<nsIDOMNode> selNode = aNode;
        *aHandled = PR_TRUE;
        res = SplitParagraph(aPara, sibling, aSelection, address_of(selNode), &aOffset);
      }
      // else just fall out to default of inserting a BR
      return res;
    }
    // inside text node
    // just fall out to default of inserting a BR
    return res;
  }
  else
  {
    // not in a text node.  
    // is there a BR prior to it?
    nsCOMPtr<nsIDOMNode> nearNode, selNode = aNode;
    res = mHTMLEditor->GetPriorHTMLNode(aNode, aOffset, address_of(nearNode));
    if (NS_FAILED(res)) return res;
    if (!nearNode || !IsVisBreak(nearNode) || nsTextEditUtils::HasMozAttr(nearNode)) 
    {
      // is there a BR after it?
      res = mHTMLEditor->GetNextHTMLNode(aNode, aOffset, address_of(nearNode));
      if (NS_FAILED(res)) return res;
      if (!nearNode || !IsVisBreak(nearNode) || nsTextEditUtils::HasMozAttr(nearNode)) 
      {
        // just fall out to default of inserting a BR
        return res;
      }
    }
    // else split para
    *aHandled = PR_TRUE;
    res = SplitParagraph(aPara, nearNode, aSelection, address_of(selNode), &aOffset);
  }
  
  return res;
}


///////////////////////////////////////////////////////////////////////////
// SplitParagraph: split a paragraph at selection point, possibly deleting a br
//                       
nsresult 
nsHTMLEditRules::SplitParagraph(nsIDOMNode *aPara,
                                nsIDOMNode *aBRNode, 
                                nsISelection *aSelection,
                                nsCOMPtr<nsIDOMNode> *aSelNode, 
                                PRInt32 *aOffset)
{
  if (!aPara || !aBRNode || !aSelNode || !*aSelNode || !aOffset || !aSelection) 
    return NS_ERROR_NULL_POINTER;
  nsresult res = NS_OK;
  
  // split para
  PRInt32 newOffset;
  // get ws code to adjust any ws
  nsCOMPtr<nsIDOMNode> leftPara, rightPara;
  res = nsWSRunObject::PrepareToSplitAcrossBlocks(mHTMLEditor, aSelNode, aOffset);
  if (NS_FAILED(res)) return res;
  // split the paragraph
  res = mHTMLEditor->SplitNodeDeep(aPara, *aSelNode, *aOffset, &newOffset, PR_FALSE,
                                   address_of(leftPara), address_of(rightPara));
  if (NS_FAILED(res)) return res;
  // get rid of the break, if it is visible (otherwise it may be needed to prevent an empty p)
  if (IsVisBreak(aBRNode))
  {
    res = mHTMLEditor->DeleteNode(aBRNode);  
    if (NS_FAILED(res)) return res;
  }
  
  // check both halves of para to see if we need mozBR
  res = InsertMozBRIfNeeded(leftPara);
  if (NS_FAILED(res)) return res;
  res = InsertMozBRIfNeeded(rightPara);
  if (NS_FAILED(res)) return res;

  // selection to beginning of right hand para;
  // look inside any containers that are up front.
  nsCOMPtr<nsIDOMNode> child = mHTMLEditor->GetLeftmostChild(rightPara, PR_TRUE);
  if (mHTMLEditor->IsTextNode(child) || mHTMLEditor->IsContainer(child))
  {
    aSelection->Collapse(child,0);
  }
  else
  {
    nsCOMPtr<nsIDOMNode> parent;
    PRInt32 offset;
    res = nsEditor::GetNodeLocation(child, address_of(parent), &offset);
    aSelection->Collapse(parent,offset);
  }
  return res;
}


///////////////////////////////////////////////////////////////////////////
// ReturnInListItem: do the right thing for returns pressed in list items
//                       
nsresult 
nsHTMLEditRules::ReturnInListItem(nsISelection *aSelection, 
                                  nsIDOMNode *aListItem, 
                                  nsIDOMNode *aNode, 
                                  PRInt32 aOffset)
{
  if (!aSelection || !aListItem || !aNode) return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsISelection> selection(aSelection);
  nsCOMPtr<nsISelectionPrivate> selPriv(do_QueryInterface(selection));
  nsresult res = NS_OK;
  
  nsCOMPtr<nsIDOMNode> listitem;
  
  // sanity check
  NS_PRECONDITION(PR_TRUE == nsHTMLEditUtils::IsListItem(aListItem),
                  "expected a list item and didnt get one");
  
  // if we are in an empty listitem, then we want to pop up out of the list
  PRBool isEmpty;
  res = IsEmptyBlock(aListItem, &isEmpty, PR_TRUE, PR_FALSE);
  if (NS_FAILED(res)) return res;
  if (isEmpty && mReturnInEmptyLIKillsList)   // but only if prefs says it's ok
  {
    nsCOMPtr<nsIDOMNode> list, listparent;
    PRInt32 offset, itemOffset;
    res = nsEditor::GetNodeLocation(aListItem, address_of(list), &itemOffset);
    if (NS_FAILED(res)) return res;
    res = nsEditor::GetNodeLocation(list, address_of(listparent), &offset);
    if (NS_FAILED(res)) return res;
    
    // are we the last list item in the list?
    PRBool bIsLast;
    res = mHTMLEditor->IsLastEditableChild(aListItem, &bIsLast);
    if (NS_FAILED(res)) return res;
    if (!bIsLast)
    {
      // we need to split the list!
      nsCOMPtr<nsIDOMNode> tempNode;
      res = mHTMLEditor->SplitNode(list, itemOffset, getter_AddRefs(tempNode));
      if (NS_FAILED(res)) return res;
    }
    // are we in a sublist?
    if (nsHTMLEditUtils::IsList(listparent))  //in a sublist
    {
      // if so, move this list item out of this list and into the grandparent list
      res = mHTMLEditor->MoveNode(aListItem,listparent,offset+1);
      if (NS_FAILED(res)) return res;
      res = aSelection->Collapse(aListItem,0);
    }
    else
    {
      // otherwise kill this listitem
      res = mHTMLEditor->DeleteNode(aListItem);
      if (NS_FAILED(res)) return res;
      
      // time to insert a break
      nsCOMPtr<nsIDOMNode> brNode;
      res = CreateMozBR(listparent, offset+1, address_of(brNode));
      if (NS_FAILED(res)) return res;
      
      // set selection to before the moz br
      selPriv->SetInterlinePosition(PR_TRUE);
      res = aSelection->Collapse(listparent,offset+1);
    }
    return res;
  }
  
  // else we want a new list item at the same list level.
  // get ws code to adjust any ws
  nsCOMPtr<nsIDOMNode> selNode = aNode;
  res = nsWSRunObject::PrepareToSplitAcrossBlocks(mHTMLEditor, address_of(selNode), &aOffset);
  if (NS_FAILED(res)) return res;
  // now split list item
  PRInt32 newOffset;
  res = mHTMLEditor->SplitNodeDeep( aListItem, selNode, aOffset, &newOffset, PR_FALSE);
  if (NS_FAILED(res)) return res;
  // hack: until I can change the damaged doc range code back to being
  // extra inclusive, I have to manually detect certain list items that
  // may be left empty.
  nsCOMPtr<nsIDOMNode> prevItem;
  mHTMLEditor->GetPriorHTMLSibling(aListItem, address_of(prevItem));

  if (prevItem && nsHTMLEditUtils::IsListItem(prevItem))
  {
    PRBool bIsEmptyNode;
    res = mHTMLEditor->IsEmptyNode(prevItem, &bIsEmptyNode);
    if (NS_FAILED(res)) return res;
    if (bIsEmptyNode)
    {
      nsCOMPtr<nsIDOMNode> brNode;
      res = CreateMozBR(prevItem, 0, address_of(brNode));
      if (NS_FAILED(res)) return res;
    }
    else {
      res = mHTMLEditor->IsEmptyNode(aListItem, &bIsEmptyNode, PR_TRUE);
      if (NS_FAILED(res)) return res;
      if (bIsEmptyNode) {
        nsCOMPtr<nsIDOMNode> brNode;
        res = mHTMLEditor->CopyLastEditableChildStyles(prevItem, aListItem, getter_AddRefs(brNode));
        if (NS_FAILED(res)) return res;
        if (brNode) {
          nsCOMPtr<nsIDOMNode> brParent;
          PRInt32 offset;
          res = nsEditor::GetNodeLocation(brNode, address_of(brParent), &offset);
          return aSelection->Collapse(brParent, offset);
        }
      }
    }
  }
  res = aSelection->Collapse(aListItem,0);
  return res;
}


///////////////////////////////////////////////////////////////////////////
// MakeBlockquote:  put the list of nodes into one or more blockquotes.  
//                       
nsresult 
nsHTMLEditRules::MakeBlockquote(nsISupportsArray *arrayOfNodes)
{
  // the idea here is to put the nodes into a minimal number of 
  // blockquotes.  When the user blockquotes something, they expect
  // one blockquote.  That may not be possible (for instance, if they
  // have two table cells selected, you need two blockquotes inside the cells).
  
  if (!arrayOfNodes) return NS_ERROR_NULL_POINTER;
  nsresult res = NS_OK;
  
  nsCOMPtr<nsIDOMNode> curNode, curParent, curBlock, newBlock;
  PRInt32 offset;
  PRUint32 listCount;
  
  arrayOfNodes->Count(&listCount);
  nsCOMPtr<nsIDOMNode> prevParent;
  
  PRUint32 i;
  for (i=0; i<listCount; i++)
  {
    // get the node to act on, and it's location
    nsCOMPtr<nsISupports> isupports  = dont_AddRef(arrayOfNodes->ElementAt(i));
    curNode = do_QueryInterface(isupports);
    res = nsEditor::GetNodeLocation(curNode, address_of(curParent), &offset);
    if (NS_FAILED(res)) return res;

    // if the node is a table element or list item, dive inside
    if ( (nsHTMLEditUtils::IsTableElement(curNode) && !(nsHTMLEditUtils::IsTable(curNode))) || 
         nsHTMLEditUtils::IsListItem(curNode) )
    {
      curBlock = 0;  // forget any previous block
      // recursion time
      nsCOMPtr<nsISupportsArray> childArray;
      res = GetChildNodesForOperation(curNode, address_of(childArray));
      if (NS_FAILED(res)) return res;
      res = MakeBlockquote(childArray);
      if (NS_FAILED(res)) return res;
    }
    
    // if the node has different parent than previous node,
    // further nodes in a new parent
    if (prevParent)
    {
      nsCOMPtr<nsIDOMNode> temp;
      curNode->GetParentNode(getter_AddRefs(temp));
      if (temp != prevParent)
      {
        curBlock = 0;  // forget any previous blockquote node we were using
        prevParent = temp;
      }
    }
    else     

    {
      curNode->GetParentNode(getter_AddRefs(prevParent));
    }
    
    // if no curBlock, make one
    if (!curBlock)
    {
      NS_NAMED_LITERAL_STRING(quoteType, "blockquote");
      res = SplitAsNeeded(&quoteType, address_of(curParent), &offset);
      if (NS_FAILED(res)) return res;
      res = mHTMLEditor->CreateNode(quoteType, curParent, offset, getter_AddRefs(curBlock));
      if (NS_FAILED(res)) return res;
      // remember our new block for postprocessing
      mNewBlock = curBlock;
      // note: doesn't matter if we set mNewBlock multiple times.
    }
      
    res = mHTMLEditor->MoveNode(curNode, curBlock, -1);
    if (NS_FAILED(res)) return res;
  }
  return res;
}



///////////////////////////////////////////////////////////////////////////
// RemoveBlockStyle:  make the nodes have no special block type.  
//                       
nsresult 
nsHTMLEditRules::RemoveBlockStyle(nsISupportsArray *arrayOfNodes)
{
  // intent of this routine is to be used for converting to/from
  // headers, paragraphs, pre, and address.  Those blocks
  // that pretty much just contain inline things...
  
  if (!arrayOfNodes) return NS_ERROR_NULL_POINTER;
  nsresult res = NS_OK;
  
  nsCOMPtr<nsIDOMNode> curNode, curParent, curBlock, firstNode, lastNode;
  PRInt32 offset;
  PRUint32 listCount;
    
  arrayOfNodes->Count(&listCount);
  
  PRUint32 i;
  for (i=0; i<listCount; i++)
  {
    // get the node to act on, and it's location
    nsCOMPtr<nsISupports> isupports  = dont_AddRef(arrayOfNodes->ElementAt(i));
    curNode = do_QueryInterface(isupports);
    res = nsEditor::GetNodeLocation(curNode, address_of(curParent), &offset);
    if (NS_FAILED(res)) return res;
    nsAutoString curNodeTag, curBlockTag;
    nsEditor::GetTagString(curNode, curNodeTag);
    ToLowerCase(curNodeTag);
 
    // if curNode is a address, p, header, address, or pre, remove it 
    if ((curNodeTag.Equals(NS_LITERAL_STRING("pre"))) || 
        (curNodeTag.Equals(NS_LITERAL_STRING("p")))   ||
        (curNodeTag.Equals(NS_LITERAL_STRING("h1")))  ||
        (curNodeTag.Equals(NS_LITERAL_STRING("h2")))  ||
        (curNodeTag.Equals(NS_LITERAL_STRING("h3")))  ||
        (curNodeTag.Equals(NS_LITERAL_STRING("h4")))  ||
        (curNodeTag.Equals(NS_LITERAL_STRING("h5")))  ||
        (curNodeTag.Equals(NS_LITERAL_STRING("h6")))  ||
        (curNodeTag.Equals(NS_LITERAL_STRING("address"))))
    {
      // process any partial progress saved
      if (curBlock)
      {
        res = RemovePartOfBlock(curBlock, firstNode, lastNode);
        if (NS_FAILED(res)) return res;
        curBlock = 0;  firstNode = 0;  lastNode = 0;
      }
      // remove curent block
      res = mHTMLEditor->RemoveBlockContainer(curNode); 
      if (NS_FAILED(res)) return res;
    }
    else if ((curNodeTag.Equals(NS_LITERAL_STRING("table")))      || 
             (curNodeTag.Equals(NS_LITERAL_STRING("tbody")))      ||
             (curNodeTag.Equals(NS_LITERAL_STRING("tr")))         ||
             (curNodeTag.Equals(NS_LITERAL_STRING("td")))         ||
             (curNodeTag.Equals(NS_LITERAL_STRING("ol")))         ||
             (curNodeTag.Equals(NS_LITERAL_STRING("ul")))         ||
             (curNodeTag.Equals(NS_LITERAL_STRING("dl")))         ||
             (curNodeTag.Equals(NS_LITERAL_STRING("li")))         ||
             (curNodeTag.Equals(NS_LITERAL_STRING("blockquote"))) ||
             (curNodeTag.Equals(NS_LITERAL_STRING("div")))) 
    {
      // process any partial progress saved
      if (curBlock)
      {
        res = RemovePartOfBlock(curBlock, firstNode, lastNode);
        if (NS_FAILED(res)) return res;
        curBlock = 0;  firstNode = 0;  lastNode = 0;
      }
      // recursion time
      nsCOMPtr<nsISupportsArray> childArray;
      res = GetChildNodesForOperation(curNode, address_of(childArray));
      if (NS_FAILED(res)) return res;
      res = RemoveBlockStyle(childArray);
      if (NS_FAILED(res)) return res;
    }
    else if (IsInlineNode(curNode))
    {
      if (curBlock)
      {
        // if so, is this node a descendant?
        if (nsHTMLEditUtils::IsDescendantOf(curNode, curBlock))
        {
          lastNode = curNode;
          continue;  // then we dont need to do anything different for this node
        }
        else
        {
          // otherwise, we have progressed beyond end of curBlock,
          // so lets handle it now.  We need to remove the portion of 
          // curBlock that contains [firstNode - lastNode].
          res = RemovePartOfBlock(curBlock, firstNode, lastNode);
          if (NS_FAILED(res)) return res;
          curBlock = 0;  firstNode = 0;  lastNode = 0;
          // fall out and handle curNode
        }
      }
      curBlock = mHTMLEditor->GetBlockNodeParent(curNode);
      nsEditor::GetTagString(curBlock, curBlockTag);
      ToLowerCase(curBlockTag);
      if ((curBlockTag.Equals(NS_LITERAL_STRING("pre"))) || 
          (curBlockTag.Equals(NS_LITERAL_STRING("p")))   ||
          (curBlockTag.Equals(NS_LITERAL_STRING("h1")))  ||
          (curBlockTag.Equals(NS_LITERAL_STRING("h2")))  ||
          (curBlockTag.Equals(NS_LITERAL_STRING("h3")))  ||
          (curBlockTag.Equals(NS_LITERAL_STRING("h4")))  ||
          (curBlockTag.Equals(NS_LITERAL_STRING("h5")))  ||
          (curBlockTag.Equals(NS_LITERAL_STRING("h6")))  ||
          (curBlockTag.Equals(NS_LITERAL_STRING("address"))))
      {
        firstNode = curNode;  
        lastNode = curNode;
      }
      else
        curBlock = 0;  // not a block kind that we care about.
    }
    else
    { // some node that is already sans block style.  skip over it and
      // process any partial progress saved
      if (curBlock)
      {
        res = RemovePartOfBlock(curBlock, firstNode, lastNode);
        if (NS_FAILED(res)) return res;
        curBlock = 0;  firstNode = 0;  lastNode = 0;
      }
    }
  }
  // process any partial progress saved
  if (curBlock)
  {
    res = RemovePartOfBlock(curBlock, firstNode, lastNode);
    if (NS_FAILED(res)) return res;
    curBlock = 0;  firstNode = 0;  lastNode = 0;
  }
  return res;
}


///////////////////////////////////////////////////////////////////////////
// ApplyBlockStyle:  do whatever it takes to make the list of nodes into 
//                   one or more blocks of type blockTag.  
//                       
nsresult 
nsHTMLEditRules::ApplyBlockStyle(nsISupportsArray *arrayOfNodes, const nsAString *aBlockTag)
{
  // intent of this routine is to be used for converting to/from
  // headers, paragraphs, pre, and address.  Those blocks
  // that pretty much just contain inline things...
  
  if (!arrayOfNodes || !aBlockTag) return NS_ERROR_NULL_POINTER;
  nsresult res = NS_OK;
  
  nsCOMPtr<nsIDOMNode> curNode, curParent, curBlock, newBlock;
  PRInt32 offset;
  PRUint32 listCount;
  nsString tString(*aBlockTag);////MJUDGE SCC NEED HELP

  arrayOfNodes->Count(&listCount);
  
  PRUint32 i;
  for (i=0; i<listCount; i++)
  {
    // get the node to act on, and it's location
    nsCOMPtr<nsISupports> isupports  = dont_AddRef(arrayOfNodes->ElementAt(i));
    curNode = do_QueryInterface(isupports);
    res = nsEditor::GetNodeLocation(curNode, address_of(curParent), &offset);
    if (NS_FAILED(res)) return res;
    nsAutoString curNodeTag;
    nsEditor::GetTagString(curNode, curNodeTag);
    ToLowerCase(curNodeTag);
 
    // is it already the right kind of block?
    if (curNodeTag == *aBlockTag)
    {
      curBlock = 0;  // forget any previous block used for previous inline nodes
      continue;  // do nothing to this block
    }
        
    // if curNode is a address, p, header, address, or pre, replace 
    // it with a new block of correct type.
    // xxx floppy moose: pre cant hold everything the others can
    if (nsHTMLEditUtils::IsMozDiv(curNode)     ||
        (curNodeTag.Equals(NS_LITERAL_STRING("pre"))) || 
        (curNodeTag.Equals(NS_LITERAL_STRING("p")))   ||
        (curNodeTag.Equals(NS_LITERAL_STRING("h1")))  ||
        (curNodeTag.Equals(NS_LITERAL_STRING("h2")))  ||
        (curNodeTag.Equals(NS_LITERAL_STRING("h3")))  ||
        (curNodeTag.Equals(NS_LITERAL_STRING("h4")))  ||
        (curNodeTag.Equals(NS_LITERAL_STRING("h5")))  ||
        (curNodeTag.Equals(NS_LITERAL_STRING("h6")))  ||
        (curNodeTag.Equals(NS_LITERAL_STRING("address"))))
    {
      curBlock = 0;  // forget any previous block used for previous inline nodes
      res = mHTMLEditor->ReplaceContainer(curNode, address_of(newBlock), *aBlockTag);
      if (NS_FAILED(res)) return res;
    }
    else if ((curNodeTag.Equals(NS_LITERAL_STRING("table")))      || 
             (curNodeTag.Equals(NS_LITERAL_STRING("tbody")))      ||
             (curNodeTag.Equals(NS_LITERAL_STRING("tr")))         ||
             (curNodeTag.Equals(NS_LITERAL_STRING("td")))         ||
             (curNodeTag.Equals(NS_LITERAL_STRING("ol")))         ||
             (curNodeTag.Equals(NS_LITERAL_STRING("ul")))         ||
             (curNodeTag.Equals(NS_LITERAL_STRING("dl")))         ||
             (curNodeTag.Equals(NS_LITERAL_STRING("li")))         ||
             (curNodeTag.Equals(NS_LITERAL_STRING("blockquote"))) ||
             (curNodeTag.Equals(NS_LITERAL_STRING("div")))) 
    {
      curBlock = 0;  // forget any previous block used for previous inline nodes
      // recursion time
      nsCOMPtr<nsISupportsArray> childArray;
      res = GetChildNodesForOperation(curNode, address_of(childArray));
      if (NS_FAILED(res)) return res;
      PRUint32 childCount;
      childArray->Count(&childCount);
      if (childCount)
      {
        res = ApplyBlockStyle(childArray, aBlockTag);
        if (NS_FAILED(res)) return res;
      }
      else
      {
        // make sure we can put a block here
        res = SplitAsNeeded(aBlockTag, address_of(curParent), &offset);
        if (NS_FAILED(res)) return res;
        nsCOMPtr<nsIDOMNode> theBlock;
        res = mHTMLEditor->CreateNode(*aBlockTag, curParent, offset, getter_AddRefs(theBlock));
        if (NS_FAILED(res)) return res;
        // remember our new block for postprocessing
        mNewBlock = theBlock;
      }
    }
    
    // if the node is a break, we honor it by putting further nodes in a new parent
    else if (curNodeTag.Equals(NS_LITERAL_STRING("br")))
    {
      if (curBlock)
      {
        curBlock = 0;  // forget any previous block used for previous inline nodes
        res = mHTMLEditor->DeleteNode(curNode);
        if (NS_FAILED(res)) return res;
      }
      else
      {
        // the break is the first (or even only) node we encountered.  Create a
        // block for it.
        res = SplitAsNeeded(aBlockTag, address_of(curParent), &offset);
        if (NS_FAILED(res)) return res;
        res = mHTMLEditor->CreateNode(*aBlockTag, curParent, offset, getter_AddRefs(curBlock));
        if (NS_FAILED(res)) return res;
        // remember our new block for postprocessing
        mNewBlock = curBlock;
        // note: doesn't matter if we set mNewBlock multiple times.
        res = mHTMLEditor->MoveNode(curNode, curBlock, -1);
        if (NS_FAILED(res)) return res;
      }
    }
        
    
    // if curNode is inline, pull it into curBlock
    // note: it's assumed that consecutive inline nodes in the 
    // arrayOfNodes are actually members of the same block parent.
    // this happens to be true now as a side effect of how
    // arrayOfNodes is contructed, but some additional logic should
    // be added here if that should change
    
    else if (IsInlineNode(curNode))
    {
      // if curNode is a non editable, drop it if we are going to <pre>
      if (tString.Equals(NS_LITERAL_STRING("pre"),nsCaseInsensitiveStringComparator()) 
        && (!mHTMLEditor->IsEditable(curNode)))
        continue; // do nothing to this block
      
      // if no curBlock, make one
      if (!curBlock)
      {
        res = SplitAsNeeded(aBlockTag, address_of(curParent), &offset);
        if (NS_FAILED(res)) return res;
        res = mHTMLEditor->CreateNode(*aBlockTag, curParent, offset, getter_AddRefs(curBlock));
        if (NS_FAILED(res)) return res;
        // remember our new block for postprocessing
        mNewBlock = curBlock;
        // note: doesn't matter if we set mNewBlock multiple times.
      }
      
      // if curNode is a Break, replace it with a return if we are going to <pre>
      // xxx floppy moose
 
      // this is a continuation of some inline nodes that belong together in
      // the same block item.  use curBlock
      res = mHTMLEditor->MoveNode(curNode, curBlock, -1);
      if (NS_FAILED(res)) return res;
    }
  }
  return res;
}


///////////////////////////////////////////////////////////////////////////
// SplitAsNeeded:  given a tag name, split inOutParent up to the point   
//                 where we can insert the tag.  Adjust inOutParent and
//                 inOutOffset to pint to new location for tag.
nsresult 
nsHTMLEditRules::SplitAsNeeded(const nsAString *aTag, 
                               nsCOMPtr<nsIDOMNode> *inOutParent,
                               PRInt32 *inOutOffset)
{
  if (!aTag || !inOutParent || !inOutOffset) return NS_ERROR_NULL_POINTER;
  if (!*inOutParent) return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsIDOMNode> tagParent, temp, splitNode, parent = *inOutParent;
  nsresult res = NS_OK;
   
  // check that we have a place that can legally contain the tag
  while (!tagParent)
  {
    // sniffing up the parent tree until we find 
    // a legal place for the block
    if (!parent) break;
    if (mHTMLEditor->CanContainTag(parent, *aTag))
    {
      tagParent = parent;
      break;
    }
    splitNode = parent;
    parent->GetParentNode(getter_AddRefs(temp));
    parent = temp;
  }
  if (!tagParent)
  {
    // could not find a place to build tag!
    return NS_ERROR_FAILURE;
  }
  if (splitNode)
  {
    // we found a place for block, but above inOutParent.  We need to split nodes.
    res = mHTMLEditor->SplitNodeDeep(splitNode, *inOutParent, *inOutOffset, inOutOffset);
    if (NS_FAILED(res)) return res;
    *inOutParent = tagParent;
  }
  return res;
}      

///////////////////////////////////////////////////////////////////////////
// JoinNodesSmart:  join two nodes, doing whatever makes sense for their  
//                  children (which often means joining them, too).
//                  aNodeLeft & aNodeRight must be same type of node.
nsresult 
nsHTMLEditRules::JoinNodesSmart( nsIDOMNode *aNodeLeft, 
                                 nsIDOMNode *aNodeRight, 
                                 nsCOMPtr<nsIDOMNode> *aOutMergeParent, 
                                 PRInt32 *aOutMergeOffset)
{
  // check parms
  if (!aNodeLeft ||  
      !aNodeRight || 
      !aOutMergeParent ||
      !aOutMergeOffset) 
    return NS_ERROR_NULL_POINTER;
  
  nsresult res = NS_OK;
  // caller responsible for:
  //   left & right node are same type
  PRInt32 parOffset;
  nsCOMPtr<nsIDOMNode> parent, rightParent;
  res = nsEditor::GetNodeLocation(aNodeLeft, address_of(parent), &parOffset);
  if (NS_FAILED(res)) return res;
  aNodeRight->GetParentNode(getter_AddRefs(rightParent));

  // if they don't have the same parent, first move the 'right' node 
  // to after the 'left' one
  if (parent != rightParent)
  {
    res = mHTMLEditor->MoveNode(aNodeRight, parent, parOffset);
    if (NS_FAILED(res)) return res;
  }
  
  // defaults for outParams
  *aOutMergeParent = aNodeRight;
  res = mHTMLEditor->GetLengthOfDOMNode(aNodeLeft, *((PRUint32*)aOutMergeOffset));
  if (NS_FAILED(res)) return res;

  // seperate join rules for differing blocks
  if (nsHTMLEditUtils::IsParagraph(aNodeLeft))
  {
    // for para's, merge deep & add a <br> after merging
    res = mHTMLEditor->JoinNodeDeep(aNodeLeft, aNodeRight, aOutMergeParent, aOutMergeOffset);
    if (NS_FAILED(res)) return res;
    // now we need to insert a br.  
    nsCOMPtr<nsIDOMNode> brNode;
    res = mHTMLEditor->CreateBR(*aOutMergeParent, *aOutMergeOffset, address_of(brNode));
    if (NS_FAILED(res)) return res;
    res = nsEditor::GetNodeLocation(brNode, aOutMergeParent, aOutMergeOffset);
    if (NS_FAILED(res)) return res;
    (*aOutMergeOffset)++;
    return res;
  }
  else if (nsHTMLEditUtils::IsList(aNodeLeft)
           || mHTMLEditor->IsTextNode(aNodeLeft))
  {
    // for list's, merge shallow (wouldn't want to combine list items)
    res = mHTMLEditor->JoinNodes(aNodeLeft, aNodeRight, parent);
    if (NS_FAILED(res)) return res;
    return res;
  }
  else
  {
    // remember the last left child, and firt right child
    nsCOMPtr<nsIDOMNode> lastLeft, firstRight;
    res = mHTMLEditor->GetLastEditableChild(aNodeLeft, address_of(lastLeft));
    if (NS_FAILED(res)) return res;
    res = mHTMLEditor->GetFirstEditableChild(aNodeRight, address_of(firstRight));
    if (NS_FAILED(res)) return res;

    // for list items, divs, etc, merge smart
    res = mHTMLEditor->JoinNodes(aNodeLeft, aNodeRight, parent);
    if (NS_FAILED(res)) return res;

    if (lastLeft && firstRight && mHTMLEditor->NodesSameType(lastLeft, firstRight))
    {
      return JoinNodesSmart(lastLeft, firstRight, aOutMergeParent, aOutMergeOffset);
    }
  }
  return res;
}


nsresult 
nsHTMLEditRules::GetTopEnclosingMailCite(nsIDOMNode *aNode, 
                                         nsCOMPtr<nsIDOMNode> *aOutCiteNode,
                                         PRBool aPlainText)
{
  // check parms
  if (!aNode || !aOutCiteNode) 
    return NS_ERROR_NULL_POINTER;
  
  nsresult res = NS_OK;
  nsCOMPtr<nsIDOMNode> node, parentNode;
  node = do_QueryInterface(aNode);
  
  while (node)
  {
    if ( (aPlainText && nsHTMLEditUtils::IsPre(node)) ||
         nsHTMLEditUtils::IsMailCite(node) )
      *aOutCiteNode = node;
    if (nsTextEditUtils::IsBody(node)) break;
    
    res = node->GetParentNode(getter_AddRefs(parentNode));
    if (NS_FAILED(res)) return res;
    node = parentNode;
  }

  return res;
}


nsresult 
nsHTMLEditRules::AdjustSpecialBreaks(PRBool aSafeToAskFrames)
{
  nsCOMPtr<nsISupportsArray> arrayOfNodes;
  nsCOMPtr<nsISupports> isupports;
  PRUint32 nodeCount,j;
  
  // gather list of empty nodes
  nsEmptyFunctor functor(mHTMLEditor);
  nsDOMIterator iter;
  nsresult res = iter.Init(mDocChangeRange);
  if (NS_FAILED(res)) return res;
  res = iter.MakeList(functor, address_of(arrayOfNodes));
  if (NS_FAILED(res)) return res;

  // put moz-br's into these empty li's and td's
  res = arrayOfNodes->Count(&nodeCount);
  if (NS_FAILED(res)) return res;
  for (j = 0; j < nodeCount; j++)
  {
    // need to put br at END of node.  It may have
    // empty containers in it and still pass the "IsEmptynode" test,
    // and we want the br's to be after them.  Also, we want the br
    // to be after the selection if the selection is in this node.
    PRUint32 len;
    isupports = dont_AddRef(arrayOfNodes->ElementAt(0));
    nsCOMPtr<nsIDOMNode> brNode, theNode( do_QueryInterface(isupports ) );
    arrayOfNodes->RemoveElementAt(0);
    res = nsEditor::GetLengthOfDOMNode(theNode, len);
    if (NS_FAILED(res)) return res;
    res = CreateMozBR(theNode, (PRInt32)len, address_of(brNode));
    if (NS_FAILED(res)) return res;
  }
  
  return res;
}

nsresult 
nsHTMLEditRules::AdjustWhitespace(nsISelection *aSelection)
{
  // get selection point
  nsCOMPtr<nsIDOMNode> selNode;
  PRInt32 selOffset;
  nsresult res = mHTMLEditor->GetStartNodeAndOffset(aSelection, address_of(selNode), &selOffset);
  if (NS_FAILED(res)) return res;
  
  // ask whitespace object to tweak nbsp's
  return nsWSRunObject(mHTMLEditor, selNode, selOffset).AdjustWhitespace();
}

nsresult 
nsHTMLEditRules::PinSelectionToNewBlock(nsISelection *aSelection)
{
  if (!aSelection) return NS_ERROR_NULL_POINTER;
  PRBool bCollapsed;
  nsresult res = aSelection->GetIsCollapsed(&bCollapsed);
  if (NS_FAILED(res)) return res;
  if (!bCollapsed) return res;

  // get the (collapsed) selection location
  nsCOMPtr<nsIDOMNode> selNode, temp;
  PRInt32 selOffset;
  res = mHTMLEditor->GetStartNodeAndOffset(aSelection, address_of(selNode), &selOffset);
  if (NS_FAILED(res)) return res;
  temp = selNode;
  
  // use ranges and mRangeHelper to compare sel point to new block
  nsCOMPtr<nsIDOMRange> range = do_CreateInstance(kRangeCID);
  res = range->SetStart(selNode, selOffset);
  if (NS_FAILED(res)) return res;
  res = range->SetEnd(selNode, selOffset);
  if (NS_FAILED(res)) return res;
  nsCOMPtr<nsIContent> block (do_QueryInterface(mNewBlock));
  if (!block) return NS_ERROR_NO_INTERFACE;
  PRBool nodeBefore, nodeAfter;
  res = mHTMLEditor->mRangeHelper->CompareNodeToRange(block, range, &nodeBefore, &nodeAfter);
  if (NS_FAILED(res)) return res;
  
  if (nodeBefore && nodeAfter)
    return NS_OK;  // selection is inside block
  else if (nodeBefore)
  {
    // selection is after block.  put at end of block.
    nsCOMPtr<nsIDOMNode> tmp = mNewBlock;
    mHTMLEditor->GetLastEditableChild(mNewBlock, address_of(tmp));
    PRUint32 endPoint;
    if (mHTMLEditor->IsTextNode(tmp) || mHTMLEditor->IsContainer(tmp))
    {
      res = nsEditor::GetLengthOfDOMNode(tmp, endPoint);
      if (NS_FAILED(res)) return res;
    }
    else
    {
      nsCOMPtr<nsIDOMNode> tmp2;
      res = nsEditor::GetNodeLocation(tmp, address_of(tmp2), (PRInt32*)&endPoint);
      if (NS_FAILED(res)) return res;
      tmp = tmp2;
      endPoint++;  // want to be after this node
    }
    return aSelection->Collapse(tmp, (PRInt32)endPoint);
  }
  else
  {
    // selection is before block.  put at start of block.
    nsCOMPtr<nsIDOMNode> tmp = mNewBlock;
    mHTMLEditor->GetFirstEditableChild(mNewBlock, address_of(tmp));
    PRInt32 offset;
    if (!(mHTMLEditor->IsTextNode(tmp) || mHTMLEditor->IsContainer(tmp)))
    {
      nsCOMPtr<nsIDOMNode> tmp2;
      res = nsEditor::GetNodeLocation(tmp, address_of(tmp2), &offset);
      if (NS_FAILED(res)) return res;
      tmp = tmp2;
    }
    return aSelection->Collapse(tmp, 0);
  }
}

nsresult 
nsHTMLEditRules::CheckInterlinePosition(nsISelection *aSelection)
{
  if (!aSelection) return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsISelection> selection(aSelection);
  nsCOMPtr<nsISelectionPrivate> selPriv(do_QueryInterface(selection));

  // if the selection isn't collapsed, do nothing.
  PRBool bCollapsed;
  nsresult res = aSelection->GetIsCollapsed(&bCollapsed);
  if (NS_FAILED(res)) return res;
  if (!bCollapsed) return res;

  // get the (collapsed) selection location
  nsCOMPtr<nsIDOMNode> selNode, node;
  PRInt32 selOffset;
  res = mHTMLEditor->GetStartNodeAndOffset(aSelection, address_of(selNode), &selOffset);
  if (NS_FAILED(res)) return res;
  
  // are we after a block?  If so try set caret to following content
  mHTMLEditor->GetPriorHTMLSibling(selNode, selOffset, address_of(node));
  if (node && IsBlockNode(node))
  {
    selPriv->SetInterlinePosition(PR_TRUE);
    return NS_OK;
  }

  // are we before a block?  If so try set caret to prior content
  mHTMLEditor->GetNextHTMLSibling(selNode, selOffset, address_of(node));
  if (node && IsBlockNode(node))
  {
    selPriv->SetInterlinePosition(PR_FALSE);
    return NS_OK;
  }
  
  // are we after a <br>?  If so we want to stick to whatever is after <br>.
  mHTMLEditor->GetPriorHTMLNode(selNode, selOffset, address_of(node), PR_TRUE);
  if (node && nsTextEditUtils::IsBreak(node))
      selPriv->SetInterlinePosition(PR_TRUE);
  return NS_OK;
}

nsresult 
nsHTMLEditRules::AdjustSelection(nsISelection *aSelection, nsIEditor::EDirection aAction)
{
  if (!aSelection) return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsISelection> selection(aSelection);
  nsCOMPtr<nsISelectionPrivate> selPriv(do_QueryInterface(selection));
 
  // if the selection isn't collapsed, do nothing.
  // moose: one thing to do instead is check for the case of
  // only a single break selected, and collapse it.  Good thing?  Beats me.
  PRBool bCollapsed;
  nsresult res = aSelection->GetIsCollapsed(&bCollapsed);
  if (NS_FAILED(res)) return res;
  if (!bCollapsed) return res;

  // get the (collapsed) selection location
  nsCOMPtr<nsIDOMNode> selNode, temp;
  PRInt32 selOffset;
  res = mHTMLEditor->GetStartNodeAndOffset(aSelection, address_of(selNode), &selOffset);
  if (NS_FAILED(res)) return res;
  temp = selNode;
  
  // are we in an editable node?
  while (!mHTMLEditor->IsEditable(selNode))
  {
    // scan up the tree until we find an editable place to be
    res = nsEditor::GetNodeLocation(temp, address_of(selNode), &selOffset);
    if (NS_FAILED(res)) return res;
    if (!selNode) return NS_ERROR_FAILURE;
    temp = selNode;
  }
  
  // make sure we aren't in an empty block - user will see no cursor.  If this
  // is happening, put a <br> in the block if allowed.
  nsCOMPtr<nsIDOMNode> theblock;
  if (IsBlockNode(selNode)) theblock = selNode;
  else theblock = mHTMLEditor->GetBlockNodeParent(selNode);
  PRBool bIsEmptyNode;
  res = mHTMLEditor->IsEmptyNode(theblock, &bIsEmptyNode, PR_FALSE, PR_FALSE);
  if (NS_FAILED(res)) return res;
  // check if br can go into the destination node
  if (bIsEmptyNode && mHTMLEditor->CanContainTag(selNode, NS_LITERAL_STRING("br")))
  {
    nsCOMPtr<nsIDOMElement> rootElement;
    res = mHTMLEditor->GetRootElement(getter_AddRefs(rootElement));
    if (NS_FAILED(res)) return res;
    if (!rootElement) return NS_ERROR_FAILURE;
    nsCOMPtr<nsIDOMNode> rootNode(do_QueryInterface(rootElement));
    if (selNode == rootNode)
    {
      // Our root node is completely empty. Don't add a <br> here.
      // AfterEditInner() will add one for us when it calls
      // CreateBogusNodeIfNeeded()!
      return NS_OK;
    }

    nsCOMPtr<nsIDOMNode> brNode;
    // we know we can skip the rest of this routine given the cirumstance
    return CreateMozBR(selNode, selOffset, address_of(brNode));
  }
  
  // are we in a text node? 
  nsCOMPtr<nsIDOMCharacterData> textNode = do_QueryInterface(selNode);
  if (textNode) 
    return NS_OK; // we LIKE it when we are in a text node.  that RULZ
  
  // do we need to insert a special mozBR?  We do if we are:
  // 1) that block is same block where selection is AND
  // 2) in a collapsed selection AND
  // 3) after a normal (non-moz) br AND
  // 4) that br is the last editable node in it's block 

  nsCOMPtr<nsIDOMNode> nearNode;
  res = mHTMLEditor->GetPriorHTMLNode(selNode, selOffset, address_of(nearNode));
  if (NS_FAILED(res)) return res;
  if (nearNode) 
  {
    // is nearNode also a descendant of same block?
    nsCOMPtr<nsIDOMNode> block, nearBlock;
    if (IsBlockNode(selNode)) block = selNode;
    else block = mHTMLEditor->GetBlockNodeParent(selNode);
    nearBlock = mHTMLEditor->GetBlockNodeParent(nearNode);
    if (block == nearBlock)
    {
      if (nearNode && nsTextEditUtils::IsBreak(nearNode) )
      {   
        if (!IsVisBreak(nearNode))
        {
          // need to insert special moz BR. Why?  Because if we don't
          // the user will see no new line for the break.  Also, things
          // like table cells won't grow in height.
          nsCOMPtr<nsIDOMNode> brNode;
          res = CreateMozBR(selNode, selOffset, address_of(brNode));
          if (NS_FAILED(res)) return res;
          res = nsEditor::GetNodeLocation(brNode, address_of(selNode), &selOffset);
          if (NS_FAILED(res)) return res;
          // selection stays *before* moz-br, sticking to it
          selPriv->SetInterlinePosition(PR_TRUE);
          res = aSelection->Collapse(selNode,selOffset);
          if (NS_FAILED(res)) return res;
        }
        else
        {
          nsCOMPtr<nsIDOMNode> nextNode;
          mHTMLEditor->GetNextHTMLNode(nearNode, address_of(nextNode), PR_TRUE);
          if (nextNode && nsTextEditUtils::IsMozBR(nextNode))
          {
            // selection between br and mozbr.  make it stick to mozbr
            // so that it will be on blank line.   
            selPriv->SetInterlinePosition(PR_TRUE);
          }
        }
      }
    }
  }

  // we aren't in a textnode: are we adjacent to a break or an image?
  res = mHTMLEditor->GetPriorHTMLSibling(selNode, selOffset, address_of(nearNode));
  if (NS_FAILED(res)) return res;
  if (nearNode && (nsTextEditUtils::IsBreak(nearNode)
                   || nsHTMLEditUtils::IsImage(nearNode)
                   || nsHTMLEditUtils::IsHR(nearNode)))
    return NS_OK; // this is a good place for the caret to be
  res = mHTMLEditor->GetNextHTMLSibling(selNode, selOffset, address_of(nearNode));
  if (NS_FAILED(res)) return res;
  if (nearNode && (nsTextEditUtils::IsBreak(nearNode)
                   || nsHTMLEditUtils::IsImage(nearNode)
                   || nsHTMLEditUtils::IsHR(nearNode)))
    return NS_OK; // this is a good place for the caret to be

  // look for a nearby text node.
  // prefer the correct direction.
  res = FindNearSelectableNode(selNode, selOffset, aAction, address_of(nearNode));
  if (NS_FAILED(res)) return res;

  if (nearNode)
  {
    // is the nearnode a text node?
    textNode = do_QueryInterface(nearNode);
    if (textNode)
    {
      PRInt32 offset = 0;
      // put selection in right place:
      if (aAction == nsIEditor::ePrevious)
        textNode->GetLength((PRUint32*)&offset);
      res = aSelection->Collapse(nearNode,offset);
    }
    else  // must be break or image
    {
      res = nsEditor::GetNodeLocation(nearNode, address_of(selNode), &selOffset);
      if (NS_FAILED(res)) return res;
      if (aAction == nsIEditor::ePrevious) selOffset++;  // want to be beyond it if we backed up to it
      res = aSelection->Collapse(selNode, selOffset);
    }
  }
  return res;
}


PRBool
nsHTMLEditRules::IsVisBreak(nsIDOMNode *aNode)
{
  if (!aNode) 
    return PR_FALSE;
  if (!nsTextEditUtils::IsBreak(aNode)) 
    return PR_FALSE;
  // check if there is a later node in block after br
  nsCOMPtr<nsIDOMNode> nextNode;
  mHTMLEditor->GetNextHTMLNode(aNode, address_of(nextNode), PR_TRUE); 
  if (!nextNode) 
    return PR_FALSE;  // this break is trailer in block, it's not visible
  if (IsBlockNode(nextNode))
    return PR_FALSE; // break is right before a block, it's not visible
  return PR_TRUE;
}


nsresult
nsHTMLEditRules::FindNearSelectableNode(nsIDOMNode *aSelNode, 
                                        PRInt32 aSelOffset, 
                                        nsIEditor::EDirection &aDirection,
                                        nsCOMPtr<nsIDOMNode> *outSelectableNode)
{
  if (!aSelNode || !outSelectableNode) return NS_ERROR_NULL_POINTER;
  *outSelectableNode = nsnull;
  nsresult res = NS_OK;
  
  nsCOMPtr<nsIDOMNode> nearNode, curNode;
  if (aDirection == nsIEditor::ePrevious)
    res = mHTMLEditor->GetPriorHTMLNode(aSelNode, aSelOffset, address_of(nearNode));
  else
    res = mHTMLEditor->GetNextHTMLNode(aSelNode, aSelOffset, address_of(nearNode));
  if (NS_FAILED(res)) return res;
  
  if (!nearNode) // try the other direction then
  {
    if (aDirection == nsIEditor::ePrevious)
      aDirection = nsIEditor::eNext;
    else
      aDirection = nsIEditor::ePrevious;
    
    if (aDirection == nsIEditor::ePrevious)
      res = mHTMLEditor->GetPriorHTMLNode(aSelNode, aSelOffset, address_of(nearNode));
    else
      res = mHTMLEditor->GetNextHTMLNode(aSelNode, aSelOffset, address_of(nearNode));
    if (NS_FAILED(res)) return res;
  }
  
  // scan in the right direction until we find an eligible text node,
  // but dont cross any breaks, images, or table elements.
  while (nearNode && !(mHTMLEditor->IsTextNode(nearNode)
                       || IsVisBreak(nearNode)
                       || nsHTMLEditUtils::IsImage(nearNode)))
  {
    curNode = nearNode;
    if (aDirection == nsIEditor::ePrevious)
      res = mHTMLEditor->GetPriorHTMLNode(curNode, address_of(nearNode));
    else
      res = mHTMLEditor->GetNextHTMLNode(curNode, address_of(nearNode));
    if (NS_FAILED(res)) return res;
  }
  
  if (nearNode)
  {
    // dont cross any table elements
    PRBool bInDifTblElems;
    res = InDifferentTableElements(nearNode, aSelNode, &bInDifTblElems);
    if (NS_FAILED(res)) return res;
    if (bInDifTblElems) return NS_OK;  
    
    // otherwise, ok, we have found a good spot to put the selection
    *outSelectableNode = do_QueryInterface(nearNode);
  }
  return res;
}


nsresult
nsHTMLEditRules::InDifferentTableElements(nsIDOMNode *aNode1, nsIDOMNode *aNode2, PRBool *aResult)
{
  NS_ASSERTION(aNode1 && aNode2 && aResult, "null args");
  if (!aNode1 || !aNode2 || !aResult) return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIDOMNode> tn1, tn2, node = aNode1, temp;
  *aResult = PR_FALSE;
  
  while (node && !nsHTMLEditUtils::IsTableElement(node))
  {
    node->GetParentNode(getter_AddRefs(temp));
    node = temp;
  }
  tn1 = node;
  
  node = aNode2;
  while (node && !nsHTMLEditUtils::IsTableElement(node))
  {
    node->GetParentNode(getter_AddRefs(temp));
    node = temp;
  }
  tn2 = node;
  
  *aResult = (tn1 != tn2);
  
  return NS_OK;
}


nsresult 
nsHTMLEditRules::RemoveEmptyNodes()
{
  nsCOMPtr<nsIContentIterator> iter;
  nsCOMPtr<nsISupportsArray> arrayOfEmptyNodes, arrayOfEmptyCites;
  nsCOMPtr<nsISupports> isupports;
  PRUint32 nodeCount,j;
  
  // some general notes on the algorithm used here: the goal is to examine all the
  // nodes in mDocChangeRange, and remove the empty ones.  We do this by using a
  // content iterator to traverse all the nodes in the range, and placing the empty
  // nodes into an array.  After finishing the iteration, we delete the empty nodes
  // in the array.  (they cannot be deleted as we find them becasue that would 
  // invalidate the iterator.)  
  // Since checking to see if a node is empty can be costly for nodes with many
  // descendants, there are some optimizations made.  I rely on the fact that the
  // iterator is post-order: it will visit children of a node before visiting the 
  // parent node.  So if I find that a child node is not empty, I know that it's
  // parent is not empty without even checking.  So I put the parent on a "skipList"
  // which is just a voidArray of nodes I can skip the empty check on.  If I 
  // encounter a node on the skiplist, i skip the processing for that node and replace
  // it's slot in the skiplist with that node's parent.
  // An interseting idea is to go ahead and regard parent nodes that are NOT on the
  // skiplist as being empty (without even doing the IsEmptyNode check) on the theory
  // that if they weren't empty, we would have encountered a non-empty child earlier
  // and thus put this parent node on the skiplist.
  // Unfortunately I can't use that strategy here, because the range may include 
  // some children of a node while excluding others.  Thus I could find all the 
  // _examined_ children empty, but still not have an empty parent.
  
  // make an isupportsArray to hold a list of nodes, and a seperate list for empty mailcites
  nsresult res = NS_NewISupportsArray(getter_AddRefs(arrayOfEmptyNodes));
  if (NS_FAILED(res)) return res;
  res = NS_NewISupportsArray(getter_AddRefs(arrayOfEmptyCites));
  if (NS_FAILED(res)) return res;

  // need an iterator
  iter = do_CreateInstance(kContentIteratorCID);
  if (!iter) return NS_ERROR_NULL_POINTER;
  
  res = iter->Init(mDocChangeRange);
  if (NS_FAILED(res)) return res;
  
  nsVoidArray skipList;

  // check for empty nodes
  while (NS_ENUMERATOR_FALSE == iter->IsDone())
  {
    nsCOMPtr<nsIDOMNode> node, parent;
    nsCOMPtr<nsIContent> content;
    res = iter->CurrentNode(getter_AddRefs(content));
    if (NS_FAILED(res)) return res;
    node = do_QueryInterface(content);
    if (!node) return NS_ERROR_FAILURE;
    node->GetParentNode(getter_AddRefs(parent));
    
    PRInt32 idx = skipList.IndexOf((void*)node);
    if (idx>=0)
    {
      // this node is on our skip list.  Skip processing for this node, 
      // and replace it's value in the skip list with the value of it's parent
      skipList.ReplaceElementAt((void*)parent, idx);
    }
    else
    {
      PRBool bIsCandidate = PR_FALSE;
      PRBool bIsEmptyNode = PR_FALSE;
      PRBool bIsMailCite = PR_FALSE;
      
      // dont delete the body
      if (!nsTextEditUtils::IsBody(node))
      {
        // only consider certain nodes to be empty for purposes of removal
        if (  (bIsMailCite = nsHTMLEditUtils::IsMailCite(node))              ||
              nsTextEditUtils::NodeIsType(node, NS_LITERAL_STRING("a"))      || 
              nsTextEditUtils::NodeIsType(node, NS_LITERAL_STRING("b"))      || 
              nsTextEditUtils::NodeIsType(node, NS_LITERAL_STRING("i"))      || 
              nsTextEditUtils::NodeIsType(node, NS_LITERAL_STRING("u"))      || 
              nsTextEditUtils::NodeIsType(node, NS_LITERAL_STRING("tt"))     || 
              nsTextEditUtils::NodeIsType(node, NS_LITERAL_STRING("s"))      || 
              nsTextEditUtils::NodeIsType(node, NS_LITERAL_STRING("strike")) || 
              nsTextEditUtils::NodeIsType(node, NS_LITERAL_STRING("big"))    || 
              nsTextEditUtils::NodeIsType(node, NS_LITERAL_STRING("small"))  || 
              nsTextEditUtils::NodeIsType(node, NS_LITERAL_STRING("blink"))  || 
              nsTextEditUtils::NodeIsType(node, NS_LITERAL_STRING("sub"))    || 
              nsTextEditUtils::NodeIsType(node, NS_LITERAL_STRING("sup"))    || 
              nsTextEditUtils::NodeIsType(node, NS_LITERAL_STRING("font"))   || 
              nsHTMLEditUtils::IsList(node)      || 
              nsHTMLEditUtils::IsDiv(node)  )
        {
          bIsCandidate = PR_TRUE;
        }
        // these node types are candidates if selection is not in them
        else if (nsHTMLEditUtils::IsParagraph(node) ||
            nsHTMLEditUtils::IsHeader(node)    ||
            nsHTMLEditUtils::IsListItem(node)  ||
            nsHTMLEditUtils::IsBlockquote(node)||
            nsHTMLEditUtils::IsPre(node)       ||
            nsHTMLEditUtils::IsAddress(node) )
        {
          // if it is one of these, dont delete if sel inside.
          // this is so we can create empty headings, etc, for the
          // user to type into.
          PRBool bIsSelInNode;
          res = SelectionEndpointInNode(node, &bIsSelInNode);
          if (NS_FAILED(res)) return res;
          if (!bIsSelInNode)
          {
            bIsCandidate = PR_TRUE;
          }
        }
      }
      
      if (bIsCandidate)
      {
        if (bIsMailCite)  // we delete mailcites even if they have a solo br in them
          res = mHTMLEditor->IsEmptyNode(node, &bIsEmptyNode, PR_TRUE, PR_TRUE);  
        else  // other nodes we require to be empty
          res = mHTMLEditor->IsEmptyNode(node, &bIsEmptyNode, PR_FALSE, PR_TRUE);
        if (NS_FAILED(res)) return res;
        if (bIsEmptyNode)
        {
          if (bIsMailCite)  // mailcites go on a seperate list from other empty nodes
          {
            isupports = do_QueryInterface(node);
            if (!isupports) return NS_ERROR_NULL_POINTER;
            arrayOfEmptyCites->AppendElement(isupports);
          }
          else
          {
            isupports = do_QueryInterface(node);
            if (!isupports) return NS_ERROR_NULL_POINTER;
            arrayOfEmptyNodes->AppendElement(isupports);
          }
        }
      }
      
      if (!bIsEmptyNode)
      {
        // put parent on skip list
        skipList.AppendElement((void*)parent);
      }
    }
    res = iter->Next();
    if (NS_FAILED(res)) return res;
  }
  
  // now delete the empty nodes
  res = arrayOfEmptyNodes->Count(&nodeCount);
  if (NS_FAILED(res)) return res;
  for (j = 0; j < nodeCount; j++)
  {
    isupports = dont_AddRef(arrayOfEmptyNodes->ElementAt(0));
    nsCOMPtr<nsIDOMNode> delNode( do_QueryInterface(isupports ) );
    arrayOfEmptyNodes->RemoveElementAt(0);
    res = mHTMLEditor->DeleteNode(delNode);
    if (NS_FAILED(res)) return res;
  }
  
  // now delete the empty mailcites
  // this is a seperate step because we want to pull out any br's and preserve them.
  res = arrayOfEmptyCites->Count(&nodeCount);
  if (NS_FAILED(res)) return res;
  for (j = 0; j < nodeCount; j++)
  {
    isupports = dont_AddRef(arrayOfEmptyCites->ElementAt(0));
    nsCOMPtr<nsIDOMNode> delNode( do_QueryInterface(isupports ) );
    arrayOfEmptyCites->RemoveElementAt(0);
    PRBool bIsEmptyNode;
    res = mHTMLEditor->IsEmptyNode(delNode, &bIsEmptyNode, PR_FALSE, PR_TRUE);
    if (NS_FAILED(res)) return res;
    if (!bIsEmptyNode)
    {
      // we are deleting a cite that has just a br.  We want to delete cite, 
      // but preserve br.
      nsCOMPtr<nsIDOMNode> parent, brNode;
      PRInt32 offset;
      res = nsEditor::GetNodeLocation(delNode, address_of(parent), &offset);
      if (NS_FAILED(res)) return res;
      res = mHTMLEditor->CreateBR(parent, offset, address_of(brNode));
      if (NS_FAILED(res)) return res;
    }
    res = mHTMLEditor->DeleteNode(delNode);
    if (NS_FAILED(res)) return res;
  }
  
  return res;
}

nsresult
nsHTMLEditRules::SelectionEndpointInNode(nsIDOMNode *aNode, PRBool *aResult)
{
  if (!aNode || !aResult) return NS_ERROR_NULL_POINTER;
  
  *aResult = PR_FALSE;
  
  nsCOMPtr<nsISelection>selection;
  nsresult res = mHTMLEditor->GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  nsCOMPtr<nsISelectionPrivate>selPriv(do_QueryInterface(selection));
  
  nsCOMPtr<nsIEnumerator> enumerator;
  res = selPriv->GetEnumerator(getter_AddRefs(enumerator));
  if (NS_FAILED(res)) return res;
  if (!enumerator) return NS_ERROR_UNEXPECTED;

  for (enumerator->First(); NS_OK!=enumerator->IsDone(); enumerator->Next())
  {
    nsCOMPtr<nsISupports> currentItem;
    res = enumerator->CurrentItem(getter_AddRefs(currentItem));
    if (NS_FAILED(res)) return res;
    if (!currentItem) return NS_ERROR_UNEXPECTED;

    nsCOMPtr<nsIDOMRange> range( do_QueryInterface(currentItem) );
    nsCOMPtr<nsIDOMNode> startParent, endParent;
    range->GetStartContainer(getter_AddRefs(startParent));
    if (startParent)
    {
      if (aNode == startParent.get())
      {
        *aResult = PR_TRUE;
        return NS_OK;
      }
      if (nsHTMLEditUtils::IsDescendantOf(startParent, aNode)) 
      {
        *aResult = PR_TRUE;
        return NS_OK;
      }
    }
    range->GetEndContainer(getter_AddRefs(endParent));
    if (startParent == endParent) continue;
    if (endParent)
    {
      if (aNode == endParent.get()) 
      {
        *aResult = PR_TRUE;
        return NS_OK;
      }
      if (nsHTMLEditUtils::IsDescendantOf(endParent, aNode))
      {
        *aResult = PR_TRUE;
        return NS_OK;
      }
    }
  }
  return res;
}

///////////////////////////////////////////////////////////////////////////
// IsEmptyInline:  return true if aNode is an empty inline container
//                
//                  
PRBool 
nsHTMLEditRules::IsEmptyInline(nsIDOMNode *aNode)
{
  if (aNode && IsInlineNode(aNode) && mHTMLEditor->IsContainer(aNode)) 
  {
    PRBool bEmpty;
    mHTMLEditor->IsEmptyNode(aNode, &bEmpty);
    return bEmpty;
  }
  return PR_FALSE;
}


PRBool 
nsHTMLEditRules::ListIsEmptyLine(nsISupportsArray *arrayOfNodes)
{
  // we have a list of nodes which we are candidates for being moved
  // into a new block.  Determine if it's anything more than a blank line.
  // Look for editable content above and beyond one single BR.
  if (!arrayOfNodes) return PR_TRUE;
  PRUint32 listCount;
  arrayOfNodes->Count(&listCount);
  if (!listCount) return PR_TRUE;
  nsCOMPtr<nsIDOMNode> somenode;
  nsCOMPtr<nsISupports> isupports;
  PRInt32 j, brCount=0;
  arrayOfNodes->Count(&listCount);
  for (j = 0; j < listCount; j++)
  {
    isupports = dont_AddRef(arrayOfNodes->ElementAt(j));
    somenode = do_QueryInterface(isupports);
    if (somenode && mHTMLEditor->IsEditable(somenode))
    {
      if (nsTextEditUtils::IsBreak(somenode))
      {
        // first break doesn't count
        if (brCount) return PR_FALSE;
        brCount++;
      }
      else if (IsEmptyInline(somenode)) 
      {
        // empty inline, keep looking
      }
      else return PR_FALSE;
    }
  }
  return PR_TRUE;
}


nsresult 
nsHTMLEditRules::PopListItem(nsIDOMNode *aListItem, PRBool *aOutOfList)
{
  // check parms
  if (!aListItem || !aOutOfList) 
    return NS_ERROR_NULL_POINTER;
  
  // init out params
  *aOutOfList = PR_FALSE;
  
  nsCOMPtr<nsIDOMNode> curParent;
  nsCOMPtr<nsIDOMNode> curNode( do_QueryInterface(aListItem));
  PRInt32 offset;
  nsresult res = nsEditor::GetNodeLocation(curNode, address_of(curParent), &offset);
  if (NS_FAILED(res)) return res;
    
  if (!nsHTMLEditUtils::IsListItem(curNode))
    return NS_ERROR_FAILURE;
    
  // if it's first or last list item, dont need to split the list
  // otherwise we do.
  nsCOMPtr<nsIDOMNode> curParPar;
  PRInt32 parOffset;
  res = nsEditor::GetNodeLocation(curParent, address_of(curParPar), &parOffset);
  if (NS_FAILED(res)) return res;
  
  PRBool bIsFirstListItem;
  res = mHTMLEditor->IsFirstEditableChild(curNode, &bIsFirstListItem);
  if (NS_FAILED(res)) return res;

  PRBool bIsLastListItem;
  res = mHTMLEditor->IsLastEditableChild(curNode, &bIsLastListItem);
  if (NS_FAILED(res)) return res;
    
  if (!bIsFirstListItem && !bIsLastListItem)
  {
    // split the list
    nsCOMPtr<nsIDOMNode> newBlock;
    res = mHTMLEditor->SplitNode(curParent, offset, getter_AddRefs(newBlock));
    if (NS_FAILED(res)) return res;
  }
  
  if (!bIsFirstListItem) parOffset++;
  
  res = mHTMLEditor->MoveNode(curNode, curParPar, parOffset);
  if (NS_FAILED(res)) return res;
    
  // unwrap list item contents if they are no longer in a list
  if (!nsHTMLEditUtils::IsList(curParPar)
      && nsHTMLEditUtils::IsListItem(curNode)) 
  {
    res = mHTMLEditor->RemoveBlockContainer(curNode);
    if (NS_FAILED(res)) return res;
    *aOutOfList = PR_TRUE;
  }
  return res;
}

nsresult
nsHTMLEditRules::RemoveListStructure(nsIDOMNode *aList)
{
  NS_ENSURE_ARG_POINTER(aList);

  nsresult res;

  nsCOMPtr<nsIDOMNode> child;
  aList->GetFirstChild(getter_AddRefs(child));

  while (child)
  {
    if (nsHTMLEditUtils::IsListItem(child))
    {
      PRBool bOutOfList;
      do
      {
        res = PopListItem(child, &bOutOfList);
        if (NS_FAILED(res)) return res;
      } while (!bOutOfList);   // keep popping it out until it's not in a list anymore
    }
    else if (nsHTMLEditUtils::IsList(child))
    {
      res = RemoveListStructure(child);
      if (NS_FAILED(res)) return res;
    }
    else
    {
      // delete any non- list items for now
      res = mHTMLEditor->DeleteNode(child);
      if (NS_FAILED(res)) return res;
    }
    aList->GetFirstChild(getter_AddRefs(child));
  }
  // delete the now-empty list
  res = mHTMLEditor->RemoveBlockContainer(aList);
  if (NS_FAILED(res)) return res;

  return res;
}


nsresult 
nsHTMLEditRules::ConfirmSelectionInBody()
{
  nsresult res = NS_OK;
  nsCOMPtr<nsIDOMElement> bodyElement;   
  nsCOMPtr<nsIDOMNode> bodyNode; 
  
  // get the body  
  res = mHTMLEditor->GetRootElement(getter_AddRefs(bodyElement));
  if (NS_FAILED(res)) return res;
  if (!bodyElement) return NS_ERROR_UNEXPECTED;
  bodyNode = do_QueryInterface(bodyElement);

  // get the selection
  nsCOMPtr<nsISelection>selection;
  res = mHTMLEditor->GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  
  // get the selection start location
  nsCOMPtr<nsIDOMNode> selNode, temp, parent;
  PRInt32 selOffset;
  res = mHTMLEditor->GetStartNodeAndOffset(selection, address_of(selNode), &selOffset);
  if (NS_FAILED(res)) return res;
  temp = selNode;
  
  // check that selNode is inside body
  while (temp && !nsTextEditUtils::IsBody(temp))
  {
    res = temp->GetParentNode(getter_AddRefs(parent));
    temp = parent;
  }
  
  // if we aren't in the body, force the issue
  if (!temp) 
  {
//    uncomment this to see when we get bad selections
//    NS_NOTREACHED("selection not in body");
    selection->Collapse(bodyNode,0);
  }
  
  // get the selection end location
  res = mHTMLEditor->GetEndNodeAndOffset(selection, address_of(selNode), &selOffset);
  if (NS_FAILED(res)) return res;
  temp = selNode;
  
  // check that selNode is inside body
  while (temp && !nsTextEditUtils::IsBody(temp))
  {
    res = temp->GetParentNode(getter_AddRefs(parent));
    temp = parent;
  }
  
  // if we aren't in the body, force the issue
  if (!temp) 
  {
//    uncomment this to see when we get bad selections
//    NS_NOTREACHED("selection not in body");
    selection->Collapse(bodyNode,0);
  }
  
  return res;
}


nsresult 
nsHTMLEditRules::UpdateDocChangeRange(nsIDOMRange *aRange)
{
  nsresult res = NS_OK;
  
  if (!mDocChangeRange)
  {
    // clone aRange.  
    res = aRange->CloneRange(getter_AddRefs(mDocChangeRange));
    return res;
  }
  else
  {
    PRInt32 result;
    
    // compare starts of ranges
    res = mDocChangeRange->CompareBoundaryPoints(nsIDOMRange::START_TO_START, aRange, &result);
    if (NS_FAILED(res)) return res;
    if (result > 0)  // positive result means mDocChangeRange start is after aRange start
    {
      nsCOMPtr<nsIDOMNode> startNode;
      PRInt32 startOffset;
      res = aRange->GetStartContainer(getter_AddRefs(startNode));
      if (NS_FAILED(res)) return res;
      res = aRange->GetStartOffset(&startOffset);
      if (NS_FAILED(res)) return res;
      res = mDocChangeRange->SetStart(startNode, startOffset);
      if (NS_FAILED(res)) return res;
    }
    
    // compare ends of ranges
    res = mDocChangeRange->CompareBoundaryPoints(nsIDOMRange::END_TO_END, aRange, &result);
    if (NS_FAILED(res)) return res;
    if (result < 0)  // negative result means mDocChangeRange end is before aRange end
    {
      nsCOMPtr<nsIDOMNode> endNode;
      PRInt32 endOffset;
      res = aRange->GetEndContainer(getter_AddRefs(endNode));
      if (NS_FAILED(res)) return res;
      res = aRange->GetEndOffset(&endOffset);
      if (NS_FAILED(res)) return res;
      res = mDocChangeRange->SetEnd(endNode, endOffset);
      if (NS_FAILED(res)) return res;
    }
  }
  return res;
}

nsresult 
nsHTMLEditRules::InsertMozBRIfNeeded(nsIDOMNode *aNode)
{
  if (!aNode) return NS_ERROR_NULL_POINTER;
  if (!IsBlockNode(aNode)) return NS_OK;
  
  PRBool isEmpty;
  nsCOMPtr<nsIDOMNode> brNode;
  nsresult res = mHTMLEditor->IsEmptyNode(aNode, &isEmpty);
  if (NS_FAILED(res)) return res;
  if (isEmpty)
  {
    res = CreateMozBR(aNode, 0, address_of(brNode));
  }
  return res;
}

#ifdef XP_MAC
#pragma mark -
#pragma mark  nsIEditActionListener methods 
#pragma mark -
#endif

NS_IMETHODIMP 
nsHTMLEditRules::WillCreateNode(const nsAString& aTag, nsIDOMNode *aParent, PRInt32 aPosition)
{
  return NS_OK;  
}

NS_IMETHODIMP 
nsHTMLEditRules::DidCreateNode(const nsAString& aTag, 
                               nsIDOMNode *aNode, 
                               nsIDOMNode *aParent, 
                               PRInt32 aPosition, 
                               nsresult aResult)
{
  if (!mListenerEnabled) return NS_OK;
  // assumption that Join keeps the righthand node
  nsresult res = mUtilRange->SelectNode(aNode);
  if (NS_FAILED(res)) return res;
  res = UpdateDocChangeRange(mUtilRange);
  return res;  
}


NS_IMETHODIMP 
nsHTMLEditRules::WillInsertNode(nsIDOMNode *aNode, nsIDOMNode *aParent, PRInt32 aPosition)
{
  return NS_OK;  
}


NS_IMETHODIMP 
nsHTMLEditRules::DidInsertNode(nsIDOMNode *aNode, 
                               nsIDOMNode *aParent, 
                               PRInt32 aPosition, 
                               nsresult aResult)
{
  if (!mListenerEnabled) return NS_OK;
  nsresult res = mUtilRange->SelectNode(aNode);
  if (NS_FAILED(res)) return res;
  res = UpdateDocChangeRange(mUtilRange);
  return res;  
}


NS_IMETHODIMP 
nsHTMLEditRules::WillDeleteNode(nsIDOMNode *aChild)
{
  if (!mListenerEnabled) return NS_OK;
  nsresult res = mUtilRange->SelectNode(aChild);
  if (NS_FAILED(res)) return res;
  res = UpdateDocChangeRange(mUtilRange);
  return res;  
}


NS_IMETHODIMP 
nsHTMLEditRules::DidDeleteNode(nsIDOMNode *aChild, nsresult aResult)
{
  return NS_OK;  
}


NS_IMETHODIMP 
nsHTMLEditRules::WillSplitNode(nsIDOMNode *aExistingRightNode, PRInt32 aOffset)
{
  return NS_OK;  
}


NS_IMETHODIMP 
nsHTMLEditRules::DidSplitNode(nsIDOMNode *aExistingRightNode, 
                              PRInt32 aOffset, 
                              nsIDOMNode *aNewLeftNode, 
                              nsresult aResult)
{
  if (!mListenerEnabled) return NS_OK;
  nsresult res = mUtilRange->SetStart(aNewLeftNode, 0);
  if (NS_FAILED(res)) return res;
  res = mUtilRange->SetEnd(aExistingRightNode, 0);
  if (NS_FAILED(res)) return res;
  res = UpdateDocChangeRange(mUtilRange);
  return res;  
}


NS_IMETHODIMP 
nsHTMLEditRules::WillJoinNodes(nsIDOMNode *aLeftNode, nsIDOMNode *aRightNode, nsIDOMNode *aParent)
{
  if (!mListenerEnabled) return NS_OK;
  // remember split point
  nsresult res = nsEditor::GetLengthOfDOMNode(aLeftNode, mJoinOffset);
  return res;  
}


NS_IMETHODIMP 
nsHTMLEditRules::DidJoinNodes(nsIDOMNode  *aLeftNode, 
                              nsIDOMNode *aRightNode, 
                              nsIDOMNode *aParent, 
                              nsresult aResult)
{
  if (!mListenerEnabled) return NS_OK;
  // assumption that Join keeps the righthand node
  nsresult res = mUtilRange->SetStart(aRightNode, mJoinOffset);
  if (NS_FAILED(res)) return res;
  res = mUtilRange->SetEnd(aRightNode, mJoinOffset);
  if (NS_FAILED(res)) return res;
  res = UpdateDocChangeRange(mUtilRange);
  return res;  
}


NS_IMETHODIMP 
nsHTMLEditRules::WillInsertText(nsIDOMCharacterData *aTextNode, PRInt32 aOffset, const nsAString &aString)
{
  return NS_OK;  
}


NS_IMETHODIMP 
nsHTMLEditRules::DidInsertText(nsIDOMCharacterData *aTextNode, 
                                  PRInt32 aOffset, 
                                  const nsAString &aString, 
                                  nsresult aResult)
{
  if (!mListenerEnabled) return NS_OK;
  PRInt32 length = aString.Length();
  nsCOMPtr<nsIDOMNode> theNode = do_QueryInterface(aTextNode);
  nsresult res = mUtilRange->SetStart(theNode, aOffset);
  if (NS_FAILED(res)) return res;
  res = mUtilRange->SetEnd(theNode, aOffset+length);
  if (NS_FAILED(res)) return res;
  res = UpdateDocChangeRange(mUtilRange);
  return res;  
}


NS_IMETHODIMP 
nsHTMLEditRules::WillDeleteText(nsIDOMCharacterData *aTextNode, PRInt32 aOffset, PRInt32 aLength)
{
  return NS_OK;  
}


NS_IMETHODIMP 
nsHTMLEditRules::DidDeleteText(nsIDOMCharacterData *aTextNode, 
                                  PRInt32 aOffset, 
                                  PRInt32 aLength, 
                                  nsresult aResult)
{
  if (!mListenerEnabled) return NS_OK;
  nsCOMPtr<nsIDOMNode> theNode = do_QueryInterface(aTextNode);
  nsresult res = mUtilRange->SetStart(theNode, aOffset);
  if (NS_FAILED(res)) return res;
  res = mUtilRange->SetEnd(theNode, aOffset);
  if (NS_FAILED(res)) return res;
  res = UpdateDocChangeRange(mUtilRange);
  return res;  
}

NS_IMETHODIMP
nsHTMLEditRules::WillDeleteRange(nsIDOMRange *aRange)
{
  if (!mListenerEnabled) return NS_OK;
  // get the (collapsed) selection location
  return UpdateDocChangeRange(aRange);
}

NS_IMETHODIMP
nsHTMLEditRules::DidDeleteRange(nsIDOMRange *aRange)
{
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLEditRules::WillDeleteSelection(nsISelection *aSelection)
{
  if (!mListenerEnabled) return NS_OK;
  // get the (collapsed) selection location
  nsCOMPtr<nsIDOMNode> selNode;
  PRInt32 selOffset;

  nsresult res = mHTMLEditor->GetStartNodeAndOffset(aSelection, address_of(selNode), &selOffset);
  if (NS_FAILED(res)) return res;
  res = mUtilRange->SetStart(selNode, selOffset);
  if (NS_FAILED(res)) return res;
  res = mHTMLEditor->GetEndNodeAndOffset(aSelection, address_of(selNode), &selOffset);
  if (NS_FAILED(res)) return res;
  res = mUtilRange->SetEnd(selNode, selOffset);
  if (NS_FAILED(res)) return res;
  res = UpdateDocChangeRange(mUtilRange);
  return res;  
}

NS_IMETHODIMP
nsHTMLEditRules::DidDeleteSelection(nsISelection *aSelection)
{
  return NS_OK;
}

// Let's remove all alignment hints in the children of aNode; it can
// be an ALIGN attribute (in case we just remove it) or a CENTER
// element (here we have to remove the container and keep its
// children). We break on tables and don't look at their children.
nsresult
nsHTMLEditRules::RemoveAlignment(nsIDOMNode * aNode, const nsAString & aAlignType, PRBool aChildrenOnly)
{
  if (!aNode) return NS_ERROR_NULL_POINTER;

  if (mHTMLEditor->IsTextNode(aNode) || nsHTMLEditUtils::IsTable(aNode)) return NS_OK;
  nsresult res = NS_OK;

  nsCOMPtr<nsIDOMNode> child = aNode,tmp;
  if (aChildrenOnly)
  {
    aNode->GetFirstChild(getter_AddRefs(child));
  }
  PRBool useCSS;
  mHTMLEditor->GetIsCSSEnabled(&useCSS);

  while (child)
  {
    if (aChildrenOnly) {
      // get the next sibling right now because we could have to remove child
      child->GetNextSibling(getter_AddRefs(tmp));
    }
    else
    {
      tmp = nsnull;
    }
    PRBool isBlock;
    res = mHTMLEditor->NodeIsBlockStatic(child, &isBlock);
    if (NS_FAILED(res)) return res;

    if ((isBlock && !nsHTMLEditUtils::IsDiv(child)) || nsHTMLEditUtils::IsHR(child))
    {
      // the current node is a block element
      nsCOMPtr<nsIDOMElement> curElem = do_QueryInterface(child);
      if (nsHTMLEditUtils::SupportsAlignAttr(child))
      {
        // remove the ALIGN attribute if this element can have it
        res = mHTMLEditor->RemoveAttribute(curElem, NS_LITERAL_STRING("align"));
        if (NS_FAILED(res)) return res;
      }
      if (useCSS)
      {
        if (nsHTMLEditUtils::IsTable(child) || nsHTMLEditUtils::IsHR(child))
        {
          res = mHTMLEditor->SetAttributeOrEquivalent(curElem, NS_LITERAL_STRING("align"), aAlignType); 
        }
        else
        {
          nsAutoString dummyCssValue;
          res = mHTMLEditor->mHTMLCSSUtils->RemoveCSSInlineStyle(child, nsIEditProperty::cssTextAlign, dummyCssValue);
        }
        if (NS_FAILED(res)) return res;
      }
      if (!nsHTMLEditUtils::IsTable(child))
      {
        // unless this is a table, look at children
        res = RemoveAlignment(child, aAlignType, PR_TRUE);
        if (NS_FAILED(res)) return res;
      }
    }
    else if (nsTextEditUtils::NodeIsType(child, NS_LITERAL_STRING("center"))
             || nsHTMLEditUtils::IsDiv(child))
    {
      // this is a CENTER or a DIV element and we have to remove it
      // first remove children's alignment
      res = RemoveAlignment(child, aAlignType, PR_TRUE);
      if (NS_FAILED(res)) return res;

      if (useCSS && nsHTMLEditUtils::IsDiv(child))
      {
        // if we are in CSS mode and if the element is a DIV, let's remove it
        // if it does not carry any style hint (style attr, class or ID)
        nsAutoString dummyCssValue;
        res = mHTMLEditor->mHTMLCSSUtils->RemoveCSSInlineStyle(child, nsIEditProperty::cssTextAlign, dummyCssValue);
        if (NS_FAILED(res)) return res;
        nsCOMPtr<nsIDOMElement> childElt = do_QueryInterface(child);
        PRBool hasStyleOrIdOrClass;
        res = mHTMLEditor->HasStyleOrIdOrClass(childElt, &hasStyleOrIdOrClass);
        if (NS_FAILED(res)) return res;
        if (!hasStyleOrIdOrClass)
        {
          // we may have to insert BRs in first and last position of DIV's children
          // if the nodes before/after are not blocks and not BRs
          res = MakeSureElemStartsOrEndsOnCR(child);
          if (NS_FAILED(res)) return res;
          res = mHTMLEditor->RemoveContainer(child);
          if (NS_FAILED(res)) return res;
        }
      }
      else
      {
        // we may have to insert BRs in first and last position of element's children
        // if the nodes before/after are not blocks and not BRs
        res = MakeSureElemStartsOrEndsOnCR(child);
        if (NS_FAILED(res)) return res;

        // in HTML mode, let's remove the element
        res = mHTMLEditor->RemoveContainer(child);
        if (NS_FAILED(res)) return res;
      }
    }
    child = tmp;
  }
  return NS_OK;
}

// Let's insert a BR as first (resp. last) child of aNode if its
// first (resp. last) child is not a block nor a BR, and if the
// previous (resp. next) sibling is not a block nor a BR
nsresult
nsHTMLEditRules::MakeSureElemStartsOrEndsOnCR(nsIDOMNode *aNode, PRBool aStarts)
{
  if (!aNode) return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIDOMNode> child;
  nsresult res;
  if (aStarts)
  {
    res = mHTMLEditor->GetFirstEditableChild(aNode, address_of(child));
  }
  else
  {
    res = mHTMLEditor->GetLastEditableChild(aNode, address_of(child));
  }
  if (NS_FAILED(res)) return res;
  if (!child) return NS_OK;
  PRBool isChildBlock;
  res = mHTMLEditor->NodeIsBlockStatic(child, &isChildBlock);
  if (NS_FAILED(res)) return res;
  PRBool foundCR = PR_FALSE;
  if (isChildBlock || nsTextEditUtils::IsBreak(child))
  {
    foundCR = PR_TRUE;
  }
  else
  {
    nsCOMPtr<nsIDOMNode> sibling;
    if (aStarts)
    {
      res = mHTMLEditor->GetPriorHTMLSibling(aNode, address_of(sibling));
    }
    else
    {
      res = mHTMLEditor->GetNextHTMLSibling(aNode, address_of(sibling));
    }
    if (NS_FAILED(res)) return res;
    if (sibling)
    {
      PRBool isBlock;
      res = mHTMLEditor->NodeIsBlockStatic(sibling, &isBlock);
      if (NS_FAILED(res)) return res;
      if (isBlock || nsTextEditUtils::IsBreak(sibling))
      {
        foundCR = PR_TRUE;
      }
    }
    else
    {
      foundCR = PR_TRUE;
    }
  }
  if (!foundCR)
  {
    nsCOMPtr<nsIDOMNode> brNode;
    PRInt32 offset = 0;
    if (!aStarts)
    {
      nsCOMPtr<nsIDOMNodeList> childNodes;
      res = aNode->GetChildNodes(getter_AddRefs(childNodes));
      if (NS_FAILED(res)) return res;
      if (!childNodes) return NS_ERROR_NULL_POINTER;
      PRUint32 childCount;
      res = childNodes->GetLength(&childCount);
      if (NS_FAILED(res)) return res;
      offset = childCount;
    }
    res = mHTMLEditor->CreateBR(aNode, offset, address_of(brNode));
    if (NS_FAILED(res)) return res;
  }
  return NS_OK;
}

nsresult
nsHTMLEditRules::MakeSureElemStartsOrEndsOnCR(nsIDOMNode *aNode)
{
  nsresult res = MakeSureElemStartsOrEndsOnCR(aNode, PR_FALSE);
  if (NS_FAILED(res)) return res;
  res = MakeSureElemStartsOrEndsOnCR(aNode, PR_TRUE);
  return res;
}

nsresult
nsHTMLEditRules::AlignBlock(nsIDOMElement * aElement, const nsAString * aAlignType, PRBool aContentsOnly)
{
  if (!aElement) return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIDOMNode> node = do_QueryInterface(aElement);
  PRBool isBlock = IsBlockNode(node);
  if (!isBlock && !nsHTMLEditUtils::IsHR(node)) {
    // we deal only with blocks; early way out
    return NS_OK;
  }

  nsresult res = RemoveAlignment(node, *aAlignType, aContentsOnly);
  if (NS_FAILED(res)) return res;
  NS_NAMED_LITERAL_STRING(attr, "align");
  PRBool useCSS;
  mHTMLEditor->GetIsCSSEnabled(&useCSS);
  if (useCSS) {
    // let's use CSS alignment; we use margin-left and margin-right for tables
    // and text-align for other block-level elements
    res = mHTMLEditor->SetAttributeOrEquivalent(aElement, attr, *aAlignType); 
    if (NS_FAILED(res)) return res;
  }
  else {
    // HTML case; this code is supposed to be called ONLY if the element
    // supports the align attribute but we'll never know...
    if (nsHTMLEditUtils::SupportsAlignAttr(node)) {
      res = mHTMLEditor->SetAttribute(aElement, attr, *aAlignType);
      if (NS_FAILED(res)) return res;
    }
  }
  return NS_OK;
}

nsresult
nsHTMLEditRules::RelativeChangeIndentation(nsIDOMNode *aNode, PRInt8 aRelativeChange)
{
  if ( !( (aRelativeChange==1) || (aRelativeChange==-1) ) )
    return NS_ERROR_ILLEGAL_VALUE;

  nsAutoString value;
  nsresult res;
  mHTMLEditor->mHTMLCSSUtils->GetSpecifiedProperty(aNode, nsIEditProperty::cssMarginLeft, value);
  float f;
  nsIAtom * unit;
  mHTMLEditor->mHTMLCSSUtils->ParseLength(value, &f, &unit);
  if (0 == f) {
    NS_IF_RELEASE(unit);
    nsAutoString defaultLengthUnit;
    mHTMLEditor->mHTMLCSSUtils->GetDefaultLengthUnit(defaultLengthUnit);
    unit = NS_NewAtom(defaultLengthUnit);
  }
  nsAutoString unitString;
  unit->ToString(unitString);
  if      (nsIEditProperty::cssInUnit == unit)
            f += NS_EDITOR_INDENT_INCREMENT_IN * aRelativeChange;
  else if (nsIEditProperty::cssCmUnit == unit)
            f += NS_EDITOR_INDENT_INCREMENT_CM * aRelativeChange;
  else if (nsIEditProperty::cssMmUnit == unit)
            f += NS_EDITOR_INDENT_INCREMENT_MM * aRelativeChange;
  else if (nsIEditProperty::cssPtUnit == unit)
            f += NS_EDITOR_INDENT_INCREMENT_PT * aRelativeChange;
  else if (nsIEditProperty::cssPcUnit == unit)
            f += NS_EDITOR_INDENT_INCREMENT_PC * aRelativeChange;
  else if (nsIEditProperty::cssEmUnit == unit)
            f += NS_EDITOR_INDENT_INCREMENT_EM * aRelativeChange;
  else if (nsIEditProperty::cssExUnit == unit)
            f += NS_EDITOR_INDENT_INCREMENT_EX * aRelativeChange;
  else if (nsIEditProperty::cssPxUnit == unit)
            f += NS_EDITOR_INDENT_INCREMENT_PX * aRelativeChange;
  else if (nsIEditProperty::cssPercentUnit == unit)
            f += NS_EDITOR_INDENT_INCREMENT_PERCENT * aRelativeChange;    

  NS_IF_RELEASE(unit);

  nsCOMPtr<nsIDOMElement> element = do_QueryInterface(aNode);
  if (element) {
    if (0 < f) {
      nsAutoString newValue;
      newValue.AppendFloat(f);
      newValue.Append(unitString);
      mHTMLEditor->mHTMLCSSUtils->SetCSSProperty(element, nsIEditProperty::cssMarginLeft, newValue);
    }
    else {
      mHTMLEditor->mHTMLCSSUtils->RemoveCSSProperty(element, nsIEditProperty::cssMarginLeft, value);
      if (nsHTMLEditUtils::IsDiv(aNode)) {
        // we deal with a DIV ; let's see if it is useless and if we can remove it
        nsCOMPtr<nsIDOMNamedNodeMap> attributeList;
        res = element->GetAttributes(getter_AddRefs(attributeList));
        if (NS_FAILED(res)) return res;
        PRUint32 count;
        attributeList->GetLength(&count);
        if (!count) {
          // the DIV has no attribute at all, let's remove it
          res = mHTMLEditor->RemoveContainer(element);
          if (NS_FAILED(res)) return res;
        }
        else if (1 == count) {
          nsCOMPtr<nsIDOMNode> styleAttributeNode;
          res = attributeList->GetNamedItem(NS_LITERAL_STRING("style"), 
                                            getter_AddRefs(styleAttributeNode));
          if (!styleAttributeNode) {
            res = mHTMLEditor->RemoveContainer(element);
            if (NS_FAILED(res)) return res;
          }
        }
      }
    }
  }
  return NS_OK;
}
