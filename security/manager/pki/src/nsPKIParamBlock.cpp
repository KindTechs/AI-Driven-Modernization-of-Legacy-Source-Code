/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Javier Delgadillo <javi@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable
 * instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

#include "nsPKIParamBlock.h"
#include "nsIServiceManager.h"
#include "nsIDialogParamBlock.h"

NS_IMPL_THREADSAFE_ISUPPORTS2(nsPKIParamBlock, nsIPKIParamBlock,
                                               nsIDialogParamBlock)

nsPKIParamBlock::nsPKIParamBlock()
{
  NS_INIT_REFCNT();
}

nsresult
nsPKIParamBlock::Init()
{
  mDialogParamBlock = do_CreateInstance("@mozilla.org/embedcomp/dialogparam;1");
  return (mDialogParamBlock == nsnull) ? NS_ERROR_OUT_OF_MEMORY : NS_OK;
}

nsPKIParamBlock::~nsPKIParamBlock()
{
}


NS_IMETHODIMP 
nsPKIParamBlock::SetNumberStrings( PRInt32 inNumStrings )
{
  return mDialogParamBlock->SetNumberStrings(inNumStrings);
}

NS_IMETHODIMP 
nsPKIParamBlock::SetInt(PRInt32 inIndex, PRInt32 inInt)
{
  return mDialogParamBlock->SetInt(inIndex, inInt);
}

NS_IMETHODIMP 
nsPKIParamBlock::GetInt(PRInt32 inIndex, PRInt32 *outInt)
{
  return mDialogParamBlock->GetInt(inIndex, outInt);
}


NS_IMETHODIMP 
nsPKIParamBlock::GetString(PRInt32 inIndex, PRUnichar **_retval)
{
  return mDialogParamBlock->GetString(inIndex, _retval);
}

NS_IMETHODIMP 
nsPKIParamBlock::SetString(PRInt32 inIndex, const PRUnichar *inString)
{
  return mDialogParamBlock->SetString(inIndex, inString);
}

/* void setISupportAtIndex (in PRInt32 index, in nsISupports object); */
NS_IMETHODIMP 
nsPKIParamBlock::SetISupportAtIndex(PRInt32 index, nsISupports *object)
{
  if (!mSupports) {
    mSupports = do_CreateInstance(NS_SUPPORTSARRAY_CONTRACTID);
    if (mSupports == nsnull) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }
  return mSupports->InsertElementAt(object, index-1);
}

/* nsISupports getISupportAtIndex (in PRInt32 index); */
NS_IMETHODIMP 
nsPKIParamBlock::GetISupportAtIndex(PRInt32 index, nsISupports **_retval)
{
  NS_ENSURE_ARG(_retval);

  *_retval = mSupports->ElementAt(index-1);
  return NS_OK;
}


