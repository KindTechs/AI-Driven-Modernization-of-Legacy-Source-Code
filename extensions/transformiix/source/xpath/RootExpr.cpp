/*
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
 * The Original Code is TransforMiiX XSLT processor.
 * 
 * The Initial Developer of the Original Code is The MITRE Corporation.
 * Portions created by MITRE are Copyright (C) 1999 The MITRE Corporation.
 *
 * Portions created by Keith Visco as a Non MITRE employee,
 * (C) 1999 Keith Visco. All Rights Reserved.
 * 
 * Contributor(s): 
 * Keith Visco, kvisco@ziplink.net
 *   -- original author.
 * 
 */

#include "Expr.h"

/**
 * Creates a new RootExpr
 * @param aSerialize should this RootExpr be serialized
 */
RootExpr::RootExpr(MBool aSerialize) {
    mSerialize = aSerialize;
}

/**
 * Evaluates this Expr based on the given context node and processor state
 * @param context the context node for evaluation of this Expr
 * @param ps the ContextState containing the stack information needed
 * for evaluation
 * @return the result of the evaluation
**/
ExprResult* RootExpr::evaluate(Node* context, ContextState* cs)
{
    if (!context)
        return new StringResult("error");

    if (context->getNodeType() != Node::DOCUMENT_NODE)
        return new NodeSet(context->getOwnerDocument());
    return new NodeSet(context);
} //-- evaluate

/**
 * Returns the default priority of this Pattern based on the given Node,
 * context Node, and ContextState.
**/
double RootExpr::getDefaultPriority(Node* node, Node* context, ContextState* cs) {
    return 0.5;
} //-- getDefaultPriority

/**
 * Determines whether this NodeExpr matches the given node within
 * the given context
**/
MBool RootExpr::matches(Node* node, Node* context, ContextState* cs) {
    return node && (node->getNodeType() == Node::DOCUMENT_NODE);
} //-- matches

/**
 * Returns the String representation of this Expr.
 * @param dest the String to use when creating the String
 * representation. The String representation will be appended to
 * any data in the destination String, to allow cascading calls to
 * other #toString() methods for Expressions.
 * @return the String representation of this Expr.
**/
void RootExpr::toString(String& dest) {
    if (mSerialize)
        dest.append('/');
} //-- toString
