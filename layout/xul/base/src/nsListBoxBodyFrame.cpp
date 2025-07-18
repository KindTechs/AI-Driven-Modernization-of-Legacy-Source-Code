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

#include "nsListBoxBodyFrame.h"

#include "nsListBoxLayout.h"

#include "nsCOMPtr.h"
#include "nsGridRowGroupLayout.h"
#include "nsISupportsArray.h"
#include "nsIServiceManager.h"
#include "nsXULAtoms.h"
#include "nsHTMLAtoms.h"
#include "nsIContent.h"
#include "nsINameSpaceManager.h"
#include "nsIDocument.h"
#include "nsIBindingManager.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOMMouseEvent.h"
#include "nsIDOMText.h"
#include "nsIDOMElement.h"
#include "nsIDOMNodeList.h"
#include "nsCSSFrameConstructor.h"
#include "nsScrollPortFrame.h"
#include "nsIScrollableFrame.h"
#include "nsIScrollbarFrame.h"
#include "nsIScrollableView.h"
#include "nsIStyleContext.h"
#include "nsIRenderingContext.h"
#include "nsIDeviceContext.h"
#include "nsIFontMetrics.h"
#include "nsITimer.h"
#include "nsITimerCallback.h"

/////////////// nsListScrollSmoother //////////////////

/* A mediator used to smooth out scrolling. It works by seeing if 
 * we have time to scroll the amount of rows requested. This is determined
 * by measuring how long it takes to scroll a row. If we can scroll the 
 * rows in time we do so. If not we start a timer and skip the request. We
 * do this until the timer finally first because the user has stopped moving
 * the mouse. Then do all the queued requests in on shot.
 */

#ifdef XP_MAC
#pragma mark -
#endif

// the longest amount of time that can go by before the use
// notices it as a delay.
#define USER_TIME_THRESHOLD 150000

// how long it takes to layout a single row inital value.
// we will time this after we scroll a few rows.
#define TIME_PER_ROW_INITAL  50000

// if we decide we can't layout the rows in the amount of time. How long
// do we wait before checking again?
#define SMOOTH_INTERVAL 100

class nsListScrollSmoother : public nsITimerCallback
{
public:
  NS_DECL_ISUPPORTS

  nsListScrollSmoother(nsListBoxBodyFrame* aOuter);
  virtual ~nsListScrollSmoother();

  // nsITimerCallback
  NS_IMETHOD_(void) Notify(nsITimer *timer);

  void Start();
  void Stop();
  PRBool IsRunning();

  nsCOMPtr<nsITimer> mRepeatTimer;
  PRBool mDelta;
  nsListBoxBodyFrame* mOuter;
}; 

nsListScrollSmoother::nsListScrollSmoother(nsListBoxBodyFrame* aOuter)
{
  NS_INIT_REFCNT();
  mDelta = 0;
  mOuter = aOuter;
}

nsListScrollSmoother::~nsListScrollSmoother()
{
  Stop();
}

NS_IMETHODIMP_(void)
nsListScrollSmoother::Notify(nsITimer *timer)
{
  Stop();

  NS_ASSERTION(mOuter, "mOuter is null, see bug #68365");
  if (!mOuter) return;

  // actually do some work.
  mOuter->InternalPositionChangedCallback();
}

PRBool
nsListScrollSmoother::IsRunning()
{
  return mRepeatTimer ? PR_TRUE : PR_FALSE;
}

void
nsListScrollSmoother::Start()
{
  Stop();
  mRepeatTimer = do_CreateInstance("@mozilla.org/timer;1");
  mRepeatTimer->Init(this, SMOOTH_INTERVAL);
}

void
nsListScrollSmoother::Stop()
{
  if ( mRepeatTimer ) {
    mRepeatTimer->Cancel();
    mRepeatTimer = nsnull;
  }
}

NS_IMPL_ISUPPORTS1(nsListScrollSmoother, nsITimerCallback)

/////////////// nsListBoxBodyFrame //////////////////

nsListBoxBodyFrame::nsListBoxBodyFrame(nsIPresShell* aPresShell, PRBool aIsRoot, nsIBoxLayout* aLayoutManager)
  : nsBoxFrame(aPresShell, aIsRoot, aLayoutManager),
    mRowCount(-1),
    mRowHeight(0),
    mRowHeightWasSet(PR_FALSE),
    mAvailableHeight(0),
    mStringWidth(-1),
    mTopFrame(nsnull),
    mBottomFrame(nsnull),
    mLinkupFrame(nsnull),
    mRowsToPrepend(0),
    mCurrentIndex(0),
    mOldIndex(0),
    mScrolling(PR_FALSE),
    mAdjustScroll(PR_FALSE),
    mYPosition(0),
    mScrollSmoother(nsnull),
    mTimePerRow(TIME_PER_ROW_INITAL),
    mReflowCallbackPosted(PR_FALSE)
{
}

nsListBoxBodyFrame::~nsListBoxBodyFrame()
{
  NS_IF_RELEASE(mScrollSmoother);

#if USE_TIMER_TO_DELAY_SCROLLING
  StopScrollTracking();
  mAutoScrollTimer = nsnull;
#endif

}

////////// nsISupports /////////////////

NS_IMETHODIMP_(nsrefcnt) 
nsListBoxBodyFrame::AddRef(void)
{
  return NS_OK;
}

NS_IMETHODIMP_(nsrefcnt)
nsListBoxBodyFrame::Release(void)
{
  return NS_OK;
}

//
// QueryInterface
//
NS_INTERFACE_MAP_BEGIN(nsListBoxBodyFrame)
  NS_INTERFACE_MAP_ENTRY(nsIListBoxObject)
  NS_INTERFACE_MAP_ENTRY(nsIScrollbarMediator)
  NS_INTERFACE_MAP_ENTRY(nsIReflowCallback)
NS_INTERFACE_MAP_END_INHERITING(nsBoxFrame)


////////// nsIFrame /////////////////

