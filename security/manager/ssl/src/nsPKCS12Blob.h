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
 * Copyright (C) 2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 *  Ian McGreer <mcgreer@netscape.com>
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
 * $Id: nsPKCS12Blob.h,v 1.6.74.1 2002/04/10 03:24:06 cltbld%netscape.com Exp $
 */

#ifndef _NS_PKCS12BLOB_H_
#define _NS_PKCS12BLOB_H_

#include "nsCOMPtr.h"
#include "nsILocalFile.h"
#include "nsIPK11TokenDB.h"
#include "nsNSSHelper.h"

#include "nss.h"

extern "C" {
#include "pkcs12.h"
#include "p12plcy.h"
}

//
// nsPKCS12Blob
//
// Class for importing/exporting PKCS#12 blobs
//
class nsPKCS12Blob
{
public:
  nsPKCS12Blob();
  virtual ~nsPKCS12Blob();

  // Set the token to use (default is internal)
  void SetToken(nsIPK11Token *token);

  // PKCS#12 Import
  nsresult ImportFromFile(nsILocalFile *file);

  // PKCS#12 Export
#if 0
  //nsresult LoadCerts(const PRUnichar **certNames, int numCerts);
  nsresult LoadCerts(nsIX509Cert **certs, int numCerts);
#endif
  nsresult ExportToFile(nsILocalFile *file, nsIX509Cert **certs, int numCerts);

private:

  nsCOMPtr<nsIPK11Token>          mToken;
  nsCOMPtr<nsISupportsArray>      mCertArray;
  nsCOMPtr<nsIInterfaceRequestor> mUIContext;

  // local helper functions
  nsresult getPKCS12FilePassword(SECItem *);
  nsresult newPKCS12FilePassword(SECItem *);
  nsresult inputToDecoder(SEC_PKCS12DecoderContext *, nsILocalFile *);
  void unicodeToItem(PRUnichar *, SECItem *);
  PRBool handleError(int myerr = 0);

  // NSPR file I/O for temporary digest file
  PRFileDesc *mTmpFile;
  char       *mTmpFilePath;
  PRBool      mTokenSet;

  // C-style callback functions for the NSS PKCS#12 library
  static SECStatus PR_CALLBACK digest_open(void *, PRBool);
  static SECStatus PR_CALLBACK digest_close(void *, PRBool);
  static int       PR_CALLBACK digest_read(void *, unsigned char *, unsigned long);
  static int       PR_CALLBACK digest_write(void *, unsigned char *, unsigned long);
  static SECItem * PR_CALLBACK nickname_collision(SECItem *, PRBool *, void *);
  static void PR_CALLBACK write_export_file(void *arg, const char *buf, unsigned long len);

};

#endif /* _NS_PKCS12BLOB_H_ */

