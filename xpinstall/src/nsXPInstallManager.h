/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *     Daniel Veditz <dveditz@netscape.com>
 */

#ifndef _NS_XPINSTALLMANAGER_H
#define _NS_XPINSTALLMANAGER_H

#include "nsInstall.h"

#include "nscore.h"
#include "nsISupports.h"
#include "nsString.h"

#include "nsIURL.h"
#include "nsIInputStream.h"
#include "nsIStreamListener.h"
#include "nsIXPINotifier.h"
#include "nsIXPIDialogService.h"
#include "nsXPITriggerInfo.h"
#include "nsIXPIProgressDialog.h"
#include "nsIChromeRegistry.h"
#include "nsIDOMWindowInternal.h"
#include "nsIObserver.h"

#include "nsISoftwareUpdate.h"

#include "nsCOMPtr.h"

#include "nsIProgressEventSink.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"

#include "nsIDialogParamBlock.h"

#define NS_XPIDIALOGSERVICE_CONTRACTID "@mozilla.org/embedui/xpinstall-dialog-service;1"
#define XPI_PROGRESS_TOPIC "xpinstall-progress"

class nsXPInstallManager : public nsIXPIListener,
                           public nsIXPIDialogService,
                           public nsIObserver,
                           public nsIStreamListener,
                           public nsIProgressEventSink,
                           public nsIInterfaceRequestor
{
    public:
        nsXPInstallManager();
        virtual ~nsXPInstallManager();

        NS_DECL_ISUPPORTS
        NS_DECL_NSIXPILISTENER
        NS_DECL_NSIXPIDIALOGSERVICE
        NS_DECL_NSIOBSERVER
        NS_DECL_NSISTREAMLISTENER
        NS_DECL_NSIPROGRESSEVENTSINK
        NS_DECL_NSIREQUESTOBSERVER
        NS_DECL_NSIINTERFACEREQUESTOR

        NS_IMETHOD InitManager(nsIScriptGlobalObject* aGlobalObject, nsXPITriggerInfo* aTrigger, PRUint32 aChromeType );

    private:
        NS_IMETHOD  DownloadNext();
        void        Shutdown();
        NS_IMETHOD  GetDestinationFile(nsString& url, nsILocalFile* *file);
        NS_IMETHOD  LoadParams(PRUint32 aCount, const PRUnichar** aPackageList, nsIDialogParamBlock** aParams);
        PRBool      ConfirmChromeInstall(nsIDOMWindowInternal* aParentWindow, const PRUnichar** aPackage);
        PRBool      TimeToUpdate(PRTime now);
        PRInt32     GetIndexFromURL(const PRUnichar* aUrl);

        nsXPITriggerInfo*   mTriggers;
        nsXPITriggerItem*   mItem;
        PRTime              mLastUpdate;
        PRUint32            mNextItem;
        PRInt32             mNumJars;
        PRUint32            mChromeType;
        PRInt32             mContentLength;
        PRBool              mDialogOpen;
        PRBool              mCancelled;
        PRBool              mSelectChrome;
        PRBool              mNeedsShutdown;

        nsCOMPtr<nsIXPIProgressDialog>  mDlg;
        nsCOMPtr<nsIStringBundle>       mStringBundle;
        nsCOMPtr<nsISoftwareUpdate>     mInstallSvc;
};

#endif
