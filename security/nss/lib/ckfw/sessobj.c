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

#ifdef DEBUG
static const char CVS_ID[] = "@(#) $RCSfile: sessobj.c,v $ $Revision: 1.8.16.1 $ $Date: 2002/04/10 03:26:44 $ $Name: MOZILLA_1_0_RELEASE $";
#endif /* DEBUG */

/*
 * sessobj.c
 *
 * This file contains an NSSCKMDObject implementation for session 
 * objects.  The framework uses this implementation to manage
 * session objects when a Module doesn't wish to be bothered.
 */

#ifndef CK_T
#include "ck.h"
#endif /* CK_T */

/*
 * nssCKMDSessionObject
 *
 *  -- create --
 *  nssCKMDSessionObject_Create
 *
 *  -- EPV calls --
 *  nss_ckmdSessionObject_Finalize
 *  nss_ckmdSessionObject_IsTokenObject
 *  nss_ckmdSessionObject_GetAttributeCount
 *  nss_ckmdSessionObject_GetAttributeTypes
 *  nss_ckmdSessionObject_GetAttributeSize
 *  nss_ckmdSessionObject_GetAttribute
 *  nss_ckmdSessionObject_SetAttribute
 *  nss_ckmdSessionObject_GetObjectSize
 */

struct nssCKMDSessionObjectStr {
  CK_ULONG n;
  NSSArena *arena;
  NSSItem *attributes;
  CK_ATTRIBUTE_TYPE_PTR types;
  nssCKFWHash *hash;
};
typedef struct nssCKMDSessionObjectStr nssCKMDSessionObject;

#ifdef DEBUG
/*
 * But first, the pointer-tracking stuff.
 *
 * NOTE: the pointer-tracking support in NSS/base currently relies
 * upon NSPR's CallOnce support.  That, however, relies upon NSPR's
 * locking, which is tied into the runtime.  We need a pointer-tracker
 * implementation that uses the locks supplied through C_Initialize.
 * That support, however, can be filled in later.  So for now, I'll
 * just do this routines as no-ops.
 */

static CK_RV
nss_ckmdSessionObject_add_pointer
(
  const NSSCKMDObject *mdObject
)
{
  return CKR_OK;
}

static CK_RV
nss_ckmdSessionObject_remove_pointer
(
  const NSSCKMDObject *mdObject
)
{
  return CKR_OK;
}

#ifdef NSS_DEBUG
static CK_RV
nss_ckmdSessionObject_verifyPointer
(
  const NSSCKMDObject *mdObject
)
{
  return CKR_OK;
}
#endif

#endif /* DEBUG */

/*
 * We must forward-declare these routines
 */
static void
nss_ckmdSessionObject_Finalize
(
  NSSCKMDObject *mdObject,
  NSSCKFWObject *fwObject,
  NSSCKMDSession *mdSession,
  NSSCKFWSession *fwSession,
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance
);

static CK_RV
nss_ckmdSessionObject_Destroy
(
  NSSCKMDObject *mdObject,
  NSSCKFWObject *fwObject,
  NSSCKMDSession *mdSession,
  NSSCKFWSession *fwSession,
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance
);

static CK_BBOOL
nss_ckmdSessionObject_IsTokenObject
(
  NSSCKMDObject *mdObject,
  NSSCKFWObject *fwObject,
  NSSCKMDSession *mdSession,
  NSSCKFWSession *fwSession,
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance
);

static CK_ULONG
nss_ckmdSessionObject_GetAttributeCount
(
  NSSCKMDObject *mdObject,
  NSSCKFWObject *fwObject,
  NSSCKMDSession *mdSession,
  NSSCKFWSession *fwSession,
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  CK_RV *pError
);

static CK_RV
nss_ckmdSessionObject_GetAttributeTypes
(
  NSSCKMDObject *mdObject,
  NSSCKFWObject *fwObject,
  NSSCKMDSession *mdSession,
  NSSCKFWSession *fwSession,
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  CK_ATTRIBUTE_TYPE_PTR typeArray,
  CK_ULONG ulCount
);

