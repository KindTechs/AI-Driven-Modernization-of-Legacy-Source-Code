/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
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
#ifndef nsBlockFrame_h___
#define nsBlockFrame_h___

#include "nsHTMLContainerFrame.h"
#include "nsHTMLParts.h"
#include "nsAbsoluteContainingBlock.h"
#include "nsLineBox.h"

class nsBlockReflowState;
class nsBulletFrame;
class nsLineBox;
class nsFirstLineFrame;
class nsILineIterator;
class nsIntervalSet;
/**
 * Child list name indices
 * @see #GetAdditionalChildListName()
 */
#define NS_BLOCK_FRAME_FLOATER_LIST_INDEX   0
#define NS_BLOCK_FRAME_BULLET_LIST_INDEX    1
#define NS_BLOCK_FRAME_ABSOLUTE_LIST_INDEX  2
#define NS_BLOCK_FRAME_LAST_LIST_INDEX      NS_BLOCK_FRAME_ABSOLUTE_LIST_INDEX

#define nsBlockFrameSuper nsHTMLContainerFrame

#define NS_BLOCK_FRAME_CID \
 { 0xa6cf90df, 0x15b3, 0x11d2,{0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

extern const nsIID kBlockFrameCID;

/*
 * Base class for block and inline frames.
 * The block frame has an additional named child list:
 * - "Absolute-list" which contains the absolutely positioned frames
 *
 * @see nsLayoutAtoms::absoluteList
 */ 
class nsBlockFrame : public nsBlockFrameSuper
{
public:
  typedef nsLineList::iterator                  line_iterator;
  typedef nsLineList::const_iterator            const_line_iterator;
  typedef nsLineList::reverse_iterator          reverse_line_iterator;
  typedef nsLineList::const_reverse_iterator    const_reverse_line_iterator;

protected:
  line_iterator begin_lines() { return mLines.begin(); }
  line_iterator end_lines() { return mLines.end(); }
  const_line_iterator begin_lines() const { return mLines.begin(); }
  const_line_iterator end_lines() const { return mLines.end(); }
  reverse_line_iterator rbegin_lines() { return mLines.rbegin(); }
  reverse_line_iterator rend_lines() { return mLines.rend(); }
  const_reverse_line_iterator rbegin_lines() const { return mLines.rbegin(); }
  const_reverse_line_iterator rend_lines() const { return mLines.rend(); }

public:
  friend nsresult NS_NewBlockFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame, PRUint32 aFlags);

  // nsISupports
  NS_IMETHOD  QueryInterface(const nsIID& aIID, void** aInstancePtr);

  // nsIFrame
  NS_IMETHOD Init(nsIPresContext*  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIStyleContext* aContext,
                  nsIFrame*        aPrevInFlow);
  NS_IMETHOD SetInitialChildList(nsIPresContext* aPresContext,
                                 nsIAtom*        aListName,
                                 nsIFrame*       aChildList);
  NS_IMETHOD  AppendFrames(nsIPresContext* aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aFrameList);
  NS_IMETHOD  InsertFrames(nsIPresContext* aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aPrevFrame,
                           nsIFrame*       aFrameList);
  NS_IMETHOD  RemoveFrame(nsIPresContext* aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aOldFrame);
  NS_IMETHOD FirstChild(nsIPresContext* aPresContext,
                        nsIAtom*        aListName,
                        nsIFrame**      aFirstChild) const;
  NS_IMETHOD GetAdditionalChildListName(PRInt32   aIndex,
                                        nsIAtom** aListName) const;
  NS_IMETHOD Destroy(nsIPresContext* aPresContext);
  NS_IMETHOD IsSplittable(nsSplittableType& aIsSplittable) const;
  NS_IMETHOD IsPercentageBase(PRBool& aBase) const;
  NS_IMETHOD Paint(nsIPresContext*      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect,
                   nsFramePaintLayer    aWhichLayer,
                   PRUint32             aFlags = 0);
  NS_IMETHOD GetFrameType(nsIAtom** aType) const;
#ifdef DEBUG
  NS_IMETHOD List(nsIPresContext* aPresContext, FILE* out, PRInt32 aIndent) const;
  NS_IMETHOD GetFrameName(nsAString& aResult) const;
  NS_IMETHOD SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const;
  NS_IMETHOD VerifyTree() const;
#endif
  NS_IMETHOD GetFrameForPoint(nsIPresContext* aPresContext, const nsPoint& aPoint, nsFramePaintLayer aWhichLayer, nsIFrame** aFrame);
  NS_IMETHOD HandleEvent(nsIPresContext* aPresContext, 
                         nsGUIEvent*     aEvent,
                         nsEventStatus*  aEventStatus);
  NS_IMETHOD ReflowDirtyChild(nsIPresShell* aPresShell, nsIFrame* aChild);

  NS_IMETHOD IsVisibleForPainting(nsIPresContext *     aPresContext, 
                                  nsIRenderingContext& aRenderingContext,
                                  PRBool               aCheckVis,
                                  PRBool*              aIsVisible);

  NS_IMETHOD IsEmpty(PRBool aIsQuirkMode, PRBool aIsPre, PRBool* aResult);

  // nsIHTMLReflow
  NS_IMETHOD Reflow(nsIPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  NS_IMETHOD AttributeChanged(nsIPresContext* aPresContext,
                              nsIContent*     aChild,
                              PRInt32         aNameSpaceID,
                              nsIAtom*        aAttribute,
                              PRInt32         aModType, 
                              PRInt32         aHint);

#ifdef DO_SELECTION
  NS_IMETHOD  HandleEvent(nsIPresContext* aPresContext,
                          nsGUIEvent* aEvent,
                          nsEventStatus* aEventStatus);

  NS_IMETHOD  HandleDrag(nsIPresContext* aPresContext, 
                         nsGUIEvent*     aEvent,
                         nsEventStatus*  aEventStatus);

  nsIFrame * FindHitFrame(nsBlockFrame * aBlockFrame, 
                          const nscoord aX, const nscoord aY,
                          const nsPoint & aPoint);

#endif

  virtual void DeleteChildsNextInFlow(nsIPresContext* aPresContext,
                                      nsIFrame* aNextInFlow);

  /** return the topmost block child based on y-index.
    * almost always the first or second line, if there is one.
    * accounts for lines that hold only compressed white space, etc.
    */
  nsIFrame* GetTopBlockChild();

  // returns true on success and false if aFoundLine is set to end_lines()
  PRBool FindLineFor(nsIFrame* aFrame,
                     PRBool* aIsFloaterResult,
                     line_iterator* aFoundLine);

  static nsresult GetCurrentLine(nsBlockReflowState *aState, nsLineBox **aOutCurrentLine);

  static void CombineRects(const nsRect& r1, nsRect& r2);

  inline nscoord GetAscent() { return mAscent; }

protected:
  nsBlockFrame();
  virtual ~nsBlockFrame();

  nsIStyleContext* GetFirstLetterStyle(nsIPresContext* aPresContext);

  /**
   * GetClosestLine will return the line that VERTICALLY owns the point closest to aPoint.y
   * aOrigin is the offset for this block frame to its frame.
   * aPoint is the point to search for.
   * aClosestLine is the result.
   */
  nsresult GetClosestLine(nsILineIterator *aLI, 
                             const nsPoint &aOrigin, 
                             const nsPoint &aPoint, 
                             PRInt32 &aClosestLine);

  void SetFlags(PRUint32 aFlags) {
    mState &= ~NS_BLOCK_FLAGS_MASK;
    mState |= aFlags;
  }

  PRBool HaveOutsideBullet() const {
#ifdef DEBUG
    if(mState & NS_BLOCK_FRAME_HAS_OUTSIDE_BULLET) {
      NS_ASSERTION(mBullet,"NS_BLOCK_FRAME_HAS_OUTSIDE_BULLET flag set and no mBullet");
    }
#endif
    return 0 != (mState & NS_BLOCK_FRAME_HAS_OUTSIDE_BULLET);
  }

  /** move the frames contained by aLine by aDY
    * if aLine is a block, it's child floaters are added to the state manager
    */
  void SlideLine(nsBlockReflowState& aState,
                 nsLineBox* aLine, nscoord aDY);

  /** grab overflow lines from this block's prevInFlow, and make them
    * part of this block's mLines list.
    * @return PR_TRUE if any lines were drained.
    */
  PRBool DrainOverflowLines(nsIPresContext* aPresContext);

  virtual PRIntn GetSkipSides() const;

  virtual void ComputeFinalSize(const nsHTMLReflowState& aReflowState,
                                nsBlockReflowState&  aState,
                                nsHTMLReflowMetrics& aMetrics);

  /** add the frames in aFrameList to this block after aPrevSibling
    * this block thinks in terms of lines, but the frame construction code
    * knows nothing about lines at all. So we need to find the line that
    * contains aPrevSibling and add aFrameList after aPrevSibling on that line.
    * new lines are created as necessary to handle block data in aFrameList.
    */
  nsresult AddFrames(nsIPresContext* aPresContext,
                     nsIFrame* aFrameList,
                     nsIFrame* aPrevSibling);

  /** move the frame list rooted at aFrame into this as a child
    * assumes prev/next sibling pointers will be or have been set elsewhere
    * changes aFrame's parent to be this, and reparents aFrame's view and stylecontext.
    */
  void FixParentAndView(nsIPresContext* aPresContext, nsIFrame* aFrame);

  /** does all the real work for removing aDeletedFrame from this
    * finds the line containing aFrame.
    * handled continued frames
    * marks lines dirty as needed
    */
  nsresult DoRemoveFrame(nsIPresContext* aPresContext,
                         nsIFrame* aDeletedFrame);


  /** set up the conditions necessary for an initial reflow */
  nsresult PrepareInitialReflow(nsBlockReflowState& aState);

  /** set up the conditions necessary for an styleChanged reflow */
  nsresult PrepareStyleChangedReflow(nsBlockReflowState& aState);

  /** set up the conditions necessary for an incremental reflow.
    * the primary task is to mark the minimumly sufficient lines dirty. 
    */
  nsresult PrepareChildIncrementalReflow(nsBlockReflowState& aState);

  /**
   * Retarget an inline incremental reflow from continuing frames that
   * will be destroyed.
   *
   * @param aState |aState.mNextRCFrame| contains the next frame in
   * the reflow path; this will be ``rewound'' to either the target
   * frame's primary frame, or to the first continuation frame after a
   * ``hard break''. In other words, it will be set to the closest
   * continuation which will not be destroyed by the unconstrained
   * reflow. The remaining frames in the reflow path for
   * |aState.mReflowState.reflowCommand| will be altered similarly.
   *
   * @param aLine is initially the line box that contains the target
   * frame. It will be ``rewound'' in lockstep with
   * |aState.mNextRCFrame|.
   *
   * @param aPrevInFlow points to the target frame's prev-in-flow.
   */
  void RetargetInlineIncrementalReflow(nsBlockReflowState &aState,
                                       line_iterator &aLine,
                                       nsIFrame *aPrevInFlow);

  /** set up the conditions necessary for an resize reflow
    * the primary task is to mark the minimumly sufficient lines dirty. 
    */
  nsresult PrepareResizeReflow(nsBlockReflowState& aState);

  /** reflow all lines that have been marked dirty */
  nsresult ReflowDirtyLines(nsBlockReflowState& aState);

  //----------------------------------------
  // Methods for line reflow
  /**
   * Reflow a line.  
   * @param aState           the current reflow state
   * @param aLine            the line to reflow.  can contain a single block frame
   *                         or contain 1 or more inline frames.
   * @param aKeepReflowGoing [OUT] indicates whether the caller should continue to reflow more lines
   * @param aDamageDirtyArea if PR_TRUE, do extra work to mark the changed areas as damaged for painting
   *                         this indicates that frames may have changed size, for example
   */
  nsresult ReflowLine(nsBlockReflowState& aState,
                      line_iterator aLine,
                      PRBool* aKeepReflowGoing,
                      PRBool aDamageDirtyArea = PR_FALSE);

  nsresult PlaceLine(nsBlockReflowState& aState,
                     nsLineLayout& aLineLayout,
                     line_iterator aLine,
                     PRBool* aKeepReflowGoing,
                     PRBool aUpdateMaximumWidth);

  /**
   * Mark |aLine| dirty, and, if necessary because of possible
   * pull-up, mark the previous line dirty as well.
   */
  nsresult MarkLineDirty(line_iterator aLine);

  // XXX blech
  void PostPlaceLine(nsBlockReflowState& aState,
                     nsLineBox* aLine,
                     const nsSize& aMaxElementSize);

  void ComputeLineMaxElementSize(nsBlockReflowState& aState,
                                 nsLineBox* aLine,
                                 nsSize* aMaxElementSize);

  // XXX where to go
  PRBool ShouldJustifyLine(nsBlockReflowState& aState,
                           line_iterator aLine);

  void DeleteLine(nsBlockReflowState& aState,
                  nsLineList::iterator aLine,
                  nsLineList::iterator aLineEnd);

  //----------------------------------------
  // Methods for individual frame reflow

  PRBool ShouldApplyTopMargin(nsBlockReflowState& aState,
                              nsLineBox* aLine);

  nsresult ReflowBlockFrame(nsBlockReflowState& aState,
                            line_iterator aLine,
                            PRBool* aKeepGoing);

  nsresult ReflowInlineFrames(nsBlockReflowState& aState,
                              line_iterator aLine,
                              PRBool* aKeepLineGoing,
                              PRBool aDamageDirtyArea,
                              PRBool aUpdateMaximumWidth = PR_FALSE);

  nsresult DoReflowInlineFrames(nsBlockReflowState& aState,
                                nsLineLayout& aLineLayout,
                                line_iterator aLine,
                                PRBool* aKeepReflowGoing,
                                PRUint8* aLineReflowStatus,
                                PRBool aUpdateMaximumWidth,
                                PRBool aDamageDirtyArea);

  nsresult DoReflowInlineFramesAuto(nsBlockReflowState& aState,
                                    line_iterator aLine,
                                    PRBool* aKeepReflowGoing,
                                    PRUint8* aLineReflowStatus,
                                    PRBool aUpdateMaximumWidth,
                                    PRBool aDamageDirtyArea);

  nsresult DoReflowInlineFramesMalloc(nsBlockReflowState& aState,
                                      line_iterator aLine,
                                      PRBool* aKeepReflowGoing,
                                      PRUint8* aLineReflowStatus,
                                      PRBool aUpdateMaximumWidth,
                                      PRBool aDamageDirtyArea);

  nsresult ReflowInlineFrame(nsBlockReflowState& aState,
                             nsLineLayout& aLineLayout,
                             line_iterator aLine,
                             nsIFrame* aFrame,
                             PRUint8* aLineReflowStatus);

  nsresult ReflowFloater(nsBlockReflowState& aState,
                         nsPlaceholderFrame* aPlaceholder,
                         nsRect& aCombinedRectResult,
                         nsMargin& aMarginResult,
                         nsMargin& aComputedOffsetsResult);

  //----------------------------------------
  // Methods for pushing/pulling lines/frames

  virtual nsresult CreateContinuationFor(nsBlockReflowState& aState,
                                         nsLineBox* aLine,
                                         nsIFrame* aFrame,
                                         PRBool& aMadeNewFrame);

  nsresult SplitLine(nsBlockReflowState& aState,
                     nsLineLayout& aLineLayout,
                     line_iterator aLine,
                     nsIFrame* aFrame);

  nsresult PullFrame(nsBlockReflowState& aState,
                     line_iterator aLine,
                     PRBool     aDamageDeletedLine,
                     nsIFrame*& aFrameResult);

  nsresult PullFrameFrom(nsBlockReflowState& aState,
                         nsLineBox* aToLine,
                         nsLineList& aFromContainer,
                         nsLineList::iterator aFromLine,
                         PRBool aUpdateGeometricParent,
                         PRBool aDamageDeletedLines,
                         nsIFrame*& aFrameResult);

  void PushLines(nsBlockReflowState& aState,
                 nsLineList::iterator aLineBefore);

  //----------------------------------------
  //XXX
  virtual void PaintChildren(nsIPresContext*      aPresContext,
                             nsIRenderingContext& aRenderingContext,
                             const nsRect&        aDirtyRect,
                             nsFramePaintLayer    aWhichLayer,
                             PRUint32             aFlags = 0);

  void PaintFloaters(nsIPresContext* aPresContext,
                     nsIRenderingContext& aRenderingContext,
                     const nsRect& aDirtyRect);

  void RememberFloaterDamage(nsBlockReflowState& aState,
                             nsLineBox* aLine,
                             const nsRect& aOldCombinedArea);

  void PropagateFloaterDamage(nsBlockReflowState& aState,
                              nsLineBox* aLine,
                              nscoord aDeltaY);

  void BuildFloaterList();

  //----------------------------------------
  // List handling kludge

  void RenumberLists(nsIPresContext* aPresContext);

  PRBool RenumberListsInBlock(nsIPresContext* aPresContext,
                              nsBlockFrame* aContainerFrame,
                              PRInt32* aOrdinal,
                              PRInt32 aDepth);

  PRBool RenumberListsFor(nsIPresContext* aPresContext, nsIFrame* aKid, PRInt32* aOrdinal, PRInt32 aDepth);

  PRBool FrameStartsCounterScope(nsIFrame* aFrame);

  nsresult UpdateBulletPosition(nsBlockReflowState& aState);

  void ReflowBullet(nsBlockReflowState& aState,
                    nsHTMLReflowMetrics& aMetrics);

  //----------------------------------------

  nsLineList* GetOverflowLines(nsIPresContext* aPresContext,
                               PRBool          aRemoveProperty) const;

  nsresult SetOverflowLines(nsIPresContext* aPresContext,
                            nsLineList*     aOverflowLines);

  nsIFrame* LastChild();

#ifdef NS_DEBUG
  PRBool IsChild(nsIPresContext* aPresContext, nsIFrame* aFrame);
  void VerifyLines(PRBool aFinalCheckOK);
  void VerifyOverflowSituation(nsIPresContext* aPresContext);
  PRInt32 GetDepth() const;
#endif

  // Ascent of our first line to support 'vertical-align: baseline' in table-cells
  nscoord mAscent;

  nsLineList mLines;

  // List of all floaters in this block
  nsFrameList mFloaters;

  // XXX_fix_me: subclass one more time!
  // For list-item frames, this is the bullet frame.
  nsBulletFrame* mBullet;

  friend class nsBlockReflowState;

private:
  nsAbsoluteContainingBlock mAbsoluteContainer;


#ifdef DEBUG
public:
  static PRBool gLamePaintMetrics;
  static PRBool gLameReflowMetrics;
  static PRBool gNoisy;
  static PRBool gNoisyDamageRepair;
  static PRBool gNoisyMaxElementSize;
  static PRBool gNoisyReflow;
  static PRBool gReallyNoisyReflow;
  static PRBool gNoisySpaceManager;
  static PRBool gVerifyLines;
  static PRBool gDisableResizeOpt;

  static PRInt32 gNoiseIndent;

  static const char* kReflowCommandType[];

protected:
  static void InitDebugFlags();
#endif
};

#endif /* nsBlockFrame_h___ */