NS_IMETHODIMP
nsListBoxBodyFrame::Init(nsIPresContext* aPresContext, nsIContent* aContent,
                         nsIFrame* aParent, nsIStyleContext* aContext, nsIFrame* aPrevInFlow)
{
  nsresult rv = nsBoxFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);

  float p2t;
  aPresContext->GetScaledPixelsToTwips(&p2t);
  mOnePixel = NSIntPixelsToTwips(1, p2t);
  
  nsIFrame* box = nsnull;
  aParent->GetParent(&box);
  if (!box)
    return rv;

  nsCOMPtr<nsIScrollableFrame> scrollFrame(do_QueryInterface(box));
  if (!scrollFrame)
    return rv;

  nsIScrollableView* scrollableView;
  scrollFrame->GetScrollableView(aPresContext, &scrollableView);
  scrollableView->SetScrollProperties(NS_SCROLL_PROPERTY_ALWAYS_BLIT);

  nsIBox* verticalScrollbar;
  scrollFrame->GetScrollbarBox(PR_TRUE, &verticalScrollbar);
  if (!verticalScrollbar) {
    NS_ERROR("Unable to install the scrollbar mediator on the listbox widget. You must be using GFX scrollbars.");
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIScrollbarFrame> scrollbarFrame(do_QueryInterface(verticalScrollbar));
  scrollbarFrame->SetScrollbarMediator(this);

  nsBoxLayoutState boxLayoutState(aPresContext);

  const nsStyleFont* font = (const nsStyleFont*)aContext->GetStyleData(eStyleStruct_Font);
  nsCOMPtr<nsIDeviceContext> dc;
  aPresContext->GetDeviceContext(getter_AddRefs(dc));
  nsCOMPtr<nsIFontMetrics> fm;
  dc->GetMetricsFor(font->mFont, *getter_AddRefs(fm));
  fm->GetHeight(mRowHeight);

  return rv;

}

NS_IMETHODIMP
nsListBoxBodyFrame::Destroy(nsIPresContext* aPresContext)
{
  // make sure we cancel any posted callbacks.
  if (mReflowCallbackPosted) {
     nsCOMPtr<nsIPresShell> shell;
     aPresContext->GetShell(getter_AddRefs(shell));
     shell->CancelReflowCallback(this);
  }

  return nsBoxFrame::Destroy(aPresContext);
}

NS_IMETHODIMP
nsListBoxBodyFrame::AttributeChanged(nsIPresContext* aPresContext, nsIContent* aChild,
                                     PRInt32 aNameSpaceID, nsIAtom* aAttribute, 
                                     PRInt32 aModType, PRInt32 aHint)
{
  nsresult rv = NS_OK;
   
  if (aAttribute == nsXULAtoms::rows) {
    nsAutoString rows;
    mContent->GetAttr(kNameSpaceID_None, nsXULAtoms::rows, rows);
    
    if (!rows.IsEmpty()) {
      PRInt32 dummy;
      PRInt32 count = rows.ToInteger(&dummy);
      float t2p;
      aPresContext->GetTwipsToPixels(&t2p);
      PRInt32 rowHeight = GetRowHeightTwips();
      rowHeight = NSTwipsToIntPixels(rowHeight, t2p);
      nsAutoString value;
      value.AppendInt(rowHeight*count);
      mContent->SetAttr(kNameSpaceID_None, nsXULAtoms::minheight, value, PR_FALSE);

      nsBoxLayoutState state(aPresContext);
      MarkDirty(state);
    }
  }
  else
    rv = nsBoxFrame::AttributeChanged(aPresContext, aChild, aNameSpaceID, aAttribute, aModType, aHint);

  return rv;
 
}

/////////// nsIBox ///////////////

NS_IMETHODIMP
nsListBoxBodyFrame::NeedsRecalc()
{
  mStringWidth = -1;
  return nsBoxFrame::NeedsRecalc();
}

/////////// nsBox ///////////////

NS_IMETHODIMP
nsListBoxBodyFrame::DoLayout(nsBoxLayoutState& aBoxLayoutState)
{
  if (mScrolling)
    aBoxLayoutState.SetDisablePainting(PR_TRUE);

  nsresult rv = nsBoxFrame::DoLayout(aBoxLayoutState);

  if (mScrolling)
    aBoxLayoutState.SetDisablePainting(PR_FALSE);

  // if we are scrolled and the row height changed
  // make sure we are scrolled to a correct index.
  if (mAdjustScroll) 
     PostReflowCallback();

  return rv;
}

///////////// nsIScrollbarMediator ///////////////

NS_IMETHODIMP
nsListBoxBodyFrame::PositionChanged(PRInt32 aOldIndex, PRInt32& aNewIndex)
{ 
  if (mScrolling)
    return NS_OK;

  PRInt32 oldTwipIndex, newTwipIndex;
  oldTwipIndex = mCurrentIndex*mRowHeight;
  newTwipIndex = (aNewIndex*mOnePixel);
  PRInt32 twipDelta = newTwipIndex > oldTwipIndex ? newTwipIndex - oldTwipIndex : oldTwipIndex - newTwipIndex;

  PRInt32 rowDelta = twipDelta / mRowHeight;
  PRInt32 remainder = twipDelta % mRowHeight;
  if (remainder > (mRowHeight/2))
    rowDelta++;

  if (rowDelta == 0)
    return NS_OK;

  // update the position to be row based.

  PRInt32 newIndex = newTwipIndex > oldTwipIndex ? mCurrentIndex + rowDelta : mCurrentIndex - rowDelta;
  //aNewIndex = newIndex*mRowHeight/mOnePixel;

  nsListScrollSmoother* smoother = GetSmoother();

  // if we can't scroll the rows in time then start a timer. We will eat
  // events until the user stops moving and the timer stops.
  if (smoother->IsRunning() || rowDelta*mTimePerRow > USER_TIME_THRESHOLD) {

     smoother->Stop();

     nsCOMPtr<nsIPresShell> shell;
     mPresContext->GetShell(getter_AddRefs(shell));
     shell->FlushPendingNotifications(PR_FALSE);

     smoother->mDelta = newTwipIndex > oldTwipIndex ? rowDelta : -rowDelta;

     smoother->Start();

     return NS_OK;
  }

  smoother->Stop();

  mCurrentIndex = newIndex;
  smoother->mDelta = 0;
  
  if (mCurrentIndex < 0) {
    mCurrentIndex = 0;
    return NS_OK;
  }

  return InternalPositionChanged(newTwipIndex < oldTwipIndex, rowDelta);
}

NS_IMETHODIMP
nsListBoxBodyFrame::VisibilityChanged(PRBool aVisible)
{
  if (!aVisible && mCurrentIndex > 0)
    EnsureIndexIsVisible(0);

  return NS_OK;
}

NS_IMETHODIMP
nsListBoxBodyFrame::ScrollbarButtonPressed(PRInt32 aOldIndex, PRInt32 aNewIndex)
{
  if (aOldIndex == aNewIndex)
    return NS_OK;
  if (aNewIndex < aOldIndex)
    mCurrentIndex--;
  else mCurrentIndex++;
  if (mCurrentIndex < 0) {
    mCurrentIndex = 0;
    return NS_OK;
  }
  InternalPositionChanged(aNewIndex < aOldIndex, 1);

  return NS_OK;
}

///////////// nsIReflowCallback ///////////////

