/*
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
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */
/*
 * This file implements the Symkey wrapper and the PKCS context
 * Interfaces.
 */
#include "seccomon.h"
#include "secmod.h"
#include "nssilock.h"
#include "secmodi.h"
#include "pkcs11.h"
#include "pk11func.h"
#include "cert.h"
#include "secitem.h"
#include "key.h"
#include "hasht.h"
#include "secoid.h"
#include "pkcs7t.h"
#include "cmsreclist.h"

#include "certdb.h"
#include "secerr.h"
#include "sslerr.h"

#ifndef NSS_3_4_CODE
#define NSS_3_4_CODE
#endif /* NSS_3_4_CODE */
#include "pki3hack.h"
#include "dev3hack.h"

#include "dev.h" 
#include "nsspki.h"
#include "pki.h"
#include "pkim.h"
#include "pkitm.h"

#define PK11_SEARCH_CHUNKSIZE 10

CK_OBJECT_HANDLE
pk11_FindPubKeyByAnyCert(CERTCertificate *cert, PK11SlotInfo **slot, void *wincx);

struct nss3_cert_cbstr {
    SECStatus(* callback)(CERTCertificate*, void *);
    nssList *cached;
    void *arg;
};

/* Translate from NSSCertificate to CERTCertificate, then pass the latter
 * to a callback.
 */
static PRStatus convert_cert(NSSCertificate *c, void *arg)
{
    CERTCertificate *nss3cert;
    SECStatus secrv;
    struct nss3_cert_cbstr *nss3cb = (struct nss3_cert_cbstr *)arg;
    nss3cert = STAN_GetCERTCertificate(c);
    if (!nss3cert) return PR_FAILURE;
    secrv = (*nss3cb->callback)(nss3cert, nss3cb->arg);
    return (secrv) ? PR_FAILURE : PR_SUCCESS;
}

static PRStatus convert_and_cache_cert(NSSCertificate *c, void *arg)
{
    PRStatus nssrv;
    NSSTrustDomain *td = STAN_GetDefaultTrustDomain();
    struct nss3_cert_cbstr *nss3cb = (struct nss3_cert_cbstr *)arg;
    NSSCertificate *cp = nssCertificate_AddRef(c);
    /* The cert coming in has been retrieved from a token.  It was not in
     * the cache when the search was begun.  But it may be in the cache now,
     * and if it isn't, it will be, because it is going to be cracked into
     * a CERTCertificate and fed into the callback.
     */
    nssrv = nssTrustDomain_AddCertsToCache(td, &c, 1);
    /* This is why the hack of copying the cert was done above.  The pointer
     * c passed to this function is provided by retrieve_cert.  That function
     * will destroy the pointer once this function returns.  Since c is a local
     * copy, there is no way to notify retrieve_cert if it has changed.  That
     * would happen if the above call to add it to the cache found the cert
     * already there.  In that case, the pointer c passed to the callback
     * below will be the cached cert, and the pointer c that retrieve_cert
     * has will be the same as the copy made above.  Thus, retrieve_cert will
     * destroy the reference to the copy, the callback will use the reference
     * to the cached entry, and everyone should be happy.
     */
    nssrv = convert_cert(c, arg);
    /* This function owns a reference to the cert, either from the AddRef
     * or by getting it from the cache.
     */
    CERT_DestroyCertificate(STAN_GetCERTCertificate(c));
    return nssrv;
}

/* this is redeclared from trustdomain.c, but this code is just 3.4 glue
 * anyway
 */
static void cert_destructor(void *el)
{
    NSSCertificate *c = (NSSCertificate *)el;
    CERTCertificate *cert = STAN_GetCERTCertificate(c);
    /* It's already been obtained as a CERTCertificate, so it must
     * be destroyed as one
     */
    CERT_DestroyCertificate(cert);
}

void
PK11Slot_SetNSSToken(PK11SlotInfo *sl, NSSToken *nsst) 
{
    sl->nssToken = nsst;
}

NSSToken *
PK11Slot_GetNSSToken(PK11SlotInfo *sl) 
{
    return sl->nssToken;
}

/*
 * build a cert nickname based on the token name and the label of the 
 * certificate If the label in NULL, build a label based on the ID.
 */
static int toHex(int x) { return (x < 10) ? (x+'0') : (x+'a'-10); }
#define MAX_CERT_ID 4
#define DEFAULT_STRING "Cert ID "
static char *
pk11_buildNickname(PK11SlotInfo *slot,CK_ATTRIBUTE *cert_label,
			CK_ATTRIBUTE *key_label, CK_ATTRIBUTE *cert_id)
{
    int prefixLen = PORT_Strlen(slot->token_name);
    int suffixLen = 0;
    char *suffix = NULL;
    char buildNew[sizeof(DEFAULT_STRING)+MAX_CERT_ID*2];
    char *next,*nickname;

    if (cert_label && (cert_label->ulValueLen)) {
	suffixLen = cert_label->ulValueLen;
	suffix = (char*)cert_label->pValue;
    } else if (key_label && (key_label->ulValueLen)) {
	suffixLen = key_label->ulValueLen;
	suffix = (char*)key_label->pValue;
    } else if (cert_id && cert_id->ulValueLen > 0) {
	int i,first = cert_id->ulValueLen - MAX_CERT_ID;
	int offset = sizeof(DEFAULT_STRING);
	char *idValue = (char *)cert_id->pValue;

	PORT_Memcpy(buildNew,DEFAULT_STRING,sizeof(DEFAULT_STRING)-1);
	next = buildNew + offset;
	if (first < 0) first = 0;
	for (i=first; i < (int) cert_id->ulValueLen; i++) {
		*next++ = toHex((idValue[i] >> 4) & 0xf);
		*next++ = toHex(idValue[i] & 0xf);
	}
	*next++ = 0;
	suffix = buildNew;
	suffixLen = PORT_Strlen(buildNew);
    } else {
	PORT_SetError( SEC_ERROR_LIBRARY_FAILURE );
	return NULL;
    }

    /* if is internal key slot, add code to skip the prefix!! */
    next = nickname = (char *)PORT_Alloc(prefixLen+1+suffixLen+1);
    if (nickname == NULL) return NULL;

    PORT_Memcpy(next,slot->token_name,prefixLen);
    next += prefixLen;
    *next++ = ':';
    PORT_Memcpy(next,suffix,suffixLen);
    next += suffixLen;
    *next++ = 0;
    return nickname;
}

/*
 * return the object handle that matches the template
 */
CK_OBJECT_HANDLE
pk11_FindObjectByTemplate(PK11SlotInfo *slot,CK_ATTRIBUTE *theTemplate,int tsize)
{
    CK_OBJECT_HANDLE object;
    CK_RV crv;
    CK_ULONG objectCount;

    /*
     * issue the find
     */
    PK11_EnterSlotMonitor(slot);
    crv=PK11_GETTAB(slot)->C_FindObjectsInit(slot->session, theTemplate, tsize);
    if (crv != CKR_OK) {
        PK11_ExitSlotMonitor(slot);
	PORT_SetError( PK11_MapError(crv) );
	return CK_INVALID_HANDLE;
    }

    crv=PK11_GETTAB(slot)->C_FindObjects(slot->session,&object,1,&objectCount);
    PK11_GETTAB(slot)->C_FindObjectsFinal(slot->session);
    PK11_ExitSlotMonitor(slot);
    if ((crv != CKR_OK) || (objectCount < 1)) {
	/* shouldn't use SSL_ERROR... here */
	PORT_SetError( crv != CKR_OK ? PK11_MapError(crv) :
						  SSL_ERROR_NO_CERTIFICATE);
	return CK_INVALID_HANDLE;
    }

    /* blow up if the PKCS #11 module returns us and invalid object handle */
    PORT_Assert(object != CK_INVALID_HANDLE);
    return object;
} 

/*
 * return all the object handles that matches the template
 */
CK_OBJECT_HANDLE *
pk11_FindObjectsByTemplate(PK11SlotInfo *slot,
		CK_ATTRIBUTE *findTemplate,int findCount,int *object_count) {
    CK_OBJECT_HANDLE *objID = NULL;
    CK_ULONG returned_count = 0;
    CK_RV crv;


    PK11_EnterSlotMonitor(slot);
    crv = PK11_GETTAB(slot)->C_FindObjectsInit(slot->session, findTemplate, 
								findCount);
    if (crv != CKR_OK) {
	PK11_ExitSlotMonitor(slot);
	PORT_SetError( PK11_MapError(crv) );
	*object_count = -1;
	return NULL;
    }


    /*
     * collect all the Matching Objects
     */
    do {
	CK_OBJECT_HANDLE *oldObjID = objID;

	if (objID == NULL) {
    	    objID = (CK_OBJECT_HANDLE *) PORT_Alloc(sizeof(CK_OBJECT_HANDLE)*
				(*object_count+ PK11_SEARCH_CHUNKSIZE));
	} else {
    	    objID = (CK_OBJECT_HANDLE *) PORT_Realloc(objID,
		sizeof(CK_OBJECT_HANDLE)*(*object_count+PK11_SEARCH_CHUNKSIZE));
	}

	if (objID == NULL) {
	    if (oldObjID) PORT_Free(oldObjID);
	    break;
	}
    	crv = PK11_GETTAB(slot)->C_FindObjects(slot->session,
		&objID[*object_count],PK11_SEARCH_CHUNKSIZE,&returned_count);
	if (crv != CKR_OK) {
	    PORT_SetError( PK11_MapError(crv) );
	    PORT_Free(objID);
	    objID = NULL;
	    break;
    	}
	*object_count += returned_count;
    } while (returned_count == PK11_SEARCH_CHUNKSIZE);

    PK11_GETTAB(slot)->C_FindObjectsFinal(slot->session);
    PK11_ExitSlotMonitor(slot);

    if (objID && (*object_count == 0)) {
	PORT_Free(objID);
	return NULL;
    }
    if (objID == NULL) *object_count = -1;
    return objID;
}
/*
 * given a PKCS #11 object, match it's peer based on the KeyID. searchID
 * is typically a privateKey or a certificate while the peer is the opposite
 */
CK_OBJECT_HANDLE
PK11_MatchItem(PK11SlotInfo *slot, CK_OBJECT_HANDLE searchID,
				 		CK_OBJECT_CLASS matchclass)
{
    CK_ATTRIBUTE theTemplate[] = {
	{ CKA_ID, NULL, 0 },
	{ CKA_CLASS, NULL, 0 }
    };
    /* if you change the array, change the variable below as well */
    CK_ATTRIBUTE *keyclass = &theTemplate[1];
    int tsize = sizeof(theTemplate)/sizeof(theTemplate[0]);
    /* if you change the array, change the variable below as well */
    CK_OBJECT_HANDLE peerID;
    CK_OBJECT_HANDLE parent;
    PRArenaPool *arena;
    CK_RV crv;

    /* now we need to create space for the public key */
    arena = PORT_NewArena( DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) return CK_INVALID_HANDLE;

    crv = PK11_GetAttributes(arena,slot,searchID,theTemplate,tsize);
    if (crv != CKR_OK) {
	PORT_FreeArena(arena,PR_FALSE);
	PORT_SetError( PK11_MapError(crv) );
	return CK_INVALID_HANDLE;
    }

    /*
     * issue the find
     */
    parent = *(CK_OBJECT_CLASS *)(keyclass->pValue);
    *(CK_OBJECT_CLASS *)(keyclass->pValue) = matchclass;

    peerID = pk11_FindObjectByTemplate(slot,theTemplate,tsize);
    PORT_FreeArena(arena,PR_FALSE);

    return peerID;
}

PRBool
PK11_IsUserCert(PK11SlotInfo *slot, CERTCertificate *cert,
						CK_OBJECT_HANDLE certID)
{
    CK_OBJECT_CLASS theClass;

    if (slot == NULL) return PR_FALSE;
    if (cert == NULL) return PR_FALSE;

    theClass = CKO_PRIVATE_KEY;
    if (!PK11_IsLoggedIn(slot,NULL) && PK11_NeedLogin(slot)) {
	theClass = CKO_PUBLIC_KEY;
    }
    if (PK11_MatchItem(slot, certID , theClass) != CK_INVALID_HANDLE) {
	return PR_TRUE;
    }

   if (theClass == CKO_PUBLIC_KEY) {
	SECKEYPublicKey *pubKey= CERT_ExtractPublicKey(cert);
	CK_ATTRIBUTE theTemplate;

	if (pubKey == NULL) {
	   return PR_FALSE;
	}

	PK11_SETATTRS(&theTemplate,0,NULL,0);
	switch (pubKey->keyType) {
	case rsaKey:
	    PK11_SETATTRS(&theTemplate,CKA_MODULUS, pubKey->u.rsa.modulus.data,
						pubKey->u.rsa.modulus.len);
	    break;
	case dsaKey:
	    PK11_SETATTRS(&theTemplate,CKA_VALUE, pubKey->u.dsa.publicValue.data,
						pubKey->u.dsa.publicValue.len);
	case dhKey:
	    PK11_SETATTRS(&theTemplate,CKA_VALUE, pubKey->u.dh.publicValue.data,
						pubKey->u.dh.publicValue.len);
	    break;
	case keaKey:
	case fortezzaKey:
	case nullKey:
	    /* fall through and return false */
	    break;
	}

	if (theTemplate.ulValueLen == 0) {
	    SECKEY_DestroyPublicKey(pubKey);
	    return PR_FALSE;
	}
	pk11_SignedToUnsigned(&theTemplate);
	if (pk11_FindObjectByTemplate(slot,&theTemplate,1) != CK_INVALID_HANDLE) {
	    SECKEY_DestroyPublicKey(pubKey);
	    return PR_TRUE;
	}
	SECKEY_DestroyPublicKey(pubKey);
    }
    return PR_FALSE;
}

/*
 * Check out if a cert has ID of zero. This is a magic ID that tells
 * NSS that this cert may be an automagically trusted cert.
 * The Cert has to be self signed as well. That check is done elsewhere.
 *  
 */
PRBool
pk11_isID0(PK11SlotInfo *slot, CK_OBJECT_HANDLE certID)
{
    CK_ATTRIBUTE keyID = {CKA_ID, NULL, 0};
    PRBool isZero = PR_FALSE;
    int i;
    CK_RV crv;


    crv = PK11_GetAttributes(NULL,slot,certID,&keyID,1);
    if (crv != CKR_OK) {
	return isZero;
    }

    if (keyID.ulValueLen != 0) {
	char *value = (char *)keyID.pValue;
	isZero = PR_TRUE; /* ID exists, may be zero */
	for (i=0; i < (int) keyID.ulValueLen; i++) {
	    if (value[i] != 0) {
		isZero = PR_FALSE; /* nope */
		break;
	    }
	}
    }
    PORT_Free(keyID.pValue);
    return isZero;

}
     
CERTCertificate
*pk11_fastCert(PK11SlotInfo *slot, CK_OBJECT_HANDLE certID, 
			CK_ATTRIBUTE *privateLabel, char **nickptr)
{
    CK_ATTRIBUTE certTemp[] = {
	{ CKA_ID, NULL, 0 },
	{ CKA_VALUE, NULL, 0 },
	{ CKA_LABEL, NULL, 0 }
    };
    CK_ATTRIBUTE *id = &certTemp[0];
    CK_ATTRIBUTE *certDER = &certTemp[1];
    CK_ATTRIBUTE *label = &certTemp[2];
    SECItem derCert;
    int csize = sizeof(certTemp)/sizeof(certTemp[0]);
    PRArenaPool *arena;
    char *nickname;
    CERTCertificate *cert;
    CK_RV crv;

    arena = PORT_NewArena( DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) return NULL;
    /*
     * grab the der encoding
     */
    crv = PK11_GetAttributes(arena,slot,certID,certTemp,csize);
    if (crv != CKR_OK) {
	PORT_FreeArena(arena,PR_FALSE);
	PORT_SetError( PK11_MapError(crv) );
	return NULL;
    }

    /*
     * build a certificate out of it
     */
    derCert.data = (unsigned char*)certDER->pValue;
    derCert.len = certDER->ulValueLen;

    /* figure out the nickname.... */
    nickname = pk11_buildNickname(slot,label,privateLabel,id);
    cert = CERT_DecodeDERCertificate(&derCert, PR_TRUE, nickname);
    if (cert) {
	cert->dbhandle = (CERTCertDBHandle *)
			nssToken_GetTrustDomain(slot->nssToken);
    }
	
    if (nickptr) {
	*nickptr = nickname;
    } else {
	if (nickname) PORT_Free(nickname);
    }
    PORT_FreeArena(arena,PR_FALSE);
    return cert;
}

CK_TRUST
pk11_GetTrustField(PK11SlotInfo *slot, PRArenaPool *arena, 
                   CK_OBJECT_HANDLE id, CK_ATTRIBUTE_TYPE type)
{
  CK_TRUST rv = 0;
  SECItem item;

  item.data = NULL;
  item.len = 0;

  if( SECSuccess == PK11_ReadAttribute(slot, id, type, arena, &item) ) {
    PORT_Assert(item.len == sizeof(CK_TRUST));
    PORT_Memcpy(&rv, item.data, sizeof(CK_TRUST));
    /* Damn, is there an endian problem here? */
    return rv;
  }

  return 0;
}

