/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
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
 * The Original Code is Mozilla Communicator client code,
 * released March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *     Conrad Carlen <conrad@ingress.com>
 */

#include "nsAppFileLocationProvider.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIAtom.h"
#include "nsILocalFile.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsISimpleEnumerator.h"
#include "prenv.h"
#include "nsCRT.h"

#if defined(XP_MAC) /* || defined(XP_MACOSX) REMIND HACKING FOR MACOS X!!! */
#include <Folders.h>
#include <Script.h>
#include <Processes.h>
#include <Gestalt.h>
#include "nsILocalFileMac.h"
#elif defined(XP_OS2)
#define INCL_DOSPROCESS
#define INCL_DOSMODULEMGR
#include <os2.h>
#elif defined(XP_PC)
#include <windows.h>
#include <shlobj.h>
#elif defined(XP_UNIX)
#include <unistd.h>
#include <stdlib.h>
#include <sys/param.h>
#elif defined(XP_BEOS)
#include <sys/param.h>
#include <kernel/image.h>
#include <storage/FindDirectory.h>
#endif


// WARNING: These hard coded names need to go away. They need to
// come from localizable resources

#if defined(XP_MAC) /* || defined(XP_MACOSX) REMIND HACKING FOR MACOS X!!! */
#define APP_REGISTRY_NAME NS_LITERAL_CSTRING("Application Registry")
#define ESSENTIAL_FILES   NS_LITERAL_CSTRING("Essential Files")
#elif defined(XP_WIN) || defined(XP_OS2)
#define APP_REGISTRY_NAME NS_LITERAL_CSTRING("registry.dat")
#else
#define APP_REGISTRY_NAME NS_LITERAL_CSTRING("appreg")
#endif

// define default product directory
#if defined(XP_WIN) || defined(XP_MAC) || defined(XP_OS2) || defined(XP_BEOS)
#define DEFAULT_PRODUCT_DIR NS_LITERAL_CSTRING("Mozilla")
#elif defined (XP_UNIX)
#define DEFAULT_PRODUCT_DIR NS_LITERAL_CSTRING(".mozilla")
#endif

// Locally defined keys used by nsAppDirectoryEnumerator
#define NS_ENV_PLUGINS_DIR          "EnvPlugins"    // env var MOZ_PLUGIN_PATH
#define NS_USER_PLUGINS_DIR         "UserPlugins"

#if XP_MAC
#define DEFAULTS_DIR_NAME           NS_LITERAL_CSTRING("Defaults")
#define DEFAULTS_PREF_DIR_NAME      NS_LITERAL_CSTRING("Pref")
#define DEFAULTS_PROFILE_DIR_NAME   NS_LITERAL_CSTRING("Profile")
#define RES_DIR_NAME                NS_LITERAL_CSTRING("Res")
#define CHROME_DIR_NAME             NS_LITERAL_CSTRING("Chrome")
#define PLUGINS_DIR_NAME            NS_LITERAL_CSTRING("Plug-ins")
#define SEARCH_DIR_NAME             NS_LITERAL_CSTRING("Search Plugins")
#else
#define DEFAULTS_DIR_NAME           NS_LITERAL_CSTRING("defaults")
#define DEFAULTS_PREF_DIR_NAME      NS_LITERAL_CSTRING("pref")
#define DEFAULTS_PROFILE_DIR_NAME   NS_LITERAL_CSTRING("profile")
#define RES_DIR_NAME                NS_LITERAL_CSTRING("res")
#define CHROME_DIR_NAME             NS_LITERAL_CSTRING("chrome")
#define PLUGINS_DIR_NAME            NS_LITERAL_CSTRING("plugins")
#define SEARCH_DIR_NAME             NS_LITERAL_CSTRING("searchplugins")
#endif

//*****************************************************************************
// nsAppFileLocationProvider::Constructor/Destructor
//*****************************************************************************

nsAppFileLocationProvider::nsAppFileLocationProvider()
{
    NS_INIT_ISUPPORTS();
}

nsAppFileLocationProvider::~nsAppFileLocationProvider()
{
}

//*****************************************************************************
// nsAppFileLocationProvider::nsISupports
//*****************************************************************************

NS_IMPL_THREADSAFE_ISUPPORTS2(nsAppFileLocationProvider, nsIDirectoryServiceProvider, nsIDirectoryServiceProvider2)

//*****************************************************************************
// nsAppFileLocationProvider::nsIDirectoryServiceProvider
//*****************************************************************************