NS_IMETHODIMP
nsListBoxBodyFrame::ReflowFinished(nsIPresShell* aPresShell, PRBool* aFlushFlag)
{
  nsBoxLayoutState state(mPresContext);

  // now create or destroy any rows as needed
  CreateRows(state);

  // keep scrollbar in sync
  if (mAdjustScroll) {
     VerticalScroll(mYPosition);
     mAdjustScroll = PR_FALSE;
  }

  // if the row height changed then mark everything as a style change. 
  // That will dirty the entire listbox
  if (mRowHeightWasSet) {
     MarkStyleChange(state);
     PRInt32 pos = mCurrentIndex * mRowHeight;
     if (mYPosition != pos) 
       mAdjustScroll = PR_TRUE;
    mRowHeightWasSet = PR_FALSE;
  }

  mReflowCallbackPosted = PR_FALSE;
  *aFlushFlag = PR_TRUE;

  return NS_OK;
}

///////// nsIListBoxObject ///////////////

NS_IMETHODIMP
nsListBoxBodyFrame::GetListboxBody(nsIListBoxObject * *aListboxBody)
{
  *aListboxBody = this;
  NS_IF_ADDREF(*aListboxBody);
  return NS_OK;
}

NS_IMETHODIMP
nsListBoxBodyFrame::GetRowCount(PRInt32* aResult)
{
  *aResult = GetRowCount();
  return NS_OK;
}

NS_IMETHODIMP
nsListBoxBodyFrame::GetNumberOfVisibleRows(PRInt32 *aResult)
{
  *aResult= mRowHeight ? GetAvailableHeight() / mRowHeight : 0;
  return NS_OK;
}

NS_IMETHODIMP
nsListBoxBodyFrame::GetIndexOfFirstVisibleRow(PRInt32 *aResult)
{
  *aResult = mCurrentIndex;
  return NS_OK;
}

NS_IMETHODIMP
nsListBoxBodyFrame::EnsureIndexIsVisible(PRInt32 aRowIndex)
{
  NS_ASSERTION(aRowIndex >= 0, "Ensure row is visible called with a negative number!");
  if (aRowIndex < 0)
    return NS_ERROR_ILLEGAL_VALUE;

  PRInt32 rows = 0;
  if (mRowHeight)
    rows = GetAvailableHeight()/mRowHeight;
  PRInt32 bottomIndex = mCurrentIndex + rows;
  
  // if row is visible, ignore
  if (mCurrentIndex <= aRowIndex && aRowIndex < bottomIndex)
    return NS_OK;

  // Check to be sure we're not scrolling off the bottom of the tree
  PRInt32 delta;

  PRBool up = aRowIndex < mCurrentIndex;
  if (up) {
    delta = mCurrentIndex - aRowIndex;
    mCurrentIndex = aRowIndex;
  }
  else {
    // Bring it just into view.
    delta = 1 + (aRowIndex-bottomIndex);
    mCurrentIndex += delta; 
  }

  InternalPositionChanged(up, delta);
  return NS_OK;
}

NS_IMETHODIMP
nsListBoxBodyFrame::ScrollToIndex(PRInt32 aRowIndex)
{
  return DoScrollToIndex(aRowIndex);
}

NS_IMETHODIMP
nsListBoxBodyFrame::ScrollByLines(PRInt32 aNumLines)
{
  PRInt32 scrollIndex, visibleRows;
  GetIndexOfFirstVisibleRow(&scrollIndex);
  GetNumberOfVisibleRows(&visibleRows);

  scrollIndex += aNumLines;
  
  if (scrollIndex < 0)
    scrollIndex = 0;
  else {
    PRInt32 numRows = GetRowCount();
    PRInt32 lastPageTopRow = numRows - visibleRows;
    if (scrollIndex > lastPageTopRow)
      scrollIndex = lastPageTopRow;
  }
  
  ScrollToIndex(scrollIndex);

  // we have to do a sync update for mac because if we scroll too quickly
  // w/out going back to the main event loop we can easily scroll the wrong
  // bits and it looks like garbage (bug 63465).
  nsIFrame* frame = nsnull;
  if (NS_FAILED(QueryInterface(NS_GET_IID(nsIFrame), (void **)&frame)))
    return NS_ERROR_FAILURE;
    
  nsIView* view = nsnull;
  frame->GetView(mPresContext, &view);
  if (!view) {
    nsIFrame* frameWithView;
    frame->GetParentWithView(mPresContext, &frameWithView);
    if (frameWithView)
      frameWithView->GetView(mPresContext, &view);
    else
      return NS_ERROR_FAILURE;
  }
  if (view) {
    nsCOMPtr<nsIViewManager> vm;
    if (view->GetViewManager(*getter_AddRefs(vm)) && nsnull != vm) {
      // I'd use Composite here, but it doesn't always work.
      // vm->Composite();
      vm->ForceUpdate();
  
    }      
  }
     
  return NS_OK;
}

// walks the DOM to get the zero-based row index of the content
NS_IMETHODIMP
nsListBoxBodyFrame::GetIndexOfItem(nsIDOMElement* aItem, PRInt32* _retval)
{
  if (!aItem)
    return NS_ERROR_NULL_POINTER;
  
  *_retval = 0;
  nsCOMPtr<nsIContent> itemContent(do_QueryInterface(aItem));

  nsCOMPtr<nsIContent> listbox;
  mContent->GetBindingParent(getter_AddRefs(listbox));

  PRInt32 childCount = 0;
  listbox->ChildCount(childCount);

  for (PRInt32 childIndex = 0; childIndex < childCount; childIndex++) {
    nsCOMPtr<nsIContent> child;
    listbox->ChildAt(childIndex, *getter_AddRefs(child));
    
    nsCOMPtr<nsIAtom> childTag;
    child->GetTag(*getter_AddRefs(childTag));

    // we hit a treerow, count it
    if (childTag == nsXULAtoms::listitem) {
      // is this it?
      if (child == itemContent)
        return NS_OK;
      
      ++(*_retval);
    }
  }

  // not found
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsListBoxBodyFrame::GetItemAtIndex(PRInt32 aIndex, nsIDOMElement** _retval)
{
  if (aIndex < 0)
    return NS_ERROR_ILLEGAL_VALUE;
  
  nsCOMPtr<nsIContent> listbox;
  mContent->GetBindingParent(getter_AddRefs(listbox));

  PRInt32 childCount = 0;
  listbox->ChildCount(childCount);

  PRInt32 itemCount = 0;
  for (PRInt32 childIndex = 0; childIndex < childCount; childIndex++) {
    nsCOMPtr<nsIContent> child;
    listbox->ChildAt(childIndex, *getter_AddRefs(child));
    
    nsCOMPtr<nsIAtom> childTag;
    child->GetTag(*getter_AddRefs(childTag));

    // we hit a treerow, count it
    if (childTag == nsXULAtoms::listitem) {
      // is this it?
      if (itemCount == aIndex) {
        nsCOMPtr<nsIDOMElement> result(do_QueryInterface(child));
        *_retval = result;
        NS_IF_ADDREF(*_retval);
        return NS_OK;
      }
      ++itemCount;
    }
  }

  // not found
  return NS_ERROR_FAILURE;
}

/////////// nsListBoxBodyFrame ///////////////

PRInt32
nsListBoxBodyFrame::GetRowCount()
{
  if (mRowCount < 0)
    ComputeTotalRowCount();
  return mRowCount;
}

PRInt32
nsListBoxBodyFrame::GetFixedRowSize()
{
  PRInt32 dummy;

  nsAutoString rows;
  mContent->GetAttr(kNameSpaceID_None, nsXULAtoms::rows, rows);
  if (!rows.IsEmpty())
    return rows.ToInteger(&dummy);
 
  mContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::size, rows);

  if (!rows.IsEmpty())
    return rows.ToInteger(&dummy);

  return -1;
}

