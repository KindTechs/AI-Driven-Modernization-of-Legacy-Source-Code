/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
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
 *  Javier Delgadillo <javi@netscape.com>
 */
#include "nsNSSComponent.h"
#include "nsCrypto.h"
#include "nsKeygenHandler.h"
#include "nsKeygenThread.h"
#include "nsINSSDialogs.h"
#include "nsNSSCertificate.h"
#include "nsPKCS12Blob.h"
#include "nsPK11TokenDB.h"
#include "nsIServiceManager.h"
#include "nsINSSDialogs.h"
#include "nsIMemory.h"
#include "nsCRT.h"
#include "prprf.h"
#include "prmem.h"
#include "nsDOMCID.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDOMClassInfo.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIScriptContext.h"
#include "nsIScriptGlobalObject.h"
#include "nsIXPConnect.h"
#include "nsIEventQueueService.h"
#include "nsIEventQueue.h"
#include "nsIRunnable.h"
#include "nsIWindowWatcher.h"
#include "nsIPrompt.h"
#include "nsIFilePicker.h"
#include "nsJSPrincipals.h"
#include "nsScriptSecurityManager.h"
#include "nsIPrincipal.h"
#include "nsXPIDLString.h"
#include "jsapi.h"
#include "jsdbgapi.h"
#include "jscntxt.h"
#include <ctype.h>
#include "nsReadableUtils.h"
#include "pk11func.h"
#include "keyhi.h"
#include "cryptohi.h"
#include "seccomon.h"
extern "C" {
#include "crmf.h"
#include "crmfi.h"
#ifdef NSS_3_4
#include "pk11pqg.h"
#endif
}
#include "cmmf.h"
#include "nssb64.h"
#include "base64.h"
#include "certdb.h"
#include "secmod.h"

/*
 * These are the most common error strings that are returned
 * by the JavaScript methods in case of error.
 */

#define JS_ERROR       "error:"
#define JS_ERROR_INVAL_PARAM JS_ERROR"invalidParameter:"
#define JS_ERROR_USER_CANCEL JS_ERROR"userCancel"
#define JS_ERROR_INTERNAL  JS_ERROR"internalError"
#define JS_ERROR_ARGC_ERR  JS_ERROR"incorrect number of parameters"

#undef REPORT_INCORRECT_NUM_ARGS

#define JS_OK_ADD_MOD                      3
#define JS_OK_DEL_EXTERNAL_MOD             2
#define JS_OK_DEL_INTERNAL_MOD             1

#define JS_ERR_INTERNAL                   -1
#define JS_ERR_USER_CANCEL_ACTION         -2
#define JS_ERR_INCORRECT_NUM_OF_ARGUMENTS -3
#define JS_ERR_DEL_MOD                    -4
#define JS_ERR_ADD_MOD                    -5
#define JS_ERR_BAD_MODULE_NAME            -6
#define JS_ERR_BAD_DLL_NAME               -7
#define JS_ERR_BAD_MECHANISM_FLAGS        -8
#define JS_ERR_BAD_CIPHER_ENABLE_FLAGS    -9
#define JS_ERR_ADD_DUPLICATE_MOD          -10

/*
 * This structure is used to store information for one key generation.
 * The nsCrypto::GenerateCRMFRequest method parses the inputs and then
 * stores one of these structures for every key generation that happens.
 * The information stored in this structure is then used to set some
 * values in the CRMF request.
 */
typedef enum {
  rsaEnc, rsaDualUse, rsaSign, rsaNonrepudiation, rsaSignNonrepudiation,
  dhEx, dsaSignNonrepudiation, dsaSign, dsaNonrepudiation, invalidKeyGen
} nsKeyGenType;

typedef struct nsKeyPairInfoStr {
  SECKEYPublicKey  *pubKey;     /* The putlic key associated with gen'd 
                                   priv key. */
  SECKEYPrivateKey *privKey;    /* The private key we generated */ 
  nsKeyGenType      keyGenType; /* What type of key gen are we doing.*/
} nsKeyPairInfo;

//
// This is the class we'll use to run the keygen done code
// as an nsIRunnable object;
//
struct CryptoRunnableEvent : PLEvent {
  CryptoRunnableEvent(nsIRunnable* runnable);
  ~CryptoRunnableEvent();

   nsIRunnable* mRunnable;
};


//This class is just used to pass arguments
//to the nsCryptoRunnable event.
class nsCryptoRunArgs : public nsISupports {
public:
  nsCryptoRunArgs();
  virtual ~nsCryptoRunArgs();
  JSContext *m_cx;
  JSObject  *m_scope;
  nsCOMPtr<nsIPrincipal> m_principals;
  nsXPIDLCString m_jsCallback;
  NS_DECL_ISUPPORTS
};

//This class is used to run the callback code
//passed to crypto.generateCRMFRequest
//We have to do that for backwards compatibility
//reasons w/ PSM 1.x and Communciator 4.x
class nsCryptoRunnable : public nsIRunnable {
public:
  nsCryptoRunnable(nsCryptoRunArgs *args);
  virtual ~nsCryptoRunnable();

  NS_IMETHOD Run ();
  NS_DECL_ISUPPORTS
private:
  nsCryptoRunArgs *m_args;
};


//We're going to inherit the memory passed
//into us.
//This class backs up an array of certificates
//as an event.
class nsP12Runnable : public nsIRunnable {
public:
  nsP12Runnable(nsIX509Cert **certArr, PRInt32 numCerts, nsIPK11Token *token);
  virtual ~nsP12Runnable();

  NS_IMETHOD Run();
  NS_DECL_ISUPPORTS
private:
  nsCOMPtr<nsIPK11Token> mToken;
  nsIX509Cert **mCertArr;
  PRInt32       mNumCerts;
};

// QueryInterface implementation for nsCrypto
NS_INTERFACE_MAP_BEGIN(nsCrypto)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCrypto)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY_DOM_CLASSINFO(Crypto)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsCrypto)
NS_IMPL_RELEASE(nsCrypto)

// QueryInterface implementation for nsCRMFObject
NS_INTERFACE_MAP_BEGIN(nsCRMFObject)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCRMFObject)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY_DOM_CLASSINFO(CRMFObject)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsCRMFObject)
NS_IMPL_RELEASE(nsCRMFObject)

// QueryInterface implementation for nsPkcs11
NS_INTERFACE_MAP_BEGIN(nsPkcs11)
  NS_INTERFACE_MAP_ENTRY(nsIDOMPkcs11)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY_DOM_CLASSINFO(Pkcs11)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsPkcs11)
NS_IMPL_RELEASE(nsPkcs11)

// QueryInterface implementation for nsCryptoRunnable
NS_INTERFACE_MAP_BEGIN(nsCryptoRunnable)
  NS_INTERFACE_MAP_ENTRY(nsIRunnable)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsCryptoRunnable)
NS_IMPL_RELEASE(nsCryptoRunnable)

// QueryInterface implementation for nsP12Runnable
NS_INTERFACE_MAP_BEGIN(nsP12Runnable)
  NS_INTERFACE_MAP_ENTRY(nsIRunnable)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END_THREADSAFE

NS_IMPL_THREADSAFE_ADDREF(nsP12Runnable)
NS_IMPL_THREADSAFE_RELEASE(nsP12Runnable)

NS_INTERFACE_MAP_BEGIN(nsCryptoRunArgs)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END_THREADSAFE

NS_IMPL_THREADSAFE_ADDREF(nsCryptoRunArgs)
NS_IMPL_THREADSAFE_RELEASE(nsCryptoRunArgs)

#ifndef NSS_3_4
/*
 * We're cheating for now so that escrowing keys on smart cards 
 * will work.  The NSS team gave us their blessing to do this
 * until they export a public function with equivalent functionality.
 */
extern "C" SECKEYPrivateKey*
pk11_loadPrivKey(PK11SlotInfo *slot,SECKEYPrivateKey *privKey,
                SECKEYPublicKey *pubKey, PRBool token, PRBool sensitive);
#define __FUNCTIONNAME_PK11_LoadPrivKey         pk11_loadPrivKey
#else
#define __FUNCTIONNAME_PK11_LoadPrivKey         PK11_LoadPrivKey
#endif

static NS_DEFINE_CID(kNSSComponentCID, NS_NSSCOMPONENT_CID);

nsCrypto::nsCrypto()
{
  NS_INIT_REFCNT();
}

nsCrypto::~nsCrypto()
{
}

//Grab the UI event queue so that we can post some events to it.
nsIEventQueue* 
nsCrypto::GetUIEventQueue()
{
  nsresult rv;
  nsCOMPtr<nsIEventQueueService> service = 
                        do_GetService(NS_EVENTQUEUESERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) 
    return nsnull;
  
  nsIEventQueue* result = nsnull;
  rv = service->GetThreadEventQueue(NS_UI_THREAD, &result);
  if (NS_FAILED(rv)) 
    return nsnull;
  
  return result;
}

//These next few functions are based on implementation in
//the script security manager to get the principals from
//a JSContext.  We need that to successfully run the 
//callback paramter passed to crypto.generateCRMFRequest
static nsresult
cryptojs_GetScriptPrincipal(JSContext *cx, JSScript *script,
                            nsIPrincipal **result)
{
  if (!script) {
    *result = nsnull;
    return NS_OK;
  }
  JSPrincipals *jsp = JS_GetScriptPrincipals(cx, script);
  if (!jsp) {
    return NS_ERROR_FAILURE;
  }
  nsJSPrincipals *nsJSPrin = NS_STATIC_CAST(nsJSPrincipals *, jsp);
  *result = nsJSPrin->nsIPrincipalPtr;
  if (!result) {
    return NS_ERROR_FAILURE;
  }
  NS_ADDREF(*result);
  return NS_OK;
}

