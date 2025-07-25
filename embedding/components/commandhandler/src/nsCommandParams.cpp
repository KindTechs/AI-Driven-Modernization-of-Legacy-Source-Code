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

#include <new.h>    // for placement new
#include "nscore.h"
#include "nsCRT.h"

#include "nsCommandParams.h"


PLDHashTableOps nsCommandParams::sHashOps =
{
    PL_DHashAllocTable,
    PL_DHashFreeTable,
    HashGetKey,
    HashKey,
    HashMatchEntry,
    HashMoveEntry,
    HashClearEntry,
    PL_DHashFinalizeStub
};


NS_IMPL_ISUPPORTS1(nsCommandParams, nsICommandParams)

nsCommandParams::nsCommandParams()
: mCurEntry(0)
, mNumEntries(eNumEntriesUnknown)
{
  NS_INIT_ISUPPORTS();  
  // init the hash table later
}

nsCommandParams::~nsCommandParams()
{
  PL_DHashTableFinish(&mValuesHash);
}

nsresult
nsCommandParams::Init()
{
  if (!PL_DHashTableInit(&mValuesHash, &sHashOps, (void *)this, sizeof(HashEntry), 4))
    return NS_ERROR_FAILURE;
    
  return NS_OK;
}

#if 0
#pragma mark -
#endif

/* short getValueType (in string name); */
NS_IMETHODIMP nsCommandParams::GetValueType(const nsAString & name, PRInt16 *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = eNoType;
  HashEntry*  foundEntry = GetNamedEntry(name);
  if (foundEntry)
  {
    *_retval = foundEntry->mEntryType;
    return NS_OK;
  }
  
  return NS_ERROR_FAILURE;
}

/* boolean getBooleanValue (in AString name); */
NS_IMETHODIMP nsCommandParams::GetBooleanValue(const nsAString & name, PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = PR_FALSE;

  HashEntry*  foundEntry = GetNamedEntry(name);
  if (foundEntry && foundEntry->mEntryType == eBooleanType)
  {
    *_retval = foundEntry->mData.mBoolean;
    return NS_OK;
  }
  
  return NS_ERROR_FAILURE;
}

/* long getLongValue (in AString name); */
NS_IMETHODIMP nsCommandParams::GetLongValue(const nsAString & name, PRInt32 *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = PR_FALSE;

  HashEntry*  foundEntry = GetNamedEntry(name);
  if (foundEntry && foundEntry->mEntryType == eLongType)
  {
    *_retval = foundEntry->mData.mLong;
    return NS_OK;
  }
  
  return NS_ERROR_FAILURE;
}

/* double getDoubleValue (in AString name); */
NS_IMETHODIMP nsCommandParams::GetDoubleValue(const nsAString & name, double *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = 0.0;

  HashEntry*  foundEntry = GetNamedEntry(name);
  if (foundEntry && foundEntry->mEntryType == eDoubleType)
  {
    *_retval = foundEntry->mData.mDouble;
    return NS_OK;
  }
  
  return NS_ERROR_FAILURE;
}

/* AString getStringValue (in AString name); */
NS_IMETHODIMP nsCommandParams::GetStringValue(const nsAString & name, nsAString & _retval)
{
  _retval.Truncate();
  HashEntry*  foundEntry = GetNamedEntry(name);
  if (foundEntry && foundEntry->mEntryType == eWStringType)
  {
    NS_ASSERTION(foundEntry->mString, "Null string");
    _retval.Assign(*foundEntry->mString);
    return NS_OK;
  }
  
  return NS_ERROR_FAILURE;
}

/* nsISupports getISupportsValue (in AString name); */
NS_IMETHODIMP nsCommandParams::GetISupportsValue(const nsAString & name, nsISupports **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nsnull;

  HashEntry*  foundEntry = GetNamedEntry(name);
  if (foundEntry && foundEntry->mEntryType == eISupportsType)
  {
    NS_IF_ADDREF(*_retval = foundEntry->mISupports.get());
    return NS_OK;
  }
  
  return NS_ERROR_FAILURE;
}

