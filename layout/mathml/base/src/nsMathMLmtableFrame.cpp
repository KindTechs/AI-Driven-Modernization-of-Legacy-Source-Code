/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla MathML Project.
 *
 * The Initial Developer of the Original Code is The University Of
 * Queensland.  Portions created by The University Of Queensland are
 * Copyright (C) 1999 The University Of Queensland.  All Rights Reserved.
 *
 * Contributor(s):
 *   Roger B. Sidje <rbs@maths.uq.edu.au>
 */

#include "nsCOMPtr.h"
#include "nsHTMLParts.h"
#include "nsIHTMLContent.h"
#include "nsFrame.h"
#include "nsAreaFrame.h"
#include "nsLineLayout.h"
#include "nsHTMLIIDs.h"
#include "nsIPresContext.h"
#include "nsHTMLAtoms.h"
#include "nsUnitConversion.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsINameSpaceManager.h"
#include "nsIRenderingContext.h"
#include "nsIFontMetrics.h"
#include "nsStyleUtil.h"

#include "nsVoidArray.h"
#include "nsIFrameManager.h"
#include "nsStyleChangeList.h"
#include "nsTableOuterFrame.h"
#include "nsTableFrame.h"
#include "nsTableCellFrame.h"

#include "nsMathMLmtableFrame.h"

//
// <mtable> -- table or matrix - implementation
//

// helper function to perform an in-place split of a space-delimited string,
// and return an array of pointers for the beginning of each segment, i.e.,
// aOffset[0] is the first string, aOffset[1] is the second string, etc.
// Used to parse attributes like columnalign='left right', rowalign='top bottom'
static void
SplitString(nsString&    aString, // [IN/OUT]
            nsVoidArray& aOffset) // [OUT]
{
  static const PRUnichar kNullCh = PRUnichar('\0');

  aString.Append(kNullCh);  // put an extra null at the end

  PRUnichar* start = (PRUnichar*)(const PRUnichar*)aString.get();
  PRUnichar* end   = start;

  while (kNullCh != *start) {
    while ((kNullCh != *start) && nsCRT::IsAsciiSpace(*start)) {  // skip leading space
      start++;
    }
    end = start;

    while ((kNullCh != *end) && (PR_FALSE == nsCRT::IsAsciiSpace(*end))) { // look for space or end
      end++;
    }
    *end = kNullCh; // end string here

    if (start < end) {
      aOffset.AppendElement(start); // record the beginning of this segment
    }

    start = ++end;
  }
}

struct nsValueList
{
  nsString    mData;
  nsVoidArray mArray;

  nsValueList(nsString& aData) {
    mData.Assign(aData);
    SplitString(mData, mArray);
  }
};

// Each rowalign='top bottom' or columnalign='left right center' (from
// <mtable> or <mtr>) is split once (lazily) into a nsValueList which is
// stored in the frame manager. Cell frames query the frame manager
// to see what values apply to them.

// XXX See bug 69409 - MathML attributes are not mapped to style.
// This code is not suitable for dynamic updates, for example when the
// rowalign and columalign attributes are changed with JavaScript.
// The code doesn't include hooks for AttributeChanged() notifications.

static void
DestroyValueListFunc(nsIPresContext* aPresContext,
                     nsIFrame*       aFrame,
                     nsIAtom*        aPropertyName,
                     void*           aPropertyValue)
{
  delete NS_STATIC_CAST(nsValueList*, aPropertyValue);
}

