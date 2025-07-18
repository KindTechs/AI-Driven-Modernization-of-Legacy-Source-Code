/*
 * ldvector.c - platform dependent DSO containing freebl implementation.
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
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 2000 Netscape Communications Corporation.  All
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
 *
 * $Id: ldvector.c,v 1.2.18.1 2002/04/10 03:27:21 cltbld%netscape.com Exp $
 */

#include "loader.h"

static const struct FREEBLVectorStr vector = {

    sizeof vector,
    FREEBL_VERSION,

    RSA_NewKey,
    RSA_PublicKeyOp,
    RSA_PrivateKeyOp,
    DSA_NewKey,
    DSA_SignDigest,
    DSA_VerifyDigest,
    DSA_NewKeyFromSeed,
    DSA_SignDigestWithSeed,
    DH_GenParam,
    DH_NewKey,
    DH_Derive,
    KEA_Derive,
    KEA_Verify,
    RC4_CreateContext,
    RC4_DestroyContext,
    RC4_Encrypt,
    RC4_Decrypt,
    RC2_CreateContext,
    RC2_DestroyContext,
    RC2_Encrypt,
    RC2_Decrypt,
    RC5_CreateContext,
    RC5_DestroyContext,
    RC5_Encrypt,
    RC5_Decrypt,
    DES_CreateContext,
    DES_DestroyContext,
    DES_Encrypt,
    DES_Decrypt,
    AES_CreateContext,
    AES_DestroyContext,
    AES_Encrypt,
    AES_Decrypt,
    MD5_Hash,
    MD5_HashBuf,
    MD5_NewContext,
    MD5_DestroyContext,
    MD5_Begin,
    MD5_Update,
    MD5_End,
    MD5_FlattenSize,
    MD5_Flatten,
    MD5_Resurrect,
    MD5_TraceState,
    MD2_Hash,
    MD2_NewContext,
    MD2_DestroyContext,
    MD2_Begin,
    MD2_Update,
    MD2_End,
    MD2_FlattenSize,
    MD2_Flatten,
    MD2_Resurrect,
    SHA1_Hash,
    SHA1_HashBuf,
    SHA1_NewContext,
    SHA1_DestroyContext,
    SHA1_Begin,
    SHA1_Update,
    SHA1_End,
    SHA1_TraceState,
    SHA1_FlattenSize,
    SHA1_Flatten,
    SHA1_Resurrect,
    RNG_RNGInit,
    RNG_RandomUpdate,
    RNG_GenerateGlobalRandomBytes,
    RNG_RNGShutdown,
    PQG_ParamGen,
    PQG_ParamGenSeedLen,
    PQG_VerifyParams,
    RSA_PrivateKeyOpDoubleChecked,
    RSA_PrivateKeyCheck,
    BL_Cleanup,
};


const FREEBLVector * 
FREEBL_GetVector(void)
{
  return &vector;
}

