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
 * Contributor(s): Bradley Baetz <bbaetz@student.usyd.edu.au>
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

#include "nsFTPChannel.h"
#include "nsFtpConnectionThread.h"
#include "nsFtpControlConnection.h"
#include "nsFtpProtocolHandler.h"

#include <limits.h>
#include <ctype.h>

#include "nsISocketTransport.h"
#include "nsIStreamConverterService.h"
#include "nsIStreamListenerTee.h"
#include "nsReadableUtils.h"
#include "prprf.h"
#include "prlog.h"
#include "prtime.h"
#include "prnetdb.h"
#include "netCore.h"
#include "ftpCore.h"
#include "nsProxiedService.h"
#include "nsCRT.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIURL.h"
#include "nsEscape.h"
#include "nsNetUtil.h"
#include "nsIDNSService.h" // for host error code
#include "nsCPasswordManager.h"
#include "nsIMemory.h"
#include "nsIStringStream.h"
#include "nsIPref.h"
#include "nsMimeTypes.h"
#include "nsIStringBundle.h"

#include "nsICacheEntryDescriptor.h"
#include "nsICacheListener.h"

#include "nsIResumableChannel.h"
#include "nsIResumableEntityID.h"

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_CID(kStreamConverterServiceCID,    NS_STREAMCONVERTERSERVICE_CID);
static NS_DEFINE_CID(kStreamListenerTeeCID, NS_STREAMLISTENERTEE_CID);
static NS_DEFINE_CID(kSocketTransportServiceCID, NS_SOCKETTRANSPORTSERVICE_CID);


#if defined(PR_LOGGING)
extern PRLogModuleInfo* gFTPLog;
#endif /* PR_LOGGING */

#define NS_SUCCESS_IGNORE_NOTIFICATION 0x00000666

class DataRequestForwarder : public nsIFTPChannel, 
                             public nsIStreamListener,
                             public nsIInterfaceRequestor,
                             public nsIProgressEventSink,
                             public nsIResumableChannel
{
public:
    DataRequestForwarder();
    virtual ~DataRequestForwarder();
    nsresult Init(nsIRequest *request);

    nsresult SetStreamListener(nsIStreamListener *listener);
    nsresult SetCacheEntry(nsICacheEntryDescriptor *entry, PRBool writing);
    nsresult SetEntityID(nsIResumableEntityID *entity);

    NS_DECL_ISUPPORTS
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSIINTERFACEREQUESTOR
    NS_DECL_NSIPROGRESSEVENTSINK
    NS_DECL_NSIRESUMABLECHANNEL

    NS_FORWARD_NSIREQUEST(mRequest->)
    NS_FORWARD_NSICHANNEL(mFTPChannel->)
    NS_FORWARD_NSIFTPCHANNEL(mFTPChannel->)
    
    PRUint32 GetBytesTransfered() {return mBytesTransfered;} ;
    void Uploading(PRBool value);
    void SetRetrying(PRBool retry);

protected:

    nsCOMPtr<nsIRequest>              mRequest;
    nsCOMPtr<nsIFTPChannel>           mFTPChannel;
    nsCOMPtr<nsIStreamListener>       mListener;
    nsCOMPtr<nsIProgressEventSink>    mEventSink;
    nsCOMPtr<nsICacheEntryDescriptor> mCacheEntry;
    nsCOMPtr<nsIResumableEntityID>    mEntityID;

    PRUint32 mBytesTransfered;
    PRPackedBool   mDelayedOnStartFired;
    PRPackedBool   mUploading;
    PRPackedBool   mRetrying;
    
    nsresult DelayedOnStartRequest(nsIRequest *request, nsISupports *ctxt);
};


// The whole purpose of this class is to opaque 
// the socket transport so that clients only see
// the same nsIChannel/nsIRequest that they started.

NS_IMPL_THREADSAFE_ISUPPORTS8(DataRequestForwarder, 
                              nsIStreamListener, 
                              nsIRequestObserver, 
                              nsIFTPChannel,
                              nsIResumableChannel,
                              nsIChannel,
                              nsIRequest,
                              nsIInterfaceRequestor,
                              nsIProgressEventSink);


DataRequestForwarder::DataRequestForwarder()
{
    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) DataRequestForwarder CREATED\n", this));
        
    mBytesTransfered = 0;
    mRetrying = mUploading = mDelayedOnStartFired = PR_FALSE;
    NS_INIT_ISUPPORTS();
}

DataRequestForwarder::~DataRequestForwarder()
{
}

// nsIInterfaceRequestor method
NS_IMETHODIMP
DataRequestForwarder::GetInterface(const nsIID &anIID, void **aResult ) 
{
    if (anIID.Equals(NS_GET_IID(nsIProgressEventSink))) 
    {
        *aResult = NS_STATIC_CAST(nsIProgressEventSink*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    } 
    return NS_ERROR_NO_INTERFACE;
}


nsresult 
DataRequestForwarder::Init(nsIRequest *request)
{
    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) DataRequestForwarder Init [request=%x]\n", this, request));

    NS_ENSURE_ARG(request);

    // for the forwarding declarations.
    mRequest    = request;
    mFTPChannel = do_QueryInterface(request);
    mEventSink  = do_QueryInterface(request);
    mListener   = do_QueryInterface(request);
    
    if (!mRequest || !mFTPChannel)
        return NS_ERROR_FAILURE;
    
    return NS_OK;
}

void 
DataRequestForwarder::Uploading(PRBool value)
{
    mUploading = value;
}

nsresult 
DataRequestForwarder::SetCacheEntry(nsICacheEntryDescriptor *cacheEntry, PRBool writing)
{
    // if there is a cache entry descriptor, send data to it.
    if (!cacheEntry) 
        return NS_ERROR_FAILURE;
    
    mCacheEntry = cacheEntry;
    if (!writing)
        return NS_OK;

    nsCOMPtr<nsITransport> cacheTransport;
    nsresult rv = cacheEntry->GetTransport(getter_AddRefs(cacheTransport));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIOutputStream> out;
    rv = cacheTransport->OpenOutputStream(0, PRUint32(-1), 0, getter_AddRefs(out));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIStreamListenerTee> tee =
        do_CreateInstance(kStreamListenerTeeCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = tee->Init(mListener, out);
    if (NS_FAILED(rv)) return rv;

    mListener = do_QueryInterface(tee, &rv);

    return NS_OK;
}


nsresult 
DataRequestForwarder::SetStreamListener(nsIStreamListener *listener)
{
    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) DataRequestForwarder SetStreamListener [listener=%x]\n", this, listener)); 
    
    mListener = listener;
    if (!mListener) 
        return NS_ERROR_FAILURE;

    return NS_OK;
}

nsresult
DataRequestForwarder::SetEntityID(nsIResumableEntityID *aEntityID)
{
    mEntityID = aEntityID;
    return NS_OK;
}

NS_IMETHODIMP
DataRequestForwarder::GetEntityID(nsIResumableEntityID* *aEntityID)
{
    *aEntityID = mEntityID;
    NS_IF_ADDREF(*aEntityID);
    return NS_OK;
}

NS_IMETHODIMP
DataRequestForwarder::AsyncOpenAt(nsIStreamListener *,
                                  nsISupports *,
                                  unsigned int, 
                                  nsIResumableEntityID *)
{
    // We shouldn't get here. This class only exists in the middle of a
    // request
    NS_NOTREACHED("DataRequestForwarder::AsyncOpenAt");
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult 
DataRequestForwarder::DelayedOnStartRequest(nsIRequest *request, nsISupports *ctxt)
{
    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) DataRequestForwarder DelayedOnStartRequest \n", this)); 
    return mListener->OnStartRequest(this, ctxt); 
}

void
DataRequestForwarder::SetRetrying(PRBool retry)
{
    // The problem here is that if we send a second PASV, our listener would
    // get an OnStop from the socket transport, and fail. So we temporarily
    // suspend notifications
    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) DataRequestForwarder SetRetrying [retry=%d]\n", this, retry));

    mRetrying = retry;
    mDelayedOnStartFired = PR_FALSE;
}

NS_IMETHODIMP 
DataRequestForwarder::OnStartRequest(nsIRequest *request, nsISupports *ctxt)
{
    NS_ASSERTION(mListener, "No Listener Set.");
    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) DataRequestForwarder OnStartRequest \n[mRetrying=%d]", this, mRetrying)); 

    if (mRetrying) {
        mRetrying = PR_FALSE;
        return NS_OK;
    }

    if (!mListener)
        return NS_ERROR_NOT_INITIALIZED;

    return NS_OK;
}

NS_IMETHODIMP
DataRequestForwarder::OnStopRequest(nsIRequest *request, nsISupports *ctxt, nsresult statusCode)
{
    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) DataRequestForwarder OnStopRequest [status=%x, mRetrying=%d]\n", this, statusCode, mRetrying)); 

    if (mRetrying || statusCode == NS_SUCCESS_IGNORE_NOTIFICATION)
        return NS_OK;

    // If there were no calls to ODA, then the onstart won't have been
    // fired - bug 122913
    if (!mDelayedOnStartFired) { 
        mDelayedOnStartFired = PR_TRUE;
        nsresult rv = DelayedOnStartRequest(request, ctxt);
        if (NS_FAILED(rv)) return rv;
    }
    
    nsCOMPtr<nsITransportRequest> trequest(do_QueryInterface(request));
    if (trequest)
    {
        nsCOMPtr<nsITransport> trans;
        trequest->GetTransport(getter_AddRefs(trans));
        nsCOMPtr<nsISocketTransport> sTrans (do_QueryInterface(trans));
        if (sTrans)
            sTrans->SetReuseConnection(PR_FALSE);
    }

    if (!mListener)
        return NS_ERROR_NOT_INITIALIZED;

    return mListener->OnStopRequest(this, ctxt, statusCode);
}