PRBool
pk11_HandleTrustObject(PK11SlotInfo *slot, CERTCertificate *cert, CERTCertTrust *trust)
{
  PRArenaPool *arena;

  CK_ATTRIBUTE tobjTemplate[] = {
    { CKA_CLASS, NULL, 0 },
    { CKA_CERT_SHA1_HASH, NULL, 0 },
  };

  CK_OBJECT_CLASS tobjc = CKO_NETSCAPE_TRUST;
  CK_OBJECT_HANDLE tobjID;
  unsigned char sha1_hash[SHA1_LENGTH];

  CK_TRUST serverAuth, codeSigning, emailProtection, clientAuth;

  PK11_HashBuf(SEC_OID_SHA1, sha1_hash, cert->derCert.data, cert->derCert.len);

  PK11_SETATTRS(&tobjTemplate[0], CKA_CLASS, &tobjc, sizeof(tobjc));
  PK11_SETATTRS(&tobjTemplate[1], CKA_CERT_SHA1_HASH, sha1_hash, 
                SHA1_LENGTH);

  tobjID = pk11_FindObjectByTemplate(slot, tobjTemplate, 
                                     sizeof(tobjTemplate)/sizeof(tobjTemplate[0]));
  if( CK_INVALID_HANDLE == tobjID ) {
    return PR_FALSE;
  }

  arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
  if( NULL == arena ) return PR_FALSE;

  /* Unfortunately, it seems that PK11_GetAttributes doesn't deal
   * well with nonexistant attributes.  I guess we have to check 
   * the trust info fields one at a time.
   */

  /* We could verify CKA_CERT_HASH here */

  /* We could verify CKA_EXPIRES here */


  /* "Purpose" trust information */
  serverAuth = pk11_GetTrustField(slot, arena, tobjID, CKA_TRUST_SERVER_AUTH);
  clientAuth = pk11_GetTrustField(slot, arena, tobjID, CKA_TRUST_CLIENT_AUTH);
  codeSigning = pk11_GetTrustField(slot, arena, tobjID, CKA_TRUST_CODE_SIGNING);
  emailProtection = pk11_GetTrustField(slot, arena, tobjID, 
						CKA_TRUST_EMAIL_PROTECTION);
  /* Here's where the fun logic happens.  We have to map back from the 
   * key usage, extended key usage, purpose, and possibly other trust values 
   * into the old trust-flags bits.  */

  /* First implementation: keep it simple for testing.  We can study what other
   * mappings would be appropriate and add them later.. fgmr 20000724 */

  if ( serverAuth ==  CKT_NETSCAPE_TRUSTED ) {
    trust->sslFlags |= CERTDB_VALID_PEER | CERTDB_TRUSTED;
  }

  if ( serverAuth == CKT_NETSCAPE_TRUSTED_DELEGATOR ) {
    trust->sslFlags |= CERTDB_VALID_CA | CERTDB_TRUSTED_CA | 
							CERTDB_NS_TRUSTED_CA;
  }
  if ( clientAuth == CKT_NETSCAPE_TRUSTED_DELEGATOR ) {
    trust->sslFlags |=  CERTDB_TRUSTED_CLIENT_CA ;
  }

  if ( emailProtection == CKT_NETSCAPE_TRUSTED ) {
    trust->emailFlags |= CERTDB_VALID_PEER | CERTDB_TRUSTED;
  }

  if ( emailProtection == CKT_NETSCAPE_TRUSTED_DELEGATOR ) {
    trust->emailFlags |= CERTDB_VALID_CA | CERTDB_TRUSTED_CA | CERTDB_NS_TRUSTED_CA;
  }

  if( codeSigning == CKT_NETSCAPE_TRUSTED ) {
    trust->objectSigningFlags |= CERTDB_VALID_PEER | CERTDB_TRUSTED;
  }

  if( codeSigning == CKT_NETSCAPE_TRUSTED_DELEGATOR ) {
    trust->objectSigningFlags |= CERTDB_VALID_CA | CERTDB_TRUSTED_CA | CERTDB_NS_TRUSTED_CA;
  }

  /* There's certainly a lot more logic that can go here.. */

  PORT_FreeArena(arena, PR_FALSE);

  return PR_TRUE;
}

/*
 * Build an CERTCertificate structure from a PKCS#11 object ID.... certID
 * Must be a CertObject. This code does not explicitly checks that.
 */
CERTCertificate *
PK11_MakeCertFromHandle(PK11SlotInfo *slot,CK_OBJECT_HANDLE certID,
						CK_ATTRIBUTE *privateLabel)
{
    char * nickname = NULL;
    CERTCertificate *cert = NULL;
    CERTCertTrust *trust;
    PRBool isFortezzaRootCA = PR_FALSE;
    PRBool swapNickname = PR_FALSE;

    cert = pk11_fastCert(slot,certID,privateLabel, &nickname);
    if (cert == NULL) goto loser;
	
    if (nickname) {
	if (cert->nickname != NULL) {
		cert->dbnickname = cert->nickname;
	} 
	cert->nickname = PORT_ArenaStrdup(cert->arena,nickname);
	PORT_Free(nickname);
	nickname = NULL;
	swapNickname = PR_TRUE;
    }

    /* remember where this cert came from.... If we have just looked
     * it up from the database and it already has a slot, don't add a new
     * one. */
    if (cert->slot == NULL) {
	cert->slot = PK11_ReferenceSlot(slot);
	cert->pkcs11ID = certID;
	cert->ownSlot = PR_TRUE;
    }

    trust = (CERTCertTrust*)PORT_ArenaAlloc(cert->arena, sizeof(CERTCertTrust));
    if (trust == NULL) goto loser;
    PORT_Memset(trust,0, sizeof(CERTCertTrust));
    cert->trust = trust;

    

    if(! pk11_HandleTrustObject(slot, cert, trust) ) {
	unsigned int type;

	/* build some cert trust flags */
	if (CERT_IsCACert(cert, &type)) {
	    unsigned int trustflags = CERTDB_VALID_CA;
	   
	    /* Allow PKCS #11 modules to give us trusted CA's. We only accept
	     * valid CA's which are self-signed here. They must have an object
	     * ID of '0'.  */ 
	    if (pk11_isID0(slot,certID) && 
		SECITEM_CompareItem(&cert->derSubject,&cert->derIssuer)
							   == SECEqual) {
		trustflags |= CERTDB_TRUSTED_CA;
		/* is the slot a fortezza card? allow the user or
		 * admin to turn on objectSigning, but don't turn
		 * full trust on explicitly */
		if (PK11_DoesMechanism(slot,CKM_KEA_KEY_DERIVE)) {
		    trust->objectSigningFlags |= CERTDB_VALID_CA;
		    isFortezzaRootCA = PR_TRUE;
		}
	    }
	    if ((type & NS_CERT_TYPE_SSL_CA) == NS_CERT_TYPE_SSL_CA) {
		trust->sslFlags |= trustflags;
	    }
	    if ((type & NS_CERT_TYPE_EMAIL_CA) == NS_CERT_TYPE_EMAIL_CA) {
		trust->emailFlags |= trustflags;
	    }
	    if ((type & NS_CERT_TYPE_OBJECT_SIGNING_CA) 
					== NS_CERT_TYPE_OBJECT_SIGNING_CA) {
		trust->objectSigningFlags |= trustflags;
	    }
	}
    }

    if (PK11_IsUserCert(slot,cert,certID)) {
	trust->sslFlags |= CERTDB_USER;
	trust->emailFlags |= CERTDB_USER;
	/*    trust->objectSigningFlags |= CERTDB_USER; */
    }

    return cert;

loser:
    if (nickname) PORT_Free(nickname);
    if (cert) CERT_DestroyCertificate(cert);
    return NULL;
}

	
/*
 * Build get a certificate from a private key
 */
CERTCertificate *
PK11_GetCertFromPrivateKey(SECKEYPrivateKey *privKey)
{
    PK11SlotInfo *slot = privKey->pkcs11Slot;
    CK_OBJECT_HANDLE handle = privKey->pkcs11ID;
    CK_OBJECT_HANDLE certID = 
		PK11_MatchItem(slot,handle,CKO_CERTIFICATE);
    CERTCertificate *cert;

    if (certID == CK_INVALID_HANDLE) {
	PORT_SetError(SSL_ERROR_NO_CERTIFICATE);
	return NULL;
    }
    cert = PK11_MakeCertFromHandle(slot,certID,NULL);
    return (cert);

}

/*
 * destroy a private key if there are no matching certs.
 * this function also frees the privKey structure.
 */
SECStatus
PK11_DeleteTokenPrivateKey(SECKEYPrivateKey *privKey, PRBool force)
{
    CERTCertificate *cert=PK11_GetCertFromPrivateKey(privKey);

    /* found a cert matching the private key?. */
    if (!force  && cert != NULL) {
	/* yes, don't delete the key */
        CERT_DestroyCertificate(cert);
	SECKEY_DestroyPrivateKey(privKey);
	return SECWouldBlock;
    }
    /* now, then it's safe for the key to go away */
    PK11_DestroyTokenObject(privKey->pkcs11Slot,privKey->pkcs11ID);
    SECKEY_DestroyPrivateKey(privKey);
    return SECSuccess;
}

/*
 * destroy a private key if there are no matching certs.
 * this function also frees the privKey structure.
 */
SECStatus
PK11_DeleteTokenPublicKey(SECKEYPublicKey *pubKey)
{
    /* now, then it's safe for the key to go away */
    if (pubKey->pkcs11Slot == NULL) {
	return SECFailure;
    }
    PK11_DestroyTokenObject(pubKey->pkcs11Slot,pubKey->pkcs11ID);
    SECKEY_DestroyPublicKey(pubKey);
    return SECSuccess;
}


/*
 * delete a cert and it's private key (if no other certs are pointing to the
 * private key.
 */
SECStatus
PK11_DeleteTokenCertAndKey(CERTCertificate *cert,void *wincx)
{
    SECKEYPrivateKey *privKey = PK11_FindKeyByAnyCert(cert,wincx);
    CK_OBJECT_HANDLE pubKey;
    PK11SlotInfo *slot = NULL;

    pubKey = pk11_FindPubKeyByAnyCert(cert, &slot, wincx);
    if (privKey) {
#ifdef NSS_CLASSIC
    	PK11_DestroyTokenObject(cert->slot,cert->pkcs11ID);
#else
	/* For 3.4, utilize the generic cert delete function */
	SEC_DeletePermCertificate(cert);
#endif
	PK11_DeleteTokenPrivateKey(privKey, PR_FALSE);
    }
    if ((pubKey != CK_INVALID_HANDLE) && (slot != NULL)) { 
    	PK11_DestroyTokenObject(slot,pubKey);
        PK11_FreeSlot(slot);
    }
    return SECSuccess;
}

/*
 * count the number of objects that match the template.
 */
int
PK11_NumberObjectsFor(PK11SlotInfo *slot, CK_ATTRIBUTE *findTemplate, 
							int templateCount)
{
    CK_OBJECT_HANDLE objID[PK11_SEARCH_CHUNKSIZE];
    int object_count = 0;
    CK_ULONG returned_count = 0;
    CK_RV crv;

    PK11_EnterSlotMonitor(slot);
    crv = PK11_GETTAB(slot)->C_FindObjectsInit(slot->session,
					findTemplate, templateCount);
    if (crv != CKR_OK) {
        PK11_ExitSlotMonitor(slot);
	PORT_SetError( PK11_MapError(crv) );
	return 0;
    }

    /*
     * collect all the Matching Objects
     */
    do {
    	crv = PK11_GETTAB(slot)->C_FindObjects(slot->session,
				objID,PK11_SEARCH_CHUNKSIZE,&returned_count);
	if (crv != CKR_OK) {
	    PORT_SetError( PK11_MapError(crv) );
	    break;
    	}
	object_count += returned_count;
    } while (returned_count == PK11_SEARCH_CHUNKSIZE);

    PK11_GETTAB(slot)->C_FindObjectsFinal(slot->session);
    PK11_ExitSlotMonitor(slot);
    return object_count;
}

/*
 * cert callback structure
 */
typedef struct pk11DoCertCallbackStr {
	SECStatus(* callback)(PK11SlotInfo *slot, CERTCertificate*, void *);
	SECStatus(* noslotcallback)(CERTCertificate*, void *);
	SECStatus(* itemcallback)(CERTCertificate*, SECItem *, void *);
	void *callbackArg;
} pk11DoCertCallback;

/*
 * callback to map object handles to certificate structures.
 */
static SECStatus
pk11_DoCerts(PK11SlotInfo *slot, CK_OBJECT_HANDLE certID, void *arg)
{
    CERTCertificate *cert;
    pk11DoCertCallback *certcb = (pk11DoCertCallback *) arg;

    cert = PK11_MakeCertFromHandle(slot, certID, NULL);

    if (cert == NULL) {
	return SECFailure;
    }

    if (certcb ) {
	if (certcb->callback) {
	    (*certcb->callback)(slot, cert, certcb->callbackArg);
	}
	if (certcb->noslotcallback) {
	    (*certcb->noslotcallback)(cert, certcb->callbackArg);
	}
	if (certcb->itemcallback) {
	    (*certcb->itemcallback)(cert, NULL, certcb->callbackArg);
	}
    }

    CERT_DestroyCertificate(cert);
	    
    return SECSuccess;
}

static SECStatus
pk11_CollectCrls(PK11SlotInfo *slot, CK_OBJECT_HANDLE crlID, void *arg)
{
    SECItem derCrl;
    CERTCrlHeadNode *head = (CERTCrlHeadNode *) arg;
    CERTCrlNode *new_node = NULL;
    CK_ATTRIBUTE fetchCrl[3] = {
	 { CKA_VALUE, NULL, 0},
	 { CKA_NETSCAPE_KRL, NULL, 0},
	 { CKA_NETSCAPE_URL, NULL, 0},
    };
    const int fetchCrlSize = sizeof(fetchCrl)/sizeof(fetchCrl[2]);
    SECStatus rv;

    rv = PK11_GetAttributes(head->arena,slot,crlID,fetchCrl,fetchCrlSize);
    if (rv == SECFailure) {
	goto loser;
    }
    rv = SECFailure;

    new_node = (CERTCrlNode *)PORT_ArenaAlloc(head->arena, sizeof(CERTCrlNode));
    if (new_node == NULL) {
        goto loser;
    }

    new_node->type =  *((CK_BBOOL *)fetchCrl[1].pValue)  ? 
						SEC_KRL_TYPE : SEC_CRL_TYPE;
    derCrl.data = (unsigned char *)fetchCrl[0].pValue;
    derCrl.len = fetchCrl[0].ulValueLen;
    new_node->crl=CERT_DecodeDERCrl(head->arena,&derCrl,new_node->type);

    if (fetchCrl[2].pValue) {
        int nnlen = fetchCrl[2].ulValueLen;
        new_node->crl->url  = (char *)PORT_ArenaAlloc(head->arena, nnlen+1);
        if ( !new_node->crl->url ) {
            goto loser;
        }
        PORT_Memcpy(new_node->crl->url, fetchCrl[2].pValue, nnlen);
        new_node->crl->url[nnlen] = 0;
    } else {
        new_node->crl->url = NULL;
    }


    new_node->next = NULL;
    if (head->last) {
        head->last->next = new_node;
        head->last = new_node;
    } else {
        head->first = head->last = new_node;
    }
    rv = SECSuccess;

loser:
    return(rv);
}


/*
 * key call back structure.
 */
typedef struct pk11KeyCallbackStr {
	SECStatus (* callback)(SECKEYPrivateKey *,void *);
	void *callbackArg;
	void *wincx;
} pk11KeyCallback;

/*
 * callback to map Object Handles to Private Keys;
 */
SECStatus
pk11_DoKeys(PK11SlotInfo *slot, CK_OBJECT_HANDLE keyHandle, void *arg)
{
    SECStatus rv = SECSuccess;
    SECKEYPrivateKey *privKey;
    pk11KeyCallback *keycb = (pk11KeyCallback *) arg;

    privKey = PK11_MakePrivKey(slot,nullKey,PR_TRUE,keyHandle,keycb->wincx);

    if (privKey == NULL) {
	return SECFailure;
    }

    if (keycb && (keycb->callback)) {
	rv = (*keycb->callback)(privKey,keycb->callbackArg);
    }

    SECKEY_DestroyPrivateKey(privKey);	    
    return rv;
}

/* Traverse slots callback */
typedef struct pk11TraverseSlotStr {
    SECStatus (*callback)(PK11SlotInfo *,CK_OBJECT_HANDLE, void *);
    void *callbackArg;
    CK_ATTRIBUTE *findTemplate;
    int templateCount;
} pk11TraverseSlot;

/*
 * Extract all the certs on a card from a slot.
 */
SECStatus
PK11_TraverseSlot(PK11SlotInfo *slot, void *arg)
{
    int i;
    CK_OBJECT_HANDLE *objID = NULL;
    int object_count = 0;
    pk11TraverseSlot *slotcb = (pk11TraverseSlot*) arg;

    objID = pk11_FindObjectsByTemplate(slot,slotcb->findTemplate,
		slotcb->templateCount,&object_count);

    /*Actually this isn't a failure... there just were no objs to be found*/
    if (object_count == 0) {
	return SECSuccess;
    }

    if (objID == NULL) {
	return SECFailure;
    }

    for (i=0; i < object_count; i++) {
	(*slotcb->callback)(slot,objID[i],slotcb->callbackArg);
    }
    PORT_Free(objID);
    return SECSuccess;
}

typedef struct pk11CertCallbackStr {
	SECStatus(* callback)(CERTCertificate*,SECItem *,void *);
	void *callbackArg;
} pk11CertCallback;

/*
 * Extract all the certs on a card from a slot.
 */
static SECStatus
pk11_TraverseAllSlots( SECStatus (*callback)(PK11SlotInfo *,void *),
						void *arg,void *wincx) {
    PK11SlotList *list;
    PK11SlotListElement *le;
    SECStatus rv;

    /* get them all! */
    list = PK11_GetAllTokens(CKM_INVALID_MECHANISM,PR_FALSE,PR_FALSE,wincx);
    if (list == NULL) return SECFailure;

    /* look at each slot and authenticate as necessary */
    for (le = list->head ; le; le = le->next) {
	if (!PK11_IsFriendly(le->slot)) {
             rv = PK11_Authenticate(le->slot, PR_FALSE, wincx);
             if (rv != SECSuccess) continue;
	}
	(*callback)(le->slot,arg);
    }

    PK11_FreeSlotList(list);

    return SECSuccess;
}

struct fake_der_cb_argstr
{
    SECStatus(* callback)(CERTCertificate*, SECItem *, void *);
    void *arg;
};

static SECStatus fake_der_cb(CERTCertificate *c, void *a)
{
    struct fake_der_cb_argstr *fda = (struct fake_der_cb_argstr *)a;
    return (*fda->callback)(c, &c->derCert, fda->arg);
}

/*
 * Extract all the certs on a card from a slot.
 */
