/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Blake Ross <blaker@netscape.com> (Original Author)
 *   Ben Goodger <ben@netscape.com> (Original Author)
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
 
#include "nsDownloadManager.h"
#include "nsIWebProgress.h"
#include "nsIRDFLiteral.h"
#include "rdf.h"
#include "nsNetUtil.h"
#include "nsIDOMWindow.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDOMEvent.h"
#include "nsIDOMEventTarget.h"
#include "nsRDFCID.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsIWebBrowserPersist.h"
#include "nsIObserver.h"
#include "nsIProgressDialog.h"
#include "nsIWebBrowserPersist.h"
#include "nsIWindowWatcher.h"
#include "nsIStringBundle.h"
#include "nsCRT.h"

/* Outstanding issues/todo:
 * 1. Implement pause/resume.
 */
  
static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);

#define DOWNLOAD_MANAGER_FE_URL "chrome://communicator/content/downloadmanager/downloadmanager.xul"
#define DOWNLOAD_MANAGER_BUNDLE "chrome://communicator/locale/downloadmanager/downloadmanager.properties"
#define INTERVAL 500

nsIRDFResource* gNC_DownloadsRoot;
nsIRDFResource* gNC_File;
nsIRDFResource* gNC_URL;
nsIRDFResource* gNC_Name;
nsIRDFResource* gNC_ProgressMode;
nsIRDFResource* gNC_ProgressPercent;
nsIRDFResource* gNC_Transferred;
nsIRDFResource* gNC_DownloadState;
nsIRDFResource* gNC_StatusText;

nsIRDFService* gRDFService;

///////////////////////////////////////////////////////////////////////////////
// nsDownloadManager

NS_IMPL_ISUPPORTS3(nsDownloadManager, nsIDownloadManager, nsIDOMEventListener, nsIObserver)

nsDownloadManager::nsDownloadManager() : mCurrDownloads(nsnull)
{
  NS_INIT_ISUPPORTS();
}

nsDownloadManager::~nsDownloadManager()
{
  gRDFService->UnregisterDataSource(mDataSource);

  NS_IF_RELEASE(gNC_DownloadsRoot);                                             
  NS_IF_RELEASE(gNC_File);                                                      
  NS_IF_RELEASE(gNC_URL);                                                       
  NS_IF_RELEASE(gNC_Name);                                                      
  NS_IF_RELEASE(gNC_ProgressMode);
  NS_IF_RELEASE(gNC_ProgressPercent);
  NS_IF_RELEASE(gNC_Transferred);
  NS_IF_RELEASE(gNC_DownloadState);
  NS_IF_RELEASE(gNC_StatusText);

  nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);                
  gRDFService = nsnull;                                                         

  delete mCurrDownloads;
  mCurrDownloads = nsnull;
}

nsresult
nsDownloadManager::Init()
{
  nsresult rv;
  mRDFContainerUtils = do_GetService("@mozilla.org/rdf/container-utils;1", &rv);
  if (NS_FAILED(rv)) return rv;

  rv = nsServiceManager::GetService(kRDFServiceCID, NS_GET_IID(nsIRDFService),
                                    (nsISupports**) &gRDFService);
  if (NS_FAILED(rv)) return rv;                                                 

  gRDFService->GetResource("NC:DownloadsRoot", &gNC_DownloadsRoot);
  gRDFService->GetResource(NC_NAMESPACE_URI "File", &gNC_File);
  gRDFService->GetResource(NC_NAMESPACE_URI "URL", &gNC_URL);
  gRDFService->GetResource(NC_NAMESPACE_URI "Name", &gNC_Name);
  gRDFService->GetResource(NC_NAMESPACE_URI "ProgressMode", &gNC_ProgressMode);
  gRDFService->GetResource(NC_NAMESPACE_URI "ProgressPercent", &gNC_ProgressPercent);
  gRDFService->GetResource(NC_NAMESPACE_URI "Transferred", &gNC_Transferred);
  gRDFService->GetResource(NC_NAMESPACE_URI "DownloadState", &gNC_DownloadState);
  gRDFService->GetResource(NC_NAMESPACE_URI "StatusText", &gNC_StatusText);

  nsCAutoString downloadsDB;
  rv = GetProfileDownloadsFileURL(downloadsDB);
  if (NS_FAILED(rv)) return rv;

  rv = gRDFService->GetDataSourceBlocking(downloadsDB.get(), getter_AddRefs(mDataSource));
  if (NS_FAILED(rv)) return rv;

  mListener = do_CreateInstance("@mozilla.org/download-manager/listener;1", &rv);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIStringBundleService> bundleService = do_GetService(kStringBundleServiceCID, &rv);
  if (NS_FAILED(rv)) return rv;
  
  return bundleService->CreateBundle(DOWNLOAD_MANAGER_BUNDLE, getter_AddRefs(mBundle));
}

nsresult
nsDownloadManager::DownloadStarted(const char* aPath)
{
  nsCStringKey key(aPath);
  if (mCurrDownloads->Exists(&key))
    AssertProgressInfoFor(aPath);

  return NS_OK;
}

