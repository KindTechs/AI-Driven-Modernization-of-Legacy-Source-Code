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

#include "nsLabel.h"
#include "nsILabel.h"
#include "nsToolkit.h"
#include "nsColor.h"
#include "nsGUIEvent.h"
#include "nsString.h"
#include "nsIFontMetrics.h"
#include "nsIDeviceContext.h"

NS_IMPL_ADDREF(nsLabel)
NS_IMPL_RELEASE(nsLabel)

//-------------------------------------------------------------------------
//
// nsLabel constructor
//
//-------------------------------------------------------------------------
nsLabel::nsLabel() : nsWindow(), nsILabel()
{
  NS_INIT_REFCNT();
  mAlignment = eAlign_Left;
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
NS_METHOD nsLabel::PreCreateWidget(nsWidgetInitData *aInitData)
{
  if (nsnull != aInitData) {
    nsLabelInitData* data = (nsLabelInitData *) aInitData;
    mAlignment = data->mAlignment;
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// nsLabel destructor
//
//-------------------------------------------------------------------------
nsLabel::~nsLabel()
{
}

//-------------------------------------------------------------------------
//
// Query interface implementation
//
//-------------------------------------------------------------------------
nsresult nsLabel::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  nsresult result = nsWindow::QueryInterface(aIID, aInstancePtr);

  static NS_DEFINE_IID(kILabelIID, NS_ILABEL_IID);
  if (result == NS_NOINTERFACE && aIID.Equals(kILabelIID)) {
      *aInstancePtr = (void*) ((nsILabel*)this);
      NS_ADDREF_THIS();
      result = NS_OK;
  }

  return result;
}

//-------------------------------------------------------------------------
//
// Set this button label
//
//-------------------------------------------------------------------------
NS_METHOD nsLabel::SetAlignment(nsLabelAlignment aAlignment)
{
  mAlignment = aAlignment;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Set this button label
//
//-------------------------------------------------------------------------
NS_METHOD nsLabel::SetLabel(const nsString& aText)
{
	if(mStringView && mStringView->LockLooper())
	{
		mStringView->SetText(
                NS_LossyConvertUCS2toASCII(Substring(aText,0,255)).get());
		mStringView->UnlockLooper();
	}
	return NS_OK;
}

//-------------------------------------------------------------------------
//
// Get this button label
//
//-------------------------------------------------------------------------
NS_METHOD nsLabel::GetLabel(nsString& aBuffer)
{
	if(mStringView && mStringView->LockLooper())
	{
		aBuffer.SetLength(0);
		aBuffer.AppendWithConversion(mStringView->Text());
		mStringView->UnlockLooper();
	}
	return NS_OK;
}

//-------------------------------------------------------------------------
//
// move, paint, resizes message - ignore
//
//-------------------------------------------------------------------------
PRBool nsLabel::OnMove(PRInt32, PRInt32)
{
  return PR_FALSE;
}

PRBool nsLabel::OnPaint(nsRect &r)
{
  //printf("** nsLabel::OnPaint **\n");
  return PR_FALSE;
}

PRBool nsLabel::OnResize(nsRect &aWindowRect)
{
    return PR_FALSE;
}


//-------------------------------------------------------------------------
//
// get position/dimensions
//
//-------------------------------------------------------------------------

NS_METHOD nsLabel::GetBounds(nsRect &aRect)
{
  return nsWindow::GetBounds(aRect);
}

//-------------------------------------------------------------------------
NS_METHOD nsLabel::GetPreferredSize(PRInt32& aWidth, PRInt32& aHeight)
{
  if (nsnull == mContext) {
    return NS_ERROR_FAILURE;
  }
  //nsIFontMetrics * fm = GetFont();;
 // mDeviceContext->GetMetricsFor(mFont, &fm);

  nsIFontMetrics* metrics;
  mContext->GetMetricsFor(*mFont, metrics);

  nsString text;
  GetLabel(text);

  nsIRenderingContext *cx;
  mContext->CreateRenderingContext(this, cx);
  cx->SetFont(metrics);
  nscoord string_height, string_width;
  metrics->GetHeight(string_height);
  cx->GetWidth(text, string_width);
  NS_RELEASE(cx);
  NS_RELEASE(metrics);

  if (mPreferredWidth != 0) {
    aWidth = mPreferredWidth;
  } else {
    aWidth = string_width+8;
  }

  if (mPreferredHeight != 0) {
    aHeight = mPreferredHeight;
  } else {
    aHeight = string_height+8;
  }

  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsLabel::SetPreferredSize(PRInt32 aWidth, PRInt32 aHeight)
{
  mPreferredWidth  = aWidth;
  mPreferredHeight = aHeight;
  return NS_OK;
}

BView *nsLabel::CreateBeOSView()
{
	return mStringView = new nsStringViewBeOS(this, BRect(0, 0, 0, 0), "", "");
}

//-------------------------------------------------------------------------
// Sub-class of BeOS StringView
//-------------------------------------------------------------------------
nsStringViewBeOS::nsStringViewBeOS( nsIWidget *aWidgetWindow, BRect aFrame, 
    const char *aName, const char *text, uint32 aResizingMode, uint32 aFlags )
  : BStringView( aFrame, aName, text, aResizingMode, aFlags ),
    nsIWidgetStore( aWidgetWindow )
{
}
