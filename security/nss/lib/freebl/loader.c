/*
 * loader.c - load platform dependent DSO containing freebl implementation.
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
 * $Id: loader.c,v 1.7.18.1 2002/04/10 03:27:21 cltbld%netscape.com Exp $
 */

#include "loader.h"
#include "prmem.h"
#include "prerror.h"
#include "prinit.h"

#if defined(SOLARIS)
#include <stddef.h>
#include <strings.h>
#include <sys/systeminfo.h>
#if defined(SOLARIS2_5)
static int
isHybrid(void)
{
    long buflen;
    int  rv             = 0;    /* false */
    char buf[256];
    buflen = sysinfo(SI_MACHINE, buf, sizeof buf);
    if (buflen > 0) {
        rv = (!strcmp(buf, "sun4u") || !strcmp(buf, "sun4u1"));
    }
    return rv;
}
#else   /* SunOS2.6or higher has SI_ISALIST */

static int
isHybrid(void)
{
    long buflen;
    int  rv             = 0;    /* false */
    char buf[256];
    buflen = sysinfo(SI_ISALIST, buf, sizeof buf);
    if (buflen > 0) {
#if defined(NSS_USE_64)
        char * found = strstr(buf, "sparcv9+vis");
#else
        char * found = strstr(buf, "sparcv8plus+vis");
#endif
        rv = (found != 0);
    }
    return rv;
}
#endif
#elif defined(HPUX)
/* This code tests to see if we're running on a PA2.x CPU.
** It returns true (1) if so, and false (0) otherwise.
*/
static int 
isHybrid(void)
{
    long cpu = sysconf(_SC_CPU_VERSION);
    return (cpu == CPU_PA_RISC2_0);
}

#else
#error "code for this platform is missing."
#endif

/*
 * On Solaris, if an application using libnss3.so is linked
 * with the -R linker option, the -R search patch is only used
 * to search for the direct dependencies of the application
 * (such as libnss3.so) and is not used to search for the
 * dependencies of libnss3.so.  So we build libnss3.so with
 * the -R '$ORIGIN' linker option to instruct it to search for
 * its dependencies (libfreebl_*.so) in the same directory
 * where it resides.  This requires that libnss3.so, not
 * libnspr4.so, be the shared library that calls dlopen().
 * Therefore we have to duplicate the relevant code in the PR
 * load library functions here.
 */

#if defined(SOLARIS)

#include <dlfcn.h>

typedef struct {
    void *dlh;
} BLLibrary;

static BLLibrary *
bl_LoadLibrary(const char *name)
{
    BLLibrary *lib;
    const char *error;

    lib = PR_NEWZAP(BLLibrary);
    if (NULL == lib) {
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
        return NULL;
    }
    lib->dlh = dlopen(name, RTLD_NOW | RTLD_LOCAL);
    if (NULL == lib->dlh) {
        PR_SetError(PR_LOAD_LIBRARY_ERROR, 0);
        error = dlerror();
        if (NULL != error)
            PR_SetErrorText(strlen(error), error);
        PR_Free(lib);
        return NULL;
    }
    return lib;
}

static void *
bl_FindSymbol(BLLibrary *lib, const char *name)
{
    void *f;
    const char *error;

    f = dlsym(lib->dlh, name);
    if (NULL == f) {
        PR_SetError(PR_FIND_SYMBOL_ERROR, 0);
        error = dlerror();
        if (NULL != error)
            PR_SetErrorText(strlen(error), error);
    }
    return f;
}

static PRStatus
bl_UnloadLibrary(BLLibrary *lib)
{
    const char *error;

    if (dlclose(lib->dlh) == -1) {
        PR_SetError(PR_UNLOAD_LIBRARY_ERROR, 0);
        error = dlerror();
        if (NULL != error)
            PR_SetErrorText(strlen(error), error);
        return PR_FAILURE;
    }
    PR_Free(lib);
    return PR_SUCCESS;
}

#elif defined(HPUX)

#include <dl.h>
#include <string.h>
#include <errno.h>

typedef struct {
    shl_t dlh;
} BLLibrary;

static BLLibrary *
bl_LoadLibrary(const char *name)
{
    BLLibrary *lib;
    const char *error;

    lib = PR_NEWZAP(BLLibrary);
    if (NULL == lib) {
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
        return NULL;
    }
    lib->dlh = shl_load(name, DYNAMIC_PATH | BIND_IMMEDIATE, 0L);
    if (NULL == lib->dlh) {
        PR_SetError(PR_LOAD_LIBRARY_ERROR, errno);
        error = strerror(errno);
        if (NULL != error)
            PR_SetErrorText(strlen(error), error);
        PR_Free(lib);
        return NULL;
    }
    return lib;
}