nsresult
nsDownloadManager::DownloadEnded(const char* aPath, const PRUnichar* aMessage)
{
  nsCStringKey key(aPath);
  if (mCurrDownloads->Exists(&key)) {
    AssertProgressInfoFor(aPath);
    mCurrDownloads->Remove(&key);
  }

  return NS_OK;
}

nsresult
nsDownloadManager::GetProfileDownloadsFileURL(nsCString& aDownloadsFileURL)
{
  nsCOMPtr<nsIFile> downloadsFile;
  nsresult rv = NS_GetSpecialDirectory(NS_APP_DOWNLOADS_50_FILE, getter_AddRefs(downloadsFile));
  if (NS_FAILED(rv))
    return rv;
    
  return NS_GetURLSpecFromFile(downloadsFile, aDownloadsFileURL);
}

nsresult
nsDownloadManager::GetDownloadsContainer(nsIRDFContainer** aResult)
{
  PRBool isContainer;
  nsresult rv = mRDFContainerUtils->IsContainer(mDataSource, gNC_DownloadsRoot, &isContainer);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIRDFContainer> ctr;

  if (!isContainer) {
    rv = mRDFContainerUtils->MakeSeq(mDataSource, gNC_DownloadsRoot, getter_AddRefs(ctr));
    if (NS_FAILED(rv)) return rv;
  }
  else {
    ctr = do_CreateInstance(NS_RDF_CONTRACTID "/container;1", &rv);
    if (NS_FAILED(rv)) return rv;
    rv = ctr->Init(mDataSource, gNC_DownloadsRoot);
    if (NS_FAILED(rv)) return rv;
  }

  *aResult = ctr;
  NS_IF_ADDREF(*aResult);

  return rv;
}

nsresult
nsDownloadManager::GetInternalListener(nsIDownloadProgressListener** aInternalListener)
{
  *aInternalListener = mListener;
  NS_IF_ADDREF(*aInternalListener);
  return NS_OK;
}

nsresult
nsDownloadManager::GetDataSource(nsIRDFDataSource** aDataSource)
{
  *aDataSource = mDataSource;
  NS_IF_ADDREF(*aDataSource);
  return NS_OK;
}

nsresult
nsDownloadManager::AssertProgressInfo()
{
  // get the downloads container
  nsCOMPtr<nsIRDFContainer> downloads;
  nsresult rv = GetDownloadsContainer(getter_AddRefs(downloads));
  if (NS_FAILED(rv)) return rv;

  // get the container's elements (nsIRDFResource's)
  nsCOMPtr<nsISimpleEnumerator> items;
  rv = downloads->GetElements(getter_AddRefs(items));
  if (NS_FAILED(rv)) return rv;

  if (!mCurrDownloads)
    mCurrDownloads = new nsHashtable();

  // enumerate the resources, use their ids to retrieve the corresponding
  // nsIDownloads from the hashtable (if they don't exist, the download isn't
  // a current transfer), get the items' progress information,
  // and assert it into the graph

  PRBool moreElements;
  items->HasMoreElements(&moreElements);
  for( ; moreElements; items->HasMoreElements(&moreElements)) {
    nsCOMPtr<nsISupports> supports;
    items->GetNext(getter_AddRefs(supports));
    nsCOMPtr<nsIRDFResource> res = do_QueryInterface(supports);
    char* id;
    res->GetValue(&id);
    rv = AssertProgressInfoFor(id);
  }
  return rv;
}

