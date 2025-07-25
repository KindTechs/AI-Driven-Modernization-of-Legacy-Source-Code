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
static const char CVS_ID[] = "@(#) $RCSfile: mechanism.c,v $ $Revision: 1.1.144.1 $ $Date: 2002/04/10 03:26:42 $ $Name: MOZILLA_1_0_RELEASE $";
#endif /* DEBUG */

/*
 * mechanism.c
 *
 * This file implements the NSSCKFWMechanism type and methods.
 * These functions are currently stubs.
 */

#ifndef CK_T
#include "ck.h"
#endif /* CK_T */

/*
 * NSSCKFWMechanism
 *
 *  -- create/destroy --
 *  nssCKFWMechanism_Create
 *  nssCKFWMechanism_Destroy
 *
 *  -- implement public accessors --
 *  nssCKFWMechanism_GetMDMechanism
 *  nssCKFWMechanism_GetParameter
 *
 *  -- private accessors --
 *
 *  -- module fronts --
 *  nssCKFWMechanism_GetMinKeySize
 *  nssCKFWMechanism_GetMaxKeySize
 *  nssCKFWMechanism_GetInHardware
 */


struct NSSCKFWMechanismStr {
   void * dummy;
};

/*
 * nssCKFWMechanism_Create
 *
 */
NSS_IMPLEMENT NSSCKFWMechanism *
nssCKFWMechanism_Create
(
  void /* XXX fgmr */
)
{
  return (NSSCKFWMechanism *)NULL;
}

/*
 * nssCKFWMechanism_Destroy
 *
 */
NSS_IMPLEMENT CK_RV
nssCKFWMechanism_Destroy
(
  NSSCKFWMechanism *fwMechanism
)
{
   return CKR_OK;
}

/*
 * nssCKFWMechanism_GetMDMechanism
 *
 */

NSS_IMPLEMENT NSSCKMDMechanism *
nssCKFWMechanism_GetMDMechanism
(
  NSSCKFWMechanism *fwMechanism
)
{
    return NULL;
}

/*
 * nssCKFWMechanism_GetParameter
 *
 * XXX fgmr-- or as an additional parameter to the crypto ops?
 */
NSS_IMPLEMENT NSSItem *
nssCKFWMechanism_GetParameter
(
  NSSCKFWMechanism *fwMechanism
)
{
	return NULL;
}

/*
 * nssCKFWMechanism_GetMinKeySize
 *
 */
NSS_IMPLEMENT CK_ULONG
nssCKFWMechanism_GetMinKeySize
(
  NSSCKFWMechanism *fwMechanism
)
{
    return 0;
}

/*
 * nssCKFWMechanism_GetMaxKeySize
 *
 */
NSS_IMPLEMENT CK_ULONG
nssCKFWMechanism_GetMaxKeySize
(
  NSSCKFWMechanism *fwMechanism
)
{
   return 0;
}

/*
 * nssCKFWMechanism_GetInHardware
 *
 */
NSS_IMPLEMENT CK_BBOOL
nssCKFWMechanism_GetInHardware
(
  NSSCKFWMechanism *fwMechanism
)
{
    return PR_FALSE;
}

