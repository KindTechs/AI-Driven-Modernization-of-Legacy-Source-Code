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

#include "nsIServiceManager.h"
#include "nsIPref.h"
#include "nsEscape.h"
#include "nsSmtpServer.h"
#include "nsIObserverService.h"
#include "nsNetUtil.h"
#include "nsIAuthPrompt.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsISmtpUrl.h"
#include "nsCRT.h"

NS_IMPL_ADDREF(nsSmtpServer)
NS_IMPL_RELEASE(nsSmtpServer)
NS_INTERFACE_MAP_BEGIN(nsSmtpServer)
    NS_INTERFACE_MAP_ENTRY(nsISmtpServer)
    NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISmtpServer)
NS_INTERFACE_MAP_END

nsSmtpServer::nsSmtpServer()
{
    NS_INIT_REFCNT();    
}

nsSmtpServer::~nsSmtpServer()
{
}

NS_IMETHODIMP
nsSmtpServer::GetKey(char * *aKey)
{
    if (!aKey) return NS_ERROR_NULL_POINTER;
    if (mKey.IsEmpty())
        *aKey = nsnull;
    else
        *aKey = ToNewCString(mKey);
    return NS_OK;
}

NS_IMETHODIMP
nsSmtpServer::SetKey(const char * aKey)
{
    mKey = aKey;
    return NS_OK;
}

NS_IMETHODIMP
nsSmtpServer::GetHostname(char * *aHostname)
{
    nsresult rv;
    nsCAutoString pref;
    NS_ENSURE_ARG_POINTER(aHostname);
    nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID, &rv));
    if (NS_FAILED(rv))
        return rv;
    getPrefString("hostname", pref);
    rv = prefs->CopyCharPref(pref.get(), aHostname);
    if (NS_FAILED(rv))
        *aHostname=nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsSmtpServer::SetHostname(const char * aHostname)
{
    nsresult rv;
    nsCAutoString pref;
    nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID, &rv));
    if (NS_FAILED(rv))
        return rv;
    getPrefString("hostname", pref);
    if (aHostname)
        return prefs->SetCharPref(pref.get(), aHostname);
    else
        prefs->ClearUserPref(pref.get());
    return NS_OK;
}

// if GetPort returns 0, it means default port
NS_IMETHODIMP
nsSmtpServer::GetPort(PRInt32 *aPort)
{
    nsresult rv;
    nsCAutoString pref;
    NS_ENSURE_ARG_POINTER(aPort);
    nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID, &rv));
    if (NS_FAILED(rv))
        return rv;
    getPrefString("port", pref);
    rv = prefs->GetIntPref(pref.get(), aPort);
    if (NS_FAILED(rv))
        *aPort = 0;
    return NS_OK;
}

NS_IMETHODIMP
nsSmtpServer::SetPort(PRInt32 aPort)
{
    nsresult rv;
    nsCAutoString pref;
    nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID, &rv));
    if (NS_FAILED(rv))
        return rv;
    getPrefString("port", pref);
    if (aPort)
        return prefs->SetIntPref(pref.get(), aPort);
    else
        prefs->ClearUserPref(pref.get());
    return NS_OK;
}

NS_IMETHODIMP
nsSmtpServer::GetDisplayname(char * *aDisplayname)
{
    nsresult rv;
    NS_ENSURE_ARG_POINTER(aDisplayname);

    nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID, &rv));
    if (NS_FAILED(rv))
        return rv;

    nsCAutoString hpref;
    getPrefString("hostname", hpref);

    nsCAutoString ppref;
    getPrefString("port", ppref);

    nsXPIDLCString hostname;
    rv = prefs->CopyCharPref(hpref.get(), getter_Copies(hostname));
    if (NS_FAILED(rv)) {
        *aDisplayname=nsnull;
        return NS_OK;
    }
    PRInt32 port;
    rv = prefs->GetIntPref(ppref.get(), &port);
    if (NS_FAILED(rv))
        port = 0;
    
    if (port) {
        nsCAutoString combined;
        combined = hostname.get();
        combined += ":";
        combined.AppendInt(port);
        *aDisplayname = ToNewCString(combined);
    }
    else {
        *aDisplayname = ToNewCString(hostname);
    }
    
    return NS_OK;
}