static PRUnichar*
GetValueAt(nsIPresContext* aPresContext,
           nsIFrame*       aTableOrRowFrame,
           nsIAtom*        aAttributeAtom,
           PRInt32         aRowOrColIndex)
{
  PRUnichar* result = nsnull;
  nsValueList* valueList = nsnull;

  nsCOMPtr<nsIPresShell> presShell;
  aPresContext->GetShell(getter_AddRefs(presShell));
  if (presShell) {
    nsCOMPtr<nsIFrameManager> frameManager;
    presShell->GetFrameManager(getter_AddRefs(frameManager));
    if (frameManager) {
      frameManager->GetFrameProperty(aTableOrRowFrame, aAttributeAtom,
                                     0, (void**)&valueList);
      if (!valueList) {
        // The property isn't there yet, so set it
        nsAutoString values;
        nsCOMPtr<nsIContent> content;
        aTableOrRowFrame->GetContent(getter_AddRefs(content));
        if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttr(kNameSpaceID_None, aAttributeAtom, values)) {
          valueList = new nsValueList(values);
          if (valueList) {
            frameManager->SetFrameProperty(aTableOrRowFrame, aAttributeAtom,
                                           valueList, DestroyValueListFunc);
          }
        }
      }
    }
  }

  if (valueList) {
    PRInt32 count = valueList->mArray.Count();
    result = (aRowOrColIndex < count)
           ? (PRUnichar*)(valueList->mArray[aRowOrColIndex])
           : (PRUnichar*)(valueList->mArray[count-1]);
  }
  return result;
}

