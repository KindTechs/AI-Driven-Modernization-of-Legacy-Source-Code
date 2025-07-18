/*
 * NSS utility functions
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
 * $Id: authcert.c,v 1.2.18.1 2002/04/10 03:29:12 cltbld%netscape.com Exp $
 */

#include <stdio.h>
#include <string.h>
#include "prerror.h"
#include "secitem.h"
#include "prnetdb.h"
#include "cert.h"
#include "nspr.h"
#include "secder.h"
#include "key.h"
#include "nss.h"
#include "ssl.h"
#include "pk11func.h"	/* for PK11_ function calls */

/*
 * This callback used by SSL to pull client sertificate upon
 * server request
 */
SECStatus 
NSS_GetClientAuthData(void *                       arg, 
                      PRFileDesc *                 socket, 
		      struct CERTDistNamesStr *    caNames, 
		      struct CERTCertificateStr ** pRetCert, 
		      struct SECKEYPrivateKeyStr **pRetKey)
{
  CERTCertificate *  cert = NULL;
  SECKEYPrivateKey * privkey = NULL;
  char *             chosenNickName = (char *)arg;    /* CONST */
  void *             proto_win  = NULL;
  SECStatus          rv         = SECFailure;
  
  proto_win = SSL_RevealPinArg(socket);
  
  if (chosenNickName) {
    cert = PK11_FindCertFromNickname(chosenNickName, proto_win);
    if ( cert ) {
      privkey = PK11_FindKeyByAnyCert(cert, proto_win);
      if ( privkey ) {
	rv = SECSuccess;
      } else {
	CERT_DestroyCertificate(cert);
      }
    }
  } else { /* no name given, automatically find the right cert. */
    CERTCertNicknames * names;
    int                 i;
      
    names = CERT_GetCertNicknames(CERT_GetDefaultCertDB(),
				  SEC_CERT_NICKNAMES_USER, proto_win);
    if (names != NULL) {
      for (i = 0; i < names->numnicknames; i++) {
	cert = PK11_FindCertFromNickname(names->nicknames[i],proto_win);
	if ( !cert )
	  continue;
	/* Only check unexpired certs */
	if (CERT_CheckCertValidTimes(cert, PR_Now(), PR_TRUE) != 
	    secCertTimeValid ) {
	  CERT_DestroyCertificate(cert);
	  continue;
	}
	rv = NSS_CmpCertChainWCANames(cert, caNames);
	if ( rv == SECSuccess ) {
	  privkey = PK11_FindKeyByAnyCert(cert, proto_win);
	  if ( privkey )
	    break;
	}
	rv = SECFailure;
	CERT_DestroyCertificate(cert);
      } 
      CERT_FreeNicknames(names);
    }
  }
  if (rv == SECSuccess) {
    *pRetCert = cert;
    *pRetKey  = privkey;
  }
  return rv;
}

