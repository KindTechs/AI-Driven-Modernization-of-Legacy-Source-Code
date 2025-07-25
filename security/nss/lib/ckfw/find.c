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
static const char CVS_ID[] = "@(#) $RCSfile: find.c,v $ $Revision: 1.3.16.1 $ $Date: 2002/04/10 03:26:41 $ $Name: MOZILLA_1_0_RELEASE $";
#endif /* DEBUG */

/*
 * find.c
 *
 * This file implements the nssCKFWFindObjects type and methods.
 */

#ifndef CK_H
#include "ck.h"
#endif /* CK_H */

/*
 * NSSCKFWFindObjects
 *
 *  -- create/destroy --
 *  nssCKFWFindObjects_Create
 *  nssCKFWFindObjects_Destroy
 *
 *  -- public accessors --
 *  NSSCKFWFindObjects_GetMDFindObjects
 * 
 *  -- implement public accessors --
 *  nssCKFWFindObjects_GetMDFindObjects
 *
 *  -- private accessors --
 *
 *  -- module fronts --
 *  nssCKFWFindObjects_Next
 */

struct NSSCKFWFindObjectsStr {
  NSSCKFWMutex *mutex; /* merely to serialise the MDObject calls */
  NSSCKMDFindObjects *mdfo1;
  NSSCKMDFindObjects *mdfo2;
  NSSCKFWSession *fwSession;
  NSSCKMDSession *mdSession;
  NSSCKFWToken *fwToken;
  NSSCKMDToken *mdToken;
  NSSCKFWInstance *fwInstance;
  NSSCKMDInstance *mdInstance;

  NSSCKMDFindObjects *mdFindObjects; /* varies */
};

#ifdef DEBUG
/*
 * But first, the pointer-tracking stuff.
 *
 * NOTE: the pointer-tracking support in NSS/base currently relies
 * upon NSPR's CallOnce support.  That, however, relies upon NSPR's
 * locking, which is tied into the runtime.  We need a pointer-tracker
 * implementation that uses the locks supplied through C_Initialize.
 * That support, however, can be filled in later.  So for now, I'll
 * just do these routines as no-ops.
 */

static CK_RV
findObjects_add_pointer
(
  const NSSCKFWFindObjects *fwFindObjects
)
{
  return CKR_OK;
}

static CK_RV
findObjects_remove_pointer
(
  const NSSCKFWFindObjects *fwFindObjects
)
{
  return CKR_OK;
}

NSS_IMPLEMENT CK_RV
nssCKFWFindObjects_verifyPointer
(
  const NSSCKFWFindObjects *fwFindObjects
)
{
  return CKR_OK;
}

#endif /* DEBUG */

/*
 * nssCKFWFindObjects_Create
 *
 */
NSS_EXTERN NSSCKFWFindObjects *
nssCKFWFindObjects_Create
(
  NSSCKFWSession *fwSession,
  NSSCKFWToken *fwToken,
  NSSCKFWInstance *fwInstance,
  NSSCKMDFindObjects *mdFindObjects1,
  NSSCKMDFindObjects *mdFindObjects2,
  CK_RV *pError
)
{
  NSSCKFWFindObjects *fwFindObjects = NULL;
  NSSArena *arena;
  NSSCKMDSession *mdSession;
  NSSCKMDToken *mdToken;
  NSSCKMDInstance *mdInstance;

  mdSession = nssCKFWSession_GetMDSession(fwSession);
  mdToken = nssCKFWToken_GetMDToken(fwToken);
  mdInstance = nssCKFWInstance_GetMDInstance(fwInstance);

#ifdef notdef
  arena = nssCKFWSession_GetArena(fwSession, pError);
  if( (NSSArena *)NULL == arena ) {
    goto loser;
  }
#endif

  fwFindObjects = nss_ZNEW(NULL, NSSCKFWFindObjects);
  if( (NSSCKFWFindObjects *)NULL == fwFindObjects ) {
    *pError = CKR_HOST_MEMORY;
    goto loser;
  }

  fwFindObjects->mdfo1 = mdFindObjects1;
  fwFindObjects->mdfo2 = mdFindObjects2;
  fwFindObjects->fwSession = fwSession;
  fwFindObjects->mdSession = mdSession;
  fwFindObjects->fwToken = fwToken;
  fwFindObjects->mdToken = mdToken;
  fwFindObjects->fwInstance = fwInstance;
  fwFindObjects->mdInstance = mdInstance;

  fwFindObjects->mutex = nssCKFWInstance_CreateMutex(fwInstance, NULL, pError);
  if( (NSSCKFWMutex *)NULL == fwFindObjects->mutex ) {
    goto loser;
  }

#ifdef DEBUG
  *pError = findObjects_add_pointer(fwFindObjects);
  if( CKR_OK != *pError ) {
    goto loser;
  }
#endif /* DEBUG */

  return fwFindObjects;

 loser:
  nss_ZFreeIf(fwFindObjects);

  if( (NSSCKMDFindObjects *)NULL != mdFindObjects1 ) {
    if( (void *)NULL != (void *)mdFindObjects1->Final ) {
      fwFindObjects->mdFindObjects = mdFindObjects1;
      mdFindObjects1->Final(mdFindObjects1, fwFindObjects, mdSession, 
        fwSession, mdToken, fwToken, mdInstance, fwInstance);
    }
  }

  if( (NSSCKMDFindObjects *)NULL != mdFindObjects2 ) {
    if( (void *)NULL != (void *)mdFindObjects2->Final ) {
      fwFindObjects->mdFindObjects = mdFindObjects2;
      mdFindObjects2->Final(mdFindObjects2, fwFindObjects, mdSession, 
        fwSession, mdToken, fwToken, mdInstance, fwInstance);
    }
  }

  if( CKR_OK == *pError ) {
    *pError = CKR_GENERAL_ERROR;
  }

  return (NSSCKFWFindObjects *)NULL;
}