SECStatus
PK11_TraverseSlotCerts(SECStatus(* callback)(CERTCertificate*,SECItem *,void *),
						void *arg, void *wincx) {
#ifdef NSS_CLASSIC
    pk11DoCertCallback caller;
    pk11TraverseSlot creater;
    CK_ATTRIBUTE theTemplate;
    CK_OBJECT_CLASS certClass = CKO_CERTIFICATE;

    PK11_SETATTRS(&theTemplate, CKA_CLASS, &certClass, sizeof(certClass));

    caller.callback = NULL;
    caller.noslotcallback = NULL;
    caller.itemcallback = callback;
    caller.callbackArg = arg;
    creater.callback = pk11_DoCerts;
    creater.callbackArg = (void *) & caller;
    creater.findTemplate = &theTemplate;
    creater.templateCount = 1;

    return pk11_TraverseAllSlots(PK11_TraverseSlot, &creater, wincx);
#else
    NSSTrustDomain *defaultTD = STAN_GetDefaultTrustDomain();
    struct fake_der_cb_argstr fda;
    struct nss3_cert_cbstr pk11cb;
    fda.callback = callback;
    fda.arg = arg;
    pk11cb.callback = fake_der_cb;
    pk11cb.arg = &fda;
    NSSTrustDomain_TraverseCertificates(defaultTD, convert_cert, &pk11cb);
    return SECSuccess;
#endif
}

/*
 * Extract all the certs on a card from a slot.
 */
SECStatus
PK11_LookupCrls(CERTCrlHeadNode *nodes, int type, void *wincx) {
    pk11TraverseSlot creater;
    CK_ATTRIBUTE theTemplate[2];
    CK_ATTRIBUTE *attrs;
    CK_OBJECT_CLASS certClass = CKO_NETSCAPE_CRL;

    attrs = theTemplate;
    PK11_SETATTRS(attrs, CKA_CLASS, &certClass, sizeof(certClass)); attrs++;
    if (type != -1) {
	CK_BBOOL isKrl = (CK_BBOOL) (type == SEC_KRL_TYPE);
        PK11_SETATTRS(attrs, CKA_NETSCAPE_KRL, &isKrl, sizeof(isKrl)); attrs++;
    }

    creater.callback = pk11_CollectCrls;
    creater.callbackArg = (void *) nodes;
    creater.findTemplate = theTemplate;
    creater.templateCount = (attrs - theTemplate);

    return pk11_TraverseAllSlots(PK11_TraverseSlot, &creater, wincx);
}

/***********************************************************************
 * PK11_TraversePrivateKeysInSlot
 *
 * Traverses all the private keys on a slot.
 *
 * INPUTS
 *      slot
 *          The PKCS #11 slot whose private keys you want to traverse.
 *      callback
 *          A callback function that will be called for each key.
 *      arg
 *          An argument that will be passed to the callback function.
 */
SECStatus
PK11_TraversePrivateKeysInSlot( PK11SlotInfo *slot,
    SECStatus(* callback)(SECKEYPrivateKey*, void*), void *arg)
{
    pk11KeyCallback perKeyCB;
    pk11TraverseSlot perObjectCB;
    CK_OBJECT_CLASS privkClass = CKO_PRIVATE_KEY;
    CK_BBOOL ckTrue = CK_TRUE;
    CK_ATTRIBUTE theTemplate[2];
    int templateSize = 2;

    theTemplate[0].type = CKA_CLASS;
    theTemplate[0].pValue = &privkClass;
    theTemplate[0].ulValueLen = sizeof(privkClass);
    theTemplate[1].type = CKA_TOKEN;
    theTemplate[1].pValue = &ckTrue;
    theTemplate[1].ulValueLen = sizeof(ckTrue);

    if(slot==NULL) {
        return SECSuccess;
    }

    perObjectCB.callback = pk11_DoKeys;
    perObjectCB.callbackArg = &perKeyCB;
    perObjectCB.findTemplate = theTemplate;
    perObjectCB.templateCount = templateSize;
    perKeyCB.callback = callback;
    perKeyCB.callbackArg = arg;
    perKeyCB.wincx = NULL;

    return PK11_TraverseSlot(slot, &perObjectCB);
}

CK_OBJECT_HANDLE *
PK11_FindObjectsFromNickname(char *nickname,PK11SlotInfo **slotptr,
		CK_OBJECT_CLASS objclass, int *returnCount, void *wincx)
{
    char *tokenName;
    char *delimit;
    PK11SlotInfo *slot;
    CK_OBJECT_HANDLE *objID;
    CK_ATTRIBUTE findTemplate[] = {
	 { CKA_LABEL, NULL, 0},
	 { CKA_CLASS, NULL, 0},
    };
    int findCount = sizeof(findTemplate)/sizeof(findTemplate[0]);
    SECStatus rv;
    PK11_SETATTRS(&findTemplate[1], CKA_CLASS, &objclass, sizeof(objclass));

    *slotptr = slot = NULL;
    *returnCount = 0;
    /* first find the slot associated with this nickname */
    if ((delimit = PORT_Strchr(nickname,':')) != NULL) {
	int len = delimit - nickname;
	tokenName = (char*)PORT_Alloc(len+1);
	PORT_Memcpy(tokenName,nickname,len);
	tokenName[len] = 0;

        slot = *slotptr = PK11_FindSlotByName(tokenName);
        PORT_Free(tokenName);
	/* if we couldn't find a slot, assume the nickname is an internal cert
	 * with no proceding slot name */
	if (slot == NULL) {
		slot = *slotptr = PK11_GetInternalKeySlot();
	} else {
		nickname = delimit+1;
	}
    } else {
	*slotptr = slot = PK11_GetInternalKeySlot();
    }
    if (slot == NULL) {
        return CK_INVALID_HANDLE;
    }

    if (!PK11_IsFriendly(slot)) {
	rv = PK11_Authenticate(slot, PR_TRUE, wincx);
	if (rv != SECSuccess) {
	    PK11_FreeSlot(slot);
	    *slotptr = NULL;
	    return CK_INVALID_HANDLE;
	 }
    }

    findTemplate[0].pValue = nickname;
    findTemplate[0].ulValueLen = PORT_Strlen(nickname);
    objID = pk11_FindObjectsByTemplate(slot,findTemplate,findCount,returnCount);
    if (objID == NULL) {
	/* PKCS #11 isn't clear on whether or not the NULL is
	 * stored in the template.... try the find again with the
	 * full null terminated string. */
    	findTemplate[0].ulValueLen += 1;
        objID = pk11_FindObjectsByTemplate(slot,findTemplate,findCount,
								returnCount);
	if (objID == NULL) {
	    /* Well that's the best we can do. It's just not here */
	    /* what about faked nicknames? */
	    PK11_FreeSlot(slot);
	    *slotptr = NULL;
	    *returnCount = 0;
	}
    }

    return objID;
}

static PRStatus 
get_newest_cert(NSSCertificate *c, void *arg)
{
    nssDecodedCert *dc, *founddc;
    NSSCertificate **cfound = (NSSCertificate **)arg;
    if (!*cfound) {
	*cfound = nssCertificate_AddRef(c);
	return PR_SUCCESS;
    }
    dc = nssCertificate_GetDecoding(c);
    founddc = nssCertificate_GetDecoding(*cfound);
    if (!founddc->isNewerThan(founddc, dc)) {
	CERT_DestroyCertificate(STAN_GetCERTCertificate(*cfound));
	*cfound = nssCertificate_AddRef(c);
    }
    return PR_SUCCESS;
}

struct token_cbstr {
    NSSToken *token;
    PRStatus (* callback)(NSSCertificate *c, void *arg);
    void *cbarg;
};

/* This callback matches all certs on a given token.  It is used to filter
 * cert lists to only those certs on a particular token.
 */
static PRStatus 
token_callback(NSSCertificate *c, void *arg)
{
    nssListIterator *instances;
    nssCryptokiInstance *instance;
    PRBool isToken = PR_FALSE;
    struct token_cbstr *token_cb = (struct token_cbstr *)arg;
    instances = c->object.instances;
    for (instance  = (nssCryptokiInstance *)nssListIterator_Start(instances);
         instance != (nssCryptokiInstance *)NULL;
         instance  = (nssCryptokiInstance *)nssListIterator_Next(instances))
    {
	if (instance->token == token_cb->token) {
	    isToken = PR_TRUE;
	    break;
	}
    }
    nssListIterator_Finish(instances);
    if (isToken) {
	return (*token_cb->callback)(c, token_cb->cbarg);
    } else {
	return PR_SUCCESS;
    }
}

/* match all token certs with a nickname */
static nssList *
filter_token_certs_nickname(NSSToken *token, NSSUTF8 *nickname)
{
    nssListIterator *certs;
    NSSCertificate *cert;
    PRStatus nssrv;
    nssList *rvList;
    certs = nssList_CreateIterator(token->certList);
    if (!certs) return NULL;
    rvList = nssList_Create(NULL, PR_FALSE);
    if (!rvList) {
	nssListIterator_Destroy(certs);
	return NULL;
    }
    for (cert  = (NSSCertificate *)nssListIterator_Start(certs);
         cert != (NSSCertificate *)NULL;
         cert  = (NSSCertificate *)nssListIterator_Next(certs))
    {
	NSSUTF8 *tokenNick = NSSCertificate_GetNickname(cert, token);
	if (!tokenNick) continue;
	if (nssUTF8_Equal(tokenNick, nickname, &nssrv)) {
	    nssList_Add(rvList, nssCertificate_AddRef(cert));
	}
    }
    nssListIterator_Finish(certs);
    nssListIterator_Destroy(certs);
    if (nssList_Count(rvList) == 0) {
	nssList_Destroy(rvList);
	rvList = NULL;
    }
    return rvList;
}

/* match all token certs with an email address */
static nssList *
filter_token_certs_email(NSSToken *token, NSSASCII7 *email)
{
    nssListIterator *certs;
    NSSCertificate *cert;
    PRStatus nssrv;
    nssList *rvList;
    certs = nssList_CreateIterator(token->certList);
    if (!certs) return NULL;
    rvList = nssList_Create(NULL, PR_FALSE);
    if (!rvList) {
	nssListIterator_Destroy(certs);
	return NULL;
    }
    for (cert  = (NSSCertificate *)nssListIterator_Start(certs);
         cert != (NSSCertificate *)NULL;
         cert  = (NSSCertificate *)nssListIterator_Next(certs))
    {
	if (!cert->email) continue;
	if (nssUTF8_Equal(cert->email, email, &nssrv)) {
	    nssList_Add(rvList, nssCertificate_AddRef(cert));
	}
    }
    nssListIterator_Finish(certs);
    nssListIterator_Destroy(certs);
    if (nssList_Count(rvList) == 0) {
	nssList_Destroy(rvList);
	rvList = NULL;
    }
    return rvList;
}

CERTCertificate *
PK11_FindCertFromNickname(char *nickname, void *wincx) {
#ifdef NSS_CLASSIC
    PK11SlotInfo *slot;
    int count=0;
    CK_OBJECT_HANDLE *certID = PK11_FindObjectsFromNickname(nickname,&slot,
				 		CKO_CERTIFICATE, &count, wincx);
    CERTCertificate *cert;

    if (certID == CK_INVALID_HANDLE) return NULL;
    cert = PK11_MakeCertFromHandle(slot,certID[0],NULL);
    PK11_FreeSlot(slot);
    PORT_Free(certID);
    return cert;
#else
    CERTCertificate *rvCert = NULL;
    NSSCertificate *cert = NULL;
    NSSUsage usage;
    NSSToken *token;
    PK11SlotInfo *slot = NULL;
    char *nickCopy;
    char *delimit = NULL;
    char *tokenName;
    NSSTrustDomain *defaultTD = STAN_GetDefaultTrustDomain();
    usage.anyUsage = PR_TRUE;
    nickCopy = PORT_Strdup(nickname);
    if ((delimit = PORT_Strchr(nickCopy,':')) != NULL) {
	tokenName = nickCopy;
	nickname = delimit + 1;
	*delimit = '\0';
	/* find token by name */
	token = NSSTrustDomain_FindTokenByName(defaultTD, (NSSUTF8 *)tokenName);
	if (token) {
		slot = PK11_ReferenceSlot(token->pk11slot);
	}
	*delimit = ':';
    } else {
	slot = PK11_GetInternalKeySlot();
	token = PK11Slot_GetNSSToken(slot);
    }
    if (token) {
	nssTokenCertSearch search;
	struct token_cbstr token_cb;
	nssList *certList;

	if (!PK11_IsPresent(slot)) {
		return NULL;
	}
	if (!PK11_IsFriendly(slot)) {
	    if (PK11_Authenticate(slot, PR_TRUE, wincx) != SECSuccess) {
		PK11_FreeSlot(slot);
		return NULL;
	    }
	}
	/* find best cert on token */
	if (!nssToken_SearchCerts(token, NULL)) {
	    /* token certs are in cache, filter the list of token certs to
	     * match the nickname
	     */
	    certList = filter_token_certs_nickname(token, nickname);
	    if (certList) {
		nssCertificateList_DoCallback(certList, 
		                              get_newest_cert, 
		                              (void *)&cert);
	    }
	} else {
	    /* find matching certs on the token */
	    certList = nssList_Create(NULL, PR_FALSE);
	    if (!certList) return NULL;
	    /* first, get all matching certs from the cache */
	    (void)nssTrustDomain_GetCertsForNicknameFromCache(defaultTD, 
	                                                      nickname, 
	                                                      certList);
	    /* set the search criteria */
	    token_cb.callback = get_newest_cert;
	    token_cb.cbarg = (void *)&cert;
	    token_cb.token = token;
	    search.callback = token_callback;
	    search.cbarg = &token_cb;
	    search.cached = certList;
	    search.searchType = nssTokenSearchType_TokenOnly;
	    /* now search the token */
	    nssToken_TraverseCertificatesByNickname(token, NULL, 
	                                            (NSSUTF8 *)nickname,
	                                            &search);
	    /* filter the list of cached certs for only those on the token */
	    nssCertificateList_DoCallback(certList, 
	                                  token_callback, 
	                                  &token_cb);
	}
	if (certList) {
	    nssList_Clear(certList, cert_destructor);
	    nssList_Destroy(certList);
	}
	/* if it wasn't found, repeat the process for email address */
	if (!cert) {
	    if (!nssToken_SearchCerts(token, NULL)) {
		certList = filter_token_certs_email(token, nickname);
		if (certList) {
		    nssCertificateList_DoCallback(certList, 
		                                  get_newest_cert, 
		                                  (void *)&cert);
		}
	    } else {
		certList = nssList_Create(NULL, PR_FALSE);
		if (!certList) return NULL;
		(void)nssTrustDomain_GetCertsForEmailAddressFromCache(
		                                                  defaultTD, 
	                                                          nickname, 
	                                                          certList);
		search.cached = certList;
		nssToken_TraverseCertificatesByEmail(token, NULL, 
		                                     (NSSASCII7 *)nickname,
		                                     &search);
		nssCertificateList_DoCallback(certList, 
		                              token_callback,
		                              &token_cb);
	    }
	    if (certList) {
		nssList_Clear(certList, cert_destructor);
		nssList_Destroy(certList);
	    }
	}
	if (cert) {
	    (void)nssTrustDomain_AddCertsToCache(defaultTD, &cert, 1);
	    rvCert = STAN_GetCERTCertificate(cert);
	}
    }
    if (slot) {
	PK11_FreeSlot(slot);
    }
    if (nickCopy) PORT_Free(nickCopy);
    return rvCert;
#endif
}

static PRStatus 
collect_certs(NSSCertificate *c, void *arg)
{
    nssList *list = (nssList *)arg;
    /* Add the cert to the return list if not present */
    if (!nssList_Get(list, (void *)c)) {
	nssCertificate_AddRef(c);
	nssList_Add(list, (void *)c);
    }
    return PR_SUCCESS;
}

CERTCertList *
PK11_FindCertsFromNickname(char *nickname, void *wincx) {
#ifdef NSS_CLASSIC
    PK11SlotInfo *slot;
    int i,count = 0;
    CK_OBJECT_HANDLE *certID = PK11_FindObjectsFromNickname(nickname,&slot,
				 		CKO_CERTIFICATE, &count, wincx);
    CERTCertList *certList = NULL;

    if (certID == NULL) return NULL;

    certList= CERT_NewCertList();

    for (i=0; i < count; i++) {
    	CERTCertificate *cert = PK11_MakeCertFromHandle(slot,certID[i],NULL);

	if (cert) CERT_AddCertToListTail(certList,cert);
    }

    if (CERT_LIST_HEAD(certList) == NULL) {
	CERT_DestroyCertList(certList);
	certList = NULL;
    }
    PK11_FreeSlot(slot);
    PORT_Free(certID);
    return certList;
#else
    PRStatus nssrv;
    char *nickCopy;
    char *delimit = NULL;
    char *tokenName;
    int i;
    CERTCertList *certList = NULL;
    NSSCertificate **foundCerts = NULL;
    NSSTrustDomain *defaultTD = STAN_GetDefaultTrustDomain();
    NSSCertificate *c;
    NSSToken *token;
    PK11SlotInfo *slot;
    nickCopy = PORT_Strdup(nickname);
    if ((delimit = PORT_Strchr(nickCopy,':')) != NULL) {
	tokenName = nickCopy;
	nickname = delimit + 1;
	*delimit = '\0';
	/* find token by name */
	token = NSSTrustDomain_FindTokenByName(defaultTD, (NSSUTF8 *)tokenName);
	if (token) {
	    slot = PK11_ReferenceSlot(token->pk11slot);
	} else {
	    slot = NULL;
	}
	*delimit = ':';
    } else {
	slot = PK11_GetInternalKeySlot();
	token = PK11Slot_GetNSSToken(slot);
    }
    if (token) {
	nssTokenCertSearch search;
	PRUint32 count;
	nssList *nameList;
	if (!PK11_IsFriendly(slot)) {
	    if (PK11_Authenticate(slot, PR_TRUE, wincx) != SECSuccess) {
		PK11_FreeSlot(slot);
		return NULL;
	    }
	}
	if (!nssToken_SearchCerts(token, NULL)) {
	    nameList = filter_token_certs_nickname(token, nickname);
	} else {
	    nameList = nssList_Create(NULL, PR_FALSE);
	    if (!nameList) return NULL;
	    (void)nssTrustDomain_GetCertsForNicknameFromCache(defaultTD,
	                                                      nickname, 
	                                                      nameList);
	    /* set the search criteria */
	    search.callback = collect_certs;
	    search.cbarg = nameList;
	    search.cached = nameList;
	    search.searchType = nssTokenSearchType_TokenOnly;
	    nssrv = nssToken_TraverseCertificatesByNickname(token, NULL, 
	                                                    nickname, &search);
	}
	if (nameList) {
	    count = nssList_Count(nameList);
	    foundCerts = nss_ZNEWARRAY(NULL, NSSCertificate *, count + 1);
	    nssList_GetArray(nameList, (void **)foundCerts, count);
	    nssList_Destroy(nameList);
	}
    }
    if (slot) {
	PK11_FreeSlot(slot);
    }
    if (nickCopy) PORT_Free(nickCopy);
    if (foundCerts) {
	certList = CERT_NewCertList();
	for (i=0, c = *foundCerts; c; c = foundCerts[++i]) {
	    CERT_AddCertToListTail(certList, STAN_GetCERTCertificate(c));
	}
	if (CERT_LIST_HEAD(certList) == NULL) {
	    CERT_DestroyCertList(certList);
	    certList = NULL;
	}
	nss_ZFreeIf(foundCerts);
    }
    return certList;
#endif
}

