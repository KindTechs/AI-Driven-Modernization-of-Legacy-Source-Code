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

#include "nsMathMLmstyleFrame.h"

//
// <mstyle> -- style change
//

nsresult
NS_NewMathMLmstyleFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsMathMLmstyleFrame* it = new (aPresShell) nsMathMLmstyleFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;  
  return NS_OK;
}

nsMathMLmstyleFrame::nsMathMLmstyleFrame()
{
}

nsMathMLmstyleFrame::~nsMathMLmstyleFrame()
{
}

// mstyle needs special care for its scriptlevel and displaystyle attributes
NS_IMETHODIMP
nsMathMLmstyleFrame::InheritAutomaticData(nsIPresContext* aPresContext,
                                          nsIFrame*       aParent) 
{
  // let the base class get the default from our parent
  nsMathMLContainerFrame::InheritAutomaticData(aPresContext, aParent);

  // sync with our current state
  mPresentationData.flags |= NS_MATHML_STRETCH_ALL_CHILDREN_VERTICALLY;
  mPresentationData.mstyle = this;

  // see if the displaystyle attribute is there
  nsAutoString value;
  if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttr(kNameSpaceID_None, 
                   nsMathMLAtoms::displaystyle_, value)) {
    if (value.Equals(NS_LITERAL_STRING("true"))) {
      mPresentationData.flags |= NS_MATHML_MSTYLE_WITH_DISPLAYSTYLE;
      mPresentationData.flags |= NS_MATHML_DISPLAYSTYLE;
    }
    else if (value.Equals(NS_LITERAL_STRING("false"))) {
      mPresentationData.flags |= NS_MATHML_MSTYLE_WITH_DISPLAYSTYLE;
      mPresentationData.flags &= ~NS_MATHML_DISPLAYSTYLE;
    }
  }

  // see if the scriptlevel attribute is there
  if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttr(kNameSpaceID_None, 
                   nsMathMLAtoms::scriptlevel_, value)) {
    PRInt32 errorCode, userValue;
    userValue = value.ToInteger(&errorCode); 
    if (!errorCode) {
      if (value[0] != '+' && value[0] != '-') { // record that it is an explicit value
        mPresentationData.flags |= NS_MATHML_MSTYLE_WITH_EXPLICIT_SCRIPTLEVEL;
        mPresentationData.scriptLevel = userValue;
      }
      else {
        mPresentationData.scriptLevel += userValue; // incremental value...
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMathMLmstyleFrame::TransmitAutomaticData(nsIPresContext* aPresContext)
{
  // Nothing particular to do here, the values that we computed in
  // InheritAutomaticData() are the values that we wanted to pass to
  // our children. Our children would have inherited these values in
  // their own InheritAutomaticData() as we descended the frame tree.

  return NS_OK;
}

NS_IMETHODIMP
nsMathMLmstyleFrame::UpdatePresentationData(nsIPresContext* aPresContext,
                                            PRInt32         aScriptLevelIncrement,
                                            PRUint32        aFlagsValues,
                                            PRUint32        aFlagsToUpdate)
{
  // mstyle is special...
  // Since UpdatePresentationData() can be called by a parent frame, the
  // scriptlevel and displaystyle attributes of mstyle must take precedence.
  // Update only if attributes are not there

  // see if updating the displaystyle flag is allowed
  if (!NS_MATHML_IS_MSTYLE_WITH_DISPLAYSTYLE(mPresentationData.flags)) {
    // see if the displaystyle flag is relevant to this call
    if (NS_MATHML_IS_DISPLAYSTYLE(aFlagsToUpdate)) {
      if (NS_MATHML_IS_DISPLAYSTYLE(aFlagsValues)) {
        mPresentationData.flags |= NS_MATHML_DISPLAYSTYLE;
      }
      else {
        mPresentationData.flags &= ~NS_MATHML_DISPLAYSTYLE;
      }
    }
  }

  // see if updating the scriptlevel is allowed
  if (!NS_MATHML_IS_MSTYLE_WITH_EXPLICIT_SCRIPTLEVEL(mPresentationData.flags)) {
    mPresentationData.scriptLevel += aScriptLevelIncrement;
  }

  // see if the compression flag is relevant to this call
  if (NS_MATHML_IS_COMPRESSED(aFlagsToUpdate)) {
    if (NS_MATHML_IS_COMPRESSED(aFlagsValues)) {
      // 'compressed' means 'prime' style in App. G, TeXbook
      mPresentationData.flags |= NS_MATHML_COMPRESSED;
    }
    // no else. the flag is sticky. it retains its value once it is set
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMathMLmstyleFrame::UpdatePresentationDataFromChildAt(nsIPresContext* aPresContext,
                                                       PRInt32         aFirstIndex,
                                                       PRInt32         aLastIndex,
                                                       PRInt32         aScriptLevelIncrement,
                                                       PRUint32        aFlagsValues,
                                                       PRUint32        aFlagsToUpdate)
{
  // mstyle is special...
  // Since UpdatePresentationDataFromChildAt() can be called by a parent frame,
  // wee need to ensure that the attributes of mstyle take precedence

  if (NS_MATHML_IS_DISPLAYSTYLE(aFlagsToUpdate)) {
    if (NS_MATHML_IS_MSTYLE_WITH_DISPLAYSTYLE(mPresentationData.flags)) {
      // our current state takes precedence, updating is not allowed
      aFlagsToUpdate &= ~NS_MATHML_DISPLAYSTYLE;
      aFlagsValues &= ~NS_MATHML_DISPLAYSTYLE;
    }
  }

  if (NS_MATHML_IS_MSTYLE_WITH_EXPLICIT_SCRIPTLEVEL(mPresentationData.flags)) {
    // our current state takes precedence, updating is not allowed
    aScriptLevelIncrement = 0;
  }

  // let the base class worry about the update
  return
    nsMathMLContainerFrame::UpdatePresentationDataFromChildAt(
      aPresContext, aFirstIndex, aLastIndex, aScriptLevelIncrement,
      aFlagsValues, aFlagsToUpdate); 
}

NS_IMETHODIMP
nsMathMLmstyleFrame::AttributeChanged(nsIPresContext* aPresContext,
                                      nsIContent*     aContent,
                                      PRInt32         aNameSpaceID,
                                      nsIAtom*        aAttribute,
                                      PRInt32         aModType, 
                                      PRInt32         aHint)
{
  // These attributes can affect too many things, ask our parent to re-layout
  // its children so that we can pick up changes in our attributes & transmit
  // them in our subtree. However, our siblings will be re-laid too. We used
  // to have a more speedier but more verbose alternative that didn't re-layout
  // our siblings. See bug 114909 - attachment 67668.
  return ReLayoutChildren(aPresContext, mParent);
}