static void *
bl_FindSymbol(BLLibrary *lib, const char *name)
{
    void *f;
    const char *error;

    if (shl_findsym(&lib->dlh, name, TYPE_PROCEDURE, &f) == -1) {
        f = NULL;
        PR_SetError(PR_FIND_SYMBOL_ERROR, errno);
        error = strerror(errno);
        if (NULL != error)
            PR_SetErrorText(strlen(error), error);
    }
    return f;
}

static PRStatus
bl_UnloadLibrary(BLLibrary *lib)
{
    const char *error;

    if (shl_unload(lib->dlh) == -1) {
        PR_SetError(PR_UNLOAD_LIBRARY_ERROR, errno);
        error = strerror(errno);
        if (NULL != error)
            PR_SetErrorText(strlen(error), error);
        return PR_FAILURE;
    }
    PR_Free(lib);
    return PR_SUCCESS;
}

#else
#error "code for this platform is missing."
#endif

#define LSB(x) ((x)&0xff)
#define MSB(x) ((x)>>8)

static const FREEBLVector *vector;

/* This function must be run only once. */
/*  determine if hybrid platform, then actually load the DSO. */
static PRStatus
freebl_LoadDSO( void ) 
{
  int          hybrid   = isHybrid();
  BLLibrary *  handle;
  const char * name;

  name = hybrid ?
#if defined( AIX )
	       "libfreebl_hybrid_3_shr.a" : "libfreebl_pure32_3_shr.a";
#elif defined( HPUX )
               "libfreebl_hybrid_3.sl"    : "libfreebl_pure32_3.sl";
#else
               "libfreebl_hybrid_3.so"    : "libfreebl_pure32_3.so";
#endif

  handle = bl_LoadLibrary(name);
  if (handle) {
    void * address = bl_FindSymbol(handle, "FREEBL_GetVector");
    if (address) {
      FREEBLGetVectorFn  * getVector = (FREEBLGetVectorFn *)address;
      const FREEBLVector * dsoVector = getVector();
      if (dsoVector) {
	unsigned short dsoVersion = dsoVector->version;
	unsigned short  myVersion = FREEBL_VERSION;
	if (MSB(dsoVersion) == MSB(myVersion) && 
	    LSB(dsoVersion) >= LSB(myVersion) &&
	    dsoVector->length >= sizeof(FREEBLVector)) {
	  vector = dsoVector;
	  return PR_SUCCESS;
	}
      }
    }
    bl_UnloadLibrary(handle);
  }
  return PR_FAILURE;
}

static PRStatus
freebl_RunLoaderOnce( void )
{
  PRStatus status;
  static PRCallOnceType once;

  status = PR_CallOnce(&once, &freebl_LoadDSO);
  return status;
}


RSAPrivateKey * 
RSA_NewKey(int keySizeInBits, SECItem * publicExponent)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return NULL;
  return (vector->p_RSA_NewKey)(keySizeInBits, publicExponent);
}

SECStatus 
RSA_PublicKeyOp(RSAPublicKey *   key,
				 unsigned char *  output,
				 const unsigned char *  input)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_RSA_PublicKeyOp)(key, output, input);
}

SECStatus 
RSA_PrivateKeyOp(RSAPrivateKey *  key,
				  unsigned char *  output,
				  const unsigned char *  input)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_RSA_PrivateKeyOp)(key, output, input);
}

SECStatus
RSA_PrivateKeyOpDoubleChecked(RSAPrivateKey *key,
                              unsigned char *output,
                              const unsigned char *input)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_RSA_PrivateKeyOpDoubleChecked)(key, output, input);
}

SECStatus
RSA_PrivateKeyCheck(RSAPrivateKey *key)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_RSA_PrivateKeyCheck)(key);
}

SECStatus 
DSA_NewKey(const PQGParams * params, DSAPrivateKey ** privKey)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_DSA_NewKey)(params, privKey);
}

SECStatus 
DSA_SignDigest(DSAPrivateKey * key, SECItem * signature, const SECItem * digest)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_DSA_SignDigest)( key,  signature,  digest);
}

