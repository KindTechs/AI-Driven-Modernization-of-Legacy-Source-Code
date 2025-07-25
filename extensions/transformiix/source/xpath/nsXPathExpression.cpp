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

#include "nsXPathExpression.h"
#include "Expr.h"
#include "nsDOMError.h"
#include "nsIDOMClassInfo.h"
#include "nsIDOMXPathNamespace.h"
#include "nsXPathResult.h"
#include "ProcessorState.h"

NS_IMPL_ADDREF(nsXPathExpression)
NS_IMPL_RELEASE(nsXPathExpression)
NS_INTERFACE_MAP_BEGIN(nsXPathExpression)
  NS_INTERFACE_MAP_ENTRY(nsIDOMXPathExpression)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMXPathExpression)
  NS_INTERFACE_MAP_ENTRY_EXTERNAL_DOM_CLASSINFO(XPathExpression)
NS_INTERFACE_MAP_END

nsXPathExpression::nsXPathExpression(Expr* aExpression) : mExpression(aExpression)
{
    NS_INIT_ISUPPORTS();
}

nsXPathExpression::~nsXPathExpression()
{
    delete mExpression;
}

NS_IMETHODIMP
nsXPathExpression::Evaluate(nsIDOMNode *aContextNode,
                            PRUint16 aType,
                            nsIDOMXPathResult *aInResult,
                            nsIDOMXPathResult **aResult)
{
    NS_ENSURE_ARG(aContextNode);

    nsresult rv;
    PRUint16 nodeType;
    rv = aContextNode->GetNodeType(&nodeType);
    NS_ENSURE_SUCCESS(rv, rv);

    if (nodeType == nsIDOMNode::TEXT_NODE ||
        nodeType == nsIDOMNode::CDATA_SECTION_NODE) {
        nsCOMPtr<nsIDOMCharacterData> textNode = do_QueryInterface(aContextNode);
        NS_ENSURE_TRUE(textNode, NS_ERROR_FAILURE);

        if (textNode) {
            PRUint32 textLength;
            textNode->GetLength(&textLength);
            if (textLength == 0)
                return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
        }

        // XXX Need to get logical XPath text node for CDATASection
        //     and Text nodes.
    }
    else if (nodeType != nsIDOMNode::DOCUMENT_NODE &&
             nodeType != nsIDOMNode::ELEMENT_NODE &&
             nodeType != nsIDOMNode::ATTRIBUTE_NODE &&
             nodeType != nsIDOMNode::COMMENT_NODE &&
             nodeType != nsIDOMNode::PROCESSING_INSTRUCTION_NODE &&
             nodeType != nsIDOMXPathNamespace::XPATH_NAMESPACE_NODE) {
        return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
    }

    NS_ENSURE_ARG(aResult);
    *aResult = nsnull;

    nsCOMPtr<nsIDOMDocument> ownerDOMDocument;
    aContextNode->GetOwnerDocument(getter_AddRefs(ownerDOMDocument));
    if (!ownerDOMDocument) {
        ownerDOMDocument = do_QueryInterface(aContextNode);
        NS_ENSURE_TRUE(ownerDOMDocument, NS_ERROR_FAILURE);
    }
    Document document(ownerDOMDocument);
    Node* node = document.createWrapper(aContextNode);

    ProcessorState processorState;
    ExprResult* exprResult = mExpression->evaluate(node, &processorState);
    NS_ENSURE_TRUE(exprResult, NS_ERROR_OUT_OF_MEMORY);

    PRUint16 resultType = aType;
    if (aType == nsIDOMXPathResult::ANY_TYPE) {
        short exprResultType = exprResult->getResultType();
        switch (exprResultType) {
            case ExprResult::NUMBER:
                resultType = nsIDOMXPathResult::NUMBER_TYPE;
                break;
            case ExprResult::STRING:
                resultType = nsIDOMXPathResult::STRING_TYPE;
                break;
            case ExprResult::BOOLEAN:
                resultType = nsIDOMXPathResult::BOOLEAN_TYPE;
                break;
            case ExprResult::NODESET:
                resultType = nsIDOMXPathResult::UNORDERED_NODE_ITERATOR_TYPE;
                break;
            case ExprResult::TREE_FRAGMENT:
                NS_ERROR("Can't return a tree fragment!");
                delete exprResult;
                return NS_ERROR_FAILURE;
        }
    }

    // We need a result object and it must be our implementation.
    nsCOMPtr<nsIXPathResult> xpathResult = do_QueryInterface(aInResult);
    if (!xpathResult) {
        // Either no aInResult or not one of ours.
        xpathResult = new nsXPathResult();
        NS_ENSURE_TRUE(xpathResult, NS_ERROR_OUT_OF_MEMORY);
    }
    rv = xpathResult->SetExprResult(exprResult, resultType);
    delete exprResult;
    NS_ENSURE_SUCCESS(rv, rv);

    return CallQueryInterface(xpathResult, aResult);
}