static CK_ULONG
nss_ckmdSessionObject_GetAttributeSize
(
  NSSCKMDObject *mdObject,
  NSSCKFWObject *fwObject,
  NSSCKMDSession *mdSession,
  NSSCKFWSession *fwSession,
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  CK_ATTRIBUTE_TYPE attribute,
  CK_RV *pError
);

static const NSSItem *
nss_ckmdSessionObject_GetAttribute
(
  NSSCKMDObject *mdObject,
  NSSCKFWObject *fwObject,
  NSSCKMDSession *mdSession,
  NSSCKFWSession *fwSession,
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  CK_ATTRIBUTE_TYPE attribute,
  CK_RV *pError
);

static CK_RV
nss_ckmdSessionObject_SetAttribute
(
  NSSCKMDObject *mdObject,
  NSSCKFWObject *fwObject,
  NSSCKMDSession *mdSession,
  NSSCKFWSession *fwSession,
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  CK_ATTRIBUTE_TYPE attribute,
  NSSItem *value
);

static CK_ULONG
nss_ckmdSessionObject_GetObjectSize
(
  NSSCKMDObject *mdObject,
  NSSCKFWObject *fwObject,
  NSSCKMDSession *mdSession,
  NSSCKFWSession *fwSession,
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  CK_RV *pError
);

/*
 * nssCKMDSessionObject_Create
 *
 */
NSS_IMPLEMENT NSSCKMDObject *
nssCKMDSessionObject_Create
(
  NSSCKFWToken *fwToken,
  NSSArena *arena,
  CK_ATTRIBUTE_PTR attributes,
  CK_ULONG ulCount,
  CK_RV *pError
)
{
  NSSCKMDObject *mdObject = (NSSCKMDObject *)NULL;
  nssCKMDSessionObject *mdso = (nssCKMDSessionObject *)NULL;
  CK_ULONG i;
  nssCKFWHash *hash;

  mdso = nss_ZNEW(arena, nssCKMDSessionObject);
  if( (nssCKMDSessionObject *)NULL == mdso ) {
    goto loser;
  }

  mdso->arena = arena;
  mdso->n = ulCount;
  mdso->attributes = nss_ZNEWARRAY(arena, NSSItem, ulCount);
  if( (NSSItem *)NULL == mdso->attributes ) {
    goto loser;
  }

  mdso->types = nss_ZNEWARRAY(arena, CK_ATTRIBUTE_TYPE, ulCount);

  for( i = 0; i < ulCount; i++ ) {
    mdso->types[i] = attributes[i].type;
    mdso->attributes[i].size = attributes[i].ulValueLen;
    mdso->attributes[i].data = nss_ZAlloc(arena, attributes[i].ulValueLen);
    if( (void *)NULL == mdso->attributes[i].data ) {
      goto loser;
    }
    (void)nsslibc_memcpy(mdso->attributes[i].data, attributes[i].pValue,
      attributes[i].ulValueLen);
  }

  mdObject = nss_ZNEW(arena, NSSCKMDObject);
  if( (NSSCKMDObject *)NULL == mdObject ) {
    goto loser;
  }

  mdObject->etc = (void *)mdso;
  mdObject->Finalize = nss_ckmdSessionObject_Finalize;
  mdObject->Destroy = nss_ckmdSessionObject_Destroy;
  mdObject->IsTokenObject = nss_ckmdSessionObject_IsTokenObject;
  mdObject->GetAttributeCount = nss_ckmdSessionObject_GetAttributeCount;
  mdObject->GetAttributeTypes = nss_ckmdSessionObject_GetAttributeTypes;
  mdObject->GetAttributeSize = nss_ckmdSessionObject_GetAttributeSize;
  mdObject->GetAttribute = nss_ckmdSessionObject_GetAttribute;
  mdObject->SetAttribute = nss_ckmdSessionObject_SetAttribute;
  mdObject->GetObjectSize = nss_ckmdSessionObject_GetObjectSize;

  hash = nssCKFWToken_GetSessionObjectHash(fwToken);
  if( (nssCKFWHash *)NULL == hash ) {
    *pError = CKR_GENERAL_ERROR;
    goto loser;
  }

  mdso->hash = hash;

  *pError = nssCKFWHash_Add(hash, mdObject, mdObject);
  if( CKR_OK != *pError ) {
    goto loser;
  }

#ifdef DEBUG
  if( CKR_OK != nss_ckmdSessionObject_add_pointer(mdObject) ) {
    goto loser;
  }
#endif /* DEBUG */

  *pError = CKR_OK;
  return mdObject;

 loser:
  if( (nssCKMDSessionObject *)NULL != mdso ) {
    if( (NSSItem *)NULL != mdso->attributes ) {
      for( i = 0; i < ulCount; i++ ) {
        nss_ZFreeIf(mdso->attributes[i].data);
      }

      nss_ZFreeIf(mdso->attributes);
    }

    nss_ZFreeIf(mdso->types);
    nss_ZFreeIf(mdso);
  }

  nss_ZFreeIf(mdObject);
  *pError = CKR_HOST_MEMORY;
  return (NSSCKMDObject *)NULL;
}