SECStatus 
DSA_VerifyDigest(DSAPublicKey * key, const SECItem * signature, 
                 const SECItem * digest)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_DSA_VerifyDigest)( key,  signature,  digest);
}

SECStatus 
DSA_NewKeyFromSeed(const PQGParams *params, const unsigned char * seed,
                   DSAPrivateKey **privKey)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_DSA_NewKeyFromSeed)(params,  seed, privKey);
}

SECStatus 
DSA_SignDigestWithSeed(DSAPrivateKey * key, SECItem * signature,
		       const SECItem * digest, const unsigned char * seed)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_DSA_SignDigestWithSeed)( key, signature, digest, seed);
}

SECStatus 
DH_GenParam(int primeLen, DHParams ** params)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_DH_GenParam)(primeLen, params);
}

SECStatus 
DH_NewKey(DHParams * params, DHPrivateKey ** privKey)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_DH_NewKey)( params, privKey);
}

SECStatus 
DH_Derive(SECItem * publicValue, SECItem * prime, SECItem * privateValue, 
			 SECItem * derivedSecret, unsigned int maxOutBytes)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_DH_Derive)( publicValue, prime, privateValue, 
				derivedSecret, maxOutBytes);
}

SECStatus 
KEA_Derive(SECItem *prime, SECItem *public1, SECItem *public2, 
	   SECItem *private1, SECItem *private2, SECItem *derivedSecret)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_KEA_Derive)(prime, public1, public2, 
	                        private1, private2, derivedSecret);
}

PRBool 
KEA_Verify(SECItem *Y, SECItem *prime, SECItem *subPrime)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return PR_FALSE;
  return (vector->p_KEA_Verify)(Y, prime, subPrime);
}

RC4Context *
RC4_CreateContext(unsigned char *key, int len)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return NULL;
  return (vector->p_RC4_CreateContext)(key, len);
}

void 
RC4_DestroyContext(RC4Context *cx, PRBool freeit)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return;
  (vector->p_RC4_DestroyContext)(cx, freeit);
}

SECStatus 
RC4_Encrypt(RC4Context *cx, unsigned char *output, unsigned int *outputLen, 
	    unsigned int maxOutputLen, const unsigned char *input, 
	    unsigned int inputLen)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_RC4_Encrypt)(cx, output, outputLen, maxOutputLen, input, 
	                         inputLen);
}

SECStatus 
RC4_Decrypt(RC4Context *cx, unsigned char *output, unsigned int *outputLen, 
	    unsigned int maxOutputLen, const unsigned char *input, 
	    unsigned int inputLen)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_RC4_Decrypt)(cx, output, outputLen, maxOutputLen, input, 
	                         inputLen);
}

RC2Context *
RC2_CreateContext(unsigned char *key, unsigned int len,
		  unsigned char *iv, int mode, unsigned effectiveKeyLen)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return NULL;
  return (vector->p_RC2_CreateContext)(key, len, iv, mode, effectiveKeyLen);
}

void 
RC2_DestroyContext(RC2Context *cx, PRBool freeit)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return;
  (vector->p_RC2_DestroyContext)(cx, freeit);
}

SECStatus 
RC2_Encrypt(RC2Context *cx, unsigned char *output, unsigned int *outputLen, 
	    unsigned int maxOutputLen, const unsigned char *input, 
	    unsigned int inputLen)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_RC2_Encrypt)(cx, output, outputLen, maxOutputLen, input, 
	                         inputLen);
}

SECStatus 
RC2_Decrypt(RC2Context *cx, unsigned char *output, unsigned int *outputLen, 
	    unsigned int maxOutputLen, const unsigned char *input, 
	    unsigned int inputLen)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_RC2_Decrypt)(cx, output, outputLen, maxOutputLen, input, 
	                         inputLen);
}

RC5Context *
RC5_CreateContext(SECItem *key, unsigned int rounds,
                     unsigned int wordSize, unsigned char *iv, int mode)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return NULL;
  return (vector->p_RC5_CreateContext)(key, rounds, wordSize, iv, mode);
}

void 
RC5_DestroyContext(RC5Context *cx, PRBool freeit)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return;
  (vector->p_RC5_DestroyContext)(cx, freeit);
}

SECStatus 
RC5_Encrypt(RC5Context *cx, unsigned char *output, unsigned int *outputLen, 
	    unsigned int maxOutputLen, const unsigned char *input, 
	    unsigned int inputLen)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_RC5_Encrypt)(cx, output, outputLen, maxOutputLen, input, 
				 inputLen);
}

