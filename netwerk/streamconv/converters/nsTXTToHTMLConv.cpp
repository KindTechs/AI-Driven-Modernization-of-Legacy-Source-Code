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

#include "nsTXTToHTMLConv.h"
#include "nsNetUtil.h"
#include "nsIStringStream.h"

#define TOKEN_DELIMITERS NS_LITERAL_STRING("\t\r\n ").get()

// nsISupports methods
NS_IMPL_THREADSAFE_ISUPPORTS4(nsTXTToHTMLConv,
                              nsIStreamConverter,
                              nsITXTToHTMLConv,
                              nsIRequestObserver,
                              nsIStreamListener);


// nsIStreamConverter methods
NS_IMETHODIMP
nsTXTToHTMLConv::Convert(nsIInputStream *aFromStream,
                         const PRUnichar *aFromType, const PRUnichar *aToType,
                         nsISupports *aCtxt, nsIInputStream * *_retval) {
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsTXTToHTMLConv::AsyncConvertData(const PRUnichar *aFromType,
                                  const PRUnichar *aToType,
                                  nsIStreamListener *aListener,
                                  nsISupports *aCtxt) {
    NS_ASSERTION(aListener, "null pointer");
    mListener = aListener;
    return NS_OK;
}


// nsIRequestObserver methods
NS_IMETHODIMP
nsTXTToHTMLConv::OnStartRequest(nsIRequest* request, nsISupports *aContext) {
    mBuffer.Assign(NS_LITERAL_STRING("<html>\n<head><title>"));
    mBuffer.Append(mPageTitle);
    mBuffer.Append(NS_LITERAL_STRING("</title></head>\n<body>\n"));
    if (mPreFormatHTML) {     // Use <pre> tags
      mBuffer.Append(NS_LITERAL_STRING("<pre>\n"));
    }

    // Push mBuffer to the listener now, so the initial HTML will not
    // be parsed in OnDataAvailable().

    nsresult rv = mListener->OnStartRequest(request, aContext);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIInputStream> inputData;
    rv = NS_NewStringInputStream(getter_AddRefs(inputData), mBuffer);
    if (NS_FAILED(rv)) return rv;

    rv = mListener->OnDataAvailable(request, aContext,
                                    inputData, 0, mBuffer.Length());
    if (NS_FAILED(rv)) return rv;
    mBuffer.Assign(NS_LITERAL_STRING(""));
    return rv;
}

NS_IMETHODIMP
nsTXTToHTMLConv::OnStopRequest(nsIRequest* request, nsISupports *aContext,
                               nsresult aStatus) {
    nsresult rv = NS_OK;
    if (mToken) {
        // we still have an outstanding token
        PRInt32 back = mBuffer.FindCharInSet(TOKEN_DELIMITERS);
        if (back == -1) back = mBuffer.Length();
        (void)CatHTML(0, mBuffer.Length());
    }
    if (mPreFormatHTML) {
      mBuffer.Append(NS_LITERAL_STRING("</pre>\n"));
    }
    mBuffer.Append(NS_LITERAL_STRING("\n</body></html>"));    
    
    nsCOMPtr<nsIInputStream> inputData;

    rv = NS_NewStringInputStream(getter_AddRefs(inputData), mBuffer);
    if (NS_FAILED(rv)) return rv;

    rv = mListener->OnDataAvailable(request, aContext,
                                    inputData, 0, mBuffer.Length());
    if (NS_FAILED(rv)) return rv;

    return mListener->OnStopRequest(request, aContext, aStatus);
}

// nsITXTToHTMLConv methods
NS_IMETHODIMP
nsTXTToHTMLConv::SetTitle(const PRUnichar *aTitle) {
  mPageTitle.Assign(aTitle);
  return NS_OK;
}

NS_IMETHODIMP
nsTXTToHTMLConv::PreFormatHTML(PRBool value) {
  mPreFormatHTML = value;
  return NS_OK;
}

// nsIStreamListener method
NS_IMETHODIMP
nsTXTToHTMLConv::OnDataAvailable(nsIRequest* request, nsISupports *aContext,
                                 nsIInputStream *aInStream,
                                 PRUint32 aOffset, PRUint32 aCount) {
    nsresult rv = NS_OK;
    nsString pushBuffer;
    PRUint32 amtRead = 0;
    char *buffer = (char*)nsMemory::Alloc(aCount+1);
    if (!buffer) return NS_ERROR_OUT_OF_MEMORY;
    
    do {
        PRUint32 read = 0;
        rv = aInStream->Read(buffer, aCount-amtRead, &read);
        if (NS_FAILED(rv)) return rv;

        buffer[read] = '\0';
        mBuffer.AppendWithConversion(buffer);
        amtRead += read;

        PRInt32 front = -1, back = -1, tokenLoc = -1, cursor = 0; 

        while ( (tokenLoc = FindToken(cursor, &mToken)) > -1) {
            front = mBuffer.RFindCharInSet(TOKEN_DELIMITERS, tokenLoc);
            front++;
    
            back = mBuffer.FindCharInSet(TOKEN_DELIMITERS, tokenLoc);
            if (back == -1) {
                // didn't find an ending, buffer up.
                mBuffer.Left(pushBuffer, front);
                cursor = front;
                break;
            } else {
                // found the end of the token.
                cursor = CatHTML(front, back);
            }
        }

        PRInt32 end = mBuffer.RFind(TOKEN_DELIMITERS, mBuffer.Length());
        mBuffer.Left(pushBuffer, PR_MAX(cursor, end));
        mBuffer.Cut(0, PR_MAX(cursor, end));
        cursor = 0;

        if (!pushBuffer.IsEmpty()) {
            nsCOMPtr<nsIInputStream> inputData;

            rv = NS_NewStringInputStream(getter_AddRefs(inputData), pushBuffer);
            if (NS_FAILED(rv)) {
                nsMemory::Free(buffer);
                return rv;
            }

            rv = mListener->OnDataAvailable(request, aContext,
                                            inputData, 0, pushBuffer.Length());
            if (NS_FAILED(rv)) {
                nsMemory::Free(buffer);
                return rv;
            }
        }
    } while (amtRead < aCount);

    nsMemory::Free(buffer);
    return rv; 
} 
// nsTXTToHTMLConv methods
nsTXTToHTMLConv::nsTXTToHTMLConv() {
    NS_INIT_REFCNT();
    mToken = nsnull;
    mPreFormatHTML = PR_FALSE;
}

static PRBool CleanupTokens(void *aElement, void *aData) {
    if (aElement) delete (convToken*)aElement;
    return PR_TRUE;
}

nsTXTToHTMLConv::~nsTXTToHTMLConv() {
    mTokens.EnumerateForwards((nsVoidArrayEnumFunc)CleanupTokens, nsnull);
}

nsresult
nsTXTToHTMLConv::Init() {
    nsresult rv = NS_OK;

    // build up the list of tokens to handle
    convToken *token = new convToken;
    if (!token) return NS_ERROR_OUT_OF_MEMORY;
    token->prepend = PR_TRUE;
    token->token.Assign(NS_LITERAL_STRING("http://")); // XXX need to iterate through all protos
    mTokens.AppendElement(token);

    token = new convToken;
    if (!token) return NS_ERROR_OUT_OF_MEMORY;
    token->prepend = PR_TRUE;
    token->token.Assign(PRUnichar('@'));
    token->modText.Assign(NS_LITERAL_STRING("mailto:"));
    mTokens.AppendElement(token);
  
    return rv;
}

PRInt32
nsTXTToHTMLConv::FindToken(PRInt32 cursor, convToken* *_retval) {
    PRInt32 loc = -1, firstToken = mBuffer.Length();
    PRInt8 token = -1;
    for (PRInt8 i=0; i < mTokens.Count(); i++) {
        loc = mBuffer.Find(((convToken*)mTokens[i])->token, cursor);
        if (loc != -1)
            if (loc < firstToken) {
                firstToken = loc;
                token = i;
            }
    }
    if (token != -1) {
        *_retval = (convToken*)mTokens[token];   
        return firstToken;
    } else {
        return -1;
    }
}

PRInt32
nsTXTToHTMLConv::CatHTML(PRInt32 front, PRInt32 back) {
    PRInt32 cursor = 0;
    if (!mToken->prepend) {
        // replace the entire token (from delimiter to delimiter)
        mBuffer.Cut(front, back);
        mBuffer.Insert(mToken->modText, front);
    } else {
        nsString linkText;
        // href is implied
        PRInt32 modLen = mToken->modText.Length();
        mBuffer.Mid(linkText, front, back-front);
        mBuffer.Insert(NS_LITERAL_STRING("<a href=\""), front);
        cursor += front+9;
        if (modLen)
            mBuffer.Insert(mToken->modText, cursor);
        cursor += modLen-front+back;
        mBuffer.Insert(NS_LITERAL_STRING("\">"), cursor);
        cursor += 2;
        mBuffer.Insert(linkText, cursor);
        cursor += linkText.Length();
        mBuffer.Insert(NS_LITERAL_STRING("</a>"), cursor);
        cursor += 4;
    }
    mToken = nsnull; // indicates completeness
    return cursor;
}

