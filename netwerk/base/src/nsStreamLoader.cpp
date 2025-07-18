/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#include "nsStreamLoader.h"
#include "nsIInputStream.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsIChannel.h"
#include "nsIHttpChannel.h"
#include "nsProxiedService.h"

static NS_DEFINE_CID(kProxyObjectManagerCID, NS_PROXYEVENT_MANAGER_CID);

NS_IMETHODIMP
nsStreamLoader::Init(nsIURI* aURL,
                     nsIStreamLoaderObserver* observer,
                     nsISupports* context,
                     nsILoadGroup* aGroup,
                     nsIInterfaceRequestor* notificationCallbacks,
                     nsLoadFlags loadAttributes,
                     nsIURI *referrer,
                     PRUint32 referrerFlags)
{
  nsresult rv = NS_OK;
  
  nsCOMPtr<nsIChannel> channel;

  rv = NS_NewChannel(getter_AddRefs(channel), aURL, nsnull, aGroup,
                     notificationCallbacks, loadAttributes);
  if (NS_FAILED(rv)) return rv;

  if (referrer) {
    nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel));

    if (httpChannel) {
      rv = httpChannel->SetReferrer(referrer, referrerFlags);
      if (NS_FAILED(rv)) return rv;
    }
  }

  rv = channel->AsyncOpen(this, context);

  if (NS_FAILED(rv) && observer) {
    // don't callback synchronously as it puts the caller
    // in a recursive situation and breaks the asynchronous
    // semantics of nsIStreamLoader
    nsresult rv2 = NS_OK;
    nsCOMPtr<nsIProxyObjectManager> pIProxyObjectManager = 
             do_GetService(kProxyObjectManagerCID, &rv2);
    if (NS_FAILED(rv2)) return rv2;

    nsCOMPtr<nsIStreamLoaderObserver> pObserver;
    rv2 = pIProxyObjectManager->GetProxyForObject(NS_CURRENT_EVENTQ, 
              NS_GET_IID(nsIStreamLoaderObserver), observer, 
              PROXY_ASYNC | PROXY_ALWAYS, getter_AddRefs(pObserver));
    if (NS_FAILED(rv2)) return rv2;

    rv = pObserver->OnStreamComplete(this, context, rv, 0, nsnull);
  }

  mObserver = observer;
  mContext  = context;
  return rv;
}

NS_METHOD
nsStreamLoader::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
  if (aOuter) return NS_ERROR_NO_AGGREGATION;

  nsStreamLoader* it = new nsStreamLoader();
  if (it == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(it);
  nsresult rv = it->QueryInterface(aIID, aResult);
  NS_RELEASE(it);
  return rv;
}

NS_IMPL_ISUPPORTS3(nsStreamLoader, nsIStreamLoader,
                   nsIRequestObserver, nsIStreamListener)

NS_IMETHODIMP 
nsStreamLoader::GetNumBytesRead(PRUint32* aNumBytes)
{
  if (!mData.IsEmpty()) {
    *aNumBytes = mData.Length();
  }
  else {
    *aNumBytes = 0;
  }

  return NS_OK;
}

/* readonly attribute nsIRequest request; */
NS_IMETHODIMP 
nsStreamLoader::GetRequest(nsIRequest **aRequest)
{
    NS_IF_ADDREF(*aRequest = mRequest);
    return NS_OK;
}

NS_IMETHODIMP 
nsStreamLoader::OnStartRequest(nsIRequest* request, nsISupports *ctxt)
{
  nsCOMPtr<nsIChannel> chan( do_QueryInterface(request) );
  if (chan) {
      PRInt32 contentLength = -1;
      chan->GetContentLength(&contentLength);
      if (contentLength >= 0) {
          // preallocate buffer
          mData.SetCapacity(contentLength + 1);
      }
  }
  return NS_OK;
}

NS_IMETHODIMP 
nsStreamLoader::OnStopRequest(nsIRequest* request, nsISupports *ctxt,
                              nsresult aStatus)
{
  if (mObserver) {
    // provide nsIStreamLoader::request during call to OnStreamComplete
    mRequest = request;
    mObserver->OnStreamComplete(this, mContext, aStatus, 
                                mData.Length(), mData.get());
    // done.. cleanup
    mRequest = 0;
    mObserver = 0;
    mContext = 0;
  }
  return NS_OK;
}

NS_METHOD
nsStreamLoader::WriteSegmentFun(nsIInputStream *inStr,
                                void *closure,
                                const char *fromSegment,
                                PRUint32 toOffset,
                                PRUint32 count,
                                PRUint32 *writeCount)
{
  nsStreamLoader *self = (nsStreamLoader *) closure;

  self->mData.Append(fromSegment, count);
  *writeCount = count;

  return NS_OK;
}

NS_IMETHODIMP 
nsStreamLoader::OnDataAvailable(nsIRequest* request, nsISupports *ctxt, 
                                nsIInputStream *inStr, 
                                PRUint32 sourceOffset, PRUint32 count)
{
  PRUint32 countRead;
  return inStr->ReadSegments(WriteSegmentFun, this, count, &countRead);
}