static nsresult
cryptojs_GetObjectPrincipal(JSContext *aCx, JSObject *aObj,
                            nsIPrincipal **result)
{
  JSObject *parent = aObj;
  do
  {
    JSClass *jsClass = JS_GetClass(aCx, parent);
    const uint32 privateNsISupports = JSCLASS_HAS_PRIVATE | 
                                      JSCLASS_PRIVATE_IS_NSISUPPORTS;
    if (jsClass && (jsClass->flags & (privateNsISupports)) == 
                    privateNsISupports)
    {
      nsCOMPtr<nsISupports> supports = (nsISupports *) JS_GetPrivate(aCx, parent);
      nsCOMPtr<nsIScriptObjectPrincipal> objPrin = do_QueryInterface(supports);
              
      if (!objPrin)
      {
        /*
         * If it's a wrapped native, check the underlying native
         * instead.
         */
        nsCOMPtr<nsIXPConnectWrappedNative> xpcNative = 
                                            do_QueryInterface(supports);

        if (xpcNative)
          xpcNative->GetNative(getter_AddRefs(supports));
          objPrin = do_QueryInterface(supports);
        }

        if (objPrin && NS_SUCCEEDED(objPrin->GetPrincipal(result)))
          return NS_OK;
        }
        parent = JS_GetParent(aCx, parent);
    } while (parent);

    // Couldn't find a principal for this object.
    return NS_ERROR_FAILURE;
}

static nsresult
cryptojs_GetFunctionObjectPrincipal(JSContext *cx, JSObject *obj,
                                    nsIPrincipal **result)
{
  JSFunction *fun = (JSFunction *) JS_GetPrivate(cx, obj);

  JSScript *script = JS_GetFunctionScript(cx, fun);
  if (script && JS_GetFunctionObject(fun) != obj)
  {
    // Scripted function has been cloned; get principals from obj's
    // parent-linked scope chain.  We do not get object principals for a
    // cloned *native* function, because the subject in that case is a
    // script or function further down the stack who is calling us.
    return cryptojs_GetObjectPrincipal(cx, obj, result);
  }
  return cryptojs_GetScriptPrincipal(cx, script, result);
}

static nsresult
cryptojs_GetFramePrincipal(JSContext *cx, JSStackFrame *fp,
                           nsIPrincipal **principal)
{
  JSObject *obj = JS_GetFrameFunctionObject(cx, fp);
  if (!obj) {
    JSScript *script = JS_GetFrameScript(cx, fp);
    return cryptojs_GetScriptPrincipal(cx, script, principal);
  }
  return cryptojs_GetFunctionObjectPrincipal(cx, obj, principal);
}

nsIPrincipal*
nsCrypto::GetScriptPrincipal(JSContext *cx)
{
  JSStackFrame *fp = nsnull;
  nsIPrincipal *principal=nsnull;

  for (fp = JS_FrameIterator(cx, &fp); fp; fp = JS_FrameIterator(cx, &fp)) {
    cryptojs_GetFramePrincipal(cx, fp, &principal);
    if (principal != nsnull) {
      break;
    }
  }

  if (!principal) {
    nsCOMPtr<nsIScriptContext> scriptContext = 
             NS_REINTERPRET_CAST(nsIScriptContext*,JS_GetContextPrivate(cx));
    if (scriptContext)
    {
      nsCOMPtr<nsIScriptGlobalObject> global;
      scriptContext->GetGlobalObject(getter_AddRefs(global));
      NS_ENSURE_TRUE(global, nsnull);
      nsCOMPtr<nsIScriptObjectPrincipal> globalData = do_QueryInterface(global);
      NS_ENSURE_TRUE(globalData, nsnull);
      globalData->GetPrincipal(&principal);
    }
  }
  return principal;
}

//A quick function to let us know if the key we're trying to generate
//can be escrowed.
static PRBool
ns_can_escrow(nsKeyGenType keyGenType)
{
  /* For now, we only escrow rsa-encryption keys. */
  return (PRBool)(keyGenType == rsaEnc);
}

//Retrieve crypto.version so that callers know what
//version of PSM this is.
NS_IMETHODIMP
nsCrypto::GetVersion(nsAString& aVersion)
{
  aVersion.Assign(NS_LITERAL_STRING(PSM_VERSION_STRING).get());
  return NS_OK;
}

/*
 * Given an nsKeyGenType, return the PKCS11 mechanism that will
 * perform the correct key generation.
 */
static PRUint32
cryptojs_convert_to_mechanism(nsKeyGenType keyGenType)
{
  PRUint32 retMech;

  switch (keyGenType) {
  case rsaEnc:
  case rsaDualUse:
  case rsaSign:
  case rsaNonrepudiation:
  case rsaSignNonrepudiation:
    retMech = CKM_RSA_PKCS_KEY_PAIR_GEN;
    break;
  case dhEx:
    retMech = CKM_DH_PKCS_KEY_PAIR_GEN;
    break;
  case dsaSign:
  case dsaSignNonrepudiation:
  case dsaNonrepudiation:
    retMech = CKM_DSA_KEY_PAIR_GEN;
    break;
  default:
    retMech = CKM_INVALID_MECHANISM;
  }
  return retMech;
}

/*
 * This function converts a string read through JavaScript parameters
 * and translates it to the internal enumeration representing the
 * key gen type.
 */
static nsKeyGenType
cryptojs_interpret_key_gen_type(char *keyAlg)
{
  char *end;
  if (keyAlg == nsnull) {
    return invalidKeyGen;
  }
  /* First let's remove all leading and trailing white space */
  while (isspace(keyAlg[0])) keyAlg++;
  end = strchr(keyAlg, '\0');
  if (end == nsnull) {
    return invalidKeyGen;
  }
  end--;
  while (isspace(*end)) end--;
  end[1] = '\0';
  if (strcmp(keyAlg, "rsa-ex") == 0) {
    return rsaEnc;
  } else if (strcmp(keyAlg, "rsa-dual-use") == 0) {
    return rsaDualUse;
  } else if (strcmp(keyAlg, "rsa-sign") == 0) {
    return rsaSign;
  } else if (strcmp(keyAlg, "rsa-sign-nonrepudiation") == 0) {
    return rsaSignNonrepudiation;
  } else if (strcmp(keyAlg, "rsa-nonrepudiation") == 0) {
    return rsaNonrepudiation;
  } else if (strcmp(keyAlg, "dsa-sign-nonrepudiation") == 0) {
    return dsaSignNonrepudiation;
  } else if (strcmp(keyAlg, "dsa-sign") ==0 ){
    return dsaSign;
  } else if (strcmp(keyAlg, "dsa-nonrepudiation") == 0) {
    return dsaNonrepudiation;
  } else if (strcmp(keyAlg, "dh-ex") == 0) {
    return dhEx;
  }
  return invalidKeyGen;
}

#ifdef NSS_3_4
#define __FUNCTIONNAME_PK11_PQG_ParamGen        PK11_PQG_ParamGen
#define __FUNCTIONNAME_PK11_PQG_DestroyVerify   PK11_PQG_DestroyVerify
#define __FUNCTIONNAME_PK11_PQG_DestroyParams   PK11_PQG_DestroyParams
#define __WRAPPER_SEC_ASN1EncodeItem_Param4(p)  SEC_ASN1_GET(p)
#else
#define __FUNCTIONNAME_PK11_PQG_ParamGen        PQG_ParamGen
#define __FUNCTIONNAME_PK11_PQG_DestroyVerify   PQG_DestroyVerify
#define __FUNCTIONNAME_PK11_PQG_DestroyParams   PQG_DestroyParams
#define __WRAPPER_SEC_ASN1EncodeItem_Param4(p)  p
#endif

//Take the string passed into us via crypto.generateCRMFRequest
//as the keygen type parameter and convert it to parameters 
//we can actually pass to the PKCS#11 layer.
static void*
nsConvertToActualKeyGenParams(PRUint32 keyGenMech, char *params,
                              PRUint32 paramLen, PRInt32 keySize)
{
  void *returnParams = nsnull;

  // We don't support passing in key generation arguments from
  // the JS code just yet.
  if (!params) {
    /* In this case we provide the parameters ourselves. */
    switch (keyGenMech) {
    case CKM_RSA_PKCS_KEY_PAIR_GEN:
    {
       PK11RSAGenParams *rsaParams;
       rsaParams = NS_STATIC_CAST(PK11RSAGenParams*,
                                  nsMemory::Alloc(sizeof(PK11RSAGenParams)));
                                 
       if (rsaParams == nsnull) {
         return nsnull;
       }
       /* I'm just taking the same parameters used in 
        * certdlgs.c:GenKey
        */
       if (keySize > 0) {
         rsaParams->keySizeInBits = keySize;
       } else {
         rsaParams->keySizeInBits = 1024;
       }
       rsaParams->pe = DEFAULT_RSA_KEYGEN_PE;
       returnParams = rsaParams;
       break;
    }
    case CKM_DSA_KEY_PAIR_GEN:
    {
       PQGParams *pqgParams = nsnull;
       PQGVerify *vfy = nsnull;
       SECStatus  rv;
       int        index;
          
       index = PQG_PBITS_TO_INDEX(keySize);
       if (index == -1) {
         returnParams = nsnull;
         break;
       }
       rv = __FUNCTIONNAME_PK11_PQG_ParamGen(0, &pqgParams, &vfy);
       if (vfy) {
         __FUNCTIONNAME_PK11_PQG_DestroyVerify(vfy);
       }
       if (rv != SECSuccess) {
         if (pqgParams) {
           __FUNCTIONNAME_PK11_PQG_DestroyParams(pqgParams);
         }
         return nsnull;
       }
       returnParams = pqgParams;
       break;
     }
     default:
       returnParams = nsnull;
     }
  }
  return returnParams;
}

//We need to choose which PKCS11 slot we're going to generate
//the key on.  Calls the default implementation provided by
//nsKeygenHandler.cpp
static PK11SlotInfo*
nsGetSlotForKeyGen(nsKeyGenType keyGenType, nsIInterfaceRequestor *ctx)
{
  PRUint32 mechanism = cryptojs_convert_to_mechanism(keyGenType);
  PK11SlotInfo *slot = nsnull;
  nsresult rv = GetSlotWithMechanism(mechanism,ctx, &slot);
  if (NS_FAILED(rv)) {
    if (slot)
      PK11_FreeSlot(slot);
    slot = nsnull;
  }
  return slot;
}