/*
 * nss_ckmdSessionObject_Finalize
 *
 */
static void
nss_ckmdSessionObject_Finalize
(
  NSSCKMDObject *mdObject,
  NSSCKFWObject *fwObject,
  NSSCKMDSession *mdSession,
  NSSCKFWSession *fwSession,
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance
)
{
  /* This shouldn't ever be called */
  return;
}

/*
 * nss_ckmdSessionObject_Destroy
 *
 */

static CK_RV
nss_ckmdSessionObject_Destroy
(
  NSSCKMDObject *mdObject,
  NSSCKFWObject *fwObject,
  NSSCKMDSession *mdSession,
  NSSCKFWSession *fwSession,
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance
)
{
#ifdef NSSDEBUG
  CK_RV error = CKR_OK;
#endif /* NSSDEBUG */
  nssCKMDSessionObject *mdso;
  CK_ULONG i;

#ifdef NSSDEBUG
  error = nss_ckmdSessionObject_verifyPointer(mdObject);
  if( CKR_OK != error ) {
    return error;
  }
#endif /* NSSDEBUG */

  mdso = (nssCKMDSessionObject *)mdObject->etc;

  nssCKFWHash_Remove(mdso->hash, mdObject);

  for( i = 0; i < mdso->n; i++ ) {
    nss_ZFreeIf(mdso->attributes[i].data);
  }
  nss_ZFreeIf(mdso->attributes);
  nss_ZFreeIf(mdso->types);
  nss_ZFreeIf(mdso);
  nss_ZFreeIf(mdObject);

#ifdef DEBUG
  (void)nss_ckmdSessionObject_remove_pointer(mdObject);
#endif /* DEBUG */

  return CKR_OK;
}

/*
 * nss_ckmdSessionObject_IsTokenObject
 *
 */

static CK_BBOOL
nss_ckmdSessionObject_IsTokenObject
(
  NSSCKMDObject *mdObject,
  NSSCKFWObject *fwObject,
  NSSCKMDSession *mdSession,
  NSSCKFWSession *fwSession,
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance
)
{
#ifdef NSSDEBUG
  if( CKR_OK != nss_ckmdSessionObject_verifyPointer(mdObject) ) {
    return CK_FALSE;
  }
#endif /* NSSDEBUG */

  /*
   * This implementation is only ever used for session objects.
   */
  return CK_FALSE;
}

/*
 * nss_ckmdSessionObject_GetAttributeCount
 *
 */
static CK_ULONG
nss_ckmdSessionObject_GetAttributeCount
(
  NSSCKMDObject *mdObject,
  NSSCKFWObject *fwObject,
  NSSCKMDSession *mdSession,
  NSSCKFWSession *fwSession,
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  CK_RV *pError
)
{
  nssCKMDSessionObject *obj;

#ifdef NSSDEBUG
  if( (CK_RV *)NULL == pError ) {
    return 0;
  }

  *pError = nss_ckmdSessionObject_verifyPointer(mdObject);
  if( CKR_OK != *pError ) {
    return 0;
  }

  /* We could even check all the other arguments, for sanity. */
#endif /* NSSDEBUG */

  obj = (nssCKMDSessionObject *)mdObject->etc;

  return obj->n;
}

/*
 * nss_ckmdSessionObject_GetAttributeTypes
 *
 */