NS_IMETHODIMP
nsSmtpServer::GetTrySSL(PRInt32 *trySSL)
{
    nsresult rv;
    nsCAutoString pref;
    NS_ENSURE_ARG_POINTER(trySSL);
    nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID, &rv));
    if (NS_FAILED(rv)) return rv;
    *trySSL= 0;
    getPrefString("try_ssl", pref);
    rv = prefs->GetIntPref(pref.get(), trySSL);
    if (NS_FAILED(rv))
        getDefaultIntPref(prefs, 0, "try_ssl", trySSL);
    return NS_OK;
}

NS_IMETHODIMP
nsSmtpServer::SetTrySSL(PRInt32 trySSL)
{
    nsresult rv;
    nsCAutoString pref;
    nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID, &rv));
    if (NS_FAILED(rv))
        return rv;
    getPrefString("try_ssl", pref);
    return prefs->SetIntPref(pref.get(), trySSL);
}

NS_IMETHODIMP
nsSmtpServer::GetAuthMethod(PRInt32 *authMethod)
{
    nsresult rv;
    nsCAutoString pref;
    NS_ENSURE_ARG_POINTER(authMethod);
    nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID, &rv));
    if (NS_FAILED(rv))
        return rv;
    *authMethod = 1;
    getPrefString("auth_method", pref);
    rv = prefs->GetIntPref(pref.get(), authMethod);
    if (NS_FAILED(rv))
        rv = getDefaultIntPref(prefs, 1, "auth_method", authMethod);
    return rv;
}

nsresult
nsSmtpServer::getDefaultIntPref(nsIPref *prefs,
                                PRInt32 defVal,
                                const char *prefName,
                                PRInt32 *val)
{
  // mail.smtpserver.default.<prefName>  
  nsCAutoString fullPrefName;
  fullPrefName = "mail.smtpserver.default.";
  fullPrefName.Append(prefName);
  nsresult rv = prefs->GetIntPref(fullPrefName.get(), val);

  if (NS_FAILED(rv))
  { // last resort
    *val = defVal;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSmtpServer::SetAuthMethod(PRInt32 authMethod)
{
    nsresult rv;
    nsCAutoString pref;
    nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID, &rv));
    if (NS_FAILED(rv)) return rv;
    getPrefString("auth_method", pref);
    return prefs->SetIntPref(pref.get(), authMethod);
}

NS_IMETHODIMP
nsSmtpServer::GetUsername(char * *aUsername)
{
    nsresult rv;
    nsCAutoString pref;
    NS_ENSURE_ARG_POINTER(aUsername);
    nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID, &rv));
    if (NS_FAILED(rv))
        return rv;
    getPrefString("username", pref);
    rv = prefs->CopyCharPref(pref.get(), aUsername);
    if (NS_FAILED(rv))
        *aUsername = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsSmtpServer::SetUsername(const char * aUsername)
{
    nsresult rv;
    nsCAutoString pref;
    nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID, &rv));
    if (NS_FAILED(rv))
        return rv;
    getPrefString("username", pref);
    if (aUsername)
        return prefs->SetCharPref(pref.get(), aUsername);
    else
        prefs->ClearUserPref(pref.get());
    return NS_OK;
}

NS_IMETHODIMP
nsSmtpServer::GetPassword(char * *aPassword)
{
    NS_ENSURE_ARG_POINTER(aPassword);
    *aPassword = ToNewCString(m_password);
    return NS_OK;
}

NS_IMETHODIMP
nsSmtpServer::SetPassword(const char * aPassword)
{
    m_password = aPassword;
    return NS_OK;
}