NS_IMETHODIMP
DataRequestForwarder::OnDataAvailable(nsIRequest *request, nsISupports *ctxt, nsIInputStream *input, PRUint32 offset, PRUint32 count)
{ 
    nsresult rv;
    NS_ASSERTION(mListener, "No Listener Set.");
    NS_ASSERTION(!mUploading, "Since we are uploading, we should never get a ODA");
 
    if (!mListener)
        return NS_ERROR_NOT_INITIALIZED;

    // we want to delay firing the onStartRequest until we know that there is data
    if (!mDelayedOnStartFired) { 
        mDelayedOnStartFired = PR_TRUE;
        rv = DelayedOnStartRequest(request, ctxt);
        if (NS_FAILED(rv)) return rv;
    }

    rv = mListener->OnDataAvailable(this, ctxt, input, mBytesTransfered, count); 
    mBytesTransfered += count;
    return rv;
} 


// nsIProgressEventSink methods
NS_IMETHODIMP
DataRequestForwarder::OnStatus(nsIRequest *request, nsISupports *aContext,
                       nsresult aStatus, const PRUnichar* aStatusArg)
{
    if (!mEventSink)
        return NS_OK;

    return mEventSink->OnStatus(nsnull, nsnull, aStatus, nsnull);
}

NS_IMETHODIMP
DataRequestForwarder::OnProgress(nsIRequest *request, nsISupports* aContext,
                                  PRUint32 aProgress, PRUint32 aProgressMax) {
    if (!mEventSink)
        return NS_OK;

    // we want to delay firing the onStartRequest until we know that there is data
    if (!mDelayedOnStartFired) { 
        mDelayedOnStartFired = PR_TRUE;
        nsresult rv = DelayedOnStartRequest(request, aContext);
        NS_ASSERTION(NS_SUCCEEDED(rv), "DelayedOnStartRequest failed");
    }
    
    PRUint32 count = mUploading ? aProgress : mBytesTransfered;
    PRUint32 max   = mUploading ? aProgressMax : 0;
    return mEventSink->OnProgress(this, nsnull, count, max);
}



NS_IMPL_THREADSAFE_ISUPPORTS3(nsFtpState,
                              nsIStreamListener, 
                              nsIRequestObserver, 
                              nsIRequest);

nsFtpState::nsFtpState() {
    NS_INIT_ISUPPORTS();

    PR_LOG(gFTPLog, PR_LOG_ALWAYS, ("(%x) nsFtpState created", this));
    // bool init
    mRETRFailed = PR_FALSE;
    mWaitingForDConn = mTryingCachedControl = mRetryPass = PR_FALSE;
    mFireCallbacks = mKeepRunning = mAnonymous = PR_TRUE;

    mAction = GET;
    mState = FTP_COMMAND_CONNECT;
    mNextState = FTP_S_USER;

    mInternalError = NS_OK;
    mSuspendCount = 0;
    mPort = 21;

    mReceivedControlData = PR_FALSE;
    mControlStatus = NS_OK;

    mIPv6Checked = PR_FALSE;
    mIPv6ServerAddress = nsnull;
    
    mControlConnection = nsnull;
    mDRequestForwarder = nsnull;
    mFileSize          = PRUint32(-1);
    mModTime           = -1;
}

nsFtpState::~nsFtpState() 
{
    PR_LOG(gFTPLog, PR_LOG_ALWAYS, ("(%x) nsFtpState destroyed", this));
    
    if (mIPv6ServerAddress) nsMemory::Free(mIPv6ServerAddress);
    NS_IF_RELEASE(mDRequestForwarder);
}

nsresult
nsFtpState::GetEntityID(nsIResumableEntityID** aEntityID)
{
    *aEntityID = mEntityID;
    NS_IF_ADDREF(*aEntityID);
    return NS_OK;
}

// nsIStreamListener implementation
NS_IMETHODIMP
nsFtpState::OnDataAvailable(nsIRequest *request,
                            nsISupports *aContext,
                            nsIInputStream *aInStream,
                            PRUint32 aOffset, 
                            PRUint32 aCount)
{    
    if (aCount == 0)
        return NS_OK; /*** should this be an error?? */
    
    if (!mReceivedControlData) {
        nsCOMPtr<nsIProgressEventSink> sink(do_QueryInterface(mChannel));
        if (sink)
            // parameter can be null cause the channel fills them in.
            sink->OnStatus(nsnull, nsnull, 
                           NS_NET_STATUS_BEGIN_FTP_TRANSACTION, nsnull);
        
        mReceivedControlData = PR_TRUE;
    }

    char* buffer = (char*)nsMemory::Alloc(aCount + 1);
    nsresult rv = aInStream->Read(buffer, aCount, &aCount);
    if (NS_FAILED(rv)) 
    {
        PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) nsFtpState - error reading %x\n", this, rv));
        return NS_ERROR_FAILURE;
    }
    buffer[aCount] = '\0';
        

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) reading %d bytes: \"%s\"", this, aCount, buffer));

    // Sometimes we can get two responses in the same packet, eg from LIST.
    // So we need to parse the response line by line
    nsCString lines(mControlReadCarryOverBuf);
    lines.Append(buffer, aCount);
    // Clear the carryover buf - if we still don't have a line, then it will
    // be reappended below
    mControlReadCarryOverBuf.Truncate();

    const char* currLine = lines.get();
    while (*currLine) {
        const char* eol = strstr(currLine, CRLF);
        if (!eol) {
            mControlReadCarryOverBuf.Assign(currLine);
            break;
        }

        // Append the current segment, including the CRLF
        nsCAutoString line;
        line.Assign(currLine, eol - currLine + 2);
        
        // Does this start with a response code?
        PRBool startNum = (line.Length() >= 3 &&
                           isdigit(line[0]) &&
                           isdigit(line[1]) &&
                           isdigit(line[2]));

        if (mResponseMsg.IsEmpty()) {
            // If we get here, then we know that we have a complete line, and
            // that it is the first one

            NS_ASSERTION(line.Length() > 4 && startNum,
                         "Read buffer doesn't include response code");
            
            mResponseCode = atoi(PromiseFlatCString(Substring(line,0,3)).get());
        }

        mResponseMsg.Append(line);

        // This is the last line if its 3 numbers followed by a space
        if (startNum && line[3] == ' ') {
            // yup. last line, let's move on.
            if (mState == mNextState) {
                NS_ASSERTION(0, "ftp read state mixup");
                mInternalError = NS_ERROR_FAILURE;
                mState = FTP_ERROR;
            } else {
                mState = mNextState;
            }

            if (mFTPEventSink)
                mFTPEventSink->OnFTPControlLog(PR_TRUE, mResponseMsg.get());
            
            rv = Process();
            mResponseMsg.Truncate();
            if (NS_FAILED(rv)) return rv;
        }

        currLine = eol+2; // CR+LF
    }

    return NS_OK;
}


// nsIRequestObserver implementation
NS_IMETHODIMP
nsFtpState::OnStartRequest(nsIRequest *request, nsISupports *aContext)
{
#if defined(PR_LOGGING)
    nsCAutoString spec;
    (void)mURL->GetAsciiSpec(spec);

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) nsFtpState::OnStartRequest() (spec =%s)\n",
        this, spec.get()));
#endif
    return NS_OK;
}

NS_IMETHODIMP
nsFtpState::OnStopRequest(nsIRequest *request, nsISupports *aContext,
                            nsresult aStatus)
{
    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) nsFtpState::OnStopRequest() rv=%x\n", this, aStatus));
    mControlStatus = aStatus;

    // HACK.  This may not always work.  I am assuming that I can mask the OnStopRequest
    // notification since I created it via the control connection.
    if (mTryingCachedControl && NS_FAILED(aStatus) && NS_SUCCEEDED(mInternalError)) {
        PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) nsFtpState::OnStopRequest() cache connect failed.  Reconnecting...\n", this, aStatus));
        mTryingCachedControl = PR_FALSE;
        Connect();
        return NS_OK;
    }        

    if (NS_FAILED(aStatus)) // aStatus will be NS_OK if we are sucessfully disconnecing the control connection. 
        StopProcessing();

    return NS_OK;
}

nsresult
nsFtpState::EstablishControlConnection()
{
    nsresult rv;

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) trying cached control\n", this));
        
    nsISupports* connection;
    (void) nsFtpProtocolHandler::RemoveConnection(mURL, &connection);
    
    if (connection) {
        mControlConnection = NS_STATIC_CAST(nsFtpControlConnection*, (nsIStreamListener*)connection);
        if (mControlConnection->IsAlive())
        {
            // set stream listener of the control connection to be us.        
            (void) mControlConnection->SetStreamListener(NS_STATIC_CAST(nsIStreamListener*, this));
            
            // read cached variables into us. 
            mServerType = mControlConnection->mServerType;           
            mPassword   = mControlConnection->mPassword;
            mPwd        = mControlConnection->mPwd;
            mTryingCachedControl = PR_TRUE;
            
            // we're already connected to this server, skip login.
            mState = FTP_S_PASV;
            mResponseCode = 530;  //assume the control connection was dropped.
            mControlStatus = NS_OK;
            mReceivedControlData = PR_FALSE;  // For this request, we have not.

            // if we succeed, return.  Otherwise, we need to 
            // create a transport
            rv = mControlConnection->Connect(mProxyInfo);
            if (NS_SUCCEEDED(rv))
                return rv;
        }
#if defined(PR_LOGGING)
        else 
        {
            PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) isAlive return false\n", this));
        }
#endif
    }

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) creating control\n", this));
        
    mState = FTP_READ_BUF;
    mNextState = FTP_S_USER;
    
    nsCAutoString host;
    rv = mURL->GetAsciiHost(host);
    if (NS_FAILED(rv)) return rv;

    mControlConnection = new nsFtpControlConnection(host.get(), mPort);
    if (!mControlConnection) return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(mControlConnection);
        
    // Must do it this way 'cuz the channel intercepts the progress notifications.
    (void) mControlConnection->SetStreamListener(NS_STATIC_CAST(nsIStreamListener*, this));

    return mControlConnection->Connect(mProxyInfo);
}