/*
 * extract a key ID for a certificate...
 * NOTE: We call this function from PKCS11.c If we ever use
 * pkcs11 to extract the public key (we currently do not), this will break.
 */
SECItem *
PK11_GetPubIndexKeyID(CERTCertificate *cert) {
    SECKEYPublicKey *pubk;
    SECItem *newItem = NULL;

    pubk = CERT_ExtractPublicKey(cert);
    if (pubk == NULL) return NULL;

    switch (pubk->keyType) {
    case rsaKey:
	newItem = SECITEM_DupItem(&pubk->u.rsa.modulus);
	break;
    case dsaKey:
        newItem = SECITEM_DupItem(&pubk->u.dsa.publicValue);
	break;
    case dhKey:
        newItem = SECITEM_DupItem(&pubk->u.dh.publicValue);
    case fortezzaKey:
    default:
	newItem = NULL; /* Fortezza Fix later... */
    }
    SECKEY_DestroyPublicKey(pubk);
    /* make hash of it */
    return newItem;
}

/*
 * generate a CKA_ID from a certificate.
 */
SECItem *
pk11_mkcertKeyID(CERTCertificate *cert) {
    SECItem *pubKeyData = PK11_GetPubIndexKeyID(cert) ;
    SECItem *certCKA_ID;

    if (pubKeyData == NULL) return NULL;
    
    certCKA_ID = PK11_MakeIDFromPubKey(pubKeyData);
    SECITEM_FreeItem(pubKeyData,PR_TRUE);
    return certCKA_ID;
}


/*
 * Generate a CKA_ID from the relevant public key data. The CKA_ID is generated
 * from the pubKeyData by SHA1_Hashing it to produce a smaller CKA_ID (to make
 * smart cards happy.
 */
SECItem *
PK11_MakeIDFromPubKey(SECItem *pubKeyData) {
    PK11Context *context;
    SECItem *certCKA_ID;
    SECStatus rv;

    context = PK11_CreateDigestContext(SEC_OID_SHA1);
    if (context == NULL) {
	return NULL;
    }

    rv = PK11_DigestBegin(context);
    if (rv == SECSuccess) {
    	rv = PK11_DigestOp(context,pubKeyData->data,pubKeyData->len);
    }
    if (rv != SECSuccess) {
	PK11_DestroyContext(context,PR_TRUE);
	return NULL;
    }

    certCKA_ID = (SECItem *)PORT_Alloc(sizeof(SECItem));
    if (certCKA_ID == NULL) {
	PK11_DestroyContext(context,PR_TRUE);
	return NULL;
    }

    certCKA_ID->len = SHA1_LENGTH;
    certCKA_ID->data = (unsigned char*)PORT_Alloc(certCKA_ID->len);
    if (certCKA_ID->data == NULL) {
	PORT_Free(certCKA_ID);
	PK11_DestroyContext(context,PR_TRUE);
        return NULL;
    }

    rv = PK11_DigestFinal(context,certCKA_ID->data,&certCKA_ID->len,
								SHA1_LENGTH);
    PK11_DestroyContext(context,PR_TRUE);
    if (rv != SECSuccess) {
    	SECITEM_FreeItem(certCKA_ID,PR_TRUE);
	return NULL;
    }

    return certCKA_ID;
}

/*
 * Write the cert into the token.
 */
SECStatus
PK11_ImportCert(PK11SlotInfo *slot, CERTCertificate *cert, 
		CK_OBJECT_HANDLE key, char *nickname, PRBool includeTrust) {
    int len = 0;
    SECItem *keyID = pk11_mkcertKeyID(cert);
    CK_ATTRIBUTE keyAttrs[] = {
	{ CKA_LABEL, NULL, 0},
	{ CKA_SUBJECT, NULL, 0},
    };
    CK_OBJECT_CLASS certc = CKO_CERTIFICATE;
    CK_CERTIFICATE_TYPE certType = CKC_X_509;
    CK_OBJECT_HANDLE certID;
    CK_SESSION_HANDLE rwsession;
    CK_BBOOL cktrue = CK_TRUE;
    SECStatus rv = SECFailure;
    CK_ATTRIBUTE certAttrs[] = {
	{ CKA_ID, NULL, 0 },
	{ CKA_LABEL, NULL, 0},
	{ CKA_CLASS,  NULL, 0},
	{ CKA_TOKEN,  NULL, 0},
	{ CKA_CERTIFICATE_TYPE, NULL, 0},
	{ CKA_SUBJECT, NULL, 0},
	{ CKA_ISSUER, NULL, 0},
	{ CKA_SERIAL_NUMBER,  NULL, 0},
	{ CKA_VALUE,  NULL, 0},
	{ CKA_NETSCAPE_TRUST,  NULL, 0},
    };
    int certCount = sizeof(certAttrs)/sizeof(certAttrs[0]), keyCount = 2;
    int realCount = 0;
    CK_ATTRIBUTE *attrs;
    CK_RV crv;
    SECCertUsage *certUsage = NULL;
    SECItem derSerial = { 0 };
    NSSToken *token;

    if (keyID == NULL) {
	PORT_SetError(SEC_ERROR_ADDING_CERT);
	return rv;
    }

    len = ((nickname) ? PORT_Strlen(nickname) : 0);
    
    attrs = certAttrs;
    PK11_SETATTRS(attrs,CKA_ID, keyID->data, keyID->len); attrs++;
    if (nickname) {
	PK11_SETATTRS(attrs,CKA_LABEL, nickname, len ); attrs++;
    }
    PK11_SETATTRS(attrs,CKA_CLASS, &certc, sizeof(certc) ); attrs++;
    PK11_SETATTRS(attrs,CKA_TOKEN, &cktrue, sizeof(cktrue) ); attrs++;
    PK11_SETATTRS(attrs,CKA_CERTIFICATE_TYPE, &certType,
						sizeof(certType)); attrs++;
    PK11_SETATTRS(attrs,CKA_SUBJECT, cert->derSubject.data,
					 cert->derSubject.len ); attrs++;
    PK11_SETATTRS(attrs,CKA_ISSUER, cert->derIssuer.data,
					 cert->derIssuer.len ); attrs++;
    if (PR_TRUE) {
	/* CERTCertificate stores serial numbers decoded.  I need the DER
	* here.  sigh.
	*/
	CERT_SerialNumberFromDERCert(&cert->derCert, &derSerial);
	PK11_SETATTRS(attrs,CKA_SERIAL_NUMBER, derSerial.data, derSerial.len); 
	attrs++;
    }
    PK11_SETATTRS(attrs,CKA_VALUE, cert->derCert.data, 
						cert->derCert.len); attrs++;
    if (includeTrust && PK11_IsInternal(slot)) {
	certUsage = (SECCertUsage*)PORT_Alloc(sizeof(SECCertUsage));
	if(!certUsage) {
	    SECITEM_FreeItem(keyID,PR_TRUE);
	    PORT_SetError(SEC_ERROR_NO_MEMORY);
	    return rv;
	}
	*certUsage = certUsageUserCertImport;
	PK11_SETATTRS(attrs,CKA_NETSCAPE_TRUST, certUsage,
							 sizeof(SECCertUsage));
	attrs++;
    }
    realCount = attrs - certAttrs;
    PORT_Assert(realCount <= certCount);

    attrs = keyAttrs;
    if(nickname) {
	PK11_SETATTRS(attrs,CKA_LABEL, nickname, len ); attrs++;
    }
    PK11_SETATTRS(attrs,CKA_SUBJECT, cert->derSubject.data,
					 cert->derSubject.len );

    if(!nickname) {
	certCount--;
	keyCount--;
    }

    rwsession = PK11_GetRWSession(slot);
    if (key != CK_INVALID_HANDLE) {
	crv = PK11_GETTAB(slot)->C_SetAttributeValue(rwsession,key,keyAttrs,
								keyCount);
	if (crv != CKR_OK) {
	    PORT_SetError( PK11_MapError(crv) );
	    goto done;
	}
    }

    crv = PK11_GETTAB(slot)->
			C_CreateObject(rwsession,certAttrs,realCount,&certID);
    if (crv == CKR_OK) {
	rv = SECSuccess;
    } else {
	PORT_SetError( PK11_MapError(crv) );
    }

    if (!cert->nickname && nickname) {
	cert->nickname = PORT_ArenaStrdup(cert->arena, nickname);
    }

    cert->pkcs11ID = certID;
    cert->dbhandle = STAN_GetDefaultTrustDomain();
    if (cert->slot == NULL) {
	cert->slot = PK11_ReferenceSlot(slot);
	cert->ownSlot = PR_TRUE;
	if (cert->nssCertificate) {
	    nssCryptokiInstance *instance;
	    NSSCertificate *c = cert->nssCertificate;
	    instance = nss_ZNEW(c->object.arena, nssCryptokiInstance);
	    instance->token = slot->nssToken;
	    instance->handle = cert->pkcs11ID;
	    instance->isTokenObject = PR_TRUE;
	    nssList_Add(c->object.instanceList, instance);
	    /* XXX Fix this! */
	    nssListIterator_Destroy(c->object.instances);
	    c->object.instances = nssList_CreateIterator(c->object.instanceList);
	} else {
	    cert->nssCertificate = STAN_GetNSSCertificate(cert);
	}
    }
    cert->trust = nssTrust_GetCERTCertTrustForCert(cert->nssCertificate, cert);
    token = PK11Slot_GetNSSToken(slot);
    if (token->certList) {
	/* create a persistent reference for the token */
	NSSTrustDomain *td = STAN_GetDefaultTrustDomain();
	nssCertificate_AddRef(cert->nssCertificate);
	nssList_Add(token->certList, cert->nssCertificate);
	/* and add the cert to the cache */
	nssTrustDomain_AddCertsToCache(td, &cert->nssCertificate, 1);
    }

done:
    if (derSerial.data) PORT_Free(derSerial.data);
    SECITEM_FreeItem(keyID,PR_TRUE);
    PK11_RestoreROSession(slot,rwsession);
    if(certUsage) {
	PORT_Free(certUsage);
    }
    return rv;

}

/*
 * get a certificate handle, look at the cached handle first..
 */
CK_OBJECT_HANDLE
pk11_getcerthandle(PK11SlotInfo *slot, CERTCertificate *cert, 
					CK_ATTRIBUTE *theTemplate,int tsize)
{
    CK_OBJECT_HANDLE certh;

    if (cert->slot == slot) {
	certh = cert->pkcs11ID;
	if (certh == CK_INVALID_HANDLE) {
    	     certh = pk11_FindObjectByTemplate(slot,theTemplate,tsize);
	     cert->pkcs11ID = certh;
	}
    } else {
    	certh = pk11_FindObjectByTemplate(slot,theTemplate,tsize);
    }
    return certh;
}

/*
 * return the private key From a given Cert
 */
SECKEYPrivateKey *
PK11_FindPrivateKeyFromCert(PK11SlotInfo *slot, CERTCertificate *cert,
								 void *wincx) {
    CK_OBJECT_CLASS certClass = CKO_CERTIFICATE;
    CK_ATTRIBUTE theTemplate[] = {
	{ CKA_VALUE, NULL, 0 },
	{ CKA_CLASS, NULL, 0 }
    };
    /* if you change the array, change the variable below as well */
    int tsize = sizeof(theTemplate)/sizeof(theTemplate[0]);
    CK_OBJECT_HANDLE certh;
    CK_OBJECT_HANDLE keyh;
    CK_ATTRIBUTE *attrs = theTemplate;
    SECStatus rv;

    PK11_SETATTRS(attrs, CKA_VALUE, cert->derCert.data, 
						cert->derCert.len); attrs++;
    PK11_SETATTRS(attrs, CKA_CLASS, &certClass, sizeof(certClass));

    /*
     * issue the find
     */
    rv = PK11_Authenticate(slot, PR_TRUE, wincx);
    if (rv != SECSuccess) {
	return NULL;
    }

    certh = pk11_getcerthandle(slot,cert,theTemplate,tsize);
    if (certh == CK_INVALID_HANDLE) {
	return NULL;
    }
    keyh = PK11_MatchItem(slot,certh,CKO_PRIVATE_KEY);
    if (keyh == CK_INVALID_HANDLE) { return NULL; }
    return PK11_MakePrivKey(slot, nullKey, PR_TRUE, keyh, wincx);
} 


/*
 * return the private key with the given ID
 */
static CK_OBJECT_HANDLE
pk11_FindPrivateKeyFromCertID(PK11SlotInfo *slot, SECItem *keyID)  {
    CK_OBJECT_CLASS privKey = CKO_PRIVATE_KEY;
    CK_ATTRIBUTE theTemplate[] = {
	{ CKA_ID, NULL, 0 },
	{ CKA_CLASS, NULL, 0 },
    };
    /* if you change the array, change the variable below as well */
    int tsize = sizeof(theTemplate)/sizeof(theTemplate[0]);
    CK_ATTRIBUTE *attrs = theTemplate;

    PK11_SETATTRS(attrs, CKA_ID, keyID->data, keyID->len ); attrs++;
    PK11_SETATTRS(attrs, CKA_CLASS, &privKey, sizeof(privKey));

    return pk11_FindObjectByTemplate(slot,theTemplate,tsize);
} 

/*
 * import a cert for a private key we have already generated. Set the label
 * on both to be the nickname. This is for the Key Gen, orphaned key case.
 */
PK11SlotInfo *
PK11_KeyForCertExists(CERTCertificate *cert, CK_OBJECT_HANDLE *keyPtr, 
								void *wincx) {
    PK11SlotList *list;
    PK11SlotListElement *le;
    SECItem *keyID;
    CK_OBJECT_HANDLE key;
    PK11SlotInfo *slot = NULL;
    SECStatus rv;

    keyID = pk11_mkcertKeyID(cert);
    /* get them all! */
    list = PK11_GetAllTokens(CKM_INVALID_MECHANISM,PR_FALSE,PR_TRUE,wincx);
    if ((keyID == NULL) || (list == NULL)) {
	if (keyID) SECITEM_FreeItem(keyID,PR_TRUE);
	if (list) PK11_FreeSlotList(list);
    	return NULL;
    }

    /* Look for the slot that holds the Key */
    for (le = list->head ; le; le = le->next) {
	rv = PK11_Authenticate(le->slot, PR_TRUE, wincx);
	if (rv != SECSuccess) continue;
	
	key = pk11_FindPrivateKeyFromCertID(le->slot,keyID);
	if (key != CK_INVALID_HANDLE) {
	    slot = PK11_ReferenceSlot(le->slot);
	    if (keyPtr) *keyPtr = key;
	    break;
	}
    }

    SECITEM_FreeItem(keyID,PR_TRUE);
    PK11_FreeSlotList(list);
    return slot;

}
/*
 * import a cert for a private key we have already generated. Set the label
 * on both to be the nickname. This is for the Key Gen, orphaned key case.
 */
PK11SlotInfo *
PK11_KeyForDERCertExists(SECItem *derCert, CK_OBJECT_HANDLE *keyPtr, 
								void *wincx) {
    CERTCertificate *cert;
    PK11SlotInfo *slot = NULL;

    cert = CERT_DecodeDERCertificate(derCert, PR_FALSE, NULL);
    if (cert == NULL) return NULL;

    slot = PK11_KeyForCertExists(cert, keyPtr, wincx);
    CERT_DestroyCertificate (cert);
    return slot;
}

PK11SlotInfo *
PK11_ImportCertForKey(CERTCertificate *cert, char *nickname,void *wincx) {
    PK11SlotInfo *slot = NULL;
    CK_OBJECT_HANDLE key;

    slot = PK11_KeyForCertExists(cert,&key,wincx);

    if (slot) {
	if (PK11_ImportCert(slot,cert,key,nickname,PR_FALSE) != SECSuccess) {
	    PK11_FreeSlot(slot);
	    slot = NULL;
	}
    } else {
	PORT_SetError(SEC_ERROR_ADDING_CERT);
    }

    return slot;
}

PK11SlotInfo *
PK11_ImportDERCertForKey(SECItem *derCert, char *nickname,void *wincx) {
    CERTCertificate *cert;
    PK11SlotInfo *slot = NULL;

    cert = CERT_DecodeDERCertificate(derCert, PR_FALSE, NULL);
    if (cert == NULL) return NULL;

    slot = PK11_ImportCertForKey(cert, nickname, wincx);
    CERT_DestroyCertificate (cert);
    return slot;
}