NS_IMETHODIMP
nsAppFileLocationProvider::GetFile(const char *prop, PRBool *persistant, nsIFile **_retval)
{
    nsCOMPtr<nsILocalFile>  localFile;
    nsresult rv = NS_ERROR_FAILURE;

    NS_ENSURE_ARG(prop);
    *_retval = nsnull;
    *persistant = PR_TRUE;
    
    if (nsCRT::strcmp(prop, NS_APP_APPLICATION_REGISTRY_DIR) == 0)
    {
        rv = GetProductDirectory(getter_AddRefs(localFile));
    }
    else if (nsCRT::strcmp(prop, NS_APP_APPLICATION_REGISTRY_FILE) == 0)
    {
        rv = GetProductDirectory(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv))
            rv = localFile->AppendNative(APP_REGISTRY_NAME);
    }
    else if (nsCRT::strcmp(prop, NS_APP_DEFAULTS_50_DIR) == 0)
    {
        rv = CloneMozBinDirectory(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv))
            rv = localFile->AppendRelativeNativePath(DEFAULTS_DIR_NAME);
    }
    else if (nsCRT::strcmp(prop, NS_APP_PREF_DEFAULTS_50_DIR) == 0)
    {
        rv = CloneMozBinDirectory(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv)) {
            rv = localFile->AppendRelativeNativePath(DEFAULTS_DIR_NAME);
            if (NS_SUCCEEDED(rv))
                rv = localFile->AppendRelativeNativePath(DEFAULTS_PREF_DIR_NAME);
        }
    }
    else if (nsCRT::strcmp(prop, NS_APP_PROFILE_DEFAULTS_50_DIR) == 0 ||
             nsCRT::strcmp(prop, NS_APP_PROFILE_DEFAULTS_NLOC_50_DIR) == 0)
    {
        rv = CloneMozBinDirectory(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv)) {
            rv = localFile->AppendRelativeNativePath(DEFAULTS_DIR_NAME);
            if (NS_SUCCEEDED(rv))
                rv = localFile->AppendRelativeNativePath(DEFAULTS_PROFILE_DIR_NAME);
        }
    }
    else if (nsCRT::strcmp(prop, NS_APP_USER_PROFILES_ROOT_DIR) == 0)
    {
        rv = GetDefaultUserProfileRoot(getter_AddRefs(localFile));
    }
    else if (nsCRT::strcmp(prop, NS_APP_RES_DIR) == 0)
    {
        rv = CloneMozBinDirectory(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv))
            rv = localFile->AppendRelativeNativePath(RES_DIR_NAME);
    }
    else if (nsCRT::strcmp(prop, NS_APP_CHROME_DIR) == 0)
    {
        rv = CloneMozBinDirectory(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv))
            rv = localFile->AppendRelativeNativePath(CHROME_DIR_NAME);
    }
    else if (nsCRT::strcmp(prop, NS_APP_PLUGINS_DIR) == 0)
    {
        rv = CloneMozBinDirectory(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv))
            rv = localFile->AppendRelativeNativePath(PLUGINS_DIR_NAME);
    }
    else if (nsCRT::strcmp(prop, NS_ENV_PLUGINS_DIR) == 0)
    {
        const char *pathVar = PR_GetEnv("MOZ_PLUGIN_PATH");
        if (pathVar)
            rv = NS_NewNativeLocalFile(nsDependentCString(pathVar), PR_TRUE, getter_AddRefs(localFile));
    }
    else if (nsCRT::strcmp(prop, NS_USER_PLUGINS_DIR) == 0)
    {
        rv = GetProductDirectory(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv))
            rv = localFile->AppendRelativeNativePath(PLUGINS_DIR_NAME);
    }
    else if (nsCRT::strcmp(prop, NS_APP_SEARCH_DIR) == 0)
    {
        rv = CloneMozBinDirectory(getter_AddRefs(localFile));
        if (NS_SUCCEEDED(rv))
            rv = localFile->AppendRelativeNativePath(SEARCH_DIR_NAME);
    }
    else if (nsCRT::strcmp(prop, NS_APP_INSTALL_CLEANUP_DIR) == 0)
    {   
        // This is cloned so that embeddors will have a hook to override
        // with their own cleanup dir.  See bugzilla bug #105087 
        rv = CloneMozBinDirectory(getter_AddRefs(localFile));
#ifdef XP_MAC
        if (NS_SUCCEEDED(rv))
            rv = localFile->AppendNative(ESSENTIAL_FILES);
#endif

    } 

    if (localFile && NS_SUCCEEDED(rv))
        return localFile->QueryInterface(NS_GET_IID(nsIFile), (void**)_retval);
        
    return rv;
}


