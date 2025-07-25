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

#ifndef nsROCSSPrimitiveValue_h___
#define nsROCSSPrimitiveValue_h___

#include "nsIDOMCSSPrimitiveValue.h"
#include "nsString.h"
#include "nsCoord.h"
#include "nsUnitConversion.h"
#include "nsReadableUtils.h"

#include "nsCOMPtr.h"
#include "nsDOMError.h"
#include "nsDOMCSSRect.h"
#include "nsDOMCSSRGBColor.h"

class nsROCSSPrimitiveValue : public nsIDOMCSSPrimitiveValue
{
public:
  NS_DECL_ISUPPORTS

  // nsIDOMCSSPrimitiveValue
  NS_DECL_NSIDOMCSSPRIMITIVEVALUE

  // nsIDOMCSSValue
  NS_DECL_NSIDOMCSSVALUE

  // nsROCSSPrimitiveValue
  nsROCSSPrimitiveValue(nsISupports *aOwner, float aP2T);
  virtual ~nsROCSSPrimitiveValue();

  void SetNumber(float aValue)
  {
    Reset();
    mValue.mFloat = aValue;
    mType = CSS_NUMBER;
  }
  
  void SetPercent(float aValue)
  {
    Reset();
    mValue.mFloat = aValue;
    mType = CSS_PERCENTAGE;
  }

  void SetTwips(nscoord aValue)
  {
    Reset();
    mValue.mTwips = aValue;
    mType = CSS_PX;
  }

  void SetIdent(const nsACString& aString)
  {
    Reset();
    mValue.mString = ToNewUnicode(aString);
    mType = (mValue.mString != nsnull) ? CSS_IDENT : CSS_UNKNOWN;
  }

  void SetIdent(const nsAString& aString)
  {
    Reset();
    mValue.mString = ToNewUnicode(aString);
    mType = (mValue.mString != nsnull) ? CSS_IDENT : CSS_UNKNOWN;
  }

  void SetString(const nsACString& aString)
  {
    Reset();
    mValue.mString = ToNewUnicode(aString);
    mType = (mValue.mString != nsnull) ? CSS_STRING : CSS_UNKNOWN;
  }

  void SetString(const nsAString& aString)
  {
    Reset();
    mValue.mString = ToNewUnicode(aString);
    mType = (mValue.mString != nsnull) ? CSS_STRING : CSS_UNKNOWN;
  }

  void SetURI(const nsACString& aString)
  {
    Reset();
    mValue.mString = ToNewUnicode(aString);
    mType = (mValue.mString != nsnull) ? CSS_URI : CSS_UNKNOWN;
  }

  void SetURI(const nsAString& aString)
  {
    Reset();
    mValue.mString = ToNewUnicode(aString);
    mType = (mValue.mString != nsnull) ? CSS_URI : CSS_UNKNOWN;
  }

  void SetColor(nsIDOMRGBColor* aColor)
  {
    NS_PRECONDITION(aColor, "Null RGBColor being set!");
    Reset();
    mValue.mColor = aColor;
    if (mValue.mColor) {
      NS_ADDREF(mValue.mColor);
      mType = CSS_RGBCOLOR;
    }
    else {
      mType = CSS_UNKNOWN;
    }
  }

  void SetRect(nsIDOMRect* aRect)
  {
    NS_PRECONDITION(aRect, "Null rect being set!");
    Reset();
    mValue.mRect = aRect;
    if (mValue.mRect) {
      NS_ADDREF(mValue.mRect);
      mType = CSS_RECT;
    }
    else {
      mType = CSS_UNKNOWN;
    }
  }

  void Reset(void)
  {
    switch (mType) {
      case CSS_IDENT:
      case CSS_STRING:
      case CSS_URI:
        NS_ASSERTION(mValue.mString, "Null string should never happen");
        nsMemory::Free(mValue.mString);
        mValue.mString = nsnull;
        break;
      case CSS_RECT:
        NS_ASSERTION(mValue.mRect, "Null Rect should never happen");
        NS_RELEASE(mValue.mRect);
        mValue.mRect = nsnull;
        break;
      case CSS_RGBCOLOR:
        NS_ASSERTION(mValue.mColor, "Null RGBColor should never happen");
        NS_RELEASE(mValue.mColor);
        mValue.mColor = nsnull;
        break;
    }
  }

private:
  void GetEscapedURI(PRUnichar *aURI, PRUnichar **aReturn);

  PRUint16 mType;

  union {
    nscoord         mTwips;
    float           mFloat;
    nsIDOMRGBColor* mColor;
    nsIDOMRect*     mRect;
    PRUnichar*      mString;
  } mValue;
  
  nsISupports *mOwner;

  float mT2P;
};

#endif /* nsROCSSPrimitiveValue_h___ */