//Free the parameters that were passed into PK11_GenerateKeyPair
//depending on the mechanism type used.
static void
nsFreeKeyGenParams(CK_MECHANISM_TYPE keyGenMechanism, void *params)
{
  switch (keyGenMechanism) {
  case CKM_RSA_PKCS_KEY_PAIR_GEN:
    nsMemory::Free(params);
    break;
  case CKM_DSA_KEY_PAIR_GEN:
    __FUNCTIONNAME_PK11_PQG_DestroyParams(NS_STATIC_CAST(PQGParams*,params));
    break;
  }
}

//Function that is used to generate a single key pair.
//Once all the arguments have been parsed and processed, this
//function gets called and takes care of actually generating
//the key pair passing the appopriate parameters to the NSS
//functions.
static nsresult
cryptojs_generateOneKeyPair(JSContext *cx, nsKeyPairInfo *keyPairInfo, 
                            PRInt32 keySize, char *params, 
                            nsIInterfaceRequestor *uiCxt,
                            PK11SlotInfo *slot, PRBool willEscrow)
                            
{
  nsIGeneratingKeypairInfoDialogs * dialogs;
  nsKeygenThread *KeygenRunnable = 0;
  nsCOMPtr<nsIKeygenThread> runnable;

  PRUint32 mechanism = cryptojs_convert_to_mechanism(keyPairInfo->keyGenType);
  void *keyGenParams = nsConvertToActualKeyGenParams(mechanism, params, 
                                                     (params) ? nsCRT::strlen(params):0, 
                                                     keySize);

  // Make sure the token has password already set on it before trying
  // to generate the key.

  nsresult rv = setPassword(slot, uiCxt);
  if (NS_FAILED(rv))
    return rv;

  if (PK11_Authenticate(slot, PR_TRUE, uiCxt) != SECSuccess)
    return NS_ERROR_FAILURE;
 

  // Smart cards will not let you extract a private key once 
  // it is on the smart card.  If we've been told to escrow
  // a private key that will ultimately wind up on a smart card,
  // then we'll generate the private key on the internal slot
  // as a temporary key, then move it to the destination slot. 
  // NOTE: We call PK11_GetInternalSlot instead of PK11_GetInternalKeySlot
  //       so that the key has zero chance of being store in the
  //       user's key3.db file.  Which the slot returned by
  //       PK11_GetInternalKeySlot has access to and PK11_GetInternalSlot
  //       does not.
  PK11SlotInfo *intSlot = nsnull, *origSlot = nsnull;
  PRBool isPerm;

  if (willEscrow && !PK11_IsInternal(slot)) {
    intSlot = PK11_GetInternalSlot();
    NS_ASSERTION(intSlot,"Couldn't get the internal slot");
    isPerm = PR_FALSE;
    origSlot = slot;
    slot = intSlot;
  } else {
    isPerm = PR_TRUE;
  }

  rv = getNSSDialogs((void**)&dialogs,
                     NS_GET_IID(nsIGeneratingKeypairInfoDialogs));

  if (NS_SUCCEEDED(rv)) {
    KeygenRunnable = new nsKeygenThread();
    if (KeygenRunnable) {
      NS_ADDREF(KeygenRunnable);
    }
  }

  if (NS_FAILED(rv) || !KeygenRunnable) {
    rv = NS_OK;
    keyPairInfo->privKey = PK11_GenerateKeyPair(slot, mechanism, keyGenParams,
                                                &keyPairInfo->pubKey, isPerm, 
                                                isPerm, uiCxt);
  } else {
    KeygenRunnable->SetParams( slot, mechanism, keyGenParams, isPerm, isPerm, uiCxt );

    runnable = do_QueryInterface(KeygenRunnable);

    if (runnable) {
      rv = dialogs->DisplayGeneratingKeypairInfo(uiCxt, runnable);

      // We call join on the thread, 
      // so we can be sure that no simultaneous access to the passed parameters will happen.
      KeygenRunnable->Join();

      NS_RELEASE(dialogs);
      if (NS_SUCCEEDED(rv)) {
        rv = KeygenRunnable->GetParams(&keyPairInfo->privKey, &keyPairInfo->pubKey);
      }
    }
  }

  nsFreeKeyGenParams(mechanism, keyGenParams);

  if (KeygenRunnable) {
    NS_RELEASE(KeygenRunnable);
  }

  if (!keyPairInfo->privKey || !keyPairInfo->pubKey) {
    if (intSlot)
      PK11_FreeSlot(intSlot);

    return NS_ERROR_FAILURE;
  }
 

  //If we generated the key pair on the internal slot because the
  // keys were going to be escrowed, move the keys over right now.
  if (willEscrow && intSlot) {
    SECKEYPrivateKey *newPrivKey = __FUNCTIONNAME_PK11_LoadPrivKey(origSlot, 
                                                    keyPairInfo->privKey,
                                                    keyPairInfo->pubKey,
                                                    PR_TRUE, PR_TRUE);
    if (!newPrivKey)
      return NS_ERROR_FAILURE;

    // The private key is stored on the selected slot now, and the copy we
    // ultimately use for escrowing when the time comes lives 
    // in the internal slot.  We will delete it from that slot
    // after the requests are made.  This call only gives up
    // our reference to the key object and does not actually 
    // physically remove it from the card itself.
    SECKEY_DestroyPrivateKey(newPrivKey);
    PK11_FreeSlot(intSlot);
    intSlot = nsnull;
  }  

  return NS_OK;
}

/*
 * FUNCTION: cryptojs_ReadArgsAndGenerateKey
 * -------------------------------------
 * INPUTS:
 *  cx
 *    The JSContext associated with the execution of the corresponging
 *    crypto.generateCRMFRequest call
 *  argv
 *    A pointer to an array of JavaScript parameters passed to the
 *    method crypto.generateCRMFRequest.  The array should have the
 *    3 arguments keySize, "keyParams", and "keyGenAlg" mentioned in
 *    the definition of crypto.generateCRMFRequest at the following
 *    document http://docs.iplanet.com/docs/manuals/psm/11/cmcjavascriptapi.html 
 *  keyGenType
 *    A structure used to store the information about the newly created
 *    key pair.
 *  uiCxt
 *    An interface requestor that would be used to get an nsIPrompt
 *    if we need to ask the user for a password.
 *  slotToUse
 *    The PKCS11 slot to use for generating the key pair. If nsnull, then
 *    this function should select a slot that can do the key generation 
 *    from the keytype associted with the keyPairInfo, and pass it back to
 *    the caller so that subsequence key generations can use the same slot. 
 *  willEscrow
 *    If PR_TRUE, then that means we will try to escrow the generated
 *    private key when building the CRMF request.  If PR_FALSE, then
 *    we will not try to escrow the private key.
 *
 * NOTES:
 * This function takes care of reading a set of 3 parameters that define
 * one key generation.  The argv pointer should be one that originates
 * from the argv parameter passed in to the method nsCrypto::GenerateCRMFRequest.
 * The function interprets the argument in the first index as an integer and
 * passes that as the key size for the key generation-this parameter is
 * mandatory.  The second parameter is read in as a string.  This value can
 * be null in JavaScript world and everything will still work.  The third
 * parameter is a mandatory string that indicates what kind of key to generate.
 * There should always be 1-to-1 correspondence between the strings compared
 * in the function cryptojs_interpret_key_gen_type and the strings listed in
 * document at http://docs.iplanet.com/docs/manuals/psm/11/cmcjavascriptapi.html 
 * under the definition of the method generateCRMFRequest, for the parameter
 * "keyGenAlgN".  After reading the parameters, the function then 
 * generates the key pairs passing the parameters parsed from the JavaScript i
 * routine.  
 *
 * RETURN:
 * NS_OK if creating the Key was successful.  Any other return value
 * indicates an error.
 */

static nsresult
cryptojs_ReadArgsAndGenerateKey(JSContext *cx,
                                jsval *argv,
                                nsKeyPairInfo *keyGenType,
                                nsIInterfaceRequestor *uiCxt,
                                PK11SlotInfo **slot, PRBool willEscrow)
{
  JSString  *jsString;
  char    *params, *keyGenAlg; //Never free these strings cause
                               //they are owned by the JS layer.
  int    keySize;
  nsresult  rv;

  if (!JSVAL_IS_INT(argv[0])) {
    JS_ReportError(cx, "%s%s\n", JS_ERROR,
                   "passed in non-integer for key size");
    return NS_ERROR_FAILURE;
  }
  keySize = JSVAL_TO_INT(argv[0]);
  if (JSVAL_IS_NULL(argv[1])) {
    params = nsnull;
  } else {
    jsString = JS_ValueToString(cx,argv[1]);
    params   = JS_GetStringBytes(jsString);
  }

  if (JSVAL_IS_NULL(argv[2])) {
    JS_ReportError(cx,"%s%s\n", JS_ERROR,
             "key generation type not specified");
    return NS_ERROR_FAILURE;
  }
  jsString = JS_ValueToString(cx, argv[2]);
  keyGenAlg = JS_GetStringBytes(jsString);
  keyGenType->keyGenType = cryptojs_interpret_key_gen_type(keyGenAlg);
  if (keyGenType->keyGenType == invalidKeyGen) {
    JS_ReportError(cx, "%s%s%s", JS_ERROR,
                   "invalid key generation argument:",
                   keyGenAlg);
    goto loser;
  }
  if (*slot == nsnull) {
    *slot = nsGetSlotForKeyGen(keyGenType->keyGenType, uiCxt);
    if (*slot == nsnull)
      goto loser;
  }

  rv = cryptojs_generateOneKeyPair(cx,keyGenType,keySize,params,uiCxt,*slot,
                                   willEscrow);

  if (rv != NS_OK) {
    JS_ReportError(cx,"%s%s%s", JS_ERROR,
                   "could not generate the key for algorithm ",
                   keyGenAlg);
    goto loser;
  }
  return NS_OK;
loser:
  return NS_ERROR_FAILURE;
}