#if 0
#pragma mark -
#endif

/* void setBooleanValue (in AString name, in boolean value); */
NS_IMETHODIMP nsCommandParams::SetBooleanValue(const nsAString & name, PRBool value)
{
  HashEntry*  foundEntry;
  GetOrMakeEntry(name, eBooleanType, foundEntry);
  if (!foundEntry) return NS_ERROR_OUT_OF_MEMORY;
  
  foundEntry->mData.mBoolean = value;
  
  return NS_OK;
}

/* void setLongValue (in AString name, in long value); */
NS_IMETHODIMP nsCommandParams::SetLongValue(const nsAString & name, PRInt32 value)
{
  HashEntry*  foundEntry;
  GetOrMakeEntry(name, eLongType, foundEntry);
  if (!foundEntry) return NS_ERROR_OUT_OF_MEMORY;
  
  foundEntry->mData.mLong = value;
  return NS_OK;
}

/* void setDoubleValue (in AString name, in double value); */
NS_IMETHODIMP nsCommandParams::SetDoubleValue(const nsAString & name, double value)
{
  HashEntry*  foundEntry;
  GetOrMakeEntry(name, eDoubleType, foundEntry);
  if (!foundEntry) return NS_ERROR_OUT_OF_MEMORY;
  
  foundEntry->mData.mDouble = value;
  return NS_OK;
}

/* void setStringValue (in AString name, in AString value); */
NS_IMETHODIMP nsCommandParams::SetStringValue(const nsAString & name, const nsAString & value)
{
  HashEntry*  foundEntry;
  GetOrMakeEntry(name, eWStringType, foundEntry);
  if (!foundEntry) return NS_ERROR_OUT_OF_MEMORY;
  
  foundEntry->mString = new nsString(value);
  return NS_OK;
}

/* void setISupportsValue (in AString name, in nsISupports value); */
NS_IMETHODIMP nsCommandParams::SetISupportsValue(const nsAString & name, nsISupports *value)
{
  HashEntry*  foundEntry;
  GetOrMakeEntry(name, eISupportsType, foundEntry);
  if (!foundEntry) return NS_ERROR_OUT_OF_MEMORY;
  
  foundEntry->mISupports = value;   // addrefs
  return NS_OK;
}

/* void removeValue (in AString name); */
NS_IMETHODIMP
nsCommandParams::RemoveValue(const nsAString & name)
{
  nsPromiseFlatString flatName = PromiseFlatString(name);
  // PL_DHASH_REMOVE doesn't tell us if the entry was really removed, so we return
  // NS_OK unconditionally.
  (void)PL_DHashTableOperate(&mValuesHash, (void *)flatName.get(), PL_DHASH_REMOVE);

  // inval the number of entries
  mNumEntries = eNumEntriesUnknown;
  return NS_OK;
}

#if 0
#pragma mark -
#endif

nsCommandParams::HashEntry*
nsCommandParams::GetNamedEntry(const nsAString& name)
{
  nsPromiseFlatString flatName = PromiseFlatString(name);
  HashEntry *foundEntry = (HashEntry *)PL_DHashTableOperate(&mValuesHash, (void *)flatName.get(), PL_DHASH_LOOKUP);

  if (PL_DHASH_ENTRY_IS_BUSY(foundEntry))
    return foundEntry;
   
  return nsnull;
}


nsCommandParams::HashEntry*
nsCommandParams::GetIndexedEntry(PRInt32 index)
{
  HashEntry*  entry = NS_REINTERPRET_CAST(HashEntry*, mValuesHash.entryStore);
  HashEntry*  limit = entry + PL_DHASH_TABLE_SIZE(&mValuesHash);
  PRUint32    entryCount = 0;
  
  do
  {  
    if (!PL_DHASH_ENTRY_IS_LIVE(entry))
      continue;

    if (entryCount == index)
      return entry;
    
    entryCount ++;
  } while (++entry < limit);

  return nsnull;
}


