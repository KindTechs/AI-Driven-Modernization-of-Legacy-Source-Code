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
 *
 * hasht.h - public data structures for the hashing library
 *
 * $Id: hasht.h,v 1.3.124.1 2002/04/10 03:27:02 cltbld%netscape.com Exp $
 */

#ifndef _HASHT_H_
#define _HASHT_H_

/* Opaque objects */
typedef struct SECHashObjectStr SECHashObject;
typedef struct HASHContextStr HASHContext;

/*
 * The hash functions the security library supports
 * NOTE the order must match the definition of SECHashObjects[]!
 */
typedef enum {
    HASH_AlgNULL = 0,
    HASH_AlgMD2 = 1,
    HASH_AlgMD5 = 2,
    HASH_AlgSHA1 = 3,
    HASH_AlgTOTAL
} HASH_HashType;

/*
 * Number of bytes each hash algorithm produces
 */
#define MD2_LENGTH	16
#define MD5_LENGTH	16
#define SHA1_LENGTH	20

/*
 * Structure to hold hash computation info and routines
 */
struct SECHashObjectStr {
    unsigned int length;
    void * (*create)(void);
    void * (*clone)(void *);
    void (*destroy)(void *, PRBool);
    void (*begin)(void *);
    void (*update)(void *, const unsigned char *, unsigned int);
    void (*end)(void *, unsigned char *, unsigned int *, unsigned int);
};

struct HASHContextStr {
    const struct SECHashObjectStr *hashobj;
    void *hash_context;
};

/* This symbol is NOT exported from the NSS DLL.  Code that needs a 
 * pointer to one of the SECHashObjects should call HASH_GetHashObject()
 * instead. See "sechash.h".
 */
extern const SECHashObject SECHashObjects[];

/* Only those functions below the PKCS #11 line should use SECRawHashObjects.
 * This symbol is not exported from the NSS DLL.
 */
extern const SECHashObject SECRawHashObjects[];

#endif /* _HASHT_H_ */
