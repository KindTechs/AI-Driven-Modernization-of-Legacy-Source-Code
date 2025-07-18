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
 *   Shyjan Mahamud <mahamud@cs.cmu.edu> (added TeX rendering rules)
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

#include "nsMathMLmmultiscriptsFrame.h"

//
// <mmultiscripts> -- attach prescripts and tensor indices to a base - implementation
//

nsresult
NS_NewMathMLmmultiscriptsFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsMathMLmmultiscriptsFrame* it = new (aPresShell) nsMathMLmmultiscriptsFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

nsMathMLmmultiscriptsFrame::nsMathMLmmultiscriptsFrame()
{
}

nsMathMLmmultiscriptsFrame::~nsMathMLmmultiscriptsFrame()
{
}

NS_IMETHODIMP
nsMathMLmmultiscriptsFrame::TransmitAutomaticData(nsIPresContext* aPresContext)
{
  // if our base is an embellished operator, let its state bubble to us
  GetEmbellishDataFrom(mFrames.FirstChild(), mEmbellishData);
  if (NS_MATHML_IS_EMBELLISH_OPERATOR(mEmbellishData.flags))
    mEmbellishData.nextFrame = mFrames.FirstChild();

  // The REC says:
  // The <mmultiscripts> element increments scriptlevel by 1, and sets
  // displaystyle to "false", within each of its arguments except base
  UpdatePresentationDataFromChildAt(aPresContext, 1, -1, 1,
    ~NS_MATHML_DISPLAYSTYLE, NS_MATHML_DISPLAYSTYLE);

  // The TeXbook (Ch 17. p.141) says the superscript inherits the compression
  // while the subscript is compressed. So here we collect subscripts and set
  // the compression flag in them.
  PRInt32 count = 0;
  PRBool isSubScript = PR_FALSE;
  nsAutoVoidArray subScriptFrames;
  nsIFrame* childFrame = mFrames.FirstChild();
  while (childFrame) {
    nsCOMPtr<nsIContent> childContent;
    nsCOMPtr<nsIAtom> childTag;
    childFrame->GetContent(getter_AddRefs(childContent));
    childContent->GetTag(*getter_AddRefs(childTag));
    if (childTag.get() == nsMathMLAtoms::mprescripts_) {
      // mprescripts frame
    }
    else if (0 == count) {
      // base frame
    }
    else {
      // super/subscript block
      if (isSubScript) {
        // subscript
        subScriptFrames.AppendElement(childFrame);
      }
      else {
        // superscript
      }
      isSubScript = !isSubScript;
    }
    count++;
    childFrame->GetNextSibling(&childFrame);
  }
  for (PRInt32 i = subScriptFrames.Count() - 1; i >= 0; i--) {
    childFrame = (nsIFrame*)subScriptFrames[i];
    PropagatePresentationDataFor(aPresContext, childFrame, 0,
      NS_MATHML_COMPRESSED, NS_MATHML_COMPRESSED);
  }

  return NS_OK;
}

void
nsMathMLmmultiscriptsFrame::ProcessAttributes(nsIPresContext* aPresContext)
{
  mSubScriptShift = 0;
  mSupScriptShift = 0;
  mScriptSpace = NSFloatPointsToTwips(0.5f); // 0.5pt as in plain TeX

  // check if the subscriptshift attribute is there
  nsAutoString value;
  if (NS_CONTENT_ATTR_HAS_VALUE == GetAttribute(mContent, mPresentationData.mstyle,
                   nsMathMLAtoms::subscriptshift_, value)) {
    nsCSSValue cssValue;
    if (ParseNumericValue(value, cssValue) && cssValue.IsLengthUnit()) {
      mSubScriptShift = CalcLength(aPresContext, mStyleContext, cssValue);
    }
  }
  // check if the superscriptshift attribute is there
  if (NS_CONTENT_ATTR_HAS_VALUE == GetAttribute(mContent, mPresentationData.mstyle,
                   nsMathMLAtoms::superscriptshift_, value)) {
    nsCSSValue cssValue;
    if (ParseNumericValue(value, cssValue) && cssValue.IsLengthUnit()) {
      mSupScriptShift = CalcLength(aPresContext, mStyleContext, cssValue);
    }
  }
}

