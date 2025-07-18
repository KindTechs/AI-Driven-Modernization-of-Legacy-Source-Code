/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim:cindent:ts=8:et:sw=4:
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
 * The Original Code is Mozilla.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications.  Portions created by Netscape Communications are
 * Copyright (C) 2001 by Netscape Communications.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   L. David Baron <dbaron@fas.harvard.edu> (original author)
 */

/*
 * This test is NOT intended to be run.  It's a test to make sure
 * a group of functions BUILD correctly.
 */

#include "nsISupportsUtils.h"
#include "nsIWeakReference.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsWeakReference.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"

#define NS_ITESTSERVICE_IID \
  {0x127b5253, 0x37b1, 0x43c7, \
    { 0x96, 0x2b, 0xab, 0xf1, 0x2d, 0x22, 0x56, 0xae }}

class NS_NO_VTABLE nsITestService : public nsISupports {
  public: 
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_ITESTSERVICE_IID)
};

class nsTestService : public nsITestService, public nsSupportsWeakReference
{
  public:
    NS_DECL_ISUPPORTS
};

NS_IMPL_ISUPPORTS2(nsTestService, nsITestService, nsISupportsWeakReference)

#define NS_TEST_SERVICE_CONTRACTID "@mozilla.org/test/testservice;1"
#define NS_TEST_SERVICE_CID \
  {0xa00c1406, 0x283a, 0x45c9, \
    {0xae, 0xd2, 0x1a, 0xb6, 0xdd, 0xba, 0xfe, 0x53}}
static NS_DEFINE_CID(kTestServiceCID, NS_TEST_SERVICE_CID);

int main()
{
    /*
     * NOTE:  This does NOT demonstrate how these functions are
     * intended to be used.  They are intended for filling in out
     * parameters that need to be |AddRef|ed.  I'm just too lazy
     * to write lots of little getter functions for a test program
     * when I don't need to.
     */

    /* Test CallQueryInterface */

    nsISupports *mySupportsPtr = NS_REINTERPRET_CAST(nsISupports*, 0x1000);

    nsITestService *myITestService = nsnull;
    CallQueryInterface(mySupportsPtr, &myITestService);

    nsTestService *myTestService =
        NS_REINTERPRET_CAST(nsTestService*, mySupportsPtr);
    nsISupportsWeakReference *mySupportsWeakRef;
    CallQueryInterface(myTestService, &mySupportsWeakRef);

    /* Test CallQueryReferent */

    nsIWeakReference *myWeakRef =
        NS_STATIC_CAST(nsIWeakReference*, mySupportsPtr);
    CallQueryReferent(myWeakRef, &myITestService);

    /* Test CallCreateInstance */

    CallCreateInstance(kTestServiceCID, mySupportsPtr, &myITestService);
    CallCreateInstance(kTestServiceCID, &myITestService);
    CallCreateInstance(NS_TEST_SERVICE_CONTRACTID, mySupportsPtr,
                       &myITestService);
    CallCreateInstance(NS_TEST_SERVICE_CONTRACTID, &myITestService);

    /* Test CallGetService */
    nsIShutdownListener *myShutdownListener = nsnull;
    CallGetService(kTestServiceCID, &myITestService);
    CallGetService(kTestServiceCID, myShutdownListener, &myITestService);
    CallGetService(NS_TEST_SERVICE_CONTRACTID, &myITestService);
    CallGetService(NS_TEST_SERVICE_CONTRACTID, myShutdownListener,
                   &myITestService);

    /* Test CallGetInterface */
    nsIInterfaceRequestor *myInterfaceRequestor =
        NS_STATIC_CAST(nsIInterfaceRequestor*, mySupportsPtr);
    CallGetInterface(myInterfaceRequestor, &myITestService);

    return 0;
}
