/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#include "nsWinReg.h"
#include "nsWinProfile.h"
#include "nsWinProfileItem.h"
#include "nspr.h"
#include <windows.h>
#include "nsReadableUtils.h"

/* Public Methods */

MOZ_DECL_CTOR_COUNTER(nsWinProfile)

nsWinProfile::nsWinProfile( nsInstall* suObj, const nsString& folder, const nsString& file )
  : mFilename(folder)
{
    MOZ_COUNT_CTOR(nsWinProfile);

    if(mFilename.Last() != '\\')
    {
        mFilename.Append(NS_LITERAL_STRING("\\"));
    }
    mFilename.Append(file);

    mInstallObject = suObj;
}

nsWinProfile::~nsWinProfile()
{
  MOZ_COUNT_DTOR(nsWinProfile);
}

PRInt32
nsWinProfile::GetString(nsString section, nsString key, nsString* aReturn)
{
  return NativeGetString(section, key, aReturn);
}

PRInt32
nsWinProfile::WriteString(nsString section, nsString key, nsString value, PRInt32* aReturn)
{
  *aReturn = NS_OK;

  nsWinProfileItem* wi = new nsWinProfileItem(this, section, key, value, aReturn);

  if(wi == nsnull)
  {
    *aReturn = nsInstall::OUT_OF_MEMORY;
    return NS_OK;
  }

  if(*aReturn != nsInstall::SUCCESS)
  {
    if(wi)
    {
      delete wi;
      return NS_OK;
    }
  }

  if (mInstallObject)
    mInstallObject->ScheduleForInstall(wi);

  return NS_OK;
}

nsString& nsWinProfile::GetFilename()
{
    return mFilename;
}

nsInstall* nsWinProfile::InstallObject()
{
    return mInstallObject;
}

PRInt32
nsWinProfile::FinalWriteString( nsString section, nsString key, nsString value )
{
    /* do we need another security check here? */
    return NativeWriteString(section, key, value);
}

/* Private Methods */

#define STRBUFLEN 255

PRInt32
nsWinProfile::NativeGetString(nsString aSection, nsString aKey, nsString* aReturn )
{
  int       numChars = 0;
  char      valbuf[STRBUFLEN];

  /* make sure conversions worked */
  if(aSection.First() != '\0' && aKey.First() != '\0' && mFilename.First() != '\0')
  {
    nsXPIDLCString section;
    nsXPIDLCString key;
    nsXPIDLCString filename;

    if (NS_FAILED(UCS2toFS( aSection.get(), getter_Copies(section) )) ||
        NS_FAILED(UCS2toFS( aKey.get(), getter_Copies(key) )) ||
        NS_FAILED(UCS2toFS( mFilename.get(), getter_Copies(filename) )) )
      return 0;

    valbuf[0] = 0;
    numChars = GetPrivateProfileString( section.get(), key.get(), "", valbuf,
                                        STRBUFLEN, filename.get());

    nsXPIDLString value;
    if (NS_SUCCEEDED(FStoUCS2( valbuf, getter_Copies(value) )))
        aReturn->Assign(value);

  }

  return numChars;
}

PRInt32
nsWinProfile::NativeWriteString( nsString aSection, nsString aKey, nsString aValue )
{
  int   success = 0;

  /* make sure conversions worked */
  if(aSection.First() != '\0' && aKey.First() != '\0' && mFilename.First() != '\0')
  {
    nsXPIDLCString section;
    nsXPIDLCString key;
    nsXPIDLCString value;
    nsXPIDLCString filename;

    if (NS_FAILED(UCS2toFS( aSection.get(), getter_Copies(section) )) ||
        NS_FAILED(UCS2toFS( aKey.get(), getter_Copies(key) )) ||
        NS_FAILED(UCS2toFS( aValue.get(), getter_Copies(value) )) ||
        NS_FAILED(UCS2toFS( mFilename.get(), getter_Copies(filename) )) )
      return 0;

    success = WritePrivateProfileString( section.get(), key.get(), value.get(), filename.get() );
  }

  return success;
}

