/*
 * This file contains prototypes for the public SSL functions.
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
 * $Id: sslt.h,v 1.3.6.1 2002/04/10 03:29:17 cltbld%netscape.com Exp $
 */

#ifndef __sslt_h_
#define __sslt_h_

#include "prtypes.h"

typedef struct SSL3StatisticsStr {
    /* statistics from ssl3_SendClientHello (sch) */
    long sch_sid_cache_hits;
    long sch_sid_cache_misses;
    long sch_sid_cache_not_ok;

    /* statistics from ssl3_HandleServerHello (hsh) */
    long hsh_sid_cache_hits;
    long hsh_sid_cache_misses;
    long hsh_sid_cache_not_ok;

    /* statistics from ssl3_HandleClientHello (hch) */
    long hch_sid_cache_hits;
    long hch_sid_cache_misses;
    long hch_sid_cache_not_ok;
} SSL3Statistics;

/* Key Exchange algorithm values */
typedef enum {
    ssl_kea_null     = 0,
    ssl_kea_rsa      = 1,
    ssl_kea_dh       = 2,
    ssl_kea_fortezza = 3,
    ssl_kea_size		/* number of ssl_kea_ algorithms */
} SSLKEAType;

/* The following defines are for backwards compatibility.
** They will be removed in a forthcoming release to reduce namespace pollution.
** programs that use the kt_ symbols should convert to the ssl_kt_ symbols
** soon.
*/
#define kt_null   	ssl_kea_null
#define kt_rsa   	ssl_kea_rsa
#define kt_dh   	ssl_kea_dh
#define kt_fortezza	ssl_kea_fortezza
#define kt_kea_size	ssl_kea_size

typedef enum {
    ssl_sign_null   = 0, 
    ssl_sign_rsa    = 1,
    ssl_sign_dsa    = 2
} SSLSignType;

typedef enum {
    ssl_auth_null   = 0, 
    ssl_auth_rsa    = 1,
    ssl_auth_dsa    = 2,
    ssl_auth_kea    = 3
} SSLAuthType;

typedef enum {
    ssl_calg_null     = 0,
    ssl_calg_rc4      = 1,
    ssl_calg_rc2      = 2,
    ssl_calg_des      = 3,
    ssl_calg_3des     = 4,
    ssl_calg_idea     = 5,
    ssl_calg_fortezza = 6,      /* skipjack */
    ssl_calg_aes      = 7       /* coming soon */
} SSLCipherAlgorithm;

typedef enum { 
    ssl_mac_null      = 0, 
    ssl_mac_md5       = 1, 
    ssl_mac_sha       = 2, 
    ssl_hmac_md5      = 3, 	/* TLS HMAC version of mac_md5 */
    ssl_hmac_sha      = 4 	/* TLS HMAC version of mac_sha */
} SSLMACAlgorithm;

typedef struct SSLChannelInfoStr {
    PRUint32             length;
    PRUint16             protocolVersion;
    PRUint16             cipherSuite;

    /* server authentication info */
    PRUint32             authKeyBits;

    /* key exchange algorithm info */
    PRUint32             keaKeyBits;

    /* session info */
    PRUint32             creationTime;		/* seconds since Jan 1, 1970 */
    PRUint32             lastAccessTime;	/* seconds since Jan 1, 1970 */
    PRUint32             expirationTime;	/* seconds since Jan 1, 1970 */
    PRUint32             sessionIDLength;	/* up to 32 */
    PRUint8              sessionID    [32];
} SSLChannelInfo;

typedef struct SSLCipherSuiteInfoStr {
    PRUint16             length;
    PRUint16             cipherSuite;

    /* Cipher Suite Name */
    const char *         cipherSuiteName;

    /* server authentication info */
    const char *         authAlgorithmName;
    SSLAuthType          authAlgorithm;

    /* key exchange algorithm info */
    const char *         keaTypeName;
    SSLKEAType           keaType;

    /* symmetric encryption info */
    const char *         symCipherName;
    SSLCipherAlgorithm   symCipher;
    PRUint16             symKeyBits;
    PRUint16             symKeySpace;
    PRUint16             effectiveKeyBits;

    /* MAC info */
    const char *         macAlgorithmName;
    SSLMACAlgorithm      macAlgorithm;
    PRUint16             macBits;

    PRUintn              isFIPS       : 1;
    PRUintn              isExportable : 1;
    PRUintn              nonStandard  : 1;
    PRUintn              reservedBits :29;

} SSLCipherSuiteInfo;

#endif /* __sslt_h_ */