static CK_RV
nss_ckmdSessionObject_GetAttributeTypes
(
  NSSCKMDObject *mdObject,
  NSSCKFWObject *fwObject,
  NSSCKMDSession *mdSession,
  NSSCKFWSession *fwSession,
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  CK_ATTRIBUTE_TYPE_PTR typeArray,
  CK_ULONG ulCount
)
{
#ifdef NSSDEBUG
  CK_RV error = CKR_OK;
#endif /* NSSDEBUG */
  nssCKMDSessionObject *obj;

#ifdef NSSDEBUG
  error = nss_ckmdSessionObject_verifyPointer(mdObject);
  if( CKR_OK != error ) {
    return error;
  }

  /* We could even check all the other arguments, for sanity. */
#endif /* NSSDEBUG */

  obj = (nssCKMDSessionObject *)mdObject->etc;

  if( ulCount < obj->n ) {
    return CKR_BUFFER_TOO_SMALL;
  }

  (void)nsslibc_memcpy(typeArray, obj->types, 
    sizeof(CK_ATTRIBUTE_TYPE) * obj->n);

  return CKR_OK;
}

/*
 * nss_ckmdSessionObject_GetAttributeSize
 *
 */
static CK_ULONG
nss_ckmdSessionObject_GetAttributeSize
(
  NSSCKMDObject *mdObject,
  NSSCKFWObject *fwObject,
  NSSCKMDSession *mdSession,
  NSSCKFWSession *fwSession,
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  CK_ATTRIBUTE_TYPE attribute,
  CK_RV *pError
)
{
  nssCKMDSessionObject *obj;
  CK_ULONG i;

#ifdef NSSDEBUG
  if( (CK_RV *)NULL == pError ) {
    return 0;
  }

  *pError = nss_ckmdSessionObject_verifyPointer(mdObject);
  if( CKR_OK != *pError ) {
    return 0;
  }

  /* We could even check all the other arguments, for sanity. */
#endif /* NSSDEBUG */

  obj = (nssCKMDSessionObject *)mdObject->etc;

  for( i = 0; i < obj->n; i++ ) {
    if( attribute == obj->types[i] ) {
      return (CK_ULONG)(obj->attributes[i].size);
    }
  }

  *pError = CKR_ATTRIBUTE_TYPE_INVALID;
  return 0;
}

/*
 * nss_ckmdSessionObject_GetAttribute
 *
 */
static const NSSItem *
nss_ckmdSessionObject_GetAttribute
(
  NSSCKMDObject *mdObject,
  NSSCKFWObject *fwObject,
  NSSCKMDSession *mdSession,
  NSSCKFWSession *fwSession,
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  CK_ATTRIBUTE_TYPE attribute,
  CK_RV *pError
)
{
  nssCKMDSessionObject *obj;
  CK_ULONG i;

#ifdef NSSDEBUG
  if( (CK_RV *)NULL == pError ) {
    return 0;
  }

  *pError = nss_ckmdSessionObject_verifyPointer(mdObject);
  if( CKR_OK != *pError ) {
    return 0;
  }

  /* We could even check all the other arguments, for sanity. */
#endif /* NSSDEBUG */

  obj = (nssCKMDSessionObject *)mdObject->etc;

  for( i = 0; i < obj->n; i++ ) {
    if( attribute == obj->types[i] ) {
      return &obj->attributes[i];
    }
  }

  *pError = CKR_ATTRIBUTE_TYPE_INVALID;
  return 0;
}

/*
 * nss_ckmdSessionObject_SetAttribute
 *
 */

/*
 * Okay, so this implementation sucks.  It doesn't support removing
 * an attribute (if value == NULL), and could be more graceful about
 * memory.  It should allow "blank" slots in the arrays, with some
 * invalid attribute type, and then it could support removal much
 * more easily.  Do this later.
 */
