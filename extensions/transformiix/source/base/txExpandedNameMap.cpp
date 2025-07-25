/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is TransforMiiX XSLT processor.
 *
 * The Initial Developer of the Original Code is
 * Jonas Sicking.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * Jonas Sicking. All Rights Reserved.
 *
 * Contributor(s):
 *   Jonas Sicking <sicking@bigfoot.com>
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

#include "txExpandedNameMap.h"
#include "string.h"

const int kTxExpandedNameMapAllocSize = 16;

txExpandedNameMap::txExpandedNameMap(MBool aOwnsValues) :
    mItems(0), mItemCount(0), mOwnsValues(aOwnsValues)
{
}

txExpandedNameMap::~txExpandedNameMap()
{
    int i;
    for (i = 0; i < mItemCount; ++i) {
        TX_IF_RELEASE_ATOM(mItems[i].mLocalName);
        if (mOwnsValues)
            delete mItems[i].mValue;
    }
    delete [] mItems;
}

/*
 * Adds an item, if an item with this key already exists an error is
 * returned
 * @param  aKey   key for item to add
 * @param  aValue value of item to add
 * @return errorcode
 */
nsresult txExpandedNameMap::add(txExpandedName& aKey, TxObject* aValue)
{
    int i;
    // Check if there already is an item with this key
    for (i = 0; i < mItemCount; ++i) {
        if (mItems[i].mLocalName == aKey.mLocalName &&
            mItems[i].mNamespaceID == aKey.mNamespaceID)
            return NS_ERROR_FAILURE;
    }
    
    // Allocate a new array if needed
    if (mItemCount % kTxExpandedNameMapAllocSize == 0) {
        MapItem* newItems = new MapItem[mItemCount +
                                        kTxExpandedNameMapAllocSize];
        if (!newItems)
            return NS_ERROR_OUT_OF_MEMORY;

        memcpy(newItems, mItems, mItemCount * sizeof(MapItem));
        delete [] mItems;
        mItems = newItems;
    }
    
    mItems[mItemCount].mNamespaceID = aKey.mNamespaceID;
    mItems[mItemCount].mLocalName = aKey.mLocalName;
    mItems[mItemCount].mValue = aValue;
    ++mItemCount;
    
    return NS_OK;
}

/*
 * Sets an item, if an item with this key already exists it is overwritten
 * with the new value
 * @param  aKey   key for item to set
 * @param  aValue value of item to set
 * @return errorcode
 */
nsresult txExpandedNameMap::set(txExpandedName& aKey, TxObject* aValue)
{
    int i;
    // Check if there already is an item with this key
    for (i = 0; i < mItemCount; ++i) {
        if (mItems[i].mLocalName == aKey.mLocalName &&
            mItems[i].mNamespaceID == aKey.mNamespaceID) {
            if (mOwnsValues)
                delete mItems[i].mValue;
            mItems[i].mValue = aValue;
            return NS_OK;
        }
    }
    
    // Allocate a new array if needed
    if (mItemCount % kTxExpandedNameMapAllocSize == 0) {
        MapItem* newItems = new MapItem[mItemCount +
                                        kTxExpandedNameMapAllocSize];
        if (!newItems)
            return NS_ERROR_OUT_OF_MEMORY;

        memcpy(newItems, mItems, mItemCount * sizeof(MapItem));
        delete [] mItems;
        mItems = newItems;
    }
    
    mItems[mItemCount].mNamespaceID = aKey.mNamespaceID;
    mItems[mItemCount].mLocalName = aKey.mLocalName;
    mItems[mItemCount].mValue = aValue;
    ++mItemCount;
    
    return NS_OK;
}

/*
 * Gets an item
 * @param  aKey  key for item to get
 * @return item with specified key, or null if no such item exists
 */
TxObject* txExpandedNameMap::get(txExpandedName& aKey)
{
    int i;
    for (i = 0; i < mItemCount; ++i) {
        if (mItems[i].mLocalName == aKey.mLocalName &&
            mItems[i].mNamespaceID == aKey.mNamespaceID)
            return mItems[i].mValue;
    }
    return 0;
}