NS_METHOD nsAppFileLocationProvider::CloneMozBinDirectory(nsILocalFile **aLocalFile)
{
    NS_ENSURE_ARG_POINTER(aLocalFile);
    nsresult rv;

    if (!mMozBinDirectory)
    {
        // Get the mozilla bin directory
        // 1. Check the directory service first for NS_XPCOM_CURRENT_PROCESS_DIR
        //    This will be set if a directory was passed to NS_InitXPCOM
        // 2. If that doesn't work, set it to be the current process directory
        nsCOMPtr<nsIProperties>
          directoryService(do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv));
        if (NS_FAILED(rv))
            return rv;

        rv = directoryService->Get(NS_XPCOM_CURRENT_PROCESS_DIR, NS_GET_IID(nsIFile), getter_AddRefs(mMozBinDirectory));
        if (NS_FAILED(rv)) {
            rv = directoryService->Get(NS_OS_CURRENT_PROCESS_DIR, NS_GET_IID(nsIFile), getter_AddRefs(mMozBinDirectory));
            if (NS_FAILED(rv))
                return rv;
        }
    }

    nsCOMPtr<nsIFile> aFile;
    rv = mMozBinDirectory->Clone(getter_AddRefs(aFile));
    if (NS_FAILED(rv))
        return rv;

    nsCOMPtr<nsILocalFile> lfile = do_QueryInterface (aFile);
    if (!lfile)
        return NS_ERROR_FAILURE;

    NS_IF_ADDREF(*aLocalFile = lfile);
    return NS_OK;
}


//----------------------------------------------------------------------------------------
// GetProductDirectory - Gets the directory which contains the application data folder
//
// UNIX   : ~/.mozilla/
// WIN    : <Application Data folder on user's machine>\Mozilla
// Mac    : :Documents:Mozilla:
//----------------------------------------------------------------------------------------
NS_METHOD nsAppFileLocationProvider::GetProductDirectory(nsILocalFile **aLocalFile)
{
    NS_ENSURE_ARG_POINTER(aLocalFile);

    nsresult rv;
    PRBool exists;
    nsCOMPtr<nsILocalFile> localDir;

#if defined(XP_MAC) /* || defined(XP_MACOSX) REMIND HACKING FOR MACOS X!!! */
    nsCOMPtr<nsIProperties> directoryService = 
             do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;
    OSErr   err;
    long    response;
    err = ::Gestalt(gestaltSystemVersion, &response);
    const char *prop = (!err && response >= 0x00001000) ? NS_MAC_USER_LIB_DIR : NS_MAC_DOCUMENTS_DIR;
    rv = directoryService->Get(prop, NS_GET_IID(nsILocalFile), getter_AddRefs(localDir));
    if (NS_FAILED(rv)) return rv;   
#elif defined(XP_OS2)
    nsCOMPtr<nsIProperties> directoryService = 
             do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;
    rv = directoryService->Get(NS_OS2_HOME_DIR, NS_GET_IID(nsILocalFile), getter_AddRefs(localDir));
    if (NS_FAILED(rv)) return rv;
#elif defined(XP_PC)
    nsCOMPtr<nsIProperties> directoryService = 
             do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;
    rv = directoryService->Get(NS_WIN_APPDATA_DIR, NS_GET_IID(nsILocalFile), getter_AddRefs(localDir));
    if (NS_SUCCEEDED(rv))
        rv = localDir->Exists(&exists);
    if (NS_FAILED(rv) || !exists)
    {
        // On some Win95 machines, NS_WIN_APPDATA_DIR does not exist - revert to NS_WIN_WINDOWS_DIR
        localDir = nsnull;
        rv = directoryService->Get(NS_WIN_WINDOWS_DIR, NS_GET_IID(nsILocalFile), getter_AddRefs(localDir));
    }
    if (NS_FAILED(rv)) return rv;
#elif defined(XP_UNIX)
    rv = NS_NewNativeLocalFile(nsDependentCString(PR_GetEnv("HOME")), PR_TRUE, getter_AddRefs(localDir));
    if (NS_FAILED(rv)) return rv;
#elif defined(XP_BEOS)
    char path[MAXPATHLEN];
    find_directory(B_USER_SETTINGS_DIRECTORY, 0, 0, path, MAXPATHLEN);
    // Need enough space to add the trailing backslash
    int len = strlen(path);
    if (len > MAXPATHLEN-2)
        return NS_ERROR_FAILURE;
    path[len]   = '/';
    path[len+1] = '\0';
    rv = NS_NewNativeLocalFile(nsDependentCString(path), PR_TRUE, getter_AddRefs(localDir));
    if (NS_FAILED(rv)) return rv;
#else
#error dont_know_how_to_get_product_dir_on_your_platform
#endif

    rv = localDir->AppendRelativeNativePath(DEFAULT_PRODUCT_DIR);
    if (NS_FAILED(rv)) return rv;
    rv = localDir->Exists(&exists);
    if (NS_SUCCEEDED(rv) && !exists)
        rv = localDir->Create(nsIFile::DIRECTORY_TYPE, 0775);
    if (NS_FAILED(rv)) return rv;

    *aLocalFile = localDir;
    NS_ADDREF(*aLocalFile);

   return rv;
}