//Utility funciton to free up the memory used by nsKeyPairInfo
//arrays.
static void
nsFreeKeyPairInfo(nsKeyPairInfo *keyids, int numIDs)
{
  NS_ASSERTION(keyids, "NULL pointer passed to nsFreeKeyPairInfo");
  if (!keyids)
    return;
  int i;
  for (i=0; i<numIDs; i++) {
    if (keyids[i].pubKey)
      SECKEY_DestroyPublicKey(keyids[i].pubKey);
    if (keyids[i].privKey)
      SECKEY_DestroyPrivateKey(keyids[i].privKey);
  }
  delete []keyids;
}

//Utility funciton used to free the genertaed cert request messages
static void
nsFreeCertReqMessages(CRMFCertReqMsg **certReqMsgs, PRInt32 numMessages)
{
  PRInt32 i;
  for (i=0; i<numMessages && certReqMsgs[i]; i++) {
    CRMF_DestroyCertReqMsg(certReqMsgs[i]);
  }
  delete []certReqMsgs;
}

//If the form called for escrowing the private key we just generated,
//this function adds all the correct elements to the request.
//That consists of adding CRMFEncryptedKey to the reques as part
//of the CRMFPKIArchiveOptions Control.
static nsresult
nsSetEscrowAuthority(CRMFCertRequest *certReq, nsKeyPairInfo *keyInfo,
                     nsNSSCertificate *wrappingCert)
{
  if (!wrappingCert ||
      CRMF_CertRequestIsControlPresent(certReq, crmfPKIArchiveOptionsControl)){
    return NS_ERROR_FAILURE;
  }
  CERTCertificate *cert = wrappingCert->GetCert();
  if (!cert)
    return NS_ERROR_FAILURE;

  CRMFEncryptedKey *encrKey = 
      CRMF_CreateEncryptedKeyWithEncryptedValue(keyInfo->privKey, cert);
  CERT_DestroyCertificate(cert);
  if (!encrKey)
    return NS_ERROR_FAILURE;

  CRMFPKIArchiveOptions *archOpt = 
      CRMF_CreatePKIArchiveOptions(crmfEncryptedPrivateKey, 
                                   NS_STATIC_CAST(void*,encrKey)); 
  if (!archOpt) {
    CRMF_DestroyEncryptedKey(encrKey);
    return NS_ERROR_FAILURE;
  }
  SECStatus srv = CRMF_CertRequestSetPKIArchiveOptions(certReq, archOpt);
  CRMF_DestroyEncryptedKey(encrKey);
  CRMF_DestroyPKIArchiveOptions(archOpt);
  if (srv != SECSuccess)
    return NS_ERROR_FAILURE;

  return NS_OK;
}

//Set the Distinguished Name (Subject Name) for the cert
//being requested.
static nsresult
nsSetDNForRequest(CRMFCertRequest *certReq, char *reqDN)
{
  if (!reqDN || CRMF_CertRequestIsFieldPresent(certReq, crmfSubject)) {
    return NS_ERROR_FAILURE;
  }
  CERTName *subjectName = CERT_AsciiToName(reqDN);
  if (!subjectName) {
    return NS_ERROR_FAILURE;
  }
  SECStatus srv = CRMF_CertRequestSetTemplateField(certReq, crmfSubject,
                                                   NS_STATIC_CAST(void*,
                                                                  subjectName));
  CERT_DestroyName(subjectName);
  return (srv == SECSuccess) ? NS_OK : NS_ERROR_FAILURE;
}

