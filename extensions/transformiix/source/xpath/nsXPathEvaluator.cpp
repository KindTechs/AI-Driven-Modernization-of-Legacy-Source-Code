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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Peter Van der Beken <peterv@netscape.com> (original author)
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

#include "nsXPathEvaluator.h"
#include "nsCOMPtr.h"
#include "nsIDOMClassInfo.h"
#include "nsXPathException.h"
#include "nsXPathExpression.h"
#include "nsXPathNSResolver.h"
#include "nsXPathResult.h"
#include "ProcessorState.h"

NS_IMPL_ADDREF(nsXPathEvaluator)
NS_IMPL_RELEASE(nsXPathEvaluator)
NS_INTERFACE_MAP_BEGIN(nsXPathEvaluator)
  NS_INTERFACE_MAP_ENTRY(nsIDOMXPathEvaluator)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMXPathEvaluator)
  NS_INTERFACE_MAP_ENTRY_EXTERNAL_DOM_CLASSINFO(XPathEvaluator)
NS_INTERFACE_MAP_END

nsXPathEvaluator::nsXPathEvaluator()
{
    NS_INIT_ISUPPORTS();
}

nsXPathEvaluator::~nsXPathEvaluator()
{
}

NS_IMETHODIMP
nsXPathEvaluator::CreateExpression(const nsAString & aExpression,
                                   nsIDOMXPathNSResolver *aResolver,
                                   nsIDOMXPathExpression **aResult)
{
    String expressionString(PromiseFlatString(aExpression).get());
    // XXX (pvdb) TODO Set the right resolver on the ExprParser
    Expr* expression = mParser.createExpr(expressionString);
    if (!expression)
        return NS_ERROR_DOM_INVALID_EXPRESSION_ERR;

    *aResult = new nsXPathExpression(expression);
    if (!*aResult) {
        delete expression;
        return NS_ERROR_OUT_OF_MEMORY;
    }

    NS_ADDREF(*aResult);
    return NS_OK;
}

NS_IMETHODIMP
nsXPathEvaluator::CreateNSResolver(nsIDOMNode *aNodeResolver,
                                   nsIDOMXPathNSResolver **aResult)
{
    NS_ENSURE_ARG(aNodeResolver);
    *aResult = new nsXPathNSResolver(aNodeResolver);
    NS_ENSURE_TRUE(*aResult, NS_ERROR_OUT_OF_MEMORY);

    NS_ADDREF(*aResult);
    return NS_OK;
}

NS_IMETHODIMP
nsXPathEvaluator::Evaluate(const nsAString & aExpression,
                           nsIDOMNode *aContextNode,
                           nsIDOMXPathNSResolver *aResolver,
                           PRUint16 aType,
                           nsIDOMXPathResult *aInResult,
                           nsIDOMXPathResult **aResult)
{
    // XXX Need to check document of aContextNode if created by
    //     QI'ing a document.

    nsCOMPtr<nsIDOMXPathExpression> expression;
    nsresult rv = CreateExpression(aExpression, aResolver,
                                   getter_AddRefs(expression));
    NS_ENSURE_SUCCESS(rv, rv);

    return expression->Evaluate(aContextNode, aType, aInResult, aResult);
}

// From build/XSLTProcessorModule.cpp
nsresult XSLT_CheckMasterXSLTPref();

nsresult
nsXPathEvaluator::CheckMasterXSLTPref()
{
    return XSLT_CheckMasterXSLTPref();
}
