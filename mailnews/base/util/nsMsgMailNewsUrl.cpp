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
 * Portions created by the Initial Developer are Copyright (C) 1999
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

#include "msgCore.h"
#include "nsMsgMailNewsUrl.h"
#include "nsMsgBaseCID.h"
#include "nsIMsgMailSession.h"
#include "nsIMsgAccountManager.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsIDocumentLoader.h"
#include "nsILoadGroup.h"
#include "nsIWebShell.h"
#include "nsIDocShell.h"
#include "nsIWebProgress.h"
#include "nsIWebProgressListener.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIIOService.h"
#include "nsNetCID.h"
#include "nsEscape.h"

static NS_DEFINE_CID(kUrlListenerManagerCID, NS_URLLISTENERMANAGER_CID);
static NS_DEFINE_CID(kStandardUrlCID, NS_STANDARDURL_CID);
static NS_DEFINE_CID(kMsgMailSessionCID, NS_MSGMAILSESSION_CID);
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);

nsMsgMailNewsUrl::nsMsgMailNewsUrl()
{
    NS_INIT_REFCNT();
 
	// nsIURI specific state
	m_errorMessage = nsnull;
	m_runningUrl = PR_FALSE;
	m_updatingFolder = PR_FALSE;
  m_addContentToCache = PR_FALSE;
  m_msgIsInLocalCache = PR_FALSE;
  m_suppressErrorMsgs = PR_FALSE;

	nsComponentManager::CreateInstance(kUrlListenerManagerCID, nsnull, NS_GET_IID(nsIUrlListenerManager), (void **) getter_AddRefs(m_urlListeners));
	nsComponentManager::CreateInstance(kStandardUrlCID, nsnull, NS_GET_IID(nsIURL), (void **) getter_AddRefs(m_baseURL));
}
 
nsMsgMailNewsUrl::~nsMsgMailNewsUrl()
{
	PR_FREEIF(m_errorMessage);
}
  
NS_IMPL_THREADSAFE_ADDREF(nsMsgMailNewsUrl);
NS_IMPL_THREADSAFE_RELEASE(nsMsgMailNewsUrl);

NS_INTERFACE_MAP_BEGIN(nsMsgMailNewsUrl)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIMsgMailNewsUrl)
   NS_INTERFACE_MAP_ENTRY(nsIMsgMailNewsUrl)
   NS_INTERFACE_MAP_ENTRY(nsIURL)
   NS_INTERFACE_MAP_ENTRY(nsIURI)
NS_INTERFACE_MAP_END_THREADSAFE

////////////////////////////////////////////////////////////////////////////////////
// Begin nsIMsgMailNewsUrl specific support
////////////////////////////////////////////////////////////////////////////////////

nsresult nsMsgMailNewsUrl::GetUrlState(PRBool * aRunningUrl)
{
	if (aRunningUrl)
		*aRunningUrl = m_runningUrl;

	return NS_OK;
}

nsresult nsMsgMailNewsUrl::SetUrlState(PRBool aRunningUrl, nsresult aExitCode)
{
  // if we already knew this running state, return, unless the url was aborted
  if (m_runningUrl == aRunningUrl && aExitCode != NS_MSG_ERROR_URL_ABORTED)
    return NS_OK;
  m_runningUrl = aRunningUrl;
  nsCOMPtr <nsIMsgStatusFeedback> statusFeedback;
  
  // put this back - we need it for urls that don't run through the doc loader
  if (NS_SUCCEEDED(GetStatusFeedback(getter_AddRefs(statusFeedback))) && statusFeedback)
  {
    if (m_runningUrl)
      statusFeedback->StartMeteors();
    else
    {
      statusFeedback->ShowProgress(0);
      statusFeedback->StopMeteors();
    }
  }
  if (m_urlListeners)
  {
    if (m_runningUrl)
    {
      m_urlListeners->OnStartRunningUrl(this);
    }
    else
    {
      m_urlListeners->OnStopRunningUrl(this, aExitCode);
      m_loadGroup = nsnull; // try to break circular refs.
    }
  }
  else
    printf("no listeners in set url state\n");
  
  return NS_OK;
}

