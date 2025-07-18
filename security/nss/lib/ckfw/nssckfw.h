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

#ifndef NSSCKFW_H
#define NSSCKFW_H

#ifdef DEBUG
static const char NSSCKFW_CVS_ID[] = "@(#) $RCSfile: nssckfw.h,v $ $Revision: 1.2.6.1 $ $Date: 2002/04/10 03:26:43 $ $Name: MOZILLA_1_0_RELEASE $";
#endif /* DEBUG */

/*
 * nssckfw.h
 *
 * This file prototypes the publicly available calls of the 
 * NSS Cryptoki Framework.
 */

#ifndef NSSBASET_H
#include "nssbaset.h"
#endif /* NSSBASET_H */

#ifndef NSSCKT_H
#include "nssckt.h"
#endif /* NSSCKT_H */

#ifndef NSSCKFWT_H
#include "nssckfwt.h"
#endif /* NSSCKFWT_H */

/*
 * NSSCKFWInstance
 *
 *  NSSCKFWInstance_GetMDInstance
 *  NSSCKFWInstance_GetArena
 *  NSSCKFWInstance_MayCreatePthreads
 *  NSSCKFWInstance_CreateMutex
 *  NSSCKFWInstance_GetConfigurationData
 */

/*
 * NSSCKFWInstance_GetMDInstance
 *
 */

NSS_EXTERN NSSCKMDInstance *
NSSCKFWInstance_GetMDInstance
(
  NSSCKFWInstance *fwInstance
);

/*
 * NSSCKFWInstance_GetArena
 *
 */

NSS_EXTERN NSSArena *
NSSCKFWInstance_GetArena
(
  NSSCKFWInstance *fwInstance,
  CK_RV *pError
);

/*
 * NSSCKFWInstance_MayCreatePthreads
 *
 */

NSS_EXTERN CK_BBOOL
NSSCKFWInstance_MayCreatePthreads
(
  NSSCKFWInstance *fwInstance
);

/*
 * NSSCKFWInstance_CreateMutex
 *
 */

NSS_EXTERN NSSCKFWMutex *
NSSCKFWInstance_CreateMutex
(
  NSSCKFWInstance *fwInstance,
  NSSArena *arena,
  CK_RV *pError
);

/*
 * NSSCKFWInstance_GetConfigurationData
 *
 */

NSS_EXTERN NSSUTF8 *
NSSCKFWInstance_GetConfigurationData
(
  NSSCKFWInstance *fwInstance
);

/*
 * NSSCKFWInstance_GetInitArgs
 *
 */

NSS_EXTERN CK_C_INITIALIZE_ARGS_PTR
NSSCKFWInstance_GetInitArgs
(
  NSSCKFWInstance *fwInstance
);

/*
 * NSSCKFWSlot
 *
 *  NSSCKFWSlot_GetMDSlot
 *  NSSCKFWSlot_GetFWInstance
 *  NSSCKFWSlot_GetMDInstance
 *
 */

/*
 * NSSCKFWSlot_GetMDSlot
 *
 */

NSS_EXTERN NSSCKMDSlot *
NSSCKFWSlot_GetMDSlot
(
  NSSCKFWSlot *fwSlot
);

/*
 * NSSCKFWSlot_GetFWInstance
 *
 */

NSS_EXTERN NSSCKFWInstance *
NSSCKFWSlot_GetFWInstance
(
  NSSCKFWSlot *fwSlot
);

/*
 * NSSCKFWSlot_GetMDInstance
 *
 */

NSS_EXTERN NSSCKMDInstance *
NSSCKFWSlot_GetMDInstance
(
  NSSCKFWSlot *fwSlot
);

/*
 * NSSCKFWToken
 *
 *  NSSCKFWToken_GetMDToken
 *  NSSCKFWToken_GetFWSlot
 *  NSSCKFWToken_GetMDSlot
 *  NSSCKFWToken_GetSessionState
 *
 */

/*
 * NSSCKFWToken_GetMDToken
 *
 */

NSS_EXTERN NSSCKMDToken *
NSSCKFWToken_GetMDToken
(
  NSSCKFWToken *fwToken
);

/*
 * NSSCKFWToken_GetArena
 *
 */

NSS_EXTERN NSSArena *
NSSCKFWToken_GetArena
(
  NSSCKFWToken *fwToken,
  CK_RV *pError
);

/*
 * NSSCKFWToken_GetFWSlot
 *
 */

NSS_EXTERN NSSCKFWSlot *
NSSCKFWToken_GetFWSlot
(
  NSSCKFWToken *fwToken
);

/*
 * NSSCKFWToken_GetMDSlot
 *
 */

NSS_EXTERN NSSCKMDSlot *
NSSCKFWToken_GetMDSlot
(
  NSSCKFWToken *fwToken
);

/*
 * NSSCKFWToken_GetSessionState
 *
 */

NSS_EXTERN CK_STATE
NSSCKFWSession_GetSessionState
(
  NSSCKFWToken *fwToken
);

/*
 * NSSCKFWMechanism
 *
 *  NSSKCFWMechanism_GetMDMechanism
 *  NSSCKFWMechanism_GetParameter
 *
 */

/*
 * NSSKCFWMechanism_GetMDMechanism
 *
 */

NSS_EXTERN NSSCKMDMechanism *
NSSCKFWMechanism_GetMDMechanism
(
  NSSCKFWMechanism *fwMechanism
);