static CK_RV
nss_ckmdSessionObject_SetAttribute
(
  NSSCKMDObject *mdObject,
  NSSCKFWObject *fwObject,
  NSSCKMDSession *mdSession,
  NSSCKFWSession *fwSession,
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  CK_ATTRIBUTE_TYPE attribute,
  NSSItem *value
)
{
  nssCKMDSessionObject *obj;
  CK_ULONG i;
  NSSItem n;
  NSSItem *ra;
  CK_ATTRIBUTE_TYPE_PTR rt;
#ifdef NSSDEBUG
  CK_RV error;
#endif /* NSSDEBUG */

#ifdef NSSDEBUG
  error = nss_ckmdSessionObject_verifyPointer(mdObject);
  if( CKR_OK != error ) {
    return 0;
  }

  /* We could even check all the other arguments, for sanity. */
#endif /* NSSDEBUG */

  obj = (nssCKMDSessionObject *)mdObject->etc;

  n.size = value->size;
  n.data = nss_ZAlloc(obj->arena, n.size);
  if( (void *)NULL == n.data ) {
    return CKR_HOST_MEMORY;
  }
  (void)nsslibc_memcpy(n.data, value->data, n.size);

  for( i = 0; i < obj->n; i++ ) {
    if( attribute == obj->types[i] ) {
      nss_ZFreeIf(obj->attributes[i].data);
      obj->attributes[i] = n;
      return CKR_OK;
    }
  }

  /*
   * It's new.
   */

  ra = (NSSItem *)nss_ZRealloc(obj->attributes, sizeof(NSSItem) * (obj->n + 1));
  if( (NSSItem *)NULL == ra ) {
    nss_ZFreeIf(n.data);
    return CKR_HOST_MEMORY;
  }

  rt = (CK_ATTRIBUTE_TYPE_PTR)nss_ZRealloc(obj->types, (obj->n + 1));
  if( (CK_ATTRIBUTE_TYPE_PTR)NULL == rt ) {
    nss_ZFreeIf(n.data);
    obj->attributes = (NSSItem *)nss_ZRealloc(ra, sizeof(NSSItem) * obj->n);
    if( (NSSItem *)NULL == obj->attributes ) {
      return CKR_GENERAL_ERROR;
    }
    return CKR_HOST_MEMORY;
  }

  obj->attributes = ra;
  obj->types = rt;
  obj->attributes[obj->n] = n;
  obj->types[obj->n] = attribute;
  obj->n++;

  return CKR_OK;
}

/*
 * nss_ckmdSessionObject_GetObjectSize
 *
 */
static CK_ULONG
nss_ckmdSessionObject_GetObjectSize
(
  NSSCKMDObject *mdObject,
  NSSCKFWObject *fwObject,
  NSSCKMDSession *mdSession,
  NSSCKFWSession *fwSession,
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  CK_RV *pError
)
{
  nssCKMDSessionObject *obj;
  CK_ULONG i;
  CK_ULONG rv = (CK_ULONG)0;

#ifdef NSSDEBUG
  if( (CK_RV *)NULL == pError ) {
    return 0;
  }

  *pError = nss_ckmdSessionObject_verifyPointer(mdObject);
  if( CKR_OK != *pError ) {
    return 0;
  }

  /* We could even check all the other arguments, for sanity. */
#endif /* NSSDEBUG */

  obj = (nssCKMDSessionObject *)mdObject->etc;

  for( i = 0; i < obj->n; i++ ) {
    rv += obj->attributes[i].size;
  }

  rv += sizeof(NSSItem) * obj->n;
  rv += sizeof(CK_ATTRIBUTE_TYPE) * obj->n;
  rv += sizeof(nssCKMDSessionObject);

  return rv;
}

/*
 * nssCKMDFindSessionObjects
 *
 *  -- create --
 *  nssCKMDFindSessionObjects_Create
 *
 *  -- EPV calls --
 *  nss_ckmdFindSessionObjects_Final
 *  nss_ckmdFindSessionObjects_Next
 */

struct nodeStr {
  struct nodeStr *next;
  NSSCKMDObject *mdObject;
};

struct nssCKMDFindSessionObjectsStr {
  NSSArena *arena;
  CK_RV error;
  CK_ATTRIBUTE_PTR pTemplate;
  CK_ULONG ulCount;
  struct nodeStr *list;
  nssCKFWHash *hash;

};
typedef struct nssCKMDFindSessionObjectsStr nssCKMDFindSessionObjects;

#ifdef DEBUG
/*
 * But first, the pointer-tracking stuff.
 *
 * NOTE: the pointer-tracking support in NSS/base currently relies
 * upon NSPR's CallOnce support.  That, however, relies upon NSPR's
 * locking, which is tied into the runtime.  We need a pointer-tracker
 * implementation that uses the locks supplied through C_Initialize.
 * That support, however, can be filled in later.  So for now, I'll
 * just do this routines as no-ops.
 */