void
nsListBoxBodyFrame::SetRowHeight(nscoord aRowHeight)
{ 
  if (aRowHeight > mRowHeight) { 
    mRowHeight = aRowHeight;
    
    nsAutoString rows;
    mContent->GetAttr(kNameSpaceID_None, nsXULAtoms::rows, rows);
    if (rows.IsEmpty())
      mContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::size, rows);
    
    if (!rows.IsEmpty()) {
      PRInt32 dummy;
      PRInt32 count = rows.ToInteger(&dummy);
      float t2p;
      mPresContext->GetTwipsToPixels(&t2p);
      PRInt32 rowHeight = NSTwipsToIntPixels(aRowHeight, t2p);
      nsAutoString value;
      value.AppendInt(rowHeight*count);
      mContent->SetAttr(kNameSpaceID_None, nsXULAtoms::minheight, value, PR_FALSE);
    }

    // signal we need to dirty everything 
    // and we want to be notified after reflow
    // so we can create or destory rows as needed
    mRowHeightWasSet = PR_TRUE;
    PostReflowCallback();
  } 
}

nscoord
nsListBoxBodyFrame::GetAvailableHeight()
{
  nsIBox* box;
  GetParentBox(&box);
  if (!box)
    return 0;

  nsRect contentRect;
  box->GetContentRect(contentRect);
  return contentRect.height;
}

nscoord
nsListBoxBodyFrame::GetYPosition()
{
  return mYPosition;
}

nscoord
nsListBoxBodyFrame::ComputeIntrinsicWidth(nsBoxLayoutState& aBoxLayoutState)
{
  if (mStringWidth != -1)
    return mStringWidth;

  nscoord largestWidth = 0;

  PRInt32 index = 0;
  nsCOMPtr<nsIDOMElement> firstRowEl;
  GetItemAtIndex(index, getter_AddRefs(firstRowEl));
  nsCOMPtr<nsIContent> firstRowContent(do_QueryInterface(firstRowEl));

  if (firstRowContent) {
    nsCOMPtr<nsIStyleContext> styleContext;
    aBoxLayoutState.GetPresContext()->ResolveStyleContextFor(firstRowContent, nsnull,
                                                             getter_AddRefs(styleContext));

    nscoord width = 0;
    nsMargin margin(0,0,0,0);

    nsStyleBorderPadding  bPad;
    styleContext->GetBorderPaddingFor(bPad);
    bPad.GetBorderPadding(margin);

    width += (margin.left + margin.right);

    const nsStyleMargin* styleMargin = (const nsStyleMargin*)styleContext->GetStyleData(eStyleStruct_Margin);
    styleMargin->GetMargin(margin);
    width += (margin.left + margin.right);

    nsCOMPtr<nsIContent> listbox;
    mContent->GetBindingParent(getter_AddRefs(listbox));

    PRInt32 childCount;
    listbox->ChildCount(childCount);

    nsCOMPtr<nsIContent> child;
    for (PRInt32 i = 0; i < childCount && i < 100; ++i) {
      listbox->ChildAt(i, *getter_AddRefs(child));

      nsCOMPtr<nsIAtom> tag;
      child->GetTag(*getter_AddRefs(tag));
      if (tag == nsXULAtoms::listitem) {
        nsIPresContext* presContext = aBoxLayoutState.GetPresContext();
        nsIRenderingContext* rendContext = aBoxLayoutState.GetReflowState()->rendContext;
        if (rendContext) {
          nsAutoString value;
          nsCOMPtr<nsIContent> textChild;
          PRInt32 textCount;
          child->ChildCount(textCount);
          for (PRInt32 j = 0; j < textCount; ++j) {
            child->ChildAt(j, *getter_AddRefs(textChild));
            nsCOMPtr<nsIDOMText> text(do_QueryInterface(textChild));
            if (text) {
              nsAutoString data;
              text->GetData(data);
              value += data;
            }
          }
          const nsStyleFont* font = (const nsStyleFont*)styleContext->GetStyleData(eStyleStruct_Font);
          nsCOMPtr<nsIDeviceContext> dc;
          presContext->GetDeviceContext(getter_AddRefs(dc));
          nsCOMPtr<nsIFontMetrics> fm;
          dc->GetMetricsFor(font->mFont, *getter_AddRefs(fm));
          rendContext->SetFont(fm);

          nscoord textWidth;
          rendContext->GetWidth(value, textWidth);
          textWidth += width;

          if (textWidth > largestWidth) 
            largestWidth = textWidth;
        }
      }
    }
  }

  mStringWidth = largestWidth;
  return mStringWidth;
}

void
nsListBoxBodyFrame::ComputeTotalRowCount()
{
  nsCOMPtr<nsIContent> listbox;
  mContent->GetBindingParent(getter_AddRefs(listbox));

  PRInt32 childCount;
  listbox->ChildCount(childCount);

  mRowCount = 0;
  for (PRInt32 i = 0; i < childCount; i++) {
    nsCOMPtr<nsIContent> childContent;
    listbox->ChildAt(i, *getter_AddRefs(childContent));
    nsCOMPtr<nsIAtom> tag;
    childContent->GetTag(*getter_AddRefs(tag));
    if (tag == nsXULAtoms::listitem)
      ++mRowCount;
  }
}

void
nsListBoxBodyFrame::PostReflowCallback()
{
  if (!mReflowCallbackPosted) {
    mReflowCallbackPosted = PR_TRUE;
    nsCOMPtr<nsIPresShell> shell;
    mPresContext->GetShell(getter_AddRefs(shell));
    shell->PostReflowCallback(this);
  }
}

////////// scrolling