//Set Registration Token Control on the request.
static nsresult
nsSetRegToken(CRMFCertRequest *certReq, char *regToken)
{
  // this should never happen, but might as well add this.
  NS_ASSERTION(certReq, "A bogus certReq passed to nsSetRegToken");
  if (regToken){
    if (CRMF_CertRequestIsControlPresent(certReq, crmfRegTokenControl))
      return NS_ERROR_FAILURE;
  
    SECItem src;
    src.data = (unsigned char*)regToken;
    src.len  = nsCRT::strlen(regToken);
    SECItem *derEncoded = SEC_ASN1EncodeItem(nsnull, nsnull, &src, 
                                        __WRAPPER_SEC_ASN1EncodeItem_Param4(SEC_UTF8StringTemplate));

    if (!derEncoded)
      return NS_ERROR_FAILURE;

    SECStatus srv = CRMF_CertRequestSetRegTokenControl(certReq, derEncoded);
    SECITEM_FreeItem(derEncoded,PR_TRUE);
    if (srv != SECSuccess)
      return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

//Set the Authenticator control on the cert reuest.  It's just
//a string that gets passed along.
static nsresult
nsSetAuthenticator(CRMFCertRequest *certReq, char *authenticator)
{
  //This should never happen, but might as well check.
  NS_ASSERTION(certReq, "Bogus certReq passed to nsSetAuthenticator");
  if (authenticator) {
    if (CRMF_CertRequestIsControlPresent(certReq, crmfAuthenticatorControl))
      return NS_ERROR_FAILURE;
    
    SECItem src;
    src.data = (unsigned char*)authenticator;
    src.len  = nsCRT::strlen(authenticator);
    SECItem *derEncoded = SEC_ASN1EncodeItem(nsnull, nsnull, &src,
                                     __WRAPPER_SEC_ASN1EncodeItem_Param4(SEC_UTF8StringTemplate));
    if (!derEncoded)
      return NS_ERROR_FAILURE;

    SECStatus srv = CRMF_CertRequestSetAuthenticatorControl(certReq, 
                                                            derEncoded);
    SECITEM_FreeItem(derEncoded, PR_TRUE);
    if (srv != SECSuccess)
      return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

// ASN1 DER encoding rules say that when encoding a BIT string,
// the length in the header for the bit string is the number 
// of "useful" bits in the BIT STRING.  So the function finds
// it and sets accordingly for the returned item.
static void
nsPrepareBitStringForEncoding (SECItem *bitsmap, SECItem *value)
{
  unsigned char onebyte;
  unsigned int i, len = 0;

  /* to prevent warning on some platform at compile time */
  onebyte = '\0';
  /* Get the position of the right-most turn-on bit */
  for (i = 0; i < (value->len ) * 8; ++i) {
    if (i % 8 == 0)
      onebyte = value->data[i/8];
    if (onebyte & 0x80)
      len = i;
    onebyte <<= 1;
  }

  bitsmap->data = value->data;
  /* Add one here since we work with base 1 */
  bitsmap->len = len + 1;
}

//This next section defines all the functions that sets the 
//keyUsageExtension for all the different types of key gens
//we handle.  The keyUsageExtension is just a bit flag extension
//that we set in wrapper functions that call straight into
//nsSetKeyUsageExtension.  There is one wrapper funciton for each
//keyGenType.  The correct function will eventually be called 
//by going through a switch statement based on the nsKeyGenType
//in the nsKeyPairInfo struct.
static nsresult
nsSetKeyUsageExtension(CRMFCertRequest *crmfReq,
                       unsigned char   keyUsage)
{
  SECItem                 *encodedExt= nsnull;
  SECItem                  keyUsageValue = { (SECItemType) 0, nsnull, 0 };
  SECItem                  bitsmap = { (SECItemType) 0, nsnull, 0 };
  SECStatus                srv;
  CRMFCertExtension       *ext = nsnull;
  CRMFCertExtCreationInfo  extAddParams;
  SEC_ASN1Template         bitStrTemplate = {SEC_ASN1_BIT_STRING, 0, nsnull,
                                             sizeof(SECItem)};

  keyUsageValue.data = &keyUsage;
  keyUsageValue.len  = 1;
  nsPrepareBitStringForEncoding(&bitsmap, &keyUsageValue);

  encodedExt = SEC_ASN1EncodeItem(nsnull, nsnull, &bitsmap,&bitStrTemplate);
  if (encodedExt == nsnull) {
    goto loser;
  }
  ext = CRMF_CreateCertExtension(SEC_OID_X509_KEY_USAGE, PR_TRUE, encodedExt);
  if (ext == nsnull) {
      goto loser;
  }
  extAddParams.numExtensions = 1;
  extAddParams.extensions = &ext;
  srv = CRMF_CertRequestSetTemplateField(crmfReq, crmfExtension,
                                         &extAddParams);
  if (srv != SECSuccess) {
      goto loser;
  }
  CRMF_DestroyCertExtension(ext);
  SECITEM_FreeItem(encodedExt, PR_TRUE);
  return NS_OK;
 loser:
  if (ext) {
    CRMF_DestroyCertExtension(ext);
  }
  if (encodedExt) {
      SECITEM_FreeItem(encodedExt, PR_TRUE);
  }
  return NS_ERROR_FAILURE;
}

static nsresult
nsSetRSADualUse(CRMFCertRequest *crmfReq)
{
  unsigned char keyUsage =   KU_DIGITAL_SIGNATURE
                           | KU_NON_REPUDIATION
                           | KU_KEY_ENCIPHERMENT;

  return nsSetKeyUsageExtension(crmfReq, keyUsage);
}

static nsresult
nsSetRSAKeyEx(CRMFCertRequest *crmfReq)
{
  unsigned char keyUsage = KU_KEY_ENCIPHERMENT;

  return nsSetKeyUsageExtension(crmfReq, keyUsage);
}

static nsresult
nsSetRSASign(CRMFCertRequest *crmfReq)
{
  unsigned char keyUsage = KU_DIGITAL_SIGNATURE;


  return nsSetKeyUsageExtension(crmfReq, keyUsage);
}

static nsresult
nsSetRSANonRepudiation(CRMFCertRequest *crmfReq)
{
  unsigned char keyUsage = KU_NON_REPUDIATION;

  return nsSetKeyUsageExtension(crmfReq, keyUsage);
}

static nsresult
nsSetRSASignNonRepudiation(CRMFCertRequest *crmfReq)
{
  unsigned char keyUsage = KU_DIGITAL_SIGNATURE |
                           KU_NON_REPUDIATION;

  return nsSetKeyUsageExtension(crmfReq, keyUsage);
}

static nsresult
nsSetDH(CRMFCertRequest *crmfReq)
{
  unsigned char keyUsage = KU_KEY_AGREEMENT;

  return nsSetKeyUsageExtension(crmfReq, keyUsage);
}

static nsresult
nsSetDSASign(CRMFCertRequest *crmfReq)
{
  unsigned char keyUsage = KU_DIGITAL_SIGNATURE;

  return nsSetKeyUsageExtension(crmfReq, keyUsage);
}

static nsresult
nsSetDSANonRepudiation(CRMFCertRequest *crmfReq)
{
  unsigned char keyUsage = KU_NON_REPUDIATION;

  return nsSetKeyUsageExtension(crmfReq, keyUsage);
}

static nsresult
nsSetDSASignNonRepudiation(CRMFCertRequest *crmfReq)
{
  unsigned char keyUsage = KU_DIGITAL_SIGNATURE |
                           KU_NON_REPUDIATION;

  return nsSetKeyUsageExtension(crmfReq, keyUsage);
}

static nsresult
nsSetKeyUsageExtension(CRMFCertRequest *crmfReq, nsKeyGenType keyGenType)
{
  nsresult rv;

  switch (keyGenType) {
  case rsaDualUse:
    rv = nsSetRSADualUse(crmfReq);
    break;
  case rsaEnc:
    rv = nsSetRSAKeyEx(crmfReq);
    break;
  case rsaSign:
    rv = nsSetRSASign(crmfReq);
    break;
  case rsaNonrepudiation:
    rv = nsSetRSANonRepudiation(crmfReq);
    break;
  case rsaSignNonrepudiation:
    rv = nsSetRSASignNonRepudiation(crmfReq);
    break;
  case dhEx:
    rv = nsSetDH(crmfReq);
    break;
  case dsaSign:
    rv = nsSetDSASign(crmfReq);
    break;
  case dsaNonrepudiation:
    rv = nsSetDSANonRepudiation(crmfReq);
    break;
  case dsaSignNonrepudiation:
    rv = nsSetDSASignNonRepudiation(crmfReq);
    break;
  default:
    rv = NS_ERROR_FAILURE;
    break;
  }
  return rv;
}

//Create a single CRMFCertRequest with all of the necessary parts 
//already installed.  The request returned by this function will
//have all the parts necessary and can just be added to a 
//Certificate Request Message.
static CRMFCertRequest*
nsCreateSingleCertReq(nsKeyPairInfo *keyInfo, char *reqDN, char *regToken, 
                      char *authenticator, nsNSSCertificate *wrappingCert)
{
  PRInt32 reqID;
  nsresult rv;

  //The draft says the ID of the request should be a random
  //number.  We don't have a way of tracking this number
  //to compare when the reply actually comes back,though.
  PK11_GenerateRandom((unsigned char*)&reqID, sizeof(reqID));
  CRMFCertRequest *certReq = CRMF_CreateCertRequest(reqID);
  if (!certReq)
    return nsnull;

  long version = SEC_CERTIFICATE_VERSION_3;
  SECStatus srv;
  CERTSubjectPublicKeyInfo *spki = nsnull;
  srv = CRMF_CertRequestSetTemplateField(certReq, crmfVersion, &version);
  if (srv != SECSuccess)
    goto loser;
  
  spki = SECKEY_CreateSubjectPublicKeyInfo(keyInfo->pubKey);
  if (!spki)
    goto loser;

  srv = CRMF_CertRequestSetTemplateField(certReq, crmfPublicKey, spki);
  SECKEY_DestroySubjectPublicKeyInfo(spki);
  if (srv != SECSuccess)
    goto loser;

  if (wrappingCert && ns_can_escrow(keyInfo->keyGenType)) {
    rv = nsSetEscrowAuthority(certReq, keyInfo, wrappingCert);
    if (NS_FAILED(rv))
      goto loser;
  }
  rv = nsSetDNForRequest(certReq, reqDN);
  if (NS_FAILED(rv))
    goto loser;

  rv = nsSetRegToken(certReq, regToken);
  if (NS_FAILED(rv))
    goto loser;

  rv = nsSetAuthenticator(certReq, authenticator);
  if (NS_FAILED(rv))
    goto loser;

 rv = nsSetKeyUsageExtension(certReq, keyInfo->keyGenType); 
  if (NS_FAILED(rv))
    goto loser;

  return certReq;
loser:
  if (certReq) {
    CRMF_DestroyCertRequest(certReq);
  }
  return nsnull;
}

/*
 * This function will set the Proof Of Possession (POP) for a request
 * associated with a key pair intended to do Key Encipherment.  Currently
 * this means encryption only keys.
 */
static nsresult
nsSetKeyEnciphermentPOP(CRMFCertReqMsg  *certReqMsg)
{
  SECItem       bitString;
  unsigned char der[2];
  SECStatus     srv;
  CRMFCertRequest *certReq =  CRMF_CertReqMsgGetCertRequest(certReqMsg);
  NS_ASSERTION(certReq,"Error getting the certRequest from the message");
  if (!certReq)
    return NS_ERROR_FAILURE;

  if (CRMF_CertRequestIsControlPresent(certReq,crmfPKIArchiveOptionsControl)) {
    /* For proof of possession on escrowed keys, we use the
     * this Message option of POPOPrivKey and include a zero
     * length bit string in the POP field.  This is OK because the encrypted
     * private key already exists as part of the PKIArchiveOptions
     * Control and that for all intents and purposes proves that
     * we do own the private key.
     */
    der[0] = 0x03; /*We've got a bit string          */
    der[1] = 0x00; /*We've got a 0 length bit string */
    bitString.data = der;
    bitString.len  = 2;
    srv = CRMF_CertReqMsgSetKeyEnciphermentPOP(certReqMsg, crmfThisMessage,
                                              crmfNoSubseqMess, &bitString);
  } else {
    /* If the encryption key is not being escrowed, then we set the 
     * Proof Of Possession to be a Challenge Response mechanism.
     */
    srv = CRMF_CertReqMsgSetKeyEnciphermentPOP(certReqMsg,
                                              crmfSubsequentMessage,
                                              crmfChallengeResp, nsnull);
  }
  CRMF_DestroyCertRequest(certReq);
  return (srv == SECSuccess) ? NS_OK : NS_ERROR_FAILURE;
}

//CRMF require ProofOfPossession to be set on a Certificate
//Request message.  Switch on the keyGenType here and add
//the appropriate POP.
static nsresult
nsSetProofOfPossession(CRMFCertReqMsg *certReqMsg, 
                       nsKeyPairInfo  *keyInfo)
{
  nsresult rv;
  switch (keyInfo->keyGenType) {
  case rsaSign:
  case rsaDualUse:
  case rsaNonrepudiation:
  case rsaSignNonrepudiation:
  case dsaSign:
  case dsaNonrepudiation:
  case dsaSignNonrepudiation:
    {
      SECStatus srv = CRMF_CertReqMsgSetSignaturePOP(certReqMsg,
                                                     keyInfo->privKey,
                                                     keyInfo->pubKey, nsnull,
                                                     nsnull, nsnull);
      rv = (srv == SECSuccess) ? NS_OK : NS_ERROR_FAILURE;
    }
    break;
  case rsaEnc:
    rv = nsSetKeyEnciphermentPOP(certReqMsg);
    break;
  case dhEx:
    /* This case may be supported in the future, but for now, we just fall 
     * though to the default case and return an error for diffie-hellman keys.
     */
  default:
    rv = NS_ERROR_FAILURE;
    break;
  }
  return rv;

}

//Create a Base64 encoded CRMFCertReqMsg that can be sent to a CA
//requesting one or more certificates to be issued.  This function
//creates a single cert request per key pair and then appends it to
//a message that is ultimately sent off to a CA.
static char*
nsCreateReqFromKeyPairs(nsKeyPairInfo *keyids, PRInt32 numRequests,
                        char *reqDN, char *regToken, char *authenticator,
                        nsNSSCertificate *wrappingCert) 
{
  // We'use the goto notation for clean-up purposes in this function
  // that calls the C API of NSS.
  PRInt32 i;
  // The ASN1 encoder in NSS wants the last entry in the array to be
  // NULL so that it knows when the last element is.
  CRMFCertReqMsg **certReqMsgs = new CRMFCertReqMsg*[numRequests+1];
  CRMFCertRequest *certReq;
  if (!certReqMsgs)
    return nsnull;
  memset(certReqMsgs, 0, sizeof(CRMFCertReqMsg*)*(1+numRequests));
  SECStatus srv;
  nsresult rv;
  CRMFCertReqMessages messages;
  memset(&messages, 0, sizeof(messages));
  messages.messages = certReqMsgs;
  SECItem *encodedReq;
  char *retString;
  for (i=0; i<numRequests; i++) {
    certReq = nsCreateSingleCertReq(&keyids[i], reqDN, regToken, authenticator,
                                    wrappingCert);
    if (!certReq)
      goto loser;

    certReqMsgs[i] = CRMF_CreateCertReqMsg();
    if (!certReqMsgs[i])
      goto loser;
    srv = CRMF_CertReqMsgSetCertRequest(certReqMsgs[i], certReq);
    if (srv != SECSuccess)
      goto loser;

    rv = nsSetProofOfPossession(certReqMsgs[i], &keyids[i]);
    if (NS_FAILED(rv))
      goto loser;
    CRMF_DestroyCertRequest(certReq);
  }
  encodedReq = SEC_ASN1EncodeItem(nsnull, nsnull, &messages,
                                  CRMFCertReqMessagesTemplate);
  nsFreeCertReqMessages(certReqMsgs, numRequests);

  retString = NSSBase64_EncodeItem (nsnull, nsnull, 0, encodedReq);
  SECITEM_FreeItem(encodedReq, PR_TRUE);
  return retString;
loser:
  nsFreeCertReqMessages(certReqMsgs,numRequests);
  return nsnull;;
}
                                                 
//The top level method which is a member of nsIDOMCrypto
//for generate a base64 encoded CRMF request.
NS_IMETHODIMP
nsCrypto::GenerateCRMFRequest(nsIDOMCRMFObject** aReturn)
{
  *aReturn = nsnull;
  nsresult nrv;
  nsCOMPtr<nsIXPConnect> xpc(do_GetService(nsIXPConnect::GetCID(), &nrv));
  NS_ENSURE_SUCCESS(nrv, nrv);

  nsCOMPtr<nsIXPCNativeCallContext> ncc;

  nrv = xpc->GetCurrentNativeCallContext(getter_AddRefs(ncc));
  NS_ENSURE_SUCCESS(nrv, nrv);

  if (!ncc)
    return NS_ERROR_NOT_AVAILABLE;

  PRUint32 argc;

  ncc->GetArgc(&argc);

  jsval *argv = nsnull;

  ncc->GetArgvPtr(&argv);

  JSContext *cx;

  ncc->GetJSContext(&cx);

  JSObject* script_obj = nsnull;
  nsCOMPtr<nsIXPConnectJSObjectHolder> holder;


  /*
   * Get all of the parameters.
   */
  if (((argc-5) % 3) != 0) {
    JS_ReportError(cx, "%s", "%s%s\n", JS_ERROR,
                  "incorrect number of parameters");
    return NS_ERROR_FAILURE;
  }
  
  if (JSVAL_IS_NULL(argv[0])) {
    JS_ReportError(cx, "%s%s\n", JS_ERROR, "no DN specified");
    return NS_ERROR_FAILURE;
  }
  
  JSString *jsString = JS_ValueToString(cx,argv[0]);
  
  char * reqDN = JS_GetStringBytes(jsString);
  char *regToken;
  if (JSVAL_IS_NULL(argv[1])) {
    regToken           = nsnull;
  } else {
    jsString = JS_ValueToString(cx, argv[1]);
    regToken = JS_GetStringBytes(jsString);
  }
  char *authenticator;
  if (JSVAL_IS_NULL(argv[2])) {
    authenticator           = nsnull;
  } else {
    jsString      = JS_ValueToString(cx, argv[2]);
    authenticator = JS_GetStringBytes(jsString);
  }
  char *eaCert;
  if (JSVAL_IS_NULL(argv[3])) {
    eaCert           = nsnull;
  } else {
    jsString     = JS_ValueToString(cx, argv[3]);
    eaCert       = JS_GetStringBytes(jsString);
  }
  if (JSVAL_IS_NULL(argv[4])) {
    JS_ReportError(cx, "%s%s\n", JS_ERROR, "no completion "
                   "function specified");
    return NS_ERROR_FAILURE;
  }
  jsString = JS_ValueToString(cx, argv[4]);
  char *jsCallback = JS_GetStringBytes(jsString);


  nrv = xpc->WrapNative(cx, ::JS_GetGlobalObject(cx),
                        NS_STATIC_CAST(nsIDOMCrypto *, this),
                        NS_GET_IID(nsIDOMCrypto), getter_AddRefs(holder));
  NS_ENSURE_SUCCESS(nrv, nrv);

  nrv = holder->GetJSObject(&script_obj);
  NS_ENSURE_SUCCESS(nrv, nrv);

  // Before doing this, lets' make sure the NSS Component has 
  // initialized itself.
  nsCOMPtr<nsISecurityManagerComponent> nss = 
                                      do_GetService(PSM_COMPONENT_CONTRACTID);
  //Put up some UI warning that someone is trying to 
  //escrow the private key.
  //Don't addref this copy.  That way ths reference goes away
  //at the same the nsIX09Cert ref goes away.
  nsNSSCertificate *escrowCert = nsnull;
  nsCOMPtr<nsIX509Cert> nssCert;
  PRBool willEscrow = PR_FALSE;
  if (eaCert) {
    SECItem certDer = {siBuffer, nsnull, 0};
    SECStatus srv = ATOB_ConvertAsciiToItem(&certDer, eaCert);
    if (srv != SECSuccess) {
      return NS_ERROR_FAILURE;
    }
    CERTCertificate *cert = CERT_NewTempCertificate(CERT_GetDefaultCertDB(),
                                                    &certDer, nsnull, PR_FALSE,
                                                    PR_TRUE);
    if (!cert)
      return NS_ERROR_FAILURE;

    escrowCert = new nsNSSCertificate(cert);
    CERT_DestroyCertificate(cert);
    nssCert = escrowCert;
    if (!nssCert)
      return NS_ERROR_OUT_OF_MEMORY;

    nsCOMPtr<nsIDOMCryptoDialogs> dialogs;
    nsresult rv = getNSSDialogs(getter_AddRefs(dialogs),
                                NS_GET_IID(nsIDOMCryptoDialogs));
    if (NS_FAILED(rv))
      return rv;

    PRBool okay=PR_FALSE;
    dialogs->ConfirmKeyEscrow(nssCert, &okay);
    if (!okay)
      return NS_OK;
    willEscrow = PR_TRUE;
  }
  nsCOMPtr<nsIInterfaceRequestor> uiCxt = new PipUIContext;
  PRInt32 numRequests = (argc - 5)/3;
  nsKeyPairInfo *keyids = new nsKeyPairInfo[numRequests];
  if (keyids == nsnull) {
    JS_ReportError(cx, "%s\n", JS_ERROR_INTERNAL);
    return NS_ERROR_OUT_OF_MEMORY;
  }
  memset(keyids, 0, sizeof(nsKeyPairInfo)*numRequests);
  int keyInfoIndex;
  PRUint32 i;
  PK11SlotInfo *slot = nsnull;
  // Go through all of the arguments and generate the appropriate key pairs.
  for (i=5,keyInfoIndex=0; i<argc; i+=3,keyInfoIndex++) {
    nrv = cryptojs_ReadArgsAndGenerateKey(cx, &argv[i], &keyids[keyInfoIndex],
                                         uiCxt, &slot, willEscrow);
                                       
    if (NS_FAILED(nrv)) {
      if (slot)
        PK11_FreeSlot(slot);
      nsFreeKeyPairInfo(keyids,numRequests);
      return nrv;
    }
  }
  // By this time we'd better have a slot for the key gen.
  NS_ASSERTION(slot, "There was no slot selected for key generation");
  if (slot) 
    PK11_FreeSlot(slot);

  char *encodedRequest = nsCreateReqFromKeyPairs(keyids, numRequests,
                                                 reqDN, regToken, 
                                                 authenticator,escrowCert);
#ifdef DEBUG_javi
  printf ("Created the folloing CRMF request:\n%s\n", encodedRequest);
#endif
  if (!encodedRequest) {
    return NS_ERROR_FAILURE;
  }                                                    
  nsCRMFObject *newObject = new nsCRMFObject();
  if (newObject == nsnull) {
    JS_ReportError(cx, "%s%s\n", JS_ERROR, "could not create crmf JS object");

    nsFreeKeyPairInfo(keyids,numRequests);
    return NS_ERROR_OUT_OF_MEMORY;
  }
  newObject->SetCRMFRequest(encodedRequest);
  *aReturn = newObject;
  //Give a reference to the returnee.
  NS_ADDREF(*aReturn);
  nsFreeKeyPairInfo(keyids, numRequests);

  // 
  // Post an event on the UI queue so that the JS gets called after
  // we return control to the JS layer.  Why do we have to this?
  // Because when this API was implemented for PSM 1.x w/ Communicator,
  // the only way to make this method work was to have a callback
  // in the JS layer that got called after key generation had happened.
  // So for backwards compatibility, we return control and then just post
  // an event to call the JS the script provides as the code to execute
  // when the request has been generated.
  //


  nsCOMPtr<nsIPrincipal>principals;
  principals = GetScriptPrincipal(cx);
  NS_ASSERTION(principals, "Couldn't get the principals");
  nsCryptoRunArgs *args = new nsCryptoRunArgs();
  if (!args)
    return NS_ERROR_OUT_OF_MEMORY;

  args->m_cx         = cx;
  args->m_scope      = JS_GetParent(cx, script_obj);
  args->m_jsCallback.Adopt(jsCallback ? nsCRT::strdup(jsCallback) : 0);
  args->m_principals = principals;
  
  nsCryptoRunnable *cryptoRunnable = new nsCryptoRunnable(args);
  if (!cryptoRunnable)
    return NS_ERROR_OUT_OF_MEMORY;

  CryptoRunnableEvent *runnable = new CryptoRunnableEvent(cryptoRunnable);
  if (!runnable) {
    delete cryptoRunnable;
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsCOMPtr<nsIEventQueue>uiQueue = dont_AddRef(GetUIEventQueue());
  uiQueue->PostEvent(runnable);
  return NS_OK;
}

//A wrapper for PLEvent that we can use to post
//our nsIRunnable Events.
static void PR_CALLBACK
handleCryptoRunnableEvent(CryptoRunnableEvent* aEvent)
{
  aEvent->mRunnable->Run();
}

static void PR_CALLBACK
destroyCryptoRunnableEvent(CryptoRunnableEvent* aEvent)
{
  delete aEvent;
}

CryptoRunnableEvent::CryptoRunnableEvent(nsIRunnable* runnable)
  :  mRunnable(runnable)
{
  NS_ADDREF(mRunnable);
  PL_InitEvent(this, nsnull, PLHandleEventProc(handleCryptoRunnableEvent),
               PLDestroyEventProc(&destroyCryptoRunnableEvent));
}

CryptoRunnableEvent::~CryptoRunnableEvent()
{
  NS_RELEASE(mRunnable);
}


// Reminder that we inherit the memory passed into us here.
// An implementation to let us back up certs as an event.
nsP12Runnable::nsP12Runnable(nsIX509Cert **certArr, PRInt32 numCerts,
                             nsIPK11Token *token)
{
  NS_INIT_REFCNT();
  mCertArr  = certArr;
  mNumCerts = numCerts;
  mToken = token;
}

nsP12Runnable::~nsP12Runnable()
{
  PRInt32 i;
  for (i=0; i<mNumCerts; i++) {
      NS_IF_RELEASE(mCertArr[i]);
  }
  delete []mCertArr;
}


//Quick helper function to alert users.
void
alertUser(const PRUnichar *message)
{
  nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService("@mozilla.org/embedcomp/window-watcher;1"));
  nsCOMPtr<nsIPrompt> prompter;
  if (wwatch)
  wwatch->GetNewPrompter(0, getter_AddRefs(prompter));

  if (prompter) {
    prompter->Alert(0, message);
  }
}

//Implementation that backs cert(s) into a PKCS12 file
NS_IMETHODIMP
nsP12Runnable::Run()
{
  NS_ASSERTION(mCertArr, "certArr is NULL while trying to back up");
  nsCOMPtr<nsIDOMCryptoDialogs> dialogs;
  nsresult rv = getNSSDialogs(getter_AddRefs(dialogs),
                              NS_GET_IID(nsIDOMCryptoDialogs));
  if (NS_FAILED(rv))
    return rv;

  nsString final;
  nsString temp;

  nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(kNSSComponentCID, &rv));
  if (NS_FAILED(rv))
    return rv;

  //Build up the message that let's the user know we're trying to 
  //make PKCS12 backups of the new certs.
  nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("ForcedBackup1").get(),
                                      final);
  final.Append(NS_LITERAL_STRING("\n\n").get());
  nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("ForcedBackup2").get(),
                                      temp);
  final.Append(temp.get());
  final.Append(NS_LITERAL_STRING("\n\n").get());

  nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("ForcedBackup3").get(),
                                      temp);

  final.Append(temp.get());
  alertUser(final.get());

  nsCOMPtr<nsIFilePicker> filePicker = 
                        do_CreateInstance("@mozilla.org/filepicker;1", &rv);
  if (!filePicker) {
    NS_ASSERTION(0, "Could not create a file picker when backing up certs.");
    return rv;
  }

  nsString filePickMessage;
  nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("chooseP12BackupFileDialog").get(),
                                      filePickMessage);
  filePicker->Init(nsnull, filePickMessage.get(), nsIFilePicker::modeSave);
  filePicker->AppendFilter(NS_LITERAL_STRING("PKCS12").get(),
                           NS_LITERAL_STRING("*.p12").get());
  filePicker->AppendFilters(nsIFilePicker::filterAll);

  PRInt16 dialogReturn;
  filePicker->Show(&dialogReturn);
  if (dialogReturn == nsIFilePicker::returnCancel)
    return NS_OK;  //User canceled.  It'd be nice if they couldn't, 
                   //but oh well.

  nsCOMPtr<nsILocalFile> localFile;
  rv = filePicker->GetFile(getter_AddRefs(localFile));
  if (NS_FAILED(rv))
    return NS_ERROR_FAILURE;

  nsPKCS12Blob p12Cxt;
  
  p12Cxt.SetToken(mToken);
  p12Cxt.ExportToFile(localFile, mCertArr, mNumCerts);
  return NS_OK;
}