NS_IMETHODIMP
nsSmtpServer::GetPasswordWithUI(const PRUnichar * aPromptMessage, const
                                PRUnichar *aPromptTitle, 
                                nsIAuthPrompt* aDialog,
                                char **aPassword) 
{
    nsresult rv = NS_OK;

    NS_ENSURE_ARG_POINTER(aPassword);

    if (m_password.IsEmpty())
    {
        NS_ENSURE_ARG_POINTER(aDialog);

        // prompt the user for the password
        if (NS_SUCCEEDED(rv))
        {
            nsXPIDLString uniPassword;
            PRBool okayValue = PR_TRUE;
            nsXPIDLCString serverUri;
            rv = GetServerURI(getter_Copies(serverUri));
            if (NS_FAILED(rv))
                return rv;
            rv = aDialog->PromptPassword(aPromptTitle, aPromptMessage, 
                    NS_ConvertASCIItoUCS2(serverUri).get(), nsIAuthPrompt::SAVE_PASSWORD_PERMANENTLY,
                    getter_Copies(uniPassword), &okayValue);
            if (NS_FAILED(rv))
                return rv;

            if (!okayValue) // if the user pressed cancel, just return NULL;
            {
                *aPassword = nsnull;
                return rv;
            }

            // we got a password back...so remember it
            nsCString aCStr; aCStr.AssignWithConversion(uniPassword); 

            rv = SetPassword(aCStr.get());
            if (NS_FAILED(rv))
                return rv;
        } // if we got a prompt dialog
    } // if the password is empty

    rv = GetPassword(aPassword);
    return rv;
}

NS_IMETHODIMP
nsSmtpServer::GetUsernamePasswordWithUI(const PRUnichar * aPromptMessage, const
                                PRUnichar *aPromptTitle, 
                                nsIAuthPrompt* aDialog,
                                char **aUsername,
                                char **aPassword) 
{
    nsresult rv = NS_OK;

    NS_ENSURE_ARG_POINTER(aUsername);
    NS_ENSURE_ARG_POINTER(aPassword);

    if (m_password.IsEmpty()) {
        NS_ENSURE_ARG_POINTER(aDialog);
        
        // prompt the user for the password
        if (NS_SUCCEEDED(rv))
        {
            nsXPIDLString uniUsername;
            nsXPIDLString uniPassword;
            PRBool okayValue = PR_TRUE;
            nsXPIDLCString serverUri;
            rv = GetServerURI(getter_Copies(serverUri));
            if (NS_FAILED(rv))
                return rv;
            rv = aDialog->PromptUsernameAndPassword(aPromptTitle, aPromptMessage, 
                                         NS_ConvertASCIItoUCS2(serverUri).get(), nsIAuthPrompt::SAVE_PASSWORD_PERMANENTLY,
                                         getter_Copies(uniUsername), getter_Copies(uniPassword), &okayValue);
            if (NS_FAILED(rv))
                return rv;
				
            if (!okayValue) // if the user pressed cancel, just return NULL;
            {
                *aUsername = nsnull;
                *aPassword = nsnull;
                return rv;
            }

            // we got a userid and password back...so remember it
            nsCString aCStr; 

            aCStr.AssignWithConversion(uniUsername); 
            rv = SetUsername(aCStr.get());
            if (NS_FAILED(rv))
                return rv;

            aCStr.AssignWithConversion(uniPassword); 
            rv = SetPassword(aCStr.get());
            if (NS_FAILED(rv))
                return rv;
        } // if we got a prompt dialog
    } // if the password is empty

    rv = GetUsername(aUsername);
    if (NS_FAILED(rv))
        return rv;
    rv = GetPassword(aPassword);
    return rv;
}