#ifdef NS_DEBUG
#define DEBUG_VERIFY_THAT_FRAME_IS(_frame, _expected) \
PR_BEGIN_MACRO \
  const nsStyleDisplay* display; \
  _frame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct *&)display); \
  NS_ASSERTION(NS_STYLE_DISPLAY_##_expected == display->mDisplay, "internal error"); \
PR_END_MACRO
#else
#define DEBUG_VERIFY_THAT_FRAME_IS(_frame, _expected)
#endif

static void
MapAttributesInto(nsIPresContext* aPresContext,
                  nsIContent*     aCellContent,
                  nsIFrame*       aCellFrame,
                  nsIFrame*       aCellInnerFrame)
{
  nsTableCellFrame* cellFrame = NS_STATIC_CAST(nsTableCellFrame*, aCellFrame);
  nsTableCellFrame* sibling;

  PRInt32 rowIndex, colIndex;
  nsresult rv = cellFrame->GetCellIndexes(rowIndex, colIndex);
  NS_ASSERTION(NS_SUCCEEDED(rv), "cannot find the position of the cell frame");
  if (NS_FAILED(rv)) return;

  nsIFrame* tableFrame;
  nsIFrame* rowgroupFrame;
  nsIFrame* rowFrame;

  cellFrame->GetParent(&rowFrame);
  rowFrame->GetParent(&rowgroupFrame);
  rowgroupFrame->GetParent(&tableFrame);
  DEBUG_VERIFY_THAT_FRAME_IS(rowFrame, TABLE_ROW);
  DEBUG_VERIFY_THAT_FRAME_IS(rowgroupFrame, TABLE_ROW_GROUP);
  DEBUG_VERIFY_THAT_FRAME_IS(tableFrame, TABLE);
#ifdef NS_DEBUG
  PRBool originates;
  ((nsTableFrame*)tableFrame)->GetCellInfoAt(rowIndex, colIndex, &originates);
  NS_ASSERTION(originates, "internal error");
#endif

  nsIAtom* atom;
  PRUnichar* attr;
  nsAutoString value;
  PRBool hasChanged = PR_FALSE;
  NS_NAMED_LITERAL_STRING(trueStr, "true");

  //////////////////////////////////////
  // process attributes that depend on the index of the row:
  // rowalign, rowlines

  // see if the rowalign attribute is not already set
  atom = nsMathMLAtoms::rowalign_;
  rv = aCellContent->GetAttr(kNameSpaceID_None, atom, value);
  if (NS_CONTENT_ATTR_NOT_THERE == rv) {
    // see if the rowalign attribute was specified on the row
    attr = GetValueAt(aPresContext, rowFrame, atom, rowIndex);
    if (!attr) {
    	// see if the rowalign attribute was specified on the table
      attr = GetValueAt(aPresContext, tableFrame, atom, rowIndex);
    }
    // set the attribute without notifying that we want a reflow
    if (attr) {
      hasChanged = PR_TRUE;
      aCellContent->SetAttr(kNameSpaceID_None, atom, nsDependentString(attr), PR_FALSE);
    }
  }
  // if we are not on the first row, see if |rowlines| was specified on the table.
  // Note that we pass 'rowIndex-1' because the CSS rule in mathml.css is associated
  // to 'border-top', and it is as if we draw the line on behalf of the previous cell.
  // This way of doing so allows us to handle selective lines, [row]\hline[row][row]',
  // and cases of spanning cells without further complications.
  if (rowIndex > 0) {
    attr = GetValueAt(aPresContext, tableFrame, nsMathMLAtoms::rowlines_, rowIndex-1);
    // set the special -moz-math-rowline without notifying that we want a reflow
    if (attr) {
      hasChanged = PR_TRUE;
      aCellContent->SetAttr(kNameSpaceID_None, nsMathMLAtoms::rowline, nsDependentString(attr), PR_FALSE);
    }
  }
  else {
    // set the special -moz-math-firstrow to annotate that we are on the first row
    hasChanged = PR_TRUE;
    aCellContent->SetAttr(kNameSpaceID_None, nsMathMLAtoms::firstrow, trueStr, PR_FALSE);
  }
  // if we are on the last row, set the special -moz-math-lastrow
  PRInt32 rowSpan = ((nsTableFrame*)tableFrame)->GetEffectiveRowSpan(*cellFrame);
  sibling = ((nsTableFrame*)tableFrame)->GetCellFrameAt(rowIndex+rowSpan, colIndex);
  if (!sibling) {
    hasChanged = PR_TRUE;
    aCellContent->SetAttr(kNameSpaceID_None, nsMathMLAtoms::lastrow, trueStr, PR_FALSE);
  }

  //////////////////////////////////////
  // process attributes that depend on the index of the column:
  // columnalign, columnlines, XXX need columnwidth too

  // see if the columnalign attribute is not already set
  atom = nsMathMLAtoms::columnalign_;
  rv = aCellContent->GetAttr(kNameSpaceID_None, atom, value);
  if (NS_CONTENT_ATTR_NOT_THERE == rv) {
    // see if the columnalign attribute was specified on the row
    attr = GetValueAt(aPresContext, rowFrame, atom, colIndex);
    if (!attr) {
    	// see if the columnalign attribute was specified on the table
      attr = GetValueAt(aPresContext, tableFrame, atom, colIndex);
    }
    if (attr) {
      hasChanged = PR_TRUE;
      aCellContent->SetAttr(kNameSpaceID_None, atom, nsDependentString(attr), PR_FALSE);
    }
  }
  // if we are not on the first column, see if |columnlines| was specified on
  // the table. Note that we pass 'colIndex-1' because the CSS rule in mathml.css
  // is associated to 'border-left', and it is as if we draw the line on behalf
  // of the previous cell. This way of doing so allows us to handle selective lines,
  // e.g., 'r|cl', and cases of spanning cells without further complications.
  if (colIndex > 0) {
    attr = GetValueAt(aPresContext, tableFrame, nsMathMLAtoms::columnlines_, colIndex-1);
    // set the special -moz-math-columnline without notifying that we want a reflow
    if (attr) {
      hasChanged = PR_TRUE;
      aCellContent->SetAttr(kNameSpaceID_None, nsMathMLAtoms::columnline, nsDependentString(attr), PR_FALSE);
    }
  }
  else {
    // set the special -moz-math-firstcolumn to annotate that we are on the first column
    hasChanged = PR_TRUE;
    aCellContent->SetAttr(kNameSpaceID_None, nsMathMLAtoms::firstcolumn, trueStr, PR_FALSE);
  }
  // if we are on the last column, set the special -moz-math-lastcolumn
  PRInt32 colSpan = ((nsTableFrame*)tableFrame)->GetEffectiveColSpan(*cellFrame);
  sibling = ((nsTableFrame*)tableFrame)->GetCellFrameAt(rowIndex, colIndex+colSpan);
  if (!sibling) {
    hasChanged = PR_TRUE;
    aCellContent->SetAttr(kNameSpaceID_None, nsMathMLAtoms::lastcolumn, trueStr, PR_FALSE);
  }

  // now, re-resolve the style contexts in our subtree to pick up any changes
  if (hasChanged) {
    nsCOMPtr<nsIPresShell> presShell;
    aPresContext->GetShell(getter_AddRefs(presShell));
    if (presShell) {
      nsCOMPtr<nsIFrameManager> fm;
      presShell->GetFrameManager(getter_AddRefs(fm));
      if (fm) {
        PRInt32 maxChange, minChange = NS_STYLE_HINT_NONE;
        nsStyleChangeList changeList;
        fm->ComputeStyleChangeFor(aPresContext, aCellFrame, kNameSpaceID_None, nsnull,
                                  changeList, minChange, maxChange);
      }
    }
  }
}

// the align attribute of mtable can have a row number which indicates
// from where to anchor the table, e.g., top5 means anchor the table at
// the top of the 5th row, axis-1 means anchor the table on the axis of
// the last row (could have been nicer if the REC used the '#' separator,
// e.g., top#5, or axis#-1)

enum eAlign {
  eAlign_top,
  eAlign_bottom,
  eAlign_center,
  eAlign_baseline,
  eAlign_axis
};

static void
ParseAlignAttribute(nsString& aValue, eAlign& aAlign, PRInt32& aRowIndex)
{
  // by default, the table is centered about the axis
  aRowIndex = 0;
  aAlign = eAlign_axis;
  PRInt32 len = 0;
  if (0 == aValue.Find("top")) {
    len = 3; // 3 is the length of 'top'
    aAlign = eAlign_top;
  }
  else if (0 == aValue.Find("bottom")) {
    len = 6; // 6 is the length of 'bottom'
    aAlign = eAlign_bottom;
  }
  else if (0 == aValue.Find("center")) {
    len = 6; // 6 is the length of 'center'
    aAlign = eAlign_center;
  }
  else if (0 == aValue.Find("baseline")) {
    len = 8; // 8 is the length of 'baseline'
    aAlign = eAlign_baseline;
  }
  else if (0 == aValue.Find("axis")) {
    len = 4; // 4 is the length of 'axis'
    aAlign = eAlign_axis;
  }
  if (len) {
    PRInt32 error;
    aValue.Cut(0, len); // aValue is not a const here
    aRowIndex = aValue.ToInteger(&error);
    if (error)
      aRowIndex = 0;
  }
}

// --------
// implementation of nsMathMLmtableOuterFrame

NS_IMPL_ADDREF_INHERITED(nsMathMLmtableOuterFrame, nsMathMLFrame)
NS_IMPL_RELEASE_INHERITED(nsMathMLmtableOuterFrame, nsMathMLFrame)
NS_IMPL_QUERY_INTERFACE_INHERITED1(nsMathMLmtableOuterFrame, nsTableOuterFrame, nsMathMLFrame)

nsresult
NS_NewMathMLmtableOuterFrame (nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsMathMLmtableOuterFrame* it = new (aPresShell) nsMathMLmtableOuterFrame;
  if (!it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;
  return NS_OK;
}

nsMathMLmtableOuterFrame::nsMathMLmtableOuterFrame()
  :nsTableOuterFrame()
{
}

nsMathMLmtableOuterFrame::~nsMathMLmtableOuterFrame()
{
}

NS_IMETHODIMP
nsMathMLmtableOuterFrame::InheritAutomaticData(nsIPresContext* aPresContext,
                                               nsIFrame*       aParent)
{
  // XXX the REC says that by default, displaystyle=false in <mtable>

  // let the base class inherit the scriptlevel and displaystyle from our parent
  nsMathMLFrame::InheritAutomaticData(aPresContext, aParent);

  // see if the displaystyle attribute is there and let it override what we inherited
  nsAutoString value;
  if (NS_CONTENT_ATTR_HAS_VALUE ==
      GetAttribute(mContent, nsnull, nsMathMLAtoms::displaystyle_, value)) {
    if (value.Equals(NS_LITERAL_STRING("true"))) {
      mPresentationData.flags |= NS_MATHML_DISPLAYSTYLE;
    }
    else if (value.Equals(NS_LITERAL_STRING("false"))) {
      mPresentationData.flags &= ~NS_MATHML_DISPLAYSTYLE;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMathMLmtableOuterFrame::Init(nsIPresContext*  aPresContext,
                               nsIContent*      aContent,
                               nsIFrame*        aParent,
                               nsIStyleContext* aContext,
                               nsIFrame*        aPrevInFlow)
{
  MapAttributesIntoCSS(aPresContext, aContent);

  return nsTableOuterFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);
}

nsIFrame*
nsMathMLmtableOuterFrame::GetRowFrameAt(nsIPresContext* aPresContext,
                                        PRInt32         aRowIndex)
{
  // To find the row at the given index, we will iterate downwards or
  // upwards depending on the sign of the index
  nsTableIteration dir = eTableLTR;
  if (aRowIndex < 0) {
    aRowIndex = -aRowIndex;
    dir = eTableRTL;
  }
  // if our inner table says that the index is valid, find the row now
  PRInt32 rowCount, colCount;
  GetTableSize(rowCount, colCount);
  if (aRowIndex <= rowCount) {
    nsIFrame* innerTableFrame = mFrames.FirstChild();
    nsTableIterator rowgroupIter(aPresContext, *innerTableFrame, dir);
    nsIFrame* rowgroupFrame = rowgroupIter.First();
    while (rowgroupFrame) {
      nsTableIterator rowIter(aPresContext, *rowgroupFrame, dir);
      nsIFrame* rowFrame = rowIter.First();
      while (rowFrame) {
        if (--aRowIndex == 0) {
          DEBUG_VERIFY_THAT_FRAME_IS(rowFrame, TABLE_ROW);
          return rowFrame;
        }
        rowFrame = rowIter.Next();
      }
      rowgroupFrame = rowgroupIter.Next();
    }
  }
  return nsnull;
}

NS_IMETHODIMP
nsMathMLmtableOuterFrame::Reflow(nsIPresContext*          aPresContext,
                                 nsHTMLReflowMetrics&     aDesiredSize,
                                 const nsHTMLReflowState& aReflowState,
                                 nsReflowStatus&          aStatus)
{
  nsresult rv;
  nsAutoString value;
  // we want to return a table that is anchored according to the align attribute

  nsHTMLReflowState reflowState(aReflowState);
  if ((NS_FRAME_FIRST_REFLOW & mState) &&
      (NS_UNCONSTRAINEDSIZE == reflowState.availableWidth)) {
    // We are going to reflow twice because the table frame code is
    // skipping the Pass 2 reflow when, at the Pass 1 reflow, the available
    // size is unconstrained. Skipping the Pass2 messes the MathML vertical
    // alignments that are resolved during the reflow of cell frames.

    nscoord oldComputedWidth = reflowState.mComputedWidth;
    reflowState.mComputedWidth = NS_UNCONSTRAINEDSIZE;
    reflowState.reason = eReflowReason_Initial;

    rv = nsTableOuterFrame::Reflow(aPresContext, aDesiredSize, reflowState, aStatus);

    // The second reflow will just be a reflow with a constrained width
    reflowState.availableWidth = aDesiredSize.width;
    reflowState.mComputedWidth = oldComputedWidth;
    reflowState.reason = eReflowReason_StyleChange;

    mState &= ~NS_FRAME_FIRST_REFLOW;
  }
  else if (mRect.width) {
    reflowState.availableWidth = mRect.width;
  }

  rv = nsTableOuterFrame::Reflow(aPresContext, aDesiredSize, reflowState, aStatus);
  NS_ASSERTION(aDesiredSize.height >= 0, "illegal height for mtable");
  NS_ASSERTION(aDesiredSize.width >= 0, "illegal width for mtable");

  // see if the user has set the align attribute on the <mtable>
  // XXX should we also check <mstyle> ?
  PRInt32 rowIndex = 0;
  eAlign tableAlign = eAlign_axis;
  if (NS_CONTENT_ATTR_HAS_VALUE ==
      GetAttribute(mContent, nsnull, nsMathMLAtoms::align_, value)) {
    ParseAlignAttribute(value, tableAlign, rowIndex);
  }

  // adjustments if there is a specified row from where to anchor the table
  // (conceptually: when there is no row of reference, picture the table as if
  // it is wrapped in a single big fictional row at dy = 0, this way of
  // doing so allows us to have a single code path for all cases).
  nscoord dy = 0;
  nscoord height = aDesiredSize.height;
  nsIFrame* rowFrame = nsnull;
  if (rowIndex) {
    rowFrame = GetRowFrameAt(aPresContext, rowIndex);
    if (rowFrame) {
      // translate the coordinates to be relative to us
      nsRect rect;
      nsIFrame* frame = rowFrame;
      frame->GetRect(rect);
      height = rect.height;
      do {
        dy += rect.y;
        frame->GetParent(&frame);
        frame->GetRect(rect);
      } while (frame != this);
    }
  }
  switch (tableAlign) {
    case eAlign_top:
      aDesiredSize.ascent = dy;
      break;
    case eAlign_bottom:
      aDesiredSize.ascent = dy + height;
      break;
    case eAlign_center:
      aDesiredSize.ascent = dy + height/2;
      break;
    case eAlign_baseline:
      if (rowFrame) {
        // anchor the table on the baseline of the row of reference
        nscoord rowAscent = ((nsTableRowFrame*)rowFrame)->GetMaxCellAscent();
        if (rowAscent) { // the row has at least one cell with 'vertical-align: baseline'
          aDesiredSize.ascent = dy + rowAscent;
          break;
        }
      }
      // in other situations, fallback to center
      aDesiredSize.ascent = dy + height/2;
      break;
    case eAlign_axis:
    default: {
      // XXX should instead use style data from the row of reference here ?
      const nsStyleFont* font;
      GetStyleData(eStyleStruct_Font, (const nsStyleStruct *&)font);
      aReflowState.rendContext->SetFont(font->mFont);
      nsCOMPtr<nsIFontMetrics> fm;
      aReflowState.rendContext->GetFontMetrics(*getter_AddRefs(fm));
      nscoord axisHeight;
      GetAxisHeight(*aReflowState.rendContext, fm, axisHeight);
      if (rowFrame) {
        // anchor the table on the axis of the row of reference
        // XXX fallback to baseline because it is a hard problem
        // XXX need to fetch the axis of the row; would need rowalign=axis to work better
        nscoord rowAscent = ((nsTableRowFrame*)rowFrame)->GetMaxCellAscent();
        if (rowAscent) { // the row has at least one cell with 'vertical-align: baseline'
          aDesiredSize.ascent = dy + rowAscent;
          break;
        }
      }
      // in other situations, fallback to using half of the height
      aDesiredSize.ascent = dy + height/2 + axisHeight;
    }
  }
  aDesiredSize.descent = aDesiredSize.height - aDesiredSize.ascent;

  mReference.x = 0;
  mReference.y = aDesiredSize.ascent;

  // just make-up a bounding metrics
  mBoundingMetrics.Clear();
  mBoundingMetrics.ascent = aDesiredSize.ascent;
  mBoundingMetrics.descent = aDesiredSize.descent;
  mBoundingMetrics.width = aDesiredSize.width;
  mBoundingMetrics.leftBearing = 0;
  mBoundingMetrics.rightBearing = aDesiredSize.width;

  aDesiredSize.mBoundingMetrics = mBoundingMetrics;

  return rv;
}

// --------
// implementation of nsMathMLmtdFrame

NS_IMPL_ADDREF_INHERITED(nsMathMLmtdFrame, nsTableCellFrame)
NS_IMPL_RELEASE_INHERITED(nsMathMLmtdFrame, nsTableCellFrame)
NS_IMPL_QUERY_INTERFACE_INHERITED0(nsMathMLmtdFrame, nsTableCellFrame)

nsresult
NS_NewMathMLmtdFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsMathMLmtdFrame* it = new (aPresShell) nsMathMLmtdFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

nsMathMLmtdFrame::nsMathMLmtdFrame()
{
}

nsMathMLmtdFrame::~nsMathMLmtdFrame()
{
}

PRInt32
nsMathMLmtdFrame::GetRowSpan()
{
  PRInt32 rowspan = 1;
  nsAutoString value;
  if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttr(kNameSpaceID_None,
                   nsMathMLAtoms::rowspan_, value)) {
    PRInt32 error;
    rowspan = value.ToInteger(&error);
    if (error)
      rowspan = 1;
  }
  return rowspan;
}

PRInt32
nsMathMLmtdFrame::GetColSpan()
{
  PRInt32 colspan = 1;
  nsAutoString value;
  if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttr(kNameSpaceID_None,
                   nsMathMLAtoms::columnspan_, value)) {
    PRInt32 error;
    colspan = value.ToInteger(&error);
    if (error)
      colspan = 1;
  }
  return colspan;
}

// --------
// implementation of nsMathMLmtdInnerFrame

NS_IMPL_ADDREF_INHERITED(nsMathMLmtdInnerFrame, nsMathMLFrame)
NS_IMPL_RELEASE_INHERITED(nsMathMLmtdInnerFrame, nsMathMLFrame)
NS_IMPL_QUERY_INTERFACE_INHERITED1(nsMathMLmtdInnerFrame, nsBlockFrame, nsMathMLFrame)

nsresult
NS_NewMathMLmtdInnerFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsMathMLmtdInnerFrame* it = new (aPresShell) nsMathMLmtdInnerFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

nsMathMLmtdInnerFrame::nsMathMLmtdInnerFrame()
{
}

nsMathMLmtdInnerFrame::~nsMathMLmtdInnerFrame()
{
}

NS_IMETHODIMP
nsMathMLmtdInnerFrame::Init(nsIPresContext*  aPresContext,
                            nsIContent*      aContent,
                            nsIFrame*        aParent,
                            nsIStyleContext* aContext,
                            nsIFrame*        aPrevInFlow)
{
  nsresult rv = nsBlockFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);

  // record that children that are ignorable whitespace should be excluded
  mState |= NS_FRAME_EXCLUDE_IGNORABLE_WHITESPACE;

  return rv;
}

NS_IMETHODIMP
nsMathMLmtdInnerFrame::Reflow(nsIPresContext*          aPresContext,
                              nsHTMLReflowMetrics&     aDesiredSize,
                              const nsHTMLReflowState& aReflowState,
                              nsReflowStatus&          aStatus)
{
  // Map attributes to style (hopefully, bug 69409 will eventually help here).
  if (NS_FRAME_FIRST_REFLOW & mState) {
    MapAttributesInto(aPresContext, mContent, mParent, this);
  }

  // Let the base class do the reflow
  nsresult rv = nsBlockFrame::Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);

  // more about <maligngroup/> and <malignmark/> later
  // ...
  return rv;
}
