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

#ifndef nsUnicodeToUCS2BE_h___
#define nsUnicodeToUCS2BE_h___

#include "nsUCvLatinSupport.h"

//----------------------------------------------------------------------
// Class nsUnicodeToUCS2BE [declaration]

/**
 * A character set converter from Unicode to UCS2BE.
 *
 * @created         05/Apr/1999
 * @author  Catalin Rotaru [CATA]
 */
class nsUnicodeToUCS2BE : public nsTableEncoderSupport
{
public:

  /**
   * Class constructor.
   */
  nsUnicodeToUCS2BE();
  
  NS_IMETHOD FillInfo(PRUint32* aInfo);

protected:

  //--------------------------------------------------------------------
  // Subclassing of nsEncoderSupport class [declaration]

  NS_IMETHOD GetMaxLength(const PRUnichar * aSrc, PRInt32 aSrcLength, 
      PRInt32 * aDestLength);
};

nsresult NEW_UnicodeToUTF16BE(nsISupports **Result);
nsresult NEW_UnicodeToUTF16LE(nsISupports **Result);
nsresult NEW_UnicodeToUTF16(nsISupports **Result);

//============== above code is obsolete ==============================

class nsUnicodeToUTF16BE: public nsBasicEncoder
{
public:
  nsUnicodeToUTF16BE() { mBOM = 0;};

  //--------------------------------------------------------------------
  // Interface nsIUnicodeEncoder [declaration]

  NS_IMETHOD Convert(const PRUnichar * aSrc, PRInt32 * aSrcLength, 
      char * aDest, PRInt32 * aDestLength);
  NS_IMETHOD GetMaxLength(const PRUnichar * aSrc, PRInt32 aSrcLength, 
      PRInt32 * aDestLength);
  NS_IMETHOD Finish(char * aDest, PRInt32 * aDestLength);
  NS_IMETHOD Reset();
  NS_IMETHOD SetOutputErrorBehavior(PRInt32 aBehavior, 
      nsIUnicharEncoder * aEncoder, PRUnichar aChar);

  //--------------------------------------------------------------------
  // Interface nsICharRepresentable [declaration]
  NS_IMETHOD FillInfo(PRUint32 *aInfo);

protected:
  PRUnichar mBOM;
  NS_IMETHOD CopyData(char* aDest, const PRUnichar* aSrc, PRInt32 aLen  );
};

class nsUnicodeToUTF16LE: public nsUnicodeToUTF16BE
{
public:
  nsUnicodeToUTF16LE() { mBOM = 0;};

protected:
  NS_IMETHOD CopyData(char* aDest, const PRUnichar* aSrc, PRInt32 aLen  );
};

class nsUnicodeToUTF16: public nsUnicodeToUTF16BE
{
public:
  nsUnicodeToUTF16() { mBOM = 0xFEFF;};
};

#endif /* nsUnicodeToUCS2BE_h___ */