NS_IMETHODIMP
nsSmtpServer::ForgetPassword()
{
    nsresult rv;
    nsCOMPtr<nsIObserverService> observerService = do_GetService("@mozilla.org/observer-service;1", &rv);
    NS_ENSURE_SUCCESS(rv,rv);

    nsXPIDLCString serverUri;
    rv = GetServerURI(getter_Copies(serverUri));
    if (NS_FAILED(rv))
        return rv;

    nsCOMPtr<nsIURI> uri;
    NS_NewURI(getter_AddRefs(uri), serverUri);
    rv = observerService->NotifyObservers(uri, "login-failed", nsnull);
    NS_ENSURE_SUCCESS(rv,rv);

    rv = SetPassword("");
    return rv;
}

NS_IMETHODIMP
nsSmtpServer::GetServerURI(char **aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    nsresult rv;
    nsCAutoString uri;

    uri += "smtp";
    uri += "://";

    nsXPIDLCString username;
    rv = GetUsername(getter_Copies(username));

    if (NS_SUCCEEDED(rv) && ((const char*)username) && username[0]) {
        nsXPIDLCString escapedUsername;
        *((char **)getter_Copies(escapedUsername)) =
            nsEscape(username, url_XAlphas);
//            nsEscape(username, url_Path);
        // not all servers have a username 
        uri.Append(escapedUsername);
        uri += '@';
    }

    nsXPIDLCString hostname;
    rv = GetHostname(getter_Copies(hostname));

    if (NS_SUCCEEDED(rv) && ((const char*)hostname) && hostname[0]) {
        nsXPIDLCString escapedHostname;
        *((char **)getter_Copies(escapedHostname)) =
            nsEscape(hostname, url_Path);
        // not all servers have a hostname
        uri.Append(escapedHostname);
    }

    *aResult = ToNewCString(uri);
    return NS_OK;
}
                          
nsresult
nsSmtpServer::getPrefString(const char *pref, nsCAutoString& result)
{
    result = "mail.smtpserver.";
    result += mKey;
    result += ".";
    result += pref;

    return NS_OK;
}
    
NS_IMETHODIMP
nsSmtpServer::SetRedirectorType(const char *aRedirectorType)
{
    nsresult rv;
    nsCAutoString pref;
    nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID, &rv));
    if (NS_FAILED(rv))
        return rv;
    getPrefString("redirector_type", pref);
    if (aRedirectorType)
        return prefs->SetCharPref(pref.get(), aRedirectorType);
    else
        prefs->ClearUserPref(pref.get());
    return NS_OK;
}

NS_IMETHODIMP
nsSmtpServer::GetRedirectorType(char **aResult)
{
    nsresult rv;
    nsCAutoString pref;
    nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID, &rv));
    if (NS_FAILED(rv))
        return rv;
    getPrefString("redirector_type", pref);
    rv = prefs->CopyCharPref(pref.get(), aResult);
    if (NS_FAILED(rv)) *aResult=nsnull;

    // Check if we need to change 'aol' to 'netscape' per #4696
    if (*aResult && !nsCRT::strcasecmp(*aResult, "aol"))
    {
      nsXPIDLCString hostName;
      rv = GetHostname(getter_Copies(hostName));
      if (NS_SUCCEEDED(rv) && (hostName.get()) && !nsCRT::strcmp(hostName, "smtp.netscape.net"))
      {
        PL_strfree(*aResult);
        pref = "netscape";
        rv = SetRedirectorType(pref.get());
        *aResult = ToNewCString(pref);
      }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsSmtpServer::ClearAllValues()
{
    nsresult rv = NS_OK;
    nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID, &rv));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCAutoString rootPref("mail.smtpserver.");
    rootPref += mKey;

    rv = prefs->EnumerateChildren(rootPref.get(), clearPrefEnum, (void *)prefs.get());

    return rv;
}

void
nsSmtpServer::clearPrefEnum(const char *aPref, void *aClosure)
{
    nsIPref *prefs = (nsIPref *)aClosure;
    prefs->ClearUserPref(aPref);
}