/*
 * NSSCKFWMechanism_GetParameter
 *
 */

NSS_EXTERN NSSItem *
NSSCKFWMechanism_GetParameter
(
  NSSCKFWMechanism *fwMechanism
);

/*
 * NSSCKFWSession
 *
 *  NSSCKFWSession_GetMDSession
 *  NSSCKFWSession_GetArena
 *  NSSCKFWSession_CallNotification
 *  NSSCKFWSession_IsRWSession
 *  NSSCKFWSession_IsSO
 *
 */

/*
 * NSSCKFWSession_GetMDSession
 *
 */

NSS_EXTERN NSSCKMDSession *
NSSCKFWSession_GetMDSession
(
  NSSCKFWSession *fwSession
);

/*
 * NSSCKFWSession_GetArena
 *
 */

NSS_EXTERN NSSArena *
NSSCKFWSession_GetArena
(
  NSSCKFWSession *fwSession,
  CK_RV *pError
);

/*
 * NSSCKFWSession_CallNotification
 *
 */

NSS_EXTERN CK_RV
NSSCKFWSession_CallNotification
(
  NSSCKFWSession *fwSession,
  CK_NOTIFICATION event
);

/*
 * NSSCKFWSession_IsRWSession
 *
 */

NSS_EXTERN CK_BBOOL
NSSCKFWSession_IsRWSession
(
  NSSCKFWSession *fwSession
);

/*
 * NSSCKFWSession_IsSO
 *
 */

NSS_EXTERN CK_BBOOL
NSSCKFWSession_IsSO
(
  NSSCKFWSession *fwSession
);

/*
 * NSSCKFWObject
 *
 *  NSSCKFWObject_GetMDObject
 *  NSSCKFWObject_GetArena
 *  NSSCKFWObject_IsTokenObject
 *  NSSCKFWObject_GetAttributeCount
 *  NSSCKFWObject_GetAttributeTypes
 *  NSSCKFWObject_GetAttributeSize
 *  NSSCKFWObject_GetAttribute
 *  NSSCKFWObject_GetObjectSize
 */

/*
 * NSSCKFWObject_GetMDObject
 *
 */
NSS_EXTERN NSSCKMDObject *
NSSCKFWObject_GetMDObject
(
  NSSCKFWObject *fwObject
);

/*
 * NSSCKFWObject_GetArena
 *
 */
NSS_EXTERN NSSArena *
NSSCKFWObject_GetArena
(
  NSSCKFWObject *fwObject,
  CK_RV *pError
);

/*
 * NSSCKFWObject_IsTokenObject
 *
 */
NSS_EXTERN CK_BBOOL
NSSCKFWObject_IsTokenObject
(
  NSSCKFWObject *fwObject
);

/*
 * NSSCKFWObject_GetAttributeCount
 *
 */
NSS_EXTERN CK_ULONG
NSSCKFWObject_GetAttributeCount
(
  NSSCKFWObject *fwObject,
  CK_RV *pError
);

/*
 * NSSCKFWObject_GetAttributeTypes
 *
 */
NSS_EXTERN CK_RV
NSSCKFWObject_GetAttributeTypes
(
  NSSCKFWObject *fwObject,
  CK_ATTRIBUTE_TYPE_PTR typeArray,
  CK_ULONG ulCount
);

/*
 * NSSCKFWObject_GetAttributeSize
 *
 */
NSS_EXTERN CK_ULONG
NSSCKFWObject_GetAttributeSize
(
  NSSCKFWObject *fwObject,
  CK_ATTRIBUTE_TYPE attribute,
  CK_RV *pError
);

/*
 * NSSCKFWObject_GetAttribute
 *
 */
NSS_EXTERN NSSItem *
NSSCKFWObject_GetAttribute
(
  NSSCKFWObject *fwObject,
  CK_ATTRIBUTE_TYPE attribute,
  NSSItem *itemOpt,
  NSSArena *arenaOpt,
  CK_RV *pError
);

/*
 * NSSCKFWObject_GetObjectSize
 *
 */
NSS_EXTERN CK_ULONG
NSSCKFWObject_GetObjectSize
(
  NSSCKFWObject *fwObject,
  CK_RV *pError
);

/*
 * NSSCKFWFindObjects
 *
 *  NSSCKFWFindObjects_GetMDFindObjects
 *
 */

/*
 * NSSCKFWFindObjects_GetMDFindObjects
 *
 */

NSS_EXTERN NSSCKMDFindObjects *
NSSCKFWFindObjects_GetMDFindObjects
(
  NSSCKFWFindObjects *
);

/*
 * NSSCKFWMutex
 *
 *  NSSCKFWMutex_Destroy
 *  NSSCKFWMutex_Lock
 *  NSSCKFWMutex_Unlock
 *
 */

/*
 * NSSCKFWMutex_Destroy
 *
 */

NSS_EXTERN CK_RV
NSSCKFWMutex_Destroy
(
  NSSCKFWMutex *mutex
);

/*
 * NSSCKFWMutex_Lock
 *
 */

NSS_EXTERN CK_RV
NSSCKFWMutex_Lock
(
  NSSCKFWMutex *mutex
);

/*
 * NSSCKFWMutex_Unlock
 *
 */

NSS_EXTERN CK_RV
NSSCKFWMutex_Unlock
(
  NSSCKFWMutex *mutex
);

#endif /* NSSCKFW_H */