void 
nsFtpState::MoveToNextState(FTP_STATE nextState)
{
    if (NS_FAILED(mInternalError)) 
    {
        mState = FTP_ERROR;
        PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) FAILED (%x)\n", this, mInternalError));
    }
    else
    {
        mState = FTP_READ_BUF;
        mNextState = nextState;
    }  
}

nsresult
nsFtpState::Process() 
{
    nsresult    rv = NS_OK;
    PRBool      processingRead = PR_TRUE;
    
    while (mKeepRunning && processingRead) 
    {
        switch(mState) 
        {
          case FTP_COMMAND_CONNECT:
              KillControlConnection();
              PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) Establishing control connection...", this));
              mInternalError = EstablishControlConnection();  // sets mState
              if (NS_FAILED(mInternalError)) {
                  mState = FTP_ERROR;
                  PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) FAILED\n", this));
              }
              else
                  PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) SUCCEEDED\n", this));
              break;
    
          case FTP_READ_BUF:
              PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) Waiting for control data (%x)...", this, rv));
              processingRead = PR_FALSE;
              break;
          
          case FTP_ERROR: // xx needs more work to handle dropped control connection cases
              if (mTryingCachedControl && mResponseCode == 530 &&
                  mInternalError == NS_ERROR_FTP_PASV) {
                  // The user was logged out during an pasv operation
                  // we want to restart this request with a new control
                  // channel.
                  mState = FTP_COMMAND_CONNECT;
              }
              else if (mResponseCode == 421 && 
                       mInternalError != NS_ERROR_FTP_LOGIN) {
                  // The command channel dropped for some reason.
                  // Fire it back up, unless we were trying to login
                  // in which case the server might just be telling us
                  // that the max number of users has been reached...
                  mState = FTP_COMMAND_CONNECT;
              } else {
                  PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) FTP_ERROR - Calling StopProcessing\n", this));
                  rv = StopProcessing();
                  NS_ASSERTION(NS_SUCCEEDED(rv), "StopProcessing failed.");
                  processingRead = PR_FALSE;
              }
              break;
          
          case FTP_COMPLETE:
              PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) COMPLETE\n", this));
              rv = StopProcessing();
              NS_ASSERTION(NS_SUCCEEDED(rv), "StopProcessing failed.");
              processingRead = PR_FALSE;
              break;

// USER           
          case FTP_S_USER:
            rv = S_user();
            
            if (NS_FAILED(rv))
                mInternalError = NS_ERROR_FTP_LOGIN;
            
            MoveToNextState(FTP_R_USER);
            break;
            
          case FTP_R_USER:
            mState = R_user();
            
            if (FTP_ERROR == mState)
                mInternalError = NS_ERROR_FTP_LOGIN;
            
            break;
// PASS            
          case FTP_S_PASS:
            rv = S_pass();
            
            if (NS_FAILED(rv))
                mInternalError = NS_ERROR_FTP_LOGIN;
            
            MoveToNextState(FTP_R_PASS);
            break;
            
          case FTP_R_PASS:
            mState = R_pass();
            
            if (FTP_ERROR == mState)
                mInternalError = NS_ERROR_FTP_LOGIN;
            
            break;
// ACCT            
          case FTP_S_ACCT:
            rv = S_acct();
            
            if (NS_FAILED(rv))
                mInternalError = NS_ERROR_FTP_LOGIN;
            
            MoveToNextState(FTP_R_ACCT);
            break;
            
          case FTP_R_ACCT:
            mState = R_acct();
            
            if (FTP_ERROR == mState)
                mInternalError = NS_ERROR_FTP_LOGIN;
            
            break;

// SYST            
          case FTP_S_SYST:
            rv = S_syst();
            
            if (NS_FAILED(rv))
                mInternalError = NS_ERROR_FTP_LOGIN;
            
            MoveToNextState(FTP_R_SYST);
            break;
            
          case FTP_R_SYST:
            mState = R_syst();
            
            if (FTP_ERROR == mState)
                mInternalError = NS_ERROR_FTP_LOGIN;

            break;

// TYPE            
          case FTP_S_TYPE:
            rv = S_type();
            
            if (NS_FAILED(rv))
                mInternalError = rv;
            
            MoveToNextState(FTP_R_TYPE);
            break;
            
          case FTP_R_TYPE:
            mState = R_type();
            
            if (FTP_ERROR == mState)
                mInternalError = NS_ERROR_FAILURE;
            
            break;
// CWD            
          case FTP_S_CWD:
            rv = S_cwd();
            
            if (NS_FAILED(rv))
                mInternalError = NS_ERROR_FTP_CWD;
            
            MoveToNextState(FTP_R_CWD);
            break;
            
          case FTP_R_CWD:
            mState = R_cwd();
            
            if (FTP_ERROR == mState)
                mInternalError = NS_ERROR_FTP_CWD;
            break;
       
// LIST
          case FTP_S_LIST:
            rv = S_list();
            
            if (NS_FAILED(rv))
                mInternalError = NS_ERROR_FTP_CWD;
            
            MoveToNextState(FTP_R_LIST);
            break;

          case FTP_R_LIST:        
            mState = R_list();
            
            if (FTP_ERROR == mState)
                mInternalError = NS_ERROR_FAILURE;

            break;

// SIZE            
          case FTP_S_SIZE:
            rv = S_size();
            
            if (NS_FAILED(rv))
                mInternalError = rv;
            
            MoveToNextState(FTP_R_SIZE);
            break;
            
          case FTP_R_SIZE: 
            mState = R_size();
            
            if (FTP_ERROR == mState)
                mInternalError = NS_ERROR_FAILURE;
            
            break;

// REST        
          case FTP_S_REST:
            rv = S_rest();
            
            if (NS_FAILED(rv))
                mInternalError = rv;
            
            MoveToNextState(FTP_R_REST);
            break;
            
          case FTP_R_REST:
            mState = R_rest();
            
            if (FTP_ERROR == mState)
                mInternalError = NS_ERROR_FAILURE;
            
            break;

// MDTM
          case FTP_S_MDTM:
            rv = S_mdtm();
            if (NS_FAILED(rv))
                mInternalError = rv;
            MoveToNextState(FTP_R_MDTM);
            break;

          case FTP_R_MDTM:
            mState = R_mdtm();

            if (FTP_ERROR == mState)
                mInternalError = NS_ERROR_FAILURE;
            
            break;
            
// RETR        
          case FTP_S_RETR:
            rv = S_retr();
            
            if (NS_FAILED(rv)) 
                mInternalError = rv;
            
            MoveToNextState(FTP_R_RETR);
            break;
            
          case FTP_R_RETR:

            mState = R_retr();
            
            if (FTP_ERROR == mState)
                mInternalError = NS_ERROR_FAILURE;
            
            break;
            
// STOR        
          case FTP_S_STOR:
            rv = S_stor();

            if (NS_FAILED(rv))
                mInternalError = rv;
            
            MoveToNextState(FTP_R_STOR);
            break;
            
          case FTP_R_STOR:
            mState = R_stor();

            if (FTP_ERROR == mState)
                mInternalError = NS_ERROR_FAILURE;

            break;
            
// PASV        
          case FTP_S_PASV:
            rv = S_pasv();

            if (NS_FAILED(rv))
                mInternalError = NS_ERROR_FTP_PASV;
            
            MoveToNextState(FTP_R_PASV);
            break;
            
          case FTP_R_PASV:
            mState = R_pasv();

            if (FTP_ERROR == mState) 
                mInternalError = NS_ERROR_FTP_PASV;

            break;
            
// PWD        
          case FTP_S_PWD:
            rv = S_pwd();

            if (NS_FAILED(rv))
                mInternalError = NS_ERROR_FTP_PWD;
            
            MoveToNextState(FTP_R_PWD);
            break;
            
          case FTP_R_PWD:
            mState = R_pwd();

            if (FTP_ERROR == mState) 
                mInternalError = NS_ERROR_FTP_PWD;

            break;
            
          default:
            ;
            
        } 
    } 

return rv;
}

///////////////////////////////////
// STATE METHODS
///////////////////////////////////
nsresult
nsFtpState::S_user() {
    // some servers on connect send us a 421.  (84525)
    if (mResponseCode == 421)
        return NS_ERROR_FAILURE;

    nsresult rv;
    nsCAutoString usernameStr("USER ");

    if (mAnonymous) {
        usernameStr.Append("anonymous");
    } else {
        if (mUsername.IsEmpty()) {
            if (!mAuthPrompter) return NS_ERROR_NOT_INITIALIZED;
            PRUnichar *user = nsnull, *passwd = nsnull;
            PRBool retval;
            nsCAutoString prePath;
            rv = mURL->GetPrePath(prePath);
            if (NS_FAILED(rv)) return rv;
            NS_ConvertUTF8toUCS2 prePathU(prePath);

            nsCOMPtr<nsIStringBundleService> bundleService = do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
            if (NS_FAILED(rv)) return rv;

            nsCOMPtr<nsIStringBundle> bundle;
            rv = bundleService->CreateBundle(NECKO_MSGS_URL, getter_AddRefs(bundle));
            if (NS_FAILED(rv)) return rv;
            
            nsXPIDLString formatedString;
            const PRUnichar *formatStrings[1] = { prePathU.get()};
            rv = bundle->FormatStringFromName(NS_LITERAL_STRING("EnterUserPasswordFor").get(),
                                              formatStrings, 1,
                                              getter_Copies(formatedString));                   
            rv = mAuthPrompter->PromptUsernameAndPassword(nsnull,
                                                          formatedString,
                                                          prePathU.get(),
                                                          nsIAuthPrompt::SAVE_PASSWORD_PERMANENTLY,
                                                          &user, 
                                                          &passwd, 
                                                          &retval);

            // if the user canceled or didn't supply a username we want to fail
            if (!retval || (user && !*user) )
                return NS_ERROR_FAILURE;
            mUsername = user;
            mPassword = passwd;
        }
        usernameStr.AppendWithConversion(mUsername);    
    }
    usernameStr.Append(CRLF);

    return SendFTPCommand(usernameStr);
}

