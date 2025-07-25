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
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *      Gagan Saksena <gagan@netscape.com>
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

/* build on macs with low memory */
#if defined(XP_MAC) && defined(MOZ_MAC_LOWMEM)
#pragma optimization_level 1
#endif

#include <string.h>
#include "nsCOMPtr.h"
#include "nsIFileSpec.h"
#include "nsSpecialSystemDirectory.h"
#include "nsIChromeRegistry.h"
#include "nsChromeRegistry.h"
#include "nsChromeUIDataSource.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFObserver.h"
#include "nsIRDFRemoteDataSource.h"
#include "nsIRDFXMLSink.h"
#include "nsCRT.h"
#include "rdf.h"
#include "nsIServiceManager.h"
#include "nsIRDFService.h"
#include "nsRDFCID.h"
#include "nsIRDFResource.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFContainer.h"
#include "nsIRDFContainerUtils.h"
#include "nsHashtable.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsXPIDLString.h"
#include "nsIStringBundle.h"
#include "nsISimpleEnumerator.h"
#include "nsNetUtil.h"
#include "nsIFileChannel.h"
#include "nsIXBLService.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDOMWindowCollection.h"
#include "nsIDOMLocation.h"
#include "nsIWindowMediator.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIXULPrototypeCache.h"
#include "nsIStyleSheet.h"
#include "nsIHTMLCSSStyleSheet.h"
#include "nsIHTMLStyleSheet.h"
#include "nsIHTMLContentContainer.h"
#include "nsIPresShell.h"
#include "nsIDocShell.h"
#include "nsIStyleSet.h"
#include "nsISupportsArray.h"
#include "nsICSSLoader.h"
#include "nsIDocumentObserver.h"
#include "nsIXULDocument.h"
#include "nsINameSpaceManager.h"
#include "nsIIOService.h"
#include "nsIResProtocolHandler.h"
#include "nsLayoutCID.h"
#include "nsGfxCIID.h"
#include "nsIBindingManager.h"
#include "prio.h"
#include "nsInt64.h"
#include "nsIDirectoryService.h"
#include "nsILocalFile.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsIPref.h"
#include "nsIObserverService.h"
#include "nsIDOMElement.h"
#include "nsIChromeEventHandler.h"
#include "nsIContent.h"
#include "nsIDOMWindowCollection.h"
#include "imgICache.h"

static char kChromePrefix[] = "chrome://";
static char kUseXBLFormsPref[] = "nglayout.debug.enable_xbl_forms";

#define kChromeFileName           NS_LITERAL_CSTRING("chrome.rdf")
#define kInstalledChromeFileName  NS_LITERAL_CSTRING("installed-chrome.txt")

static NS_DEFINE_CID(kWindowMediatorCID, NS_WINDOWMEDIATOR_CID);
static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFXMLDataSourceCID, NS_RDFXMLDATASOURCE_CID);
static NS_DEFINE_CID(kRDFContainerUtilsCID,      NS_RDFCONTAINERUTILS_CID);
static NS_DEFINE_CID(kCSSLoaderCID, NS_CSS_LOADER_CID);
static NS_DEFINE_CID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);

class nsChromeRegistry;

#define CHROME_URI "http://www.mozilla.org/rdf/chrome#"

DEFINE_RDF_VOCAB(CHROME_URI, CHROME, selectedSkin);
DEFINE_RDF_VOCAB(CHROME_URI, CHROME, selectedLocale);
DEFINE_RDF_VOCAB(CHROME_URI, CHROME, baseURL);
DEFINE_RDF_VOCAB(CHROME_URI, CHROME, packages);
DEFINE_RDF_VOCAB(CHROME_URI, CHROME, package);
DEFINE_RDF_VOCAB(CHROME_URI, CHROME, name);
DEFINE_RDF_VOCAB(CHROME_URI, CHROME, image);
DEFINE_RDF_VOCAB(CHROME_URI, CHROME, locType);
DEFINE_RDF_VOCAB(CHROME_URI, CHROME, allowScripts);
DEFINE_RDF_VOCAB(CHROME_URI, CHROME, hasOverlays);
DEFINE_RDF_VOCAB(CHROME_URI, CHROME, hasStylesheets);
DEFINE_RDF_VOCAB(CHROME_URI, CHROME, skinVersion);
DEFINE_RDF_VOCAB(CHROME_URI, CHROME, localeVersion);
DEFINE_RDF_VOCAB(CHROME_URI, CHROME, packageVersion);

////////////////////////////////////////////////////////////////////////////////

class nsOverlayEnumerator : public nsISimpleEnumerator
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISIMPLEENUMERATOR

  nsOverlayEnumerator(nsISimpleEnumerator *aInstallArcs,
                      nsISimpleEnumerator *aProfileArcs);
  virtual ~nsOverlayEnumerator();

private:
  nsCOMPtr<nsISimpleEnumerator> mInstallArcs;
  nsCOMPtr<nsISimpleEnumerator> mProfileArcs;
  nsCOMPtr<nsISimpleEnumerator> mCurrentArcs;
};

NS_IMPL_ISUPPORTS1(nsOverlayEnumerator, nsISimpleEnumerator)

nsOverlayEnumerator::nsOverlayEnumerator(nsISimpleEnumerator *aInstallArcs,
                                         nsISimpleEnumerator *aProfileArcs)
{
  NS_INIT_REFCNT();
  mInstallArcs = aInstallArcs;
  mProfileArcs = aProfileArcs;
  mCurrentArcs = mInstallArcs;
}

nsOverlayEnumerator::~nsOverlayEnumerator()
{
}

NS_IMETHODIMP nsOverlayEnumerator::HasMoreElements(PRBool *aIsTrue)
{
  *aIsTrue = PR_FALSE;
  if (!mProfileArcs) {
    if (!mInstallArcs)
      return NS_OK;   // No arcs period. We default to false,
    return mInstallArcs->HasMoreElements(aIsTrue); // no profile arcs. use install arcs,
  }

  nsresult rv = mProfileArcs->HasMoreElements(aIsTrue);
  if (*aIsTrue || !mInstallArcs)
    return rv;

  return mInstallArcs->HasMoreElements(aIsTrue);
}