nsresult nsMsgMailNewsUrl::RegisterListener (nsIUrlListener * aUrlListener)
{
	if (m_urlListeners)
		m_urlListeners->RegisterListener(aUrlListener);
	return NS_OK;
}

nsresult nsMsgMailNewsUrl::UnRegisterListener (nsIUrlListener * aUrlListener)
{
	if (m_urlListeners)
		m_urlListeners->UnRegisterListener(aUrlListener);
	return NS_OK;
}

nsresult nsMsgMailNewsUrl::SetErrorMessage (const char * errorMessage)
{
	// functionality has been moved to nsIMsgStatusFeedback
	return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsMsgMailNewsUrl::GetErrorMessage (char ** errorMessage)
{
	// functionality has been moved to nsIMsgStatusFeedback
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgMailNewsUrl::GetServer(nsIMsgIncomingServer ** aIncomingServer)
{
	// mscott --> we could cache a copy of the server here....but if we did, we run
	// the risk of leaking the server if any single url gets leaked....of course that
	// shouldn't happen...but it could. so i'm going to look it up every time and
	// we can look at caching it later.

	nsCAutoString host;
	nsCAutoString scheme;
	nsCAutoString userName;

	nsresult rv = GetAsciiHost(host);

	/* GetUsername() returns an escaped string, so we need to manually unescape it.
	 */
	GetUsername(userName);
	NS_UnescapeURL(userName); // XXX may result in non-ASCII octets!

	rv = GetScheme(scheme);
    if (NS_SUCCEEDED(rv))
    {
        if (scheme.Equals("pop"))
          scheme.Assign("pop3");
        // we use "nntp" in the server list so translate it here.
        if (scheme.Equals("news"))
          scheme.Assign("nntp");
        nsCOMPtr<nsIMsgAccountManager> accountManager = 
                 do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
        if (NS_FAILED(rv)) return rv;
        
        nsCOMPtr<nsIMsgIncomingServer> server;
        rv = accountManager->FindServer(userName.get(),
                                        host.get(),
                                        scheme.get(),
                                        aIncomingServer);
        if (!*aIncomingServer && scheme.Equals("imap"))
        {
          // look for any imap server with this host name so clicking on 
          // other users folder urls will work. We could override this method
          // for imap urls, or we could make caching of servers work and
          // just set the server in the imap code for this case.
          rv = accountManager->FindServer("",
                                          host.get(),
                                          scheme.get(),
                                          aIncomingServer);
        }
    }

    return rv;
}

NS_IMETHODIMP nsMsgMailNewsUrl::SetStatusFeedback(nsIMsgStatusFeedback *aMsgFeedback)
{
	if (aMsgFeedback)
		m_statusFeedback = do_QueryInterface(aMsgFeedback);
	return NS_OK;
}

NS_IMETHODIMP nsMsgMailNewsUrl::GetMsgWindow(nsIMsgWindow **aMsgWindow)
{
	nsresult rv = NS_OK;
	// note: it is okay to return a null msg window and not return an error
	// it's possible the url really doesn't have msg window
	if (!m_msgWindow)
	{
//		nsCOMPtr<nsIMsgMailSession> mailSession = 
//		         do_GetService(kMsgMailSessionCID, &rv); 

//		if(NS_SUCCEEDED(rv))
//		mailSession->GetTemporaryMsgStatusFeedback(getter_AddRefs(m_statusFeedback));
	}
	if (aMsgWindow)
	{
		*aMsgWindow = m_msgWindow;
		NS_IF_ADDREF(*aMsgWindow);
	}
	else
		rv = NS_ERROR_NULL_POINTER;
	return rv;
}

NS_IMETHODIMP nsMsgMailNewsUrl::SetMsgWindow(nsIMsgWindow *aMsgWindow)
{
	if (aMsgWindow)
		m_msgWindow = do_QueryInterface(aMsgWindow);
	return NS_OK;
}

NS_IMETHODIMP nsMsgMailNewsUrl::GetStatusFeedback(nsIMsgStatusFeedback **aMsgFeedback)
{
	nsresult rv = NS_OK;
	// note: it is okay to return a null status feedback and not return an error
	// it's possible the url really doesn't have status feedback
	if (!m_statusFeedback)
	{

		if(m_msgWindow)
		{
			m_msgWindow->GetStatusFeedback(getter_AddRefs(m_statusFeedback));
		}
	}
	if (aMsgFeedback)
	{
		*aMsgFeedback = m_statusFeedback;
		NS_IF_ADDREF(*aMsgFeedback);
	}
	else
		rv = NS_ERROR_NULL_POINTER;
	return rv;
}

NS_IMETHODIMP nsMsgMailNewsUrl::GetLoadGroup(nsILoadGroup **aLoadGroup)
{
	nsresult rv = NS_OK;
	// note: it is okay to return a null load group and not return an error
	// it's possible the url really doesn't have load group
	if (!m_loadGroup)
	{
		if (m_msgWindow)
		{
            nsCOMPtr<nsIDocShell> docShell;
            m_msgWindow->GetRootDocShell(getter_AddRefs(docShell));
            nsCOMPtr<nsIWebShell> webShell(do_QueryInterface(docShell));

#if 0   // since we're not going through the doc loader for most mail/news urls,
       //, this code isn't useful
        // but I can imagine it could be useful at some point.

            // load group needs status feedback set, since it's
            // not the main window load group.
            nsCOMPtr<nsIMsgStatusFeedback> statusFeedback;
            m_msgWindow->GetStatusFeedback(getter_AddRefs(statusFeedback));

            if (statusFeedback)
            {
              nsCOMPtr<nsIWebProgress> webProgress(do_GetInterface(docShell));
              nsCOMPtr<nsIWebProgressListener> webProgressListener(do_QueryInterface(statusFeedback));

              // register our status feedback object
              if (statusFeedback && docShell)
              {
                webProgressListener = do_QueryInterface(statusFeedback);
                webProgress->AddProgressListener(webProgressListener,
                                                 nsIWebProgress::NOTIFY_ALL);
              }
            }
#endif
			if (webShell)
			{
				nsCOMPtr <nsIDocumentLoader> docLoader;
				webShell->GetDocumentLoader(*getter_AddRefs(docLoader));
				if (docLoader)
					docLoader->GetLoadGroup(getter_AddRefs(m_loadGroup));
			}
		}
	}

	if (aLoadGroup)
	{
		*aLoadGroup = m_loadGroup;
		NS_IF_ADDREF(*aLoadGroup);
	}
	else
		rv = NS_ERROR_NULL_POINTER;
	return rv;

}

NS_IMETHODIMP nsMsgMailNewsUrl::GetUpdatingFolder(PRBool *aResult)
{
  NS_ENSURE_ARG(aResult);
	*aResult = m_updatingFolder;
	return NS_OK;
}

NS_IMETHODIMP nsMsgMailNewsUrl::SetUpdatingFolder(PRBool updatingFolder)
{
	m_updatingFolder = updatingFolder;
	return NS_OK;
}

NS_IMETHODIMP nsMsgMailNewsUrl::GetAddToMemoryCache(PRBool *aAddToCache)
{
  NS_ENSURE_ARG(aAddToCache); 
	*aAddToCache = m_addContentToCache;
	return NS_OK;
}

NS_IMETHODIMP nsMsgMailNewsUrl::SetAddToMemoryCache(PRBool aAddToCache)
{
	m_addContentToCache = aAddToCache;
	return NS_OK;
}

NS_IMETHODIMP nsMsgMailNewsUrl::GetMsgIsInLocalCache(PRBool *aMsgIsInLocalCache)
{
  NS_ENSURE_ARG(aMsgIsInLocalCache); 
	*aMsgIsInLocalCache = m_msgIsInLocalCache;
	return NS_OK;
}

NS_IMETHODIMP nsMsgMailNewsUrl::SetMsgIsInLocalCache(PRBool aMsgIsInLocalCache)
{
	m_msgIsInLocalCache = aMsgIsInLocalCache;
	return NS_OK;
}

NS_IMETHODIMP nsMsgMailNewsUrl::GetSuppressErrorMsgs(PRBool *aSuppressErrorMsgs)
{
  NS_ENSURE_ARG(aSuppressErrorMsgs); 
	*aSuppressErrorMsgs = m_suppressErrorMsgs;
	return NS_OK;
}

NS_IMETHODIMP nsMsgMailNewsUrl::SetSuppressErrorMsgs(PRBool aSuppressErrorMsgs)
{
	m_suppressErrorMsgs = aSuppressErrorMsgs;
	return NS_OK;
}

NS_IMETHODIMP nsMsgMailNewsUrl::IsUrlType(PRUint32 type, PRBool *isType)
{
	//base class doesn't know about any specific types
	NS_ENSURE_ARG(isType);
	*isType = PR_FALSE;
	return NS_OK;

}

NS_IMETHODIMP nsMsgMailNewsUrl::SetSearchSession(nsIMsgSearchSession *aSearchSession)
{
	if (aSearchSession)
		m_searchSession = do_QueryInterface(aSearchSession);
	return NS_OK;
}

NS_IMETHODIMP nsMsgMailNewsUrl::GetSearchSession(nsIMsgSearchSession **aSearchSession)
{
  NS_ENSURE_ARG(aSearchSession);
	*aSearchSession = m_searchSession;
	NS_IF_ADDREF(*aSearchSession);
	return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////////
// End nsIMsgMailNewsUrl specific support
////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////
// Begin nsIURI support
////////////////////////////////////////////////////////////////////////////////////


NS_IMETHODIMP nsMsgMailNewsUrl::GetSpec(nsACString &aSpec)
{
	return m_baseURL->GetSpec(aSpec);
}

#define FILENAME_PART "&filename="
#define FILENAME_PART_LEN 10

NS_IMETHODIMP nsMsgMailNewsUrl::SetSpec(const nsACString &aSpec)
{
  const nsPromiseFlatCString &spec = PromiseFlatCString(aSpec);
  // Parse out "filename" attribute if present.
  char *start, *end;
  start = PL_strcasestr(spec.get(),FILENAME_PART);
  if (start)
  {	// Make sure we only get our own value.
    end = PL_strcasestr((char*)(start+FILENAME_PART_LEN),"&");
    if (end)
    {
      *end = 0;
      mAttachmentFileName = start+FILENAME_PART_LEN;
      *end = '&';
    }
    else
      mAttachmentFileName = start+FILENAME_PART_LEN;
  }
  // Now, set the rest.
  return m_baseURL->SetSpec(aSpec);
}

NS_IMETHODIMP nsMsgMailNewsUrl::GetPrePath(nsACString &aPrePath)
{
	return m_baseURL->GetPrePath(aPrePath);
}

NS_IMETHODIMP nsMsgMailNewsUrl::GetScheme(nsACString &aScheme)
{
	return m_baseURL->GetScheme(aScheme);
}

NS_IMETHODIMP nsMsgMailNewsUrl::SetScheme(const nsACString &aScheme)
{
	return m_baseURL->SetScheme(aScheme);
}


NS_IMETHODIMP nsMsgMailNewsUrl::GetUserPass(nsACString &aUserPass)
{
	return m_baseURL->GetUserPass(aUserPass);
}

NS_IMETHODIMP nsMsgMailNewsUrl::SetUserPass(const nsACString &aUserPass)
{
	return m_baseURL->SetUserPass(aUserPass);
}

NS_IMETHODIMP nsMsgMailNewsUrl::GetUsername(nsACString &aUsername)
{
	/* note:  this will return an escaped string */
	return m_baseURL->GetUsername(aUsername);
}

NS_IMETHODIMP nsMsgMailNewsUrl::SetUsername(const nsACString &aUsername)
{
	return m_baseURL->SetUsername(aUsername);
}

NS_IMETHODIMP nsMsgMailNewsUrl::GetPassword(nsACString &aPassword)
{
	return m_baseURL->GetPassword(aPassword);
}

NS_IMETHODIMP nsMsgMailNewsUrl::SetPassword(const nsACString &aPassword)
{
	return m_baseURL->SetPassword(aPassword);
}

NS_IMETHODIMP nsMsgMailNewsUrl::GetHostPort(nsACString &aHostPort)
{
	return m_baseURL->GetHostPort(aHostPort);
}

NS_IMETHODIMP nsMsgMailNewsUrl::SetHostPort(const nsACString &aHostPort)
{
	return m_baseURL->SetHostPort(aHostPort);
}

NS_IMETHODIMP nsMsgMailNewsUrl::GetHost(nsACString &aHost)
{
	return m_baseURL->GetHost(aHost);
}

NS_IMETHODIMP nsMsgMailNewsUrl::SetHost(const nsACString &aHost)
{
	return m_baseURL->SetHost(aHost);
}

NS_IMETHODIMP nsMsgMailNewsUrl::GetPort(PRInt32 *aPort)
{
	return m_baseURL->GetPort(aPort);
}

NS_IMETHODIMP nsMsgMailNewsUrl::SetPort(PRInt32 aPort)
{
	return m_baseURL->SetPort(aPort);
}

NS_IMETHODIMP nsMsgMailNewsUrl::GetPath(nsACString &aPath)
{
	return m_baseURL->GetPath(aPath);
}

NS_IMETHODIMP nsMsgMailNewsUrl::SetPath(const nsACString &aPath)
{
	return m_baseURL->SetPath(aPath);
}

NS_IMETHODIMP nsMsgMailNewsUrl::GetAsciiHost(nsACString &aHostA)
{
    return m_baseURL->GetAsciiHost(aHostA);
}

NS_IMETHODIMP nsMsgMailNewsUrl::GetAsciiSpec(nsACString &aSpecA)
{
    return m_baseURL->GetAsciiSpec(aSpecA);
}

NS_IMETHODIMP nsMsgMailNewsUrl::GetOriginCharset(nsACString &aOriginCharset)
{
    return m_baseURL->GetOriginCharset(aOriginCharset);
}

NS_IMETHODIMP nsMsgMailNewsUrl::Equals(nsIURI *other, PRBool *_retval)
{
	return m_baseURL->Equals(other, _retval);
}

NS_IMETHODIMP nsMsgMailNewsUrl::SchemeIs(const char *aScheme, PRBool *_retval)
{
  nsCAutoString scheme;
  nsresult rv = m_baseURL->GetScheme(scheme);
  NS_ENSURE_SUCCESS(rv,rv);

  // fix #76200 crash on email with <img> with no src.
  //
  // make sure we have a scheme before calling SchemeIs()
  // we have to do this because url parsing can result in a null mScheme
  // this extra string copy should be removed when #73845 is fixed.
  if (!scheme.IsEmpty()) {
    return m_baseURL->SchemeIs(aScheme, _retval);
  }
  else {
    *_retval = PR_FALSE;
    return NS_OK;
  }
}

NS_IMETHODIMP nsMsgMailNewsUrl::Clone(nsIURI **_retval)
{
  nsresult rv;
  nsCAutoString urlSpec;
  nsCOMPtr<nsIIOService> ioService = do_GetService(kIOServiceCID, &rv);
  if (NS_FAILED(rv)) return rv;
  rv = GetSpec(urlSpec);
  if (NS_FAILED(rv)) return rv;
  return ioService->NewURI(urlSpec, nsnull, nsnull, _retval);
}	

NS_IMETHODIMP nsMsgMailNewsUrl::Resolve(const nsACString &relativePath, nsACString &result) 
{
  // only resolve anchor urls....i.e. urls which start with '#' against the mailnews url...
  // everything else shouldn't be resolved against mailnews urls.
  nsresult rv = NS_OK;

  if (relativePath.First() == '#') // an anchor
    return m_baseURL->Resolve(relativePath, result);
  else
  {
    // if relativePath is a complete url with it's own scheme then allow it...
    nsCOMPtr<nsIIOService> ioService = do_GetService(NS_IOSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCAutoString scheme;

    rv = ioService->ExtractScheme(relativePath, scheme);
    // if we have a fully qualified scheme then pass the relative path back as the result
    if (NS_SUCCEEDED(rv) && !scheme.IsEmpty())
    {
      result = relativePath;
    }
    else
    {
      result.Truncate();
      rv = NS_ERROR_FAILURE;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsMsgMailNewsUrl::GetDirectory(nsACString &aDirectory)
{
	return m_baseURL->GetDirectory(aDirectory);
}

NS_IMETHODIMP nsMsgMailNewsUrl::SetDirectory(const nsACString &aDirectory)
{

	return m_baseURL->SetDirectory(aDirectory);
}

NS_IMETHODIMP nsMsgMailNewsUrl::GetFileName(nsACString &aFileName)
{
  if (!mAttachmentFileName.IsEmpty())
  {
    aFileName = mAttachmentFileName;
    return NS_OK;
  }
  return m_baseURL->GetFileName(aFileName);
}

NS_IMETHODIMP nsMsgMailNewsUrl::GetFileBaseName(nsACString &aFileBaseName)
{
	return m_baseURL->GetFileBaseName(aFileBaseName);
}

NS_IMETHODIMP nsMsgMailNewsUrl::SetFileBaseName(const nsACString &aFileBaseName)
{
	return m_baseURL->SetFileBaseName(aFileBaseName);
}

NS_IMETHODIMP nsMsgMailNewsUrl::GetFileExtension(nsACString &aFileExtension)
{
  if (!mAttachmentFileName.IsEmpty())
  {
    nsCAutoString extension;
    PRInt32 pos = mAttachmentFileName.RFindChar(PRUnichar('.'));
    if (pos > 0)
      mAttachmentFileName.Right(extension,
                                mAttachmentFileName.Length() -
                                (pos + 1) /* skip the '.' */);
    aFileExtension = extension;
    return NS_OK;
  }
  return m_baseURL->GetFileExtension(aFileExtension);
}

NS_IMETHODIMP nsMsgMailNewsUrl::SetFileExtension(const nsACString &aFileExtension)
{
	return m_baseURL->SetFileExtension(aFileExtension);
}

NS_IMETHODIMP nsMsgMailNewsUrl::SetFileName(const nsACString &aFileName)
{
  mAttachmentFileName = aFileName;
  return NS_OK;
}

NS_IMETHODIMP nsMsgMailNewsUrl::GetParam(nsACString &aParam)
{
	return m_baseURL->GetParam(aParam);
}

NS_IMETHODIMP nsMsgMailNewsUrl::SetParam(const nsACString &aParam)
{
	return m_baseURL->SetParam(aParam);
}

NS_IMETHODIMP nsMsgMailNewsUrl::GetQuery(nsACString &aQuery)
{
	return m_baseURL->GetQuery(aQuery);
}

NS_IMETHODIMP nsMsgMailNewsUrl::SetQuery(const nsACString &aQuery)
{
	return m_baseURL->SetQuery(aQuery);
}

NS_IMETHODIMP nsMsgMailNewsUrl::GetRef(nsACString &aRef)
{
	return m_baseURL->GetRef(aRef);
}

NS_IMETHODIMP nsMsgMailNewsUrl::SetRef(const nsACString &aRef)
{
	return m_baseURL->SetRef(aRef);
}

NS_IMETHODIMP nsMsgMailNewsUrl::GetFilePath(nsACString &o_DirFile)
{
	return m_baseURL->GetFilePath(o_DirFile);
}

NS_IMETHODIMP nsMsgMailNewsUrl::SetFilePath(const nsACString &i_DirFile)
{
	return m_baseURL->SetFilePath(i_DirFile);
}

NS_IMETHODIMP nsMsgMailNewsUrl::GetCommonBaseSpec(nsIURI *uri2, nsACString &result)
{
  return m_baseURL->GetCommonBaseSpec(uri2, result);
}

NS_IMETHODIMP nsMsgMailNewsUrl::GetRelativeSpec(nsIURI *uri2, nsACString &result)
{
  return m_baseURL->GetRelativeSpec(uri2, result);
}

NS_IMETHODIMP nsMsgMailNewsUrl::SetMemCacheEntry(nsICacheEntryDescriptor *memCacheEntry)
{
  m_memCacheEntry = memCacheEntry;
  return NS_OK;
}

NS_IMETHODIMP nsMsgMailNewsUrl:: GetMemCacheEntry(nsICacheEntryDescriptor **memCacheEntry)
{
  NS_ENSURE_ARG(memCacheEntry);
  nsresult rv = NS_OK;

  if (m_memCacheEntry)
  {
    *memCacheEntry = m_memCacheEntry;
    NS_ADDREF(*memCacheEntry);
  }
  else
  {
    *memCacheEntry = nsnull;
    return NS_ERROR_NULL_POINTER;
  }

  return rv;
}

NS_IMETHODIMP nsMsgMailNewsUrl::SetImageCacheSession(nsICacheSession *imageCacheSession)
{
  m_imageCacheSession = imageCacheSession;
  return NS_OK;
}

NS_IMETHODIMP nsMsgMailNewsUrl:: GetImageCacheSession(nsICacheSession **imageCacheSession)
{
  NS_ENSURE_ARG(imageCacheSession);

  NS_IF_ADDREF(*imageCacheSession = m_imageCacheSession);

  return NS_OK;
}

NS_IMETHODIMP nsMsgMailNewsUrl:: CacheCacheEntry(nsICacheEntryDescriptor *cacheEntry)
{
  if (!m_cachedMemCacheEntries)
    NS_NewISupportsArray(getter_AddRefs(m_cachedMemCacheEntries));
  if (m_cachedMemCacheEntries)
  {
    nsCOMPtr<nsISupports> cacheEntrySupports(do_QueryInterface(cacheEntry));
    if(cacheEntrySupports)
      m_cachedMemCacheEntries->AppendElement(cacheEntrySupports);
  }

  return NS_OK;
}

NS_IMETHODIMP nsMsgMailNewsUrl:: RemoveCacheEntry(nsICacheEntryDescriptor *cacheEntry)
{
  if (m_cachedMemCacheEntries)
  {
    nsCOMPtr<nsISupports> cacheEntrySupports(do_QueryInterface(cacheEntry));
    if(cacheEntrySupports)
      m_cachedMemCacheEntries->RemoveElement(cacheEntrySupports);
  }
  return NS_OK;
}

NS_IMETHODIMP nsMsgMailNewsUrl::GetMimeHeaders(nsIMimeHeaders * *mimeHeaders)
{
    NS_ENSURE_ARG_POINTER(mimeHeaders);
    NS_IF_ADDREF(*mimeHeaders = mMimeHeaders);
    return NS_OK;
}

NS_IMETHODIMP nsMsgMailNewsUrl::SetMimeHeaders(nsIMimeHeaders *mimeHeaders)
{
    mMimeHeaders = mimeHeaders;
    return NS_OK;
}