PRUint32
nsCommandParams::GetNumEntries()
{
  HashEntry*  entry = NS_REINTERPRET_CAST(HashEntry*, mValuesHash.entryStore);
  HashEntry*  limit = entry + PL_DHASH_TABLE_SIZE(&mValuesHash);
  PRUint32    entryCount = 0;
  
  do
  {  
    if (PL_DHASH_ENTRY_IS_LIVE(entry))
      entryCount ++;
  } while (++entry < limit);

  return entryCount;
}

nsresult
nsCommandParams::GetOrMakeEntry(const nsAString& name, PRUint8 entryType, HashEntry*& outEntry)
{
  nsPromiseFlatString flatName = PromiseFlatString(name);

  HashEntry *foundEntry = (HashEntry *)PL_DHashTableOperate(&mValuesHash, (void *)flatName.get(), PL_DHASH_LOOKUP);
  if (PL_DHASH_ENTRY_IS_BUSY(foundEntry))   // reuse existing entry  
  {
    foundEntry->Reset(entryType);
    foundEntry->mEntryName.Assign(name);
    outEntry = foundEntry;
    return NS_OK;
  }

  foundEntry = (HashEntry *)PL_DHashTableOperate(&mValuesHash, (void *)flatName.get(), PL_DHASH_ADD);
  if (!foundEntry) return NS_ERROR_OUT_OF_MEMORY;
  
  // placement new that sucker. Our ctor does not clobber keyHash, which is important.
  outEntry = new (foundEntry) HashEntry(entryType, name);  
  return NS_OK;
}

#if 0
#pragma mark -
#endif

const void *
nsCommandParams::HashGetKey(PLDHashTable *table, PLDHashEntryHdr *entry)
{
  HashEntry*    thisEntry = NS_STATIC_CAST(HashEntry*, entry);
  return (void *)thisEntry->mEntryName.get();
}


PLDHashNumber
nsCommandParams::HashKey(PLDHashTable *table, const void *key)
{
  return nsCRT::HashCode((const PRUnichar*)key);
}

PRBool
nsCommandParams::HashMatchEntry(PLDHashTable *table,
                                const PLDHashEntryHdr *entry, const void *key)
{
  const PRUnichar*   keyString = (const PRUnichar*)key;
  const HashEntry*   thisEntry = NS_STATIC_CAST(const HashEntry*, entry);
  
  return thisEntry->mEntryName.Equals(keyString);
}

void
nsCommandParams::HashMoveEntry(PLDHashTable *table, const PLDHashEntryHdr *from,
                                PLDHashEntryHdr *to)
{
  const HashEntry*   fromEntry  = NS_STATIC_CAST(const HashEntry*, from);
  HashEntry*         toEntry    = NS_STATIC_CAST(HashEntry*, to);
  
  *toEntry = *fromEntry;
  // we leave from dirty, but that's OK
}

void
nsCommandParams::HashClearEntry(PLDHashTable *table, PLDHashEntryHdr *entry)
{
  HashEntry*    thisEntry = NS_STATIC_CAST(HashEntry*, entry);
  thisEntry->~HashEntry();      // call dtor explicitly
  memset(thisEntry, 0, sizeof(HashEntry));    // and clear out
}

#if 0
#pragma mark -
#endif

/* boolean hasMoreElements (); */
NS_IMETHODIMP
nsCommandParams::HasMoreElements(PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  if (mNumEntries == eNumEntriesUnknown)
    mNumEntries = GetNumEntries();
  
  *_retval = mCurEntry < mNumEntries;
  return NS_OK;
}

/* void first (); */
NS_IMETHODIMP
nsCommandParams::First()
{
  mCurEntry = 0;
  return NS_OK;
}

/* AString getNext (); */
NS_IMETHODIMP
nsCommandParams::GetNext(nsAString & _retval)
{
  HashEntry*    thisEntry = GetIndexedEntry(mCurEntry);
  if (!thisEntry)
    return NS_ERROR_FAILURE;
  
  _retval.Assign(thisEntry->mEntryName);
  mCurEntry++;
  return NS_OK;
}