//----------------------------------------------------------------------------------------
// GetDefaultUserProfileRoot - Gets the directory which contains each user profile dir
//
// UNIX   : ~/.mozilla/
// WIN    : <Application Data folder on user's machine>\Mozilla\Profiles
// Mac    : :Documents:Mozilla:Profiles:
//----------------------------------------------------------------------------------------
NS_METHOD nsAppFileLocationProvider::GetDefaultUserProfileRoot(nsILocalFile **aLocalFile)
{
    NS_ENSURE_ARG_POINTER(aLocalFile);

    nsresult rv;
    nsCOMPtr<nsILocalFile> localDir;

    rv = GetProductDirectory(getter_AddRefs(localDir));
    if (NS_FAILED(rv)) return rv;

#if defined(XP_MAC) || defined(XP_MACOSX) || defined(XP_OS2) || defined(XP_PC)
    // These 3 platforms share this part of the path - do them as one
    rv = localDir->AppendRelativeNativePath(NS_LITERAL_CSTRING("Profiles"));
    if (NS_FAILED(rv)) return rv;

    PRBool exists;
    rv = localDir->Exists(&exists);
    if (NS_SUCCEEDED(rv) && !exists)
        rv = localDir->Create(nsIFile::DIRECTORY_TYPE, 0775);
    if (NS_FAILED(rv)) return rv;
#endif

    *aLocalFile = localDir;
    NS_ADDREF(*aLocalFile);

   return rv;
}

//*****************************************************************************
// nsAppFileLocationProvider::nsIDirectoryServiceProvider2
//*****************************************************************************

class nsAppDirectoryEnumerator : public nsISimpleEnumerator
{
  public:
    NS_DECL_ISUPPORTS

    /**
     * aKeyList is a list of properties which are provided by aProvider
     * They do not need to be publicly defined keys.
     */
    nsAppDirectoryEnumerator(nsIDirectoryServiceProvider *aProvider,
                             const char* aKeyList[],
                             PRInt32 aNumKeys) :
        mProvider(aProvider),
        mKeyList(aKeyList),
        mCurrentIndex(0), mMaxIndex(aNumKeys)
    {
        NS_INIT_REFCNT();
    }

    NS_IMETHOD HasMoreElements(PRBool *result) 
    {
        while (!mNext && (mCurrentIndex < mMaxIndex))
        {
            PRBool dontCare;
            (void)mProvider->GetFile(mKeyList[mCurrentIndex++], &dontCare, getter_AddRefs(mNext));
        }
        *result = mNext != nsnull;
        return NS_OK;
    }

    NS_IMETHOD GetNext(nsISupports **result) 
    {
        NS_ENSURE_ARG_POINTER(result);
        *result = nsnull;

        PRBool hasMore;
        HasMoreElements(&hasMore);
        if (!hasMore)
            return NS_ERROR_FAILURE;
            
        *result = mNext;
        NS_IF_ADDREF(*result);
        mNext = nsnull;
        
        return *result ? NS_OK : NS_ERROR_FAILURE;
    }

    virtual ~nsAppDirectoryEnumerator()
    {
    }

  protected:
    nsIDirectoryServiceProvider *mProvider;
    const char** mKeyList;
    PRInt32      mCurrentIndex, mMaxIndex;
    nsCOMPtr<nsIFile> mNext;
};

NS_IMPL_ISUPPORTS1(nsAppDirectoryEnumerator, nsISimpleEnumerator)

NS_IMETHODIMP
nsAppFileLocationProvider::GetFiles(const char *prop, nsISimpleEnumerator **_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    *_retval = nsnull;
    nsresult rv = NS_ERROR_FAILURE;
    
    if (!nsCRT::strcmp(prop, NS_APP_PLUGINS_DIR_LIST))
    {
#ifdef XP_MAC
        static const char* keys[] = { NS_APP_PLUGINS_DIR };
#else
        static const char* keys[] = { NS_ENV_PLUGINS_DIR, NS_USER_PLUGINS_DIR, NS_APP_PLUGINS_DIR };
#endif

        *_retval = new nsAppDirectoryEnumerator(this, keys, sizeof(keys) / sizeof(keys[0]));
        NS_IF_ADDREF(*_retval);
        rv = *_retval ? NS_OK : NS_ERROR_OUT_OF_MEMORY;        
    }
    return rv;
}
