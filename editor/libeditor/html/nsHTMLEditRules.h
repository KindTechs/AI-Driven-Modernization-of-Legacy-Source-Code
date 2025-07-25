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

#ifndef nsHTMLEditRules_h__
#define nsHTMLEditRules_h__

#include "nsTextEditRules.h"
#include "nsIHTMLEditRules.h"
#include "nsIEditActionListener.h"
#include "nsISupportsArray.h"
#include "nsCOMPtr.h"
#include "nsString.h"

class nsISupportsArray;
class nsVoidArray;
class nsIDOMElement;
class nsIEditor;
class nsHTMLEditor;

class nsHTMLEditRules : public nsIHTMLEditRules, public nsTextEditRules, public nsIEditActionListener
{
public:

  NS_DECL_ISUPPORTS_INHERITED
  
            nsHTMLEditRules();
  virtual   ~nsHTMLEditRules();


  // nsIEditRules methods
  NS_IMETHOD Init(nsPlaintextEditor *aEditor, PRUint32 aFlags);
  NS_IMETHOD BeforeEdit(PRInt32 action, nsIEditor::EDirection aDirection);
  NS_IMETHOD AfterEdit(PRInt32 action, nsIEditor::EDirection aDirection);
  NS_IMETHOD WillDoAction(nsISelection *aSelection, nsRulesInfo *aInfo, PRBool *aCancel, PRBool *aHandled);
  NS_IMETHOD DidDoAction(nsISelection *aSelection, nsRulesInfo *aInfo, nsresult aResult);

  // nsIHTMLEditRules methods
  
  NS_IMETHOD GetListState(PRBool *aMixed, PRBool *aOL, PRBool *aUL, PRBool *aDL);
  NS_IMETHOD GetListItemState(PRBool *aMixed, PRBool *aLI, PRBool *aDT, PRBool *aDD);
  NS_IMETHOD GetIndentState(PRBool *aCanIndent, PRBool *aCanOutdent);
  NS_IMETHOD GetAlignment(PRBool *aMixed, nsIHTMLEditor::EAlignment *aAlign);
  NS_IMETHOD GetParagraphState(PRBool *aMixed, nsAString &outFormat);

  // nsIEditActionListener methods
  
  NS_IMETHOD WillCreateNode(const nsAString& aTag, nsIDOMNode *aParent, PRInt32 aPosition);
  NS_IMETHOD DidCreateNode(const nsAString& aTag, nsIDOMNode *aNode, nsIDOMNode *aParent, PRInt32 aPosition, nsresult aResult);
  NS_IMETHOD WillInsertNode(nsIDOMNode *aNode, nsIDOMNode *aParent, PRInt32 aPosition);
  NS_IMETHOD DidInsertNode(nsIDOMNode *aNode, nsIDOMNode *aParent, PRInt32 aPosition, nsresult aResult);
  NS_IMETHOD WillDeleteNode(nsIDOMNode *aChild);
  NS_IMETHOD DidDeleteNode(nsIDOMNode *aChild, nsresult aResult);
  NS_IMETHOD WillSplitNode(nsIDOMNode *aExistingRightNode, PRInt32 aOffset);
  NS_IMETHOD DidSplitNode(nsIDOMNode *aExistingRightNode, PRInt32 aOffset, nsIDOMNode *aNewLeftNode, nsresult aResult);
  NS_IMETHOD WillJoinNodes(nsIDOMNode *aLeftNode, nsIDOMNode *aRightNode, nsIDOMNode *aParent);
  NS_IMETHOD DidJoinNodes(nsIDOMNode  *aLeftNode, nsIDOMNode *aRightNode, nsIDOMNode *aParent, nsresult aResult);
  NS_IMETHOD WillInsertText(nsIDOMCharacterData *aTextNode, PRInt32 aOffset, const nsAString &aString);
  NS_IMETHOD DidInsertText(nsIDOMCharacterData *aTextNode, PRInt32 aOffset, const nsAString &aString, nsresult aResult);
  NS_IMETHOD WillDeleteText(nsIDOMCharacterData *aTextNode, PRInt32 aOffset, PRInt32 aLength);
  NS_IMETHOD DidDeleteText(nsIDOMCharacterData *aTextNode, PRInt32 aOffset, PRInt32 aLength, nsresult aResult);
  NS_IMETHOD WillDeleteRange(nsIDOMRange *aRange);
  NS_IMETHOD DidDeleteRange(nsIDOMRange *aRange);
  NS_IMETHOD WillDeleteSelection(nsISelection *aSelection);
  NS_IMETHOD DidDeleteSelection(nsISelection *aSelection);

protected:

  enum RulesEndpoint
  {
    kStart,
    kEnd
  };

  enum BRLocation
  {
    kBeforeBlock,
    kBlockEnd
  };



  // nsHTMLEditRules implementation methods
  nsresult WillInsert(nsISelection *aSelection, PRBool *aCancel);
#ifdef XXX_DEAD_CODE
  nsresult DidInsert(nsISelection *aSelection, nsresult aResult);
#endif
  nsresult WillInsertText(  PRInt32          aAction,
                            nsISelection *aSelection, 
                            PRBool          *aCancel,
                            PRBool          *aHandled,
                            const nsAString *inString,
                            nsAString       *outString,
                            PRInt32          aMaxLength);
  nsresult WillLoadHTML(nsISelection *aSelection, PRBool *aCancel);
  nsresult WillInsertBreak(nsISelection *aSelection, PRBool *aCancel, PRBool *aHandled);
  nsresult DidInsertBreak(nsISelection *aSelection, nsresult aResult);
  nsresult WillDeleteSelection(nsISelection *aSelection, nsIEditor::EDirection aAction, 
                               PRBool *aCancel, PRBool *aHandled);
  nsresult JoinBlocks(nsISelection *aSelection, nsCOMPtr<nsIDOMNode> *aLeftBlock, 
                      nsCOMPtr<nsIDOMNode> *aRightBlock, PRBool *aCanceled);
  nsresult MoveBlock(nsISelection *aSelection, nsIDOMNode *aNewParent, PRInt32 aOffset = -1);
  nsresult MoveNodeSmart(nsIDOMNode *aSource, nsIDOMNode *aDest, PRInt32 *aOffset);
  nsresult MoveContents(nsIDOMNode *aSource, nsIDOMNode *aDest, PRInt32 *aOffset);
  nsresult DeleteNonTableElements(nsIDOMNode *aNode);
  nsresult WillMakeList(nsISelection *aSelection, const nsAString *aListType, PRBool aEntireList, const nsAString *aBulletType, PRBool *aCancel, PRBool *aHandled, const nsAString *aItemType=nsnull);
  nsresult WillRemoveList(nsISelection *aSelection, PRBool aOrderd, PRBool *aCancel, PRBool *aHandled);
  nsresult WillIndent(nsISelection *aSelection, PRBool *aCancel, PRBool *aHandled);
  nsresult WillCSSIndent(nsISelection *aSelection, PRBool *aCancel, PRBool *aHandled);
  nsresult WillHTMLIndent(nsISelection *aSelection, PRBool *aCancel, PRBool *aHandled);
  nsresult WillOutdent(nsISelection *aSelection, PRBool *aCancel, PRBool *aHandled);
  nsresult WillAlign(nsISelection *aSelection, const nsAString *alignType, PRBool *aCancel, PRBool *aHandled);
  nsresult WillMakeDefListItem(nsISelection *aSelection, const nsAString *aBlockType, PRBool aEntireList, PRBool *aCancel, PRBool *aHandled);
  nsresult WillMakeBasicBlock(nsISelection *aSelection, const nsAString *aBlockType, PRBool *aCancel, PRBool *aHandled);
  nsresult DidMakeBasicBlock(nsISelection *aSelection, nsRulesInfo *aInfo, nsresult aResult);
  nsresult AlignInnerBlocks(nsIDOMNode *aNode, const nsAString *alignType);
  nsresult AlignBlockContents(nsIDOMNode *aNode, const nsAString *alignType);
  PRBool   IsFormatNode(nsIDOMNode *aNode);
  nsresult AppendInnerFormatNodes(nsISupportsArray *aArray, nsIDOMNode *aNode);
  nsresult GetFormatString(nsIDOMNode *aNode, nsAString &outFormat);
  nsresult GetInnerContent(nsIDOMNode *aNode, nsISupportsArray *outArrayOfNodes, PRInt32 *aIndex, PRBool aList = PR_TRUE, PRBool aTble = PR_TRUE);
  nsCOMPtr<nsIDOMNode> IsInListItem(nsIDOMNode *aNode);
  nsresult ReturnInHeader(nsISelection *aSelection, nsIDOMNode *aHeader, nsIDOMNode *aTextNode, PRInt32 aOffset);
  nsresult ReturnInParagraph(nsISelection *aSelection, nsIDOMNode *aHeader, nsIDOMNode *aTextNode, PRInt32 aOffset, PRBool *aCancel, PRBool *aHandled);
  nsresult SplitParagraph(nsIDOMNode *aPara,
                          nsIDOMNode *aBRNode, 
                          nsISelection *aSelection,
                          nsCOMPtr<nsIDOMNode> *aSelNode, 
                          PRInt32 *aOffset);
  nsresult ReturnInListItem(nsISelection *aSelection, nsIDOMNode *aHeader, nsIDOMNode *aTextNode, PRInt32 aOffset);
  nsresult AfterEditInner(PRInt32 action, nsIEditor::EDirection aDirection);
  nsresult RemovePartOfBlock(nsIDOMNode *aBlock, 
                             nsIDOMNode *aStartChild, 
                             nsIDOMNode *aEndChild,
                             nsCOMPtr<nsIDOMNode> *aLeftNode = 0,
                             nsCOMPtr<nsIDOMNode> *aRightNode = 0);
  nsresult ConvertListType(nsIDOMNode *aList, nsCOMPtr<nsIDOMNode> *outList, const nsAString& aListType, const nsAString& aItemType);
  nsresult CreateStyleForInsertText(nsISelection *aSelection, nsIDOMDocument *aDoc);
  nsresult IsEmptyBlock(nsIDOMNode *aNode, 
                        PRBool *outIsEmptyBlock, 
                        PRBool aMozBRDoesntCount = PR_FALSE,
                        PRBool aListItemsNotEmpty = PR_FALSE);
  nsresult CheckForEmptyBlock(nsIDOMNode *aStartNode, 
                              nsIDOMNode *aBodyNode,
                              nsISelection *aSelection,
                              PRBool *aHandled);
  nsresult CheckForWhitespaceDeletion(nsCOMPtr<nsIDOMNode> *ioStartNode,
                                      PRInt32 *ioStartOffset,
                                      PRInt32 aAction,
                                      PRBool *aHandled);
  nsresult CheckForInvisibleBR(nsIDOMNode *aBlock, nsHTMLEditRules::BRLocation aWhere, 
                               nsCOMPtr<nsIDOMNode> *outBRNode, PRInt32 aOffset=0);
  PRBool IsFirstNode(nsIDOMNode *aNode);
  PRBool IsLastNode(nsIDOMNode *aNode);
#ifdef XXX_DEAD_CODE
  PRBool AtStartOfBlock(nsIDOMNode *aNode, PRInt32 aOffset, nsIDOMNode *aBlock);
  PRBool AtEndOfBlock(nsIDOMNode *aNode, PRInt32 aOffset, nsIDOMNode *aBlock);
#endif
  nsresult NormalizeSelection(nsISelection *inSelection);
  nsresult GetPromotedPoint(RulesEndpoint aWhere, nsIDOMNode *aNode, PRInt32 aOffset, 
                            PRInt32 actionID, nsCOMPtr<nsIDOMNode> *outNode, PRInt32 *outOffset);
  nsresult GetPromotedRanges(nsISelection *inSelection, 
                             nsCOMPtr<nsISupportsArray> *outArrayOfRanges, 
                             PRInt32 inOperationType);
  nsresult PromoteRange(nsIDOMRange *inRange, PRInt32 inOperationType);
  nsresult GetNodesForOperation(nsISupportsArray *inArrayOfRanges, 
                                nsCOMPtr<nsISupportsArray> *outArrayOfNodes, 
                                PRInt32 inOperationType,
                                PRBool aDontTouchContent=PR_FALSE);
  nsresult GetChildNodesForOperation(nsIDOMNode *inNode, 
                                     nsCOMPtr<nsISupportsArray> *outArrayOfNodes);
  nsresult GetNodesFromSelection(nsISelection *selection,
                                       PRInt32 operation,
                                       nsCOMPtr<nsISupportsArray> *arrayOfNodes,
                                       PRBool aDontTouchContent=PR_FALSE);
  nsresult GetListActionNodes(nsCOMPtr<nsISupportsArray> *outArrayOfNodes, PRBool aEntireList, PRBool aDontTouchContent=PR_FALSE);
  nsresult GetDefinitionListItemTypes(nsIDOMNode *aNode, PRBool &aDT, PRBool &aDD);
  nsresult GetParagraphFormatNodes(nsCOMPtr<nsISupportsArray> *outArrayOfNodes, PRBool aDontTouchContent=PR_FALSE);
  nsresult LookInsideDivBQandList(nsISupportsArray *aNodeArray);
  nsresult BustUpInlinesAtRangeEndpoints(nsRangeStore &inRange);
  nsresult BustUpInlinesAtBRs(nsIDOMNode *inNode, 
                                   nsCOMPtr<nsISupportsArray> *outArrayOfNodes);
  nsCOMPtr<nsIDOMNode> GetHighestInlineParent(nsIDOMNode* aNode);
  nsresult MakeTransitionList(nsISupportsArray *inArrayOfNodes, 
                                   nsVoidArray *inTransitionArray);
  nsresult RemoveBlockStyle(nsISupportsArray *arrayOfNodes);
  nsresult ApplyBlockStyle(nsISupportsArray *arrayOfNodes, const nsAString *aBlockTag);
  nsresult MakeBlockquote(nsISupportsArray *arrayOfNodes);
  nsresult SplitAsNeeded(const nsAString *aTag, nsCOMPtr<nsIDOMNode> *inOutParent, PRInt32 *inOutOffset);
  nsresult AddTerminatingBR(nsIDOMNode *aBlock);
  nsresult JoinNodesSmart( nsIDOMNode *aNodeLeft, 
                           nsIDOMNode *aNodeRight, 
                           nsCOMPtr<nsIDOMNode> *aOutMergeParent, 
                           PRInt32 *aOutMergeOffset);
  nsresult GetTopEnclosingMailCite(nsIDOMNode *aNode, nsCOMPtr<nsIDOMNode> *aOutCiteNode, PRBool aPlaintext);
  nsresult PopListItem(nsIDOMNode *aListItem, PRBool *aOutOfList);
  nsresult RemoveListStructure(nsIDOMNode *aList);
  nsresult AdjustSpecialBreaks(PRBool aSafeToAskFrames = PR_FALSE);
  nsresult AdjustWhitespace(nsISelection *aSelection);
  nsresult PinSelectionToNewBlock(nsISelection *aSelection);
  nsresult CheckInterlinePosition(nsISelection *aSelection);
  nsresult AdjustSelection(nsISelection *aSelection, nsIEditor::EDirection aAction);
  nsresult FindNearSelectableNode(nsIDOMNode *aSelNode, 
                                  PRInt32 aSelOffset, 
                                  nsIEditor::EDirection &aDirection,
                                  nsCOMPtr<nsIDOMNode> *outSelectableNode);
  nsresult InDifferentTableElements(nsIDOMNode *aNode1, nsIDOMNode *aNode2, PRBool *aResult);
  nsresult RemoveEmptyNodes();
  nsresult SelectionEndpointInNode(nsIDOMNode *aNode, PRBool *aResult);
  nsresult UpdateDocChangeRange(nsIDOMRange *aRange);
  nsresult ConfirmSelectionInBody();
  nsresult InsertMozBRIfNeeded(nsIDOMNode *aNode);
  PRBool   IsVisBreak(nsIDOMNode *aNode);
  PRBool   IsEmptyInline(nsIDOMNode *aNode);
  PRBool   ListIsEmptyLine(nsISupportsArray *arrayOfNodes);
  nsresult RemoveAlignment(nsIDOMNode * aNode, const nsAString & aAlignType, PRBool aChildrenOnly);
  nsresult MakeSureElemStartsOrEndsOnCR(nsIDOMNode *aNode, PRBool aStarts);
  nsresult MakeSureElemStartsOrEndsOnCR(nsIDOMNode *aNode);
  nsresult AlignBlock(nsIDOMElement * aElement, const nsAString * aAlignType, PRBool aContentsOnly);
  nsresult RelativeChangeIndentation(nsIDOMNode *aNode, PRInt8 aRelativeChange);

// data members
protected:
  nsHTMLEditor           *mHTMLEditor;
  nsCOMPtr<nsIDOMRange>   mDocChangeRange;
  PRBool                  mListenerEnabled;
  PRBool                  mReturnInEmptyLIKillsList;
  nsCOMPtr<nsIDOMRange>   mUtilRange;
  PRUint32                mJoinOffset;  // need to remember an int across willJoin/didJoin...
  nsCOMPtr<nsIDOMNode>    mNewBlock;

};

nsresult NS_NewHTMLEditRules(nsIEditRules** aInstancePtrResult);

#endif //nsHTMLEditRules_h__

