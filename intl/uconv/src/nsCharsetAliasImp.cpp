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


#include "nsICharsetAlias.h"
#include "pratom.h"

// for NS_IMPL_IDS only
#include "nsIPlatformCharset.h"

#include "nsUConvDll.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsURLProperties.h"
#include "nsITimelineService.h"
#include "nsCharsetAlias.h"

//--------------------------------------------------------------
NS_IMPL_ISUPPORTS1(nsCharsetAlias2, nsICharsetAlias);

//--------------------------------------------------------------
nsCharsetAlias2::nsCharsetAlias2()
{
  NS_INIT_REFCNT();
  mDelegate = nsnull; // delay the load of mDelegate untill we need it.
}
//--------------------------------------------------------------
nsCharsetAlias2::~nsCharsetAlias2()
{
  if(mDelegate)
     delete mDelegate;
}
//--------------------------------------------------------------
NS_IMETHODIMP nsCharsetAlias2::GetPreferred(const nsAString& aAlias, nsAString& oResult)
{
   if (aAlias.IsEmpty()) return NS_ERROR_NULL_POINTER;
   NS_TIMELINE_START_TIMER("nsCharsetAlias2:GetPreferred");

   nsAutoString aKey(aAlias);
   ToLowerCase(aKey);
   oResult.Truncate();

   //delay loading charsetalias.properties by resolving most freq. aliases
   if(!mDelegate) {
     if(aKey.Equals(NS_LITERAL_STRING("utf-8"))) {
       oResult = NS_LITERAL_STRING("UTF-8");
       NS_TIMELINE_STOP_TIMER("nsCharsetAlias2:GetPreferred");
       return NS_OK;
     } 
     if(aKey.Equals(NS_LITERAL_STRING("iso-8859-1"))) {
       oResult = NS_LITERAL_STRING("ISO-8859-1");
       NS_TIMELINE_STOP_TIMER("nsCharsetAlias2:GetPreferred");
       return NS_OK;
     } 
     if(aKey.Equals(NS_LITERAL_STRING("x-sjis")) ||
        aKey.Equals(NS_LITERAL_STRING("shift_jis"))) {
       oResult = NS_LITERAL_STRING("Shift_JIS");
       NS_TIMELINE_STOP_TIMER("nsCharsetAlias2:GetPreferred");
       return NS_OK;
     } 
     //load charsetalias.properties string bundle with all remaining aliases
     // we may need to protect the following section with a lock so we won't call the 
     // 'new nsURLProperties' from two different threads
     mDelegate = new nsURLProperties( NS_LITERAL_STRING("resource:/res/charsetalias.properties") );
     NS_ASSERTION(mDelegate, "cannot create nsURLProperties");
     if(nsnull == mDelegate)
       return NS_ERROR_OUT_OF_MEMORY;
   }

   NS_TIMELINE_STOP_TIMER("nsCharsetAlias2:GetPreferred");
   NS_TIMELINE_MARK_TIMER("nsCharsetAlias2:GetPreferred");

   return mDelegate->Get(aKey, oResult);
}
//--------------------------------------------------------------
NS_IMETHODIMP nsCharsetAlias2::GetPreferred(const PRUnichar* aAlias, const PRUnichar** oResult)
{
   // this method should be obsoleted
   return NS_ERROR_NOT_IMPLEMENTED;
}
//--------------------------------------------------------------
NS_IMETHODIMP nsCharsetAlias2::GetPreferred(const char* aAlias, char* oResult, PRInt32 aBufLength) 
{
   // this method should be obsoleted
   return NS_ERROR_NOT_IMPLEMENTED;
}
//--------------------------------------------------------------
NS_IMETHODIMP nsCharsetAlias2::Equals(const nsAString& aCharset1, const nsAString& aCharset2, PRBool* oResult)
{
   nsresult res = NS_OK;

   if(aCharset1.Equals(aCharset2, nsCaseInsensitiveStringComparator())) {
      *oResult = PR_TRUE;
      return res;
   }

   if(aCharset1.IsEmpty() || aCharset2.IsEmpty()) {
      *oResult = PR_FALSE;
      return res;
   }

   *oResult = PR_FALSE;
   nsString name1;
   nsString name2;
   res = this->GetPreferred(aCharset1, name1);
   if(NS_SUCCEEDED(res)) {
      res = this->GetPreferred(aCharset2, name2);
      if(NS_SUCCEEDED(res)) {
        *oResult = name1.Equals(name2, nsCaseInsensitiveStringComparator());
      }
   }
   
   return res;
}
//--------------------------------------------------------------
NS_IMETHODIMP nsCharsetAlias2::Equals(const PRUnichar* aCharset1, const PRUnichar* aCharset2, PRBool* oResult)
{
   // this method should be obsoleted
   return NS_ERROR_NOT_IMPLEMENTED;
}
//--------------------------------------------------------------
NS_IMETHODIMP nsCharsetAlias2::Equals(const char* aCharset1, const char* aCharset2, PRBool* oResult)
{
   // this method should be obsoleted
   return NS_ERROR_NOT_IMPLEMENTED;
}
 