NS_IMETHODIMP
nsListBoxBodyFrame::DoScrollToIndex(PRInt32 aRowIndex, PRBool aForceDestruct)
{
  if (( aRowIndex < 0 ) || (mRowHeight == 0))
    return NS_OK;
    
  PRInt32 newIndex = aRowIndex;
  PRInt32 delta = mCurrentIndex > newIndex ? mCurrentIndex - newIndex : newIndex - mCurrentIndex;
  PRBool up = newIndex < mCurrentIndex;

  // Check to be sure we're not scrolling off the bottom of the tree
  PRInt32 lastPageTopRow = GetRowCount() - (GetAvailableHeight() / mRowHeight);
  if (lastPageTopRow < 0)
    lastPageTopRow = 0;

  if (aRowIndex > lastPageTopRow)
    return NS_OK;

  mCurrentIndex = newIndex;
  InternalPositionChanged(up, delta, aForceDestruct);

  // This change has to happen immediately.
  // Flush any pending reflow commands.
  nsCOMPtr<nsIDocument> doc;
  mContent->GetDocument(*getter_AddRefs(doc));
  doc->FlushPendingNotifications();

  return NS_OK;
}

NS_IMETHODIMP
nsListBoxBodyFrame::InternalPositionChangedCallback()
{
  nsListScrollSmoother* smoother = GetSmoother();

  if (smoother->mDelta == 0)
    return NS_OK;

  mCurrentIndex += smoother->mDelta;

  if (mCurrentIndex < 0)
    mCurrentIndex = 0;

  return InternalPositionChanged(smoother->mDelta < 0, smoother->mDelta < 0 ? -smoother->mDelta : smoother->mDelta);
}

NS_IMETHODIMP
nsListBoxBodyFrame::InternalPositionChanged(PRBool aUp, PRInt32 aDelta, PRBool aForceDestruct)
{  
  if (aDelta == 0)
    return NS_OK;

  // begin timing how long it takes to scroll a row
  PRTime start = PR_Now();

  nsCOMPtr<nsIPresShell> shell;
  mPresContext->GetShell(getter_AddRefs(shell));
  shell->FlushPendingNotifications(PR_FALSE);

  PRInt32 visibleRows = 0;
  if (mRowHeight)
	  visibleRows = GetAvailableHeight()/mRowHeight;
  
  if (aDelta < visibleRows && !aForceDestruct) {
    PRInt32 loseRows = aDelta;
    if (aUp) {
      // scrolling up, destroy rows from the bottom downwards
      ReverseDestroyRows(loseRows);
      mRowsToPrepend += aDelta;
      mLinkupFrame = nsnull;
    }
    else {
      // scrolling down, destroy rows from the top upwards
      DestroyRows(loseRows);
      mRowsToPrepend = 0;
    }
  }
  else {
    // We have scrolled so much that all of our current frames will
    // go off screen, so blow them all away. Weeee!
    nsIBox* currBox;
    GetChildBox(&currBox);
    nsBoxLayoutState state(mPresContext);
    while (currBox) {
      nsIBox* nextBox;
      currBox->GetNextBox(&nextBox);
      nsIFrame* frame;
      currBox->QueryInterface(NS_GET_IID(nsIFrame), (void**)&frame); 
      mFrameConstructor->RemoveMappingsForFrameSubtree(mPresContext, frame, nsnull);
      Remove(state, frame);
      mFrames.DestroyFrame(mPresContext, frame);
      currBox = nextBox;
    }
  }

  // clear frame markers so that CreateRows will re-create
  mTopFrame = mBottomFrame = nsnull; 
  
  mYPosition = mCurrentIndex*mRowHeight;
  nsBoxLayoutState state(mPresContext);
  mScrolling = PR_TRUE;
  MarkDirtyChildren(state);
  shell->FlushPendingNotifications(PR_FALSE); // Calls CreateRows
  mScrolling = PR_FALSE;
  
  VerticalScroll(mYPosition);

  if (aForceDestruct)
    Redraw(state, nsnull, PR_FALSE);
  
  PRTime end = PR_Now();

  PRTime difTime;
  LL_SUB(difTime, end, start);

  PRInt32 newTime;
  LL_L2I(newTime, difTime);
  newTime /= aDelta;

  // average old and new
  mTimePerRow = (newTime + mTimePerRow)/2;
  
  return NS_OK;
}

nsListScrollSmoother* 
nsListBoxBodyFrame::GetSmoother()
{
  if (!mScrollSmoother) {
    mScrollSmoother = new nsListScrollSmoother(this);
    NS_ASSERTION(mScrollSmoother, "out of memory");
    NS_IF_ADDREF(mScrollSmoother);
  }

  return mScrollSmoother;
}

void
nsListBoxBodyFrame::VerticalScroll(PRInt32 aPosition)
{
  nsIBox* box;
  GetParentBox(&box);
  if (!box)
    return;

  box->GetParentBox(&box);
  if (!box)
    return;

  nsCOMPtr<nsIScrollableFrame> scrollFrame(do_QueryInterface(box));
  if (!scrollFrame)
    return;

  nscoord x, y;
  scrollFrame->GetScrollPosition(mPresContext, x, y);
 
  scrollFrame->ScrollTo(mPresContext, x, aPosition, NS_SCROLL_PROPERTY_ALWAYS_BLIT);

  mYPosition = aPosition;
}

////////// frame and box retrieval

nsIFrame*
nsListBoxBodyFrame::GetFirstFrame()
{
  mTopFrame = mFrames.FirstChild();
  return mTopFrame;
}

nsIFrame*
nsListBoxBodyFrame::GetNextFrame(nsIFrame* aPrevFrame)
{
  nsIFrame* nextFrame = nsnull;
  aPrevFrame->GetNextSibling(&nextFrame);
  return nextFrame;
}

nsIFrame*
nsListBoxBodyFrame::GetLastFrame()
{
  return mFrames.LastChild();
}

////////// lazy row creation and destruction

void
nsListBoxBodyFrame::CreateRows(nsBoxLayoutState& aState)
{
  // Get our client rect.
  nsRect clientRect;
  GetClientRect(clientRect);

  // Get the starting y position and the remaining available
  // height.
  nscoord availableHeight = GetAvailableHeight();
  
  if (availableHeight <= 0) {
    PRBool fixed = (GetFixedRowSize() != -1);
    if (fixed)
      availableHeight = 10;
    else
      return;
  }
  
  // get the first tree box. If there isn't one create one.
  PRBool created = PR_FALSE;
  nsIBox* box = GetFirstItemBox(0, &created);
  nscoord rowHeight = GetRowHeightTwips();
  while (box) {  
    if (created && mRowsToPrepend > 0)
      --mRowsToPrepend;

    // if the row height is 0 then fail. Wait until someone 
    // laid out and sets the row height.
    if (rowHeight == 0)
        return;
     
    availableHeight -= rowHeight;
    
    // should we continue? Is the enought height?
    if (!ContinueReflow(availableHeight))
      break;

    // get the next tree box. Create one if needed.
    box = GetNextItemBox(box, 0, &created);
  }

  mRowsToPrepend = 0;
  mLinkupFrame = nsnull;
}