static CK_OBJECT_HANDLE
pk11_FindCertObjectByTemplate(PK11SlotInfo **slotPtr, 
		CK_ATTRIBUTE *searchTemplate, int count, void *wincx) {
    PK11SlotList *list;
    PK11SlotListElement *le;
    CK_OBJECT_HANDLE certHandle = CK_INVALID_HANDLE;
    PK11SlotInfo *slot = NULL;
    SECStatus rv;

    *slotPtr = NULL;

    /* get them all! */
    list = PK11_GetAllTokens(CKM_INVALID_MECHANISM,PR_FALSE,PR_TRUE,wincx);
    if (list == NULL) {
	if (list) PK11_FreeSlotList(list);
    	return CK_INVALID_HANDLE;
    }


    /* Look for the slot that holds the Key */
    for (le = list->head ; le; le = le->next) {
 	if (!PK11_IsFriendly(le->slot)) {
	    rv = PK11_Authenticate(le->slot, PR_TRUE, wincx);
	    if (rv != SECSuccess) continue;
	}

	certHandle = pk11_FindObjectByTemplate(le->slot,searchTemplate,count);
	if (certHandle != CK_INVALID_HANDLE) {
	    slot = PK11_ReferenceSlot(le->slot);
	    break;
	}
    }

    PK11_FreeSlotList(list);

    if (slot == NULL) {
	return CK_INVALID_HANDLE;
    }
    *slotPtr = slot;
    return certHandle;
}


/*
 * We're looking for a cert which we have the private key for that's on the
 * list of recipients. This searches one slot.
 * this is the new version for NSS SMIME code
 * this stuff should REALLY be in the SMIME code, but some things in here are not public
 * (they should be!)
 */
static CK_OBJECT_HANDLE
pk11_FindCertObjectByRecipientNew(PK11SlotInfo *slot, NSSCMSRecipient **recipientlist, int *rlIndex)
{
    CK_OBJECT_HANDLE certHandle;
    CK_OBJECT_CLASS certClass = CKO_CERTIFICATE;
    CK_OBJECT_CLASS peerClass ;
    CK_ATTRIBUTE searchTemplate[] = {
	{ CKA_CLASS, NULL, 0 },
	{ CKA_ISSUER, NULL, 0 },
	{ CKA_SERIAL_NUMBER, NULL, 0}
    };
    int count = sizeof(searchTemplate)/sizeof(CK_ATTRIBUTE);
    NSSCMSRecipient *ri = NULL;
    CK_ATTRIBUTE *attrs;
    int i;

    peerClass = CKO_PRIVATE_KEY;
    if (!PK11_IsLoggedIn(slot,NULL) && PK11_NeedLogin(slot)) {
	peerClass = CKO_PUBLIC_KEY;
    }

    for (i=0; (ri = recipientlist[i]) != NULL; i++) {
	/* XXXXX fixme - not yet implemented! */
	if (ri->kind == RLSubjKeyID)
	    continue;

    	attrs = searchTemplate;

	PK11_SETATTRS(attrs, CKA_CLASS, &certClass,sizeof(certClass)); attrs++;
	PK11_SETATTRS(attrs, CKA_ISSUER, ri->id.issuerAndSN->derIssuer.data, 
				ri->id.issuerAndSN->derIssuer.len); attrs++;
	PK11_SETATTRS(attrs, CKA_SERIAL_NUMBER, 
	  ri->id.issuerAndSN->serialNumber.data,ri->id.issuerAndSN->serialNumber.len);

	certHandle = pk11_FindObjectByTemplate(slot,searchTemplate,count);
	if (certHandle != CK_INVALID_HANDLE) {
	    CERTCertificate *cert = pk11_fastCert(slot,certHandle,NULL,NULL);
	    if (PK11_IsUserCert(slot,cert,certHandle)) {
		/* we've found a cert handle, now let's see if there is a key
	         * associated with it... */
		ri->slot = PK11_ReferenceSlot(slot);
		*rlIndex = i;
	
		CERT_DestroyCertificate(cert);       
		return certHandle;
	    }
	    CERT_DestroyCertificate(cert);       
	}
    }
    *rlIndex = -1;
    return CK_INVALID_HANDLE;
}

/*
 * This function is the same as above, but it searches all the slots.
 * this is the new version for NSS SMIME code
 * this stuff should REALLY be in the SMIME code, but some things in here are not public
 * (they should be!)
 */
static CK_OBJECT_HANDLE
pk11_AllFindCertObjectByRecipientNew(NSSCMSRecipient **recipientlist, void *wincx, int *rlIndex)
{
    PK11SlotList *list;
    PK11SlotListElement *le;
    CK_OBJECT_HANDLE certHandle = CK_INVALID_HANDLE;
    SECStatus rv;

    /* get them all! */
    list = PK11_GetAllTokens(CKM_INVALID_MECHANISM,PR_FALSE,PR_TRUE,wincx);
    if (list == NULL) {
	if (list) PK11_FreeSlotList(list);
    	return CK_INVALID_HANDLE;
    }

    /* Look for the slot that holds the Key */
    for (le = list->head ; le; le = le->next) {
	if ( !PK11_IsFriendly(le->slot)) {
	    rv = PK11_Authenticate(le->slot, PR_TRUE, wincx);
	    if (rv != SECSuccess) continue;
	}

	certHandle = pk11_FindCertObjectByRecipientNew(le->slot, recipientlist, rlIndex);
	if (certHandle != CK_INVALID_HANDLE)
	    break;
    }

    PK11_FreeSlotList(list);

    return (le == NULL) ? CK_INVALID_HANDLE : certHandle;
}

/*
 * We're looking for a cert which we have the private key for that's on the
 * list of recipients. This searches one slot.
 */
static CK_OBJECT_HANDLE
pk11_FindCertObjectByRecipient(PK11SlotInfo *slot, 
	SEC_PKCS7RecipientInfo **recipientArray,SEC_PKCS7RecipientInfo **rip)
{
    CK_OBJECT_HANDLE certHandle;
    CK_OBJECT_CLASS certClass = CKO_CERTIFICATE;
    CK_OBJECT_CLASS peerClass ;
    CK_ATTRIBUTE searchTemplate[] = {
	{ CKA_CLASS, NULL, 0 },
	{ CKA_ISSUER, NULL, 0 },
	{ CKA_SERIAL_NUMBER, NULL, 0}
    };
    int count = sizeof(searchTemplate)/sizeof(CK_ATTRIBUTE);
    SEC_PKCS7RecipientInfo *ri = NULL;
    CK_ATTRIBUTE *attrs;
    int i;


    peerClass = CKO_PRIVATE_KEY;
    if (!PK11_IsLoggedIn(slot,NULL) && PK11_NeedLogin(slot)) {
	peerClass = CKO_PUBLIC_KEY;
    }

    for (i=0; (ri = recipientArray[i]) != NULL; i++) {
    	attrs = searchTemplate;

	PK11_SETATTRS(attrs, CKA_CLASS, &certClass,sizeof(certClass)); attrs++;
	PK11_SETATTRS(attrs, CKA_ISSUER, ri->issuerAndSN->derIssuer.data, 
				ri->issuerAndSN->derIssuer.len); attrs++;
	PK11_SETATTRS(attrs, CKA_SERIAL_NUMBER, 
	  ri->issuerAndSN->serialNumber.data,ri->issuerAndSN->serialNumber.len);

	certHandle = pk11_FindObjectByTemplate(slot,searchTemplate,count);
	if (certHandle != CK_INVALID_HANDLE) {
	    CERTCertificate *cert = pk11_fastCert(slot,certHandle,NULL,NULL);
	    if (PK11_IsUserCert(slot,cert,certHandle)) {
		/* we've found a cert handle, now let's see if there is a key
	         * associated with it... */
		*rip = ri;
	
		CERT_DestroyCertificate(cert);       
		return certHandle;
	    }
	    CERT_DestroyCertificate(cert);       
	}
    }
    *rip = NULL;
    return CK_INVALID_HANDLE;
}

/*
 * This function is the same as above, but it searches all the slots.
 */
static CK_OBJECT_HANDLE
pk11_AllFindCertObjectByRecipient(PK11SlotInfo **slotPtr, 
	SEC_PKCS7RecipientInfo **recipientArray,SEC_PKCS7RecipientInfo **rip,
							void *wincx) {
    PK11SlotList *list;
    PK11SlotListElement *le;
    CK_OBJECT_HANDLE certHandle = CK_INVALID_HANDLE;
    PK11SlotInfo *slot = NULL;
    SECStatus rv;

    *slotPtr = NULL;

    /* get them all! */
    list = PK11_GetAllTokens(CKM_INVALID_MECHANISM,PR_FALSE,PR_TRUE,wincx);
    if (list == NULL) {
	if (list) PK11_FreeSlotList(list);
    	return CK_INVALID_HANDLE;
    }

    *rip = NULL;

    /* Look for the slot that holds the Key */
    for (le = list->head ; le; le = le->next) {
	if ( !PK11_IsFriendly(le->slot)) {
	    rv = PK11_Authenticate(le->slot, PR_TRUE, wincx);
	    if (rv != SECSuccess) continue;
	}

	certHandle = pk11_FindCertObjectByRecipient(le->slot,
							recipientArray,rip);
	if (certHandle != CK_INVALID_HANDLE) {
	    slot = PK11_ReferenceSlot(le->slot);
	    break;
	}
    }

    PK11_FreeSlotList(list);

    if (slot == NULL) {
	return CK_INVALID_HANDLE;
    }
    *slotPtr = slot;
    return certHandle;
}

/*
 * We need to invert the search logic for PKCS 7 because if we search for
 * each cert on the list over all the slots, we wind up with lots of spurious
 * password prompts. This way we get only one password prompt per slot, at
 * the max, and most of the time we can find the cert, and only prompt for
 * the key...
 */
CERTCertificate *
PK11_FindCertAndKeyByRecipientList(PK11SlotInfo **slotPtr, 
	SEC_PKCS7RecipientInfo **array, SEC_PKCS7RecipientInfo **rip,
				SECKEYPrivateKey**privKey, void *wincx)
{
    CK_OBJECT_HANDLE certHandle = CK_INVALID_HANDLE;
    CK_OBJECT_HANDLE keyHandle = CK_INVALID_HANDLE;
    CERTCertificate *cert = NULL;
    SECStatus rv;

    *privKey = NULL;
    certHandle = pk11_AllFindCertObjectByRecipient(slotPtr,array,rip,wincx);
    if (certHandle == CK_INVALID_HANDLE) {
	return NULL;
    }

    rv = PK11_Authenticate(*slotPtr,PR_TRUE,wincx);
    if (rv != SECSuccess) {
	PK11_FreeSlot(*slotPtr);
	*slotPtr = NULL;
	return NULL;
    }

    keyHandle = PK11_MatchItem(*slotPtr,certHandle,CKO_PRIVATE_KEY);
    if (keyHandle == CK_INVALID_HANDLE) { 
	PK11_FreeSlot(*slotPtr);
	*slotPtr = NULL;
	return NULL;
    }

    *privKey =  PK11_MakePrivKey(*slotPtr, nullKey, PR_TRUE, keyHandle, wincx);
    if (*privKey == NULL) {
	PK11_FreeSlot(*slotPtr);
	*slotPtr = NULL;
	return NULL;
    }
    cert = PK11_MakeCertFromHandle(*slotPtr,certHandle,NULL);
    if (cert == NULL) {
	PK11_FreeSlot(*slotPtr);
	SECKEY_DestroyPrivateKey(*privKey);
	*slotPtr = NULL;
	*privKey = NULL;
	return NULL;
    }
    return cert;
}

/*
 * This is the new version of the above function for NSS SMIME code
 * this stuff should REALLY be in the SMIME code, but some things in here are not public
 * (they should be!)
 */
int
PK11_FindCertAndKeyByRecipientListNew(NSSCMSRecipient **recipientlist, void *wincx)
{
    CK_OBJECT_HANDLE certHandle = CK_INVALID_HANDLE;
    CK_OBJECT_HANDLE keyHandle = CK_INVALID_HANDLE;
    NSSCMSRecipient *rl;
    int rlIndex;

    certHandle = pk11_AllFindCertObjectByRecipientNew(recipientlist, wincx, &rlIndex);
    if (certHandle == CK_INVALID_HANDLE) {
	return -1;
    }

    rl = recipientlist[rlIndex];

    /* at this point, rl->slot is set */

    /* authenticate to the token */
    if (PK11_Authenticate(rl->slot, PR_TRUE, wincx) != SECSuccess) {
	PK11_FreeSlot(rl->slot);
	rl->slot = NULL;
	return -1;
    }

    /* try to get a private key handle for the cert we found */
    keyHandle = PK11_MatchItem(rl->slot, certHandle, CKO_PRIVATE_KEY);
    if (keyHandle == CK_INVALID_HANDLE) { 
	PK11_FreeSlot(rl->slot);
	rl->slot = NULL;
	return -1;
    }

    /* make a private key out of the handle */
    rl->privkey = PK11_MakePrivKey(rl->slot, nullKey, PR_TRUE, keyHandle, wincx);
    if (rl->privkey == NULL) {
	PK11_FreeSlot(rl->slot);
	rl->slot = NULL;
	return -1;
    }
    /* make a cert from the cert handle */
    rl->cert = PK11_MakeCertFromHandle(rl->slot, certHandle, NULL);
    if (rl->cert == NULL) {
	PK11_FreeSlot(rl->slot);
	SECKEY_DestroyPrivateKey(rl->privkey);
	rl->slot = NULL;
	rl->privkey = NULL;
	return -1;
    }
    return rlIndex;
}

CERTCertificate *
PK11_FindCertByIssuerAndSN(PK11SlotInfo **slotPtr, CERTIssuerAndSN *issuerSN,
							 void *wincx)
{
#ifdef NSS_CLASSIC
    CK_OBJECT_HANDLE certHandle;
    CERTCertificate *cert = NULL;
    CK_OBJECT_CLASS certClass = CKO_CERTIFICATE;
    CK_ATTRIBUTE searchTemplate[] = {
 	{ CKA_CLASS, NULL, 0 },
	{ CKA_ISSUER, NULL, 0 },
	{ CKA_SERIAL_NUMBER, NULL, 0}
    };
    int count = sizeof(searchTemplate)/sizeof(CK_ATTRIBUTE);
    CK_ATTRIBUTE *attrs = searchTemplate;

    PK11_SETATTRS(attrs, CKA_CLASS, &certClass, sizeof(certClass)); attrs++;
    PK11_SETATTRS(attrs, CKA_ISSUER, issuerSN->derIssuer.data, 
					issuerSN->derIssuer.len); attrs++;
    PK11_SETATTRS(attrs, CKA_SERIAL_NUMBER, issuerSN->serialNumber.data, 
						issuerSN->serialNumber.len);

    certHandle = pk11_FindCertObjectByTemplate
					(slotPtr,searchTemplate,count,wincx);
    if (certHandle == CK_INVALID_HANDLE) {
	return NULL;
    }
    cert = PK11_MakeCertFromHandle(*slotPtr,certHandle,NULL);
    if (cert == NULL) {
	PK11_FreeSlot(*slotPtr);
	return NULL;
    }
    return cert;
#else
    CERTCertificate *rvCert = NULL;
    NSSCertificate *cert;
    NSSDER issuer, serial;
    NSSCryptoContext *cc;
    SECItem *derSerial;

    if (slotPtr) *slotPtr = NULL;

    /* PKCS#11 needs to use DER-encoded serial numbers.  Create a
     * CERTIssuerAndSN that actually has the encoded value and pass that
     * to PKCS#11 (and the crypto context).
     */
    derSerial = SEC_ASN1EncodeItem(NULL, NULL,
                                   &issuerSN->serialNumber,
                                   SEC_IntegerTemplate);
    if (!derSerial) {
	return NULL;
    }

    NSSITEM_FROM_SECITEM(&issuer, &issuerSN->derIssuer);
    NSSITEM_FROM_SECITEM(&serial, derSerial);

    cc = STAN_GetDefaultCryptoContext();
    cert = NSSCryptoContext_FindCertificateByIssuerAndSerialNumber(cc, 
                                                                &issuer, 
                                                                &serial);
    if (cert) {
	SECITEM_FreeItem(derSerial, PR_TRUE);
	return STAN_GetCERTCertificate(cert);
    }
    cert = NSSTrustDomain_FindCertificateByIssuerAndSerialNumber(
                                                  STAN_GetDefaultTrustDomain(),
                                                  &issuer,
                                                  &serial);
    if (cert) {
	rvCert = STAN_GetCERTCertificate(cert);
	if (slotPtr) *slotPtr = PK11_ReferenceSlot(rvCert->slot);
    }
    SECITEM_FreeItem(derSerial, PR_TRUE);
    return rvCert;
#endif
}

CK_OBJECT_HANDLE
PK11_FindObjectForCert(CERTCertificate *cert, void *wincx, PK11SlotInfo **pSlot)
{
    CK_OBJECT_HANDLE certHandle;
    CK_ATTRIBUTE searchTemplate	= { CKA_VALUE, NULL, 0 };
    
    PK11_SETATTRS(&searchTemplate, CKA_VALUE, cert->derCert.data,
		  cert->derCert.len);

    if (cert->slot) {
	certHandle = pk11_getcerthandle(cert->slot,cert,&searchTemplate,1);
	if (certHandle != CK_INVALID_HANDLE) {
	    *pSlot = PK11_ReferenceSlot(cert->slot);
	    return certHandle;
	}
    }

    certHandle = pk11_FindCertObjectByTemplate(pSlot,&searchTemplate,1,wincx);
    if (certHandle != CK_INVALID_HANDLE) {
	if (cert->slot == NULL) {
	    cert->slot = PK11_ReferenceSlot(*pSlot);
	    cert->pkcs11ID = certHandle;
	    cert->ownSlot = PR_TRUE;
	}
    }

    return(certHandle);
}

SECKEYPrivateKey *
PK11_FindKeyByAnyCert(CERTCertificate *cert, void *wincx)
{
    CK_OBJECT_HANDLE certHandle;
    CK_OBJECT_HANDLE keyHandle;
    PK11SlotInfo *slot = NULL;
    SECKEYPrivateKey *privKey;
    SECStatus rv;

    certHandle = PK11_FindObjectForCert(cert, wincx, &slot);
    if (certHandle == CK_INVALID_HANDLE) {
	 return NULL;
    }
    rv = PK11_Authenticate(slot, PR_TRUE, wincx);
    if (rv != SECSuccess) {
	PK11_FreeSlot(slot);
	return NULL;
    }
    keyHandle = PK11_MatchItem(slot,certHandle,CKO_PRIVATE_KEY);
    if (keyHandle == CK_INVALID_HANDLE) { 
	PK11_FreeSlot(slot);
	return NULL;
    }
    privKey =  PK11_MakePrivKey(slot, nullKey, PR_TRUE, keyHandle, wincx);
    PK11_FreeSlot(slot);
    return privKey;
}

