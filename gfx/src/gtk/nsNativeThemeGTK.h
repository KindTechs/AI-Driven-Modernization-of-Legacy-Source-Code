/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brian Ryner <bryner@netscape.com>  (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsITheme.h"
#include "nsCOMPtr.h"
#include "nsIAtom.h"

#include "gtkdrawing.h"
#include <gtk/gtkwidget.h>

class nsNativeThemeGTK: public nsITheme {
public:
  NS_DECL_ISUPPORTS

  // The nsITheme interface.
  NS_IMETHOD DrawWidgetBackground(nsIRenderingContext* aContext,
                                  nsIFrame* aFrame,
                                  PRUint8 aWidgetType,
                                  const nsRect& aRect,
                                  const nsRect& aClipRect);

  NS_IMETHOD GetWidgetBorder(nsIDeviceContext* aContext, 
                             nsIFrame* aFrame,
                             PRUint8 aWidgetType,
                             nsMargin* aResult);

  NS_IMETHOD GetMinimumWidgetSize(nsIRenderingContext* aContext, nsIFrame* aFrame,
                                  PRUint8 aWidgetType,
                                  nsSize* aResult,
                                  PRBool* aIsOverridable);

  NS_IMETHOD WidgetStateChanged(nsIFrame* aFrame, PRUint8 aWidgetType, 
                                nsIAtom* aAttribute, PRBool* aShouldRepaint);

  NS_IMETHOD ThemeChanged();

  NS_IMETHOD_(PRBool) ThemeSupportsWidget(nsIPresContext* aPresContext,
                             PRUint8 aWidgetType);

  NS_IMETHOD_(PRBool) WidgetIsContainer(PRUint8 aWidgetType);

  nsNativeThemeGTK();
  virtual ~nsNativeThemeGTK();

protected:
  PRBool IsDisabled(nsIFrame* aFrame);

  void GetGtkWidgetState(PRUint8 aWidgetType,
                         nsIFrame* aFrame, GtkWidgetState* aState);

  void GetScrollbarMetrics(gint* slider_width,
                           gint* trough_border,
                           gint* stepper_size,
                           gint* stepper_spacing);

  void SetupWidgetPrototype(GtkWidget* widget);
  void EnsureButtonWidget();
  void EnsureCheckBoxWidget();
  void EnsureScrollbarWidget();
  void EnsureGripperWidget();
  void EnsureEntryWidget();
  void EnsureArrowWidget();
  void EnsureHandleBoxWidget();
  void EnsureTooltipWidget();
  void EnsureFrameWidget();
  void EnsureProgressBarWidget();
  void EnsureTabWidget();

private:
  nsCOMPtr<nsIAtom> mCheckedAtom;
  nsCOMPtr<nsIAtom> mDisabledAtom;
  nsCOMPtr<nsIAtom> mSelectedAtom;
  nsCOMPtr<nsIAtom> mTypeAtom;
  nsCOMPtr<nsIAtom> mInputCheckedAtom;
  nsCOMPtr<nsIAtom> mInputAtom;
  nsCOMPtr<nsIAtom> mFocusedAtom;

  GtkWidget* mProtoLayout;
};