static CK_RV
nss_ckmdFindSessionObjects_add_pointer
(
  const NSSCKMDFindObjects *mdFindObjects
)
{
  return CKR_OK;
}

static CK_RV
nss_ckmdFindSessionObjects_remove_pointer
(
  const NSSCKMDFindObjects *mdFindObjects
)
{
  return CKR_OK;
}

#ifdef NSS_DEBUG
static CK_RV
nss_ckmdFindSessionObjects_verifyPointer
(
  const NSSCKMDFindObjects *mdFindObjects
)
{
  return CKR_OK;
}
#endif

#endif /* DEBUG */

/*
 * We must forward-declare these routines.
 */
static void
nss_ckmdFindSessionObjects_Final
(
  NSSCKMDFindObjects *mdFindObjects,
  NSSCKFWFindObjects *fwFindObjects,
  NSSCKMDSession *mdSession,
  NSSCKFWSession *fwSession,
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance
);

static NSSCKMDObject *
nss_ckmdFindSessionObjects_Next
(
  NSSCKMDFindObjects *mdFindObjects,
  NSSCKFWFindObjects *fwFindObjects,
  NSSCKMDSession *mdSession,
  NSSCKFWSession *fwSession,
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  NSSArena *arena,
  CK_RV *pError
);

static CK_BBOOL
items_match
(
  NSSItem *a,
  CK_VOID_PTR pValue,
  CK_ULONG ulValueLen
)
{
  if( a->size != ulValueLen ) {
    return CK_FALSE;
  }

  if( PR_TRUE == nsslibc_memequal(a->data, pValue, ulValueLen, (PRStatus *)NULL) ) {
    return CK_TRUE;
  } else {
    return CK_FALSE;
  }
}

/*
 * Our hashtable iterator
 */
static void
findfcn
(
  const void *key,
  void *value,
  void *closure
)
{
  NSSCKMDObject *mdObject = (NSSCKMDObject *)value;
  nssCKMDSessionObject *mdso = (nssCKMDSessionObject *)mdObject->etc;
  nssCKMDFindSessionObjects *mdfso = (nssCKMDFindSessionObjects *)closure;
  CK_ULONG i, j;
  struct nodeStr *node;

  if( CKR_OK != mdfso->error ) {
    return;
  }

  for( i = 0; i < mdfso->ulCount; i++ ) {
    CK_ATTRIBUTE_PTR p = &mdfso->pTemplate[i];

    for( j = 0; j < mdso->n; j++ ) {
      if( mdso->types[j] == p->type ) {
        if( !items_match(&mdso->attributes[j], p->pValue, p->ulValueLen) ) {
          return;
        } else {
          break;
        }
      }
    }

    if( j == mdso->n ) {
      /* Attribute not found */
      return;
    }
  }

  /* Matches */
  node = nss_ZNEW(mdfso->arena, struct nodeStr);
  if( (struct nodeStr *)NULL == node ) {
    mdfso->error = CKR_HOST_MEMORY;
    return;
  }

  node->mdObject = mdObject;
  node->next = mdfso->list;
  mdfso->list = node;

  return;
}

/*
 * nssCKMDFindSessionObjects_Create
 *
 */
