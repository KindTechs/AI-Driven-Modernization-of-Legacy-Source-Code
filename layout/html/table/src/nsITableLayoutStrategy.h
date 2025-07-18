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

#ifndef nsITableLayoutStrategy_h__
#define nsITableLayoutStrategy_h__

#include "nscore.h"
#include "nsSize.h"

class nsIPresContext;
class nsIStyleContext;
struct nsHTMLReflowState;
struct nsMargin;
class nsTableCellFrame;
class nsStyleCoord;
class nsISizeOfHandler;

class nsITableLayoutStrategy
{
public:
  virtual ~nsITableLayoutStrategy() {};

  /** call once every time any table thing changes (content, structure, or style) 
    * @param aPresContext - the presentation context
	   * @param aReflowState - the reflow state for mTableFrame
    */
  virtual PRBool Initialize(nsIPresContext*          aPresContext,
                            const nsHTMLReflowState& aReflowState)=0;

  /** assign widths for each column, taking into account the table content, the effective style, 
    * the layout constraints, and the compatibility mode.  Sets mColumnWidths as a side effect.
    * @param aPresContext - the presentation context
	   * @param aReflowState - the reflow state for mTableFrame
    */
  virtual PRBool BalanceColumnWidths(nsIPresContext*          aPresContext,
                                     const nsHTMLReflowState& aReflowState)=0;

  /**
    * Calculate the basis for percent width calculations of the table elements
    * @param aReflowState   - the reflow state of the table
    * @param aAvailWidth    - the available width for the table
    * @param aPixelToTwips  - the number of twips in a pixel.
    * @return               - the basis for percent calculations
    */
  virtual nscoord CalcPctAdjTableWidth(nsIPresContext&          aPresContext,
                                       const nsHTMLReflowState& aReflowState,
                                       nscoord                  aAvailWidth,
                                       float                    aPixelToTwips)=0;

#ifdef DEBUG
  virtual void SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const = 0;
#endif
};

#endif

