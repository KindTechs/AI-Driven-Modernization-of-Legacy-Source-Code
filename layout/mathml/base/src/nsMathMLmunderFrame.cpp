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
 *   David J. Fiddes <D.J.Fiddes@hw.ac.uk>
 *   Shyjan Mahamud <mahamud@cs.cmu.edu>
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */


#include "nsCOMPtr.h"
#include "nsHTMLParts.h"
#include "nsIHTMLContent.h"
#include "nsFrame.h"
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

#include "nsMathMLmunderFrame.h"
#include "nsMathMLmsubFrame.h"

//
// <munder> -- attach an underscript to a base - implementation
//

nsresult
NS_NewMathMLmunderFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsMathMLmunderFrame* it = new (aPresShell) nsMathMLmunderFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

nsMathMLmunderFrame::nsMathMLmunderFrame()
{
}

nsMathMLmunderFrame::~nsMathMLmunderFrame()
{
}

NS_IMETHODIMP
nsMathMLmunderFrame::AttributeChanged(nsIPresContext* aPresContext,
                                      nsIContent*     aContent,
                                      PRInt32         aNameSpaceID,
                                      nsIAtom*        aAttribute,
                                      PRInt32         aModType, 
                                      PRInt32         aHint)
{
  if (nsMathMLAtoms::accentunder_ == aAttribute) {
    // When we have automatic data to update within ourselves, we ask our
    // parent to re-layout its children
    return ReLayoutChildren(aPresContext, mParent);
  }

  return nsMathMLContainerFrame::
         AttributeChanged(aPresContext, aContent, aNameSpaceID,
                          aAttribute, aModType, aHint);
}