CK_OBJECT_HANDLE
pk11_FindPubKeyByAnyCert(CERTCertificate *cert, PK11SlotInfo **slot, void *wincx)
{
    CK_OBJECT_HANDLE certHandle;
    CK_OBJECT_HANDLE keyHandle;

    certHandle = PK11_FindObjectForCert(cert, wincx, slot);
    if (certHandle == CK_INVALID_HANDLE) {
	 return CK_INVALID_HANDLE;
    }
    keyHandle = PK11_MatchItem(*slot,certHandle,CKO_PUBLIC_KEY);
    if (keyHandle == CK_INVALID_HANDLE) { 
	PK11_FreeSlot(*slot);
	return CK_INVALID_HANDLE;
    }
    return keyHandle;
}

SECKEYPrivateKey *
PK11_FindKeyByKeyID(PK11SlotInfo *slot, SECItem *keyID, void *wincx)
{
    CK_OBJECT_HANDLE keyHandle;
    SECKEYPrivateKey *privKey;

    keyHandle = pk11_FindPrivateKeyFromCertID(slot, keyID);
    if (keyHandle == CK_INVALID_HANDLE) { 
	return NULL;
    }
    privKey =  PK11_MakePrivKey(slot, nullKey, PR_TRUE, keyHandle, wincx);
    return privKey;
}

/*
 * find the number of certs in the slot with the same subject name
 */
int
PK11_NumberCertsForCertSubject(CERTCertificate *cert)
{
    CK_OBJECT_CLASS certClass = CKO_CERTIFICATE;
    CK_ATTRIBUTE theTemplate[] = {
	{ CKA_CLASS, NULL, 0 },
	{ CKA_SUBJECT, NULL, 0 },
    };
    CK_ATTRIBUTE *attr = theTemplate;
   int templateSize = sizeof(theTemplate)/sizeof(theTemplate[0]);

    PK11_SETATTRS(attr,CKA_CLASS, &certClass, sizeof(certClass)); attr++;
    PK11_SETATTRS(attr,CKA_SUBJECT,cert->derSubject.data,cert->derSubject.len);

    if (cert->slot == NULL) {
	PK11SlotList *list = PK11_GetAllTokens(CKM_INVALID_MECHANISM,
							PR_FALSE,PR_TRUE,NULL);
	PK11SlotListElement *le;
	int count = 0;

	/* loop through all the fortezza tokens */
	for (le = list->head; le; le = le->next) {
	    count += PK11_NumberObjectsFor(le->slot,theTemplate,templateSize);
	}
	PK11_FreeSlotList(list);
	return count;
    }

    return PK11_NumberObjectsFor(cert->slot,theTemplate,templateSize);
}

/*
 *  Walk all the certs with the same subject
 */
SECStatus
PK11_TraverseCertsForSubject(CERTCertificate *cert,
        SECStatus(* callback)(CERTCertificate*, void *), void *arg)
{
    if(!cert) {
	return SECFailure;
    }
    if (cert->slot == NULL) {
	PK11SlotList *list = PK11_GetAllTokens(CKM_INVALID_MECHANISM,
							PR_FALSE,PR_TRUE,NULL);
	PK11SlotListElement *le;

	/* loop through all the fortezza tokens */
	for (le = list->head; le; le = le->next) {
	    PK11_TraverseCertsForSubjectInSlot(cert,le->slot,callback,arg);
	}
	PK11_FreeSlotList(list);
	return SECSuccess;

    }

    return PK11_TraverseCertsForSubjectInSlot(cert, cert->slot, callback, arg);
}

/* Find all certs with a given subject in the list of token certs */
static nssList *
filter_token_certs_subject(NSSToken *token, NSSDER *subject)
{
    nssListIterator *certs;
    NSSCertificate *cert;
    PRStatus nssrv;
    nssList *rvList;
    certs = nssList_CreateIterator(token->certList);
    if (!certs) return NULL;
    rvList = nssList_Create(NULL, PR_FALSE);
    if (!rvList) {
	nssListIterator_Destroy(certs);
	return NULL;
    }
    for (cert  = (NSSCertificate *)nssListIterator_Start(certs);
         cert != (NSSCertificate *)NULL;
         cert  = (NSSCertificate *)nssListIterator_Next(certs))
    {
	if (nssItem_Equal(&cert->subject, subject, &nssrv)) {
	    nssList_Add(rvList, nssCertificate_AddRef(cert));
	}
    }
    nssListIterator_Finish(certs);
    nssListIterator_Destroy(certs);
    if (nssList_Count(rvList) == 0) {
	nssList_Destroy(rvList);
	rvList = NULL;
    }
    return rvList;
}

/* remove all certs in a list that are not on the given token */
static void 
filter_list_for_token_certs(nssList *certList, NSSToken *token)
{
    nssListIterator *instances, *certs;
    nssCryptokiInstance *instance;
    NSSCertificate *c;
    PRBool isToken = PR_FALSE;
    certs = nssList_CreateIterator(certList);
    if (!certs) return;
    for (c  = (NSSCertificate *)nssListIterator_Start(certs);
         c != (NSSCertificate *)NULL;
         c  = (NSSCertificate *)nssListIterator_Next(certs)) {
	instances = c->object.instances;
	for (instance  = (nssCryptokiInstance *)nssListIterator_Start(instances);
	     instance != (nssCryptokiInstance *)NULL;
	     instance  = (nssCryptokiInstance *)nssListIterator_Next(instances))
	{
	    if (instance->token == token) {
		isToken = PR_TRUE;
		break;
	    }
	}
	nssListIterator_Finish(instances);
	if (!isToken) {
	    /* safe since iterator is copied */
	    nssList_Remove(certList, c);
	    CERT_DestroyCertificate(STAN_GetCERTCertificate(c));
	}
    }
    nssListIterator_Finish(certs);
    nssListIterator_Destroy(certs);
}

SECStatus
PK11_TraverseCertsForSubjectInSlot(CERTCertificate *cert, PK11SlotInfo *slot,
	SECStatus(* callback)(CERTCertificate*, void *), void *arg)
{
#ifdef NSS_CLASSIC
    pk11DoCertCallback caller;
    pk11TraverseSlot callarg;
    CK_OBJECT_CLASS certClass = CKO_CERTIFICATE;
    CK_ATTRIBUTE theTemplate[] = {
	{ CKA_CLASS, NULL, 0 },
	{ CKA_SUBJECT, NULL, 0 },
    };
    CK_ATTRIBUTE *attr = theTemplate;
   int templateSize = sizeof(theTemplate)/sizeof(theTemplate[0]);

    PK11_SETATTRS(attr,CKA_CLASS, &certClass, sizeof(certClass)); attr++;
    PK11_SETATTRS(attr,CKA_SUBJECT,cert->derSubject.data,cert->derSubject.len);

    if (slot == NULL)  {
	return SECSuccess;
    }
    caller.noslotcallback = callback;
    caller.callback = NULL;
    caller.itemcallback = NULL;
    caller.callbackArg = arg;
    callarg.callback = pk11_DoCerts;
    callarg.callbackArg = (void *) & caller;
    callarg.findTemplate = theTemplate;
    callarg.templateCount = templateSize;
    
    return PK11_TraverseSlot(slot, &callarg);
#else
    struct nss3_cert_cbstr pk11cb;
    PRStatus nssrv = PR_SUCCESS;
    NSSToken *token;
    NSSDER subject;
    NSSTrustDomain *td;
    nssList *subjectList;
    nssTokenCertSearch search;
    pk11cb.callback = callback;
    pk11cb.arg = arg;
    td = STAN_GetDefaultTrustDomain();
    NSSITEM_FROM_SECITEM(&subject, &cert->derSubject);
    token = PK11Slot_GetNSSToken(slot);
    if (!nssToken_SearchCerts(token, NULL)) {
	subjectList = filter_token_certs_subject(token, &subject);
	if (subjectList) {
	    nssrv = nssCertificateList_DoCallback(subjectList, 
	                                          convert_cert, &pk11cb);
	}
    } else {
	subjectList = nssList_Create(NULL, PR_FALSE);
	if (!subjectList) {
	    return SECFailure;
	}
	(void)nssTrustDomain_GetCertsForSubjectFromCache(td, &subject, 
	                                                 subjectList);
	/* set the search criteria */
	search.callback = convert_and_cache_cert;
	search.cbarg = &pk11cb;
	search.cached = subjectList;
	search.searchType = nssTokenSearchType_TokenOnly;
	pk11cb.cached = subjectList;
	nssrv = nssToken_TraverseCertificatesBySubject(token, NULL, 
	                                               &subject, &search);
	if (nssrv == PR_SUCCESS) {
	    filter_list_for_token_certs(subjectList, token);
	    nssrv = nssCertificateList_DoCallback(subjectList, 
	                                          convert_cert, &pk11cb);
	}
    }
    if (subjectList) {
	nssList_Clear(subjectList, cert_destructor);
	nssList_Destroy(subjectList);
    }
    return (nssrv == PR_SUCCESS) ? SECSuccess : SECFailure;
#endif
}

SECStatus
PK11_TraverseCertsForNicknameInSlot(SECItem *nickname, PK11SlotInfo *slot,
	SECStatus(* callback)(CERTCertificate*, void *), void *arg)
{
#ifdef NSS_CLASSIC
    pk11DoCertCallback caller;
    pk11TraverseSlot callarg;
    CK_OBJECT_CLASS certClass = CKO_CERTIFICATE;
    CK_ATTRIBUTE theTemplate[] = {
	{ CKA_CLASS, NULL, 0 },
	{ CKA_LABEL, NULL, 0 },
    };
    CK_ATTRIBUTE *attr = theTemplate;
    int templateSize = sizeof(theTemplate)/sizeof(theTemplate[0]);

    if(!nickname) {
	return SECSuccess;
    }

    PK11_SETATTRS(attr,CKA_CLASS, &certClass, sizeof(certClass)); attr++;
    PK11_SETATTRS(attr,CKA_LABEL,nickname->data,nickname->len);

    if (slot == NULL) {
	return SECSuccess;
    }

    caller.noslotcallback = callback;
    caller.callback = NULL;
    caller.itemcallback = NULL;
    caller.callbackArg = arg;
    callarg.callback = pk11_DoCerts;
    callarg.callbackArg = (void *) & caller;
    callarg.findTemplate = theTemplate;
    callarg.templateCount = templateSize;

    return PK11_TraverseSlot(slot, &callarg);
#else
    struct nss3_cert_cbstr pk11cb;
    PRStatus nssrv = PR_SUCCESS;
    NSSToken *token;
    NSSTrustDomain *td;
    NSSUTF8 *nick;
    PRBool created = PR_FALSE;
    nssTokenCertSearch search;
    nssList *nameList;
    pk11cb.callback = callback;
    pk11cb.arg = arg;
    if (nickname->data[nickname->len-1] != '\0') {
	nick = nssUTF8_Create(NULL, nssStringType_UTF8String, 
	                      nickname->data, nickname->len);
	created = PR_TRUE;
    } else {
	nick = (NSSUTF8 *)nickname->data;
    }
    td = STAN_GetDefaultTrustDomain();
    token = PK11Slot_GetNSSToken(slot);
    if (!nssToken_SearchCerts(token, NULL)) {
	nameList = filter_token_certs_nickname(token, nick);
	if (nameList) {
	    nssrv = nssCertificateList_DoCallback(nameList, 
	                                          convert_cert, &pk11cb);
	}
    } else {
	nameList = nssList_Create(NULL, PR_FALSE);
	(void)nssTrustDomain_GetCertsForNicknameFromCache(td, nick, nameList);
	/* set the search criteria */
	search.callback = convert_and_cache_cert;
	search.cbarg = &pk11cb;
	search.cached = nameList;
	search.searchType = nssTokenSearchType_TokenOnly;
	pk11cb.cached = nameList;
	nssrv = nssToken_TraverseCertificatesByNickname(token, NULL, 
	                                                nick, &search);
	if (nssrv == PR_SUCCESS) {
	    filter_list_for_token_certs(nameList, token);
	    nssrv = nssCertificateList_DoCallback(nameList, 
	                                          convert_cert, &pk11cb);
	}
    }
    if (nameList) {
	nssList_Clear(nameList, cert_destructor);
	nssList_Destroy(nameList);
    }
    if (created) nss_ZFreeIf(nick);
    return (nssrv == PR_SUCCESS) ? SECSuccess : SECFailure;
#endif
}

SECStatus
PK11_TraverseCertsInSlot(PK11SlotInfo *slot,
	SECStatus(* callback)(CERTCertificate*, void *), void *arg)
{
#ifdef NSS_CLASSIC
    pk11DoCertCallback caller;
    pk11TraverseSlot callarg;
    CK_OBJECT_CLASS certClass = CKO_CERTIFICATE;
    CK_ATTRIBUTE theTemplate[] = {
	{ CKA_CLASS, NULL, 0 },
    };
    CK_ATTRIBUTE *attr = theTemplate;
    int templateSize = sizeof(theTemplate)/sizeof(theTemplate[0]);

    PK11_SETATTRS(attr,CKA_CLASS, &certClass, sizeof(certClass)); attr++;

    if (slot == NULL) {
	return SECSuccess;
    }

    caller.noslotcallback = callback;
    caller.callback = NULL;
    caller.itemcallback = NULL;
    caller.callbackArg = arg;
    callarg.callback = pk11_DoCerts;
    callarg.callbackArg = (void *) & caller;
    callarg.findTemplate = theTemplate;
    callarg.templateCount = templateSize;
    return PK11_TraverseSlot(slot, &callarg);
#else
    PRStatus nssrv;
    NSSTrustDomain *td = STAN_GetDefaultTrustDomain();
    struct nss3_cert_cbstr pk11cb;
    NSSToken *tok;
    nssList *certList = NULL;
    nssTokenCertSearch search;
    pk11cb.callback = callback;
    pk11cb.arg = arg;
    tok = PK11Slot_GetNSSToken(slot);
    if (!nssToken_SearchCerts(tok, NULL)) {
	certList = tok->certList;
	nssrv = nssCertificateList_DoCallback(certList, convert_cert, &pk11cb);
    } else {
	certList = nssList_Create(NULL, PR_FALSE);
	if (!certList) {
	    return SECFailure;
	}
	(void *)nssTrustDomain_GetCertsFromCache(td, certList);
	/* set the search criteria */
	search.callback = convert_and_cache_cert;
	search.cbarg = &pk11cb;
	search.cached = certList;
	search.searchType = nssTokenSearchType_TokenOnly;
	pk11cb.cached = certList;
	nssrv = nssToken_TraverseCertificates(tok, NULL, &search);
	if (nssrv == PR_SUCCESS) {
	    filter_list_for_token_certs(certList, tok);
	    nssrv = nssCertificateList_DoCallback(certList, 
	                                          convert_cert, &pk11cb);
	}
	nssList_Clear(certList, cert_destructor);
	nssList_Destroy(certList);
    }
    return (nssrv == PR_SUCCESS) ? SECSuccess : SECFailure;
#endif
}

static NSSCertificate *
filter_token_certs_DER(NSSToken *token, NSSDER *der)
{
    nssListIterator *certs;
    NSSCertificate *cert, *rvCert;
    PRStatus nssrv;
    certs = nssList_CreateIterator(token->certList);
    if (!certs) return NULL;
    rvCert = NULL;
    for (cert  = (NSSCertificate *)nssListIterator_Start(certs);
         cert != (NSSCertificate *)NULL;
         cert  = (NSSCertificate *)nssListIterator_Next(certs))
    {
	if (nssItem_Equal(&cert->encoding, der, &nssrv)) {
	    rvCert = nssCertificate_AddRef(cert);
	    break;
	}
    }
    nssListIterator_Finish(certs);
    nssListIterator_Destroy(certs);
    return rvCert;
}

/*
 * return the certificate associated with a derCert 
 */
CERTCertificate *
PK11_FindCertFromDERCert(PK11SlotInfo *slot, CERTCertificate *cert,
								 void *wincx)
{
#ifdef NSS_CLASSIC
    CK_OBJECT_CLASS certClass = CKO_CERTIFICATE;
    CK_ATTRIBUTE theTemplate[] = {
	{ CKA_VALUE, NULL, 0 },
	{ CKA_CLASS, NULL, 0 }
    };
    /* if you change the array, change the variable below as well */
    int tsize = sizeof(theTemplate)/sizeof(theTemplate[0]);
    CK_OBJECT_HANDLE certh;
    CK_ATTRIBUTE *attrs = theTemplate;
    SECStatus rv;

    PK11_SETATTRS(attrs, CKA_VALUE, cert->derCert.data, 
						cert->derCert.len); attrs++;
    PK11_SETATTRS(attrs, CKA_CLASS, &certClass, sizeof(certClass));

    /*
     * issue the find
     */
    if ( !PK11_IsFriendly(slot)) {
	rv = PK11_Authenticate(slot, PR_TRUE, wincx);
	if (rv != SECSuccess) return NULL;
    }

    certh = pk11_getcerthandle(slot,cert,theTemplate,tsize);
    if (certh == CK_INVALID_HANDLE) {
	return NULL;
    }
    return PK11_MakeCertFromHandle(slot, certh, NULL);
#else
    CERTCertificate *rvCert = NULL;
    NSSCertificate *c;
    NSSDER derCert;
    NSSToken *tok;
    NSSTrustDomain *td = STAN_GetDefaultTrustDomain();
    tok = PK11Slot_GetNSSToken(slot);
    NSSITEM_FROM_SECITEM(&derCert, &cert->derCert);
    if (!PK11_IsFriendly(slot)) {
	if (PK11_Authenticate(slot, PR_TRUE, wincx) != SECSuccess) {
	    PK11_FreeSlot(slot);
	    return NULL;
	}
    }
    if (!nssToken_SearchCerts(tok, NULL)) {
	c = filter_token_certs_DER(tok, &derCert);
    } else {
	c = nssTrustDomain_GetCertByDERFromCache(td, &derCert);
	if (!c) {
	    c = nssToken_FindCertificateByEncodedCertificate(tok, NULL, 
	                                        &derCert,
                                                nssTokenSearchType_TokenOnly);
	}
    }
    if (c) {
	rvCert = STAN_GetCERTCertificate(c);
    }
    return rvCert;
#endif
} 