FTP_STATE
nsFtpState::R_user() {
    if (mResponseCode/100 == 3) {
        // send off the password
        return FTP_S_PASS;
    } else if (mResponseCode/100 == 2) {
        // no password required, we're already logged in
        return FTP_S_SYST;
    } else if (mResponseCode/100 == 5) {
        // problem logging in. typically this means the server
        // has reached it's user limit.
        return FTP_ERROR;
    } else {
        // LOGIN FAILED
        if (mAnonymous) {
            // we just tried to login anonymously and failed.
            // kick back out to S_user() and try again after
            // gathering uname/pwd info from the user.
            mAnonymous = PR_FALSE;
            return FTP_S_USER;
        } else {
            return FTP_ERROR;
        }
    }
}


nsresult
nsFtpState::S_pass() {
    nsresult rv;
    nsCAutoString passwordStr("PASS ");

    mResponseMsg = "";

    if (mAnonymous) {
        char* anonPassword = nsnull;
        PRBool useRealEmail = PR_FALSE;
        nsCOMPtr<nsIPref> pPref(do_GetService(kPrefCID, &rv));
        if (NS_SUCCEEDED(rv) && pPref) {
            rv = pPref->GetBoolPref("advanced.mailftp", &useRealEmail);
            if (NS_SUCCEEDED(rv) && useRealEmail)
                rv = pPref->CopyCharPref("network.ftp.anonymous_password", &anonPassword);
        }
        if (NS_SUCCEEDED(rv) && useRealEmail && anonPassword && *anonPassword != '\0') {
            passwordStr.Append(anonPassword);
            nsMemory::Free(anonPassword);
        }
        else {
            // We need to default to a valid email address - bug 101027
            // example.com is reserved (rfc2606), so use that
            passwordStr.Append("mozilla@example.com");
        }
    } else {
        if (mPassword.IsEmpty() || mRetryPass) {
            if (!mAuthPrompter) return NS_ERROR_NOT_INITIALIZED;

            PRUnichar *passwd = nsnull;
            PRBool retval;
            
            nsCAutoString prePath;
            rv = mURL->GetPrePath(prePath);
            if (NS_FAILED(rv)) return rv;
            NS_ConvertUTF8toUCS2 prePathU(prePath);
            
            nsCOMPtr<nsIStringBundleService> bundleService = do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
            if (NS_FAILED(rv)) return rv;

            nsCOMPtr<nsIStringBundle> bundle;
            rv = bundleService->CreateBundle(NECKO_MSGS_URL, getter_AddRefs(bundle));

            nsXPIDLString formatedString;
            const PRUnichar *formatStrings[2] = { mUsername.get(), prePathU.get() };
            rv = bundle->FormatStringFromName(NS_LITERAL_STRING("EnterPasswordFor").get(),
                                              formatStrings, 2,
                                              getter_Copies(formatedString)); 

            if (NS_FAILED(rv)) return rv;
            rv = mAuthPrompter->PromptPassword(nsnull,
                                               formatedString,
                                               prePathU.get(), 
                                               nsIAuthPrompt::SAVE_PASSWORD_PERMANENTLY,
                                               &passwd, &retval);

            // we want to fail if the user canceled or didn't enter a password.
            if (!retval || (passwd && !*passwd) )
                return NS_ERROR_FAILURE;
            mPassword = passwd;
        }
        // XXX mPassword may contain non-ASCII characters!  what do we do?
        passwordStr.AppendWithConversion(mPassword);    
    }
    passwordStr.Append(CRLF);

    return SendFTPCommand(passwordStr);
}

FTP_STATE
nsFtpState::R_pass() {
    if (mResponseCode/100 == 3) {
        // send account info
        return FTP_S_ACCT;
    } else if (mResponseCode/100 == 2) {
        // logged in
        return FTP_S_SYST;
    } else if (mResponseCode == 503) {
        // start over w/ the user command.
        // note: the password was successful, and it's stored in mPassword
        mRetryPass = PR_FALSE;
        return FTP_S_USER;
    } else if (mResponseCode/100 == 5 || mResponseCode==421) {
        // There is no difference between a too-many-users error,
        // a wrong-password error, or any other sort of error
        // So we need to tell wallet to forget the password if we had one,
        // and error out. That will then show the error message, and the
        // user can retry if they want to

        if (!mPassword.IsEmpty()) {
            nsCOMPtr<nsIPasswordManager> pm = do_GetService(NS_PASSWORDMANAGER_CONTRACTID);
            if (pm) {
                nsCAutoString prePath;
                nsresult rv = mURL->GetPrePath(prePath);
                NS_ASSERTION(NS_SUCCEEDED(rv), "Failed to get prepath");
                if (NS_SUCCEEDED(rv)) {
                    pm->RemoveUser(prePath, NS_LITERAL_STRING(""));
                }
            }
        }

        mRetryPass = PR_TRUE;
        return FTP_ERROR;
    }
    // unexpected response code
    return FTP_ERROR;
}

nsresult
nsFtpState::S_pwd() {
    nsCString pwdStr("PWD" CRLF);
    return SendFTPCommand(pwdStr);
}

FTP_STATE
nsFtpState::R_pwd() {
    if (mResponseCode/100 != 2) 
        return FTP_ERROR;
    nsCAutoString respStr(mResponseMsg);
    PRInt32 pos = respStr.FindChar('"');
    if (pos > -1) {
        respStr.Cut(0,pos+1);
        pos = respStr.FindChar('"');
        if (pos > -1) {
            respStr.Truncate(pos);
            if (respStr.Last() != '/')
                respStr.Append("/");
            mPwd = respStr;
        }
    }
    return FTP_S_TYPE;
}

nsresult
nsFtpState::S_syst() {
    nsCString systString("SYST" CRLF);
    return SendFTPCommand( systString );
}

FTP_STATE
nsFtpState::R_syst() {
    if (mResponseCode/100 == 2) {
        if (( mResponseMsg.Find("L8") > -1) || 
            ( mResponseMsg.Find("UNIX") > -1) || 
            ( mResponseMsg.Find("BSD") > -1) ||
            ( mResponseMsg.Find("MACOS Peter's Server") > -1))
        {
            mServerType = FTP_UNIX_TYPE;
        }
        else if ( ( mResponseMsg.Find("WIN32", PR_TRUE) > -1) ||
                  ( mResponseMsg.Find("windows", PR_TRUE) > -1) )
        {
            mServerType = FTP_NT_TYPE;
        }
        else if ( mResponseMsg.Find("OS/2", PR_TRUE) > -1)
        {
            mServerType = FTP_OS2_TYPE;
        }
        else
        {
            NS_ASSERTION(0, "Server type list format unrecognized.");
            // Guessing causes crashes.
            // (Of course, the parsing code should be more robust...)
            nsresult rv;
            nsCOMPtr<nsIStringBundleService> bundleService =
                do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
            if (NS_FAILED(rv)) return FTP_ERROR;

            nsCOMPtr<nsIStringBundle> bundle;
            rv = bundleService->CreateBundle(NECKO_MSGS_URL,
                                             getter_AddRefs(bundle));
            if (NS_FAILED(rv)) return FTP_ERROR;
            
            nsXPIDLString formatedString;
            PRUnichar* ucs2Response = ToNewUnicode(mResponseMsg);
            const PRUnichar *formatStrings[1] = { ucs2Response };
            rv = bundle->FormatStringFromName(NS_LITERAL_STRING("UnsupportedFTPServer").get(),
                                              formatStrings, 1,
                                              getter_Copies(formatedString));
            nsMemory::Free(ucs2Response);
            if (NS_FAILED(rv)) return FTP_ERROR;

            NS_ASSERTION(mPrompter, "no prompter!");
            if (mPrompter)
                (void) mPrompter->Alert(nsnull, formatedString.get());
            
            // since we just alerted the user, clear mResponseMsg,
            // which is displayed to the user.
            mResponseMsg = "";
            
            return FTP_ERROR;
        }
        
        return FTP_S_PWD;
    }

    if (mResponseCode/100 == 5) {   
        // server didn't like the SYST command.  Probably (500, 501, 502)
        // No clue.  We will just hope it is UNIX type server.
        mServerType = FTP_UNIX_TYPE;

        return FTP_S_PWD;
    }
    return FTP_ERROR;
}

nsresult
nsFtpState::S_acct() {
    nsCString acctString("ACCT noaccount" CRLF);
    return SendFTPCommand(acctString);
}

FTP_STATE
nsFtpState::R_acct() {
    if (mResponseCode/100 == 2)
        return FTP_S_SYST;
    else
        return FTP_ERROR;
}

nsresult
nsFtpState::S_type() {
    nsCString typeString("TYPE I" CRLF);
    return SendFTPCommand(typeString);
}

FTP_STATE
nsFtpState::R_type() {
    if (mResponseCode/100 != 2) 
        return FTP_ERROR;
    
    return FTP_S_PASV;
}

nsresult
nsFtpState::S_cwd() {
    nsCAutoString cwdStr(mPath);
    if (cwdStr.IsEmpty() || cwdStr.First() != '/')
        cwdStr.Insert(mPwd,0);
    cwdStr.Insert("CWD ",0);
    cwdStr.Append(CRLF);

    return SendFTPCommand(cwdStr);
}

FTP_STATE
nsFtpState::R_cwd() {
    if (mResponseCode/100 == 2) {
        if (mAction == PUT)
            return FTP_S_STOR;
        
        return FTP_S_LIST;
    }
    
    return FTP_ERROR;
}