NSS_IMPLEMENT NSSCKMDFindObjects *
nssCKMDFindSessionObjects_Create
(
  NSSCKFWToken *fwToken,
  CK_ATTRIBUTE_PTR pTemplate,
  CK_ULONG ulCount,
  CK_RV *pError
)
{
  NSSArena *arena;
  nssCKMDFindSessionObjects *mdfso;
  nssCKFWHash *hash;
  NSSCKMDFindObjects *rv;

#ifdef NSSDEBUG
  if( (CK_RV *)NULL == pError ) {
    return (NSSCKMDFindObjects *)NULL;
  }

  *pError = nssCKFWToken_verifyPointer(fwToken);
  if( CKR_OK != *pError ) {
    return (NSSCKMDFindObjects *)NULL;
  }

  if( (CK_ATTRIBUTE_PTR)NULL == pTemplate ) {
    *pError = CKR_ARGUMENTS_BAD;
    return (NSSCKMDFindObjects *)NULL;
  }
#endif /* NSSDEBUG */

  hash = nssCKFWToken_GetSessionObjectHash(fwToken);
  if( (nssCKFWHash *)NULL == hash ) {
    *pError= CKR_GENERAL_ERROR;
    return (NSSCKMDFindObjects *)NULL;
  }

  arena = NSSArena_Create();
  if( (NSSArena *)NULL == arena ) {
    *pError = CKR_HOST_MEMORY;
    return (NSSCKMDFindObjects *)NULL;
  }

  mdfso = nss_ZNEW(arena, nssCKMDFindSessionObjects);
  if( (nssCKMDFindSessionObjects *)NULL == mdfso ) {
    NSSArena_Destroy(arena);
    *pError = CKR_HOST_MEMORY;
    return (NSSCKMDFindObjects *)NULL;
  }

  rv = nss_ZNEW(arena, NSSCKMDFindObjects);

  mdfso->error = CKR_OK;
  mdfso->pTemplate = pTemplate;
  mdfso->ulCount = ulCount;
  mdfso->hash = hash;

  nssCKFWHash_Iterate(hash, findfcn, mdfso);

  if( CKR_OK != mdfso->error ) {
    NSSArena_Destroy(arena);
    *pError = CKR_HOST_MEMORY;
    return (NSSCKMDFindObjects *)NULL;
  }

  rv->etc = (void *)mdfso;
  rv->Final = nss_ckmdFindSessionObjects_Final;
  rv->Next = nss_ckmdFindSessionObjects_Next;

#ifdef DEBUG
  if( (*pError = nss_ckmdFindSessionObjects_add_pointer(rv)) != CKR_OK ) {
    NSSArena_Destroy(arena);
    return (NSSCKMDFindObjects *)NULL;
  }
#endif /* DEBUG */    
  mdfso->arena = arena;

  return rv;
}

static void
nss_ckmdFindSessionObjects_Final
(
  NSSCKMDFindObjects *mdFindObjects,
  NSSCKFWFindObjects *fwFindObjects,
  NSSCKMDSession *mdSession,
  NSSCKFWSession *fwSession,
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance
)
{
  nssCKMDFindSessionObjects *mdfso;

#ifdef NSSDEBUG
  if( CKR_OK != nss_ckmdFindSessionObjects_verifyPointer(mdFindObjects) ) {
    return;
  }
#endif /* NSSDEBUG */

  mdfso = (nssCKMDFindSessionObjects *)mdFindObjects->etc;
  if (mdfso->arena) NSSArena_Destroy(mdfso->arena);

#ifdef DEBUG
  (void)nss_ckmdFindSessionObjects_remove_pointer(mdFindObjects);
#endif /* DEBUG */

  return;
}

static NSSCKMDObject *
nss_ckmdFindSessionObjects_Next
(
  NSSCKMDFindObjects *mdFindObjects,
  NSSCKFWFindObjects *fwFindObjects,
  NSSCKMDSession *mdSession,
  NSSCKFWSession *fwSession,
  NSSCKMDToken *mdToken,
  NSSCKFWToken *fwToken,
  NSSCKMDInstance *mdInstance,
  NSSCKFWInstance *fwInstance,
  NSSArena *arena,
  CK_RV *pError
)
{
  nssCKMDFindSessionObjects *mdfso;
  NSSCKMDObject *rv = (NSSCKMDObject *)NULL;

#ifdef NSSDEBUG
  if( CKR_OK != nss_ckmdFindSessionObjects_verifyPointer(mdFindObjects) ) {
    return (NSSCKMDObject *)NULL;
  }
#endif /* NSSDEBUG */

  mdfso = (nssCKMDFindSessionObjects *)mdFindObjects->etc;

  while( (NSSCKMDObject *)NULL == rv ) {
    if( (struct nodeStr *)NULL == mdfso->list ) {
      *pError = CKR_OK;
      return (NSSCKMDObject *)NULL;
    }

    if( nssCKFWHash_Exists(mdfso->hash, mdfso->list->mdObject) ) {
      rv = mdfso->list->mdObject;
    }

    mdfso->list = mdfso->list->next;
  }

  return rv;
}
