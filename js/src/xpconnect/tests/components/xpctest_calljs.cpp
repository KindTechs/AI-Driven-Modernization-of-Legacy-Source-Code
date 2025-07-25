/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   John Bandhauer <jband@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

#include "xpctest_private.h"
#include "nsIXPCScriptable.h"
#include "xpctest_calljs.h"
#include "nsISupports.h"

class xpcTestCallJS : public nsIXPCTestCallJS, public nsIXPCScriptable {
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCTESTCALLJS
    NS_DECL_NSIXPCSCRIPTABLE
    xpcTestCallJS();
    virtual ~xpcTestCallJS();

private:
    nsIXPCTestCallJS* jsobject;
};


NS_IMPL_ISUPPORTS2_CI(xpcTestCallJS, nsIXPCTestCallJS, nsIXPCScriptable);

// The nsIXPCScriptable map declaration that will generate stubs for us...
#define XPC_MAP_CLASSNAME           xpcTestCallJS
#define XPC_MAP_QUOTED_CLASSNAME   "xpcTestCallJS"
#define XPC_MAP_FLAGS               0
#include "xpc_map_end.h" /* This will #undef the above */

xpcTestCallJS :: xpcTestCallJS() {
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();
};

xpcTestCallJS :: ~xpcTestCallJS() {

};

NS_IMETHODIMP xpcTestCallJS :: SetJSObject( nsIXPCTestCallJS* o ) {
    //if (jsobject)
    //  NS_RELEASE( jsobject );
    jsobject = o;
    if ( jsobject ) 
        NS_ADDREF( jsobject );
    return NS_OK;
};

NS_IMETHODIMP xpcTestCallJS :: CallMethodNoArgs(PRBool *_retval) {
    *_retval = PR_TRUE;
    return NS_OK;
};

NS_IMETHODIMP xpcTestCallJS :: Evaluate ( const char *s ) {
    if (jsobject)
        return jsobject->Evaluate(s);
    return NS_OK;
};

NS_IMETHODIMP 
xpcTestCallJS :: EvaluateAndReturnError(nsresult in, nsresult *_retval){
    if (jsobject) {
        jsobject->EvaluateAndReturnError(in, _retval);
    } else {
        *_retval = in;
    }
    return *_retval;
};

NS_IMETHODIMP xpcTestCallJS :: EvaluateAndEatErrors(const char *s) {
    if ( jsobject ) 
        jsobject->Evaluate(s);
    return NS_OK;
};

NS_IMETHODIMP xpcTestCallJS :: UnscriptableMethod() {
    return NS_OK;
};

NS_IMETHODIMP
xpctest::ConstructXPCTestCallJS(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    nsresult rv;
    NS_ASSERTION(aOuter == nsnull, "no aggregation");
    xpcTestCallJS *obj = new xpcTestCallJS();

    if(obj)
    {
        rv = obj->QueryInterface(aIID, aResult);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to find correct interface");
        NS_RELEASE(obj);
    }
    else
    {
        *aResult = nsnull;
        rv = NS_ERROR_OUT_OF_MEMORY;
    }
    return rv;
};
