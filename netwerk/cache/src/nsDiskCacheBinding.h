/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is nsDiskCacheBinding.h, released May 10, 2001.
 * 
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *    Gordon Sheridan  <gordon@netscape.com>
 *    Patrick C. Beard <beard@netscape.com>
 */


#ifndef _nsDiskCacheBinding_h_
#define _nsDiskCacheBinding_h_

#include "nspr.h"
#include "pldhash.h"

#include "nsISupports.h"
#include "nsCacheEntry.h"

#ifdef MOZ_NEW_CACHE_REUSE_TRANSPORTS
#include "nsITransport.h"
#endif

#include "nsDiskCacheMap.h"


/******************************************************************************
 *  nsDiskCacheBinding
 *
 *  Created for disk cache specific data and stored in nsCacheEntry.mData as
 *  an nsISupports.  Also stored in nsDiskCacheHashTable, with collisions
 *  linked by the PRCList.
 *
 *****************************************************************************/

class nsDiskCacheBinding : public nsISupports, public PRCList {
public:
    NS_DECL_ISUPPORTS

    nsDiskCacheBinding(nsCacheEntry* entry, nsDiskCacheRecord * record);
    virtual ~nsDiskCacheBinding();

#ifdef MOZ_NEW_CACHE_REUSE_TRANSPORTS
    /**
     * Maps a cache access mode to a cached nsITransport for that access
     * mode. We keep these cached to avoid repeated trips to the
     * file transport service.
     */
    nsCOMPtr<nsITransport>& getTransport(nsCacheAccessMode mode)
    {
        return mTransports[mode - 1];
    }
#endif


// XXX make friends
public:
    nsCacheEntry*           mCacheEntry;    // back pointer to parent nsCacheEntry
    nsDiskCacheRecord       mRecord;
    PRBool                  mDoomed;        // record is not stored in cache map
    PRUint8                 mGeneration;    // possibly just reservation

private:
#ifdef MOZ_NEW_CACHE_REUSE_TRANSPORTS
    nsCOMPtr<nsITransport>  mTransports[3];
#endif
};


/******************************************************************************
 *  Utility Functions
 *****************************************************************************/

nsDiskCacheBinding *   GetCacheEntryBinding(nsCacheEntry * entry);



/******************************************************************************
 *  nsDiskCacheBindery
 *
 *  Used to keep track of nsDiskCacheBinding associated with active/bound (and
 *  possibly doomed) entries.  Lookups on 4 byte disk hash to find collisions
 *  (which need to be doomed, instead of just evicted.  Collisions are linked
 *  using a PRCList to keep track of current generation number.
 *
 *  Used to detect hash number collisions, and find available generation numbers.
 *
 *  Not all nsDiskCacheBinding have a generation number.
 *
 *  Generation numbers may be aquired late, or lost (when data fits in block file)
 *
 *  Collisions can occur:
 *      BindEntry()       - hashnumbers collide (possibly different keys)
 *
 *  Generation number required:
 *      DeactivateEntry() - metadata written to disk, may require file
 *      GetFileForEntry() - force data to require file
 *      writing to stream - data size may require file
 *
 *  Binding can be kept in PRCList in order of generation numbers.
 *  Binding with no generation number can be Appended to PRCList (last).
 *
 *****************************************************************************/

class nsDiskCacheBindery {
public:
    nsDiskCacheBindery();
    ~nsDiskCacheBindery();

    nsresult                Init();
    void                    Reset();

    nsDiskCacheBinding *   CreateBinding(nsCacheEntry *       entry,
                                         nsDiskCacheRecord *  record);

    nsDiskCacheBinding *    FindActiveBinding(PRUint32  hashNumber);
    nsDiskCacheBinding *    FindBinding(nsDiskCacheRecord * record);
    nsresult                AddBinding(nsDiskCacheBinding * binding);
    void                    RemoveBinding(nsDiskCacheBinding * binding);

    
private:
    // member variables
    static PLDHashTableOps ops;
    PLDHashTable           table;
    PRBool                 initialized;
};

#endif /* _nsDiskCacheBinding_h_ */
