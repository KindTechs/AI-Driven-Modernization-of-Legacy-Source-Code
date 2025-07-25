/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#include "nsXPCOM.h"
#include "nsMemory.h"
#include "nsXPCOMPrivate.h"



static nsIMemory* gMemory = nsnull;
static PRBool gHasMemoryShutdown = PR_FALSE;

static NS_METHOD FreeGlobalMemory(void)
{
    NS_IF_RELEASE(gMemory);
    gHasMemoryShutdown = PR_TRUE;
    return NS_OK;
}

#define ENSURE_ALLOCATOR \
  (gMemory ? PR_TRUE : !gHasMemoryShutdown && SetupGlobalMemory())

static nsIMemory*
SetupGlobalMemory()
{
    NS_ASSERTION(!gMemory, "bad call");
    NS_GetMemoryManager(&gMemory);
    NS_ASSERTION(gMemory, "can't get memory manager!");
    NS_RegisterXPCOMExitRoutine(FreeGlobalMemory, 0);
    return gMemory;
}


////////////////////////////////////////////////////////////////////////////////
// nsMemory static helper routines

NS_COM void*
nsMemory::Alloc(PRSize size)
{
    if (!ENSURE_ALLOCATOR)
        return nsnull;
 
   return gMemory->Alloc(size);
}

NS_COM void*
nsMemory::Realloc(void* ptr, PRSize size)
{
    if (!ENSURE_ALLOCATOR)
        return nsnull;    

    return gMemory->Realloc(ptr, size);
}

NS_COM void
nsMemory::Free(void* ptr)
{
    if (!ENSURE_ALLOCATOR)
        return;    

    gMemory->Free(ptr);
}

NS_COM nsresult
nsMemory::HeapMinimize(PRBool aImmediate)
{
    if (!ENSURE_ALLOCATOR)
        return NS_ERROR_FAILURE;    

    return gMemory->HeapMinimize(aImmediate);
}

NS_COM void*
nsMemory::Clone(const void* ptr, PRSize size)
{
    if (!ENSURE_ALLOCATOR)
        return nsnull;    

    void* newPtr = gMemory->Alloc(size);
    if (newPtr)
        memcpy(newPtr, ptr, size);
    return newPtr;
}

NS_COM nsIMemory*
nsMemory::GetGlobalMemoryService()
{
    if (!ENSURE_ALLOCATOR)
        return nsnull;    
   
    nsIMemory* result = gMemory;
    NS_IF_ADDREF(result);
    return result;
}

//----------------------------------------------------------------------