nsresult
nsDownloadManager::AssertProgressInfoFor(const char* aPath)
{
  nsCStringKey key(aPath);
  if (!mCurrDownloads->Exists(&key))
    return NS_ERROR_FAILURE;
 
  nsDownload* internalDownload = NS_STATIC_CAST(nsDownload*, mCurrDownloads->Get(&key));
  nsCOMPtr<nsIDownload> download;
  internalDownload->QueryInterface(NS_GET_IID(nsIDownload), (void**) getter_AddRefs(download));
  if (!download)
    return NS_ERROR_FAILURE;
  
  nsresult rv;
  PRInt32 percentComplete;
  nsCOMPtr<nsIRDFNode> oldTarget;
  nsCOMPtr<nsIRDFInt> intLiteral;
  nsCOMPtr<nsIRDFResource> res;
  nsCOMPtr<nsIRDFLiteral> literal;

  gRDFService->GetResource(aPath, getter_AddRefs(res));

  DownloadState state;
  internalDownload->GetDownloadState(&state);
 
  // update progress mode
  nsAutoString progressMode;
  if (state == DOWNLOADING)
    progressMode.Assign(NS_LITERAL_STRING("normal"));
  else
    progressMode.Assign(NS_LITERAL_STRING("none"));

  gRDFService->GetLiteral(progressMode.get(), getter_AddRefs(literal));

  rv = mDataSource->GetTarget(res, gNC_ProgressMode, PR_TRUE, getter_AddRefs(oldTarget));
  
  if (oldTarget)
    rv = mDataSource->Change(res, gNC_ProgressMode, oldTarget, literal);
  else
    rv = mDataSource->Assert(res, gNC_ProgressMode, literal, PR_TRUE);
  if (NS_FAILED(rv)) return rv;

  // update download state (not started, downloading, queued, finished, etc...)
  gRDFService->GetIntLiteral(state, getter_AddRefs(intLiteral));

  mDataSource->GetTarget(res, gNC_DownloadState, PR_TRUE, getter_AddRefs(oldTarget));
  
  if (oldTarget) {
    rv = mDataSource->Change(res, gNC_DownloadState, oldTarget, intLiteral);
    if (NS_FAILED(rv)) return rv;
  }

  nsAutoString strKey;
  if (state == NOTSTARTED)
    strKey.Assign(NS_LITERAL_STRING("notStarted"));
  else if (state == DOWNLOADING)
    strKey.Assign(NS_LITERAL_STRING("downloading"));
  else if (state == FINISHED)
    strKey.Assign(NS_LITERAL_STRING("finished"));
  else if (state == FAILED)
    strKey.Assign(NS_LITERAL_STRING("failed"));
  else if (state == CANCELED)
    strKey.Assign(NS_LITERAL_STRING("canceled"));

  nsXPIDLString value;
  rv = mBundle->GetStringFromName(strKey.get(), getter_Copies(value));    
  if (NS_FAILED(rv)) return rv;

  gRDFService->GetLiteral(value, getter_AddRefs(literal));

  rv = mDataSource->GetTarget(res, gNC_StatusText, PR_TRUE, getter_AddRefs(oldTarget));
  
  if (oldTarget) {
    rv = mDataSource->Change(res, gNC_StatusText, oldTarget, literal);
    if (NS_FAILED(rv)) return rv;
  }
  else {
    rv = mDataSource->Assert(res, gNC_StatusText, literal, PR_TRUE);
    if (NS_FAILED(rv)) return rv;
  }
  
  // update percentage
  download->GetPercentComplete(&percentComplete);

  mDataSource->GetTarget(res, gNC_ProgressPercent, PR_TRUE, getter_AddRefs(oldTarget));
  gRDFService->GetIntLiteral(percentComplete, getter_AddRefs(intLiteral));

  if (oldTarget)
    rv = mDataSource->Change(res, gNC_ProgressPercent, oldTarget, intLiteral);
  else
    rv = mDataSource->Assert(res, gNC_ProgressPercent, intLiteral, PR_TRUE);
  if (NS_FAILED(rv)) return rv;

  // update transferred
  PRInt32 current = 0;
  PRInt32 max = 0;
  internalDownload->GetTransferInformation(&current, &max);
 
  nsAutoString currBytes; currBytes.AppendInt(current);
  nsAutoString maxBytes; maxBytes.AppendInt(max);
  const PRUnichar *strings[] = {
    currBytes.get(),
    maxBytes.get()
  };
  
  rv = mBundle->FormatStringFromName(NS_LITERAL_STRING("transferred").get(),
                                     strings, 2, getter_Copies(value));
  if (NS_FAILED(rv)) return rv;

  gRDFService->GetLiteral(value, getter_AddRefs(literal));
 
  mDataSource->GetTarget(res, gNC_Transferred, PR_TRUE, getter_AddRefs(oldTarget));
 
  if (oldTarget)
    rv = mDataSource->Change(res, gNC_Transferred, oldTarget, literal);
  else
    rv = mDataSource->Assert(res, gNC_Transferred, literal, PR_TRUE);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIRDFRemoteDataSource> remote = do_QueryInterface(mDataSource);
  remote->Flush();

  // XXX should also store and update time elapsed
  return rv;
}  

///////////////////////////////////////////////////////////////////////////////
// nsIDownloadManager