SECStatus 
RC5_Decrypt(RC5Context *cx, unsigned char *output, unsigned int *outputLen, 
	    unsigned int maxOutputLen, const unsigned char *input, 
	    unsigned int inputLen)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_RC5_Decrypt)(cx, output, outputLen, maxOutputLen, input, 
	                         inputLen);
}

DESContext *
DES_CreateContext(unsigned char *key, unsigned char *iv,
				     int mode, PRBool encrypt)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return NULL;
  return (vector->p_DES_CreateContext)(key, iv, mode, encrypt);
}

void 
DES_DestroyContext(DESContext *cx, PRBool freeit)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return;
  (vector->p_DES_DestroyContext)(cx, freeit);
}

SECStatus 
DES_Encrypt(DESContext *cx, unsigned char *output, unsigned int *outputLen, 
	    unsigned int maxOutputLen, const unsigned char *input, 
	    unsigned int inputLen)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_DES_Encrypt)(cx, output, outputLen, maxOutputLen, input, 
	                         inputLen);
}

SECStatus 
DES_Decrypt(DESContext *cx, unsigned char *output, unsigned int *outputLen, 
	    unsigned int maxOutputLen, const unsigned char *input, 
	    unsigned int inputLen)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_DES_Decrypt)(cx, output, outputLen, maxOutputLen, input, 
	                         inputLen);
}

AESContext *
AES_CreateContext(unsigned char *key, unsigned char *iv, int mode, int encrypt,
                  unsigned int keylen, unsigned int blocklen)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return NULL;
  return (vector->p_AES_CreateContext)(key, iv, mode, encrypt, keylen, 
				       blocklen);
}

void 
AES_DestroyContext(AESContext *cx, PRBool freeit)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return ;
  (vector->p_AES_DestroyContext)(cx, freeit);
}

SECStatus 
AES_Encrypt(AESContext *cx, unsigned char *output,
            unsigned int *outputLen, unsigned int maxOutputLen,
            const unsigned char *input, unsigned int inputLen)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_AES_Encrypt)(cx, output, outputLen, maxOutputLen, 
				 input, inputLen);
}

SECStatus 
AES_Decrypt(AESContext *cx, unsigned char *output,
            unsigned int *outputLen, unsigned int maxOutputLen,
            const unsigned char *input, unsigned int inputLen)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_AES_Decrypt)(cx, output, outputLen, maxOutputLen, 
				 input, inputLen);
}

SECStatus 
MD5_Hash(unsigned char *dest, const char *src)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_MD5_Hash)(dest, src);
}

SECStatus 
MD5_HashBuf(unsigned char *dest, const unsigned char *src, uint32 src_length)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_MD5_HashBuf)(dest, src, src_length);
}

MD5Context *
MD5_NewContext(void)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return NULL;
  return (vector->p_MD5_NewContext)();
}

void 
MD5_DestroyContext(MD5Context *cx, PRBool freeit)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return;
  (vector->p_MD5_DestroyContext)(cx, freeit);
}

void 
MD5_Begin(MD5Context *cx)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return;
  (vector->p_MD5_Begin)(cx);
}

void 
MD5_Update(MD5Context *cx, const unsigned char *input, unsigned int inputLen)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return;
  (vector->p_MD5_Update)(cx, input, inputLen);
}

void 
MD5_End(MD5Context *cx, unsigned char *digest,
		    unsigned int *digestLen, unsigned int maxDigestLen)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return;
  (vector->p_MD5_End)(cx, digest, digestLen, maxDigestLen);
}

unsigned int 
MD5_FlattenSize(MD5Context *cx)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return 0;
  return (vector->p_MD5_FlattenSize)(cx);
}

SECStatus 
MD5_Flatten(MD5Context *cx,unsigned char *space)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_MD5_Flatten)(cx, space);
}

MD5Context * 
MD5_Resurrect(unsigned char *space, void *arg)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return NULL;
  return (vector->p_MD5_Resurrect)(space, arg);
}

void 
MD5_TraceState(MD5Context *cx)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return ;
  (vector->p_MD5_TraceState)(cx);
}

SECStatus 
MD2_Hash(unsigned char *dest, const char *src)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_MD2_Hash)(dest, src);
}

MD2Context *
MD2_NewContext(void)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return NULL;
  return (vector->p_MD2_NewContext)();
}

void 
MD2_DestroyContext(MD2Context *cx, PRBool freeit)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return ;
  (vector->p_MD2_DestroyContext)(cx, freeit);
}

