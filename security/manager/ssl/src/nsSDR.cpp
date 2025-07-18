/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *  Terry Hayes <thayes@netscape.com>
 */

#include "stdlib.h"
#include "plstr.h"
#include "plbase64.h"

#include "nsMemory.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsISupports.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIServiceManager.h"
#include "nsISecurityManagerComponent.h"
#include "nsIWindowWatcher.h"
#include "nsIPrompt.h"
#include "nsProxiedService.h"
#include "nsINSSDialogs.h"

#include "nsISecretDecoderRing.h"
#include "nsSDR.h"
#include "nsNSSComponent.h"

#include "pk11func.h"
#include "pk11sdr.h" // For PK11SDR_Encrypt, PK11SDR_Decrypt

//
// Implementation of an nsIInterfaceRequestor for use
// as context for NSS calls
//
class nsSDRContext : public nsIInterfaceRequestor
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIINTERFACEREQUESTOR

  nsSDRContext();
  virtual ~nsSDRContext();

};

NS_IMPL_ISUPPORTS1(nsSDRContext, nsIInterfaceRequestor)

nsSDRContext::nsSDRContext()
{
  NS_INIT_ISUPPORTS();
}

nsSDRContext::~nsSDRContext()
{
}

/* void getInterface (in nsIIDRef uuid, [iid_is (uuid), retval] out nsQIResult result); */
NS_IMETHODIMP nsSDRContext::GetInterface(const nsIID & uuid, void * *result)
{
  if (uuid.Equals(NS_GET_IID(nsIPrompt))) {
    nsCOMPtr<nsIProxyObjectManager> proxyman(do_GetService(NS_XPCOMPROXY_CONTRACTID));
    if (!proxyman) return NS_ERROR_FAILURE;

    nsCOMPtr<nsIPrompt> prompter;
    nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService("@mozilla.org/embedcomp/window-watcher;1"));
    if (wwatch) {
      wwatch->GetNewPrompter(0, getter_AddRefs(prompter));
      if (prompter) {
        nsCOMPtr<nsIPrompt> proxyPrompt;
        proxyman->GetProxyForObject(NS_UI_THREAD_EVENTQ, NS_GET_IID(nsIPrompt),
                                    prompter, PROXY_SYNC, getter_AddRefs(proxyPrompt));
        if (!proxyPrompt) return NS_ERROR_FAILURE;
        *result = proxyPrompt;
        NS_ADDREF((nsIPrompt*)*result);
      }
    }
  } else {
    return NS_ERROR_NO_INTERFACE;
  }

  return NS_OK;
}

// Standard ISupports implementation
// NOTE: Should these be the thread-safe versions?
NS_IMPL_ISUPPORTS2(nsSecretDecoderRing, nsISecretDecoderRing, nsISecretDecoderRingConfig)

// nsSecretDecoderRing constructor
nsSecretDecoderRing::nsSecretDecoderRing()
{
  // initialize superclass
  NS_INIT_ISUPPORTS();

  // (Possibly) create the Security Manager component to get things
  // initialized
  nsCOMPtr<nsISecurityManagerComponent> nss = do_GetService(PSM_COMPONENT_CONTRACTID);
}

// nsSecretDecoderRing destructor
nsSecretDecoderRing::~nsSecretDecoderRing()
{
}

/* [noscript] long encrypt (in buffer data, in long dataLen, out buffer result); */
NS_IMETHODIMP nsSecretDecoderRing::
Encrypt(unsigned char * data, PRInt32 dataLen, unsigned char * *result, PRInt32 *_retval)
{
  nsresult rv = NS_OK;
  PK11SlotInfo *slot = 0;
  SECItem keyid;
  SECItem request;
  SECItem reply;
  SECStatus s;
  nsCOMPtr<nsIInterfaceRequestor> ctx = new nsSDRContext();

  slot = PK11_GetInternalKeySlot();
  if (!slot) { rv = NS_ERROR_NOT_AVAILABLE; goto loser; }

  /* Make sure token is initialized. */
  rv = setPassword(slot, ctx);
  if (NS_FAILED(rv))
    goto loser;

  s = PK11_Authenticate(slot, PR_TRUE, ctx);
  if (s != SECSuccess) { rv = NS_ERROR_FAILURE; goto loser; }

  /* Use default key id */
  keyid.data = 0;
  keyid.len = 0;
  request.data = data;
  request.len = dataLen;
  reply.data = 0;
  reply.len = 0;
  s= PK11SDR_Encrypt(&keyid, &request, &reply, ctx);
  if (s != SECSuccess) { rv = NS_ERROR_FAILURE; goto loser; }

  *result = reply.data;
  *_retval = reply.len;

loser:
  if (slot) PK11_FreeSlot(slot);
  return rv;
}