NS_IMETHODIMP
nsDownloadManager::AddDownload(nsIURI* aSource,
                               nsILocalFile* aTarget,
                               const PRUnichar* aDisplayName,
                               const PRUnichar* aOpeningWith,
                               PRInt64 aStartTime,
                               nsIWebBrowserPersist* aPersist,
                               nsIDownload** aDownload)
{
  NS_ENSURE_ARG_POINTER(aSource);
  NS_ENSURE_ARG_POINTER(aTarget);
  NS_ENSURE_ARG_POINTER(aDownload);

  nsCOMPtr<nsIRDFContainer> downloads;
  nsresult rv = GetDownloadsContainer(getter_AddRefs(downloads));
  if (NS_FAILED(rv)) return rv;

  nsDownload* internalDownload = new nsDownload();
  if (!internalDownload)
    return NS_ERROR_OUT_OF_MEMORY;

  internalDownload->QueryInterface(NS_GET_IID(nsIDownload), (void**) aDownload);
  if (!aDownload)
    return NS_ERROR_FAILURE;

  // give our new nsIDownload some info so it's ready to go off into the world
  internalDownload->SetDownloadManager(this);
  internalDownload->SetTarget(aTarget);
  internalDownload->SetSource(aSource);

  // the persistent descriptor of the target is the unique identifier we use
  nsCAutoString path;
  rv = aTarget->GetNativePath(path);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIRDFResource> downloadRes;
  gRDFService->GetResource(path.get(), getter_AddRefs(downloadRes));

  // if the resource is in the container already (the user has already
  // downloaded this file), remove it
  PRInt32 itemIndex;
  nsCOMPtr<nsIRDFNode> node;
  downloads->IndexOf(downloadRes, &itemIndex);
  if (itemIndex > 0) {
    rv = downloads->RemoveElementAt(itemIndex, PR_TRUE, getter_AddRefs(node));
    if (NS_FAILED(rv)) return rv;
  }
  rv = downloads->AppendElement(downloadRes);
  if (NS_FAILED(rv)) return rv;
  
  // Assert source url information
  nsCAutoString spec;
  aSource->GetSpec(spec);

  nsCOMPtr<nsIRDFResource> urlResource;
  gRDFService->GetResource(spec.get(), getter_AddRefs(urlResource));
  mDataSource->GetTarget(downloadRes, gNC_URL, PR_TRUE, getter_AddRefs(node));
  if (node)
    rv = mDataSource->Change(downloadRes, gNC_URL, node, urlResource);
  else
    rv = mDataSource->Assert(downloadRes, gNC_URL, urlResource, PR_TRUE);
  if (NS_FAILED(rv)) {
    downloads->IndexOf(downloadRes, &itemIndex);
    downloads->RemoveElementAt(itemIndex, PR_TRUE, getter_AddRefs(node));
    return rv;
  }

  // Set and assert the "pretty" (display) name of the download
  nsAutoString displayName; displayName.Assign(aDisplayName);
  if (displayName.IsEmpty()) {
    aTarget->GetLeafName(displayName);
  }
  (*aDownload)->SetDisplayName(displayName.get());
 
  nsCOMPtr<nsIRDFLiteral> nameLiteral;
  gRDFService->GetLiteral(displayName.get(), getter_AddRefs(nameLiteral));
  mDataSource->GetTarget(downloadRes, gNC_Name, PR_TRUE, getter_AddRefs(node));
  if (node)
    rv = mDataSource->Change(downloadRes, gNC_Name, node, nameLiteral);
  else
    rv = mDataSource->Assert(downloadRes, gNC_Name, nameLiteral, PR_TRUE);
  if (NS_FAILED(rv)) {
    downloads->IndexOf(downloadRes, &itemIndex);
    downloads->RemoveElementAt(itemIndex, PR_TRUE, getter_AddRefs(node));
    return rv;
  }
  
  internalDownload->SetOpeningWith(aOpeningWith);
  internalDownload->SetStartTime(aStartTime);

  // Assert file information
  nsCOMPtr<nsIRDFResource> fileResource;
  gRDFService->GetResource(path.get(), getter_AddRefs(fileResource));
  rv = mDataSource->Assert(downloadRes, gNC_File, fileResource, PR_TRUE);
  if (NS_FAILED(rv)) {
    downloads->IndexOf(downloadRes, &itemIndex);
    downloads->RemoveElementAt(itemIndex, PR_TRUE, getter_AddRefs(node));
    return rv;
  }
  
  // Assert download state information (NOTSTARTED, since it's just now being added)
  nsCOMPtr<nsIRDFInt> intLiteral;
  gRDFService->GetIntLiteral(NOTSTARTED, getter_AddRefs(intLiteral));
  mDataSource->GetTarget(downloadRes, gNC_DownloadState, PR_TRUE, getter_AddRefs(node));
  if (node)
    rv = mDataSource->Change(downloadRes, gNC_DownloadState, node, intLiteral);
  else
    rv = mDataSource->Assert(downloadRes, gNC_DownloadState, intLiteral, PR_TRUE);
  if (NS_FAILED(rv)) {
    downloads->IndexOf(downloadRes, &itemIndex);
    downloads->RemoveElementAt(itemIndex, PR_TRUE, getter_AddRefs(node));
    return rv;
  }
  
  // Now flush all this to disk
  nsCOMPtr<nsIRDFRemoteDataSource> remote(do_QueryInterface(mDataSource));
  rv = remote->Flush();
  if (NS_FAILED(rv)) {
    downloads->IndexOf(downloadRes, &itemIndex);
    downloads->RemoveElementAt(itemIndex, PR_TRUE, getter_AddRefs(node));
    return rv;
  }

  // if a persist object was specified, set the download item as the progress listener
  // this will create a cycle that will be broken in nsDownload::OnStateChange
  if (aPersist) {
    internalDownload->SetPersist(aPersist);
    nsCOMPtr<nsIWebProgressListener> listener = do_QueryInterface(*aDownload);
    aPersist->SetProgressListener(listener);
  }

  // we use this table to manage ongoing downloads internally
  if (!mCurrDownloads)
    mCurrDownloads = new nsHashtable();
  
  nsCStringKey key(path);
  if (mCurrDownloads->Exists(&key))
    mCurrDownloads->Remove(&key);

  mCurrDownloads->Put(&key, *aDownload);

  return rv;
}