/* mcgreer 3.4 -- nobody uses this, ignoring */
/*
 * return the certificate associated with a derCert 
 */
CERTCertificate *
PK11_FindCertFromDERSubjectAndNickname(PK11SlotInfo *slot, 
					CERTCertificate *cert, 
					char *nickname, void *wincx)
{
    CK_OBJECT_CLASS certClass = CKO_CERTIFICATE;
    CK_ATTRIBUTE theTemplate[] = {
	{ CKA_SUBJECT, NULL, 0 },
	{ CKA_LABEL, NULL, 0 },
	{ CKA_CLASS, NULL, 0 }
    };
    /* if you change the array, change the variable below as well */
    int tsize = sizeof(theTemplate)/sizeof(theTemplate[0]);
    CK_OBJECT_HANDLE certh;
    CK_ATTRIBUTE *attrs = theTemplate;
    SECStatus rv;

    PK11_SETATTRS(attrs, CKA_SUBJECT, cert->derSubject.data, 
						cert->derSubject.len); attrs++;
    PK11_SETATTRS(attrs, CKA_LABEL, nickname, PORT_Strlen(nickname));
    PK11_SETATTRS(attrs, CKA_CLASS, &certClass, sizeof(certClass));

    /*
     * issue the find
     */
    if ( !PK11_IsFriendly(slot)) {
	rv = PK11_Authenticate(slot, PR_TRUE, wincx);
	if (rv != SECSuccess) return NULL;
    }

    certh = pk11_getcerthandle(slot,cert,theTemplate,tsize);
    if (certh == CK_INVALID_HANDLE) {
	return NULL;
    }

    return PK11_MakeCertFromHandle(slot, certh, NULL);
}

/*
 * import a cert for a private key we have already generated. Set the label
 * on both to be the nickname.
 */
static CK_OBJECT_HANDLE 
pk11_findKeyObjectByDERCert(PK11SlotInfo *slot, CERTCertificate *cert, 
								void *wincx)
{
    SECItem *keyID;
    CK_OBJECT_HANDLE key;
    SECStatus rv;

    if((slot == NULL) || (cert == NULL)) {
	return CK_INVALID_HANDLE;
    }

    keyID = pk11_mkcertKeyID(cert);
    if(keyID == NULL) {
	return CK_INVALID_HANDLE;
    }

    key = CK_INVALID_HANDLE;

    rv = PK11_Authenticate(slot, PR_TRUE, wincx);
    if (rv != SECSuccess) goto loser;

    key = pk11_FindPrivateKeyFromCertID(slot, keyID);

loser:
    SECITEM_ZfreeItem(keyID, PR_TRUE);
    return key;
}

SECKEYPrivateKey *
PK11_FindKeyByDERCert(PK11SlotInfo *slot, CERTCertificate *cert, 
								void *wincx)
{
    CK_OBJECT_HANDLE keyHandle;

    if((slot == NULL) || (cert == NULL)) {
	return NULL;
    }

    keyHandle = pk11_findKeyObjectByDERCert(slot, cert, wincx);
    if (keyHandle == CK_INVALID_HANDLE) {
	return NULL;
    }

    return PK11_MakePrivKey(slot,nullKey,PR_TRUE,keyHandle,wincx);
}

SECStatus
PK11_ImportCertForKeyToSlot(PK11SlotInfo *slot, CERTCertificate *cert, 
						char *nickname, 
						PRBool addCertUsage,void *wincx)
{
    CK_OBJECT_HANDLE keyHandle;

    if((slot == NULL) || (cert == NULL) || (nickname == NULL)) {
	return SECFailure;
    }

    keyHandle = pk11_findKeyObjectByDERCert(slot, cert, wincx);
    if (keyHandle == CK_INVALID_HANDLE) {
	return SECFailure;
    }

    return PK11_ImportCert(slot, cert, keyHandle, nickname, addCertUsage);
}   


/* remove when the real version comes out */
#define SEC_OID_MISSI_KEA 300  /* until we have v3 stuff merged */
PRBool
KEAPQGCompare(CERTCertificate *server,CERTCertificate *cert) {

    if ( SECKEY_KEAParamCompare(server,cert) == SECEqual ) {
        return PR_TRUE;
    } else {
	return PR_FALSE;
    }
}

PRBool
PK11_FortezzaHasKEA(CERTCertificate *cert) {
   /* look at the subject and see if it is a KEA for MISSI key */
   SECOidData *oid;

   if ((cert->trust == NULL) ||
       ((cert->trust->sslFlags & CERTDB_USER) != CERTDB_USER)) {
       return PR_FALSE;
   }

   oid = SECOID_FindOID(&cert->subjectPublicKeyInfo.algorithm.algorithm);


   return (PRBool)((oid->offset == SEC_OID_MISSI_KEA_DSS_OLD) || 
		(oid->offset == SEC_OID_MISSI_KEA_DSS) ||
				(oid->offset == SEC_OID_MISSI_KEA)) ;
}

/*
 * Find a kea cert on this slot that matches the domain of it's peer
 */
static CERTCertificate
*pk11_GetKEAMate(PK11SlotInfo *slot,CERTCertificate *peer)
{
    int i;
    CERTCertificate *returnedCert = NULL;

    for (i=0; i < slot->cert_count; i++) {
	CERTCertificate *cert = slot->cert_array[i];

	if (PK11_FortezzaHasKEA(cert) && KEAPQGCompare(peer,cert)) {
		returnedCert = CERT_DupCertificate(cert);
		break;
	}
    }
    return returnedCert;
}

/*
 * The following is a FORTEZZA only Certificate request. We call this when we
 * are doing a non-client auth SSL connection. We are only interested in the
 * fortezza slots, and we are only interested in certs that share the same root
 * key as the server.
 */
CERTCertificate *
PK11_FindBestKEAMatch(CERTCertificate *server, void *wincx)
{
    PK11SlotList *keaList = PK11_GetAllTokens(CKM_KEA_KEY_DERIVE,
							PR_FALSE,PR_TRUE,wincx);
    PK11SlotListElement *le;
    CERTCertificate *returnedCert = NULL;
    SECStatus rv;

    /* loop through all the fortezza tokens */
    for (le = keaList->head; le; le = le->next) {
        rv = PK11_Authenticate(le->slot, PR_TRUE, wincx);
        if (rv != SECSuccess) continue;
	if (le->slot->session == CK_INVALID_SESSION) {
	    continue;
	}
	returnedCert = pk11_GetKEAMate(le->slot,server);
	if (returnedCert) break;
    }
    PK11_FreeSlotList(keaList);

    return returnedCert;
}

/*
 * find a matched pair of kea certs to key exchange parameters from one 
 * fortezza card to another as necessary.
 */
SECStatus
PK11_GetKEAMatchedCerts(PK11SlotInfo *slot1, PK11SlotInfo *slot2,
	CERTCertificate **cert1, CERTCertificate **cert2)
{
    CERTCertificate *returnedCert = NULL;
    int i;

    for (i=0; i < slot1->cert_count; i++) {
	CERTCertificate *cert = slot1->cert_array[i];

	if (PK11_FortezzaHasKEA(cert)) {
	    returnedCert = pk11_GetKEAMate(slot2,cert);
	    if (returnedCert != NULL) {
		*cert2 = returnedCert;
		*cert1 = CERT_DupCertificate(cert);
		return SECSuccess;
	    }
	}
    }
    return SECFailure;
}

SECOidTag 
PK11_FortezzaMapSig(SECOidTag algTag)
{
    switch (algTag) {
    case SEC_OID_MISSI_KEA_DSS:
    case SEC_OID_MISSI_DSS:
    case SEC_OID_MISSI_DSS_OLD:
    case SEC_OID_MISSI_KEA_DSS_OLD:
    case SEC_OID_BOGUS_DSA_SIGNATURE_WITH_SHA1_DIGEST:
	return SEC_OID_ANSIX9_DSA_SIGNATURE;
    default:
	break;
    }
    return algTag;
}

/*
 * return the private key From a given Cert
 */
CK_OBJECT_HANDLE
PK11_FindCertInSlot(PK11SlotInfo *slot, CERTCertificate *cert, void *wincx)
{
    CK_OBJECT_CLASS certClass = CKO_CERTIFICATE;
    CK_ATTRIBUTE theTemplate[] = {
	{ CKA_VALUE, NULL, 0 },
	{ CKA_CLASS, NULL, 0 }
    };
    /* if you change the array, change the variable below as well */
    int tsize = sizeof(theTemplate)/sizeof(theTemplate[0]);
    CK_ATTRIBUTE *attrs = theTemplate;
    SECStatus rv;

    PK11_SETATTRS(attrs, CKA_VALUE, cert->derCert.data,
						cert->derCert.len); attrs++;
    PK11_SETATTRS(attrs, CKA_CLASS, &certClass, sizeof(certClass));

    /*
     * issue the find
     */
    rv = PK11_Authenticate(slot, PR_TRUE, wincx);
    if (rv != SECSuccess) {
	return CK_INVALID_HANDLE;
    }

    return pk11_getcerthandle(slot,cert,theTemplate,tsize);
}

SECItem *
PK11_GetKeyIDFromCert(CERTCertificate *cert, void *wincx)
{
    CK_OBJECT_HANDLE handle;
    PK11SlotInfo *slot = NULL;
    CK_ATTRIBUTE theTemplate[] = {
	{ CKA_ID, NULL, 0 },
    };
    int tsize = sizeof(theTemplate)/sizeof(theTemplate[0]);
    SECItem *item = NULL;
    CK_RV crv;

    handle = PK11_FindObjectForCert(cert,wincx,&slot);
    if (handle == CK_INVALID_HANDLE) {
	goto loser;
    }


    crv = PK11_GetAttributes(NULL,slot,handle,theTemplate,tsize);
    if (crv != CKR_OK) {
	PORT_SetError( PK11_MapError(crv) );
	goto loser;
    }

    item = PORT_ZNew(SECItem);
    if (item) {
        item->data = theTemplate[0].pValue;
        item->len = theTemplate[0].ulValueLen;
    }


loser:
    PK11_FreeSlot(slot);
    return item;
}

SECItem *
PK11_GetKeyIDFromPrivateKey(SECKEYPrivateKey *key, void *wincx)
{
    CK_ATTRIBUTE theTemplate[] = {
	{ CKA_ID, NULL, 0 },
    };
    int tsize = sizeof(theTemplate)/sizeof(theTemplate[0]);
    SECItem *item = NULL;
    CK_RV crv;

    crv = PK11_GetAttributes(NULL,key->pkcs11Slot,key->pkcs11ID,
							theTemplate,tsize);
    if (crv != CKR_OK) {
	PORT_SetError( PK11_MapError(crv) );
	goto loser;
    }

    item = PORT_ZNew(SECItem);
    if (item) {
        item->data = theTemplate[0].pValue;
        item->len = theTemplate[0].ulValueLen;
    }


loser:
    return item;
}

struct listCertsStr {
    PK11CertListType type;
    CERTCertList *certList;
};

static PRBool
isOnList(CERTCertList *certList,CERTCertificate *cert)
{
	CERTCertListNode *cln;

	for (cln = CERT_LIST_HEAD(certList); !CERT_LIST_END(cln,certList);
			cln = CERT_LIST_NEXT(cln)) {
	    if (cln->cert == cert) {
		return PR_TRUE;
	    }
	}
	return PR_FALSE;
}
static SECStatus
#ifdef NSS_CLASSIC
pk11ListCertCallback(CERTCertificate *cert, SECItem *derCert, void *arg)
#else
pk11ListCertCallback(CERTCertificate *cert, void *arg)
#endif
{
    struct listCertsStr *listCertP = (struct listCertsStr *)arg;
    CERTCertificate *newCert = NULL;
    PK11CertListType type = listCertP->type;
    CERTCertList *certList = listCertP->certList;
    CERTCertTrust *trust;
    PRBool isUnique = PR_FALSE;
    char *nickname = NULL;
    unsigned int certType;

    if ((type == PK11CertListUnique) || (type == PK11CertListRootUnique)) {
	isUnique = PR_TRUE;
    }
    /* at this point the nickname is correct for the cert. save it for later */
    if (!isUnique && cert->nickname) {
         nickname = PORT_ArenaStrdup(listCertP->certList->arena,cert->nickname);
    }
#ifdef NSS_CLASSIC
    if (derCert == NULL) {
	newCert=CERT_DupCertificate(cert);
    } else {
	newCert=CERT_FindCertByDERCert(CERT_GetDefaultCertDB(),&cert->derCert);
    }
#else
    newCert=CERT_DupCertificate(cert);
#endif

    if (newCert == NULL) return SECSuccess;

    trust = newCert->trust;

    /* if we want user certs and we don't have one skip this cert */
    if ((type == PK11CertListUser) && 
	  ((trust == NULL) || 
		( ((trust->sslFlags & CERTDB_USER) == 0)  && 
			((trust->emailFlags & CERTDB_USER) == 0) )) ) {
	CERT_DestroyCertificate(newCert);
	return SECSuccess;
    }

    /* if we want root certs, skip the user certs */
    if ((type == PK11CertListRootUnique) && 
	  ((trust) && (((trust->sslFlags & CERTDB_USER )  || 
				(trust->emailFlags & CERTDB_USER))) ) ) {
	CERT_DestroyCertificate(newCert);
	return SECSuccess;
    }


    /* if we want Unique certs and we already have it on our list, skip it */
    if ( isUnique && isOnList(certList,newCert) ) {
	CERT_DestroyCertificate(newCert);
	return SECSuccess;
    }

    /* if we want CA certs and it ain't one, skip it */
    if( type == PK11CertListCA  && (!CERT_IsCACert(newCert, &certType)) ) {
	CERT_DestroyCertificate(newCert);
	return SECSuccess;
    }

    /* put slot certs at the end */
    if (newCert->slot && !PK11_IsInternal(newCert->slot)) {
    	CERT_AddCertToListTailWithData(certList,newCert,nickname);
    } else {
    	CERT_AddCertToListHeadWithData(certList,newCert,nickname);
    }
    return SECSuccess;
}


CERTCertList *
PK11_ListCerts(PK11CertListType type, void *pwarg)
{
#ifdef NSS_CLASSIC
    CERTCertList *certList = NULL;
    struct listCertsStr listCerts;
    
    certList= CERT_NewCertList();
    listCerts.type = type;
    listCerts.certList = certList;

    PK11_TraverseSlotCerts(pk11ListCertCallback,&listCerts,pwarg);

    if (CERT_LIST_HEAD(certList) == NULL) {
	CERT_DestroyCertList(certList);
	certList = NULL;
    }
    return certList;
#else
    NSSTrustDomain *defaultTD = STAN_GetDefaultTrustDomain();
    CERTCertList *certList = NULL;
    struct nss3_cert_cbstr pk11cb;
    struct listCertsStr listCerts;
    certList = CERT_NewCertList();
    listCerts.type = type;
    listCerts.certList = certList;
    pk11cb.callback = pk11ListCertCallback;
    pk11cb.arg = &listCerts;
    NSSTrustDomain_TraverseCertificates(defaultTD, convert_cert, &pk11cb);
    return certList;
#endif
}

static SECItem *
pk11_GetLowLevelKeyFromHandle(PK11SlotInfo *slot, CK_OBJECT_HANDLE handle) {
    CK_ATTRIBUTE theTemplate[] = {
	{ CKA_ID, NULL, 0 },
    };
    int tsize = sizeof(theTemplate)/sizeof(theTemplate[0]);
    CK_RV crv;
    SECItem *item;

    item = SECITEM_AllocItem(NULL, NULL, 0);

    if (item == NULL) {
	return NULL;
    }

    crv = PK11_GetAttributes(NULL,slot,handle,theTemplate,tsize);
    if (crv != CKR_OK) {
	SECITEM_FreeItem(item,PR_TRUE);
	PORT_SetError( PK11_MapError(crv) );
	return NULL;
    }

    item->data = theTemplate[0].pValue;
    item->len =theTemplate[0].ulValueLen;

    return item;
}
    
SECItem *
PK11_GetLowLevelKeyIDForCert(PK11SlotInfo *slot,
					CERTCertificate *cert, void *wincx)
{
    CK_ATTRIBUTE theTemplate[] = {
	{ CKA_VALUE, NULL, 0 },
	{ CKA_CLASS, NULL, 0 }
    };
    /* if you change the array, change the variable below as well */
    int tsize = sizeof(theTemplate)/sizeof(theTemplate[0]);
    CK_OBJECT_HANDLE certHandle;
    CK_ATTRIBUTE *attrs = theTemplate;
    PK11SlotInfo *slotRef = NULL;
    SECItem *item;
    SECStatus rv;

    if (slot) {
	PK11_SETATTRS(attrs, CKA_VALUE, cert->derCert.data, 
						cert->derCert.len); attrs++;

	rv = PK11_Authenticate(slot, PR_TRUE, wincx);
	if (rv != SECSuccess) {
	    return NULL;
	}
        certHandle = pk11_getcerthandle(slot,cert,theTemplate,tsize);
    } else {
    	certHandle = PK11_FindObjectForCert(cert, wincx, &slotRef);
	if (certHandle == CK_INVALID_HANDLE) {
	   return pk11_mkcertKeyID(cert);
	}
	slot = slotRef;
    }

    if (certHandle == CK_INVALID_HANDLE) {
	 return NULL;
    }

    item = pk11_GetLowLevelKeyFromHandle(slot,certHandle);
    if (slotRef) PK11_FreeSlot(slotRef);
    return item;
}

SECItem *
PK11_GetLowLevelKeyIDForPrivateKey(SECKEYPrivateKey *privKey)
{
    return pk11_GetLowLevelKeyFromHandle(privKey->pkcs11Slot,privKey->pkcs11ID);
}

static SECStatus
listCertsCallback(CERTCertificate* cert, void*arg)
{
    CERTCertList *list = (CERTCertList*)arg;
    char *nickname = NULL;

    if (cert->nickname) {
	nickname = PORT_ArenaStrdup(list->arena,cert->nickname);
    }

    return CERT_AddCertToListTailWithData(list, 
				CERT_DupCertificate(cert),nickname);
}