nsCryptoRunArgs::nsCryptoRunArgs() 
{
  NS_INIT_REFCNT();
}
nsCryptoRunArgs::~nsCryptoRunArgs() {}


nsCryptoRunnable::nsCryptoRunnable(nsCryptoRunArgs *args)
{
  NS_INIT_REFCNT();
  NS_ASSERTION(args,"Passed nsnull to nsCryptoRunnable constructor.");
  m_args = args;
  NS_IF_ADDREF(m_args);
  JS_AddNamedRoot(args->m_cx, &args->m_scope,"nsCryptoRunnable::mScope");
}

nsCryptoRunnable::~nsCryptoRunnable()
{
  JS_RemoveRoot(m_args->m_cx, &m_args->m_scope);
  NS_IF_RELEASE(m_args);
}

//Implementation that runs the callback passed to 
//crypto.generateCRMFRequest as an event.
NS_IMETHODIMP
nsCryptoRunnable::Run()
{
  JSPrincipals *principals;

  nsresult rv = m_args->m_principals->GetJSPrincipals(&principals);
  if (NS_FAILED(rv))
    return NS_ERROR_FAILURE;

  jsval retval;
  if (JS_EvaluateScriptForPrincipals(m_args->m_cx, m_args->m_scope, principals,
                                     m_args->m_jsCallback, 
                                     nsCRT::strlen(m_args->m_jsCallback),
                                     nsnull, 0,
                                     &retval) != JS_TRUE) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

//Quick helper function to check if a newly issued cert
//already exists in the user's database.
static PRBool
nsCertAlreadyExists(SECItem *derCert)
{
  CERTCertDBHandle *handle = CERT_GetDefaultCertDB();
  PRArenaPool *arena;
  CERTCertificate *cert;
  PRBool retVal = PR_FALSE;

  arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
  NS_ASSERTION(arena, "Couldn't allocate an arena!");
  if (!arena)
    return PR_FALSE; //What else could we return?

#ifdef NSS_3_4
  cert = CERT_FindCertByDERCert(handle, derCert);
#else
  SECItem key;
  SECStatus srv = CERT_KeyFromDERCert(arena, derCert, &key);
  if (srv != SECSuccess)
    return PR_FALSE;

  cert = CERT_FindCertByKey(handle, &key);
#endif
  if (cert) {
    if (cert->isperm && !cert->nickname && !cert->emailAddr) {
      //If the cert doesn't have a nickname or email addr, it is
      //bogus cruft, so delete it.
      SEC_DeletePermCertificate(cert);
    } else if (cert->isperm) {
      retVal = PR_TRUE;
    }
    CERT_DestroyCertificate(cert);
  }
  return retVal;
}

static PRInt32
nsCertListCount(CERTCertList *certList)
{
  PRInt32 numCerts = 0;
  CERTCertListNode *node;

  node = CERT_LIST_HEAD(certList);
  while (!CERT_LIST_END(node, certList)) {
    numCerts++;
    node = CERT_LIST_NEXT(node);
  }
  return numCerts;
}


//Import user certificates that arrive as a CMMF base64 encoded
//string.
NS_IMETHODIMP
nsCrypto::ImportUserCertificates(const nsAString& aNickname, 
                                 const nsAString& aCmmfResponse, 
                                 PRBool aDoForcedBackup, 
                                 nsAString& aReturn)
{
  char *nickname=nsnull, *cmmfResponse=nsnull;
  char *retString=nsnull;
  char *freeString=nsnull;
  CMMFCertRepContent *certRepContent = nsnull;
  int numResponses = 0;
  nsIX509Cert **certArr = nsnull;
  int i;
  CMMFCertResponse *currResponse;
  CMMFPKIStatus reqStatus;
  CERTCertificate *currCert;
  PK11SlotInfo *slot;
  PRBool freeLocalNickname = PR_FALSE;
  char *localNick;
  nsCOMPtr<nsIInterfaceRequestor> ctx = new PipUIContext();
  nsresult rv = NS_OK;
  CERTCertList *caPubs = nsnull;
  nsCOMPtr<nsIPK11Token> token;

  nickname = ToNewCString(aNickname);
  cmmfResponse = ToNewCString(aCmmfResponse);
  if (nsCRT::strcmp("null", nickname) == 0) {
    nsMemory::Free(nickname);
    nickname = nsnull;
  }

  SECItem cmmfDer = {siBuffer, nsnull, 0};
  SECStatus srv = ATOB_ConvertAsciiToItem(&cmmfDer, cmmfResponse);

  if (srv != SECSuccess) {
    rv = NS_ERROR_FAILURE;
    goto loser;
  }

  certRepContent = CMMF_CreateCertRepContentFromDER(CERT_GetDefaultCertDB(),
                                                    (const char*)cmmfDer.data,
                                                    cmmfDer.len);
  if (!certRepContent) {
    rv = NS_ERROR_FAILURE;
    goto loser;
  }

  numResponses = CMMF_CertRepContentGetNumResponses(certRepContent);

  if (aDoForcedBackup) {
    //We've been asked to force the user to back up these
    //certificates.  Let's keep an array of them around which
    //we pass along to the nsP12Runnable to use.
    certArr = new nsIX509Cert*[numResponses];
    // If this is NULL, chances are we're gonna fail really soon,
    // but let's try to keep going just in case.
    if (!certArr)
      aDoForcedBackup = PR_FALSE;

    memset(certArr, 0, sizeof(nsIX509Cert*)*numResponses);
  }
  for (i=0; i<numResponses; i++) {
    currResponse = CMMF_CertRepContentGetResponseAtIndex(certRepContent,i);
    if (!currResponse) {
      rv = NS_ERROR_FAILURE;
      goto loser;
    }
    reqStatus = CMMF_CertResponseGetPKIStatusInfoStatus(currResponse);
    if (!(reqStatus == cmmfGranted || reqStatus == cmmfGrantedWithMods)) {
      // The CA didn't give us the cert we requested.
      rv = NS_ERROR_FAILURE;
      goto loser;
    }
    currCert = CMMF_CertResponseGetCertificate(currResponse, 
                                               CERT_GetDefaultCertDB());
    if (!currCert) {
      rv = NS_ERROR_FAILURE;
      goto loser;
    }

    if (nsCertAlreadyExists(&currCert->derCert)) {
      if (aDoForcedBackup) {
        certArr[i] = new nsNSSCertificate(currCert);
        NS_ADDREF(certArr[i]);
      }
      CERT_DestroyCertificate(currCert);
      CMMF_DestroyCertResponse(currResponse);
      continue;
    }
    // Let's figure out which nickname to give the cert.  If 
    // a certificate with the same subject name already exists,
    // then just use that one, otherwise, get the default nickname.
#ifdef NSS_3_4
    if (currCert->nickname) {
      localNick = currCert->nickname;
    }
#else
    if (currCert->subjectList && currCert->subjectList->entry &&
      currCert->subjectList->entry->nickname) {
      localNick = currCert->subjectList->entry->nickname;
    }
#endif
    else if (nickname == nsnull || nickname[0] == '\0') {
      localNick = default_nickname(currCert, ctx);
      freeLocalNickname = PR_TRUE;
    } else {
      //This is the case where we're getting a brand new
      //cert that doesn't have the same subjectName as a cert
      //that already exists in our db and the CA page has 
      //designated a nickname to use for the newly issued cert.
      localNick = nickname;
    }
    slot = PK11_ImportCertForKey(currCert, localNick, ctx);
    if (freeLocalNickname) {
      nsMemory::Free(localNick);
      freeLocalNickname = PR_FALSE;
    }
    if (slot == nsnull) {
      rv = NS_ERROR_FAILURE;
      goto loser;
    }
    if (aDoForcedBackup) {
      certArr[i] = new nsNSSCertificate(currCert);
      NS_ADDREF(certArr[i]);
    }
    CERT_DestroyCertificate(currCert);

    if (!token)
      token = new nsPK11Token(slot);

    PK11_FreeSlot(slot);
    CMMF_DestroyCertResponse(currResponse);
  }
  //Let the loser: label take care of freeing up our reference to
  //nickname (This way we don't free it twice and avoid crashing.
  //That would be a good thing.

  retString = "";

  //Import the root chain into the cert db.
  caPubs = CMMF_CertRepContentGetCAPubs(certRepContent);
  if (caPubs) {
    PRInt32 numCAs = nsCertListCount(caPubs);
    
    NS_ASSERTION(numCAs > 0, "Invalid number of CA's");
    if (numCAs > 0) {
      CERTCertListNode *node;
      SECItem *derCerts;

      derCerts = NS_STATIC_CAST(SECItem*,
                                nsMemory::Alloc(sizeof(SECItem)*numCAs));
      if (!derCerts) {
        rv = NS_ERROR_OUT_OF_MEMORY;
        goto loser;
      }
      for (node = CERT_LIST_HEAD(caPubs), i=0; 
           !CERT_LIST_END(node, caPubs);
           node = CERT_LIST_NEXT(node), i++) {
        derCerts[i] = node->cert->derCert;
      }
      CERT_ImportCAChain(derCerts, numCAs, certUsageUserCertImport);
      nsMemory::Free(derCerts);
    }
  }

  if (aDoForcedBackup) {
    // I can't pop up a file picker from the depths of JavaScript,
    // so I'll just post an event on the UI queue to do the backups
    // later.
    nsCOMPtr<nsIRunnable> p12Runnable = new nsP12Runnable(certArr, numResponses,
                                                          token);
    CryptoRunnableEvent *runnable;
    if (!p12Runnable) {
      rv = NS_ERROR_FAILURE;
      goto loser;
    }

    // null out the certArr pointer which has now been inherited by
    // the nsP12Runnable instance so that we don't free up the
    // memory on the way out.
    certArr = nsnull;

    runnable = new CryptoRunnableEvent(p12Runnable);
    if (!runnable) {
      rv = NS_ERROR_FAILURE;
      goto loser;
    }
    nsCOMPtr<nsIEventQueue>uiQueue = dont_AddRef(GetUIEventQueue());
    uiQueue->PostEvent(runnable);
  }

 loser:
  if (certArr) {
    for (i=0; i<numResponses; i++) {
      NS_IF_RELEASE(certArr[i]);
    }
    delete []certArr;
  }
  aReturn.Assign(NS_ConvertASCIItoUCS2(retString));
  if (freeString != nsnull) {
    PR_smprintf_free(freeString);
  }
  if (nickname) {
    nsCRT::free(nickname);
  }
  if (cmmfResponse) {
    nsCRT::free(cmmfResponse);
  }
  if (certRepContent) {
    CMMF_DestroyCertRepContent(certRepContent);
  }
  return rv;
}

NS_IMETHODIMP
nsCrypto::PopChallengeResponse(const nsAString& aChallenge, 
                               nsAString& aReturn)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCrypto::Random(PRInt32 aNumBytes, nsAString& aReturn)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCrypto::SignText(nsAString& aReturn)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
nsCrypto::Alert(const nsAString& aMessage)
{
  PRUnichar *message = ToNewUnicode(aMessage);
  alertUser(message);
  nsMemory::Free(message);
  return NS_OK;
}

//Logout out of all installed PKCS11 tokens.
NS_IMETHODIMP
nsCrypto::Logout()
{
  nsCOMPtr<nsISecurityManagerComponent> nss = 
                     do_GetService(PSM_COMPONENT_CONTRACTID);
  PK11_LogoutAll();
  return NS_OK;
}

NS_IMETHODIMP
nsCrypto::DisableRightClick()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsCRMFObject::nsCRMFObject()
{
  NS_INIT_ISUPPORTS();
}

nsCRMFObject::~nsCRMFObject()
{
}

nsresult
nsCRMFObject::init()
{
  return NS_OK;
}

NS_IMETHODIMP
nsCRMFObject::GetRequest(nsAString& aRequest)
{
  aRequest.Assign(mBase64Request);
  return NS_OK;
}

nsresult
nsCRMFObject::SetCRMFRequest(char *inRequest)
{
  mBase64Request.AssignWithConversion(inRequest);  
  return NS_OK;
}

nsPkcs11::nsPkcs11()
{
  NS_INIT_REFCNT();
}

nsPkcs11::~nsPkcs11()
{
}

//Quick function to confirm with the user.
PRBool
confirm_user(const PRUnichar *message)
{
  PRBool confirmation = PR_FALSE;
  nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService("@mozilla.org/embedcomp/window-watcher;1"));
  nsCOMPtr<nsIPrompt> prompter;
  if (wwatch)
    wwatch->GetNewPrompter(0, getter_AddRefs(prompter));

  if (prompter) {
    prompter->Confirm(0, message, &confirmation);
  }

  return confirmation;
}

