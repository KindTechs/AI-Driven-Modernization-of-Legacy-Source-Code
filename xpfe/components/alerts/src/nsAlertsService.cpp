/* -*- Mode: C++; tab-width: 2; indent-tabs-mode:nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Scott MacGregor <mscott@netscape.com>
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

#include "nsAlertsService.h"
#include "nsISupportsArray.h"
#include "nsISupportsPrimitives.h"
#include "nsIServiceManager.h"
#include "nsIDOMWindowInternal.h"
#include "nsIWindowWatcher.h"

#define ALERT_CHROME_URL "chrome://communicator/content/alerts/alert.xul"

NS_IMPL_THREADSAFE_ADDREF(nsAlertsService);
NS_IMPL_THREADSAFE_RELEASE(nsAlertsService);

NS_INTERFACE_MAP_BEGIN(nsAlertsService)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIAlertsService)
   NS_INTERFACE_MAP_ENTRY(nsIAlertsService)
NS_INTERFACE_MAP_END_THREADSAFE

nsAlertsService::nsAlertsService()
{
  NS_INIT_ISUPPORTS();
}

nsAlertsService::~nsAlertsService()
{}

NS_IMETHODIMP nsAlertsService::ShowAlertNotification(const char * aImageUrl, const PRUnichar * aAlertTitle, 
                                                     const PRUnichar * aAlertText, PRBool aAlertTextClickable,
                                                     const PRUnichar * aAlertCookie,
                                                     nsIAlertListener * aAlertListener)
{
  nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService("@mozilla.org/embedcomp/window-watcher;1"));
  nsCOMPtr<nsIDOMWindow> newWindow;
  nsresult rv;

  nsCOMPtr<nsISupportsArray> argsArray;
  rv = NS_NewISupportsArray(getter_AddRefs(argsArray));
  NS_ENSURE_SUCCESS(rv, rv);

  // create scriptable versions of our strings that we can store in our nsISupportsArray....
  nsCOMPtr<nsISupportsString> scriptableImageUrl (do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID));
  NS_ENSURE_TRUE(scriptableImageUrl, NS_ERROR_FAILURE);

  scriptableImageUrl->SetData(aImageUrl);
  argsArray->AppendElement(scriptableImageUrl);

  nsCOMPtr<nsISupportsWString> scriptableAlertTitle (do_CreateInstance(NS_SUPPORTS_WSTRING_CONTRACTID));
  NS_ENSURE_TRUE(scriptableAlertTitle, NS_ERROR_FAILURE);

  scriptableAlertTitle->SetData(aAlertTitle);
  argsArray->AppendElement(scriptableAlertTitle);

  nsCOMPtr<nsISupportsWString> scriptableAlertText (do_CreateInstance(NS_SUPPORTS_WSTRING_CONTRACTID));
  NS_ENSURE_TRUE(scriptableAlertText, NS_ERROR_FAILURE);

  scriptableAlertText->SetData(aAlertText);
  argsArray->AppendElement(scriptableAlertText);

  nsCOMPtr<nsISupportsPRBool> scriptableIsClickable (do_CreateInstance(NS_SUPPORTS_PRBOOL_CONTRACTID));
  NS_ENSURE_TRUE(scriptableIsClickable, NS_ERROR_FAILURE);

  scriptableIsClickable->SetData(aAlertTextClickable);
  argsArray->AppendElement(scriptableIsClickable);

  nsCOMPtr<nsISupportsWString> scriptableAlertCookie (do_CreateInstance(NS_SUPPORTS_WSTRING_CONTRACTID));
  NS_ENSURE_TRUE(scriptableAlertCookie, NS_ERROR_FAILURE);

  scriptableAlertCookie->SetData(aAlertCookie);
  argsArray->AppendElement(scriptableAlertCookie);

  if (aAlertListener)
  {
    nsCOMPtr<nsISupportsInterfacePointer> ifptr = do_CreateInstance(NS_SUPPORTS_INTERFACE_POINTER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsISupports> iSupports (do_QueryInterface(aAlertListener));
    ifptr->SetData(iSupports);
    ifptr->SetDataIID(&NS_GET_IID(nsIAlertListener));
    argsArray->AppendElement(ifptr);
  }
  
  rv = wwatch->OpenWindow(0, ALERT_CHROME_URL, "_blank",
                 "chrome,dialog=yes,titlebar=no,popup=yes", argsArray,
                 getter_AddRefs(newWindow));
  return rv;
}
