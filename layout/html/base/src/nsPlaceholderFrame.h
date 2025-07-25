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
#ifndef nsPlaceholderFrame_h___
#define nsPlaceholderFrame_h___

#include "nsFrame.h"

nsresult NS_NewPlaceholderFrame(nsIPresShell* aPresShell, nsIFrame**  aInstancePtrResult);

/**
 * Implementation of a frame that's used as a placeholder for a frame that
 * has been moved out of the flow
 */
class nsPlaceholderFrame : public nsFrame {
public:
  /**
   * Create a new placeholder frame
   */
  friend nsresult NS_NewPlaceholderFrame(nsIPresShell* aPresShell, nsIFrame**  aInstancePtrResult);

  // Get/Set the associated out of flow frame
  nsIFrame*  GetOutOfFlowFrame() const {return mOutOfFlowFrame;}
  void       SetOutOfFlowFrame(nsIFrame* aFrame) {mOutOfFlowFrame = aFrame;}

  // nsIHTMLReflow overrides
  NS_IMETHOD Reflow(nsIPresContext* aPresContext,
                    nsHTMLReflowMetrics& aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus& aStatus);

  // nsIFrame overrides
#ifdef DEBUG
  NS_IMETHOD Paint(nsIPresContext*      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect,
                   nsFramePaintLayer    aWhichLayer,
                   PRUint32             aFlags = 0);
  NS_IMETHOD List(nsIPresContext* aPresContext, FILE* out, PRInt32 aIndent) const;
#endif

  /**
   * Get the "type" of the frame
   *
   * @see nsLayoutAtoms::placeholderFrame
   */
  NS_IMETHOD GetFrameType(nsIAtom** aType) const;

#ifdef DEBUG
  NS_IMETHOD SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const;
  NS_IMETHOD GetFrameName(nsAString& aResult) const;
#endif

  NS_IMETHOD IsEmpty(PRBool aIsQuirkMode, PRBool aIsPre, PRBool *aResult);

protected:
  nsIFrame* mOutOfFlowFrame;
};

#endif /* nsPlaceholderFrame_h___ */