/*
 * nssCKFWFindObjects_Destroy
 *
 */
NSS_EXTERN void
nssCKFWFindObjects_Destroy
(
  NSSCKFWFindObjects *fwFindObjects
)
{
#ifdef NSSDEBUG
  if( CKR_OK != nssCKFWFindObjects_verifyPointer(fwFindObjects) ) {
    return;
  }
#endif /* NSSDEBUG */

  (void)nssCKFWMutex_Destroy(fwFindObjects->mutex);

  if( (NSSCKMDFindObjects *)NULL != fwFindObjects->mdfo1 ) {
    if( (void *)NULL != (void *)fwFindObjects->mdfo1->Final ) {
      fwFindObjects->mdFindObjects = fwFindObjects->mdfo1;
      fwFindObjects->mdfo1->Final(fwFindObjects->mdfo1, fwFindObjects,
        fwFindObjects->mdSession, fwFindObjects->fwSession, 
        fwFindObjects->mdToken, fwFindObjects->fwToken,
        fwFindObjects->mdInstance, fwFindObjects->fwInstance);
    }
  }

  if( (NSSCKMDFindObjects *)NULL != fwFindObjects->mdfo2 ) {
    if( (void *)NULL != (void *)fwFindObjects->mdfo2->Final ) {
      fwFindObjects->mdFindObjects = fwFindObjects->mdfo2;
      fwFindObjects->mdfo2->Final(fwFindObjects->mdfo2, fwFindObjects,
        fwFindObjects->mdSession, fwFindObjects->fwSession, 
        fwFindObjects->mdToken, fwFindObjects->fwToken,
        fwFindObjects->mdInstance, fwFindObjects->fwInstance);
    }
  }

  nss_ZFreeIf(fwFindObjects);

#ifdef DEBUG
  (void)findObjects_remove_pointer(fwFindObjects);
#endif /* DEBUG */

  return;
}

/*
 * nssCKFWFindObjects_GetMDFindObjects
 *
 */
NSS_EXTERN NSSCKMDFindObjects *
nssCKFWFindObjects_GetMDFindObjects
(
  NSSCKFWFindObjects *fwFindObjects
)
{
#ifdef NSSDEBUG
  if( CKR_OK != nssCKFWFindObjects_verifyPointer(fwFindObjects) ) {
    return (NSSCKMDFindObjects *)NULL;
  }
#endif /* NSSDEBUG */

  return fwFindObjects->mdFindObjects;
}

/*
 * nssCKFWFindObjects_Next
 *
 */
