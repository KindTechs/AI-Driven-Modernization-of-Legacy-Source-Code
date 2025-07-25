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
 *   Dan Rosen <dr@netscape.com>
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

#include "nsSupportsPrimitives.h"
#include "nsCRT.h"
#include "nsMemory.h"
#include "prprf.h"
#include "nsIInterfaceInfoManager.h"

/***************************************************************************/

NS_IMPL_ISUPPORTS2(nsSupportsIDImpl, nsISupportsID, nsISupportsPrimitive)

nsSupportsIDImpl::nsSupportsIDImpl()
    : mData(nsnull)
{
    NS_INIT_ISUPPORTS();
}

nsSupportsIDImpl::~nsSupportsIDImpl()
{
    if(mData)
      nsMemory::Free(mData);
}

NS_IMETHODIMP nsSupportsIDImpl::GetType(PRUint16 *aType)
{
    NS_ASSERTION(aType, "Bad pointer");
    *aType = TYPE_ID;

    return NS_OK;
}

NS_IMETHODIMP nsSupportsIDImpl::GetData(nsID **aData)
{
    NS_ASSERTION(aData, "Bad pointer");
    if(mData)
    {
        *aData = (nsID*) nsMemory::Clone(mData, sizeof(nsID));
        return *aData ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
    }
    *aData = nsnull;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsIDImpl::SetData(const nsID *aData)
{
    if(mData)
      nsMemory::Free(mData);
    if(aData)
        mData = (nsID*) nsMemory::Clone(aData, sizeof(nsID));
    else
        mData = nsnull;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsIDImpl::ToString(char **_retval)
{
    char* result;
    NS_ASSERTION(_retval, "Bad pointer");
    if(mData)
    {
        result = mData->ToString();
    }
    else
    {
        static const char nullStr[] = "null";
        result = (char*) nsMemory::Clone(nullStr, sizeof(nullStr));
    }

    *_retval = result;
    return result ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}  

/*****************************************************************************
 * nsSupportsStringImpl
 *****************************************************************************/

NS_IMPL_ISUPPORTS2(nsSupportsStringImpl, nsISupportsString,
                   nsISupportsPrimitive)

nsSupportsStringImpl::nsSupportsStringImpl()
    : mData(0), mLength(0)
{
    NS_INIT_ISUPPORTS();
}

nsSupportsStringImpl::~nsSupportsStringImpl()
{
    if (mData)
        nsMemory::Free(mData);
}

NS_IMETHODIMP nsSupportsStringImpl::GetType(PRUint16 *aType)
{
    NS_ASSERTION(aType, "Bad pointer");

    *aType = TYPE_STRING;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsStringImpl::GetData(char **aData)
{
    nsresult rv = NS_OK;
    // copy the buffer
    if (mData) {
        size_t size((mLength + 1) * sizeof(char));
        *aData = NS_STATIC_CAST(char*, nsMemory::Clone(mData, size));
        if (!(*aData))
            rv = NS_ERROR_OUT_OF_MEMORY;
    }
    else
        *aData = nsnull;

    return rv;
}

NS_IMETHODIMP nsSupportsStringImpl::SetData(const char *aData)
{
    return SetDataWithLength(aData ? strlen(aData) : 0, aData);
}

NS_IMETHODIMP nsSupportsStringImpl::ToString(char **_retval)
{
    return GetData(_retval);
}

NS_IMETHODIMP nsSupportsStringImpl::SetDataWithLength(PRUint32 aLength,
                                                      const char *aData)
{
    // if the new string length is the same as the old,
    // just copy into the old buffer to avoid reallocating. we
    // can further avoid reallocating by reusing the same
    // buffer if aLength <= mLength, but there's high potential
    // for bloat there. see bug 80097 for discussion.
    if ((aLength == mLength) && aData && mData) {
        memcpy(mData, aData, aLength * sizeof(char));
        return NS_OK;
    }

    // otherwise, we'll have to allocate a new buffer, copy
    // into that new buffer, and adopt it
    char* newData = nsnull;
    if (aData) {
        // allocate a new buffer
        size_t size((aLength + 1) * sizeof(char));
        newData = NS_STATIC_CAST(char*, nsMemory::Alloc(size));
        if (!newData)
            return NS_ERROR_OUT_OF_MEMORY;
        // copy into that buffer if it was successfully allocated
        memcpy(newData, aData, aLength * sizeof(char));
    }

    // if we've succeeded so far, adopt the new buffer. the adopt
    // function handles the null buffer case
    return AdoptDataWithLength(aLength, newData);
}

NS_IMETHODIMP nsSupportsStringImpl::AdoptData(char *aData)
{
    return AdoptDataWithLength(aData ? strlen(aData) : 0, aData);
}

NS_IMETHODIMP nsSupportsStringImpl::AdoptDataWithLength(PRUint32 aLength,
                                                        char *aData)
{
    // free current buffer
    if (mData)
        nsMemory::Free(mData);

    // subsume new buffer, null or not
    mData = aData;

    if (mData) {
        // set length of new buffer
        mLength = aLength;
        // always make sure we're null-terminated
        mData[mLength] = '\0';
    }
    else
        mLength = 0;

    return NS_OK;
}

/*****************************************************************************
 * nsSupportsWStringImpl
 *****************************************************************************/

NS_IMPL_ISUPPORTS2(nsSupportsWStringImpl, nsISupportsWString,
                   nsISupportsPrimitive)

nsSupportsWStringImpl::nsSupportsWStringImpl()
    : mData(0), mLength(0)
{
    NS_INIT_ISUPPORTS();
}

nsSupportsWStringImpl::~nsSupportsWStringImpl()
{
    if (mData)
        nsMemory::Free(mData);
}

NS_IMETHODIMP nsSupportsWStringImpl::GetType(PRUint16 *aType)
{
    NS_ASSERTION(aType, "Bad pointer");

    *aType = TYPE_WSTRING;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsWStringImpl::GetData(PRUnichar **aData)
{
    nsresult rv = NS_OK;
    // copy the buffer
    if (mData) {
        size_t size((mLength + 1) * sizeof(PRUnichar));
        *aData = NS_STATIC_CAST(PRUnichar*, nsMemory::Clone(mData, size));
        if (!(*aData))
            rv = NS_ERROR_OUT_OF_MEMORY;
    }
    else
        *aData = nsnull;

    return rv;
}

NS_IMETHODIMP nsSupportsWStringImpl::SetData(const PRUnichar *aData)
{
    return SetDataWithLength(aData ? nsCRT::strlen(aData) : 0, aData);
}

NS_IMETHODIMP nsSupportsWStringImpl::ToString(PRUnichar **_retval)
{
    return GetData(_retval);
}

NS_IMETHODIMP nsSupportsWStringImpl::SetDataWithLength(PRUint32 aLength,
                                                       const PRUnichar *aData)
{
    // if the new string length is the same as the old,
    // just copy into the old buffer to avoid reallocating. we
    // can further avoid reallocating by reusing the same
    // buffer if aLength <= mLength, but there's high potential
    // for bloat there. see bug 80097 for discussion.
    if ((aLength == mLength) && aData && mData) {
        memcpy(mData, aData, aLength * sizeof(PRUnichar));
        return NS_OK;
    }

    // otherwise, we'll have to allocate a new buffer, copy
    // into that new buffer, and adopt it
    PRUnichar* newData = nsnull;
    if (aData) {
        // allocate a new buffer
        size_t size((aLength + 1) * sizeof(PRUnichar));
        newData = NS_STATIC_CAST(PRUnichar*, nsMemory::Alloc(size));
        if (!newData)
            return NS_ERROR_OUT_OF_MEMORY;
        // copy into that buffer if it was successfully allocated
        memcpy(newData, aData, aLength * sizeof(PRUnichar));
    }

    // if we've succeeded so far, adopt the new buffer. the adopt
    // function handles the null buffer case
    return AdoptDataWithLength(aLength, newData);
}

NS_IMETHODIMP nsSupportsWStringImpl::AdoptData(PRUnichar *aData)
{
    return AdoptDataWithLength(aData ? nsCRT::strlen(aData) : 0, aData);
}

NS_IMETHODIMP nsSupportsWStringImpl::AdoptDataWithLength(PRUint32 aLength,
                                                         PRUnichar *aData)
{
    // free current buffer
    if (mData)
        nsMemory::Free(mData);

    // subsume new buffer, null or not
    mData = aData;

    if (mData) {
        // set length of new buffer
        mLength = aLength;
        // always make sure we're null-terminated
        mData[mLength] = NS_STATIC_CAST(PRUnichar, 0);
    }
    else
        mLength = 0;

    return NS_OK;
}

/***************************************************************************/

NS_IMPL_THREADSAFE_ISUPPORTS2(nsSupportsPRBoolImpl, nsISupportsPRBool,
                              nsISupportsPrimitive)

nsSupportsPRBoolImpl::nsSupportsPRBoolImpl()
    : mData(PR_FALSE)
{
    NS_INIT_ISUPPORTS();
}

nsSupportsPRBoolImpl::~nsSupportsPRBoolImpl() {}

NS_IMETHODIMP nsSupportsPRBoolImpl::GetType(PRUint16 *aType)
{
    NS_ASSERTION(aType, "Bad pointer");
    *aType = TYPE_PRBOOL;

    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRBoolImpl::GetData(PRBool *aData)
{
    NS_ASSERTION(aData, "Bad pointer");
    *aData = mData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRBoolImpl::SetData(PRBool aData)
{
    mData = aData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRBoolImpl::ToString(char **_retval)
{
    NS_ASSERTION(_retval, "Bad pointer");
    const char * str = mData ? "true" : "false";
    char* result = (char*) nsMemory::Clone(str,
                                (strlen(str)+1)*sizeof(char));
    *_retval = result;
    return result ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}  

NS_COM nsresult
NS_NewISupportsPRBool (nsISupportsPRBool ** aResult)
{
    NS_ENSURE_ARG_POINTER (aResult);
    nsISupportsPRBool * rval = (nsISupportsPRBool *) (new nsSupportsPRBoolImpl ());
    
    if (!rval)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF (rval);
    *aResult = rval;

    return NS_OK;
}

/***************************************************************************/

NS_IMPL_ISUPPORTS2(nsSupportsPRUint8Impl, nsISupportsPRUint8,
                   nsISupportsPrimitive)

nsSupportsPRUint8Impl::nsSupportsPRUint8Impl()
    : mData(0)
{
    NS_INIT_ISUPPORTS();
}

nsSupportsPRUint8Impl::~nsSupportsPRUint8Impl() {}

NS_IMETHODIMP nsSupportsPRUint8Impl::GetType(PRUint16 *aType)
{
    NS_ASSERTION(aType, "Bad pointer");
    *aType = TYPE_PRUINT8;

    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRUint8Impl::GetData(PRUint8 *aData)
{
    NS_ASSERTION(aData, "Bad pointer");
    *aData = mData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRUint8Impl::SetData(PRUint8 aData)
{
    mData = aData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRUint8Impl::ToString(char **_retval)
{
    NS_ASSERTION(_retval, "Bad pointer");
    static const int size = 8;
    char buf[size];

    PR_snprintf(buf, size, "%u", (PRUint16) mData);

    char* result = (char*) nsMemory::Clone(buf,
                                (strlen(buf)+1)*sizeof(char));
    *_retval = result;
    return result ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}  

/***************************************************************************/

NS_IMPL_ISUPPORTS2(nsSupportsPRUint16Impl, nsISupportsPRUint16,
                   nsISupportsPrimitive)

nsSupportsPRUint16Impl::nsSupportsPRUint16Impl()
    : mData(0)
{
    NS_INIT_ISUPPORTS();
}

nsSupportsPRUint16Impl::~nsSupportsPRUint16Impl() {}

NS_IMETHODIMP nsSupportsPRUint16Impl::GetType(PRUint16 *aType)
{
    NS_ASSERTION(aType, "Bad pointer");
    *aType = TYPE_PRUINT16;

    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRUint16Impl::GetData(PRUint16 *aData)
{
    NS_ASSERTION(aData, "Bad pointer");
    *aData = mData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRUint16Impl::SetData(PRUint16 aData)
{
    mData = aData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRUint16Impl::ToString(char **_retval)
{
    NS_ASSERTION(_retval, "Bad pointer");
    static const int size = 8;
    char buf[size];

    PR_snprintf(buf, size, "%u", (int) mData);

    char* result = (char*) nsMemory::Clone(buf,
                                (strlen(buf)+1)*sizeof(char));
    *_retval = result;
    return result ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}  

/***************************************************************************/

NS_IMPL_ISUPPORTS2(nsSupportsPRUint32Impl, nsISupportsPRUint32,
                   nsISupportsPrimitive)

nsSupportsPRUint32Impl::nsSupportsPRUint32Impl()
    : mData(0)
{
    NS_INIT_ISUPPORTS();
}

nsSupportsPRUint32Impl::~nsSupportsPRUint32Impl() {}

NS_IMETHODIMP nsSupportsPRUint32Impl::GetType(PRUint16 *aType)
{
    NS_ASSERTION(aType, "Bad pointer");
    *aType = TYPE_PRUINT32;

    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRUint32Impl::GetData(PRUint32 *aData)
{
    NS_ASSERTION(aData, "Bad pointer");
    *aData = mData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRUint32Impl::SetData(PRUint32 aData)
{
    mData = aData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRUint32Impl::ToString(char **_retval)
{
    NS_ASSERTION(_retval, "Bad pointer");
    static const int size = 16;
    char buf[size];

    PR_snprintf(buf, size, "%lu", mData);

    char* result = (char*) nsMemory::Clone(buf,
                                (strlen(buf)+1)*sizeof(char));
    *_retval = result;
    return result ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}  

/***************************************************************************/

NS_IMPL_ISUPPORTS2(nsSupportsPRUint64Impl, nsISupportsPRUint64,
                   nsISupportsPrimitive)

nsSupportsPRUint64Impl::nsSupportsPRUint64Impl()
    : mData(LL_ZERO)
{
    NS_INIT_ISUPPORTS();
}

nsSupportsPRUint64Impl::~nsSupportsPRUint64Impl() {}

NS_IMETHODIMP nsSupportsPRUint64Impl::GetType(PRUint16 *aType)
{
    NS_ASSERTION(aType, "Bad pointer");
    *aType = TYPE_PRUINT64;

    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRUint64Impl::GetData(PRUint64 *aData)
{
    NS_ASSERTION(aData, "Bad pointer");
    *aData = mData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRUint64Impl::SetData(PRUint64 aData)
{
    mData = aData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRUint64Impl::ToString(char **_retval)
{
    NS_ASSERTION(_retval, "Bad pointer");
    static const int size = 32;
    char buf[size];

    PR_snprintf(buf, size, "%llu", mData);

    char* result = (char*) nsMemory::Clone(buf,
                                (strlen(buf)+1)*sizeof(char));
    *_retval = result;
    return result ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}  

/***************************************************************************/

NS_IMPL_ISUPPORTS2(nsSupportsPRTimeImpl, nsISupportsPRTime,
                   nsISupportsPrimitive)

nsSupportsPRTimeImpl::nsSupportsPRTimeImpl()
    : mData(LL_ZERO)
{
    NS_INIT_ISUPPORTS();
}

nsSupportsPRTimeImpl::~nsSupportsPRTimeImpl() {}

NS_IMETHODIMP nsSupportsPRTimeImpl::GetType(PRUint16 *aType)
{
    NS_ASSERTION(aType, "Bad pointer");
    *aType = TYPE_PRTIME;

    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRTimeImpl::GetData(PRTime *aData)
{
    NS_ASSERTION(aData, "Bad pointer");
    *aData = mData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRTimeImpl::SetData(PRTime aData)
{
    mData = aData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRTimeImpl::ToString(char **_retval)
{
    NS_ASSERTION(_retval, "Bad pointer");
    static const int size = 32;
    char buf[size];

    PR_snprintf(buf, size, "%llu", mData);

    char* result = (char*) nsMemory::Clone(buf,
                                (strlen(buf)+1)*sizeof(char));
    *_retval = result;
    return result ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}  

/***************************************************************************/

NS_IMPL_ISUPPORTS2(nsSupportsCharImpl, nsISupportsChar,
                   nsISupportsPrimitive)

nsSupportsCharImpl::nsSupportsCharImpl()
    : mData(0)
{
    NS_INIT_ISUPPORTS();
}

nsSupportsCharImpl::~nsSupportsCharImpl() {}

NS_IMETHODIMP nsSupportsCharImpl::GetType(PRUint16 *aType)
{
    NS_ASSERTION(aType, "Bad pointer");
    *aType = TYPE_CHAR;

    return NS_OK;
}

NS_IMETHODIMP nsSupportsCharImpl::GetData(char *aData)
{
    NS_ASSERTION(aData, "Bad pointer");
    *aData = mData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsCharImpl::SetData(char aData)
{
    mData = aData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsCharImpl::ToString(char **_retval)
{
    char* result;
    NS_ASSERTION(_retval, "Bad pointer");

    if(nsnull != (result = (char*) nsMemory::Alloc(2*sizeof(char))))
    {
        result[0] = mData;
        result[1] = '\0';
    }
    *_retval = result;
    return result ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}  

/***************************************************************************/

NS_IMPL_ISUPPORTS2(nsSupportsPRInt16Impl, nsISupportsPRInt16,
                   nsISupportsPrimitive)

nsSupportsPRInt16Impl::nsSupportsPRInt16Impl()
    : mData(0)
{
    NS_INIT_ISUPPORTS();
}

nsSupportsPRInt16Impl::~nsSupportsPRInt16Impl() {}

NS_IMETHODIMP nsSupportsPRInt16Impl::GetType(PRUint16 *aType)
{
    NS_ASSERTION(aType, "Bad pointer");
    *aType = TYPE_PRINT16;

    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRInt16Impl::GetData(PRInt16 *aData)
{
    NS_ASSERTION(aData, "Bad pointer");
    *aData = mData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRInt16Impl::SetData(PRInt16 aData)
{
    mData = aData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRInt16Impl::ToString(char **_retval)
{
    NS_ASSERTION(_retval, "Bad pointer");
    static const int size = 8;
    char buf[size];

    PR_snprintf(buf, size, "%d", mData);

    char* result = (char*) nsMemory::Clone(buf,
                                (strlen(buf)+1)*sizeof(char));
    *_retval = result;
    return result ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}  

/***************************************************************************/

NS_IMPL_ISUPPORTS2(nsSupportsPRInt32Impl, nsISupportsPRInt32,
                   nsISupportsPrimitive)

nsSupportsPRInt32Impl::nsSupportsPRInt32Impl()
    : mData(0)
{
    NS_INIT_ISUPPORTS();
}

nsSupportsPRInt32Impl::~nsSupportsPRInt32Impl() {}

NS_IMETHODIMP nsSupportsPRInt32Impl::GetType(PRUint16 *aType)
{
    NS_ASSERTION(aType, "Bad pointer");
    *aType = TYPE_PRINT32;

    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRInt32Impl::GetData(PRInt32 *aData)
{
    NS_ASSERTION(aData, "Bad pointer");
    *aData = mData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRInt32Impl::SetData(PRInt32 aData)
{
    mData = aData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRInt32Impl::ToString(char **_retval)
{
    NS_ASSERTION(_retval, "Bad pointer");
    static const int size = 16;
    char buf[size];

    PR_snprintf(buf, size, "%ld", mData);

    char* result = (char*) nsMemory::Clone(buf,
                                (strlen(buf)+1)*sizeof(char));
    *_retval = result;
    return result ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}  

/***************************************************************************/

NS_IMPL_ISUPPORTS2(nsSupportsPRInt64Impl, nsISupportsPRInt64,
                   nsISupportsPrimitive)

nsSupportsPRInt64Impl::nsSupportsPRInt64Impl()
    : mData(LL_ZERO)
{
    NS_INIT_ISUPPORTS();
}

nsSupportsPRInt64Impl::~nsSupportsPRInt64Impl() {}

NS_IMETHODIMP nsSupportsPRInt64Impl::GetType(PRUint16 *aType)
{
    NS_ASSERTION(aType, "Bad pointer");
    *aType = TYPE_PRINT64;

    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRInt64Impl::GetData(PRInt64 *aData)
{
    NS_ASSERTION(aData, "Bad pointer");
    *aData = mData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRInt64Impl::SetData(PRInt64 aData)
{
    mData = aData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsPRInt64Impl::ToString(char **_retval)
{
    NS_ASSERTION(_retval, "Bad pointer");
    static const int size = 32;
    char buf[size];

    PR_snprintf(buf, size, "%lld", mData);

    char* result = (char*) nsMemory::Clone(buf,
                                (strlen(buf)+1)*sizeof(char));
    *_retval = result;
    return result ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}  

/***************************************************************************/

NS_IMPL_ISUPPORTS2(nsSupportsFloatImpl, nsISupportsFloat,
                   nsISupportsPrimitive)

nsSupportsFloatImpl::nsSupportsFloatImpl()
    : mData(float(0.0))
{
    NS_INIT_ISUPPORTS();
}

nsSupportsFloatImpl::~nsSupportsFloatImpl() {}

NS_IMETHODIMP nsSupportsFloatImpl::GetType(PRUint16 *aType)
{
    NS_ASSERTION(aType, "Bad pointer");
    *aType = TYPE_FLOAT;

    return NS_OK;
}

NS_IMETHODIMP nsSupportsFloatImpl::GetData(float *aData)
{
    NS_ASSERTION(aData, "Bad pointer");
    *aData = mData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsFloatImpl::SetData(float aData)
{
    mData = aData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsFloatImpl::ToString(char **_retval)
{
    NS_ASSERTION(_retval, "Bad pointer");
    static const int size = 32;
    char buf[size];

    PR_snprintf(buf, size, "%f", (double) mData);

    char* result = (char*) nsMemory::Clone(buf,
                                (strlen(buf)+1)*sizeof(char));
    *_retval = result;
    return result ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}  

/***************************************************************************/

NS_IMPL_ISUPPORTS2(nsSupportsDoubleImpl, nsISupportsDouble,
                   nsISupportsPrimitive)

nsSupportsDoubleImpl::nsSupportsDoubleImpl()
    : mData(double(0.0))
{
    NS_INIT_ISUPPORTS();
}

nsSupportsDoubleImpl::~nsSupportsDoubleImpl() {}

NS_IMETHODIMP nsSupportsDoubleImpl::GetType(PRUint16 *aType)
{
    NS_ASSERTION(aType, "Bad pointer");
    *aType = TYPE_DOUBLE;

    return NS_OK;
}

NS_IMETHODIMP nsSupportsDoubleImpl::GetData(double *aData)
{
    NS_ASSERTION(aData, "Bad pointer");
    *aData = mData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsDoubleImpl::SetData(double aData)
{
    mData = aData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsDoubleImpl::ToString(char **_retval)
{
    NS_ASSERTION(_retval, "Bad pointer");
    static const int size = 32;
    char buf[size];

    PR_snprintf(buf, size, "%f", mData);

    char* result = (char*) nsMemory::Clone(buf,
                                (strlen(buf)+1)*sizeof(char));
    *_retval = result;
    return  result ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}  

/***************************************************************************/


NS_IMPL_THREADSAFE_ISUPPORTS2(nsSupportsVoidImpl, nsISupportsVoid,
                              nsISupportsPrimitive)

nsSupportsVoidImpl::nsSupportsVoidImpl()
    : mData(nsnull)
{
    NS_INIT_ISUPPORTS();
}

nsSupportsVoidImpl::~nsSupportsVoidImpl() {}

NS_IMETHODIMP nsSupportsVoidImpl::GetType(PRUint16 *aType)
{
    NS_ASSERTION(aType, "Bad pointer");
    *aType = TYPE_VOID;

    return NS_OK;
}

NS_IMETHODIMP nsSupportsVoidImpl::GetData(void * *aData)
{
    NS_ASSERTION(aData, "Bad pointer");
    *aData = mData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsVoidImpl::SetData(void * aData)
{
    mData = aData;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsVoidImpl::ToString(char **_retval)
{
    NS_ASSERTION(_retval, "Bad pointer");

    static const char str[] = "[raw data]";
    char* result = (char*) nsMemory::Clone(str, sizeof(str));
    *_retval = result;
    return  result ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}  

/***************************************************************************/


NS_IMPL_THREADSAFE_ISUPPORTS2(nsSupportsInterfacePointerImpl,
                              nsISupportsInterfacePointer,
                              nsISupportsPrimitive)

nsSupportsInterfacePointerImpl::nsSupportsInterfacePointerImpl()
    : mIID(nsnull)
{
    NS_INIT_ISUPPORTS();
}

nsSupportsInterfacePointerImpl::~nsSupportsInterfacePointerImpl()
{
    if (mIID) {
        nsMemory::Free(mIID);
    }
}

NS_IMETHODIMP nsSupportsInterfacePointerImpl::GetType(PRUint16 *aType)
{
    NS_ASSERTION(aType, "Bad pointer");
    *aType = TYPE_INTERFACE_POINTER;

    return NS_OK;
}

NS_IMETHODIMP nsSupportsInterfacePointerImpl::GetData(nsISupports **aData)
{
    NS_ASSERTION(aData,"Bad pointer");

    *aData = mData;
    NS_IF_ADDREF(*aData);

    return NS_OK;
}

NS_IMETHODIMP nsSupportsInterfacePointerImpl::SetData(nsISupports * aData)
{
    mData = aData;

    return NS_OK;
}

NS_IMETHODIMP nsSupportsInterfacePointerImpl::GetDataIID(nsID **aIID)
{
    NS_ASSERTION(aIID,"Bad pointer");

    if(mIID)
    {
        *aIID = (nsID*) nsMemory::Clone(mIID, sizeof(nsID));
        return *aIID ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
    }
    *aIID = nsnull;
    return NS_OK;
}

NS_IMETHODIMP nsSupportsInterfacePointerImpl::SetDataIID(const nsID *aIID)
{
    if(mIID)
        nsMemory::Free(mIID);
    if(aIID)
        mIID = (nsID*) nsMemory::Clone(aIID, sizeof(nsID));
    else
        mIID = nsnull;

    return NS_OK;
}

NS_IMETHODIMP nsSupportsInterfacePointerImpl::ToString(char **_retval)
{
    NS_ASSERTION(_retval, "Bad pointer");

    static const char str[] = "[interface pointer]";

    // jband sez: think about asking nsIInterfaceInfoManager whether
    // the interface has a known human-readable name
    char* result = (char*) nsMemory::Clone(str, sizeof(str));
    *_retval = result;
    return  result ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

/***************************************************************************/