NS_IMETHODIMP
nsDownloadManager::GetDownload(const char* aPath, nsIDownload** aDownloadItem)
{
  NS_ENSURE_ARG_POINTER(aDownloadItem);

  // if it's currently downloading we can get it from the table
  // XXX otherwise we should look for it in the datasource and
  //     create a new nsIDownload with the resource's properties
  nsCStringKey key(aPath);
  if (mCurrDownloads->Exists(&key)) {
    *aDownloadItem = NS_STATIC_CAST(nsIDownload*, mCurrDownloads->Get(&key));
    NS_ADDREF(*aDownloadItem);
    return NS_OK;
  }

  *aDownloadItem = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsDownloadManager::CancelDownload(const char* aPath)
{
  nsresult rv = NS_OK;
  nsCStringKey key(aPath);
  if (!mCurrDownloads->Exists(&key))
    return NS_ERROR_FAILURE;
  
  nsCOMPtr<nsIDownload> download;
  nsDownload* internalDownload = NS_STATIC_CAST(nsDownload*, mCurrDownloads->Get(&key));
  internalDownload->QueryInterface(NS_GET_IID(nsIDownload), (void**) getter_AddRefs(download));
  if (!download)
    return NS_ERROR_FAILURE;
    
  internalDownload->SetDownloadState(CANCELED);

  // if a persist was provided, we can do the cancel ourselves.
  nsCOMPtr<nsIWebBrowserPersist> persist;
  download->GetPersist(getter_AddRefs(persist));
  if (persist) {
    rv = persist->CancelSave();
    if (NS_FAILED(rv)) return rv;
  }

  // if an observer was provided, notify that the download was cancelled.
  // if no persist was provided, this is necessary so that whatever transfer
  // component being used can cancel the download itself.
  nsCOMPtr<nsIObserver> observer;
  download->GetObserver(getter_AddRefs(observer));
  if (observer) {
    rv = observer->Observe(download, "oncancel", nsnull);
    if (NS_FAILED(rv)) return rv;
  }
  
  DownloadEnded(aPath, nsnull);
  
  // if there's a progress dialog open for the item,
  // we have to notify it that we're cancelling
  nsCOMPtr<nsIProgressDialog> dialog;
  internalDownload->GetDialog(getter_AddRefs(dialog));
  if (dialog) {
    observer = do_QueryInterface(dialog);
    rv = observer->Observe(download, "oncancel", nsnull);
    if (NS_FAILED(rv)) return rv;
  }
  
  return rv;
}

NS_IMETHODIMP
nsDownloadManager::RemoveDownload(const char* aPath)
{
  nsCStringKey key(aPath);
  
  // RemoveDownload is for downloads not currently in progress. Having it
  // cancel in-progress downloads would make things complicated, so just return.
  PRBool inProgress = mCurrDownloads->Exists(&key);
  NS_ASSERTION(!inProgress, "Can't call RemoveDownload on a download in progress!");
  if (inProgress)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIRDFContainer> downloads;
    nsresult rv = GetDownloadsContainer(getter_AddRefs(downloads));
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr<nsIRDFResource> res;
  gRDFService->GetResource(aPath, getter_AddRefs(res));

  PRInt32 itemIndex;
  downloads->IndexOf(res, &itemIndex);
  if (itemIndex <= 0)
    return NS_ERROR_FAILURE;
  
  nsCOMPtr<nsIRDFNode> node;
  rv = downloads->RemoveElementAt(itemIndex, PR_TRUE, getter_AddRefs(node));
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr<nsIRDFRemoteDataSource> remote = do_QueryInterface(mDataSource);
  return remote->Flush();
}  

NS_IMETHODIMP
nsDownloadManager::Open(nsIDOMWindow* aParent)
{

  // first assert new progress info so the ui is correctly updated
  // if this fails, it fails -- continue.
  AssertProgressInfo();
  
  // if we ever have the capability to display the UI of third party dl managers,
  // we'll open their UI here instead.
  nsresult rv;
  nsCOMPtr<nsIWindowWatcher> ww = do_GetService("@mozilla.org/embedcomp/window-watcher;1", &rv);
  if (NS_FAILED(rv)) return rv;

  // pass the datasource to the window
  nsCOMPtr<nsISupportsArray> params(do_CreateInstance(NS_SUPPORTSARRAY_CONTRACTID));
  nsCOMPtr<nsISupports> dsSupports(do_QueryInterface(mDataSource));
  params->AppendElement(dsSupports);

  nsCOMPtr<nsIDOMWindow> newWindow;
  rv = ww->OpenWindow(aParent,
                      DOWNLOAD_MANAGER_FE_URL,
                      "_blank",
                      "chrome,all,dialog=no,resizable",
                      params,
                      getter_AddRefs(newWindow));

  if (NS_FAILED(rv)) return rv;

  // XXX whether or not mDocument is null is not a sufficient flag,
  // because in the future we may support using a third party download manager that
  // doesn't use our architecture

  nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(newWindow);
  if (!target) return NS_ERROR_FAILURE;
  
  rv = target->AddEventListener(NS_LITERAL_STRING("load"), this, PR_FALSE);
  if (NS_FAILED(rv)) return rv;
 
  return target->AddEventListener(NS_LITERAL_STRING("unload"), this, PR_FALSE);
}

NS_IMETHODIMP
nsDownloadManager::OpenProgressDialogFor(const char* aPath, nsIDOMWindow* aParent)
{
  nsresult rv;
  nsCStringKey key(aPath);
  if (!mCurrDownloads->Exists(&key))
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDownload> download;
  nsDownload* internalDownload = NS_STATIC_CAST(nsDownload*, mCurrDownloads->Get(&key));
  internalDownload->QueryInterface(NS_GET_IID(nsIDownload), (void**) getter_AddRefs(download));
  if (!download)
    return NS_ERROR_FAILURE;
 

  nsCOMPtr<nsIProgressDialog> oldDialog;
  internalDownload->GetDialog(getter_AddRefs(oldDialog));
  
  if (oldDialog) {
    nsCOMPtr<nsIDOMWindow> window;
    oldDialog->GetDialog(getter_AddRefs(window));
    if (window) {
      nsCOMPtr<nsIDOMWindowInternal> internalWin = do_QueryInterface(window);
      internalWin->Focus();
      return NS_OK;
    }
  }

  nsCOMPtr<nsIProgressDialog> dialog(do_CreateInstance("@mozilla.org/progressdialog;1", &rv));
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr<nsIDownload> dl = do_QueryInterface(dialog);

  // now give the dialog the necessary context
  
  // start time...
  PRInt64 startTime = 0;
  download->GetStartTime(&startTime);
  
  // source...
  nsCOMPtr<nsIURI> source;
  download->GetSource(getter_AddRefs(source));

  // target...
  nsCOMPtr<nsILocalFile> target;
  download->GetTarget(getter_AddRefs(target));
  
  // helper app...
  nsXPIDLString openingWith;
  download->GetOpeningWith(getter_Copies(openingWith));

  dl->Init(source, target, nsnull, openingWith, startTime, nsnull); 
  dl->SetObserver(this);

  // now set the listener so we forward notifications to the dialog
  nsCOMPtr<nsIWebProgressListener> listener = do_QueryInterface(dialog);
  internalDownload->SetDialogListener(listener);
  
  internalDownload->SetDialog(dialog);
  
  return dialog->Open(aParent);
}

NS_IMETHODIMP
nsDownloadManager::OnClose()
{
  mDocument = nsnull;
  mListener->SetDocument(nsnull);
  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// nsIDOMEventListener

NS_IMETHODIMP
nsDownloadManager::HandleEvent(nsIDOMEvent* aEvent)
{
  // the event is either load or unload
  nsAutoString eventType;
  aEvent->GetType(eventType);
  if (eventType.Equals(NS_LITERAL_STRING("unload")))
    return OnClose();

  nsCOMPtr<nsIDOMEventTarget> target;
  nsresult rv = aEvent->GetTarget(getter_AddRefs(target));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIDOMNode> targetNode(do_QueryInterface(target));
  mDocument = do_QueryInterface(targetNode);
  mListener->SetDocument(mDocument);

  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// nsIObserver

NS_IMETHODIMP
nsDownloadManager::Observe(nsISupports* aSubject, const char* aTopic, const PRUnichar* aData)
{
  nsCOMPtr<nsIProgressDialog> dialog = do_QueryInterface(aSubject);
  if (nsCRT::strcmp(aTopic, "oncancel") == 0) {
    nsCOMPtr<nsILocalFile> target;
    dialog->GetTarget(getter_AddRefs(target));
    
    nsCAutoString path;
    nsresult rv = target->GetNativePath(path);
    if (NS_FAILED(rv)) return rv;
    
    nsCStringKey key(path);
    if (mCurrDownloads->Exists(&key)) {
      // unset dialog since it's closing
      nsDownload* download = NS_STATIC_CAST(nsDownload*, mCurrDownloads->Get(&key));
      download->SetDialog(nsnull);
      
      return CancelDownload(path.get());  
    }
  }
  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// nsDownload

NS_IMPL_ISUPPORTS2(nsDownload, nsIDownload, nsIWebProgressListener)

nsDownload::nsDownload():mStartTime(0),
                         mPercentComplete(0),
                         mCurrBytes(0),
                         mMaxBytes(0),
                         mDownloadState(NOTSTARTED),
                         mLastUpdate(-500),
                         mOpeningWith(nsnull)
{
  NS_INIT_ISUPPORTS();
}

nsDownload::~nsDownload()
{  
  nsCAutoString path;
  nsresult rv = mTarget->GetNativePath(path);
  if (NS_FAILED(rv)) return;

  mDownloadManager->AssertProgressInfoFor(path.get());
}

nsresult
nsDownload::SetDownloadManager(nsDownloadManager* aDownloadManager)
{
  mDownloadManager = aDownloadManager;
  return NS_OK;
}

nsresult
nsDownload::SetDialogListener(nsIWebProgressListener* aDialogListener)
{
  mDialogListener = aDialogListener;
  return NS_OK;
}

nsresult
nsDownload::GetDialogListener(nsIWebProgressListener** aDialogListener)
{
  *aDialogListener = mDialogListener;
  NS_IF_ADDREF(*aDialogListener);
  return NS_OK;
}

nsresult
nsDownload::SetDialog(nsIProgressDialog* aDialog)
{
  mDialog = aDialog;
  return NS_OK;
}

nsresult
nsDownload::GetDialog(nsIProgressDialog** aDialog)
{
  *aDialog = mDialog;
  NS_IF_ADDREF(*aDialog);
  return NS_OK;
}

nsresult
nsDownload::GetDownloadState(DownloadState* aState)
{
  *aState = mDownloadState;
  return NS_OK;
}

nsresult
nsDownload::SetDownloadState(DownloadState aState)
{
  mDownloadState = aState;
  return NS_OK;
}

nsresult
nsDownload::SetPersist(nsIWebBrowserPersist* aPersist)
{
  mPersist = aPersist;
  return NS_OK;
}

nsresult
nsDownload::SetSource(nsIURI* aSource)
{
  mSource = aSource;
  return NS_OK;
}

nsresult
nsDownload::SetTarget(nsILocalFile* aTarget)
{
  mTarget = aTarget;
  return NS_OK;
}

nsresult
nsDownload::GetTransferInformation(PRInt32* aCurr, PRInt32* aMax)
{
  *aCurr = mCurrBytes;
  *aMax = mMaxBytes;
  return NS_OK;
}

nsresult
nsDownload::SetStartTime(PRInt64 aStartTime)
{
  mStartTime = aStartTime;
  return NS_OK;
}

nsresult
nsDownload::SetOpeningWith(const PRUnichar* aOpeningWith)
{
  mOpeningWith = aOpeningWith;
  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// nsIWebProgressListener

NS_IMETHODIMP
nsDownload::OnProgressChange(nsIWebProgress *aWebProgress,
                             nsIRequest *aRequest,
                             PRInt32 aCurSelfProgress,
                             PRInt32 aMaxSelfProgress,
                             PRInt32 aCurTotalProgress,
                             PRInt32 aMaxTotalProgress)
{

  if (!mRequest)
    mRequest = aRequest; // used for pause/resume

  // filter notifications since they come in so frequently
  PRTime delta;
  PRTime now = PR_Now();
  LL_SUB(delta, now, mLastUpdate);
  if (LL_CMP(delta, <, INTERVAL) && aMaxTotalProgress != -1 && aCurTotalProgress < aMaxTotalProgress)
    return NS_OK;

  mLastUpdate = now;

  if (mDownloadState == NOTSTARTED) {
    nsCAutoString path;
    nsresult rv = mTarget->GetNativePath(path);
    if (NS_FAILED(rv)) return rv;

    mDownloadState = DOWNLOADING;
    mDownloadManager->DownloadStarted(path.get());
  }

  if (aMaxTotalProgress > 0)
    mPercentComplete = aCurTotalProgress * 100 / aMaxTotalProgress;
  else
    mPercentComplete = -1;

  mCurrBytes = (PRInt32)((PRFloat64)aCurTotalProgress / 1024.0 + .5);
  mMaxBytes = (PRInt32)((PRFloat64)aMaxTotalProgress / 1024 + .5);

  if (mListener) {
    mListener->OnProgressChange(aWebProgress, aRequest, aCurSelfProgress, aMaxSelfProgress,
                                aCurTotalProgress, aMaxTotalProgress);
  }

  if (mDownloadManager->MustUpdateUI()) {
    nsCOMPtr<nsIDownloadProgressListener> internalListener;
    mDownloadManager->GetInternalListener(getter_AddRefs(internalListener));
    if (internalListener) {
      internalListener->OnProgressChange(aWebProgress, aRequest, aCurSelfProgress, aMaxSelfProgress,
                                          aCurTotalProgress, aMaxTotalProgress, this);
    }
  }

  if (mDialogListener) {
    mDialogListener->OnProgressChange(aWebProgress, aRequest, aCurSelfProgress, aMaxSelfProgress,
                                          aCurTotalProgress, aMaxTotalProgress);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDownload::OnLocationChange(nsIWebProgress *aWebProgress,
                             nsIRequest *aRequest, nsIURI *aLocation)
{
  if (mListener)
    mListener->OnLocationChange(aWebProgress, aRequest, aLocation);

  if (mDownloadManager->MustUpdateUI()) {
    nsCOMPtr<nsIDownloadProgressListener> internalListener;
    mDownloadManager->GetInternalListener(getter_AddRefs(internalListener));
    if (internalListener)
      internalListener->OnLocationChange(aWebProgress, aRequest, aLocation, this);
  }

  if (mDialogListener)
    mDialogListener->OnLocationChange(aWebProgress, aRequest, aLocation);

  return NS_OK;
}

NS_IMETHODIMP
nsDownload::OnStatusChange(nsIWebProgress *aWebProgress,
                           nsIRequest *aRequest, nsresult aStatus,
                           const PRUnichar *aMessage)
{   
  if (NS_FAILED(aStatus)) {
    mDownloadState = FAILED;
    nsCAutoString path;
    nsresult rv = mTarget->GetNativePath(path);
    if (NS_FAILED(rv)) return rv;

    mDownloadManager->DownloadEnded(path.get(), aMessage);
  }

  if (mListener)
    mListener->OnStatusChange(aWebProgress, aRequest, aStatus, aMessage);

  if (mDownloadManager->MustUpdateUI()) {
    nsCOMPtr<nsIDownloadProgressListener> internalListener;
    mDownloadManager->GetInternalListener(getter_AddRefs(internalListener));
    if (internalListener)
      internalListener->OnStatusChange(aWebProgress, aRequest, aStatus, aMessage, this);
  }

  if (mDialogListener)
    mDialogListener->OnStatusChange(aWebProgress, aRequest, aStatus, aMessage);

  return NS_OK;
}

NS_IMETHODIMP
nsDownload::OnStateChange(nsIWebProgress* aWebProgress,
                          nsIRequest* aRequest, PRUint32 aStateFlags,
                          nsresult aStatus)
{
  if (aStateFlags & STATE_START)
    mStartTime = PR_Now();

  if (mListener)
    mListener->OnStateChange(aWebProgress, aRequest, aStateFlags, aStatus);

  if (mDownloadManager->MustUpdateUI()) {
    nsCOMPtr<nsIDownloadProgressListener> internalListener;
    mDownloadManager->GetInternalListener(getter_AddRefs(internalListener));
    if (internalListener)
      internalListener->OnStateChange(aWebProgress, aRequest, aStateFlags, aStatus, this);
  }

  if (mDialogListener)
    mDialogListener->OnStateChange(aWebProgress, aRequest, aStateFlags, aStatus);

  if (aStateFlags & STATE_STOP) {
    if (mDownloadState == DOWNLOADING || mDownloadState == NOTSTARTED) {
      mDownloadState = FINISHED;
      mCurrBytes = mMaxBytes;
      mPercentComplete = 100;

      nsCAutoString path;
      nsresult rv = mTarget->GetNativePath(path);
      if (NS_FAILED(rv)) return rv;

      mDownloadManager->DownloadEnded(path.get(), nsnull);
    }

    // break the cycle we created in AddDownload
    if (mPersist)
      mPersist->SetProgressListener(nsnull);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDownload::OnSecurityChange(nsIWebProgress *aWebProgress,
                             nsIRequest *aRequest, PRUint32 aState)
{
  if (mListener)
    mListener->OnSecurityChange(aWebProgress, aRequest, aState);

  if (mDownloadManager->MustUpdateUI()) {
    nsCOMPtr<nsIDownloadProgressListener> internalListener;
    mDownloadManager->GetInternalListener(getter_AddRefs(internalListener));
    if (internalListener)
      internalListener->OnSecurityChange(aWebProgress, aRequest, aState, this);
  }

  if (mDialogListener)
    mDialogListener->OnSecurityChange(aWebProgress, aRequest, aState);

  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// nsIDownload

NS_IMETHODIMP
nsDownload::Init(nsIURI* aSource,
                 nsILocalFile* aTarget,
                 const PRUnichar* aDisplayName,
                 const PRUnichar* aOpeningWith,
                 PRInt64 aStartTime,
                 nsIWebBrowserPersist* aPersist)
{
  NS_WARNING("Huh...how did we get here?!");
  return NS_OK;
}

NS_IMETHODIMP
nsDownload::SetDisplayName(const PRUnichar* aDisplayName)
{
  mDisplayName = aDisplayName;

  nsCOMPtr<nsIRDFDataSource> ds;
  mDownloadManager->GetDataSource(getter_AddRefs(ds));

  nsCOMPtr<nsIRDFLiteral> nameLiteral;
  nsCOMPtr<nsIRDFResource> res;
  nsCAutoString path;
  nsresult rv = mTarget->GetNativePath(path);
  if (NS_FAILED(rv)) return rv;

  gRDFService->GetResource(path.get(), getter_AddRefs(res));
  
  gRDFService->GetLiteral(aDisplayName, getter_AddRefs(nameLiteral));
  ds->Assert(res, gNC_Name, nameLiteral, PR_TRUE);

  return NS_OK;
}

NS_IMETHODIMP
nsDownload::GetDisplayName(PRUnichar** aDisplayName)
{
  *aDisplayName = ToNewUnicode(mDisplayName);
  return NS_OK;
}

NS_IMETHODIMP
nsDownload::GetTarget(nsILocalFile** aTarget)
{
  *aTarget = mTarget;
  NS_IF_ADDREF(*aTarget);
  return NS_OK;
}

NS_IMETHODIMP
nsDownload::GetSource(nsIURI** aSource)
{
  *aSource = mSource;
  NS_IF_ADDREF(*aSource);
  return NS_OK;
}

NS_IMETHODIMP
nsDownload::GetPersist(nsIWebBrowserPersist** aPersist)
{
  *aPersist = mPersist;
  NS_IF_ADDREF(*aPersist);
  return NS_OK;
}

NS_IMETHODIMP
nsDownload::GetStartTime(PRInt64* aStartTime)
{
  *aStartTime = mStartTime;
  return NS_OK;
}

NS_IMETHODIMP
nsDownload::GetPercentComplete(PRInt32* aPercentComplete)
{
  *aPercentComplete = mPercentComplete;
  return NS_OK;
}

NS_IMETHODIMP
nsDownload::SetListener(nsIWebProgressListener* aListener)
{
  mListener = aListener;
  return NS_OK;
}

NS_IMETHODIMP
nsDownload::GetListener(nsIWebProgressListener** aListener)
{
  *aListener = mListener;
  NS_IF_ADDREF(*aListener);
  return NS_OK;
}

NS_IMETHODIMP
nsDownload::SetObserver(nsIObserver* aObserver)
{
  mObserver = aObserver;
  return NS_OK;
}

NS_IMETHODIMP
nsDownload::GetObserver(nsIObserver** aObserver)
{
  *aObserver = mObserver;
  NS_IF_ADDREF(*aObserver);
  return NS_OK;
}

NS_IMETHODIMP
nsDownload::GetOpeningWith(PRUnichar** aOpeningWith)
{
  *aOpeningWith = ToNewUnicode(mOpeningWith);
  return NS_OK;
}