CERTCertList *
PK11_ListCertsInSlot(PK11SlotInfo *slot)
{
    SECStatus status;
    CERTCertList *certs;

    certs = CERT_NewCertList();
    if(certs == NULL) return NULL;

    status = PK11_TraverseCertsInSlot(slot, listCertsCallback,
		(void*)certs);

    if( status != SECSuccess ) {
	CERT_DestroyCertList(certs);
	certs = NULL;
    }

    return certs;
}

static SECStatus
privateKeyListCallback(SECKEYPrivateKey *key, void *arg)
{
    SECKEYPrivateKeyList *list = (SECKEYPrivateKeyList*)arg;
    return SECKEY_AddPrivateKeyToListTail(list, SECKEY_CopyPrivateKey(key));
}

SECKEYPrivateKeyList*
PK11_ListPrivateKeysInSlot(PK11SlotInfo *slot)
{
    SECStatus status;
    SECKEYPrivateKeyList *keys;

    keys = SECKEY_NewPrivateKeyList();
    if(keys == NULL) return NULL;

    status = PK11_TraversePrivateKeysInSlot(slot, privateKeyListCallback,
		(void*)keys);

    if( status != SECSuccess ) {
	SECKEY_DestroyPrivateKeyList(keys);
	keys = NULL;
    }

    return keys;
}

SECKEYPublicKeyList*
PK11_ListPublicKeysInSlot(PK11SlotInfo *slot, char *nickname)
{
    CK_ATTRIBUTE findTemp[4];
    CK_ATTRIBUTE *attrs;
    CK_BBOOL ckTrue = CK_TRUE;
    CK_OBJECT_CLASS keyclass = CKO_PUBLIC_KEY;
    int tsize = 0;
    int objCount = 0;
    CK_OBJECT_HANDLE *key_ids;
    SECStatus status;
    SECKEYPublicKeyList *keys;
    int i,len;


    attrs = findTemp;
    PK11_SETATTRS(attrs, CKA_CLASS, &keyclass, sizeof(keyclass)); attrs++;
    PK11_SETATTRS(attrs, CKA_TOKEN, &ckTrue, sizeof(ckTrue)); attrs++;
    if (nickname) {
	len = PORT_Strlen(nickname)-1;
	PK11_SETATTRS(attrs, CKA_LABEL, nickname, len); attrs++;
    }
    tsize = attrs - findTemp;
    PORT_Assert(tsize <= sizeof(findTemp)/sizeof(CK_ATTRIBUTE));

    key_ids = pk11_FindObjectsByTemplate(slot,findTemp,tsize,&objCount);
    if (key_ids == NULL) {
	return NULL;
    }
    keys = SECKEY_NewPublicKeyList();
    if (keys == NULL) {
	PORT_Free(key_ids);
    }

    for (i=0; i < objCount ; i++) {
	SECKEYPublicKey *pubKey = 
				PK11_ExtractPublicKey(slot,nullKey,key_ids[i]);
	SECKEY_AddPublicKeyToListTail(keys, pubKey);
   }

   PORT_Free(key_ids);
   return keys;
}

SECKEYPrivateKeyList*
PK11_ListPrivKeysInSlot(PK11SlotInfo *slot, char *nickname, void *wincx)
{
    CK_ATTRIBUTE findTemp[4];
    CK_ATTRIBUTE *attrs;
    CK_BBOOL ckTrue = CK_TRUE;
    CK_OBJECT_CLASS keyclass = CKO_PRIVATE_KEY;
    int tsize = 0;
    int objCount = 0;
    CK_OBJECT_HANDLE *key_ids;
    SECStatus status;
    SECKEYPrivateKeyList *keys;
    int i,len;


    attrs = findTemp;
    PK11_SETATTRS(attrs, CKA_CLASS, &keyclass, sizeof(keyclass)); attrs++;
    PK11_SETATTRS(attrs, CKA_TOKEN, &ckTrue, sizeof(ckTrue)); attrs++;
    if (nickname) {
	len = PORT_Strlen(nickname)-1;
	PK11_SETATTRS(attrs, CKA_LABEL, nickname, len); attrs++;
    }
    tsize = attrs - findTemp;
    PORT_Assert(tsize <= sizeof(findTemp)/sizeof(CK_ATTRIBUTE));

    key_ids = pk11_FindObjectsByTemplate(slot,findTemp,tsize,&objCount);
    if (key_ids == NULL) {
	return NULL;
    }
    keys = SECKEY_NewPrivateKeyList();
    if (keys == NULL) {
	PORT_Free(key_ids);
    }

    for (i=0; i < objCount ; i++) {
	SECKEYPrivateKey *privKey = 
		PK11_MakePrivKey(slot,nullKey,PR_TRUE,key_ids[i],wincx);
	SECKEY_AddPrivateKeyToListTail(keys, privKey);
   }

   PORT_Free(key_ids);
   return keys;
}

/*
 * return the certificate associated with a derCert 
 */
SECItem *
PK11_FindCrlByName(PK11SlotInfo **slot, CK_OBJECT_HANDLE *crlHandle,
					 SECItem *name, int type, char **url)
{
    CK_OBJECT_CLASS crlClass = CKO_NETSCAPE_CRL;
    CK_ATTRIBUTE theTemplate[] = {
	{ CKA_SUBJECT, NULL, 0 },
	{ CKA_CLASS, NULL, 0 },
	{ CKA_NETSCAPE_KRL, NULL, 0 },
    };
    CK_ATTRIBUTE crlData[] = {
	{ CKA_VALUE, NULL, 0 },
	{ CKA_NETSCAPE_URL, NULL, 0 },
    };
    /* if you change the array, change the variable below as well */
    int tsize = sizeof(theTemplate)/sizeof(theTemplate[0]);
    CK_BBOOL ck_true = CK_TRUE;
    CK_BBOOL ck_false = CK_FALSE;
    CK_OBJECT_HANDLE crlh = CK_INVALID_HANDLE;
    CK_ATTRIBUTE *attrs = theTemplate;
    CK_RV crv;
    SECItem *derCrl = NULL;

    PK11_SETATTRS(attrs, CKA_SUBJECT, name->data, name->len); attrs++;
    PK11_SETATTRS(attrs, CKA_CLASS, &crlClass, sizeof(crlClass)); attrs++;
    PK11_SETATTRS(attrs, CKA_NETSCAPE_KRL, (type == SEC_CRL_TYPE) ? 
			&ck_false : &ck_true, sizeof (CK_BBOOL)); attrs++;

    if (*slot) {
    	crlh = pk11_FindObjectByTemplate(*slot,theTemplate,tsize);
    } else {
	PK11SlotList *list = PK11_GetAllTokens(CKM_INVALID_MECHANISM,
							PR_FALSE,PR_TRUE,NULL);
	PK11SlotListElement *le;

	/* loop through all the fortezza tokens */
	for (le = list->head; le; le = le->next) {
	    if (le->slot->nssToken && !nssToken_HasCrls(le->slot->nssToken)) {
		continue;
	    }
	    crlh = pk11_FindObjectByTemplate(le->slot,theTemplate,tsize);
	    if (crlh != CK_INVALID_HANDLE) {
		*slot = PK11_ReferenceSlot(le->slot);
		break;
	    }
	}
	PK11_FreeSlotList(list);
    }
    
    if (crlh == CK_INVALID_HANDLE) {
	PORT_SetError(SEC_ERROR_NO_KRL);
	return NULL;
    }
    crv = PK11_GetAttributes(NULL,*slot,crlh,crlData,2);
    if (crv != CKR_OK) {
	PORT_SetError(PK11_MapError (crv));
	goto loser;
    }

    derCrl = (SECItem *)PORT_ZAlloc(sizeof(SECItem));    
    if (derCrl == NULL) {
	goto loser;
    }

    derCrl->data = crlData[0].pValue;
    derCrl->len = crlData[0].ulValueLen;

    if (crlHandle) {
        *crlHandle = crlh;
    }

    if ((url) && crlData[1].ulValueLen != 0) {
	/* make sure it's a null terminated string */
	*url = PORT_ZAlloc (crlData[1].ulValueLen+1);
	if (*url) {
	    PORT_Memcpy(*url,crlData[1].pValue,crlData[1].ulValueLen);
	}
    }
	

loser:
    if (!derCrl) {
	if (crlData[0].pValue) PORT_Free(crlData[0].pValue);
    }
    if (crlData[1].pValue) PORT_Free(crlData[1].pValue);
    return derCrl;
}

CK_OBJECT_HANDLE
PK11_PutCrl(PK11SlotInfo *slot, SECItem *crl, SECItem *name, 
							char *url, int type)
{
    CK_OBJECT_CLASS crlClass = CKO_NETSCAPE_CRL;
    CK_ATTRIBUTE theTemplate[] = {
	{ CKA_SUBJECT, NULL, 0 },
	{ CKA_CLASS, NULL, 0 },
	{ CKA_NETSCAPE_KRL, NULL, 0 },
	{ CKA_NETSCAPE_URL, NULL, 0 },
	{ CKA_VALUE, NULL, 0 },
	{ CKA_TOKEN, NULL, 0 }
    };
    /* if you change the array, change the variable below as well */
    int tsize;
    CK_BBOOL ck_true = CK_TRUE;
    CK_BBOOL ck_false = CK_FALSE;
    CK_OBJECT_HANDLE crlh = CK_INVALID_HANDLE;
    CK_ATTRIBUTE *attrs = theTemplate;
    CK_SESSION_HANDLE rwsession;
    CK_RV crv;

    PK11_SETATTRS(attrs, CKA_SUBJECT, name->data, name->len); attrs++;
    PK11_SETATTRS(attrs, CKA_CLASS, &crlClass, sizeof(crlClass)); attrs++;
    PK11_SETATTRS(attrs, CKA_NETSCAPE_KRL, (type == SEC_CRL_TYPE) ? 
			&ck_false : &ck_true, sizeof (CK_BBOOL)); attrs++;
    if (url) {
	PK11_SETATTRS(attrs, CKA_NETSCAPE_URL, url, PORT_Strlen(url)+1); attrs++;
    }
    PK11_SETATTRS(attrs, CKA_VALUE,crl->data,crl->len); attrs++;
    PK11_SETATTRS(attrs, CKA_TOKEN, &ck_true,sizeof(CK_BBOOL)); attrs++;

    tsize = attrs - &theTemplate[0];
    PORT_Assert(tsize <= sizeof(theTemplate)/sizeof(theTemplate[0]));

    rwsession = PK11_GetRWSession(slot);
    if (rwsession == CK_INVALID_SESSION) {
	PORT_SetError(SEC_ERROR_READ_ONLY);
	return crlh;
    }

    crv = PK11_GETTAB(slot)->
                        C_CreateObject(rwsession,theTemplate,tsize,&crlh);
    if (crv != CKR_OK) {
        PORT_SetError( PK11_MapError(crv) );
    }

    PK11_RestoreROSession(slot,rwsession);

    if (slot->nssToken) {
	nssToken_SetHasCrls(slot->nssToken);
    }
    return crlh;
}



/*
 * delete a crl.
 */
SECStatus
SEC_DeletePermCRL(CERTSignedCrl *crl)
{
    PK11SlotInfo *slot = crl->slot;
    CK_RV crv;

    if (slot == NULL) {
	/* shouldn't happen */
	PORT_SetError( SEC_ERROR_CRL_INVALID);
	return SECFailure;
    }

    crv = PK11_DestroyTokenObject(slot,crl->pkcs11ID);
    if (crv != CKR_OK) {
        PORT_SetError( PK11_MapError(crv) );
	goto loser;
    }
    crl->slot = NULL;
    PK11_FreeSlot(slot);
loser:
    return SECSuccess;
}

/*
 * return the certificate associated with a derCert 
 */
SECItem *
PK11_FindSMimeProfile(PK11SlotInfo **slot, char *emailAddr,
				 SECItem *name, SECItem **profileTime)
{
    CK_OBJECT_CLASS smimeClass = CKO_NETSCAPE_SMIME;
    CK_ATTRIBUTE theTemplate[] = {
	{ CKA_SUBJECT, NULL, 0 },
	{ CKA_CLASS, NULL, 0 },
	{ CKA_NETSCAPE_EMAIL, NULL, 0 },
    };
    CK_ATTRIBUTE smimeData[] =  {
	{ CKA_SUBJECT, NULL, 0 },
	{ CKA_VALUE, NULL, 0 },
    };
    /* if you change the array, change the variable below as well */
    int tsize = sizeof(theTemplate)/sizeof(theTemplate[0]);
    CK_OBJECT_HANDLE smimeh = CK_INVALID_HANDLE;
    CK_ATTRIBUTE *attrs = theTemplate;
    CK_RV crv;
    SECItem *emailProfile = NULL;

    PK11_SETATTRS(attrs, CKA_SUBJECT, name->data, name->len); attrs++;
    PK11_SETATTRS(attrs, CKA_CLASS, &smimeClass, sizeof(smimeClass)); attrs++;
    PK11_SETATTRS(attrs, CKA_NETSCAPE_EMAIL, emailAddr, strlen(emailAddr)); 
								    attrs++;

    if (*slot) {
    	smimeh = pk11_FindObjectByTemplate(*slot,theTemplate,tsize);
    } else {
	PK11SlotList *list = PK11_GetAllTokens(CKM_INVALID_MECHANISM,
							PR_FALSE,PR_TRUE,NULL);
	PK11SlotListElement *le;

	/* loop through all the fortezza tokens */
	for (le = list->head; le; le = le->next) {
	    smimeh = pk11_FindObjectByTemplate(le->slot,theTemplate,tsize);
	    if (smimeh != CK_INVALID_HANDLE) {
		*slot = PK11_ReferenceSlot(le->slot);
		break;
	    }
	}
	PK11_FreeSlotList(list);
    }
    
    if (smimeh == CK_INVALID_HANDLE) {
	PORT_SetError(SEC_ERROR_NO_KRL);
	return NULL;
    }

    if (profileTime) {
    	PK11_SETATTRS(smimeData, CKA_NETSCAPE_SMIME_TIMESTAMP, NULL, 0);
    } 
    
    crv = PK11_GetAttributes(NULL,*slot,smimeh,smimeData,2);
    if (crv != CKR_OK) {
	PORT_SetError(PK11_MapError (crv));
	goto loser;
    }

    if (!profileTime) {
	SECItem profileSubject;

	profileSubject.data = smimeData[0].pValue;
	profileSubject.len = smimeData[0].ulValueLen;
	if (!SECITEM_ItemsAreEqual(&profileSubject,name)) {
	    goto loser;
	}
    }

    emailProfile = (SECItem *)PORT_ZAlloc(sizeof(SECItem));    
    if (emailProfile == NULL) {
	goto loser;
    }

    emailProfile->data = smimeData[1].pValue;
    emailProfile->len = smimeData[1].ulValueLen;

    if (profileTime) {
	*profileTime = (SECItem *)PORT_ZAlloc(sizeof(SECItem));    
	if (*profileTime) {
	    (*profileTime)->data = smimeData[0].pValue;
	    (*profileTime)->len = smimeData[0].ulValueLen;
	}
    }

loser:
    if (emailProfile == NULL) {
	if (smimeData[1].pValue) {
	    PORT_Free(smimeData[1].pValue);
	}
    }
    if (profileTime == NULL || *profileTime == NULL) {
	if (smimeData[0].pValue) {
	    PORT_Free(smimeData[0].pValue);
	}
    }
    return emailProfile;
}


SECStatus
PK11_SaveSMimeProfile(PK11SlotInfo *slot, char *emailAddr, SECItem *derSubj,
				 SECItem *emailProfile,  SECItem *profileTime)
{
    CK_OBJECT_CLASS smimeClass = CKO_NETSCAPE_SMIME;
    CK_BBOOL ck_true = CK_TRUE;
    CK_ATTRIBUTE theTemplate[] = {
	{ CKA_CLASS, NULL, 0 },
	{ CKA_TOKEN, NULL, 0 },
	{ CKA_SUBJECT, NULL, 0 },
	{ CKA_NETSCAPE_EMAIL, NULL, 0 },
	{ CKA_NETSCAPE_SMIME_TIMESTAMP, NULL, 0 },
	{ CKA_VALUE, NULL, 0 }
    };
    /* if you change the array, change the variable below as well */
    int realSize = 0;
    CK_OBJECT_HANDLE smimeh = CK_INVALID_HANDLE;
    CK_ATTRIBUTE *attrs = theTemplate;
    CK_SESSION_HANDLE rwsession;
    CK_RV crv;
#ifdef DEBUG
    int tsize = sizeof(theTemplate)/sizeof(theTemplate[0]);
#endif

    PK11_SETATTRS(attrs, CKA_CLASS, &smimeClass, sizeof(smimeClass)); attrs++;
    PK11_SETATTRS(attrs, CKA_TOKEN, &ck_true, sizeof(ck_true)); attrs++;
    PK11_SETATTRS(attrs, CKA_SUBJECT, derSubj->data, derSubj->len); attrs++;
    PK11_SETATTRS(attrs, CKA_NETSCAPE_EMAIL, 
				emailAddr, PORT_Strlen(emailAddr)+1); attrs++;
    if (profileTime) {
	PK11_SETATTRS(attrs, CKA_NETSCAPE_SMIME_TIMESTAMP, profileTime->data,
	                                            profileTime->len); attrs++;
	PK11_SETATTRS(attrs, CKA_VALUE,emailProfile->data,
	                                            emailProfile->len); attrs++;
    }
    realSize = attrs - theTemplate;
    PORT_Assert (realSize <= tsize);

    if (slot == NULL) {
	slot = PK11_GetInternalKeySlot();
	/* we need to free the key slot in the end!!! */
    }

    rwsession = PK11_GetRWSession(slot);
    if (rwsession == CK_INVALID_SESSION) {
	PORT_SetError(SEC_ERROR_READ_ONLY);
	return SECFailure;
    }

    crv = PK11_GETTAB(slot)->
                        C_CreateObject(rwsession,theTemplate,realSize,&smimeh);
    if (crv != CKR_OK) {
        PORT_SetError( PK11_MapError(crv) );
    }

    PK11_RestoreROSession(slot,rwsession);
    return SECSuccess;
}