nsresult
nsFtpState::S_size() {
    nsCAutoString sizeBuf(mPath);
    if (sizeBuf.IsEmpty() || sizeBuf.First() != '/')
        sizeBuf.Insert(mPwd,0);
    sizeBuf.Insert("SIZE ",0);
    sizeBuf.Append(CRLF);

    return SendFTPCommand(sizeBuf);
}

FTP_STATE
nsFtpState::R_size() {
    if (mResponseCode/100 == 2) {
        mFileSize = atoi(mResponseMsg.get()+4);
        if (NS_FAILED(mChannel->SetContentLength(mFileSize))) return FTP_ERROR;
    }

    // We may want to be able to resume this
    return FTP_S_MDTM;
}

nsresult
nsFtpState::S_mdtm() {
    nsCAutoString mdtmBuf(mPath);
    if (mdtmBuf.IsEmpty() || mdtmBuf.First() != '/')
        mdtmBuf.Insert(mPwd,0);
    mdtmBuf.Insert("MDTM ",0);
    mdtmBuf.Append(CRLF);

    return SendFTPCommand(mdtmBuf);
}

FTP_STATE
nsFtpState::R_mdtm() {
    if (mResponseCode == 213) {
        mResponseMsg.Cut(0,4);
        mResponseMsg.Trim(" \t\r\n");
        // yyyymmddhhmmss
        if (mResponseMsg.Length() != 14) {
            NS_ASSERTION(mResponseMsg.Length() == 14, "Unknown MDTM response");
        } else {
            const char* date = mResponseMsg.get();
            PRExplodedTime exp;
            exp.tm_year = (date[0]-'0')*1000 + (date[1]-'0')*100 +
                (date[2]-'0')*10 + (date[3]-'0');
            exp.tm_month = (date[4]-'0')*10 + (date[5]-'0');
            exp.tm_mday = (date[6]-'0')*10 + (date[7]-'0');
            exp.tm_hour = (date[8]-'0')*10 + (date[9]-'0');
            exp.tm_min = (date[10]-'0')*10 + (date[11]-'0');
            exp.tm_sec = (date[12]-'0')*10 + (date[13]-'0');
            exp.tm_usec = 0;
            exp.tm_wday = 0;
            exp.tm_yday = 0;
            exp.tm_params.tp_gmt_offset = 0;
            exp.tm_params.tp_dst_offset = 0;
            mModTime = PR_ImplodeTime(&exp);
        }
    }

    nsresult rv = NS_NewResumableEntityID(getter_AddRefs(mEntityID),
                                          mFileSize, mModTime);
    if (NS_FAILED(rv)) return FTP_ERROR;
    mDRequestForwarder->SetEntityID(mEntityID);

    // if we tried downloading this, lets try restarting it...
    if (mDRequestForwarder && mDRequestForwarder->GetBytesTransfered() > 0) {
        mStartPos = mDRequestForwarder->GetBytesTransfered();
        return FTP_S_REST;
    }
    
    // We weren't asked to resume
    if (mStartPos == PRUint32(-1))
        return FTP_S_RETR;

    //if (our entityID == supplied one (if any))
    PRBool entEqual = PR_FALSE;
    if (!mSuppliedEntityID ||
        (NS_SUCCEEDED(mEntityID->Equals(mSuppliedEntityID, &entEqual)) &&
         entEqual)) {
        return FTP_S_REST;
    } else {
        mInternalError = NS_ERROR_NOT_RESUMABLE;
        mResponseMsg.Truncate();
        return FTP_ERROR;
    }
}

nsresult 
nsFtpState::SetContentType()
{
    nsCOMPtr<nsIDirectoryListing> list = do_QueryInterface(mChannel);
    NS_ASSERTION(list, "ftp channel isn't listable!");
    (void)list->GetListFormat(&mListFormat);
    nsCAutoString contentType;
    switch (mListFormat) {
    case nsIDirectoryListing::FORMAT_RAW:
        {
            nsAutoString fromStr(NS_LITERAL_STRING("text/ftp-dir-"));
            SetDirMIMEType(fromStr);

            contentType = NS_LITERAL_CSTRING("text/ftp-dir-");
        }
        break;
    default:
        NS_WARNING("Unknown directory type");
        // fall through
    case nsIDirectoryListing::FORMAT_HTML:
        contentType = NS_LITERAL_CSTRING(TEXT_HTML);
        break;
    case nsIDirectoryListing::FORMAT_HTTP_INDEX:
        contentType = NS_LITERAL_CSTRING(APPLICATION_HTTP_INDEX_FORMAT);
        break;
    }

    return mChannel->SetContentType(contentType);
}

nsresult
nsFtpState::S_list() {
    nsresult rv;

    if (!mDRequestForwarder) 
        return NS_ERROR_FAILURE;

    rv = SetContentType();
    
    if (NS_FAILED(rv)) 
        return FTP_ERROR;
    
    // save off the server type if we are caching.
    if(mCacheEntry) {
        nsCAutoString serverType;
        serverType.AppendInt(mServerType);
        (void) mCacheEntry->SetMetaDataElement("servertype", serverType.get());
    }

    nsCOMPtr<nsIStreamListener> converter;
    
    rv = BuildStreamConverter(getter_AddRefs(converter));
    if (NS_FAILED(rv)) {
        // clear mResponseMsg which is displayed to the user.
        // TODO: we should probably set this to something 
        // meaningful.
        mResponseMsg = "";
        return rv;
    }
    
    // the data forwarder defaults to sending notifications
    // to the channel.  Lets hijack and send the notifications
    // to the stream converter.
    mDRequestForwarder->SetStreamListener(converter);
    mDRequestForwarder->SetCacheEntry(mCacheEntry, PR_TRUE);
    // dir listings aren't resumable
    NS_ASSERTION(!mSuppliedEntityID,
                 "Entity ID given to directory request");
    NS_ASSERTION(mStartPos == PRUint32(-1) || mStartPos == 0,
                 "Non-intial start position given to directory request");
    if (mSuppliedEntityID || (mStartPos != PRUint32(-1) && mStartPos != 0)) {
        // If we reach this code, then the caller is in error
        return NS_ERROR_NOT_RESUMABLE;
    }

    mDRequestForwarder->SetEntityID(nsnull);

    nsCAutoString listString("LIST" CRLF);

    return SendFTPCommand(listString);
}

FTP_STATE
nsFtpState::R_list() {
    if (mResponseCode/100 == 1) {
        nsresult rv = mDPipeRequest->Resume();
        if (NS_FAILED(rv)) {
            PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) dataPipe->Resume (rv=%x)\n", this, rv));
            return FTP_ERROR;
        }
        return FTP_READ_BUF;
    }

    if (mResponseCode/100 == 2) {
        //(DONE)
        mNextState = FTP_COMPLETE;
        return FTP_COMPLETE;
    }

    mFireCallbacks = PR_TRUE;
    return FTP_ERROR;
}

nsresult
nsFtpState::S_retr() {
    nsresult rv = NS_OK;
    nsCAutoString retrStr(mPath);
    if (retrStr.IsEmpty() || retrStr.First() != '/')
        retrStr.Insert(mPwd,0);
    retrStr.Insert("RETR ",0);
    retrStr.Append(CRLF);
    
    if (!mDRequestForwarder)
        return NS_ERROR_FAILURE;
    
    rv = SendFTPCommand(retrStr);
    return rv;
}

FTP_STATE
nsFtpState::R_retr() {
    if (mResponseCode/100 == 2) {
        //(DONE)
        mNextState = FTP_COMPLETE;
        return FTP_COMPLETE;
    }

    if (mResponseCode/100 == 1) {
        // We're going to grab a file, not a directory. So we need to clear
        // any cache entry, otherwise we'll have problems reading it later.
        // See bug 122548
        if (mCacheEntry) {
            (void)mCacheEntry->Doom();
            mCacheEntry = nsnull;
        }
        nsresult rv = mDPipeRequest->Resume();
        if (NS_FAILED(rv)) {
            PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) dataPipe->Resume (rv=%x)\n", this, rv));
            return FTP_ERROR;
        }
        return FTP_READ_BUF;
    }
    
    // These error codes are related to problems with the connection.  
    // If we encounter any at this point, do not try CWD and abort.
    if (mResponseCode == 421 || mResponseCode == 425 || mResponseCode == 426)
        return FTP_ERROR;

    if (mResponseCode/100 == 5) {
        mRETRFailed = PR_TRUE;
        mDRequestForwarder->SetRetrying(PR_TRUE);
        return FTP_S_PASV;
    }

    return FTP_S_CWD;
}


nsresult
nsFtpState::S_rest() {
    
    nsCAutoString restString("REST ");
    restString.AppendInt(mStartPos, 10);
    restString.Append(CRLF);

    return SendFTPCommand(restString);
}

FTP_STATE
nsFtpState::R_rest() {
    if (mResponseCode/100 == 4) {
        // If REST fails, then we can't resume
        mEntityID = nsnull;

        mInternalError = NS_ERROR_NOT_RESUMABLE;
        mResponseMsg.Truncate();

        return FTP_ERROR;
    }
   
    return FTP_S_RETR; 
}

nsresult
nsFtpState::S_stor() {
    NS_ASSERTION(mWriteStream, "we're trying to upload without any data");

    if (!mWriteStream)
        return NS_ERROR_FAILURE;

    NS_ASSERTION(mAction == PUT, "Wrong state to be here");
    
    nsCAutoString storStr;
    nsresult rv;
    nsCOMPtr<nsIURL> aURL(do_QueryInterface(mURL, &rv));
    if (NS_FAILED(rv)) return rv;

    rv = aURL->GetFilePath(storStr);
    if (NS_FAILED(rv)) return rv;
    NS_ASSERTION(!storStr.IsEmpty(), "What does it mean to store a empty path");
        
    if (storStr.First() == '/')
    {
        // kill the first slash since we want to be relative to CWD.
        storStr.Cut(0,1);
    }
    NS_UnescapeURL(storStr);
    storStr.Insert("STOR ",0);
    storStr.Append(CRLF);

    return SendFTPCommand(storStr);
}