/* [noscript] long decrypt (in buffer data, in long dataLen, out buffer result); */
NS_IMETHODIMP nsSecretDecoderRing::
Decrypt(unsigned char * data, PRInt32 dataLen, unsigned char * *result, PRInt32 *_retval)
{
  nsresult rv = NS_OK;
  PK11SlotInfo *slot = 0;
  SECStatus s;
  SECItem request;
  SECItem reply;
  nsCOMPtr<nsIInterfaceRequestor> ctx = new nsSDRContext();

  *result = 0;
  *_retval = 0;

  /* Find token with SDR key */
  slot = PK11_GetInternalKeySlot();
  if (!slot) { rv = NS_ERROR_NOT_AVAILABLE; goto loser; }

  /* Force authentication */
  if (PK11_Authenticate(slot, PR_TRUE, ctx) != SECSuccess)
  {
    rv = NS_ERROR_NOT_AVAILABLE;
    goto loser;
  }

  request.data = data;
  request.len = dataLen;
  reply.data = 0;
  reply.len = 0;
  s = PK11SDR_Decrypt(&request, &reply, ctx);
  if (s != SECSuccess) { rv = NS_ERROR_FAILURE; goto loser; }

  *result = reply.data;
  *_retval = reply.len;

loser:
  if (slot) PK11_FreeSlot(slot);
  return rv;
}

/* string encryptString (in string text); */
NS_IMETHODIMP nsSecretDecoderRing::
EncryptString(const char *text, char **_retval)
{
    nsresult rv = NS_OK;
    unsigned char *encrypted = 0;
    PRInt32 eLen;

    if (text == nsnull || _retval == nsnull) {
        rv = NS_ERROR_INVALID_POINTER;
        goto loser;
    }

    rv = Encrypt((unsigned char *)text, PL_strlen(text), &encrypted, &eLen);
    if (rv != NS_OK) { goto loser; }

    rv = encode(encrypted, eLen, _retval);

loser:
    if (encrypted) nsMemory::Free(encrypted);

    return rv;
}

/* string decryptString (in string crypt); */
NS_IMETHODIMP nsSecretDecoderRing::
DecryptString(const char *crypt, char **_retval)
{
    nsresult rv = NS_OK;
    char *r = 0;
    unsigned char *decoded = 0;
    PRInt32 decodedLen;
    unsigned char *decrypted = 0;
    PRInt32 decryptedLen;

    if (crypt == nsnull || _retval == nsnull) {
      rv = NS_ERROR_INVALID_POINTER;
      goto loser;
    }

    rv = decode(crypt, &decoded, &decodedLen);
    if (rv != NS_OK) goto loser;

    rv = Decrypt(decoded, decodedLen, &decrypted, &decryptedLen);
    if (rv != NS_OK) goto loser;

    // Convert to NUL-terminated string
    r = (char *)nsMemory::Alloc(decryptedLen+1);
    if (!r) { rv = NS_ERROR_OUT_OF_MEMORY; goto loser; }

    memcpy(r, decrypted, decryptedLen);
    r[decryptedLen] = 0;

    *_retval = r;
    r = 0;

loser:
    if (r) nsMemory::Free(r);
    if (decrypted) nsMemory::Free(decrypted);
    if (decoded) nsMemory::Free(decoded);
 
    return rv;
}

/* void changePassword(); */
NS_IMETHODIMP nsSecretDecoderRing::
ChangePassword()
{
  nsresult rv;
  PK11SlotInfo *slot;

  slot = PK11_GetInternalKeySlot();
  if (!slot) return NS_ERROR_NOT_AVAILABLE;

  /* Convert UTF8 token name to UCS2 */
  NS_ConvertUTF8toUCS2 tokenName(PK11_GetTokenName(slot));

  PK11_FreeSlot(slot);

  /* Get the set password dialog handler imlementation */
  nsCOMPtr<nsITokenPasswordDialogs> dialogs;

  rv = getNSSDialogs(getter_AddRefs(dialogs),
           NS_GET_IID(nsITokenPasswordDialogs));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIInterfaceRequestor> ctx = new nsSDRContext();
  PRBool canceled;

  rv = dialogs->SetPassword(ctx, tokenName.get(), &canceled);

  /* canceled is ignored */


  return rv;
}

/* void logout(); */
NS_IMETHODIMP nsSecretDecoderRing::
Logout()
{
  PK11_LogoutAll();
  return NS_OK;
}

/* void setWindow(in nsISupports w); */
nsresult nsSecretDecoderRing::
SetWindow(nsISupports *w)
{
  return NS_OK;
}

// Support routines

nsresult nsSecretDecoderRing::
encode(const unsigned char *data, PRInt32 dataLen, char **_retval)
{
    nsresult rv = NS_OK;

    *_retval = PL_Base64Encode((const char *)data, dataLen, NULL);
    if (!*_retval) { rv = NS_ERROR_OUT_OF_MEMORY; goto loser; }

loser:
    return rv;
}

nsresult nsSecretDecoderRing::
decode(const char *data, unsigned char **result, PRInt32 * _retval)
{
    nsresult rv = NS_OK;
    PRUint32 len = PL_strlen(data);
    int adjust = 0;

    /* Compute length adjustment */
    if (data[len-1] == '=') {
      adjust++;
      if (data[len-2] == '=') adjust++;
    }

    *result = (unsigned char *)PL_Base64Decode(data, len, NULL);
    if (!*result) { rv = NS_ERROR_ILLEGAL_VALUE; goto loser; }

    *_retval = (len*3)/4 - adjust;

loser:
    return rv;
}