void
nsListBoxBodyFrame::DestroyRows(PRInt32& aRowsToLose) 
{
  // We need to destroy frames until our row count has been properly
  // reduced.  A reflow will then pick up and create the new frames.
  nsIFrame* childFrame = GetFirstFrame();
  while (childFrame && aRowsToLose > 0) {
    --aRowsToLose;
    
    nsIFrame* nextFrame = GetNextFrame(childFrame);
    mFrameConstructor->RemoveMappingsForFrameSubtree(mPresContext, childFrame, nsnull);
    nsBoxLayoutState state(mPresContext);

    Remove(state, childFrame);
    mFrames.DestroyFrame(mPresContext, childFrame);
    MarkDirtyChildren(state);

    mTopFrame = childFrame = nextFrame;
  }
}

void
nsListBoxBodyFrame::ReverseDestroyRows(PRInt32& aRowsToLose) 
{
  // We need to destroy frames until our row count has been properly
  // reduced.  A reflow will then pick up and create the new frames.
  nsIFrame* childFrame = GetLastFrame();
  while (childFrame && aRowsToLose > 0) {
    --aRowsToLose;
    
    nsIFrame* prevFrame;
    prevFrame = mFrames.GetPrevSiblingFor(childFrame);
    mFrameConstructor->RemoveMappingsForFrameSubtree(mPresContext, childFrame, nsnull);
    nsBoxLayoutState state(mPresContext);

    Remove(state, childFrame);
    mFrames.DestroyFrame(mPresContext, childFrame);
    MarkDirtyChildren(state);

    mBottomFrame = childFrame = prevFrame;
  }
}

//
// Get the nsIBox for the first visible listitem, and if none exists,
// create one.
//
nsIBox* 
nsListBoxBodyFrame::GetFirstItemBox(PRInt32 aOffset, PRBool* aCreated)
{
  if (aCreated)
   *aCreated = PR_FALSE;

  // Clear ourselves out.
  mBottomFrame = mTopFrame;

  if (mTopFrame) {
    nsCOMPtr<nsIBox> box(do_QueryInterface(mTopFrame));
    return box;
  }

  // top frame was cleared out
  mTopFrame = GetFirstFrame();
  mBottomFrame = mTopFrame;

  if (mTopFrame && mRowsToPrepend <= 0) {
    nsCOMPtr<nsIBox> box(do_QueryInterface(mTopFrame));
    return box;
  }

  // At this point, we either have no frames at all, 
  // or the user has scrolled upwards, leaving frames
  // to be created at the top.  Let's determine which
  // content needs a new frame first.

  nsCOMPtr<nsIContent> startContent;
  if (mTopFrame && mRowsToPrepend > 0) {
    // We need to insert rows before the top frame
    nsCOMPtr<nsIContent> topContent;
    mTopFrame->GetContent(getter_AddRefs(topContent));
    nsCOMPtr<nsIContent> topParent;
    topContent->GetParent(*getter_AddRefs(topParent));
    PRInt32 contentIndex;
    topParent->IndexOf(topContent, contentIndex);
    contentIndex -= aOffset;
    if (contentIndex < 0)
      return nsnull;
    topParent->ChildAt(contentIndex-mRowsToPrepend, *getter_AddRefs(startContent));
  } else {
    // This will be the first item frame we create.  Use the content
    // at the current index, which is the first index scrolled into view
    GetListItemContentAt(mCurrentIndex+aOffset, getter_AddRefs(startContent));
  }

  if (startContent) {  
    // Either append the new frame, or prepend it (at index 0)
    // XXX check here if frame was even created, it may not have been if
    //     display: none was on listitem content
    PRBool isAppend = mRowsToPrepend <= 0;
    mFrameConstructor->CreateListBoxContent(mPresContext, this, nsnull, startContent,
                                            &mTopFrame, isAppend, PR_FALSE, nsnull);
    
    if (mTopFrame) {
      if (aCreated)
        *aCreated = PR_TRUE;

      mBottomFrame = mTopFrame;

      nsCOMPtr<nsIBox> box(do_QueryInterface(mTopFrame));
      return box;
    } else
      return GetFirstItemBox(++aOffset, 0);
  }

  return nsnull;
}

//
// Get the nsIBox for the next visible listitem after aBox, and if none
// exists, create one.
//
nsIBox* 
nsListBoxBodyFrame::GetNextItemBox(nsIBox* aBox, PRInt32 aOffset, PRBool* aCreated)
{
  if (aCreated)
    *aCreated = PR_FALSE;

  nsIFrame* result = nsnull;
  nsIFrame* frame = nsnull;
  aBox->GetFrame(&frame);
  frame->GetNextSibling(&result);

  if (!result || result == mLinkupFrame || mRowsToPrepend > 0) {
    // No result found. See if there's a content node that wants a frame.
    PRInt32 i, childCount;
    nsCOMPtr<nsIContent> prevContent;
    frame->GetContent(getter_AddRefs(prevContent));
    nsCOMPtr<nsIContent> parentContent;
    prevContent->GetParent(*getter_AddRefs(parentContent));
    parentContent->IndexOf(prevContent, i);
    parentContent->ChildCount(childCount);
    if (i+aOffset+1 < childCount) {
      // There is a content node that wants a frame.
      nsCOMPtr<nsIContent> nextContent;
      parentContent->ChildAt(i+aOffset+1, *getter_AddRefs(nextContent));

      // Either append the new frame, or insert it after the current frame
      PRBool isAppend = result != mLinkupFrame && mRowsToPrepend <= 0;
      nsIFrame* prevFrame = isAppend ? nsnull : frame;
      mFrameConstructor->CreateListBoxContent(mPresContext, this, prevFrame, nextContent,
                                              &result, isAppend, PR_FALSE,
                                              nsnull);

      if (result) {
        if (aCreated)
           *aCreated = PR_TRUE;
      } else
        return GetNextItemBox(aBox, ++aOffset, aCreated);
            
      mLinkupFrame = nsnull;
    }
  }

  if (result)
    mBottomFrame = result;

  nsCOMPtr<nsIBox> box(do_QueryInterface(result));
  return box;
}