NS_IMETHODIMP
nsMathMLmmultiscriptsFrame::Place(nsIPresContext*      aPresContext,
                                  nsIRenderingContext& aRenderingContext,
                                  PRBool               aPlaceOrigin,
                                  nsHTMLReflowMetrics& aDesiredSize)
{
  ////////////////////////////////////
  // Get the children's desired sizes

  nscoord minShiftFromXHeight, subDrop, supDrop;

  ////////////////////////////////////////
  // Initialize super/sub shifts that
  // depend only on the current font
  ////////////////////////////////////////

  ProcessAttributes(aPresContext);

  // get x-height (an ex)
  const nsStyleFont *font = NS_STATIC_CAST(const nsStyleFont*,
    mStyleContext->GetStyleData(eStyleStruct_Font));
  aRenderingContext.SetFont(font->mFont);
  nsCOMPtr<nsIFontMetrics> fm;
  aRenderingContext.GetFontMetrics(*getter_AddRefs(fm));

  nscoord xHeight;
  fm->GetXHeight (xHeight);

  nscoord ruleSize;
  GetRuleThickness (aRenderingContext, fm, ruleSize);

  /////////////////////////////////////
  // first the shift for the subscript

  // subScriptShift{1,2}
  // = minimum amount to shift the subscript down
  // = sub{1,2} in TeXbook
  // subScriptShift1 = subscriptshift attribute * x-height
  nscoord subScriptShift1, subScriptShift2;

  // Get subScriptShift{1,2} default from font
  GetSubScriptShifts (fm, subScriptShift1, subScriptShift2);
  if (0 < mSubScriptShift) {
    // the user has set the subscriptshift attribute
    float scaler = ((float) subScriptShift2) / subScriptShift1;
    subScriptShift1 = PR_MAX(subScriptShift1, mSubScriptShift);
    subScriptShift2 = NSToCoordRound(scaler * subScriptShift1);
  }
  // the font dependent shift
  nscoord subScriptShift = PR_MAX(subScriptShift1,subScriptShift2);

  /////////////////////////////////////
  // next the shift for the superscript

  // supScriptShift{1,2,3}
  // = minimum amount to shift the supscript up
  // = sup{1,2,3} in TeX
  // supScriptShift1 = superscriptshift attribute * x-height
  // Note that there are THREE values for supscript shifts depending
  // on the current style
  nscoord supScriptShift1, supScriptShift2, supScriptShift3;
  // Set supScriptShift{1,2,3} default from font
  GetSupScriptShifts (fm, supScriptShift1, supScriptShift2, supScriptShift3);
  if (0 < mSupScriptShift) {
    // the user has set the superscriptshift attribute
    float scaler2 = ((float) supScriptShift2) / supScriptShift1;
    float scaler3 = ((float) supScriptShift3) / supScriptShift1;
    supScriptShift1 = PR_MAX(supScriptShift1, mSupScriptShift);
    supScriptShift2 = NSToCoordRound(scaler2 * supScriptShift1);
    supScriptShift3 = NSToCoordRound(scaler3 * supScriptShift1);
  }

  // get sup script shift depending on current script level and display style
  // Rule 18c, App. G, TeXbook
  nscoord supScriptShift;
  if ( mPresentationData.scriptLevel == 0 &&
       NS_MATHML_IS_DISPLAYSTYLE(mPresentationData.flags) &&
      !NS_MATHML_IS_COMPRESSED(mPresentationData.flags)) {
    // Style D in TeXbook
    supScriptShift = supScriptShift1;
  }
  else if (NS_MATHML_IS_COMPRESSED(mPresentationData.flags)) {
    // Style C' in TeXbook = D',T',S',SS'
    supScriptShift = supScriptShift3;
  }
  else {
    // everything else = T,S,SS
    supScriptShift = supScriptShift2;
  }

  ////////////////////////////////////
  // Get the children's sizes
  ////////////////////////////////////

  nscoord width = 0, prescriptsWidth = 0, rightBearing = 0;
  nsIFrame* mprescriptsFrame = nsnull; // frame of <mprescripts/>, if there.
  PRBool isSubScript = PR_FALSE;
  nscoord minSubScriptShift = 0, minSupScriptShift = 0;
  nscoord trySubScriptShift = subScriptShift;
  nscoord trySupScriptShift = supScriptShift;
  nscoord maxSubScriptShift = subScriptShift;
  nscoord maxSupScriptShift = supScriptShift;
  PRInt32 count = 0;
  nsHTMLReflowMetrics baseSize (nsnull);
  nsHTMLReflowMetrics subScriptSize (nsnull);
  nsHTMLReflowMetrics supScriptSize (nsnull);
  nsIFrame* baseFrame = nsnull;
  nsIFrame* subScriptFrame = nsnull;
  nsIFrame* supScriptFrame = nsnull;

  PRBool firstPrescriptsPair = PR_FALSE;
  nsBoundingMetrics bmBase, bmSubScript, bmSupScript;
  nscoord italicCorrection = 0;

  mBoundingMetrics.width = 0;
  mBoundingMetrics.ascent = mBoundingMetrics.descent = -0x7FFFFFFF;
  aDesiredSize.ascent = aDesiredSize.descent = -0x7FFFFFFF;
  aDesiredSize.width = aDesiredSize.height = 0;

  nsIFrame* childFrame = mFrames.FirstChild();
  while (childFrame) {
    nsCOMPtr<nsIContent> childContent;
    nsCOMPtr<nsIAtom> childTag;
    childFrame->GetContent(getter_AddRefs(childContent));
    childContent->GetTag(*getter_AddRefs(childTag));

    if (childTag.get() == nsMathMLAtoms::mprescripts_) {
      if (mprescriptsFrame) {
        // duplicate <mprescripts/> found
        // report an error, encourage people to get their markups in order
        NS_WARNING("invalid markup");
        return ReflowError(aPresContext, aRenderingContext, aDesiredSize);
      }
      mprescriptsFrame = childFrame;
      firstPrescriptsPair = PR_TRUE;
    }
    else {
      if (0 == count) {
        // base
        baseFrame = childFrame;
        GetReflowAndBoundingMetricsFor(baseFrame, baseSize, bmBase);
        GetItalicCorrection(bmBase, italicCorrection);

        // we update mBoundingMetrics.{ascent,descent} with that
        // of the baseFrame only after processing all the sup/sub pairs
        // XXX need italic correction only *if* there are postscripts ?
        mBoundingMetrics.width = bmBase.width + italicCorrection;
        mBoundingMetrics.rightBearing = bmBase.rightBearing;
        mBoundingMetrics.leftBearing = bmBase.leftBearing; // until overwritten
      }
      else {
        // super/subscript block
        if (isSubScript) {
          // subscript
          subScriptFrame = childFrame;
          GetReflowAndBoundingMetricsFor(subScriptFrame, subScriptSize, bmSubScript);
          // get the subdrop from the subscript font
          GetSubDropFromChild (aPresContext, subScriptFrame, subDrop);
          // parameter v, Rule 18a, App. G, TeXbook
          minSubScriptShift = bmBase.descent + subDrop;
          trySubScriptShift = PR_MAX(minSubScriptShift,subScriptShift);
          mBoundingMetrics.descent =
            PR_MAX(mBoundingMetrics.descent,bmSubScript.descent);
          aDesiredSize.descent =
            PR_MAX(aDesiredSize.descent,subScriptSize.descent);
          width = bmSubScript.width + mScriptSpace;
          rightBearing = bmSubScript.rightBearing + mScriptSpace;          
        }
        else {
          // supscript
          supScriptFrame = childFrame;
          GetReflowAndBoundingMetricsFor(supScriptFrame, supScriptSize, bmSupScript);
          // get the supdrop from the supscript font
          GetSupDropFromChild (aPresContext, supScriptFrame, supDrop);
          // parameter u, Rule 18a, App. G, TeXbook
          minSupScriptShift = bmBase.ascent - supDrop;
          // get min supscript shift limit from x-height
          // = d(x) + 1/4 * sigma_5, Rule 18c, App. G, TeXbook
          minShiftFromXHeight = NSToCoordRound
            ((bmSupScript.descent + (1.0f/4.0f) * xHeight));
          trySupScriptShift =
            PR_MAX(minSupScriptShift,PR_MAX(minShiftFromXHeight,supScriptShift));
          mBoundingMetrics.ascent =
            PR_MAX(mBoundingMetrics.ascent,bmSupScript.ascent);
          aDesiredSize.ascent =
            PR_MAX(aDesiredSize.ascent,supScriptSize.ascent);
          width = PR_MAX(width, bmSupScript.width + mScriptSpace);
          rightBearing = PR_MAX(rightBearing, bmSupScript.rightBearing + mScriptSpace);

          if (!mprescriptsFrame) { // we are still looping over base & postscripts
            mBoundingMetrics.rightBearing = mBoundingMetrics.width + rightBearing;
            mBoundingMetrics.width += width;
          }
          else {
            prescriptsWidth += width;
            if (firstPrescriptsPair) {
              firstPrescriptsPair = PR_FALSE;
              mBoundingMetrics.leftBearing =
                PR_MIN(bmSubScript.leftBearing, bmSupScript.leftBearing);
            }
          }
          width = rightBearing = 0;

          // negotiate between the various shifts so that
          // there is enough gap between the sup and subscripts
          // Rule 18e, App. G, TeXbook
          nscoord gap =
            (trySupScriptShift - bmSupScript.descent) -
            (bmSubScript.ascent - trySubScriptShift);
          if (gap < 4.0f * ruleSize) {
            // adjust trySubScriptShift to get a gap of (4.0 * ruleSize)
            trySubScriptShift += NSToCoordRound ((4.0f * ruleSize) - gap);
          }

          // next we want to ensure that the bottom of the superscript
          // will be > (4/5) * x-height above baseline
          gap = NSToCoordRound ((4.0f/5.0f) * xHeight -
                  (trySupScriptShift - bmSupScript.descent));
          if (gap > 0.0f) {
            trySupScriptShift += gap;
            trySubScriptShift -= gap;
          }
          
          maxSubScriptShift = PR_MAX(maxSubScriptShift, trySubScriptShift);
          maxSupScriptShift = PR_MAX(maxSupScriptShift, trySupScriptShift);

          trySubScriptShift = subScriptShift;
          trySupScriptShift = supScriptShift;
        }
      }

      isSubScript = !isSubScript;
    }
    count++;
    childFrame->GetNextSibling(&childFrame);
  }
  // note: width=0 if all sup-sub pairs match correctly
  if ((0 != width) || !baseFrame || !subScriptFrame || !supScriptFrame) {
    // report an error, encourage people to get their markups in order
    NS_WARNING("invalid markup");
    return ReflowError(aPresContext, aRenderingContext, aDesiredSize);
  }

  // we left out the width of prescripts, so ...
  mBoundingMetrics.rightBearing += prescriptsWidth;
  mBoundingMetrics.width += prescriptsWidth;

  // we left out the base during our bounding box updates, so ...
  mBoundingMetrics.ascent =
    PR_MAX(mBoundingMetrics.ascent+maxSupScriptShift,bmBase.ascent);
  mBoundingMetrics.descent =
    PR_MAX(mBoundingMetrics.descent+maxSubScriptShift,bmBase.descent);

  // get the reflow metrics ...
  aDesiredSize.ascent =
    PR_MAX(aDesiredSize.ascent+maxSupScriptShift,baseSize.ascent);
  aDesiredSize.descent =
    PR_MAX(aDesiredSize.descent+maxSubScriptShift,baseSize.descent);
  aDesiredSize.height = aDesiredSize.ascent + aDesiredSize.descent;
  aDesiredSize.width = mBoundingMetrics.width;
  aDesiredSize.mBoundingMetrics = mBoundingMetrics;

  mReference.x = 0;
  mReference.y = aDesiredSize.ascent;

  //////////////////
  // Place Children

  // Place prescripts, followed by base, and then postscripts.
  // The list of frames is in the order: {base} {postscripts} {prescripts}
  // We go over the list in a circular manner, starting at <prescripts/>

  if (aPlaceOrigin) {
    nscoord dx = 0, dy = 0;

    count = 0;
    childFrame = mprescriptsFrame;
    do {
      if (!childFrame) { // end of prescripts,
        // place the base ...
        childFrame = baseFrame;
        dy = aDesiredSize.ascent - baseSize.ascent;
        FinishReflowChild (baseFrame, aPresContext, nsnull, baseSize, dx, dy, 0);
        dx += bmBase.width + mScriptSpace + italicCorrection;
      }
      else if (mprescriptsFrame != childFrame) {
        // process each sup/sub pair
        if (0 == count) {
          subScriptFrame = childFrame;
          count = 1;
        }
        else if (1 == count) {
          supScriptFrame = childFrame;
          count = 0;

          // get the ascent/descent of sup/subscripts stored in their rects
          // rect.x = descent, rect.y = ascent
          GetReflowAndBoundingMetricsFor(subScriptFrame, subScriptSize, bmSubScript);
          GetReflowAndBoundingMetricsFor(supScriptFrame, supScriptSize, bmSupScript);

          // center w.r.t. largest width
          width = PR_MAX(subScriptSize.width, supScriptSize.width);

          dy = aDesiredSize.ascent - subScriptSize.ascent +
            maxSubScriptShift;
          FinishReflowChild (subScriptFrame, aPresContext, nsnull, subScriptSize,
                             dx + (width-subScriptSize.width)/2, dy, 0);

          dy = aDesiredSize.ascent - supScriptSize.ascent -
            maxSupScriptShift;
          FinishReflowChild (supScriptFrame, aPresContext, nsnull, supScriptSize,
                             dx + (width-supScriptSize.width)/2, dy, 0);

          dx += mScriptSpace + width;
        }
      }
      childFrame->GetNextSibling(&childFrame);
    } while (mprescriptsFrame != childFrame);
  }

  return NS_OK;
}