NS_IMETHODIMP
nsMathMLmunderFrame::UpdatePresentationData(nsIPresContext* aPresContext,
                                            PRInt32         aScriptLevelIncrement,
                                            PRUint32        aFlagsValues,
                                            PRUint32        aFlagsToUpdate)
{
  nsMathMLContainerFrame::UpdatePresentationData(aPresContext,
    aScriptLevelIncrement, aFlagsValues, aFlagsToUpdate);
  // disable the stretch-all flag if we are going to act like a subscript
  if ( NS_MATHML_EMBELLISH_IS_MOVABLELIMITS(mEmbellishData.flags) &&
      !NS_MATHML_IS_DISPLAYSTYLE(mPresentationData.flags)) {
    mPresentationData.flags &= ~NS_MATHML_STRETCH_ALL_CHILDREN_HORIZONTALLY;
  }
  else {
    mPresentationData.flags |= NS_MATHML_STRETCH_ALL_CHILDREN_HORIZONTALLY;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsMathMLmunderFrame::UpdatePresentationDataFromChildAt(nsIPresContext* aPresContext,
                                                       PRInt32         aFirstIndex,
                                                       PRInt32         aLastIndex,
                                                       PRInt32         aScriptLevelIncrement,
                                                       PRUint32        aFlagsValues,
                                                       PRUint32        aFlagsToUpdate)
{
  // munder is special... The REC says:
  // Within underscript, <munder> always sets displaystyle to "false", 
  // but increments scriptlevel by 1 only when accentunder is "false".
  // This means that
  // 1. don't allow displaystyle to change in the underscript
  // 2. if the value of the accent is changed, we need to recompute the
  //    scriptlevel of the underscript. The problem is that the accent
  //    can change in the <mo> deep down the embellished hierarchy

  // Do #1 here, never allow displaystyle to be changed in the underscript
  PRInt32 index = 0;
  nsIFrame* childFrame = mFrames.FirstChild();
  while (childFrame) {
    if ((index >= aFirstIndex) &&
        ((aLastIndex <= 0) || ((aLastIndex > 0) && (index <= aLastIndex)))) {
      if (index > 0) {
        // disable the flag
        aFlagsToUpdate &= ~NS_MATHML_DISPLAYSTYLE;
        aFlagsValues &= ~NS_MATHML_DISPLAYSTYLE;
      }
      PropagatePresentationDataFor(aPresContext, childFrame,
        aScriptLevelIncrement, aFlagsValues, aFlagsToUpdate);
    }
    index++;
    childFrame->GetNextSibling(&childFrame);
  }
  return NS_OK;

  // For #2, changing the accent attribute will trigger a re-build of
  // all automatic data in the embellished hierarchy
}

NS_IMETHODIMP
nsMathMLmunderFrame::InheritAutomaticData(nsIPresContext* aPresContext,
                                          nsIFrame*       aParent)
{
  // let the base class get the default from our parent
  nsMathMLContainerFrame::InheritAutomaticData(aPresContext, aParent);

  mPresentationData.flags |= NS_MATHML_STRETCH_ALL_CHILDREN_HORIZONTALLY;

  return NS_OK;
}

NS_IMETHODIMP
nsMathMLmunderFrame::TransmitAutomaticData(nsIPresContext* aPresContext)
{
  // At this stage, all our children are in sync and we can fully
  // resolve our own mEmbellishData struct
  //---------------------------------------------------------------------

  /* The REC says:
  The default value of accentunder is false, unless underscript
  is an <mo> element or an embellished operator.  If underscript is 
  an <mo> element, the value of its accent attribute is used as the
  default value of accentunder. If underscript is an embellished
  operator, the accent attribute of the <mo> element at its
  core is used as the default value. As with all attributes, an
  explicitly given value overrides the default.

XXX The winner is the outermost setting in conflicting settings like these:
<munder accent='true'>
  <mi>...</mi>
  <mo accent='false'> ... </mo>
</munder>
   */

  nsIFrame* underscriptFrame = nsnull;
  nsIFrame* baseFrame = mFrames.FirstChild();
  if (baseFrame)
    baseFrame->GetNextSibling(&underscriptFrame);
  if (!baseFrame || !underscriptFrame)
    return NS_OK; // a visual error indicator will be reported later during layout

  // if our base is an embellished operator, let its state bubble to us (in particular,
  // this is where we get the flag for NS_MATHML_EMBELLISH_MOVABLELIMITS). Our flags
  // are reset to the default values of false if the base frame isn't embellished.
  GetEmbellishDataFrom(baseFrame, mEmbellishData);
  if (NS_MATHML_IS_EMBELLISH_OPERATOR(mEmbellishData.flags))
    mEmbellishData.nextFrame = baseFrame;

  nsAutoString value;

  // The default value of accentunder is false, unless the underscript is embellished
  // and its core <mo> is an accent
  nsEmbellishData embellishData;
  GetEmbellishDataFrom(underscriptFrame, embellishData);
  if (NS_MATHML_EMBELLISH_IS_ACCENT(embellishData.flags))
    mEmbellishData.flags |= NS_MATHML_EMBELLISH_ACCENTUNDER;
  else
    mEmbellishData.flags &= ~NS_MATHML_EMBELLISH_ACCENTUNDER;

  // if we have an accentunder attribute, it overrides what the underscript said
  if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttr(kNameSpaceID_None, 
                   nsMathMLAtoms::accentunder_, value)) {
    if (value.Equals(NS_LITERAL_STRING("true")))
      mEmbellishData.flags |= NS_MATHML_EMBELLISH_ACCENTUNDER;
    else if (value.Equals(NS_LITERAL_STRING("false"))) 
      mEmbellishData.flags &= ~NS_MATHML_EMBELLISH_ACCENTUNDER;
  }

  // disable the stretch-all flag if we are going to act like a superscript
  if ( NS_MATHML_EMBELLISH_IS_MOVABLELIMITS(mEmbellishData.flags) &&
      !NS_MATHML_IS_DISPLAYSTYLE(mPresentationData.flags))
    mPresentationData.flags &= ~NS_MATHML_STRETCH_ALL_CHILDREN_HORIZONTALLY;

  // Now transmit any change that we want to our children so that they
  // can update their mPresentationData structs
  //---------------------------------------------------------------------

  /* The REC says:
     Within underscript, <munder> always sets displaystyle to "false", 
     but increments scriptlevel by 1 only when accentunder is "false".

     The TeXBook treats 'under' like a subscript, so p.141 or Rule 13a 
     say it should be compressed
  */
  PRInt32 increment = NS_MATHML_EMBELLISH_IS_ACCENTUNDER(mEmbellishData.flags)
    ? 0 : 1;
  PropagatePresentationDataFor(aPresContext, underscriptFrame, increment,
    ~NS_MATHML_DISPLAYSTYLE | NS_MATHML_COMPRESSED,
     NS_MATHML_DISPLAYSTYLE | NS_MATHML_COMPRESSED);

  return NS_OK;
}

/*
The REC says:
* If the base is an operator with movablelimits="true" (or
  an embellished operator whose <mo> element core has
  movablelimits="true"), and displaystyle="false", then
  underscript is drawn in a subscript position. In this case,
  the accentunder attribute is ignored. This is often used
  for limits on symbols such as &sum;. 

i.e.,:
 if ( NS_MATHML_EMBELLISH_IS_MOVABLELIMITS(mEmbellishData.flags) &&
     !NS_MATHML_IS_DISPLAYSTYLE(mPresentationData.flags)) {
  // place like subscript
 }
 else {
  // place like underscript 
 }
*/

NS_IMETHODIMP
nsMathMLmunderFrame::Place(nsIPresContext*      aPresContext,
                           nsIRenderingContext& aRenderingContext,
                           PRBool               aPlaceOrigin,
                           nsHTMLReflowMetrics& aDesiredSize)
{
  if ( NS_MATHML_EMBELLISH_IS_MOVABLELIMITS(mEmbellishData.flags) &&
      !NS_MATHML_IS_DISPLAYSTYLE(mPresentationData.flags)) {
    // place like subscript
    return nsMathMLmsubFrame::PlaceSubScript(aPresContext,
                                             aRenderingContext,
                                             aPlaceOrigin,
                                             aDesiredSize,
                                             this);
  }

  ////////////////////////////////////
  // Get the children's desired sizes

  nsBoundingMetrics bmBase, bmUnder;
  nsHTMLReflowMetrics baseSize (nsnull);
  nsHTMLReflowMetrics underSize (nsnull);
  nsIFrame* underFrame = nsnull;
  nsIFrame* baseFrame = mFrames.FirstChild();
  if (baseFrame)
    baseFrame->GetNextSibling(&underFrame);
  if (!baseFrame || !underFrame || HasNextSibling(underFrame)) {
    // report an error, encourage people to get their markups in order
    NS_WARNING("invalid markup");
    return ReflowError(aPresContext, aRenderingContext, aDesiredSize);
  }
  GetReflowAndBoundingMetricsFor(baseFrame, baseSize, bmBase);
  GetReflowAndBoundingMetricsFor(underFrame, underSize, bmUnder);

  ////////////////////
  // Place Children

  const nsStyleFont* font =
    (const nsStyleFont*) mStyleContext->GetStyleData (eStyleStruct_Font);
  aRenderingContext.SetFont(font->mFont);
  nsCOMPtr<nsIFontMetrics> fm;
  aRenderingContext.GetFontMetrics(*getter_AddRefs(fm));

  nscoord xHeight = 0;
  fm->GetXHeight (xHeight);

  nscoord ruleThickness;
  GetRuleThickness (aRenderingContext, fm, ruleThickness);

  // there are 2 different types of placement depending on 
  // whether we want an accented under or not

  nscoord italicCorrection = 0;
  nscoord delta1 = 0; // gap between base and underscript
  nscoord delta2 = 0; // extra space beneath underscript
  if (!NS_MATHML_EMBELLISH_IS_ACCENTUNDER(mEmbellishData.flags)) {    
    // Rule 13a, App. G, TeXbook
    GetItalicCorrection (bmBase, italicCorrection);
    nscoord bigOpSpacing2, bigOpSpacing4, bigOpSpacing5, dummy; 
    GetBigOpSpacings (fm, 
                      dummy, bigOpSpacing2, 
                      dummy, bigOpSpacing4, 
                      bigOpSpacing5);
    delta1 = PR_MAX(bigOpSpacing2, (bigOpSpacing4 - bmUnder.ascent));
    delta2 = bigOpSpacing5;
  }
  else {
    // No corresponding rule in TeXbook - we are on our own here
    // XXX tune the gap delta between base and underscript 

    // Should we use Rule 10 like \underline does?
    delta1 = ruleThickness;
    delta2 = ruleThickness;
  }
  // empty under?
  if (!(bmUnder.ascent + bmUnder.descent)) delta1 = 0;

  mBoundingMetrics.ascent = bmBase.ascent;
  mBoundingMetrics.descent = 
    bmBase.descent + delta1 + bmUnder.ascent + bmUnder.descent;
  mBoundingMetrics.width = 
    PR_MAX(bmBase.width/2,(bmUnder.width + italicCorrection/2)/2) +
    PR_MAX(bmBase.width/2,(bmUnder.width - italicCorrection/2)/2);

  nscoord dxBase = (mBoundingMetrics.width - bmBase.width) / 2;
  nscoord dxUnder = (mBoundingMetrics.width - (bmUnder.width + italicCorrection/2)) / 2;
  
  mBoundingMetrics.leftBearing = 
    PR_MIN(dxBase + bmBase.leftBearing, dxUnder + bmUnder.leftBearing);
  mBoundingMetrics.rightBearing = 
    PR_MAX(dxBase + bmBase.rightBearing, dxUnder + bmUnder.rightBearing);

  aDesiredSize.ascent = baseSize.ascent;
  aDesiredSize.descent = 
    PR_MAX(mBoundingMetrics.descent + delta2,
           bmBase.descent + delta1 + bmUnder.ascent + underSize.descent);
  aDesiredSize.height = aDesiredSize.ascent + aDesiredSize.descent;
  aDesiredSize.width = 
    PR_MAX(baseSize.width/2,(underSize.width + italicCorrection/2)/2) +
    PR_MAX(baseSize.width/2,(underSize.width - italicCorrection/2)/2);
  aDesiredSize.mBoundingMetrics = mBoundingMetrics;

  mReference.x = 0;
  mReference.y = aDesiredSize.ascent;

  if (aPlaceOrigin) {
    nscoord dy = 0;
    // place base
    FinishReflowChild(baseFrame, aPresContext, nsnull, baseSize, dxBase, dy, 0);
    // place underscript
    dy = aDesiredSize.ascent + mBoundingMetrics.descent - bmUnder.descent - underSize.ascent;
    FinishReflowChild(underFrame, aPresContext, nsnull, underSize, dxUnder, dy, 0);
  }

  return NS_OK;
}