PRBool
nsListBoxBodyFrame::ContinueReflow(nscoord height) 
{ 
  if (height <= 0) {
    nsIFrame* lastChild = GetLastFrame();
    nsIFrame* startingPoint = mBottomFrame;
    if (startingPoint == nsnull) {
      // We just want to delete everything but the first item.
      startingPoint = GetFirstFrame();
    }

    if (lastChild != startingPoint) {
      // We have some hangers on (probably caused by shrinking the size of the window).
      // Nuke them.
      nsIFrame* currFrame;
      startingPoint->GetNextSibling(&currFrame);
      nsBoxLayoutState state(mPresContext);

      while (currFrame) {
        nsIFrame* nextFrame;
        currFrame->GetNextSibling(&nextFrame);
        mFrameConstructor->RemoveMappingsForFrameSubtree(mPresContext, currFrame, nsnull);
        
        Remove(state, currFrame);

        mFrames.DestroyFrame(mPresContext, currFrame);

        currFrame = nextFrame;
      }

      MarkDirtyChildren(state);
    }
    return PR_FALSE;
  }
  else
    return PR_TRUE;
}

NS_IMETHODIMP
nsListBoxBodyFrame::ListBoxAppendFrames(nsIFrame* aFrameList)
{
  // append them after
  nsBoxLayoutState state(mPresContext);
  Append(state,aFrameList);
  mFrames.AppendFrames(nsnull, aFrameList);
  MarkDirtyChildren(state);
  
  return NS_OK;
}

NS_IMETHODIMP
nsListBoxBodyFrame::ListBoxInsertFrames(nsIFrame* aPrevFrame, nsIFrame* aFrameList)
{
  // insert the frames to our info list
  nsBoxLayoutState state(mPresContext);
  Insert(state, aPrevFrame, aFrameList);
  mFrames.InsertFrames(nsnull, aPrevFrame, aFrameList);
  MarkDirtyChildren(state);

  return NS_OK;
}

// 
// Called by nsCSSFrameConstructor when a new listitem content is inserted.
//
void 
nsListBoxBodyFrame::OnContentInserted(nsIPresContext* aPresContext, nsIContent* aChildContent)
{
  ++mRowCount;

  nsCOMPtr<nsIPresShell> shell;
  aPresContext->GetShell(getter_AddRefs(shell));
  // The RDF content builder will build content nodes such that they are all 
  // ready when OnContentInserted is first called, meaning the first call
  // to CreateRows will create all the frames, but OnContentInserted will
  // still be called again for each content node - so we need to make sure
  // that the frame for each content node hasn't already been created.
  nsIFrame* childFrame = nsnull;
  shell->GetPrimaryFrameFor(aChildContent, &childFrame);
  if (childFrame)
    return;

  PRInt32 siblingIndex;
  nsCOMPtr<nsIContent> nextSiblingContent;
  GetListItemNextSibling(aChildContent, getter_AddRefs(nextSiblingContent), siblingIndex);
  
  // if we're inserting our item before the first visible content,
  // then we need to shift all rows down by one
  if (siblingIndex >= 0 &&  siblingIndex-1 <= mCurrentIndex) {
    mTopFrame = nsnull;
    mRowsToPrepend = 1;
  } else if (nextSiblingContent) {
    // we may be inserting before a frame that is on screen
    nsIFrame* nextSiblingFrame = nsnull;
    shell->GetPrimaryFrameFor(nextSiblingContent, &nextSiblingFrame);
    mLinkupFrame = nextSiblingFrame;
  }
  
  nsBoxLayoutState state(aPresContext);
  MarkDirtyChildren(state);
  shell->FlushPendingNotifications(PR_FALSE);
}

// 
// Called by nsCSSFrameConstructor when listitem content is removed.
//
void
nsListBoxBodyFrame::OnContentRemoved(nsIPresContext* aPresContext, nsIFrame* aChildFrame, PRInt32 aIndex)
{
  --mRowCount;

  if (!aChildFrame) {
    // The row we are removing is out of view, so we need to try to
    // determine the index of its next sibling.
    nsCOMPtr<nsIContent> listboxContent;
    mContent->GetBindingParent(getter_AddRefs(listboxContent));
    nsCOMPtr<nsIContent> oldNextSiblingContent;
    listboxContent->ChildAt(aIndex, *getter_AddRefs(oldNextSiblingContent));
    PRInt32 siblingIndex = -1;
    if (oldNextSiblingContent) {
      nsCOMPtr<nsIContent> nextSiblingContent;
      GetListItemNextSibling(oldNextSiblingContent, getter_AddRefs(nextSiblingContent), siblingIndex);
    }
  
    // if the row being removed is off-screen and above the top frame, we need to
    // adjust our top index and tell the scrollbar to shift up one row.
    if (siblingIndex >= 0 && siblingIndex-1 <= mCurrentIndex) {
      --mCurrentIndex;
      mYPosition = mCurrentIndex*mRowHeight;
      VerticalScroll(mYPosition);
    }
  } else if (mCurrentIndex > 0) {
    // At this point, we know we have a scrollbar, and we need to know 
    // if we are scrolled to the last row.  In this case, the behavior
    // of the scrollbar is to stay locked to the bottom.  Since we are
    // removing visible content, the first visible row will have to move
    // down by one, and we will have to insert a new frame at the top.
    
    // if the last content node has a frame, we are scrolled to the bottom
    nsCOMPtr<nsIContent> listBoxContent;
    mContent->GetBindingParent(getter_AddRefs(listBoxContent));
    PRInt32 childCount;
    listBoxContent->ChildCount(childCount);
    if (childCount > 0) {
      nsCOMPtr<nsIContent> lastChild;
      listBoxContent->ChildAt(childCount-1, *getter_AddRefs(lastChild));
      nsCOMPtr<nsIPresShell> shell;
      aPresContext->GetShell(getter_AddRefs(shell));
      nsIFrame* lastChildFrame = nsnull;
      shell->GetPrimaryFrameFor(lastChild, &lastChildFrame);
    
      if (lastChildFrame) {
        mTopFrame = nsnull;
        mRowsToPrepend = 1;
        --mCurrentIndex;
        mYPosition = mCurrentIndex*mRowHeight;
        VerticalScroll(mYPosition);
      }
    }
  }

  // if we're removing the top row, the new top row is the next row
  if (mTopFrame && mTopFrame == aChildFrame)
    mTopFrame->GetNextSibling(&mTopFrame);

  // Go ahead and delete the frame.
  nsBoxLayoutState state(aPresContext);
  if (aChildFrame) {
    mFrameConstructor->RemoveMappingsForFrameSubtree(aPresContext, aChildFrame, nsnull);

    Remove(state, aChildFrame);
    mFrames.DestroyFrame(aPresContext, aChildFrame);
  }

  MarkDirtyChildren(state);
  nsCOMPtr<nsIPresShell> shell;
  aPresContext->GetShell(getter_AddRefs(shell));
  shell->FlushPendingNotifications(PR_FALSE);
}

