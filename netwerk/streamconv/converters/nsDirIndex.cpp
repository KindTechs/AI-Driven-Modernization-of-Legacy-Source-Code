/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ----- BEGIN LICENSE BLOCK -----
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
 * The Original Code is the directory index parsing code.
 *
 * The Initial Developer of the Original Code is Bradley Baetz.
 * Portions created by Bradley Baetz are
 * Copyright (C) 2001 Bradley Baetz.
 * All Rights Reserved.
 *
 * Contributor(s): Bradley Baetz <bbaetz@cs.mcgill.ca>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#include "nsDirIndex.h"
#include "nsReadableUtils.h"
#include "nsCRT.h"

NS_IMPL_THREADSAFE_ISUPPORTS1(nsDirIndex,
                              nsIDirIndex);

nsDirIndex::nsDirIndex() : mType(TYPE_UNKNOWN),
                           mSize((PRUint32)(-1)),
                           mLastModified(-1) {
  NS_INIT_REFCNT();
};

nsDirIndex::~nsDirIndex() {};

NS_IMPL_GETSET(nsDirIndex, Type, PRUint32, mType);

// GETSET macros for modern strings would be nice...

NS_IMETHODIMP
nsDirIndex::GetContentType(char* *aContentType) {
  *aContentType = ToNewCString(mContentType);
  if (!*aContentType)
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

NS_IMETHODIMP
nsDirIndex::SetContentType(const char* aContentType) {
  mContentType.Adopt(nsCRT::strdup(aContentType));
  return NS_OK;
}

NS_IMETHODIMP
nsDirIndex::GetLocation(char* *aLocation) {
  *aLocation = ToNewCString(mLocation);
  if (!*aLocation)
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

NS_IMETHODIMP
nsDirIndex::SetLocation(const char* aLocation) {
  mLocation.Adopt(nsCRT::strdup(aLocation));
  return NS_OK;
}

NS_IMETHODIMP
nsDirIndex::GetDescription(PRUnichar* *aDescription) {
  *aDescription = ToNewUnicode(mDescription);
  if (!*aDescription)
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

NS_IMETHODIMP
nsDirIndex::SetDescription(const PRUnichar* aDescription) {
  mDescription.Assign(aDescription);
  return NS_OK;
}

NS_IMPL_GETSET(nsDirIndex, Size, PRUint32, mSize);
NS_IMPL_GETSET(nsDirIndex, LastModified, PRInt64, mLastModified);