void 
MD2_Begin(MD2Context *cx)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return ;
  (vector->p_MD2_Begin)(cx);
}

void 
MD2_Update(MD2Context *cx, const unsigned char *input, unsigned int inputLen)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return ;
  (vector->p_MD2_Update)(cx, input, inputLen);
}

void 
MD2_End(MD2Context *cx, unsigned char *digest,
		    unsigned int *digestLen, unsigned int maxDigestLen)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return ;
  (vector->p_MD2_End)(cx, digest, digestLen, maxDigestLen);
}

unsigned int 
MD2_FlattenSize(MD2Context *cx)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return 0;
  return (vector->p_MD2_FlattenSize)(cx);
}

SECStatus 
MD2_Flatten(MD2Context *cx,unsigned char *space)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_MD2_Flatten)(cx, space);
}

MD2Context * 
MD2_Resurrect(unsigned char *space, void *arg)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return NULL;
  return (vector->p_MD2_Resurrect)(space, arg);
}


SECStatus 
SHA1_Hash(unsigned char *dest, const char *src)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_SHA1_Hash)(dest, src);
}

SECStatus 
SHA1_HashBuf(unsigned char *dest, const unsigned char *src, uint32 src_length)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_SHA1_HashBuf)(dest, src, src_length);
}

SHA1Context *
SHA1_NewContext(void)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return NULL;
  return (vector->p_SHA1_NewContext)();
}

void 
SHA1_DestroyContext(SHA1Context *cx, PRBool freeit)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return ;
  (vector->p_SHA1_DestroyContext)(cx, freeit);
}

void 
SHA1_Begin(SHA1Context *cx)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return ;
  (vector->p_SHA1_Begin)(cx);
}

void 
SHA1_Update(SHA1Context *cx, const unsigned char *input,
			unsigned int inputLen)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return ;
  (vector->p_SHA1_Update)(cx, input, inputLen);
}

void 
SHA1_End(SHA1Context *cx, unsigned char *digest,
		     unsigned int *digestLen, unsigned int maxDigestLen)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return ;
  (vector->p_SHA1_End)(cx, digest, digestLen, maxDigestLen);
}

void 
SHA1_TraceState(SHA1Context *cx)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return ;
  (vector->p_SHA1_TraceState)(cx);
}

unsigned int 
SHA1_FlattenSize(SHA1Context *cx)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return 0;
  return (vector->p_SHA1_FlattenSize)(cx);
}

SECStatus 
SHA1_Flatten(SHA1Context *cx,unsigned char *space)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_SHA1_Flatten)(cx, space);
}

SHA1Context * 
SHA1_Resurrect(unsigned char *space, void *arg)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return NULL;
  return (vector->p_SHA1_Resurrect)(space, arg);
}

SECStatus 
RNG_RNGInit(void)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_RNG_RNGInit)();
}

SECStatus 
RNG_RandomUpdate(const void *data, size_t bytes)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_RNG_RandomUpdate)(data, bytes);
}

SECStatus 
RNG_GenerateGlobalRandomBytes(void *dest, size_t len)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_RNG_GenerateGlobalRandomBytes)(dest, len);
}

void  
RNG_RNGShutdown(void)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return ;
  (vector->p_RNG_RNGShutdown)();
}

SECStatus
PQG_ParamGen(unsigned int j, PQGParams **pParams, PQGVerify **pVfy)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_PQG_ParamGen)(j, pParams, pVfy); 
}

SECStatus
PQG_ParamGenSeedLen( unsigned int j, unsigned int seedBytes, 
                     PQGParams **pParams, PQGVerify **pVfy)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_PQG_ParamGenSeedLen)(j, seedBytes, pParams, pVfy);
}

SECStatus   
PQG_VerifyParams(const PQGParams *params, const PQGVerify *vfy, 
		 SECStatus *result)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return SECFailure;
  return (vector->p_PQG_VerifyParams)(params, vfy, result);
}
#if 0
void 
PQG_DestroyParams(PQGParams *params)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return;
  (vector->p_PQG_DestroyParams)( params);
}

void 
PQG_DestroyVerify(PQGVerify *vfy)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return;
  (vector->p_PQG_DestroyVerify)( vfy);
}
#endif

void 
BL_Cleanup(void)
{
  if (!vector && PR_SUCCESS != freebl_RunLoaderOnce())
      return;
  (vector->p_BL_Cleanup)();
}
