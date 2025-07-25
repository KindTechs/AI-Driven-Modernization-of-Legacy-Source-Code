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
 * Certificate Extensions handling code
 *
 */

#include "cert.h"
#include "secitem.h"
#include "secoid.h"
#include "secder.h"
#include "secasn1.h"
#include "certxutl.h"
#include "secerr.h"

#ifdef OLD
#include "ocspti.h"	/* XXX a better extensions interface would not
			 * require knowledge of data structures of callers */
#endif

static CERTCertExtension *
GetExtension (CERTCertExtension **extensions, SECItem *oid)
{
    CERTCertExtension **exts;
    CERTCertExtension *ext = NULL;
    SECComparison comp;

    exts = extensions;
    
    if (exts) {
	while ( *exts ) {
	    ext = *exts;
	    comp = SECITEM_CompareItem(oid, &ext->id);
	    if ( comp == SECEqual ) 
		break;

	    exts++;
	}
	return (*exts ? ext : NULL);
    }
    return (NULL);
}

SECStatus
cert_FindExtensionByOID (CERTCertExtension **extensions, SECItem *oid, SECItem *value)
{
    CERTCertExtension *ext;
    SECStatus rv = SECSuccess;
    
    ext = GetExtension (extensions, oid);
    if (ext == NULL) {
	PORT_SetError (SEC_ERROR_EXTENSION_NOT_FOUND);
	return (SECFailure);
    }
    if (value)
	rv = SECITEM_CopyItem(NULL, value, &ext->value);
    return (rv);
}
    

SECStatus
CERT_GetExtenCriticality (CERTCertExtension **extensions, int tag, PRBool *isCritical)
{
    CERTCertExtension *ext;
    SECOidData *oid;

    if (!isCritical)
	return (SECSuccess);
    
    /* find the extension in the extensions list */
    oid = SECOID_FindOIDByTag((SECOidTag)tag);
    if ( !oid ) {
	return(SECFailure);
    }
    ext = GetExtension (extensions, &oid->oid);
    if (ext == NULL) {
	PORT_SetError (SEC_ERROR_EXTENSION_NOT_FOUND);
	return (SECFailure);
    }

    /* If the criticality is omitted, then it is false by default.
       ex->critical.data is NULL */
    if (ext->critical.data == NULL)
	*isCritical = PR_FALSE;
    else
	*isCritical = (ext->critical.data[0] == 0xff) ? PR_TRUE : PR_FALSE;
    return (SECSuccess);    
}

SECStatus
cert_FindExtension(CERTCertExtension **extensions, int tag, SECItem *value)
{
    SECOidData *oid;
    
    oid = SECOID_FindOIDByTag((SECOidTag)tag);
    if ( !oid ) {
	return(SECFailure);
    }

    return(cert_FindExtensionByOID(extensions, &oid->oid, value));
}


typedef struct _extNode {
    struct _extNode *next;
    CERTCertExtension *ext;
} extNode;

typedef struct {
    void (*setExts)(void *object, CERTCertExtension **exts);
    void *object;
    PRArenaPool *ownerArena;
    PRArenaPool *arena;
    extNode *head;
    int count;
}extRec;

/*
 * cert_StartExtensions
 *
 * NOTE: This interface changed significantly to remove knowledge
 *   about callers data structures (owner objects)
 */
void *
cert_StartExtensions(void *owner, PRArenaPool *ownerArena,
   void (*setExts)(void *object, CERTCertExtension **exts))
{
    PRArenaPool *arena;
    extRec *handle;
    
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if ( !arena ) {
	return(0);
    }

    handle = (extRec *)PORT_ArenaAlloc(arena, sizeof(extRec));
    if ( !handle ) {
	PORT_FreeArena(arena, PR_FALSE);
	return(0);
    }

    handle->object = owner;
    handle->ownerArena = ownerArena;
    handle->setExts = setExts;

    handle->arena = arena;
    handle->head = 0;
    handle->count = 0;
    
    return(handle);
}

static unsigned char hextrue = 0xff;

/*
 * Note - assumes that data pointed to by oid->data will not move
 */
SECStatus
CERT_AddExtensionByOID (void *exthandle, SECItem *oid, SECItem *value,
			PRBool critical, PRBool copyData)
{
    CERTCertExtension *ext;
    SECStatus rv;
    extNode *node;
    extRec *handle;
    
    handle = (extRec *)exthandle;

    /* allocate space for extension and list node */
    ext = (CERTCertExtension*)PORT_ArenaZAlloc(handle->ownerArena,
                                               sizeof(CERTCertExtension));
    if ( !ext ) {
	return(SECFailure);
    }

    node = (extNode*)PORT_ArenaAlloc(handle->arena, sizeof(extNode));
    if ( !node ) {
	return(SECFailure);
    }

    /* add to list */
    node->next = handle->head;
    handle->head = node;
   
    /* point to ext struct */
    node->ext = ext;
    
    /* the object ID of the extension */
    ext->id = *oid;
    
    /* set critical field */
    if ( critical ) {
	ext->critical.data = (unsigned char*)&hextrue;
	ext->critical.len = 1;
    }

    /* set the value */
    if ( copyData ) {
	rv = SECITEM_CopyItem(handle->ownerArena, &ext->value, value);
	if ( rv ) {
	    return(SECFailure);
	}
    } else {
	ext->value = *value;
    }
    
    handle->count++;
    
    return(SECSuccess);

}

SECStatus
CERT_AddExtension(void *exthandle, int idtag, SECItem *value,
		     PRBool critical, PRBool copyData)
{
    SECOidData *oid;
    
    oid = SECOID_FindOIDByTag((SECOidTag)idtag);
    if ( !oid ) {
	return(SECFailure);
    }

    return(CERT_AddExtensionByOID(exthandle, &oid->oid, value, critical, copyData));
}