//Delete a PKCS11 module from the user's profile.
NS_IMETHODIMP
nsPkcs11::Deletemodule(const nsAString& aModuleName, PRInt32* aReturn)
{
  nsresult rv;
  nsString errorMessage;

  nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(kNSSComponentCID, &rv));
  if (aModuleName.IsEmpty()) {
    *aReturn = JS_ERR_BAD_MODULE_NAME;
    nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("DelModuleBadName").get(),
                                        errorMessage);
    alertUser(errorMessage.get());
    return NS_OK;
  }
  nsString final;
  nsXPIDLString temp;
  //Make sure the user knows we're trying to do this.
  nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("DelModuleWarning").get(),
                                      final);
  final.Append(NS_LITERAL_STRING("\n").get());
  PRUnichar *tempUni = ToNewUnicode(aModuleName);
  const PRUnichar *formatStrings[1] = { tempUni };
  rv = nssComponent->PIPBundleFormatStringFromName(NS_LITERAL_STRING("AddModuleName").get(),
                                                   formatStrings, 1,
                                                   getter_Copies(temp));
  nsMemory::Free(tempUni);
  final.Append(temp);
  if (!confirm_user(final.get())) {
    *aReturn = JS_ERR_USER_CANCEL_ACTION;
    return NS_OK;
  }
  
  char *modName = ToNewCString(aModuleName);
  PRInt32 modType;
  SECStatus srv = SECMOD_DeleteModule(modName, &modType);
  if (srv == SECSuccess) {
    if (modType == SECMOD_EXTERNAL) {
      nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("DelModuleExtSuccess").get(),
                                          errorMessage);
      *aReturn = JS_OK_DEL_EXTERNAL_MOD;
    } else {
      nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("DelModuleIntSuccess").get(),
                                          errorMessage);
      *aReturn = JS_OK_DEL_INTERNAL_MOD;
    }
  } else {
    *aReturn = JS_ERR_DEL_MOD;
    nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("DelModuleError").get(),
                                        errorMessage);
  }
  alertUser(errorMessage.get());
  return NS_OK;
}