FTP_STATE
nsFtpState::R_stor() {
    if (mResponseCode/100 == 2) {
        //(DONE)
        mNextState = FTP_COMPLETE;
        return FTP_COMPLETE;
    }

    if (mResponseCode/100 == 1) {
        PRUint32 len;
        mWriteStream->Available(&len);
        PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) writing on Data Transport\n", this));
        
        // Close the read pipe since we are going to be writing data.
        if (mDPipeRequest)
            mDPipeRequest->Cancel(NS_SUCCESS_IGNORE_NOTIFICATION);
    
        nsresult rv = NS_AsyncWriteFromStream(getter_AddRefs(mDPipeRequest), 
                                              mDPipe, 
                                              mWriteStream, 
                                              0, 
                                              len,
                                              0, 
                                              mDRequestForwarder);
        if (NS_FAILED(rv)) return FTP_ERROR;
        return FTP_READ_BUF;
    }

   return FTP_ERROR;
}


nsresult
nsFtpState::S_pasv() {
    nsresult rv;

    if (mIPv6Checked == PR_FALSE) {
        // Find IPv6 socket address, if server is IPv6
        mIPv6Checked = PR_TRUE;
        PR_ASSERT(mIPv6ServerAddress == 0);
        nsCOMPtr<nsITransport> controlSocket;
        mControlConnection->GetTransport(getter_AddRefs(controlSocket));
        if (!controlSocket) return FTP_ERROR;
        nsCOMPtr<nsISocketTransport> sTrans = do_QueryInterface(controlSocket, &rv);
        
        if (sTrans) {
            rv = sTrans->GetIPStr(100, &mIPv6ServerAddress);
        }

        if (NS_SUCCEEDED(rv)) {
            PRNetAddr addr;
            if (PR_StringToNetAddr(mIPv6ServerAddress, &addr) != PR_SUCCESS ||
                PR_IsNetAddrType(&addr, PR_IpAddrV4Mapped)) {
                nsMemory::Free(mIPv6ServerAddress);
                mIPv6ServerAddress = 0;
            }
            PR_ASSERT(!mIPv6ServerAddress || addr.raw.family == PR_AF_INET6);
        }
    }


    const char * string;
    if (mIPv6ServerAddress)
        string = "EPSV" CRLF;
    else
        string = "PASV" CRLF;

    nsCString pasvString(string);
    return SendFTPCommand(pasvString);
    
}

FTP_STATE
nsFtpState::R_pasv() {
    nsresult rv;
    PRInt32 port;

    if (mResponseCode/100 != 2)  {
        return FTP_ERROR;
    }

    char *response = ToNewCString(mResponseMsg);
    if (!response) return FTP_ERROR;
    char *ptr = response;

    nsCAutoString host;
    if (mIPv6ServerAddress) {
        // The returned string is of the form
        // text (|||ppp|)
        // Where '|' can be any single character
        char delim;
        while (*ptr && *ptr != '(') ptr++;
        if (*ptr++ != '(') {
            return FTP_ERROR;
        }
        delim = *ptr++;
        if (!delim || *ptr++ != delim || *ptr++ != delim || 
            *ptr < '0' || *ptr > '9') {
            return FTP_ERROR;
        }
        port = 0;
        do {
            port = port * 10 + *ptr++ - '0';
        } while (*ptr >= '0' && *ptr <= '9');
        if (*ptr++ != delim || *ptr != ')') {
            return FTP_ERROR;
        }
    } else {
        // The returned address string can be of the form
        // (xxx,xxx,xxx,xxx,ppp,ppp) or
        //  xxx,xxx,xxx,xxx,ppp,ppp (without parens)
        PRInt32 h0, h1, h2, h3, p0, p1;

        PRUint32 fields = 0;
        // First try with parens
        while (*ptr && *ptr != '(')
            ++ptr;
        if (*ptr) {
            ++ptr;
            fields = PR_sscanf(ptr, 
                               "%ld,%ld,%ld,%ld,%ld,%ld",
                               &h0, &h1, &h2, &h3, &p0, &p1);
        }
        if (!*ptr || fields < 6) {
            // OK, lets try w/o parens
            ptr = response;
            while (*ptr && *ptr != ',')
                ++ptr;
            if (*ptr) {
                // backup to the start of the digits
                do {
                    ptr--;
                } while ((ptr >=response) && (*ptr >= '0') && (*ptr <= '9'));
                ptr++; // get back onto the numbers
                fields = PR_sscanf(ptr, 
                                   "%ld,%ld,%ld,%ld,%ld,%ld",
                                   &h0, &h1, &h2, &h3, &p0, &p1);
            }
        }

        NS_ASSERTION(fields == 6, "Can't parse PASV response");
        if (fields < 6) {
            return FTP_ERROR;
        }

        port = ((PRInt32) (p0<<8)) + p1;
        host.AppendInt(h0);
        host.Append('.');
        host.AppendInt(h1);
        host.Append('.');
        host.AppendInt(h2);
        host.Append('.');
        host.AppendInt(h3);
    }

    nsMemory::Free(response);

    const char* hostStr = mIPv6ServerAddress ? mIPv6ServerAddress : host.get();

    PRBool newDataConn = PR_TRUE;
    if (mDPipeRequest) {
        // Reuse this connection only if its still alive, and the port
        // is the same

        nsCOMPtr<nsISocketTransport> st = do_QueryInterface(mDPipe);

        if (st) {
            PRInt32 oldPort;
            nsresult rv = st->GetPort(&oldPort);
            if (NS_SUCCEEDED(rv)) {
                if (oldPort == port) {
                    PRBool isAlive;
                    if (NS_SUCCEEDED(st->IsAlive(0, &isAlive)) && isAlive) {
                        newDataConn = PR_FALSE;
                    }
                }
            }
        }

        if (newDataConn) {
            if (st)
                st->SetReuseConnection(PR_FALSE);
            mDPipe = 0;
            mDPipeRequest->Cancel(NS_OK);
            mDPipeRequest = 0;
        } else {
            mDRequestForwarder->SetRetrying(PR_FALSE);
        }
    }

    if (newDataConn) {
        // now we know where to connect our data channel
        nsCOMPtr<nsISocketTransportService> sts = do_GetService(kSocketTransportServiceCID, &rv);
        
        rv =  sts->CreateTransport(hostStr, 
                                   port, 
                                   mProxyInfo,
                                   FTP_DATA_CHANNEL_SEG_SIZE, 
                                   FTP_DATA_CHANNEL_MAX_SIZE, 
                                   getter_AddRefs(mDPipe)); // the data channel
        if (NS_FAILED(rv)) return FTP_ERROR;
        
        PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) Created Data Transport (%s:%x)\n", this, hostStr, port));
        
        nsCOMPtr<nsISocketTransport> sTrans = do_QueryInterface(mDPipe, &rv);
        if (NS_FAILED(rv)) return FTP_ERROR;
        
        if (NS_FAILED(sTrans->SetReuseConnection(PR_TRUE))) return FTP_ERROR;
        
        if (!mDRequestForwarder) {
            mDRequestForwarder = new DataRequestForwarder;
            if (!mDRequestForwarder) return FTP_ERROR;
            NS_ADDREF(mDRequestForwarder);
            
            rv = mDRequestForwarder->Init(mChannel);
            if (NS_FAILED(rv)){
                PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) forwarder->Init failed (rv=%x)\n", this, rv));
                return FTP_ERROR;
            }
        }
        
        // hook ourself up as a proxy for progress notifications
        mWaitingForDConn = PR_TRUE;
        rv = mDPipe->SetNotificationCallbacks(NS_STATIC_CAST(nsIInterfaceRequestor*, mDRequestForwarder), PR_FALSE);
        if (NS_FAILED(rv)){
            PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) forwarder->SetNotificationCallbacks failed (rv=%x)\n", this, rv));
            return FTP_ERROR;
        }
        
        // we need to get mDPipe going so tcp connection is made
        rv = mDPipe->AsyncRead(mDRequestForwarder, nsnull, 0, PRUint32(-1), 0, getter_AddRefs(mDPipeRequest));    
        if (NS_FAILED(rv)){
            PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) forwarder->AsyncRead failed (rv=%x)\n", this, rv));
            return FTP_ERROR;
        }
        
        if (mAction == PUT) {
            NS_ASSERTION(!mRETRFailed, "Failed before uploading");
            mDRequestForwarder->Uploading(PR_TRUE);
            return FTP_S_STOR;
        }

        // Suspend the read
        // If we don't do this, then the remote server could close the
        // connection before we get the error message, and then we process the
        // onstop as if it was from the real data connection
        rv = mDPipeRequest->Suspend();
        if (NS_FAILED(rv)){
            PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) dataPipe->Suspend failed (rv=%x)\n", this, rv));
            return FTP_ERROR;
        }
    }

    if (mRETRFailed)
        return FTP_S_CWD;
    return FTP_S_SIZE;
}


////////////////////////////////////////////////////////////////////////////////
// nsIRequest methods:

NS_IMETHODIMP
nsFtpState::GetName(nsACString &result)
{
    return mURL->GetSpec(result);
}

NS_IMETHODIMP
nsFtpState::IsPending(PRBool *result)
{
    nsresult rv = NS_OK;
    *result = PR_FALSE;
    
    nsCOMPtr<nsIRequest> request;
    mControlConnection->GetReadRequest(getter_AddRefs(request));

    if (request) {
        rv = request->IsPending(result);
        if (NS_FAILED(rv)) return rv;
    }
    return rv;
}

NS_IMETHODIMP
nsFtpState::GetStatus(nsresult *status)
{
    *status = mInternalError;
    return NS_OK;
}