NS_IMETHODIMP nsOverlayEnumerator::GetNext(nsISupports **aResult)
{
  nsresult rv;
  *aResult = nsnull;

  if (!mCurrentArcs) {
    mCurrentArcs = mInstallArcs;
    if (!mCurrentArcs)
      return NS_ERROR_FAILURE;
  }
  else if (mCurrentArcs == mProfileArcs) {
    PRBool hasMore;
    rv = mCurrentArcs->HasMoreElements(&hasMore);
    if (NS_FAILED(rv)) return rv;
    if (!hasMore)
      mCurrentArcs = mInstallArcs;
    if (!mInstallArcs)
      return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsISupports> supports;
  rv = mCurrentArcs->GetNext(getter_AddRefs(supports));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIRDFLiteral> value = do_QueryInterface(supports, &rv);
  if (NS_FAILED(rv))
    return NS_OK;

  const PRUnichar* valueStr;
  rv = value->GetValueConst(&valueStr);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIURI> url;
  rv = NS_NewURI(getter_AddRefs(url), NS_ConvertUCS2toUTF8(valueStr));

  if (NS_FAILED(rv))
    return NS_OK;

  nsCOMPtr<nsISupports> sup;
  sup = do_QueryInterface(url, &rv);
  if (NS_FAILED(rv))
    return NS_OK;

  *aResult = sup;
  NS_ADDREF(*aResult);

  return NS_OK;
}


////////////////////////////////////////////////////////////////////////////////

nsChromeRegistry::nsChromeRegistry()
{
  NS_INIT_REFCNT();

  mInstallInitialized = PR_FALSE;
  mProfileInitialized = PR_FALSE;

  mUseXBLForms = PR_FALSE;
  mBatchInstallFlushes = PR_FALSE;
  mRuntimeProvider = PR_FALSE;

  nsCOMPtr<nsIPref> prefService(do_GetService(kPrefServiceCID));
  if (prefService)
    prefService->GetBoolPref(kUseXBLFormsPref, &mUseXBLForms);

  mDataSourceTable = nsnull;
}


static PRBool PR_CALLBACK DatasourceEnumerator(nsHashKey *aKey, void *aData, void* closure)
{
    if (!closure || !aData)
        return PR_FALSE;

    nsIRDFCompositeDataSource* compositeDS = (nsIRDFCompositeDataSource*) closure;

    nsCOMPtr<nsISupports> supports = (nsISupports*)aData;

    nsCOMPtr<nsIRDFDataSource> dataSource = do_QueryInterface(supports);
    if (!dataSource)
        return PR_FALSE;

    nsresult rv = compositeDS->RemoveDataSource(dataSource);
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to RemoveDataSource");
    return PR_TRUE;
}


nsChromeRegistry::~nsChromeRegistry()
{
  if (mDataSourceTable) {
      mDataSourceTable->Enumerate(DatasourceEnumerator, mChromeDataSource);
      delete mDataSourceTable;
  }

  if (mRDFService) {
    nsServiceManager::ReleaseService(kRDFServiceCID, mRDFService);
    mRDFService = nsnull;
  }

  if (mRDFContainerUtils) {
    nsServiceManager::ReleaseService(kRDFContainerUtilsCID, mRDFContainerUtils);
    mRDFContainerUtils = nsnull;
  }

}

NS_IMPL_THREADSAFE_ISUPPORTS3(nsChromeRegistry, nsIChromeRegistry, nsIObserver, nsISupportsWeakReference);

////////////////////////////////////////////////////////////////////////////////
// nsIChromeRegistry methods:

nsresult
nsChromeRegistry::Init()
{
  nsresult rv;
  rv = nsServiceManager::GetService(kRDFServiceCID,
                                    NS_GET_IID(nsIRDFService),
                                    (nsISupports**)&mRDFService);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = nsServiceManager::GetService(kRDFContainerUtilsCID,
                                    NS_GET_IID(nsIRDFContainerUtils),
                                    (nsISupports**)&mRDFContainerUtils);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mRDFService->GetResource(kURICHROME_selectedSkin, getter_AddRefs(mSelectedSkin));
  NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF resource");

  rv = mRDFService->GetResource(kURICHROME_selectedLocale, getter_AddRefs(mSelectedLocale));
  NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF resource");

  rv = mRDFService->GetResource(kURICHROME_baseURL, getter_AddRefs(mBaseURL));
  NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF resource");

  rv = mRDFService->GetResource(kURICHROME_packages, getter_AddRefs(mPackages));
  NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF resource");

  rv = mRDFService->GetResource(kURICHROME_package, getter_AddRefs(mPackage));
  NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF resource");

  rv = mRDFService->GetResource(kURICHROME_name, getter_AddRefs(mName));
  NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF resource");

  rv = mRDFService->GetResource(kURICHROME_image, getter_AddRefs(mImage));
  NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF resource");

  rv = mRDFService->GetResource(kURICHROME_locType, getter_AddRefs(mLocType));
  NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF resource");

  rv = mRDFService->GetResource(kURICHROME_allowScripts, getter_AddRefs(mAllowScripts));
  NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF resource");

  rv = mRDFService->GetResource(kURICHROME_hasOverlays, getter_AddRefs(mHasOverlays));
  NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF resource");

  rv = mRDFService->GetResource(kURICHROME_hasStylesheets, getter_AddRefs(mHasStylesheets));
  NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF resource");

  rv = mRDFService->GetResource(kURICHROME_skinVersion, getter_AddRefs(mSkinVersion));
  NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF resource");

  rv = mRDFService->GetResource(kURICHROME_localeVersion, getter_AddRefs(mLocaleVersion));
  NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF resource");

  rv = mRDFService->GetResource(kURICHROME_packageVersion, getter_AddRefs(mPackageVersion));
  NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF resource");

  nsCOMPtr<nsIObserverService> observerService =
           do_GetService("@mozilla.org/observer-service;1", &rv);
  if (observerService) {
    observerService->AddObserver(this, "profile-before-change", PR_TRUE);
    observerService->AddObserver(this, "profile-after-change", PR_TRUE);
  }

  CheckForNewChrome();

  return NS_OK;
}


static nsresult
SplitURL(nsIURI *aChromeURI, nsCString& aPackage, nsCString& aProvider, nsCString& aFile,
         PRBool *aModified = nsnull)
{
  // Splits a "chrome:" URL into its package, provider, and file parts.
  // Here are the current portions of a
  // chrome: url that make up the chrome-
  //
  //     chrome://global/skin/foo?bar
  //     \------/ \----/\---/ \-----/
  //         |       |     |     |
  //         |       |     |     `-- RemainingPortion
  //         |       |     |
  //         |       |     `-- Provider
  //         |       |
  //         |       `-- Package
  //         |
  //         `-- Always "chrome://"
  //
  //

  nsresult rv;

  nsCAutoString str;
  rv = aChromeURI->GetSpec(str);
  if (NS_FAILED(rv)) return rv;

  // We only want to deal with "chrome:" URLs here. We could return
  // an error code if the URL isn't properly prefixed here...
  if (PL_strncmp(str.get(), kChromePrefix, sizeof(kChromePrefix) - 1) != 0)
    return NS_ERROR_INVALID_ARG;

  // Cull out the "package" string; e.g., "navigator"
  aPackage = str.get() + sizeof(kChromePrefix) - 1;

  PRInt32 idx;
  idx = aPackage.FindChar('/');
  if (idx < 0)
    return NS_OK;

  // Cull out the "provider" string; e.g., "content"
  aPackage.Right(aProvider, aPackage.Length() - (idx + 1));
  aPackage.Truncate(idx);

  idx = aProvider.FindChar('/');
  if (idx < 0) {
    // Force the provider to end with a '/'
    idx = aProvider.Length();
    aProvider.Append('/');
  }

  // Cull out the "file"; e.g., "navigator.xul"
  aProvider.Right(aFile, aProvider.Length() - (idx + 1));
  aProvider.Truncate(idx);

  PRBool nofile = (aFile.Length() == 0);
  if (nofile) {
    // If there is no file, then construct the default file
    aFile = aPackage;

    if (aProvider.Equals("content")) {
      aFile += ".xul";
    }
    else if (aProvider.Equals("skin")) {
      aFile += ".css";
    }
    else if (aProvider.Equals("locale")) {
      aFile += ".dtd";
    }
    else {
      NS_ERROR("unknown provider");
      return NS_ERROR_FAILURE;
    }
  } else {
    // Protect against URIs containing .. that reach up out of the
    // chrome directory to grant chrome privileges to non-chrome files.
    int depth = 0;
    PRBool sawSlash = PR_TRUE;  // .. at the beginning is suspect as well as /..
    for (const char* p=aFile.get(); *p; p++) {
      if (sawSlash) {
        if (p[0] == '.' && p[1] == '.'){
          depth--;    // we have /.., decrement depth.
        } else {
          static const char escape[] = "%2E%2E";
          if (PL_strncasecmp(p, escape, sizeof(escape)-1) == 0)
            depth--;   // we have the HTML-escaped form of /.., decrement depth.
        }
      } else if (p[0] != '/') {
        depth++;        // we have /x for some x that is not /
      }
      sawSlash = (p[0] == '/');

      if (depth < 0) {
        return NS_ERROR_FAILURE;
      }
    }
  }
  if (aModified)
    *aModified = nofile;
  return NS_OK;
}


NS_IMETHODIMP
nsChromeRegistry::Canonify(nsIURI* aChromeURI)
{
  // Canonicalize 'chrome:' URLs. We'll take any 'chrome:' URL
  // without a filename, and change it to a URL -with- a filename;
  // e.g., "chrome://navigator/content" to
  // "chrome://navigator/content/navigator.xul".
  if (! aChromeURI)
      return NS_ERROR_NULL_POINTER;

  PRBool modified = PR_TRUE; // default is we do canonification
  nsCAutoString package, provider, file;
  nsresult rv;
  rv = SplitURL(aChromeURI, package, provider, file, &modified);
  if (NS_FAILED(rv))
    return rv;

  if (!modified)
    return NS_OK;

  nsCAutoString canonical( kChromePrefix );
  canonical += package;
  canonical += "/";
  canonical += provider;
  canonical += "/";
  canonical += file;

  return aChromeURI->SetSpec(canonical);
}

NS_IMETHODIMP
nsChromeRegistry::ConvertChromeURL(nsIURI* aChromeURL, char** aResult)
{
  nsresult rv = NS_OK;
  NS_ASSERTION(aChromeURL, "null url!");
  if (!aChromeURL)
      return NS_ERROR_NULL_POINTER;

  // No need to canonify as the SplitURL() that we
  // do is the equivalent of canonification without modifying
  // aChromeURL

  // Obtain the package, provider and remaining from the URL
  nsCAutoString package, provider, remaining;

  rv = SplitURL(aChromeURL, package, provider, remaining);
  if (NS_FAILED(rv)) return rv;

  // Try for the profile data source first because it
  // will load the install data source as well.
  if (!mProfileInitialized) {
    rv = LoadProfileDataSource();
    if (NS_FAILED(rv)) return rv;
  }
  if (!mInstallInitialized) {
    rv = LoadInstallDataSource();
    if (NS_FAILED(rv)) return rv;
  }

  nsCAutoString finalURL;
  rv = GetBaseURL(package, provider, finalURL);
#ifdef DEBUG
  if (NS_FAILED(rv)) {
    nsCAutoString msg("chrome: failed to get base url");
    nsCAutoString url;
    rv = aChromeURL->GetSpec(url);
    if (NS_SUCCEEDED(rv)) {
      msg += " for ";
      msg += url.get();
    }
    msg += " -- using wacky default";
    NS_WARNING(msg.get());
  }
#endif
  if (finalURL.IsEmpty()) {
    // hard-coded fallback
    if (provider.Equals("skin")) {
      finalURL = "resource:/chrome/skins/classic/";
    }
    else if (provider.Equals("locale")) {
      finalURL = "resource:/chrome/locales/en-US/";
    }
    else if (package.Equals("aim")) {
      finalURL = "resource:/chrome/packages/aim/";
    }
    else if (package.Equals("messenger")) {
      finalURL = "resource:/chrome/packages/messenger/";
    }
    else if (package.Equals("global")) {
      finalURL = "resource:/chrome/packages/widget-toolkit/";
    }
    else {
      finalURL = "resource:/chrome/packages/core/";
    }
  }

  *aResult = ToNewCString(finalURL + remaining);

  return NS_OK;
}

NS_IMETHODIMP
nsChromeRegistry::GetBaseURL(const nsCString& aPackage, const nsCString& aProvider,
                             nsCString& aBaseURL)
{
  nsCOMPtr<nsIRDFResource> resource;

  nsCAutoString resourceStr("urn:mozilla:package:");
  resourceStr += aPackage;

  // Obtain the resource.
  nsresult rv = NS_OK;
  nsCOMPtr<nsIRDFResource> packageResource;
  rv = GetResource(resourceStr, getter_AddRefs(packageResource));
  if (NS_FAILED(rv)) {
    NS_ERROR("Unable to obtain the package resource.");
    return rv;
  }

  // Follow the "selectedSkin" or "selectedLocale" arc.
  nsCOMPtr<nsIRDFResource> arc;
  if (aProvider.Equals(nsCAutoString("skin"))) {
    arc = mSelectedSkin;
  }
  else if (aProvider.Equals(nsCAutoString("locale"))) {
    arc = mSelectedLocale;
  }
  else
    // We're a package.
    resource = packageResource;

  if (arc) {

    nsCOMPtr<nsIRDFNode> selectedProvider;
    if (NS_FAILED(rv = mChromeDataSource->GetTarget(packageResource, arc, PR_TRUE, getter_AddRefs(selectedProvider)))) {
      NS_ERROR("Unable to obtain the provider.");
      return rv;
    }

    resource = do_QueryInterface(selectedProvider);

    if (resource) {
      // We found a selected provider, but now we need to verify that the version
      // specified by the package and the version specified by the provider are
      // one and the same.  If they aren't, then we cannot use this provider.
      nsCOMPtr<nsIRDFResource> versionArc;
      if (arc == mSelectedSkin)
        versionArc = mSkinVersion;
      else // Locale arc
        versionArc = mLocaleVersion;

      nsCAutoString packageVersion;
      nsChromeRegistry::FollowArc(mChromeDataSource, packageVersion, packageResource, versionArc);
      if (!packageVersion.IsEmpty()) {
        // The package only wants providers (skins) that say they can work with it.  Let's find out
        // if our provider (skin) can work with it.
        nsCAutoString providerVersion;
        nsChromeRegistry::FollowArc(mChromeDataSource, providerVersion, resource, versionArc);
        if (!providerVersion.Equals(packageVersion))
          selectedProvider = nsnull;
      }
    }

    if (!selectedProvider) {
      // Find provider will attempt to auto-select a version-compatible provider (skin).  If none
      // exist it will return nsnull in the selectedProvider variable.
      FindProvider(aPackage, aProvider, arc, getter_AddRefs(selectedProvider));
      resource = do_QueryInterface(selectedProvider);
    }

    if (!selectedProvider)
      return rv;

    if (!resource)
      return NS_ERROR_FAILURE;
  }

  // From this resource, follow the "baseURL" arc.
  return nsChromeRegistry::FollowArc(mChromeDataSource,
                                     aBaseURL,
                                     resource,
                                     mBaseURL);
}

// locate
NS_IMETHODIMP
nsChromeRegistry::FindProvider(const nsCString& aPackage,
                               const nsCString& aProvider,
                               nsIRDFResource *aArc,
                               nsIRDFNode **aSelectedProvider)
{
  *aSelectedProvider = nsnull;

  nsCAutoString rootStr("urn:mozilla:");
  nsresult rv = NS_OK;

  rootStr += aProvider;
  rootStr += ":root";

  // obtain the provider root resource
  nsCOMPtr<nsIRDFResource> resource;
  rv = GetResource(rootStr, getter_AddRefs(resource));
  if (NS_FAILED(rv)) {
    NS_ERROR("Unable to obtain the provider root resource.");
    return rv;
  }

  // wrap it in a container
  nsCOMPtr<nsIRDFContainer> container;
  rv = nsComponentManager::CreateInstance("@mozilla.org/rdf/container;1",
                                          nsnull,
                                          NS_GET_IID(nsIRDFContainer),
                                          getter_AddRefs(container));
  if (NS_FAILED(rv)) return rv;

  rv = container->Init(mChromeDataSource, resource);
  if (NS_FAILED(rv)) return rv;

  // step through its (seq) arcs
  nsCOMPtr<nsISimpleEnumerator> arcs;
  rv = container->GetElements(getter_AddRefs(arcs));
  if (NS_FAILED(rv)) return rv;

  PRBool moreElements;
  rv = arcs->HasMoreElements(&moreElements);
  if (NS_FAILED(rv)) return rv;
  for ( ; moreElements; arcs->HasMoreElements(&moreElements)) {

    // get next arc resource
    nsCOMPtr<nsISupports> supports;
    rv = arcs->GetNext(getter_AddRefs(supports));
    if (NS_FAILED(rv)) return rv;
    nsCOMPtr<nsIRDFResource> kid = do_QueryInterface(supports);

    if (kid) {
      // get its name
      nsCAutoString providerName;
      rv = nsChromeRegistry::FollowArc(mChromeDataSource, providerName, kid, mName);
      if (NS_FAILED(rv)) return rv;

      // get its package list
      nsCOMPtr<nsIRDFNode> packageNode;
      nsCOMPtr<nsIRDFResource> packageList;
      rv = mChromeDataSource->GetTarget(kid, mPackages, PR_TRUE, getter_AddRefs(packageNode));
      if (NS_SUCCEEDED(rv))
        packageList = do_QueryInterface(packageNode);
      if (!packageList)
        continue;

      // if aPackage is named in kid's package list, select it and we're done
      rv = SelectPackageInProvider(packageList, aPackage, aProvider, providerName,
                                   aArc, aSelectedProvider);
      if (NS_FAILED(rv))
        continue; // Don't let this be disastrous.  We may find another acceptable match.

      if (*aSelectedProvider)
        return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsChromeRegistry::SelectPackageInProvider(nsIRDFResource *aPackageList,
                                          const nsCString& aPackage,
                                          const nsCString& aProvider,
                                          const nsCString& aProviderName,
                                          nsIRDFResource *aArc,
                                          nsIRDFNode **aSelectedProvider)
{
  *aSelectedProvider = nsnull;

  nsresult rv = NS_OK;

  // wrap aPackageList in a container
  nsCOMPtr<nsIRDFContainer> container;
  rv = nsComponentManager::CreateInstance("@mozilla.org/rdf/container;1",
                                          nsnull,
                                          NS_GET_IID(nsIRDFContainer),
                                          getter_AddRefs(container));
  if (NS_SUCCEEDED(rv))
    rv = container->Init(mChromeDataSource, aPackageList);
  if (NS_FAILED(rv))
    return rv;

  // step through its (seq) arcs
  nsCOMPtr<nsISimpleEnumerator> arcs;
  rv = container->GetElements(getter_AddRefs(arcs));
  if (NS_FAILED(rv)) return rv;

  PRBool moreElements;
  rv = arcs->HasMoreElements(&moreElements);
  if (NS_FAILED(rv)) return rv;
  for ( ; moreElements; arcs->HasMoreElements(&moreElements)) {

    // get next arc resource
    nsCOMPtr<nsISupports> supports;
    rv = arcs->GetNext(getter_AddRefs(supports));
    if (NS_FAILED(rv)) return rv;
    nsCOMPtr<nsIRDFResource> kid = do_QueryInterface(supports);

    if (kid) {
      // get its package resource
      nsCOMPtr<nsIRDFNode> packageNode;
      nsCOMPtr<nsIRDFResource> package;
      rv = mChromeDataSource->GetTarget(kid, mPackage, PR_TRUE, getter_AddRefs(packageNode));
      if (NS_SUCCEEDED(rv))
        package = do_QueryInterface(packageNode);
      if (!package)
        continue;

      // get its name
      nsCAutoString packageName;
      rv = nsChromeRegistry::FollowArc(mChromeDataSource, packageName, package, mName);
      if (NS_FAILED(rv))
        continue;       // don't fail if package has not yet been installed

      if (packageName.Equals(aPackage)) {
        PRBool useProfile = !mProfileRoot.IsEmpty();
        if (packageName.Equals("global") || packageName.Equals("communicator"))
          useProfile = PR_FALSE; // Always force the auto-selection to be in the
                                 // install dir for the packages required to bring up the profile UI.
        rv = SelectProviderForPackage(aProvider,
                                      NS_ConvertASCIItoUCS2(aProviderName).get(),
                                      NS_ConvertASCIItoUCS2(packageName).get(),
                                      aArc, useProfile, PR_TRUE);
        if (NS_FAILED(rv))
          return NS_ERROR_FAILURE;

        *aSelectedProvider = kid;
        NS_ADDREF(*aSelectedProvider);
        return NS_OK;
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP nsChromeRegistry::GetDynamicDataSource(nsIURI *aChromeURL, PRBool aIsOverlay, PRBool aUseProfile,
                                                     PRBool aCreateDS, nsIRDFDataSource **aResult)
{
  *aResult = nsnull;

  nsresult rv;

  if (!mDataSourceTable)
    return NS_OK;

  // Obtain the package, provider and remaining from the URL
  nsCAutoString package, provider, remaining;

  rv = SplitURL(aChromeURL, package, provider, remaining);
  if (NS_FAILED(rv)) return rv;

  if (!aCreateDS) {
    // We are not supposed to create the data source, which means
    // we should first check our chrome.rdf file to see if this
    // package even has dynamic data.  Only if it claims to have
    // dynamic data are we willing to hand back a datasource.
    nsDependentCString dataSourceStr(kChromeFileName);
    nsCOMPtr<nsIRDFDataSource> mainDataSource;
    rv = LoadDataSource(dataSourceStr, getter_AddRefs(mainDataSource), aUseProfile, nsnull);
    if (NS_FAILED(rv)) return rv;
    
    // Now that we have the appropriate chrome.rdf file, we
    // must check the package resource for stylesheets or overlays.
    nsCOMPtr<nsIRDFResource> hasDynamicDataArc;
    if (aIsOverlay)
      hasDynamicDataArc = mHasOverlays;
    else
      hasDynamicDataArc = mHasStylesheets;
    
    // Obtain the resource for the package.
    nsCAutoString packageResourceStr("urn:mozilla:package:");
    packageResourceStr += package;
    nsCOMPtr<nsIRDFResource> packageResource;
    GetResource(packageResourceStr, getter_AddRefs(packageResource));
    
    // Follow the dynamic data arc to see if we should continue.
    // Only if it claims to have dynamic data do we even bother.
    nsCAutoString hasDynamicDS;
    nsChromeRegistry::FollowArc(mainDataSource, hasDynamicDS, 
                                packageResource, hasDynamicDataArc);
    if (hasDynamicDS.IsEmpty())
      return NS_OK; // No data source exists.
  }

  // Retrieve the mInner data source.
  nsCAutoString overlayFile( "overlayinfo/" );
  overlayFile += package;
  overlayFile += "/";

  if (aIsOverlay)
    overlayFile += "content/overlays.rdf";
  else overlayFile += "skin/stylesheets.rdf";

  return LoadDataSource(overlayFile, aResult, aUseProfile, nsnull);
}

NS_IMETHODIMP nsChromeRegistry::GetStyleSheets(nsIURI *aChromeURL, nsISupportsArray **aResult)
{
  *aResult = nsnull;

  nsCOMPtr<nsISimpleEnumerator> sheets;
  nsresult rv = GetDynamicInfo(aChromeURL, PR_FALSE, getter_AddRefs(sheets));
  if (NS_FAILED(rv) || !sheets) return rv;
  PRBool hasMore;
  rv = sheets->HasMoreElements(&hasMore);
  if (NS_FAILED(rv)) return rv;
  while (hasMore) {
    if (!*aResult) {
      rv = NS_NewISupportsArray(aResult);
      if (NS_FAILED(rv)) return rv;
    }

    nsCOMPtr<nsISupports> supp;
    rv = sheets->GetNext(getter_AddRefs(supp));
    if (NS_FAILED(rv)) return rv;
    nsCOMPtr<nsIURL> url(do_QueryInterface(supp));
    if (url) {
      nsCOMPtr<nsICSSStyleSheet> sheet;
      nsCAutoString str;
      rv = url->GetSpec(str);
      if (NS_FAILED(rv)) return rv;
      rv = LoadStyleSheet(getter_AddRefs(sheet), str);
      if (NS_FAILED(rv)) return rv;
      rv = (*aResult)->AppendElement(sheet) ? NS_OK : NS_ERROR_FAILURE;
      if (NS_FAILED(rv)) return rv;
    }
    sheets->HasMoreElements(&hasMore);
  }
  return NS_OK;
}

NS_IMETHODIMP nsChromeRegistry::GetOverlays(nsIURI *aChromeURL, nsISimpleEnumerator **aResult)
{
  return GetDynamicInfo(aChromeURL, PR_TRUE, aResult);
}

NS_IMETHODIMP nsChromeRegistry::GetDynamicInfo(nsIURI *aChromeURL, PRBool aIsOverlay, nsISimpleEnumerator **aResult)
{
  *aResult = nsnull;

  nsresult rv;

  if (!mDataSourceTable)
    return NS_OK;

  nsCOMPtr<nsIRDFDataSource> installSource;
  rv = GetDynamicDataSource(aChromeURL, aIsOverlay, PR_FALSE, PR_FALSE, getter_AddRefs(installSource));
  if (NS_FAILED(rv)) return rv;
  nsCOMPtr<nsIRDFDataSource> profileSource;
  if (mProfileInitialized) {
    rv = GetDynamicDataSource(aChromeURL, aIsOverlay, PR_TRUE, PR_FALSE, getter_AddRefs(profileSource));
    if (NS_FAILED(rv)) return rv;
  }

  nsCAutoString lookup;
  rv = aChromeURL->GetSpec(lookup);
  if (NS_FAILED(rv)) return rv;

  // Get the chromeResource from this lookup string
  nsCOMPtr<nsIRDFResource> chromeResource;
  rv = GetResource(lookup, getter_AddRefs(chromeResource));
  if (NS_FAILED(rv)) {
      NS_ERROR("Unable to retrieve the resource corresponding to the chrome skin or content.");
      return rv;
  }

  nsCOMPtr<nsISimpleEnumerator> installArcs;
  nsCOMPtr<nsISimpleEnumerator> profileArcs;

  if (installSource)
  {
    nsCOMPtr<nsIRDFContainer> container;
    rv = nsComponentManager::CreateInstance("@mozilla.org/rdf/container;1",
                                            nsnull,
                                            NS_GET_IID(nsIRDFContainer),
                                            getter_AddRefs(container));
    if (NS_SUCCEEDED(rv) && NS_SUCCEEDED(container->Init(installSource, chromeResource)))
      rv = container->GetElements(getter_AddRefs(installArcs));
    if (NS_FAILED(rv)) return rv;
  }

  if (profileSource)
  {
    nsCOMPtr<nsIRDFContainer> container;
    rv = nsComponentManager::CreateInstance("@mozilla.org/rdf/container;1",
                                            nsnull,
                                            NS_GET_IID(nsIRDFContainer),
                                            getter_AddRefs(container));
    if (NS_SUCCEEDED(rv) && NS_SUCCEEDED(container->Init(profileSource, chromeResource)))
      rv = container->GetElements(getter_AddRefs(profileArcs));
    if (NS_FAILED(rv)) return rv;
  }


  *aResult = new nsOverlayEnumerator(installArcs, profileArcs);
  NS_ADDREF(*aResult);

  return NS_OK;
}

NS_IMETHODIMP nsChromeRegistry::LoadDataSource(const nsACString &aFileName,
                                               nsIRDFDataSource **aResult,
                                               PRBool aUseProfileDir,
                                               const char *aProfilePath)
{
  // Init the data source to null.
  *aResult = nsnull;

  nsCAutoString key;

  // Try the profile root first.
  if (aUseProfileDir) {
    // use given profile path if non-null
    if (aProfilePath) {
      key = aProfilePath;
      key += "chrome/";
    }
    else
      key = mProfileRoot;

    key += aFileName;
  }
  else {
    key = mInstallRoot;
    key += aFileName;
  }

  if (mDataSourceTable)
  {
    nsCStringKey skey(key);
    nsCOMPtr<nsISupports> supports =
      getter_AddRefs(NS_STATIC_CAST(nsISupports*, mDataSourceTable->Get(&skey)));

    if (supports)
    {
      nsCOMPtr<nsIRDFDataSource> dataSource = do_QueryInterface(supports);
      if (dataSource)
      {
        *aResult = dataSource;
        NS_ADDREF(*aResult);
        return NS_OK;
      }
      return NS_ERROR_FAILURE;
    }
  }

  nsresult rv = nsComponentManager::CreateInstance(kRDFXMLDataSourceCID,
                                                     nsnull,
                                                     NS_GET_IID(nsIRDFDataSource),
                                                     (void**) aResult);
  if (NS_FAILED(rv)) return rv;

  // Seed the datasource with the ``chrome'' namespace
  nsCOMPtr<nsIRDFXMLSink> sink = do_QueryInterface(*aResult);
  if (sink) {
    nsCOMPtr<nsIAtom> prefix = getter_AddRefs(NS_NewAtom("c"));
    sink->AddNameSpace(prefix, NS_ConvertASCIItoUCS2(CHROME_URI));
  }

  nsCOMPtr<nsIRDFRemoteDataSource> remote = do_QueryInterface(*aResult);
  if (! remote)
      return NS_ERROR_UNEXPECTED;

  if (!mDataSourceTable)
    mDataSourceTable = new nsSupportsHashtable;

  // We need to read this synchronously.
  rv = remote->Init(key.get());
  if (NS_SUCCEEDED(rv))
    rv = remote->Refresh(PR_TRUE);

  nsCOMPtr<nsISupports> supports = do_QueryInterface(remote);
  nsCStringKey skey(key);
  mDataSourceTable->Put(&skey, supports.get());

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

nsresult
nsChromeRegistry::GetResource(const nsCString& aURL,
                              nsIRDFResource** aResult)
{
  nsresult rv = NS_OK;
  if (NS_FAILED(rv = mRDFService->GetResource(aURL.get(), aResult))) {
    NS_ERROR("Unable to retrieve a resource for this URL.");
    *aResult = nsnull;
    return rv;
  }
  return NS_OK;
}

nsresult
nsChromeRegistry::FollowArc(nsIRDFDataSource *aDataSource,
                            nsCString& aResult,
                            nsIRDFResource* aChromeResource,
                            nsIRDFResource* aProperty)
{
  if (!aDataSource)
    return NS_ERROR_FAILURE;

  nsresult rv;

  nsCOMPtr<nsIRDFNode> chromeBase;
  rv = aDataSource->GetTarget(aChromeResource, aProperty, PR_TRUE, getter_AddRefs(chromeBase));
  if (NS_FAILED(rv)) {
    NS_ERROR("Unable to obtain a base resource.");
    return rv;
  }

  if (chromeBase == nsnull)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIRDFResource> resource(do_QueryInterface(chromeBase));

  if (resource) {
    nsXPIDLCString uri;
    rv = resource->GetValue(getter_Copies(uri));
    if (NS_FAILED(rv)) return rv;
    aResult.Assign(uri);
    return NS_OK;
  }

  nsCOMPtr<nsIRDFLiteral> literal(do_QueryInterface(chromeBase));
  if (literal) {
    const PRUnichar *s;
    rv = literal->GetValueConst(&s);
    if (NS_FAILED(rv)) return rv;
    aResult.AssignWithConversion(s);
  }
  else {
    // This should _never_ happen.
    NS_ERROR("uh, this isn't a resource or a literal!");
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

nsresult
nsChromeRegistry::UpdateArc(nsIRDFDataSource *aDataSource, nsIRDFResource* aSource,
                            nsIRDFResource* aProperty,
                            nsIRDFNode *aTarget, PRBool aRemove)
{
  nsresult rv;
  // Get the old targets
  nsCOMPtr<nsIRDFNode> retVal;
  rv = aDataSource->GetTarget(aSource, aProperty, PR_TRUE, getter_AddRefs(retVal));
  if (NS_FAILED(rv)) return rv;

  if (retVal) {
    if (!aRemove)
      aDataSource->Change(aSource, aProperty, retVal, aTarget);
    else
      aDataSource->Unassert(aSource, aProperty, aTarget);
  }
  else if (!aRemove)
    aDataSource->Assert(aSource, aProperty, aTarget, PR_TRUE);

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////

// theme stuff


static void FlushSkinBindingsForWindow(nsIDOMWindowInternal* aWindow)
{
  // Get the DOM document.
  nsCOMPtr<nsIDOMDocument> domDocument;
  aWindow->GetDocument(getter_AddRefs(domDocument));
  if (!domDocument)
    return;

  nsCOMPtr<nsIDocument> document = do_QueryInterface(domDocument);
  if (!document)
    return;

  // Annihilate all XBL bindings.
  nsCOMPtr<nsIBindingManager> bindingManager;
  document->GetBindingManager(getter_AddRefs(bindingManager));
  bindingManager->FlushSkinBindings();
}

NS_IMETHODIMP nsChromeRegistry::RefreshSkins()
{
  nsCOMPtr<nsIWindowMediator> windowMediator(do_GetService(kWindowMediatorCID));
  if (!windowMediator)
    return NS_OK;

  nsCOMPtr<nsISimpleEnumerator> windowEnumerator;
  windowMediator->GetEnumerator(nsnull, getter_AddRefs(windowEnumerator));
  PRBool more;
  windowEnumerator->HasMoreElements(&more);
  while (more) {
    nsCOMPtr<nsISupports> protoWindow;
    windowEnumerator->GetNext(getter_AddRefs(protoWindow));
    if (protoWindow) {
      nsCOMPtr<nsIDOMWindowInternal> domWindow = do_QueryInterface(protoWindow);
      if (domWindow)
        FlushSkinBindingsForWindow(domWindow);
    }
    windowEnumerator->HasMoreElements(&more);
  }

  FlushCaches();
  
  windowMediator->GetEnumerator(nsnull, getter_AddRefs(windowEnumerator));
  windowEnumerator->HasMoreElements(&more);
  while (more) {
    nsCOMPtr<nsISupports> protoWindow;
    windowEnumerator->GetNext(getter_AddRefs(protoWindow));
    if (protoWindow) {
      nsCOMPtr<nsIDOMWindowInternal> domWindow = do_QueryInterface(protoWindow);
      if (domWindow)
        RefreshWindow(domWindow);
    }
    windowEnumerator->HasMoreElements(&more);
  }
   
  return NS_OK;
}


nsresult nsChromeRegistry::FlushCaches()
{
  nsresult rv;

  // Flush the style sheet cache completely.
  nsCOMPtr<nsIXULPrototypeCache> xulCache =
           do_GetService("@mozilla.org/xul/xul-prototype-cache;1", &rv);
  if (NS_SUCCEEDED(rv) && xulCache)
    xulCache->FlushSkinFiles();

  // Flush the new imagelib image chrome cache.
  nsCOMPtr<imgICache> imageCache(do_GetService("@mozilla.org/image/cache;1", &rv));
  if (NS_SUCCEEDED(rv) && imageCache) {
    imageCache->ClearCache(PR_TRUE);
  }

  return rv;
}

static PRBool IsChromeURI(nsIURI* aURI)
{
    PRBool isChrome=PR_FALSE;
    if (NS_SUCCEEDED(aURI->SchemeIs("chrome", &isChrome)) && isChrome)
        return PR_TRUE;
    return PR_FALSE;
}

NS_IMETHODIMP nsChromeRegistry::RefreshWindow(nsIDOMWindowInternal* aWindow)
{
  // Deal with our subframes first.
  nsCOMPtr<nsIDOMWindowCollection> frames;
  aWindow->GetFrames(getter_AddRefs(frames));
  PRUint32 length;
  frames->GetLength(&length);
  for (PRUint32 i = 0; i < length; i++) {
    nsCOMPtr<nsIDOMWindow> childWin;
    frames->Item(i, getter_AddRefs(childWin));
    nsCOMPtr<nsIDOMWindowInternal> childInt(do_QueryInterface(childWin));
    RefreshWindow(childInt);
  }

  nsresult rv;
  // Get the DOM document.
  nsCOMPtr<nsIDOMDocument> domDocument;
  aWindow->GetDocument(getter_AddRefs(domDocument));
  if (!domDocument)
    return NS_OK;

  nsCOMPtr<nsIDocument> document = do_QueryInterface(domDocument);
  if (!document)
    return NS_OK;

  // Deal with the agent sheets first.
  PRInt32 shellCount = document->GetNumberOfShells();
  for (PRInt32 k = 0; k < shellCount; k++) {
    nsCOMPtr<nsIPresShell> shell;
    document->GetShellAt(k, getter_AddRefs(shell));
    if (shell) {
      nsCOMPtr<nsIStyleSet> styleSet;
      rv = shell->GetStyleSet(getter_AddRefs(styleSet));
      if (NS_FAILED(rv)) return rv;
      if (styleSet) {
        // Reload only the chrome URL agent style sheets.
        nsCOMPtr<nsISupportsArray> agents;
        rv = NS_NewISupportsArray(getter_AddRefs(agents));
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsISupportsArray> newAgentSheets;
        rv = NS_NewISupportsArray(getter_AddRefs(newAgentSheets));
        if (NS_FAILED(rv)) return rv;

        PRInt32 bc = styleSet->GetNumberOfAgentStyleSheets();
        for (PRInt32 l = 0; l < bc; l++) {
          nsCOMPtr<nsIStyleSheet> sheet = getter_AddRefs(styleSet->GetAgentStyleSheetAt(l));
          nsCOMPtr<nsIURI> uri;
          rv = sheet->GetURL(*getter_AddRefs(uri));
          if (NS_FAILED(rv)) return rv;

          if (IsChromeURI(uri)) {
            // Reload the sheet.
            nsCOMPtr<nsICSSStyleSheet> newSheet;
            rv = LoadStyleSheetWithURL(uri, getter_AddRefs(newSheet));
            if (NS_FAILED(rv)) return rv;
            if (newSheet) {
              rv = newAgentSheets->AppendElement(newSheet) ? NS_OK : NS_ERROR_FAILURE;
              if (NS_FAILED(rv)) return rv;
            }
          }
          else {  // Just use the same sheet.
            rv = newAgentSheets->AppendElement(sheet) ? NS_OK : NS_ERROR_FAILURE;
            if (NS_FAILED(rv)) return rv;
          }
        }

        styleSet->ReplaceAgentStyleSheets(newAgentSheets);
      }
    }
    
    nsCOMPtr<nsIHTMLContentContainer> container = do_QueryInterface(document);
    nsCOMPtr<nsICSSLoader> cssLoader;
    rv = container->GetCSSLoader(*getter_AddRefs(cssLoader));
    if (NS_FAILED(rv)) return rv;

    // Build an array of nsIURIs of style sheets we need to load.
    nsCOMPtr<nsISupportsArray> oldSheets;
    rv = NS_NewISupportsArray(getter_AddRefs(oldSheets));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsISupportsArray> newSheets;
    rv = NS_NewISupportsArray(getter_AddRefs(newSheets));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIHTMLStyleSheet> attrSheet;
    rv = container->GetAttributeStyleSheet(getter_AddRefs(attrSheet));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIHTMLCSSStyleSheet> inlineSheet;
    rv = container->GetInlineStyleSheet(getter_AddRefs(inlineSheet));
    if (NS_FAILED(rv)) return rv;

    PRInt32 count = 0;
    document->GetNumberOfStyleSheets(&count);

    // Iterate over the style sheets.
    PRUint32 i;
    for (i = 0; i < (PRUint32)count; i++) {
      // Get the style sheet
      nsCOMPtr<nsIStyleSheet> styleSheet;
      document->GetStyleSheetAt(i, getter_AddRefs(styleSheet));

      // Make sure we aren't the special style sheets that never change.  We
      // want to skip those.

      nsCOMPtr<nsIStyleSheet> attr = do_QueryInterface(attrSheet);
      nsCOMPtr<nsIStyleSheet> inl = do_QueryInterface(inlineSheet);
      if ((attr.get() != styleSheet.get()) &&
          (inl.get() != styleSheet.get()))
        // Add this sheet to the list of old style sheets.
        oldSheets->AppendElement(styleSheet);
    }

    // Iterate over our old sheets and kick off a sync load of the new 
    // sheet if and only if it's a chrome URL.
    PRUint32 oldCount;
    oldSheets->Count(&oldCount);
    for (i = 0; i < oldCount; i++) {
      nsCOMPtr<nsISupports> supp = getter_AddRefs(oldSheets->ElementAt(i));
      nsCOMPtr<nsIStyleSheet> sheet(do_QueryInterface(supp));
      nsCOMPtr<nsIURI> uri;
      rv = sheet->GetURL(*getter_AddRefs(uri));
      if (NS_FAILED(rv)) return rv;

      if (IsChromeURI(uri)) {
        // Reload the sheet.
        nsCOMPtr<nsICSSStyleSheet> newSheet;
        LoadStyleSheetWithURL(uri, getter_AddRefs(newSheet));
        if (newSheet)
          newSheets->AppendElement(newSheet);
      }
      else  // Just use the same sheet.
        newSheets->AppendElement(sheet);
    }

    // Now notify the document that multiple sheets have been added and removed.
    document->UpdateStyleSheets(oldSheets, newSheets);
  }

  return NS_OK;
}

NS_IMETHODIMP nsChromeRegistry::WriteInfoToDataSource(const char *aDocURI,
                                                      const PRUnichar *aOverlayURI,
                                                      PRBool aIsOverlay,
                                                      PRBool aUseProfile,
                                                      PRBool aRemove)
{
  nsresult rv;
  nsCOMPtr<nsIURI> uri;
  nsCAutoString str(aDocURI);
  rv = NS_NewURI(getter_AddRefs(uri), str);
  if (NS_FAILED(rv)) return rv;

  if (!aRemove) {
    // We are installing a dynamic overlay or package.  
    // We must split the doc URI and obtain our package or skin.
    // We then annotate the chrome.rdf datasource in the appropriate
    // install/profile dir (based off aUseProfile) with the knowledge
    // that we have overlays or stylesheets.
    nsCAutoString package, provider, file;
    rv = SplitURL(uri, package, provider, file);
    if (NS_FAILED(rv)) return NS_OK;
    
    // Obtain our chrome data source.
    nsDependentCString dataSourceStr(kChromeFileName);
    nsCOMPtr<nsIRDFDataSource> mainDataSource;
    rv = LoadDataSource(dataSourceStr, getter_AddRefs(mainDataSource), aUseProfile, nsnull);
    if (NS_FAILED(rv)) return rv;
    
    // Now that we have the appropriate chrome.rdf file, we 
    // must annotate the package resource with the knowledge of
    // whether or not we have stylesheets or overlays.
    nsCOMPtr<nsIRDFResource> hasDynamicDataArc;
    if (aIsOverlay)
      hasDynamicDataArc = mHasOverlays;
    else
      hasDynamicDataArc = mHasStylesheets;
    
    // Obtain the resource for the package.
    nsCAutoString packageResourceStr("urn:mozilla:package:");
    packageResourceStr += package;
    nsCOMPtr<nsIRDFResource> packageResource;
    GetResource(packageResourceStr, getter_AddRefs(packageResource));
    
    // Now add the arc to the package.
    nsCOMPtr<nsIRDFLiteral> trueLiteral;
    mRDFService->GetLiteral(NS_LITERAL_STRING("true").get(), getter_AddRefs(trueLiteral));
    nsChromeRegistry::UpdateArc(mainDataSource, packageResource, 
                                hasDynamicDataArc, 
                                trueLiteral, PR_FALSE);
  }

  nsCOMPtr<nsIRDFDataSource> dataSource;
  rv = GetDynamicDataSource(uri, aIsOverlay, aUseProfile, PR_TRUE, getter_AddRefs(dataSource));
  if (NS_FAILED(rv)) return rv;

  if (!dataSource)
    return NS_OK;

  nsCOMPtr<nsIRDFResource> resource;
  rv = GetResource(str, getter_AddRefs(resource));

  if (NS_FAILED(rv))
    return NS_OK;

  nsCOMPtr<nsIRDFContainer> container;
  rv = mRDFContainerUtils->MakeSeq(dataSource, resource, getter_AddRefs(container));
  if (NS_FAILED(rv)) return rv;
  if (!container) {
    // Already exists. Create a container instead.
    rv = nsComponentManager::CreateInstance("@mozilla.org/rdf/container;1",
                                      nsnull,
                                      NS_GET_IID(nsIRDFContainer),
                                      getter_AddRefs(container));
    if (NS_FAILED(rv)) return rv;
    rv = container->Init(dataSource, resource);
    if (NS_FAILED(rv)) return rv;
  }

  nsAutoString unistr(aOverlayURI);
  nsCOMPtr<nsIRDFLiteral> literal;
  rv = mRDFService->GetLiteral(unistr.get(), getter_AddRefs(literal));
  if (NS_FAILED(rv)) return rv;

  if (aRemove) {
    rv = container->RemoveElement(literal, PR_TRUE);
    if (NS_FAILED(rv)) return rv;
  }
  else {
    PRInt32 index;
    rv = container->IndexOf(literal, &index);
    if (NS_FAILED(rv)) return rv;
    if (index == -1) {
      rv = container->AppendElement(literal);
      if (NS_FAILED(rv)) return rv;
    }
  }

  nsCOMPtr<nsIRDFRemoteDataSource> remote = do_QueryInterface(dataSource, &rv);
  if (NS_SUCCEEDED(rv)) {
    rv = remote->Flush();
  }

  return rv;
}

NS_IMETHODIMP nsChromeRegistry::UpdateDynamicDataSource(nsIRDFDataSource *aDataSource, nsIRDFResource *aResource,
                                                        PRBool aIsOverlay, PRBool aUseProfile, PRBool aRemove)
{
  nsCOMPtr<nsIRDFContainer> container;
  nsresult rv;

  rv = nsComponentManager::CreateInstance("@mozilla.org/rdf/container;1",
                                          nsnull,
                                          NS_GET_IID(nsIRDFContainer),
                                          getter_AddRefs(container));
  if (NS_FAILED(rv)) return rv;

  rv = container->Init(aDataSource, aResource);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsISimpleEnumerator> arcs;
  rv = container->GetElements(getter_AddRefs(arcs));
  if (NS_FAILED(rv)) return rv;

  PRBool moreElements;
  rv = arcs->HasMoreElements(&moreElements);
  if (NS_FAILED(rv)) return rv;

  const char *value;
  rv = aResource->GetValueConst(&value);
  if (NS_FAILED(rv)) return rv;

  while (moreElements)
  {
    nsCOMPtr<nsISupports> supports;
    rv = arcs->GetNext(getter_AddRefs(supports));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIRDFLiteral> literal = do_QueryInterface(supports, &rv);

    if (NS_SUCCEEDED(rv))
    {
      const PRUnichar* valueStr;
      rv = literal->GetValueConst(&valueStr);
      if (NS_FAILED(rv)) return rv;

      rv = WriteInfoToDataSource(value, valueStr, aIsOverlay, aUseProfile, aRemove);
      if (NS_FAILED(rv)) return rv;
    }
    rv = arcs->HasMoreElements(&moreElements);
    if (NS_FAILED(rv)) return rv;
  }

  return NS_OK;
}


NS_IMETHODIMP nsChromeRegistry::UpdateDynamicDataSources(nsIRDFDataSource *aDataSource,
                                                        PRBool aIsOverlay, PRBool aUseProfile, PRBool aRemove)
{
  nsresult rv;
  nsCOMPtr<nsIRDFResource> resource;
  nsCAutoString root;
  if (aIsOverlay)
    root.Assign("urn:mozilla:overlays");
  else root.Assign("urn:mozilla:stylesheets");

  rv = GetResource(root, getter_AddRefs(resource));

  if (!resource)
    return NS_OK;

  nsCOMPtr<nsIRDFContainer> container(do_CreateInstance("@mozilla.org/rdf/container;1"));
  if (!container)
    return NS_OK;

  if (NS_FAILED(container->Init(aDataSource, resource)))
    return NS_OK;

  nsCOMPtr<nsISimpleEnumerator> arcs;
  if (NS_FAILED(container->GetElements(getter_AddRefs(arcs))))
    return NS_OK;

  PRBool moreElements;
  rv = arcs->HasMoreElements(&moreElements);
  if (NS_FAILED(rv)) return rv;

  while (moreElements)
  {
    nsCOMPtr<nsISupports> supports;
    rv = arcs->GetNext(getter_AddRefs(supports));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIRDFResource> resource2 = do_QueryInterface(supports, &rv);

    if (NS_SUCCEEDED(rv))
    {
      rv = UpdateDynamicDataSource(aDataSource, resource2, aIsOverlay, aUseProfile, aRemove);
      if (NS_FAILED(rv)) return rv;
    }

    rv = arcs->HasMoreElements(&moreElements);
    if (NS_FAILED(rv)) return rv;
  }

  return NS_OK;
}

NS_IMETHODIMP nsChromeRegistry::SelectSkin(const PRUnichar* aSkin,
                                        PRBool aUseProfile)
{
  return SetProvider(nsCAutoString("skin"), mSelectedSkin, aSkin, aUseProfile, nsnull, PR_TRUE);
}

NS_IMETHODIMP nsChromeRegistry::SelectLocale(const PRUnichar* aLocale,
                                          PRBool aUseProfile)
{
  return SetProvider(nsCAutoString("locale"), mSelectedLocale, aLocale, aUseProfile, nsnull, PR_TRUE);
}

NS_IMETHODIMP nsChromeRegistry::SelectLocaleForProfile(const PRUnichar *aLocale,
                                                       const PRUnichar *aProfilePath)
{
  // to be changed to use given path
  return SetProvider(nsCAutoString("locale"), mSelectedLocale, aLocale, PR_TRUE, NS_ConvertUCS2toUTF8(aProfilePath).get(), PR_TRUE);
}

/* void setRuntimeProvider (in boolean runtimeProvider); */
// should we inline this one?
NS_IMETHODIMP nsChromeRegistry::SetRuntimeProvider(PRBool runtimeProvider)
{
  mRuntimeProvider = runtimeProvider;
  return NS_OK;
}


/* wstring getSelectedLocale (); */
NS_IMETHODIMP nsChromeRegistry::GetSelectedLocale(const PRUnichar *aPackageName,
                                                  PRUnichar **_retval)
{
  // check if mChromeDataSource is null; do we need to apply this to every instance?
  // is there a better way to test if the data source is ready?
  if (!mChromeDataSource) {
    return NS_ERROR_FAILURE;
  }

  nsString packageStr(aPackageName);
  nsCAutoString resourceStr("urn:mozilla:package:");
  resourceStr += NS_ConvertUCS2toUTF8(packageStr.get());

  // Obtain the resource.
  nsresult rv = NS_OK;
  nsCOMPtr<nsIRDFResource> resource;
  rv = GetResource(resourceStr, getter_AddRefs(resource));
  if (NS_FAILED(rv)) {
    NS_ERROR("Unable to obtain the package resource.");
    return rv;
  }

  if (mChromeDataSource == nsnull)
    return NS_ERROR_NULL_POINTER;

  // Follow the "selectedLocale" arc.
  nsCOMPtr<nsIRDFNode> selectedProvider;
  if (NS_FAILED(rv = mChromeDataSource->GetTarget(resource, mSelectedLocale, PR_TRUE, getter_AddRefs(selectedProvider)))) {
    NS_ERROR("Unable to obtain the provider.");
    return rv;
  }

  if (!selectedProvider) {
    rv = FindProvider(NS_ConvertUCS2toUTF8(packageStr.get()), nsCAutoString("locale"), mSelectedLocale, getter_AddRefs(selectedProvider));
    if (!selectedProvider)
      return rv;
  }

  resource = do_QueryInterface(selectedProvider);
  if (!resource)
    return NS_ERROR_FAILURE;

  // selectedProvider.mURI now looks like "urn:mozilla:locale:ja-JP:navigator"
  const char *uri;
  if (NS_FAILED(rv = resource->GetValueConst(&uri)))
    return rv;

  // trim down to "urn:mozilla:locale:ja-JP"
  nsAutoString ustr = NS_ConvertUTF8toUCS2(uri);

  packageStr.Insert(PRUnichar(':'), 0);
  PRInt32 pos = ustr.RFind(packageStr);
  nsString urn;
  ustr.Left(urn, pos);

  rv = GetResource(NS_ConvertUCS2toUTF8(urn.get()), getter_AddRefs(resource));
  if (NS_FAILED(rv)) {
    NS_ERROR("Unable to obtain the provider resource.");
    return rv;
  }

  // From this resource, follow the "name" arc.
  nsCAutoString lc_name; // is this i18n friendly? RDF now use UTF8 internally
  rv = nsChromeRegistry::FollowArc(mChromeDataSource,
                                   lc_name,
                                   resource,
                                   mName);
  if (NS_FAILED(rv)) return rv;

  // this is not i18n friendly? RDF now use UTF8 internally.
  *_retval = ToNewUnicode(lc_name);

  return NS_OK;
}

NS_IMETHODIMP nsChromeRegistry::DeselectSkin(const PRUnichar* aSkin,
                                        PRBool aUseProfile)
{
  return SetProvider(nsCAutoString("skin"), mSelectedSkin, aSkin, aUseProfile, nsnull, PR_FALSE);
}

NS_IMETHODIMP nsChromeRegistry::DeselectLocale(const PRUnichar* aLocale,
                                          PRBool aUseProfile)
{
  return SetProvider(nsCAutoString("locale"), mSelectedLocale, aLocale, aUseProfile, nsnull, PR_FALSE);
}

NS_IMETHODIMP nsChromeRegistry::SetProvider(const nsCString& aProvider,
                                            nsIRDFResource* aSelectionArc,
                                            const PRUnichar* aProviderName,
                                            PRBool aUseProfile, const char *aProfilePath,
                                            PRBool aIsAdding)
{
  // Build the provider resource str.
  // e.g., urn:mozilla:skin:aqua/1.0
  nsCAutoString resourceStr( "urn:mozilla:" );
  resourceStr += aProvider;
  resourceStr += ":";
  resourceStr.AppendWithConversion(aProviderName);

  // Obtain the provider resource.
  nsresult rv = NS_OK;
  nsCOMPtr<nsIRDFResource> resource;
  rv = GetResource(resourceStr, getter_AddRefs(resource));
  if (NS_FAILED(rv)) {
    NS_ERROR("Unable to obtain the package resource.");
    return rv;
  }
  NS_ASSERTION(resource, "failed to GetResource");

  // Follow the packages arc to the package resources.
  nsCOMPtr<nsIRDFNode> packageList;
  rv = mChromeDataSource->GetTarget(resource, mPackages, PR_TRUE, getter_AddRefs(packageList));
  if (NS_FAILED(rv)) {
    NS_ERROR("Unable to obtain the SEQ for the package list.");
    return rv;
  }
  // ok for packageList to be null here -- it just means that we haven't encountered that package yet

  nsCOMPtr<nsIRDFResource> packageSeq(do_QueryInterface(packageList, &rv));
  if (NS_FAILED(rv)) return rv;

  // Build an RDF container to wrap the SEQ
  nsCOMPtr<nsIRDFContainer> container;
  rv = nsComponentManager::CreateInstance("@mozilla.org/rdf/container;1",
                                          nsnull,
                                          NS_GET_IID(nsIRDFContainer),
                                          getter_AddRefs(container));
  if (NS_FAILED(rv))
    return NS_OK;

  if (NS_FAILED(container->Init(mChromeDataSource, packageSeq)))
    return NS_OK;

  nsCOMPtr<nsISimpleEnumerator> arcs;
  if (NS_FAILED(container->GetElements(getter_AddRefs(arcs))))
    return NS_OK;

  // For each skin/package entry, follow the arcs to the real package
  // resource.
  PRBool more;
  rv = arcs->HasMoreElements(&more);
  if (NS_FAILED(rv)) return rv;
  while (more) {
    nsCOMPtr<nsISupports> packageSkinEntry;
    rv = arcs->GetNext(getter_AddRefs(packageSkinEntry));
    if (NS_SUCCEEDED(rv) && packageSkinEntry) {
      nsCOMPtr<nsIRDFResource> entry = do_QueryInterface(packageSkinEntry);
      if (entry) {
         // Obtain the real package resource.
         nsCOMPtr<nsIRDFNode> packageNode;
         rv = mChromeDataSource->GetTarget(entry, mPackage, PR_TRUE, getter_AddRefs(packageNode));
         if (NS_FAILED(rv)) {
           NS_ERROR("Unable to obtain the package resource.");
           return rv;
         }

         // Select the skin for this package resource.
         nsCOMPtr<nsIRDFResource> packageResource(do_QueryInterface(packageNode));
         if (packageResource) {
           rv = SetProviderForPackage(aProvider, packageResource, entry, aSelectionArc, aUseProfile, aProfilePath, aIsAdding);
           if (NS_FAILED(rv))
             continue; // Well, let's set as many sub-packages as we can...
         }
      }
    }
    rv = arcs->HasMoreElements(&more);
    if (NS_FAILED(rv)) return rv;
  }

  if (aProvider.Equals("skin") && mScrollbarSheet)
    LoadStyleSheet(getter_AddRefs(mScrollbarSheet), nsCAutoString("chrome://global/skin/scrollbars.css"));

  return NS_OK;
}

NS_IMETHODIMP
nsChromeRegistry::SetProviderForPackage(const nsCString& aProvider,
                                        nsIRDFResource* aPackageResource,
                                        nsIRDFResource* aProviderPackageResource,
                                        nsIRDFResource* aSelectionArc,
                                        PRBool aUseProfile, const char *aProfilePath,
                                        PRBool aIsAdding)
{
  nsresult rv;
  
  // Figure out which file we're needing to modify, e.g., is it the install
  // dir or the profile dir, and get the right datasource.
  nsCOMPtr<nsIRDFDataSource> dataSource;
  rv = LoadDataSource(kChromeFileName, getter_AddRefs(dataSource), aUseProfile, aProfilePath);
  if (NS_FAILED(rv)) return rv;

  rv = nsChromeRegistry::UpdateArc(dataSource, aPackageResource, aSelectionArc, aProviderPackageResource, !aIsAdding);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIRDFRemoteDataSource> remote = do_QueryInterface(dataSource, &rv);
  if (NS_FAILED(rv)) return rv;

  // add one more check: 
  //   assert the data source only when we are not setting runtime-only provider
  if (!mBatchInstallFlushes && !mRuntimeProvider)
    rv = remote->Flush();

  // always reset the flag
  mRuntimeProvider = PR_FALSE;

  return rv;
}

NS_IMETHODIMP nsChromeRegistry::SelectSkinForPackage(const PRUnichar *aSkin,
                                                  const PRUnichar *aPackageName,
                                                  PRBool aUseProfile)
{
  nsCAutoString provider("skin");
  return SelectProviderForPackage(provider, aSkin, aPackageName, mSelectedSkin, aUseProfile, PR_TRUE);
}

NS_IMETHODIMP nsChromeRegistry::SelectLocaleForPackage(const PRUnichar *aLocale,
                                                    const PRUnichar *aPackageName,
                                                    PRBool aUseProfile)
{
  nsCAutoString provider("locale");
  return SelectProviderForPackage(provider, aLocale, aPackageName, mSelectedLocale, aUseProfile, PR_TRUE);
}

NS_IMETHODIMP nsChromeRegistry::DeselectSkinForPackage(const PRUnichar *aSkin,
                                                  const PRUnichar *aPackageName,
                                                  PRBool aUseProfile)
{
  nsCAutoString provider("skin");
  return SelectProviderForPackage(provider, aSkin, aPackageName, mSelectedSkin, aUseProfile, PR_FALSE);
}

NS_IMETHODIMP nsChromeRegistry::DeselectLocaleForPackage(const PRUnichar *aLocale,
                                                    const PRUnichar *aPackageName,
                                                    PRBool aUseProfile)
{
  nsCAutoString provider("locale");
  return SelectProviderForPackage(provider, aLocale, aPackageName, mSelectedLocale, aUseProfile, PR_FALSE);
}

NS_IMETHODIMP nsChromeRegistry::IsSkinSelectedForPackage(const PRUnichar *aSkin,
                                                  const PRUnichar *aPackageName,
                                                  PRBool aUseProfile, PRBool* aResult)
{
  nsCAutoString provider("skin");
  return IsProviderSelectedForPackage(provider, aSkin, aPackageName, mSelectedSkin, aUseProfile, aResult);
}

NS_IMETHODIMP nsChromeRegistry::IsLocaleSelectedForPackage(const PRUnichar *aLocale,
                                                    const PRUnichar *aPackageName,
                                                    PRBool aUseProfile, PRBool* aResult)
{
  nsCAutoString provider("locale");
  return IsProviderSelectedForPackage(provider, aLocale, aPackageName, mSelectedLocale, aUseProfile, aResult);
}

NS_IMETHODIMP nsChromeRegistry::SelectProviderForPackage(const nsCString& aProviderType,
                                        const PRUnichar *aProviderName,
                                        const PRUnichar *aPackageName,
                                        nsIRDFResource* aSelectionArc,
                                        PRBool aUseProfile, PRBool aIsAdding)
{
  nsCAutoString package( "urn:mozilla:package:" );
  package.AppendWithConversion(aPackageName);

  nsCAutoString provider( "urn:mozilla:" );
  provider += aProviderType;
  provider += ":";
  provider.AppendWithConversion(aProviderName);
  provider += ":";
  provider.AppendWithConversion(aPackageName);

  // Obtain the package resource.
  nsresult rv = NS_OK;
  nsCOMPtr<nsIRDFResource> packageResource;
  rv = GetResource(package, getter_AddRefs(packageResource));
  if (NS_FAILED(rv)) {
    NS_ERROR("Unable to obtain the package resource.");
    return rv;
  }
  NS_ASSERTION(packageResource, "failed to get packageResource");

  // Obtain the provider resource.
  nsCOMPtr<nsIRDFResource> providerResource;
  rv = GetResource(provider, getter_AddRefs(providerResource));
  if (NS_FAILED(rv)) {
    NS_ERROR("Unable to obtain the provider resource.");
    return rv;
  }
  NS_ASSERTION(providerResource, "failed to get providerResource");

  // Version-check before selecting.  If this skin isn't a compatible version, then
  // don't allow the selection.
  // We found a selected provider, but now we need to verify that the version
  // specified by the package and the version specified by the provider are
  // one and the same.  If they aren't, then we cannot use this provider.
  nsCOMPtr<nsIRDFResource> versionArc;
  if (aSelectionArc == mSelectedSkin)
    versionArc = mSkinVersion;
  else // Locale arc
    versionArc = mLocaleVersion;

  nsCAutoString packageVersion;
  nsChromeRegistry::FollowArc(mChromeDataSource, packageVersion, packageResource, versionArc);
  if (!packageVersion.IsEmpty()) {
    // The package only wants providers (skins) that say they can work with it.  Let's find out
    // if our provider (skin) can work with it.
    nsCAutoString providerVersion;
    nsChromeRegistry::FollowArc(mChromeDataSource, providerVersion, providerResource, versionArc);
    if (!providerVersion.Equals(packageVersion))
      return NS_ERROR_FAILURE;
  }

  return SetProviderForPackage(aProviderType, packageResource, providerResource, aSelectionArc,
                               aUseProfile, nsnull, aIsAdding);
}

NS_IMETHODIMP nsChromeRegistry::IsSkinSelected(const PRUnichar* aSkin,
                                               PRBool aUseProfile, PRInt32* aResult)
{
  return IsProviderSelected(nsCAutoString("skin"), aSkin, mSelectedSkin, aUseProfile, aResult);
}

NS_IMETHODIMP nsChromeRegistry::IsLocaleSelected(const PRUnichar* aLocale,
                                                 PRBool aUseProfile, PRInt32* aResult)
{
  return IsProviderSelected(nsCAutoString("locale"), aLocale, mSelectedLocale, aUseProfile, aResult);
}

NS_IMETHODIMP nsChromeRegistry::IsProviderSelected(const nsCString& aProvider,
                                                   const PRUnichar* aProviderName,
                                                   nsIRDFResource* aSelectionArc,
                                                   PRBool aUseProfile, PRInt32* aResult)
{
  // Build the provider resource str.
  // e.g., urn:mozilla:skin:aqua/1.0
  *aResult = NONE;
  nsCAutoString resourceStr( "urn:mozilla:" );
  resourceStr += aProvider;
  resourceStr += ":";
  resourceStr.AppendWithConversion(aProviderName);
  // Obtain the provider resource.
  nsresult rv = NS_OK;
  nsCOMPtr<nsIRDFResource> resource;
  rv = GetResource(resourceStr, getter_AddRefs(resource));
  if (NS_FAILED(rv)) {
    NS_ERROR("Unable to obtain the package resource.");
    return rv;
  }
  NS_ASSERTION(resource, "failed to GetResource");

  // Follow the packages arc to the package resources.
  nsCOMPtr<nsIRDFNode> packageList;
  rv = mChromeDataSource->GetTarget(resource, mPackages, PR_TRUE, getter_AddRefs(packageList));
  if (NS_FAILED(rv)) {
    NS_ERROR("Unable to obtain the SEQ for the package list.");
    return rv;
  }
  // ok for packageList to be null here -- it just means that we haven't encountered that package yet

  nsCOMPtr<nsIRDFResource> packageSeq(do_QueryInterface(packageList, &rv));
  if (NS_FAILED(rv)) return rv;

  // Build an RDF container to wrap the SEQ
  nsCOMPtr<nsIRDFContainer> container(do_CreateInstance("@mozilla.org/rdf/container;1"));
  if (NS_FAILED(container->Init(mChromeDataSource, packageSeq)))
    return NS_OK;

  nsCOMPtr<nsISimpleEnumerator> arcs;
  container->GetElements(getter_AddRefs(arcs));

  // For each skin/package entry, follow the arcs to the real package
  // resource.
  PRBool more;
  PRInt32 numSet = 0;
  PRInt32 numPackages = 0;
  rv = arcs->HasMoreElements(&more);
  if (NS_FAILED(rv)) return rv;
  while (more) {
    nsCOMPtr<nsISupports> packageSkinEntry;
    rv = arcs->GetNext(getter_AddRefs(packageSkinEntry));
    if (NS_SUCCEEDED(rv) && packageSkinEntry) {
      nsCOMPtr<nsIRDFResource> entry = do_QueryInterface(packageSkinEntry);
      if (entry) {
         // Obtain the real package resource.
         nsCOMPtr<nsIRDFNode> packageNode;
         rv = mChromeDataSource->GetTarget(entry, mPackage, PR_TRUE, getter_AddRefs(packageNode));
         if (NS_FAILED(rv)) {
           NS_ERROR("Unable to obtain the package resource.");
           return rv;
         }

         // Select the skin for this package resource.
         nsCOMPtr<nsIRDFResource> packageResource(do_QueryInterface(packageNode));
         if (packageResource) {
           PRBool isSet = PR_FALSE;
           rv = IsProviderSetForPackage(aProvider, packageResource, entry, aSelectionArc, aUseProfile, &isSet);
           if (NS_FAILED(rv)) {
             NS_ERROR("Unable to set provider for package resource.");
             return rv;
           }
           ++numPackages;
           if (isSet)
             ++numSet;
         }
      }
    }
    rv = arcs->HasMoreElements(&more);
    if (NS_FAILED(rv)) return rv;
  }
  if (numPackages == numSet)
    *aResult = FULL;
  else if (numSet)
    *aResult = PARTIAL;
  return NS_OK;
}

NS_IMETHODIMP
nsChromeRegistry::IsProviderSelectedForPackage(const nsCString& aProviderType,
                                               const PRUnichar *aProviderName,
                                               const PRUnichar *aPackageName,
                                               nsIRDFResource* aSelectionArc,
                                               PRBool aUseProfile, PRBool* aResult)
{
  *aResult = PR_FALSE;
  nsCAutoString package( "urn:mozilla:package:" );
  package.AppendWithConversion(aPackageName);

  nsCAutoString provider( "urn:mozilla:" );
  provider += aProviderType;
  provider += ":";
  provider.AppendWithConversion(aProviderName);
  provider += ":";
  provider.AppendWithConversion(aPackageName);

  // Obtain the package resource.
  nsresult rv = NS_OK;
  nsCOMPtr<nsIRDFResource> packageResource;
  rv = GetResource(package, getter_AddRefs(packageResource));
  if (NS_FAILED(rv)) {
    NS_ERROR("Unable to obtain the package resource.");
    return rv;
  }
  NS_ASSERTION(packageResource, "failed to get packageResource");

  // Obtain the provider resource.
  nsCOMPtr<nsIRDFResource> providerResource;
  rv = GetResource(provider, getter_AddRefs(providerResource));
  if (NS_FAILED(rv)) {
    NS_ERROR("Unable to obtain the provider resource.");
    return rv;
  }
  NS_ASSERTION(providerResource, "failed to get providerResource");

  return IsProviderSetForPackage(aProviderType, packageResource, providerResource, aSelectionArc,
                                 aUseProfile, aResult);
}

NS_IMETHODIMP
nsChromeRegistry::IsProviderSetForPackage(const nsCString& aProvider,
                                          nsIRDFResource* aPackageResource,
                                          nsIRDFResource* aProviderPackageResource,
                                          nsIRDFResource* aSelectionArc,
                                          PRBool aUseProfile, PRBool* aResult)
{
  nsresult rv;
  // Figure out which file we're needing to modify, e.g., is it the install
  // dir or the profile dir, and get the right datasource.
  
  nsCOMPtr<nsIRDFDataSource> dataSource;
  rv = LoadDataSource(kChromeFileName, getter_AddRefs(dataSource), aUseProfile, nsnull);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIRDFNode> retVal;
  dataSource->GetTarget(aPackageResource, aSelectionArc, PR_TRUE, getter_AddRefs(retVal));
  if (retVal) {
    nsCOMPtr<nsIRDFNode> node(do_QueryInterface(aProviderPackageResource));
    if (node == retVal)
      *aResult = PR_TRUE;
  }

  return NS_OK;
}

NS_IMETHODIMP nsChromeRegistry::InstallProvider(const nsCString& aProviderType,
                                                const nsCString& aBaseURL,
                                                PRBool aUseProfile, PRBool aAllowScripts,
                                                PRBool aRemove)
{
  // XXX don't allow local chrome overrides of install chrome!
#ifdef DEBUG
  printf("*** Chrome Registration of %s: Checking for contents.rdf at %s\n", aProviderType.get(), aBaseURL.get());
#endif

  // Load the data source found at the base URL.
  nsCOMPtr<nsIRDFDataSource> dataSource;
  nsresult rv = nsComponentManager::CreateInstance(kRDFXMLDataSourceCID,
                                                   nsnull,
                                                   NS_GET_IID(nsIRDFDataSource),
                                                   (void**) getter_AddRefs(dataSource));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIRDFRemoteDataSource> remote = do_QueryInterface(dataSource, &rv);
  if (NS_FAILED(rv)) return rv;

  // We need to read this synchronously.
  nsCAutoString key(aBaseURL);
  key += "contents.rdf";
  remote->Init(key.get());
  remote->Refresh(PR_TRUE);

  PRBool skinCount = GetProviderCount(nsCAutoString("skin"), dataSource);
  PRBool localeCount = GetProviderCount(nsCAutoString("locale"), dataSource);
  PRBool packageCount = GetProviderCount(nsCAutoString("package"), dataSource);

  PRBool appendPackage = PR_FALSE;
  PRBool appendProvider = PR_FALSE;
  PRBool appendProviderName = PR_FALSE;

  if (skinCount == 0 && localeCount == 0 && packageCount == 0) {
    // Try the old-style manifest.rdf instead
    key = aBaseURL;
    key += "manifest.rdf";
    (void)remote->Init(key.get());      // ignore failure here
    rv = remote->Refresh(PR_TRUE);
    if (NS_FAILED(rv)) return rv;
    appendPackage = PR_TRUE;
    appendProvider = PR_TRUE;
    NS_WARNING("Trying old-style manifest.rdf. Please update to contents.rdf.");
  }
  else {
    if ((skinCount > 1 && aProviderType.Equals("skin")) ||
        (localeCount > 1 && aProviderType.Equals("locale")))
      appendProviderName = PR_TRUE;

    if (!appendProviderName && packageCount > 1) {
      appendPackage = PR_TRUE;
    }

    if (aProviderType.Equals("skin")) {
      if (!appendProviderName && (localeCount == 1 || packageCount != 0))
        appendProvider = PR_TRUE;
    }
    else if (aProviderType.Equals("locale")) {
      if (!appendProviderName && (skinCount == 1 || packageCount != 0))
        appendProvider = PR_TRUE;
    }
    else {
      // Package install.
      if (localeCount == 1 || skinCount == 1)
        appendProvider = PR_TRUE;
    }
  }

  // Load the install data source that we wish to manipulate.
  nsCOMPtr<nsIRDFDataSource> installSource;
  rv = LoadDataSource(kChromeFileName, getter_AddRefs(installSource), aUseProfile, nsnull);
  if (NS_FAILED(rv)) return rv;
  NS_ASSERTION(installSource, "failed to get installSource");

  // install our dynamic overlays
  if (aProviderType.Equals("package"))
    rv = UpdateDynamicDataSources(dataSource, PR_TRUE, aUseProfile, aRemove);
  else if (aProviderType.Equals("skin"))
    rv = UpdateDynamicDataSources(dataSource, PR_FALSE, aUseProfile, aRemove);
  if (NS_FAILED(rv)) return rv;

  // Get the literal for our loc type.
  nsAutoString locstr;
  if (aUseProfile)
    locstr.Assign(NS_LITERAL_STRING("profile"));
  else locstr.Assign(NS_LITERAL_STRING("install"));
  nsCOMPtr<nsIRDFLiteral> locLiteral;
  rv = mRDFService->GetLiteral(locstr.get(), getter_AddRefs(locLiteral));
  if (NS_FAILED(rv)) return rv;

  // Get the literal for our script access.
  nsAutoString scriptstr;
  scriptstr.Assign(NS_LITERAL_STRING("false"));
  nsCOMPtr<nsIRDFLiteral> scriptLiteral;
  rv = mRDFService->GetLiteral(scriptstr.get(), getter_AddRefs(scriptLiteral));
  if (NS_FAILED(rv)) return rv;

  // Build the prefix string. Only resources with this prefix string will have their
  // assertions copied.
  nsCAutoString prefix( "urn:mozilla:" );
  prefix += aProviderType;
  prefix += ":";

  // Get all the resources
  nsCOMPtr<nsISimpleEnumerator> resources;
  rv = dataSource->GetAllResources(getter_AddRefs(resources));
  if (NS_FAILED(rv)) return rv;

  // For each resource
  PRBool moreElements;
  rv = resources->HasMoreElements(&moreElements);
  if (NS_FAILED(rv)) return rv;

  while (moreElements) {
    nsCOMPtr<nsISupports> supports;
    rv = resources->GetNext(getter_AddRefs(supports));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIRDFResource> resource = do_QueryInterface(supports);

    // Check against the prefix string
    const char* value;
    rv = resource->GetValueConst(&value);
    if (NS_FAILED(rv)) return rv;
    nsCAutoString val(value);
    if (val.Find(prefix) == 0) {
      // It's valid.

      if (aProviderType.Equals("package") && !val.Equals("urn:mozilla:package:root")) {
        // Add arcs for the base url and loctype
        // Get the value of the base literal.
        nsCAutoString baseURL(aBaseURL);

        // Peel off the package.
        const char* val2;
        rv = resource->GetValueConst(&val2);
        if (NS_FAILED(rv)) return rv;
        nsCAutoString value2(val2);
        PRInt32 index = value2.RFind(":");
        nsCAutoString packageName;
        value2.Right(packageName, value2.Length() - index - 1);

        if (appendPackage) {
          baseURL += packageName;
          baseURL += "/";
        }
        if (appendProvider) {
          baseURL += "content/";
        }

        nsCOMPtr<nsIRDFLiteral> baseLiteral;
        mRDFService->GetLiteral(NS_ConvertASCIItoUCS2(baseURL).get(), getter_AddRefs(baseLiteral));

        rv = nsChromeRegistry::UpdateArc(installSource, resource, mBaseURL, baseLiteral, aRemove);
        if (NS_FAILED(rv)) return rv;
        rv = nsChromeRegistry::UpdateArc(installSource, resource, mLocType, locLiteral, aRemove);
        if (NS_FAILED(rv)) return rv;
      }

      nsCOMPtr<nsIRDFContainer> container;
      rv = nsComponentManager::CreateInstance("@mozilla.org/rdf/container;1",
                                            nsnull,
                                            NS_GET_IID(nsIRDFContainer),
                                            getter_AddRefs(container));
      if (NS_FAILED(rv)) return rv;
      rv = container->Init(dataSource, resource);
      if (NS_SUCCEEDED(rv)) {
        // XXX Deal with BAGS and ALTs? Aww, to hell with it. Who cares? I certainly don't.
        // We're a SEQ. Different rules apply. Do an AppendElement instead.
        // First do the decoration in the install data source.
        nsCOMPtr<nsIRDFContainer> installContainer;
        rv = mRDFContainerUtils->MakeSeq(installSource, resource, getter_AddRefs(installContainer));
        if (NS_FAILED(rv)) return rv;
        if (!installContainer) {
          // Already exists. Create a container instead.
          rv = nsComponentManager::CreateInstance("@mozilla.org/rdf/container;1",
                                            nsnull,
                                            NS_GET_IID(nsIRDFContainer),
                                            getter_AddRefs(installContainer));
          if (NS_FAILED(rv)) return rv;
          rv = installContainer->Init(installSource, resource);
          if (NS_FAILED(rv)) return rv;
        }

        // Put all our elements into the install container.
        nsCOMPtr<nsISimpleEnumerator> seqKids;
        rv = container->GetElements(getter_AddRefs(seqKids));
        if (NS_FAILED(rv)) return rv;
        PRBool moreKids;
        rv = seqKids->HasMoreElements(&moreKids);
        if (NS_FAILED(rv)) return rv;
        while (moreKids) {
          nsCOMPtr<nsISupports> supp;
          rv = seqKids->GetNext(getter_AddRefs(supp));
          if (NS_FAILED(rv)) return rv;
          nsCOMPtr<nsIRDFNode> kid = do_QueryInterface(supp);
          if (aRemove) {
            rv = installContainer->RemoveElement(kid, PR_TRUE);
            if (NS_FAILED(rv)) return rv;
          }
          else {
            PRInt32 index;
            rv = installContainer->IndexOf(kid, &index);
            if (NS_FAILED(rv)) return rv;
            if (index == -1) {
              rv = installContainer->AppendElement(kid);
              if (NS_FAILED(rv)) return rv;
            }
            rv = seqKids->HasMoreElements(&moreKids);
            if (NS_FAILED(rv)) return rv;
          }
        }

        // See if we're a packages seq in a skin/locale.  If so, we need to set up the baseURL, allowScripts
        // and package arcs.
        if (val.Find(":packages") != -1 && !aProviderType.Equals(nsCAutoString("package"))) {
          PRBool doAppendPackage = appendPackage;
          PRInt32 perProviderPackageCount;
          container->GetCount(&perProviderPackageCount);
          if (perProviderPackageCount > 1)
            doAppendPackage = PR_TRUE;

          // Iterate over our kids a second time.
          nsCOMPtr<nsISimpleEnumerator> seqKids2;
          rv = container->GetElements(getter_AddRefs(seqKids2));
          if (NS_FAILED(rv)) return rv;
          PRBool moreKids2;
          rv = seqKids2->HasMoreElements(&moreKids2);
          if (NS_FAILED(rv)) return rv;
          while (moreKids2) {
            nsCOMPtr<nsISupports> supp;
            rv = seqKids2->GetNext(getter_AddRefs(supp));
            if (NS_FAILED(rv)) return rv;
            nsCOMPtr<nsIRDFResource> entry(do_QueryInterface(supp));
            if (entry) {
              // Get the value of the base literal.
              nsCAutoString baseURL(aBaseURL);

              // Peel off the package and the provider.
              const char* val2;
              rv = entry->GetValueConst(&val2);
              if (NS_FAILED(rv)) return rv;
              nsCAutoString value2(val2);
              PRInt32 index = value2.RFind(":");
              nsCAutoString packageName;
              value2.Right(packageName, value2.Length() - index - 1);
              nsCAutoString remainder;
              value2.Left(remainder, index);

              nsCAutoString providerName;
              index = remainder.RFind(":");
              remainder.Right(providerName, remainder.Length() - index - 1);

              // Append them to the base literal and tack on a final slash.
              if (appendProviderName) {
                baseURL += providerName;
                baseURL += "/";
              }
              if (doAppendPackage) {
                baseURL += packageName;
                baseURL += "/";
              }
              if (appendProvider) {
                baseURL += aProviderType;
                baseURL += "/";
              }

              nsCOMPtr<nsIRDFLiteral> baseLiteral;
              mRDFService->GetLiteral(NS_ConvertASCIItoUCS2(baseURL).get(), getter_AddRefs(baseLiteral));

              rv = nsChromeRegistry::UpdateArc(installSource, entry, mBaseURL, baseLiteral, aRemove);
              if (NS_FAILED(rv)) return rv;
              if (aProviderType.Equals(nsCAutoString("skin")) && !aAllowScripts) {
                rv = nsChromeRegistry::UpdateArc(installSource, entry, mAllowScripts, scriptLiteral, aRemove);
                if (NS_FAILED(rv)) return rv;
              }

              // Now set up the package arc.
              if (index != -1) {
                // Peel off the package name.

                nsCAutoString resourceName("urn:mozilla:package:");
                resourceName += packageName;
                nsCOMPtr<nsIRDFResource> packageResource;
                rv = GetResource(resourceName, getter_AddRefs(packageResource));
                if (NS_FAILED(rv)) return rv;
                if (packageResource) {
                  rv = nsChromeRegistry::UpdateArc(installSource, entry, mPackage, packageResource, aRemove);
                  if (NS_FAILED(rv)) return rv;
                }
              }
            }

            rv = seqKids2->HasMoreElements(&moreKids2);
            if (NS_FAILED(rv)) return rv;
          }
        }
      }
      else {
        // We're not a seq. Get all of the arcs that go out.
        nsCOMPtr<nsISimpleEnumerator> arcs;
        rv = dataSource->ArcLabelsOut(resource, getter_AddRefs(arcs));
        if (NS_FAILED(rv)) return rv;

        PRBool moreArcs;
        rv = arcs->HasMoreElements(&moreArcs);
        if (NS_FAILED(rv)) return rv;
        while (moreArcs) {
          nsCOMPtr<nsISupports> supp;
          rv = arcs->GetNext(getter_AddRefs(supp));
          if (NS_FAILED(rv)) return rv;
          nsCOMPtr<nsIRDFResource> arc = do_QueryInterface(supp);

          if (arc == mPackages) {
            // We are the main entry for a skin/locale.
            // Set up our loctype and our script access
            rv = nsChromeRegistry::UpdateArc(installSource, resource, mLocType, locLiteral, aRemove);
            if (NS_FAILED(rv)) return rv;
          }

          nsCOMPtr<nsIRDFNode> newTarget;
          rv = dataSource->GetTarget(resource, arc, PR_TRUE, getter_AddRefs(newTarget));
          if (NS_FAILED(rv)) return rv;

          if (arc == mImage) {
            // We are an image URL.  Check to see if we're a relative URL.
            nsCOMPtr<nsIRDFLiteral> literal(do_QueryInterface(newTarget));
            if (literal) {
              const PRUnichar* valueStr;
              literal->GetValueConst(&valueStr);
              nsAutoString imageURL(valueStr);
              if (imageURL.FindChar(':') == -1) {
                // We're relative. Prepend the base URL of the package.
                nsAutoString fullURL; fullURL.AssignWithConversion(aBaseURL.get());
                fullURL += imageURL;
                mRDFService->GetLiteral(fullURL.get(), getter_AddRefs(literal));
                newTarget = do_QueryInterface(literal);
              }
            }
          }

          rv = nsChromeRegistry::UpdateArc(installSource, resource, arc, newTarget, aRemove);
          if (NS_FAILED(rv)) return rv;

          rv = arcs->HasMoreElements(&moreArcs);
          if (NS_FAILED(rv)) return rv;
        }
      }
    }
    rv = resources->HasMoreElements(&moreElements);
    if (NS_FAILED(rv)) return rv;
  }

  // Flush the install source
  nsCOMPtr<nsIRDFRemoteDataSource> remoteInstall = do_QueryInterface(installSource, &rv);
  if (NS_FAILED(rv))
    return NS_OK;

  if (!mBatchInstallFlushes)
    rv = remoteInstall->Flush();

  // XXX Handle the installation of overlays.

  return rv;
}

NS_IMETHODIMP nsChromeRegistry::InstallSkin(const char* aBaseURL, PRBool aUseProfile, PRBool aAllowScripts)
{
  nsCAutoString provider("skin");
  return InstallProvider(provider, nsCAutoString(aBaseURL), aUseProfile, aAllowScripts, PR_FALSE);
}

NS_IMETHODIMP nsChromeRegistry::InstallLocale(const char* aBaseURL, PRBool aUseProfile)
{
  nsCAutoString provider("locale");
  return InstallProvider(provider, nsCAutoString(aBaseURL), aUseProfile, PR_TRUE, PR_FALSE);
}

NS_IMETHODIMP nsChromeRegistry::InstallPackage(const char* aBaseURL, PRBool aUseProfile)
{
  nsCAutoString provider("package");
  return InstallProvider(provider, nsCAutoString(aBaseURL), aUseProfile, PR_TRUE, PR_FALSE);
}

NS_IMETHODIMP nsChromeRegistry::UninstallSkin(const PRUnichar* aSkinName, PRBool aUseProfile)
{
  // The skin must first be deselected.
  DeselectSkin(aSkinName, aUseProfile);

  // Now uninstall it.
  nsCAutoString provider("skin");
  return UninstallProvider(provider, aSkinName, aUseProfile);
}

NS_IMETHODIMP nsChromeRegistry::UninstallLocale(const PRUnichar* aLocaleName, PRBool aUseProfile)
{
  // The locale must first be deselected.
  DeselectLocale(aLocaleName, aUseProfile);

  nsCAutoString provider("locale");
  return UninstallProvider(provider, aLocaleName, aUseProfile);
}

NS_IMETHODIMP nsChromeRegistry::UninstallPackage(const PRUnichar* aPackageName, PRBool aUseProfile)
{
  NS_ERROR("XXX Write me!\n");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsChromeRegistry::UninstallProvider(const nsCString& aProviderType,
                                    const PRUnichar* aProviderName,
                                    PRBool aUseProfile)
{
  // XXX We are going to simply do a snip of the arc from the seq ROOT to
  // the associated package.  waterson is going to provide the ability to name
  // roots in a datasource, and only resources that are reachable from the
  // root will be saved.
  nsresult rv = NS_OK;
  nsCAutoString prefix( "urn:mozilla:" );
  prefix += aProviderType;
  prefix += ":";

  // Obtain the root.
  nsCAutoString providerRoot(prefix);
  providerRoot += "root";

  // Obtain the child we wish to remove.
  nsCAutoString specificChild(prefix);
  nsCAutoString provName; provName.AssignWithConversion(aProviderName);
  specificChild += provName;

  // Instantiate the data source we wish to modify.
  nsCOMPtr<nsIRDFDataSource> installSource;
  rv = LoadDataSource(kChromeFileName, getter_AddRefs(installSource), aUseProfile, nsnull);
  if (NS_FAILED(rv)) return rv;
  NS_ASSERTION(installSource, "failed to get installSource");

  // Now make a container out of the root seq.
  nsCOMPtr<nsIRDFContainer> container(do_CreateInstance("@mozilla.org/rdf/container;1"));

  // Get the resource for the root.
  nsCOMPtr<nsIRDFResource> chromeResource;
  if (NS_FAILED(rv = GetResource(providerRoot, getter_AddRefs(chromeResource)))) {
    NS_ERROR("Unable to retrieve the resource corresponding to the skin/locale root.");
    return rv;
  }

  if (NS_FAILED(container->Init(installSource, chromeResource)))
    return NS_ERROR_FAILURE;

  // Get the resource for the child.
  nsCOMPtr<nsIRDFResource> childResource;
  if (NS_FAILED(rv = GetResource(specificChild, getter_AddRefs(childResource)))) {
    NS_ERROR("Unable to retrieve the resource corresponding to the skin/locale child being removed.");
    return rv;
  }

  // Remove the child from the container.
  container->RemoveElement(childResource, PR_TRUE);

  // Now flush the datasource.
  nsCOMPtr<nsIRDFRemoteDataSource> remote = do_QueryInterface(installSource);
  remote->Flush();

  return NS_OK;
}

NS_IMETHODIMP
nsChromeRegistry::GetProfileRoot(nsCString& aFileURL)
{
   nsresult rv;
   nsCOMPtr<nsIFile> userChromeDir;

   // Build a fileSpec that points to the destination
   // (profile dir + chrome)
   rv = NS_GetSpecialDirectory(NS_APP_USER_CHROME_DIR, getter_AddRefs(userChromeDir));
   if (NS_FAILED(rv) || !userChromeDir)
     return NS_ERROR_FAILURE;

   PRBool exists;
   rv = userChromeDir->Exists(&exists);
   if (NS_SUCCEEDED(rv) && !exists) {
     rv = userChromeDir->Create(nsIFile::DIRECTORY_TYPE, 0755);
     if (NS_SUCCEEDED(rv)) {
       // now we need to put the userContent.css and userChrome.css
       // stubs into place

       // first get the locations of the defaults
       nsCOMPtr<nsIFile> defaultUserContentFile;
       nsCOMPtr<nsIFile> defaultUserChromeFile;
       rv = NS_GetSpecialDirectory(NS_APP_PROFILE_DEFAULTS_50_DIR,
                                   getter_AddRefs(defaultUserContentFile));
       if (NS_FAILED(rv))
         rv = NS_GetSpecialDirectory(NS_APP_PROFILE_DEFAULTS_NLOC_50_DIR,
                                     getter_AddRefs(defaultUserContentFile));
       if (NS_FAILED(rv))
         return(rv);
       rv = NS_GetSpecialDirectory(NS_APP_PROFILE_DEFAULTS_50_DIR,
                                   getter_AddRefs(defaultUserChromeFile));
       if (NS_FAILED(rv))
         rv = NS_GetSpecialDirectory(NS_APP_PROFILE_DEFAULTS_NLOC_50_DIR,
                                     getter_AddRefs(defaultUserChromeFile));
       if (NS_FAILED(rv))
         return(rv);
       defaultUserContentFile->AppendNative(NS_LITERAL_CSTRING("chrome"));
       defaultUserContentFile->AppendNative(NS_LITERAL_CSTRING("userContent.css"));
       defaultUserChromeFile->AppendNative(NS_LITERAL_CSTRING("chrome"));
       defaultUserChromeFile->AppendNative(NS_LITERAL_CSTRING("userChrome.css"));

       // copy along
       // It aint an error if these files dont exist
       (void) defaultUserContentFile->CopyToNative(userChromeDir, nsCString());
       (void) defaultUserChromeFile->CopyToNative(userChromeDir, nsCString());
     }
   }
   if (NS_FAILED(rv))
     return rv;

   return NS_GetURLSpecFromFile(userChromeDir, aFileURL);
}

NS_IMETHODIMP
nsChromeRegistry::GetInstallRoot(nsCString& aFileURL)
{
  nsresult rv;
  nsCOMPtr<nsIFile> appChromeDir;

  // Build a fileSpec that points to the destination
  // (bin dir + chrome)
  rv = NS_GetSpecialDirectory(NS_APP_CHROME_DIR, getter_AddRefs(appChromeDir));
  if (NS_FAILED(rv) || !appChromeDir)
    return NS_ERROR_FAILURE;

  return NS_GetURLSpecFromFile(appChromeDir, aFileURL);
}

NS_IMETHODIMP
nsChromeRegistry::ReloadChrome()
{
  // Do a reload of all top level windows.
  nsresult rv = NS_OK;

  // Flush the cache completely.
  nsCOMPtr<nsIXULPrototypeCache> xulCache =
    do_GetService("@mozilla.org/xul/xul-prototype-cache;1", &rv);
  if (NS_SUCCEEDED(rv) && xulCache) {
    rv = xulCache->Flush();
    if (NS_FAILED(rv)) return rv;
  }

  nsCOMPtr<nsIStringBundleService> bundleService =
    do_GetService(kStringBundleServiceCID, &rv);

  if (NS_SUCCEEDED(rv)) {
    rv = bundleService->FlushBundles();
    if (NS_FAILED(rv)) return rv;
  }

  // Get the window mediator
  nsCOMPtr<nsIWindowMediator> windowMediator = do_GetService(kWindowMediatorCID, &rv);
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsISimpleEnumerator> windowEnumerator;

    rv = windowMediator->GetEnumerator(nsnull, getter_AddRefs(windowEnumerator));
    if (NS_SUCCEEDED(rv)) {
      // Get each dom window
      PRBool more;
      rv = windowEnumerator->HasMoreElements(&more);
      if (NS_FAILED(rv)) return rv;
      while (more) {
        nsCOMPtr<nsISupports> protoWindow;
        rv = windowEnumerator->GetNext(getter_AddRefs(protoWindow));
        if (NS_SUCCEEDED(rv)) {
          nsCOMPtr<nsIDOMWindowInternal> domWindow =
            do_QueryInterface(protoWindow);
          if (domWindow) {
            nsCOMPtr<nsIDOMLocation> location;
            domWindow->GetLocation(getter_AddRefs(location));
            if (location) {
              rv = location->Reload(PR_FALSE);
              if (NS_FAILED(rv)) return rv;
            }
          }
        }
        rv = windowEnumerator->HasMoreElements(&more);
        if (NS_FAILED(rv)) return rv;
      }
    }
  }
  return rv;
}

NS_IMETHODIMP
nsChromeRegistry::GetArcs(nsIRDFDataSource* aDataSource,
                          const nsCString& aType,
                          nsISimpleEnumerator** aResult)
{
  nsCOMPtr<nsIRDFContainer> container;
  nsresult rv = nsComponentManager::CreateInstance("@mozilla.org/rdf/container;1",
                                          nsnull,
                                          NS_GET_IID(nsIRDFContainer),
                                          getter_AddRefs(container));
  if (NS_FAILED(rv))
    return NS_OK;

  nsCAutoString lookup("chrome:");
  lookup += aType;

  // Get the chromeResource from this lookup string
  nsCOMPtr<nsIRDFResource> chromeResource;
  if (NS_FAILED(rv = GetResource(lookup, getter_AddRefs(chromeResource)))) {
    NS_ERROR("Unable to retrieve the resource corresponding to the chrome skin or content.");
    return rv;
  }

  if (NS_FAILED(container->Init(aDataSource, chromeResource)))
    return NS_OK;

  nsCOMPtr<nsISimpleEnumerator> arcs;
  if (NS_FAILED(container->GetElements(getter_AddRefs(arcs))))
    return NS_OK;

  *aResult = arcs;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsChromeRegistry::AddToCompositeDataSource(PRBool aUseProfile)
{
  nsresult rv = NS_OK;
  if (!mChromeDataSource) {
    rv = nsComponentManager::CreateInstance("@mozilla.org/rdf/datasource;1?name=composite-datasource",
                                            nsnull,
                                            NS_GET_IID(nsIRDFCompositeDataSource),
                                            getter_AddRefs(mChromeDataSource));
    if (NS_FAILED(rv))
      return rv;

    // Also create and hold on to our UI data source.
    rv = NS_NewChromeUIDataSource(mChromeDataSource, getter_AddRefs(mUIDataSource));
    if (NS_FAILED(rv)) return rv;
  }

  if (aUseProfile) {
    // Profiles take precedence.  Load them first.
    nsCOMPtr<nsIRDFDataSource> dataSource;
    LoadDataSource(kChromeFileName, getter_AddRefs(dataSource), PR_TRUE, nsnull);
    mChromeDataSource->AddDataSource(dataSource);
  }

  // Always load the install dir datasources
  nsCOMPtr<nsIRDFDataSource> dataSource;
  LoadDataSource(kChromeFileName, getter_AddRefs(dataSource), PR_FALSE, nsnull);
  mChromeDataSource->AddDataSource(dataSource);
  
  return NS_OK;
}

NS_IMETHODIMP
nsChromeRegistry::GetAgentSheets(nsIDocShell* aDocShell, nsISupportsArray **aResult)
{
  nsresult rv = NS_NewISupportsArray(aResult);

  // Determine the agent sheets that should be loaded.
  if (!mScrollbarSheet)
    LoadStyleSheet(getter_AddRefs(mScrollbarSheet), nsCAutoString("chrome://global/skin/scrollbars.css"));

  if (!mFormSheet) {
    nsCAutoString sheetURL;
    GetFormSheetURL(sheetURL);
    LoadStyleSheet(getter_AddRefs(mFormSheet), sheetURL);
  }

  PRBool shouldOverride = PR_FALSE;
  nsCOMPtr<nsIChromeEventHandler> chromeHandler;
  aDocShell->GetChromeEventHandler(getter_AddRefs(chromeHandler));
  if (chromeHandler) {
    nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(chromeHandler));
    if (elt) {
      nsAutoString sheets;
      elt->GetAttribute(NS_LITERAL_STRING("usechromesheets"), sheets);
      if (!sheets.IsEmpty()) {
        // Construct the URIs and try to load each sheet.
        nsCAutoString sheetsStr; sheetsStr.AssignWithConversion(sheets);
        char* str = ToNewCString(sheets);
        char* newStr;
        char* token = nsCRT::strtok( str, ", ", &newStr );
        while (token) {
          nsCOMPtr<nsIContent> content(do_QueryInterface(elt));
          nsCOMPtr<nsIDocument> doc;
          content->GetDocument(*getter_AddRefs(doc));
          nsCOMPtr<nsIURI> docURL;
          doc->GetDocumentURL(getter_AddRefs(docURL));
          nsCOMPtr<nsIURI> url;
          rv = NS_NewURI(getter_AddRefs(url), nsDependentCString(token), nsnull, docURL);

          PRBool enabled = PR_FALSE;
          nsCOMPtr<nsICSSStyleSheet> sheet;
          nsCOMPtr<nsIXULPrototypeCache> cache(do_GetService("@mozilla.org/xul/xul-prototype-cache;1"));
          if (cache) {
            cache->GetEnabled(&enabled);
            if (enabled) {
              nsCOMPtr<nsICSSStyleSheet> cachedSheet;
              cache->GetStyleSheet(url, getter_AddRefs(cachedSheet));
              if (cachedSheet)
                sheet = cachedSheet;
            }
          }

          if (!sheet) {
            LoadStyleSheetWithURL(url, getter_AddRefs(sheet));
            if (sheet) {
              if (enabled)
                cache->PutStyleSheet(sheet);
            }
          }

          if (sheet) {
            // A sheet was loaded successfully.  We will *not* use the default
            // set of agent sheets (which consists solely of the scrollbar sheet).
            shouldOverride = PR_TRUE;
            (*aResult)->AppendElement(sheet);
          }

          // Advance to the next sheet URL.
          token = nsCRT::strtok( newStr, ", ", &newStr );
        }
        nsMemory::Free(str);
      }
    }
  }

  if (mScrollbarSheet && !shouldOverride)
    (*aResult)->AppendElement(mScrollbarSheet);

  if (mFormSheet)
    (*aResult)->AppendElement(mFormSheet);

  return NS_OK;
}

NS_IMETHODIMP
nsChromeRegistry::GetUserSheets(PRBool aIsChrome, nsISupportsArray **aResult)
{
  nsresult rv;

  if ((aIsChrome && mUserChromeSheet) || (!aIsChrome && mUserContentSheet)) {
    rv = NS_NewISupportsArray(aResult);
    if (NS_FAILED(rv)) return rv;
    if (aIsChrome && mUserChromeSheet) {
      rv = (*aResult)->AppendElement(mUserChromeSheet) ? NS_OK : NS_ERROR_FAILURE;
      if (NS_FAILED(rv)) return rv;
    }

    if (!aIsChrome && mUserContentSheet) {
      rv = (*aResult)->AppendElement(mUserContentSheet) ? NS_OK : NS_ERROR_FAILURE;
      if (NS_FAILED(rv)) return rv;
    }
  }
  return NS_OK;
}

nsresult nsChromeRegistry::LoadStyleSheet(nsICSSStyleSheet** aSheet, const nsCString& aURL)
{
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aURL);
  if (NS_FAILED(rv)) return rv;

  rv = LoadStyleSheetWithURL(uri, aSheet);
  return rv;
}

nsresult nsChromeRegistry::LoadStyleSheetWithURL(nsIURI* aURL, nsICSSStyleSheet** aSheet)
{
  nsCOMPtr<nsICSSLoader> loader;
  nsresult rv = nsComponentManager::CreateInstance(kCSSLoaderCID,
                                    nsnull,
                                    NS_GET_IID(nsICSSLoader),
                                    getter_AddRefs(loader));
  if (NS_FAILED(rv)) return rv;
  if (loader) {
      PRBool complete;
      rv = loader->LoadAgentSheet(aURL, *aSheet, complete,
                                  nsnull);
      if (NS_FAILED(rv)) return rv;
  }
  return NS_OK;
}

nsresult nsChromeRegistry::GetUserSheetURL(PRBool aIsChrome, nsCString & aURL)
{
  aURL = mProfileRoot;
  if (aIsChrome)
    aURL.Append("userChrome.css");
  else aURL.Append("userContent.css");

  return NS_OK;
}

nsresult nsChromeRegistry::GetFormSheetURL(nsCString& aURL)
{
  aURL = mUseXBLForms ? "chrome://forms/skin/forms.css" : "resource:/res/forms.css";

  return NS_OK;
}

nsresult nsChromeRegistry::LoadInstallDataSource()
{
    nsresult rv = GetInstallRoot(mInstallRoot);
    if (NS_FAILED(rv)) return rv;
    mInstallInitialized = PR_TRUE;
    return AddToCompositeDataSource(PR_FALSE);
}

nsresult nsChromeRegistry::LoadProfileDataSource()
{
  nsresult rv = GetProfileRoot(mProfileRoot);
  if (NS_SUCCEEDED(rv)) {
    // Load the profile search path for skins, content, and locales
    // Prepend them to our list of substitutions.
    mProfileInitialized = mInstallInitialized = PR_TRUE;
    mChromeDataSource = nsnull;
    rv = AddToCompositeDataSource(PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    // XXX this sucks ASS. This is a temporary hack until we get
    // around to fixing the skin switching bugs.
    // Select and Remove skins based on a pref set in a previous session.
    nsCOMPtr<nsIPref> pref(do_GetService("@mozilla.org/preferences;1"));
    if (pref) {
      nsXPIDLString skinToSelect;
      rv = pref->CopyUnicharPref("general.skins.selectedSkin", getter_Copies(skinToSelect));
      if (NS_SUCCEEDED(rv)) {
        rv = SelectSkin(skinToSelect, PR_TRUE);
        if (NS_SUCCEEDED(rv))
          pref->DeleteBranch("general.skins.selectedSkin");
      }
    }

    // We have to flush the chrome cache!
    rv = FlushCaches();
    if (NS_FAILED(rv)) return rv;

    rv = LoadStyleSheet(getter_AddRefs(mScrollbarSheet), nsCAutoString("chrome://global/skin/scrollbars.css"));
    // This must always be the last line of profile initialization!

    nsCAutoString sheetURL;
    rv = GetUserSheetURL(PR_TRUE, sheetURL);
    if (NS_FAILED(rv)) return rv;
    if (!sheetURL.IsEmpty()) {
      (void)LoadStyleSheet(getter_AddRefs(mUserChromeSheet), sheetURL);
      // it's ok to not have a user.css file
    }
    rv = GetUserSheetURL(PR_FALSE, sheetURL);
    if (NS_FAILED(rv)) return rv;
    if (!sheetURL.IsEmpty()) {
      (void)LoadStyleSheet(getter_AddRefs(mUserContentSheet), sheetURL);
      // it's ok not to have a userContent.css or userChrome.css file
    }
    rv = GetFormSheetURL(sheetURL);
    if (NS_FAILED(rv)) return rv;
    if (!sheetURL.IsEmpty())
      (void)LoadStyleSheet(getter_AddRefs(mFormSheet), sheetURL);
  }
  return NS_OK;
}

NS_IMETHODIMP nsChromeRegistry::AllowScriptsForSkin(nsIURI* aChromeURI, PRBool *aResult)
{
  *aResult = PR_TRUE;

  // split the url
  nsCAutoString package, provider, file;
  nsresult rv;
  rv = SplitURL(aChromeURI, package, provider, file);
  if (NS_FAILED(rv)) return NS_OK;

  // verify it's a skin url
  if (!provider.Equals("skin"))
    return NS_OK;

  // XXX could factor this with selectproviderforpackage
  // get the selected skin resource for the package
  nsCOMPtr<nsIRDFNode> selectedProvider;

  nsCAutoString resourceStr("urn:mozilla:package:");
  resourceStr += package;

  // Obtain the resource.
  nsCOMPtr<nsIRDFResource> resource;
  rv = GetResource(resourceStr, getter_AddRefs(resource));
  if (NS_FAILED(rv)) {
    NS_ERROR("Unable to obtain the package resource.");
    return rv;
  }

  if (NS_FAILED(rv = mChromeDataSource->GetTarget(resource, mSelectedSkin, PR_TRUE, getter_AddRefs(selectedProvider))))
    return NS_OK;

  if (!selectedProvider) {
    rv = FindProvider(package, provider, mSelectedSkin, getter_AddRefs(selectedProvider));
    if (NS_FAILED(rv)) return rv;
  }
  if (!selectedProvider)
    return NS_OK;

  resource = do_QueryInterface(selectedProvider, &rv);
  if (NS_SUCCEEDED(rv)) {
    // get its script access
    nsCAutoString scriptAccess;
    rv = nsChromeRegistry::FollowArc(mChromeDataSource,
                                     scriptAccess,
                                     resource,
                                     mAllowScripts);
    if (NS_FAILED(rv)) return rv;

    if (!scriptAccess.IsEmpty())
      *aResult = PR_FALSE;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsChromeRegistry::CheckForNewChrome()
{
  nsresult rv;

  rv = LoadInstallDataSource();
  if (NS_FAILED(rv)) return rv;

  // open the installed-chrome file
  nsCOMPtr<nsILocalFile> listFile;
  nsCOMPtr<nsIProperties> directoryService =
           do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    return rv;
  rv = directoryService->Get(NS_APP_CHROME_DIR, NS_GET_IID(nsILocalFile), getter_AddRefs(listFile));
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIFile> chromeFile;
  rv = listFile->Clone(getter_AddRefs(chromeFile));
  if (NS_FAILED(rv)) return rv;
  rv = chromeFile->AppendNative(kChromeFileName);
  if (NS_FAILED(rv)) return rv;
  nsInt64 chromeDate;
  (void)chromeFile->GetLastModifiedTime(&chromeDate.mValue);

  rv = listFile->AppendRelativeNativePath(kInstalledChromeFileName);
  if (NS_FAILED(rv)) return rv;
  nsInt64 listFileDate;
  (void)listFile->GetLastModifiedTime(&listFileDate.mValue);

  if (listFileDate < chromeDate)
    return NS_OK;

  PRFileDesc *file;
  rv = listFile->OpenNSPRFileDesc(PR_RDONLY, 0, &file);
  if (NS_FAILED(rv)) return rv;

  // file is open.

  PRFileInfo finfo;

  if (PR_GetOpenFileInfo(file, &finfo) == PR_SUCCESS) {
    char *dataBuffer = new char[finfo.size+1];
    if (dataBuffer) {
      PRInt32 bufferSize = PR_Read(file, dataBuffer, finfo.size);
      if (bufferSize > 0) {
        dataBuffer[bufferSize] = '\r'; //  be sure to terminate in a delimiter
        rv = ProcessNewChromeBuffer(dataBuffer, bufferSize);
      }
      delete [] dataBuffer;
    }
  }
  PR_Close(file);
  // listFile->Remove(PR_FALSE);

  return rv;
}

// flaming unthreadsafe function
nsresult
nsChromeRegistry::ProcessNewChromeBuffer(char *aBuffer, PRInt32 aLength)
{
  nsresult rv = NS_OK;
  char   *bufferEnd = aBuffer + aLength;
  char   *chromeType,      // "content", "locale" or "skin"
         *chromeProfile,   // "install" or "profile"
         *chromeLocType,   // type of location (local path or URL)
         *chromeLocation;  // base location of chrome (jar file)
  PRBool isProfile;
  PRBool isSelection;

  nsCAutoString content("content");
  nsCAutoString locale("locale");
  nsCAutoString skin("skin");
  nsCAutoString profile("profile");
  nsCAutoString select("select");
  nsCAutoString path("path");
  nsCAutoString fileURL;
  nsCAutoString chromeURL;

  mBatchInstallFlushes = PR_TRUE;

  while (aBuffer < bufferEnd) {
    // parse one line of installed-chrome.txt
    chromeType = aBuffer;
    while (aBuffer < bufferEnd && *aBuffer != ',')
      ++aBuffer;
    if (aBuffer >= bufferEnd)
      break;
    *aBuffer = '\0';

    chromeProfile = ++aBuffer;
    while (aBuffer < bufferEnd && *aBuffer != ',')
      ++aBuffer;
    if (aBuffer >= bufferEnd)
      break;
    *aBuffer = '\0';

    chromeLocType = ++aBuffer;
    while (aBuffer < bufferEnd && *aBuffer != ',')
      ++aBuffer;
    if (aBuffer >= bufferEnd)
      break;
    *aBuffer = '\0';

    chromeLocation = ++aBuffer;
    while (aBuffer < bufferEnd && (*aBuffer != '\r' && *aBuffer != '\n'))
      ++aBuffer;
    if (aBuffer >= bufferEnd)
      break;
    while (*--aBuffer == ' ')
      ;
    *++aBuffer = '\0';

    // process the parsed line
    isProfile = profile.Equals(chromeProfile);
    isSelection = select.Equals(chromeLocType);

    if (path.Equals(chromeLocType)) {
      // location is a (full) path. convert it to an URL.

      /* this is some convoluted shit... this creates a file, inits it with
       * the path parsed above (chromeLocation), makes a url, and inits it
       * with the file created. the purpose of this is just to have the
       * canonical url of the stupid thing.
       */
      nsCOMPtr<nsILocalFile> chromeFile(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv));
      if (NS_FAILED(rv))
        return rv;
      // assuming chromeLocation is given in the charset of the current locale
      rv = chromeFile->InitWithNativePath(nsDependentCString(chromeLocation));
      if (NS_FAILED(rv))
        return rv;

      /* xpidl strings aren't unified with strings, so this fu is necessary.
       * all we want here is the canonical url
       */
      rv = NS_GetURLSpecFromFile(chromeFile, chromeURL);
      if (NS_FAILED(rv)) return rv;

      /* if we're a file, we must be a jar file. do appropriate string munging.
       * otherwise, the string we got from GetSpec is fine.
       */
      PRBool isFile;
      rv = chromeFile->IsFile(&isFile);
      if (NS_FAILED(rv))
        return rv;

      if (isFile) {
        fileURL = "jar:";
        fileURL += chromeURL;
        fileURL += "!/";
        chromeURL = fileURL;
      }
    }
    else {
      // not "path". assume "url".
      chromeURL = chromeLocation;
    }

    // process the line
    if (skin.Equals(chromeType)) {
      if (isSelection) {
        nsAutoString name; name.AssignWithConversion(chromeLocation);
        rv = SelectSkin(name.get(), isProfile);
#ifdef DEBUG
        printf("***** Chrome Registration: Selecting skin %s as default\n", (const char*)chromeLocation);
#endif
      }
      else
        rv = InstallSkin(chromeURL.get(), isProfile, PR_FALSE);
    }
    else if (content.Equals(chromeType))
      rv = InstallPackage(chromeURL.get(), isProfile);
    else if (locale.Equals(chromeType)) {
      if (isSelection) {
        nsAutoString name; name.AssignWithConversion(chromeLocation);
        rv = SelectLocale(name.get(), isProfile);
#ifdef DEBUG
        printf("***** Chrome Registration: Selecting locale %s as default\n", (const char*)chromeLocation);
#endif
      }
      else
        rv = InstallLocale(chromeURL.get(), isProfile);
    }
    
    while (aBuffer < bufferEnd && (*aBuffer == '\0' || *aBuffer == ' ' || *aBuffer == '\r' || *aBuffer == '\n'))
      ++aBuffer;
  }

  mBatchInstallFlushes = PR_FALSE;
  nsCOMPtr<nsIRDFDataSource> dataSource;
  LoadDataSource(kChromeFileName, getter_AddRefs(dataSource), PR_FALSE, nsnull);
  nsCOMPtr<nsIRDFRemoteDataSource> remote(do_QueryInterface(dataSource));
  remote->Flush();
  return NS_OK;
}

PRBool
nsChromeRegistry::GetProviderCount(const nsCString& aProviderType, nsIRDFDataSource* aDataSource)
{
  nsresult rv;

  nsCAutoString rootStr("urn:mozilla:");
  rootStr += aProviderType;
  rootStr += ":root";

  // obtain the provider root resource
  nsCOMPtr<nsIRDFResource> resource;
  rv = GetResource(rootStr, getter_AddRefs(resource));
  if (NS_FAILED(rv))
    return 0;

  // wrap it in a container
  nsCOMPtr<nsIRDFContainer> container;
  rv = nsComponentManager::CreateInstance("@mozilla.org/rdf/container;1",
                                          nsnull,
                                          NS_GET_IID(nsIRDFContainer),
                                          getter_AddRefs(container));
  if (NS_FAILED(rv)) return 0;

  rv = container->Init(aDataSource, resource);
  if (NS_FAILED(rv)) return 0;

  PRInt32 count;
  container->GetCount(&count);
  return count;
}


NS_IMETHODIMP nsChromeRegistry::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *someData)
{
  nsresult rv = NS_OK;

  if (!nsCRT::strcmp("profile-before-change", aTopic)) {

    mChromeDataSource = nsnull;
    mScrollbarSheet = mFormSheet = nsnull;
    mInstallInitialized = mProfileInitialized = PR_FALSE;

    if (!nsCRT::strcmp("shutdown-cleanse", NS_ConvertUCS2toUTF8(someData).get())) {
      nsCOMPtr<nsIFile> userChromeDir;
      rv = NS_GetSpecialDirectory(NS_APP_USER_CHROME_DIR, getter_AddRefs(userChromeDir));
      if (NS_SUCCEEDED(rv) && userChromeDir)
        rv = userChromeDir->Remove(PR_TRUE);
    }
  }
  else if (!nsCRT::strcmp("profile-after-change", aTopic)) {
    if (!mProfileInitialized) {
      nsCOMPtr<nsIPref> prefService(do_GetService(kPrefServiceCID));
      if (prefService)
        prefService->GetBoolPref(kUseXBLFormsPref, &mUseXBLForms);

      rv = LoadProfileDataSource();
    }
  }

  return rv;
}

NS_IMETHODIMP nsChromeRegistry::CheckThemeVersion(const PRUnichar *aSkin,
                                                  PRBool* aResult)
{
  nsCAutoString provider("skin");
  return CheckProviderVersion(provider, aSkin, mSkinVersion, aResult);
}


NS_IMETHODIMP nsChromeRegistry::CheckLocaleVersion(const PRUnichar *aLocale,
                                                   PRBool* aResult)
{
  nsCAutoString provider("locale");
  return CheckProviderVersion(provider, aLocale, mLocaleVersion, aResult);
}


NS_IMETHODIMP nsChromeRegistry::CheckProviderVersion (const nsCString& aProviderType,
                                                      const PRUnichar* aProviderName,
                                                      nsIRDFResource* aSelectionArc,
                                                      PRBool *aCompatible)
{
  *aCompatible = PR_TRUE;

  // Build the provider resource str.
  // e.g., urn:mozilla:skin:aqua/1.0
  nsCAutoString resourceStr( "urn:mozilla:" );
  resourceStr += aProviderType;
  resourceStr += ":";
  resourceStr.AppendWithConversion(aProviderName);

  // Obtain the provider resource.
  nsresult rv = NS_OK;
  nsCOMPtr<nsIRDFResource> resource;
  rv = GetResource(resourceStr, getter_AddRefs(resource));
  if (NS_FAILED(rv)) {
    NS_ERROR("Unable to obtain the package resource.");
    return rv;
  }

  // Follow the packages arc to the package resources.
  nsCOMPtr<nsIRDFNode> packageList;
  rv = mChromeDataSource->GetTarget(resource, mPackages, PR_TRUE, getter_AddRefs(packageList));
  if (NS_FAILED(rv)) {
    NS_ERROR("Unable to obtain the SEQ for the package list.");
    return rv;
  }
  // ok for packageList to be null here -- it just means that we haven't encountered that package yet

  nsCOMPtr<nsIRDFResource> packageSeq(do_QueryInterface(packageList, &rv));
  if (NS_FAILED(rv)) return rv;

  // Build an RDF container to wrap the SEQ
  nsCOMPtr<nsIRDFContainer> container;
  rv = nsComponentManager::CreateInstance("@mozilla.org/rdf/container;1",
                                          nsnull,
                                          NS_GET_IID(nsIRDFContainer),
                                          getter_AddRefs(container));
  if (NS_FAILED(rv))
    return rv;

  rv = container->Init(mChromeDataSource, packageSeq);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsISimpleEnumerator> arcs;

  rv = container->GetElements(getter_AddRefs(arcs));
  if (NS_FAILED(rv))
    return NS_OK;

  // For each skin-package entry, follow the arcs to the real package
  // resource.
  PRBool more;
  rv = arcs->HasMoreElements(&more);
  if (NS_FAILED(rv)) return rv;
  while (more) {
    nsCOMPtr<nsISupports> packageSkinEntry;
    rv = arcs->GetNext(getter_AddRefs(packageSkinEntry));
    if (NS_SUCCEEDED(rv) && packageSkinEntry) {
      nsCOMPtr<nsIRDFResource> entry = do_QueryInterface(packageSkinEntry);
      if (entry) {

        nsCAutoString themePackageVersion;
        nsChromeRegistry::FollowArc(mChromeDataSource, themePackageVersion, entry, aSelectionArc);

        // Obtain the real package resource.
        nsCOMPtr<nsIRDFNode> packageNode;
        rv = mChromeDataSource->GetTarget(entry, mPackage, PR_TRUE, getter_AddRefs(packageNode));
        if (NS_FAILED(rv)) {
          NS_ERROR("Unable to obtain the package resource.");
          return rv;
        }

        nsCOMPtr<nsIRDFResource> packageResource(do_QueryInterface(packageNode));
        if (packageResource) {

          nsCAutoString packageVersion;
          nsChromeRegistry::FollowArc(mChromeDataSource, packageVersion, packageResource, aSelectionArc);

          nsCAutoString packageName;
          nsChromeRegistry::FollowArc(mChromeDataSource, packageName, packageResource, mName);

          if (packageName.IsEmpty())
            // the package is not represented for current version, so ignore it
            *aCompatible = PR_TRUE;
          else {
            if (packageVersion.IsEmpty() && themePackageVersion.IsEmpty())
              *aCompatible = PR_TRUE;
            else {
              if (!packageVersion.IsEmpty() && !themePackageVersion.IsEmpty())
                *aCompatible = ( themePackageVersion.Equals(packageVersion));
              else
                *aCompatible = PR_FALSE;
            }
          }

           // if just one theme package is NOT compatible, the theme will be disabled
           if (!(*aCompatible))
             return NS_OK;

         }
      }
    }
    rv = arcs->HasMoreElements(&more);
    if (NS_FAILED(rv))
      return rv;
  }

  return NS_OK;
}