NSS_EXTERN NSSCKFWObject *
nssCKFWFindObjects_Next
(
  NSSCKFWFindObjects *fwFindObjects,
  NSSArena *arenaOpt,
  CK_RV *pError
)
{
  NSSCKMDObject *mdObject;
  NSSCKFWObject *fwObject = (NSSCKFWObject *)NULL;
  NSSArena *objArena;

#ifdef NSSDEBUG
  if( (CK_RV *)NULL == pError ) {
    return (NSSCKFWObject *)NULL;
  }

  *pError = nssCKFWFindObjects_verifyPointer(fwFindObjects);
  if( CKR_OK != *pError ) {
    return (NSSCKFWObject *)NULL;
  }
#endif /* NSSDEBUG */

  *pError = nssCKFWMutex_Lock(fwFindObjects->mutex);
  if( CKR_OK != *pError ) {
    return (NSSCKFWObject *)NULL;
  }

  if( (NSSCKMDFindObjects *)NULL != fwFindObjects->mdfo1 ) {
    if( (void *)NULL != (void *)fwFindObjects->mdfo1->Next ) {
      fwFindObjects->mdFindObjects = fwFindObjects->mdfo1;
      mdObject = fwFindObjects->mdfo1->Next(fwFindObjects->mdfo1,
        fwFindObjects, fwFindObjects->mdSession, fwFindObjects->fwSession,
        fwFindObjects->mdToken, fwFindObjects->fwToken, 
        fwFindObjects->mdInstance, fwFindObjects->fwInstance,
        arenaOpt, pError);
      if( (NSSCKMDObject *)NULL == mdObject ) {
        if( CKR_OK != *pError ) {
          goto done;
        }

        /* All done. */
        fwFindObjects->mdfo1->Final(fwFindObjects->mdfo1, fwFindObjects,
          fwFindObjects->mdSession, fwFindObjects->fwSession,
          fwFindObjects->mdToken, fwFindObjects->fwToken, 
          fwFindObjects->mdInstance, fwFindObjects->fwInstance);
        fwFindObjects->mdfo1 = (NSSCKMDFindObjects *)NULL;
      } else {
        goto wrap;
      }
    }
  }

  if( (NSSCKMDFindObjects *)NULL != fwFindObjects->mdfo2 ) {
    if( (void *)NULL != (void *)fwFindObjects->mdfo2->Next ) {
      fwFindObjects->mdFindObjects = fwFindObjects->mdfo2;
      mdObject = fwFindObjects->mdfo2->Next(fwFindObjects->mdfo2,
        fwFindObjects, fwFindObjects->mdSession, fwFindObjects->fwSession,
        fwFindObjects->mdToken, fwFindObjects->fwToken, 
        fwFindObjects->mdInstance, fwFindObjects->fwInstance,
        arenaOpt, pError);
      if( (NSSCKMDObject *)NULL == mdObject ) {
        if( CKR_OK != *pError ) {
          goto done;
        }

        /* All done. */
        fwFindObjects->mdfo2->Final(fwFindObjects->mdfo2, fwFindObjects,
          fwFindObjects->mdSession, fwFindObjects->fwSession,
          fwFindObjects->mdToken, fwFindObjects->fwToken, 
          fwFindObjects->mdInstance, fwFindObjects->fwInstance);
        fwFindObjects->mdfo2 = (NSSCKMDFindObjects *)NULL;
      } else {
        goto wrap;
      }
    }
  }
  
  /* No more objects */
  *pError = CKR_OK;
  goto done;

 wrap:
  /*
   * This is less than ideal-- we should determine if it's a token
   * object or a session object, and use the appropriate arena.
   * But that duplicates logic in nssCKFWObject_IsTokenObject.
   * Worry about that later.  For now, be conservative, and use
   * the token arena.
   */
  objArena = nssCKFWToken_GetArena(fwFindObjects->fwToken, pError);
  if( (NSSArena *)NULL == objArena ) {
    if( CKR_OK == *pError ) {
      *pError = CKR_HOST_MEMORY;
    }
    goto done;
  }

  fwObject = nssCKFWObject_Create(objArena, mdObject,
               fwFindObjects->fwSession, fwFindObjects->fwToken, 
               fwFindObjects->fwInstance, pError);
  if( (NSSCKFWObject *)NULL == fwObject ) {
    if( CKR_OK == *pError ) {
      *pError = CKR_GENERAL_ERROR;
    }
  }

 done:
  (void)nssCKFWMutex_Unlock(fwFindObjects->mutex);
  return fwObject;
}

/*
 * NSSCKFWFindObjects_GetMDFindObjects
 *
 */

NSS_EXTERN NSSCKMDFindObjects *
NSSCKFWFindObjects_GetMDFindObjects
(
  NSSCKFWFindObjects *fwFindObjects
)
{
#ifdef DEBUG
  if( CKR_OK != nssCKFWFindObjects_verifyPointer(fwFindObjects) ) {
    return (NSSCKMDFindObjects *)NULL;
  }
#endif /* DEBUG */

  return nssCKFWFindObjects_GetMDFindObjects(fwFindObjects);
}