NS_IMETHODIMP
nsFtpState::Cancel(nsresult status)
{
    NS_ASSERTION(NS_FAILED(status), "shouldn't cancel with a success code");
    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) nsFtpState::Cancel() rv=%x\n", this, status));

    // we should try to recover the control connection....
    if (NS_SUCCEEDED(mControlStatus))
        mControlStatus = status;

    // kill the data connection immediately. 
    NS_IF_RELEASE(mDRequestForwarder);
    if (mDPipeRequest) {
        mDPipeRequest->Cancel(status);
        mDPipeRequest = 0;
    }

    (void) StopProcessing();   
    return NS_OK;
}

NS_IMETHODIMP
nsFtpState::Suspend(void)
{
    nsresult rv = NS_OK;
    
    if (!mControlConnection)
        return NS_ERROR_FAILURE;

    // suspending the underlying socket transport will
    // cause the FTP state machine to "suspend" when it
    // tries to use the transport. May not be granular 
    // enough.
    if (mSuspendCount < 1) {
        mSuspendCount++;

        // only worry about the read request.
        nsCOMPtr<nsIRequest> request;
        mControlConnection->GetReadRequest(getter_AddRefs(request));

        if (request) {
            rv = request->Suspend();
            if (NS_FAILED(rv)) return rv;
        }

        if (mDPipeRequest) {
            rv = mDPipeRequest->Suspend();
            if (NS_FAILED(rv)) return rv;
        }
    }

    return rv;
}

NS_IMETHODIMP
nsFtpState::Resume(void)
{
    nsresult rv = NS_ERROR_FAILURE;
    
    // resuming the underlying socket transports will
    // cause the FTP state machine to unblock and 
    // go on about it's business.
    if (mSuspendCount) {

        PRBool dataAlive = PR_FALSE;

        nsCOMPtr<nsISocketTransport> dataSocket = do_QueryInterface(mDPipe);
        if (dataSocket) 
            dataSocket->IsAlive(0, &dataAlive);            
            
        if (dataSocket && dataAlive && mControlConnection->IsAlive())
        {
            nsCOMPtr<nsIRequest> controlRequest;
            mControlConnection->GetReadRequest(getter_AddRefs(controlRequest));
            NS_ASSERTION(controlRequest, "where did my request go!");
            
            controlRequest->Resume();
            rv = mDPipeRequest->Resume();
        }
        else
        {
            // control or data connection went down.  need to reconnect.  
            // if we were downloading, we need to perform REST.
            rv = EstablishControlConnection();
        }
    }
    mSuspendCount--;
    return rv;
}