void
nsListBoxBodyFrame::GetListItemContentAt(PRInt32 aIndex, nsIContent** aContent)
{
  nsCOMPtr<nsIContent> listboxContent;
  mContent->GetBindingParent(getter_AddRefs(listboxContent));

  PRInt32 childCount;
  listboxContent->ChildCount(childCount);
  PRInt32 itemsFound = 0;
  for (PRInt32 i = 0; i < childCount; ++i) {
    nsCOMPtr<nsIContent> kid;
    listboxContent->ChildAt(i, *getter_AddRefs(kid));
    nsCOMPtr<nsIAtom> tag;
    kid->GetTag(*getter_AddRefs(tag));
    if (tag.get() == nsXULAtoms::listitem) {
      ++itemsFound;
      if (itemsFound-1 == aIndex) {
        *aContent = kid;
        NS_IF_ADDREF(*aContent);
        return;
      }
        
    }
  }
}

void
nsListBoxBodyFrame::GetListItemNextSibling(nsIContent* aListItem, nsIContent** aContent, PRInt32& aSiblingIndex)
{
  nsCOMPtr<nsIContent> listboxContent;
  mContent->GetBindingParent(getter_AddRefs(listboxContent));

  aSiblingIndex = -1;

  PRInt32 childCount;
  listboxContent->ChildCount(childCount);
  nsCOMPtr<nsIContent> prevKid;
  for (PRInt32 i = 0; i < childCount; ++i) {
    nsCOMPtr<nsIContent> kid;
    listboxContent->ChildAt(i, *getter_AddRefs(kid));
    nsCOMPtr<nsIAtom> tag;
    kid->GetTag(*getter_AddRefs(tag));
    if (tag.get() == nsXULAtoms::listitem) {
      ++aSiblingIndex;
      if (prevKid.get() == aListItem) {
        *aContent = kid;
        NS_IF_ADDREF(*aContent);
        return;
      }
        
    }
    prevKid = kid;
  }

  aSiblingIndex = -1; // no match, so there is no next sibling
}

void
nsListBoxBodyFrame::ForceDrawFrame(nsIPresContext* aPresContext, nsIFrame * aFrame)
{
  if (aFrame == nsnull) {
    return;
  }
  nsRect    rect;
  nsIView * view;
  nsPoint   pnt;
  aFrame->GetOffsetFromView(aPresContext, pnt, &view);
  aFrame->GetRect(rect);
  rect.x = pnt.x;
  rect.y = pnt.y;
  if (view) {
    nsCOMPtr<nsIViewManager> viewMgr;
    view->GetViewManager(*getter_AddRefs(viewMgr));
    if (viewMgr)
      viewMgr->UpdateView(view, rect, NS_VMREFRESH_IMMEDIATE);
  }

}

//////////////////////////////////////////////////////////////////////////
///// nsListboxScrollPortFrame

class nsListboxScrollPortFrame : public nsScrollPortFrame
{
public:
  nsListboxScrollPortFrame(nsIPresShell* aShell);
  friend nsresult NS_NewScrollBoxFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);
  NS_IMETHOD GetPrefSize(nsBoxLayoutState& aBoxLayoutState, nsSize& aSize);
  NS_IMETHOD GetMinSize(nsBoxLayoutState& aBoxLayoutState, nsSize& aSize);
};

nsListboxScrollPortFrame::nsListboxScrollPortFrame(nsIPresShell* aShell):nsScrollPortFrame(aShell)
{
}

NS_IMETHODIMP
nsListboxScrollPortFrame::GetMinSize(nsBoxLayoutState& aBoxLayoutState, nsSize& aSize)
{  
  nsIBox* child = nsnull;
  GetChildBox(&child);
 
  nsresult rv = child->GetPrefSize(aBoxLayoutState, aSize);
  nsListBoxBodyFrame* outer = NS_STATIC_CAST(nsListBoxBodyFrame*,child);

  nsAutoString sizeMode;
  nsCOMPtr<nsIContent> content;
  outer->GetContent(getter_AddRefs(content));
  content->GetAttr(kNameSpaceID_None, nsXULAtoms::sizemode, sizeMode);
  if (!sizeMode.IsEmpty()) {  
    nsCOMPtr<nsIScrollableFrame> scrollFrame(do_QueryInterface(mParent));
    if (scrollFrame) {
      nsIScrollableFrame::nsScrollPref scrollPref;
      scrollFrame->GetScrollPreference(aBoxLayoutState.GetPresContext(), &scrollPref);

      if (scrollPref == nsIScrollableFrame::Auto) {
        nscoord vbarwidth, hbarheight;
        scrollFrame->GetScrollbarSizes(aBoxLayoutState.GetPresContext(),
                                       &vbarwidth, &hbarheight);
        aSize.width += vbarwidth;
      }
    }
  }
  else aSize.width = 0;

  aSize.height = 0;
  
  AddMargin(child, aSize);
  AddBorderAndPadding(aSize);
  AddInset(aSize);
  nsIBox::AddCSSMinSize(aBoxLayoutState, this, aSize);
  return rv;

}

NS_IMETHODIMP
nsListboxScrollPortFrame::GetPrefSize(nsBoxLayoutState& aBoxLayoutState, nsSize& aSize)
{  
  nsIBox* child = nsnull;
  GetChildBox(&child);
 
  nsresult rv = child->GetPrefSize(aBoxLayoutState, aSize);
  nsListBoxBodyFrame* outer = NS_STATIC_CAST(nsListBoxBodyFrame*,child);

  PRInt32 size = outer->GetFixedRowSize();

  if (size > -1)
    aSize.height = size*outer->GetRowHeightTwips();
   
  nsCOMPtr<nsIScrollableFrame> scrollFrame(do_QueryInterface(mParent));
  if (scrollFrame) {
    nsIScrollableFrame::nsScrollPref scrollPref;
    scrollFrame->GetScrollPreference(aBoxLayoutState.GetPresContext(), &scrollPref);

    if (scrollPref == nsIScrollableFrame::Auto) {
      nscoord vbarwidth, hbarheight;
      scrollFrame->GetScrollbarSizes(aBoxLayoutState.GetPresContext(),
                                     &vbarwidth, &hbarheight);
      aSize.width += vbarwidth;
    }
  }

  AddMargin(child, aSize);
  AddBorderAndPadding(aSize);
  AddInset(aSize);
  nsIBox::AddCSSPrefSize(aBoxLayoutState, this, aSize);
  return rv;

}

// Creation Routines ///////////////////////////////////////////////////////////////////////

nsresult
NS_NewListBoxBodyFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame, PRBool aIsRoot, 
                       nsIBoxLayout* aLayoutManager)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsListBoxBodyFrame* it = new (aPresShell) nsListBoxBodyFrame(aPresShell, aIsRoot, aLayoutManager);
  if (!it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;
  return NS_OK;
  
}

nsresult
NS_NewListBoxScrollPortFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsListboxScrollPortFrame* it = new (aPresShell) nsListboxScrollPortFrame (aPresShell);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}