//Add a new PKCS11 module to the user's profile.
NS_IMETHODIMP
nsPkcs11::Addmodule(const nsAString& aModuleName, 
                    const nsAString& aLibraryFullPath, 
                    PRInt32 aCryptoMechanismFlags, 
                    PRInt32 aCipherFlags, PRInt32* aReturn)
{
  nsresult rv;
  nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(kNSSComponentCID, &rv));
  nsString final;
  nsXPIDLString temp;

  rv = nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("AddModulePrompt").get(),
                                           final);
  if (NS_FAILED(rv))
    return rv;

  final.Append(NS_LITERAL_STRING("\n").get());
  
  PRUnichar *tempUni = ToNewUnicode(aModuleName); 
  const PRUnichar *formatStrings[1] = { tempUni };
  rv = nssComponent->PIPBundleFormatStringFromName(NS_LITERAL_STRING("AddModuleName").get(),
                                                   formatStrings, 1,
                                                   getter_Copies(temp));
  nsMemory::Free(tempUni);

  if (NS_FAILED(rv))
    return rv;

  final.Append(temp);
  final.Append(NS_LITERAL_STRING("\n").get());

  tempUni = ToNewUnicode(aLibraryFullPath);
  formatStrings[0] = tempUni;
  rv = nssComponent->PIPBundleFormatStringFromName(NS_LITERAL_STRING("AddModulePath").get(),
                                                   formatStrings, 1,
                                                   getter_Copies(temp));
  nsMemory::Free(tempUni);
  if (NS_FAILED(rv))
    return rv;

  final.Append(temp);
  final.Append(NS_LITERAL_STRING("\n").get());
 
  if (!confirm_user(final.get())) {
    // The user has canceled. So let's return now.
    *aReturn = JS_ERR_USER_CANCEL_ACTION;
    return NS_OK;
  }
  
  char *moduleName = ToNewCString(aModuleName);
  char *fullPath   = ToNewCString(aLibraryFullPath);
  PRUint32 mechFlags = SECMOD_PubMechFlagstoInternal(aCryptoMechanismFlags);
  PRUint32 cipherFlags = SECMOD_PubCipherFlagstoInternal(aCipherFlags);
  SECStatus srv = SECMOD_AddNewModule(moduleName, fullPath, 
                                      mechFlags, cipherFlags);
  nsMemory::Free(moduleName);
  nsMemory::Free(fullPath);

  // The error message we report to the user depends directly on 
  // what the return value for SEDMOD_AddNewModule is
  switch (srv) {
  case SECSuccess:
    nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("AddModuleSuccess").get(),
                                        final);
    *aReturn = JS_OK_ADD_MOD;
    break;
  case SECFailure:
    nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("AddModuleFailure").get(),
                                        final);
    *aReturn = JS_ERR_ADD_MOD;
    break;
  case -2:
    nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("AddModuleDup").get(),
                                        final);
    *aReturn = JS_ERR_ADD_DUPLICATE_MOD;
    break;
  default:
    NS_ASSERTION(0,"Bogus return value, this should never happen");
    return NS_ERROR_FAILURE;
  }
  alertUser(final.get());
  return NS_OK;
}