NS_IMETHODIMP
nsFtpState::GetLoadGroup(nsILoadGroup **aLoadGroup)
{
    *aLoadGroup = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsFtpState::SetLoadGroup(nsILoadGroup *aLoadGroup)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFtpState::GetLoadFlags(nsLoadFlags *aLoadFlags)
{
    *aLoadFlags = nsIRequest::LOAD_NORMAL;
    return NS_OK;
}

NS_IMETHODIMP
nsFtpState::SetLoadFlags(nsLoadFlags aLoadFlags)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

// This really really needs to be somewhere else
static inline PRUint32
PRTimeToSeconds(PRTime t_usec)
{
    PRTime usec_per_sec;
    PRUint32 t_sec;
    LL_I2L(usec_per_sec, PR_USEC_PER_SEC);
    LL_DIV(t_usec, t_usec, usec_per_sec);
    LL_L2I(t_sec, t_usec);
    return t_sec;
}

#define NowInSeconds() PRTimeToSeconds(PR_Now())

PRUint32 nsFtpState::mSessionStartTime = NowInSeconds();

/* Is this cache entry valid to use for reading?
 * Since we make up an expiration time for ftp, use the following rules:
 * (see bug 103726)
 *
 * LOAD_FROM_CACHE                    : always use cache entry, even if expired
 * LOAD_BYPASS_CACHE                  : overwrite cache entry
 * LOAD_NORMAL|VALIDATE_ALWAYS        : overwrite cache entry
 * LOAD_NORMAL                        : honor expiration time
 * LOAD_NORMAL|VALIDATE_ONCE_PER_SESSION : overwrite cache entry if first access 
 *                                         this session, otherwise use cache entry 
 *                                         even if expired.
 * LOAD_NORMAL|VALIDATE_NEVER         : always use cache entry, even if expired
 *
 * Note that in theory we could use the mdtm time on the directory
 * In practice, the lack of a timezone plus the general lack of support for that
 * on directories means that its not worth it, I suspect. Revisit if we start
 * caching files - bbaetz
 */
PRBool
nsFtpState::CanReadEntry()
{
    nsCacheAccessMode access;
    nsresult rv = mCacheEntry->GetAccessGranted(&access);
    if (NS_FAILED(rv)) return PR_FALSE;
    
    // If I'm not granted read access, then I can't reuse it...
    if (!(access & nsICache::ACCESS_READ))
        return PR_FALSE;

    nsLoadFlags flags;
    rv = mChannel->GetLoadFlags(&flags);
    if (NS_FAILED(rv)) return PR_FALSE;

    if (flags & LOAD_FROM_CACHE)
        return PR_TRUE;

    if (flags & LOAD_BYPASS_CACHE)
        return PR_FALSE;
    
    if (flags & VALIDATE_ALWAYS)
        return PR_FALSE;
    
    PRUint32 time;
    if (flags & VALIDATE_ONCE_PER_SESSION) {
        rv = mCacheEntry->GetLastModified(&time);
        if (NS_FAILED(rv)) return PR_FALSE;
        return (mSessionStartTime > time);
    }

    if (flags & VALIDATE_NEVER)
        return PR_TRUE;

    // OK, now we just check the expiration time as usual
    rv = mCacheEntry->GetExpirationTime(&time);
    if (NS_FAILED(rv)) return rv;
    return (NowInSeconds() <= time);
}

nsresult
nsFtpState::Init(nsIFTPChannel* aChannel,
                 nsIPrompt*  aPrompter,
                 nsIAuthPrompt* aAuthPrompter,
                 nsIFTPEventSink* sink,
                 nsICacheEntryDescriptor* cacheEntry,
                 nsIProxyInfo* proxyInfo,
                 PRUint32 startPos,
                 nsIResumableEntityID* entity) 
{
    nsresult rv = NS_OK;

    mKeepRunning = PR_TRUE;
    mPrompter = aPrompter;
    if (!mPrompter) {
        nsCOMPtr<nsILoadGroup> ldGrp;
        aChannel->GetLoadGroup(getter_AddRefs(ldGrp));
        if (ldGrp) {
            nsCOMPtr<nsIInterfaceRequestor> cbs;
            rv = ldGrp->GetNotificationCallbacks(getter_AddRefs(cbs));
            if (NS_SUCCEEDED(rv))
                mPrompter = do_GetInterface(cbs);
        }
    }
    mFTPEventSink = sink;
    mAuthPrompter = aAuthPrompter;
    mCacheEntry = cacheEntry;
    mProxyInfo = proxyInfo;
    mStartPos = startPos;
    mSuppliedEntityID = entity;
    
    // parameter validation
    NS_ASSERTION(aChannel, "FTP: needs a channel");

    mChannel = aChannel; // a straight com ptr to the channel

    rv = aChannel->GetURI(getter_AddRefs(mURL));
    if (NS_FAILED(rv)) return rv;
        
    if (mCacheEntry) {
        if (CanReadEntry()) {
            // XXX - all this code assumes that we only cache directories
            // If we start caching files, this needs to be updated

            // make sure the channel knows wassup
            SetContentType();
            
            NS_ASSERTION(!mDRequestForwarder, "there should not be a data forwarder");
            mDRequestForwarder = new DataRequestForwarder;
            if (!mDRequestForwarder) return NS_ERROR_OUT_OF_MEMORY;
            NS_ADDREF(mDRequestForwarder);
    
            rv = mDRequestForwarder->Init(mChannel);
            
            nsXPIDLCString serverType;
            (void) mCacheEntry->GetMetaDataElement("servertype", getter_Copies(serverType));
            nsCAutoString serverNum(serverType.get());
            PRInt32 err;
            mServerType = serverNum.ToInteger(&err);
            
            nsCOMPtr<nsIStreamListener> converter;
            rv = BuildStreamConverter(getter_AddRefs(converter));
            if (NS_FAILED(rv)) return rv;
            
            mDRequestForwarder->SetStreamListener(converter);
            mDRequestForwarder->SetCacheEntry(mCacheEntry, PR_FALSE);
            mDRequestForwarder->SetEntityID(nsnull);

            // Get a transport to the cached data...
            nsCOMPtr<nsITransport> transport;
            rv = mCacheEntry->GetTransport(getter_AddRefs(transport));
            if (NS_FAILED(rv)) return rv;

            // Pump the cache data downstream
            return transport->AsyncRead(mDRequestForwarder, 
                                        nsnull,
                                        0, PRUint32(-1), 0,
                                        getter_AddRefs(mDPipeRequest));
        }
    }

    nsCAutoString path;
    nsCOMPtr<nsIURL> aURL(do_QueryInterface(mURL));
    if (aURL)
        rv = aURL->GetFilePath(path);
    else
        rv = mURL->GetPath(path);
    if (NS_FAILED(rv)) return rv;

    // Skip leading slash
    char* fwdPtr = (char *)path.get();
    if (fwdPtr && (*fwdPtr == '/'))
        fwdPtr++;
    if (*fwdPtr != '\0') {
        // now unescape it... %xx reduced inline to resulting character
        nsUnescape(fwdPtr);
        mPath.Assign(fwdPtr);

        // return an error if we find a CR or LF in the path
        if (mPath.FindCharInSet(CRLF) >= 0)
            return NS_ERROR_MALFORMED_URI;
    }

    // pull any username and/or password out of the uri
    nsCAutoString uname;
    rv = mURL->GetUsername(uname);
    if (NS_FAILED(rv)) {
        return rv;
    } else {
        if (!uname.IsEmpty()) {
            mAnonymous = PR_FALSE;
            mUsername = NS_ConvertUTF8toUCS2(NS_UnescapeURL(uname));

            // return an error if we find a CR or LF in the username
            if (uname.FindCharInSet(CRLF) >= 0)
                return NS_ERROR_MALFORMED_URI;
        }
    }

    nsCAutoString password;
    rv = mURL->GetPassword(password);
    if (NS_FAILED(rv))
        return rv;

    mPassword = NS_ConvertUTF8toUCS2(NS_UnescapeURL(password));

    // return an error if we find a CR or LF in the password
    if (mPassword.FindCharInSet(CRLF) >= 0)
        return NS_ERROR_MALFORMED_URI;

    // setup the connection cache key

    PRInt32 port;
    rv = mURL->GetPort(&port);
    if (NS_FAILED(rv)) return rv;

    if (port > 0)
        mPort = port;

    return NS_OK;
}

nsresult
nsFtpState::Connect()
{
    if (mDRequestForwarder)
        return NS_OK;  // we are already connected.
    
    nsresult rv;

    mState = FTP_COMMAND_CONNECT;
    mNextState = FTP_S_USER;

    rv = Process();

    // check for errors.
    if (NS_FAILED(rv)) {
        PR_LOG(gFTPLog, PR_LOG_DEBUG, ("-- Connect() on Control Connect FAILED with rv = %x\n", rv));
        mInternalError = NS_ERROR_FAILURE;
        mState = FTP_ERROR;
    }

    return rv;
}
nsresult
nsFtpState::SetWriteStream(nsIInputStream* aInStream) {
    if (!aInStream)
        return NS_OK;

    mAction = PUT;
    mWriteStream = aInStream;
    return NS_OK;
}

void
nsFtpState::KillControlConnection() {
    mControlReadCarryOverBuf.Truncate(0);

    if (mDPipe) {
        mDPipe->SetNotificationCallbacks(nsnull, PR_FALSE);
        mDPipe = 0;
    }
    
    NS_IF_RELEASE(mDRequestForwarder);

    mIPv6Checked = PR_FALSE;
    if (mIPv6ServerAddress) {
        nsMemory::Free(mIPv6ServerAddress);
        mIPv6ServerAddress = 0;
    }
    // if everything went okay, save the connection. 
    // FIX: need a better way to determine if we can cache the connections.
    //      there are some errors which do not mean that we need to kill the connection
    //      e.g. fnf.

    if (!mControlConnection)
        return;

    // kill the reference to ourselves in the control connection.
    (void) mControlConnection->SetStreamListener(nsnull);

    if (FTP_CACHE_CONTROL_CONNECTION && 
        NS_SUCCEEDED(mInternalError) &&
        NS_SUCCEEDED(mControlStatus) &&
        mControlConnection->IsAlive()) {

        PR_LOG(gFTPLog, PR_LOG_ALWAYS, ("(%x) nsFtpState caching control connection", this));

        // Store connection persistant data
        mControlConnection->mServerType = mServerType;           
        mControlConnection->mPassword = mPassword;
        mControlConnection->mPwd = mPwd;
        nsresult rv = nsFtpProtocolHandler::InsertConnection(mURL, 
                                           NS_STATIC_CAST(nsISupports*, (nsIStreamListener*)mControlConnection));
        // Can't cache it?  Kill it then.  
        mControlConnection->Disconnect(rv);
    } 
    else
        mControlConnection->Disconnect(NS_BINDING_ABORTED);

    NS_RELEASE(mControlConnection);
 
    return;
}

nsresult
nsFtpState::StopProcessing() {
    PR_LOG(gFTPLog, PR_LOG_ALWAYS, ("(%x) nsFtpState stopping", this));

#ifdef DEBUG_dougt
    printf("FTP Stopped: [response code %d] [response msg follows:]\n%s\n", mResponseCode, mResponseMsg.get());
#endif

    if ( NS_FAILED(mInternalError) && !mResponseMsg.IsEmpty()) 
    {
        // check to see if the control status is bad.
        // web shell wont throw an alert.  we better:
        
        NS_ASSERTION(mPrompter, "no prompter!");
        if (mPrompter)
            (void) mPrompter->Alert(nsnull, NS_ConvertASCIItoUCS2(mResponseMsg).get());
    }
    
    nsresult broadcastErrorCode = mControlStatus;
    if ( NS_SUCCEEDED(broadcastErrorCode))
        broadcastErrorCode = mInternalError;

    if (mFireCallbacks && mChannel) {
        nsCOMPtr<nsIStreamListener> channelListener = do_QueryInterface(mChannel);
        NS_ASSERTION(channelListener, "ftp channel should be a stream listener");
        nsCOMPtr<nsIStreamListener> asyncListener;
        NS_NewAsyncStreamListener(getter_AddRefs(asyncListener), channelListener, NS_UI_THREAD_EVENTQ);
        if(asyncListener) {
            (void) asyncListener->OnStartRequest(this, nsnull);
            (void) asyncListener->OnStopRequest(this, nsnull, broadcastErrorCode);
        }
    }

    // Clean up the event loop
    mKeepRunning = PR_FALSE;

    KillControlConnection();

    nsCOMPtr<nsIProgressEventSink> sink(do_QueryInterface(mChannel));
    if (sink)
        sink->OnStatus(nsnull, nsnull, NS_NET_STATUS_END_FTP_TRANSACTION, nsnull);

    // Release the Observers
    mWriteStream = 0;  // should this call close before setting to null?

    mPrompter = 0;
    mAuthPrompter = 0;
    mChannel = 0;
    mProxyInfo = 0;

    return NS_OK;
}

void
nsFtpState::SetDirMIMEType(nsString& aString) {
    // the from content type is a string of the form
    // "text/ftp-dir-SERVER_TYPE" where SERVER_TYPE represents the server we're talking to.
    switch (mServerType) {
    case FTP_UNIX_TYPE:
        aString.Append(NS_LITERAL_STRING("unix"));
        break;
    case FTP_NT_TYPE:
        aString.Append(NS_LITERAL_STRING("nt"));
        break;
    case FTP_OS2_TYPE:
        aString.Append(NS_LITERAL_STRING("os2"));
        break;
    default:
        aString.Append(NS_LITERAL_STRING("generic"));
    }
}

nsresult
nsFtpState::BuildStreamConverter(nsIStreamListener** convertStreamListener)
{
    nsresult rv;
    // setup a listener to push the data into. This listener sits inbetween the
    // unconverted data of fromType, and the final listener in the chain (in this case
    // the mListener).
    nsCOMPtr<nsIStreamListener> converterListener;
    nsCOMPtr<nsIStreamListener> listener = do_QueryInterface(mChannel);

    nsCOMPtr<nsIStreamConverterService> scs = 
             do_GetService(kStreamConverterServiceCID, &rv);

    if (NS_FAILED(rv)) 
        return rv;

    nsAutoString fromStr(NS_LITERAL_STRING("text/ftp-dir-"));
    SetDirMIMEType(fromStr);
    switch (mListFormat) {
    case nsIDirectoryListing::FORMAT_RAW:
        converterListener = listener;
        break;
    default:
        // fall through
    case nsIDirectoryListing::FORMAT_HTML:
        // XXX - work arround bug 126417. We have to do the chaining
        // manually so that we don't crash
        {
            nsCOMPtr<nsIStreamListener> tmpListener;
            rv = scs->AsyncConvertData(NS_LITERAL_STRING(APPLICATION_HTTP_INDEX_FORMAT).get(),
                                       NS_LITERAL_STRING(TEXT_HTML).get(),
                                       listener, 
                                       mURL, 
                                       getter_AddRefs(tmpListener));
            if (NS_FAILED(rv)) break;
            rv = scs->AsyncConvertData(fromStr.get(),
                                       NS_LITERAL_STRING(APPLICATION_HTTP_INDEX_FORMAT).get(),
                                       tmpListener,
                                       mURL,
                                       getter_AddRefs(converterListener));
                                       
        }
        break;
    case nsIDirectoryListing::FORMAT_HTTP_INDEX:
        rv = scs->AsyncConvertData(fromStr.get(), 
                                   NS_LITERAL_STRING(APPLICATION_HTTP_INDEX_FORMAT).get(),
                                   listener, 
                                   mURL, 
                                   getter_AddRefs(converterListener));
        break;
    }

    if (NS_FAILED(rv)) {
        PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) scs->AsyncConvertData failed (rv=%x)\n", this, rv));
        return rv;
    }

    NS_ADDREF(*convertStreamListener = converterListener);
    return rv;
}

nsresult
nsFtpState::DataConnectionEstablished()
{
    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) Data Connection established.", this));
    
    mFireCallbacks  = PR_FALSE; // observer callbacks will be handled by the transport.
    mWaitingForDConn= PR_FALSE;

    // sending empty string with (mWaitingForDConn == PR_FALSE) will cause the 
    // control socket to write out its buffer.
    nsCString a("");
    SendFTPCommand(a);
    
    return NS_OK;
}

nsresult 
nsFtpState::SendFTPCommand(nsCString& command)
{
    NS_ASSERTION(mControlConnection, "null control connection");        
    
    // we don't want to log the password:
    nsCAutoString logcmd(command);
    if (Substring(command, 0, 5).Equals(NS_LITERAL_CSTRING("PASS "))) 
        logcmd = "PASS xxxxx";
    
    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x)(dwait=%d) Writing \"%s\"\n", this, mWaitingForDConn, logcmd.get()));
    if (mFTPEventSink)
        mFTPEventSink->OnFTPControlLog(PR_FALSE, logcmd.get());
    
    if (mControlConnection) {
        return mControlConnection->Write(command, mWaitingForDConn);
    }
    return NS_ERROR_FAILURE;
}