SECStatus
CERT_EncodeAndAddExtension(void *exthandle, int idtag, void *value,
			   PRBool critical, const SEC_ASN1Template *atemplate)
{
    extRec *handle;
    SECItem *encitem;

    handle = (extRec *)exthandle;

    encitem = SEC_ASN1EncodeItem(handle->ownerArena, NULL, value, atemplate);
    if ( encitem == NULL ) {
	return(SECFailure);
    }

    return CERT_AddExtension(exthandle, idtag, encitem, critical, PR_FALSE);
}

void
PrepareBitStringForEncoding (SECItem *bitsmap, SECItem *value)
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

SECStatus
CERT_EncodeAndAddBitStrExtension (void *exthandle, int idtag,
				  SECItem *value, PRBool critical)
{
  SECItem bitsmap;
  
  PrepareBitStringForEncoding (&bitsmap, value);
  return (CERT_EncodeAndAddExtension
	  (exthandle, idtag, &bitsmap, critical, SEC_BitStringTemplate));
}

SECStatus
CERT_FinishExtensions(void *exthandle)
{
    extRec *handle;
    extNode *node;
    CERTCertExtension **exts;
    SECStatus rv = SECFailure;
    
    handle = (extRec *)exthandle;

    /* allocate space for extensions array */
    exts = PORT_ArenaNewArray(handle->ownerArena, CERTCertExtension *,
			      handle->count + 1);
    if (exts == NULL) {
	goto loser;
    }

    /* put extensions in owner object and update its version number */

#ifdef OLD
    switch (handle->type) {
      case CertificateExtensions:
	handle->owner.cert->extensions = exts;
	DER_SetUInteger (ownerArena, &(handle->owner.cert->version),
			 SEC_CERTIFICATE_VERSION_3);
	break;
      case CrlExtensions:
	handle->owner.crl->extensions = exts;
	DER_SetUInteger (ownerArena, &(handle->owner.crl->version),
			 SEC_CRL_VERSION_2);
	break;
      case OCSPRequestExtensions:
	handle->owner.request->tbsRequest->requestExtensions = exts;
	break;
      case OCSPSingleRequestExtensions:
	handle->owner.singleRequest->singleRequestExtensions = exts;	
	break;
      case OCSPResponseSingleExtensions:
	handle->owner.singleResponse->singleExtensions = exts;	
	break;
    }
#endif

    handle->setExts(handle->object, exts);
	
    /* update the version number */

    /* copy each extension pointer */
    node = handle->head;
    while ( node ) {
	*exts = node->ext;
	
	node = node->next;
	exts++;
    }

    /* terminate the array of extensions */
    *exts = 0;

    rv = SECSuccess;

loser:
    /* free working arena */
    PORT_FreeArena(handle->arena, PR_FALSE);
    return rv;
}

/*
 * get the value of the Netscape Certificate Type Extension
 */
SECStatus
CERT_FindBitStringExtension (CERTCertExtension **extensions, int tag,
			     SECItem *retItem)
{
    SECItem wrapperItem, tmpItem = {siBuffer,0};
    SECStatus rv;
    PRArenaPool *arena = NULL;
    
    wrapperItem.data = NULL;
    tmpItem.data = NULL;
    
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    
    if ( ! arena ) {
	return(SECFailure);
    }
    
    rv = cert_FindExtension(extensions, tag, &wrapperItem);
    if ( rv != SECSuccess ) {
	goto loser;
    }

    rv = SEC_ASN1DecodeItem(arena, &tmpItem, SEC_BitStringTemplate, 
			    &wrapperItem);

    if ( rv != SECSuccess ) {
	goto loser;
    }

    retItem->data = (unsigned char *)PORT_Alloc( ( tmpItem.len + 7 ) >> 3 );
    if ( retItem->data == NULL ) {
	goto loser;
    }
    
    PORT_Memcpy(retItem->data, tmpItem.data, ( tmpItem.len + 7 ) >> 3);
    retItem->len = tmpItem.len;
    
    rv = SECSuccess;
    goto done;
    
loser:
    rv = SECFailure;

done:
    if ( arena ) {
	PORT_FreeArena(arena, PR_FALSE);
    }
    
    if ( wrapperItem.data ) {
	PORT_Free(wrapperItem.data);
    }

    return(rv);
}

PRBool
cert_HasCriticalExtension (CERTCertExtension **extensions)
{
    CERTCertExtension **exts;
    CERTCertExtension *ext = NULL;
    PRBool hasCriticalExten = PR_FALSE;
    
    exts = extensions;
    
    if (exts) {
	while ( *exts ) {
	    ext = *exts;
	    /* If the criticality is omitted, it's non-critical */
	    if (ext->critical.data && ext->critical.data[0] == 0xff) {
		hasCriticalExten = PR_TRUE;
		break;
	    }
	    exts++;
	}
    }
    return (hasCriticalExten);
}

PRBool
cert_HasUnknownCriticalExten (CERTCertExtension **extensions)
{
    CERTCertExtension **exts;
    CERTCertExtension *ext = NULL;
    PRBool hasUnknownCriticalExten = PR_FALSE;
    
    exts = extensions;
    
    if (exts) {
	while ( *exts ) {
	    ext = *exts;
	    /* If the criticality is omitted, it's non-critical.
	       If an extension is critical, make sure that we know
	       how to process the extension.
             */
	    if (ext->critical.data && ext->critical.data[0] == 0xff) {
		if (SECOID_KnownCertExtenOID (&ext->id) == PR_FALSE) {
		    hasUnknownCriticalExten = PR_TRUE;
		    break;
		}
	    }
	    exts++;
	}
    }
    return (hasUnknownCriticalExten);
}
