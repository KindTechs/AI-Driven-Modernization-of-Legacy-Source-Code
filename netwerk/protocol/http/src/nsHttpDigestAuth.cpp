/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Justin Bradford <jab@atdot.org> (original author of nsDigestAuth.cpp)
 *   An-Cheng Huang <pach@cs.cmu.edu>
 *   Darin Fisher <darin@netscape.com>
 */

#include <stdlib.h>
#include "nsHttp.h"
#include "nsHttpDigestAuth.h"
#include "nsIHttpChannel.h"
#include "nsIServiceManager.h"
#include "nsISupportsPrimitives.h"
#include "nsIURI.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "plbase64.h"
#include "plstr.h"
#include "prprf.h"
#include "prmem.h"
#include "nsCRT.h"

//-----------------------------------------------------------------------------
// nsHttpDigestAuth <public>
//-----------------------------------------------------------------------------

nsHttpDigestAuth::nsHttpDigestAuth()
{
  NS_INIT_ISUPPORTS();

  mVerifier = do_GetService(SIGNATURE_VERIFIER_CONTRACTID);
  mGotVerifier = (mVerifier != nsnull);

#if defined(PR_LOGGING)
  if (mGotVerifier) {
    LOG(("nsHttpDigestAuth: Got signature_verifier\n"));
  } else {
    LOG(("nsHttpDigestAuth: No signature_verifier available\n"));
  }
#endif
}

//-----------------------------------------------------------------------------
// nsHttpDigestAuth::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS1(nsHttpDigestAuth, nsIHttpAuthenticator);

//-----------------------------------------------------------------------------
// nsHttpDigestAuth <protected>
//-----------------------------------------------------------------------------

nsresult
nsHttpDigestAuth::MD5Hash(const char *buf, PRUint32 len)
{
  if (!mGotVerifier)
    return NS_ERROR_NOT_INITIALIZED;

  nsresult rv;
  HASHContextStr *hid;
  unsigned char cbuf[DIGEST_LENGTH], *chash = cbuf;
  PRUint32 clen;

  rv = mVerifier->HashBegin(nsISignatureVerifier::MD5, &hid);
  if (NS_FAILED(rv)) return rv;

  rv = mVerifier->HashUpdate(hid, buf, len);
  if (NS_FAILED(rv)) return rv;

  rv = mVerifier->HashEnd(hid, &chash, &clen, DIGEST_LENGTH);
  if (NS_FAILED(rv)) return rv;

  memcpy(mHashBuf, chash, DIGEST_LENGTH);
  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpDigestAuth::nsIHttpAuthenticator
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpDigestAuth::GenerateCredentials(nsIHttpChannel *httpChannel,
                                      const char *challenge,
                                      const PRUnichar *username,
                                      const PRUnichar *password,
                                      nsISupports *extra,
                                      char **creds)

{
  LOG(("nsHttpDigestAuth::GenerateCredentials [challenge=%s]\n", challenge));

  NS_ENSURE_ARG_POINTER(creds);

  PRBool isDigestAuth = !PL_strncasecmp(challenge, "digest ", 7);
  NS_ENSURE_TRUE(isDigestAuth, NS_ERROR_UNEXPECTED);

  NS_ConvertUCS2toUTF8 cUser(username), cPass(password);

  nsresult rv;
  nsCOMPtr<nsIURI> uri;
  nsCAutoString path;
  nsCAutoString httpMethod;

  rv = httpChannel->GetURI(getter_AddRefs(uri));
  if (NS_FAILED(rv)) return rv;

  rv = uri->GetPath(path);
  if (NS_FAILED(rv)) return rv;

  rv = httpChannel->GetRequestMethod(httpMethod);
  if (NS_FAILED(rv)) return rv;

  nsCAutoString realm, domain, nonce, opaque;
  PRBool stale;
  PRUint16 algorithm, qop;

  rv = ParseChallenge(challenge, realm, domain, nonce, opaque,
                      &stale, &algorithm, &qop);
  if (NS_FAILED(rv)) {
    LOG(("nsHttpDigestAuth::GenerateCredentials [ParseChallenge failed rv=%x]\n", rv));
    return rv;
  }

  char ha1_digest[EXPANDED_DIGEST_LENGTH+1];
  char ha2_digest[EXPANDED_DIGEST_LENGTH+1];
  char response_digest[EXPANDED_DIGEST_LENGTH+1];
  char upload_data_digest[EXPANDED_DIGEST_LENGTH+1];

  if (qop & QOP_AUTH_INT) {
    // we do not support auth-int "quality of protection" currently
    qop &= ~QOP_AUTH_INT;

    NS_WARNING("no support for Digest authentication with data integrity quality of protection");

    /* TODO: to support auth-int, we need to get an MD5 digest of
     * TODO: the data uploaded with this request.
     * TODO: however, i am not sure how to read in the file in without
     * TODO: disturbing the channel''s use of it. do i need to copy it 
     * TODO: somehow?
     */
#if 0
    if (http_channel != nsnull)
    {
      nsIInputStream * upload;
      http_channel->GetUploadStream(&upload);
      if (upload) {
        char * upload_buffer;
        int upload_buffer_length = 0;
        //TODO: read input stream into buffer
        const char * digest = (const char*)
        nsNetwerkMD5Digest(upload_buffer, upload_buffer_length);
        ExpandToHex(digest, upload_data_digest);
        NS_RELEASE(upload);
      }
    }
#endif
  }

  if (!(algorithm & ALGO_MD5 || algorithm & ALGO_MD5_SESS)) {
    // they asked only for algorithms that we do not support
    NS_WARNING("unsupported algorithm requested by Digest authentication");
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  //
  // the following are for increasing security.  see RFC 2617 for more
  // information.
  // 
  // nonce_count allows the server to keep track of auth challenges (to help
  // prevent spoofing). we increase this count every time.
  //
  char nonce_count[NONCE_COUNT_LENGTH+1] = "00000001"; // in hex
  nsCOMPtr<nsISupportsPRUint32> v(do_QueryInterface(extra));
  if (v) {
    PRUint32 nc;
    v->GetData(&nc);
    PR_snprintf(nonce_count, sizeof(nonce_count), "%08x", ++nc);
    v->SetData(nc);
  }
  LOG(("   nonce_count=%s\n", nonce_count));

  //
  // this lets the client verify the server response (via a server
  // returned Authentication-Info header). also used for session info.
  //
  nsCAutoString cnonce;
  static const char hexChar[] = "0123456789abcdef"; 
  for (int i=0; i<16; ++i) {
    cnonce.Append(hexChar[(int)(15.0 * rand()/(RAND_MAX + 1.0))]);
  }
  LOG(("   cnonce=%s\n", cnonce.get()));

  //
  // calculate credentials
  //

  rv = CalculateHA1(cUser, cPass, realm, algorithm, nonce, cnonce, ha1_digest);
  if (NS_FAILED(rv)) return rv;

  rv = CalculateHA2(httpMethod, path, qop, upload_data_digest, ha2_digest);
  if (NS_FAILED(rv)) return rv;

  rv = CalculateResponse(ha1_digest, ha2_digest, nonce, qop, nonce_count,
                         cnonce, response_digest);
  if (NS_FAILED(rv)) return rv;

  nsCAutoString authString("Digest ");
  authString += "username=\"";
  authString += cUser;
  authString += NS_LITERAL_CSTRING("\", realm=\"");
  authString += realm;
  authString += NS_LITERAL_CSTRING("\", nonce=\"");
  authString += nonce;
  authString += NS_LITERAL_CSTRING("\", uri=\"");
  authString += path;
  if (algorithm & ALGO_SPECIFIED) {
    authString += "\", algorithm=";
    if (algorithm & ALGO_MD5_SESS)
      authString += "MD5-sess";
    else
      authString += "MD5";
  } else {
    authString += "\"";
  }
  authString += ", response=\"";
  authString += response_digest;

  if (!opaque.IsEmpty()) {
    authString += "\", opaque=\"";
    authString += opaque;
  }

  if (qop) {
    authString += "\", qop=";
    if (qop & QOP_AUTH_INT)
      authString += "auth-int";
    else
      authString += "auth";
    authString += ", nc=";
    authString += nonce_count;
    authString += ", cnonce=\"";
    authString += cnonce;
  }
  authString += "\"";

  *creds = ToNewCString(authString);
  return NS_OK;
}

NS_IMETHODIMP
nsHttpDigestAuth::AreCredentialsReusable(PRBool *result)
{
  *result = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsHttpDigestAuth::AllocateMetaData(nsISupports **result)
{
  nsresult rv;
  nsCOMPtr<nsISupportsPRUint32> v(
          do_CreateInstance(NS_SUPPORTS_PRUINT32_CONTRACTID, &rv));
  NS_ADDREF(*result = v);
  return rv;
}

nsresult
nsHttpDigestAuth::CalculateResponse(const char * ha1_digest,
                                    const char * ha2_digest,
                                    const nsAFlatCString & nonce,
                                    PRUint16 qop,
                                    const char * nonce_count,
                                    const nsAFlatCString & cnonce,
                                    char * result)
{
  PRUint32 len = 2*EXPANDED_DIGEST_LENGTH + nonce.Length() + 2;

  if (qop & QOP_AUTH || qop & QOP_AUTH_INT) {
    len += cnonce.Length() + NONCE_COUNT_LENGTH + 3;
    if (qop & QOP_AUTH_INT)
      len += 8; // length of "auth-int"
    else
      len += 4; // length of "auth"
  }

  nsCAutoString contents;
  contents.SetCapacity(len);

  contents.Assign(ha1_digest, EXPANDED_DIGEST_LENGTH);
  contents.Append(':');
  contents.Append(nonce);
  contents.Append(':');

  if (qop & QOP_AUTH || qop & QOP_AUTH_INT) {
    contents.Append(nonce_count, NONCE_COUNT_LENGTH);
    contents.Append(':');
    contents.Append(cnonce);
    contents.Append(':');
    if (qop & QOP_AUTH_INT)
      contents.Append(NS_LITERAL_CSTRING("auth-int:"));
    else
      contents.Append(NS_LITERAL_CSTRING("auth:"));
  }

  contents.Append(ha2_digest, EXPANDED_DIGEST_LENGTH);

  nsresult rv = MD5Hash(contents.get(), contents.Length());
  if (NS_FAILED(rv)) return rv;

  return ExpandToHex(mHashBuf, result);
}

nsresult
nsHttpDigestAuth::ExpandToHex(const char * digest, char * result)
{
  PRInt16 index, value;

  for (index = 0; index < DIGEST_LENGTH; index++) {
    value = (digest[index] >> 4) & 0xf;
    if (value < 10)
      result[index*2] = value + '0';
    else
      result[index*2] = value - 10 + 'a';

    value = digest[index] & 0xf;
    if (value < 10)
      result[(index*2)+1] = value + '0';
    else
      result[(index*2)+1] = value - 10 + 'a';
  }

  result[EXPANDED_DIGEST_LENGTH] = 0;
  return NS_OK;
}

nsresult
nsHttpDigestAuth::CalculateHA1(const nsAFlatCString & username,
                               const nsAFlatCString & password,
                               const nsAFlatCString & realm,
                               PRUint16 algorithm,
                               const nsAFlatCString & nonce,
                               const nsAFlatCString & cnonce,
                               char * result)
{
  PRInt16 len = username.Length() + password.Length() + realm.Length() + 2;
  if (algorithm & ALGO_MD5_SESS) {
    PRInt16 exlen = EXPANDED_DIGEST_LENGTH + nonce.Length() + cnonce.Length() + 2;
    if (exlen > len)
        len = exlen;
  }

  nsCAutoString contents;
  contents.SetCapacity(len + 1);

  contents.Assign(username);
  contents.Append(':');
  contents.Append(realm);
  contents.Append(':');
  contents.Append(password);

  nsresult rv;
  rv = MD5Hash(contents.get(), contents.Length());
  if (NS_FAILED(rv))
    return rv;

  if (algorithm & ALGO_MD5_SESS) {
    char part1[EXPANDED_DIGEST_LENGTH+1];
    ExpandToHex(mHashBuf, part1);

    contents.Assign(part1, EXPANDED_DIGEST_LENGTH);
    contents.Append(':');
    contents.Append(nonce);
    contents.Append(':');
    contents.Append(cnonce);

    rv = MD5Hash(contents.get(), contents.Length());
    if (NS_FAILED(rv))
      return rv;
  }

  return ExpandToHex(mHashBuf, result);
}

nsresult
nsHttpDigestAuth::CalculateHA2(const nsAFlatCString & method,
                               const nsAFlatCString & path,
                               PRUint16 qop,
                               const char * bodyDigest,
                               char * result)
{
  PRInt16 methodLen = method.Length();
  PRInt16 pathLen = path.Length();
  PRInt16 len = methodLen + pathLen + 1;

  if (qop & QOP_AUTH_INT) {
    len += EXPANDED_DIGEST_LENGTH + 1;
  }

  nsCAutoString contents;
  contents.SetCapacity(len);

  contents.Assign(method);
  contents.Append(':');
  contents.Append(path);

  if (qop & QOP_AUTH_INT) {
    contents.Append(':');
    contents.Append(bodyDigest, EXPANDED_DIGEST_LENGTH);
  }

  nsresult rv = MD5Hash(contents.get(), contents.Length());
  if (NS_FAILED(rv)) return rv;

  return ExpandToHex(mHashBuf, result);
}

nsresult
nsHttpDigestAuth::ParseChallenge(const char * challenge,
                                 nsACString & realm,
                                 nsACString & domain,
                                 nsACString & nonce,
                                 nsACString & opaque,
                                 PRBool * stale,
                                 PRUint16 * algorithm,
                                 PRUint16 * qop)
{
  const char *p = challenge + 7; // first 7 characters are "Digest "

  *stale = PR_FALSE;
  *algorithm = ALGO_MD5; // default is MD5
  *qop = 0;

  for (;;) {
    while (*p && (*p == ',' || nsCRT::IsAsciiSpace(*p)))
      ++p;
    if (!*p)
      break;

    // name
    PRInt16 nameStart = (p - challenge);
    while (*p && !nsCRT::IsAsciiSpace(*p) && *p != '=') 
      ++p;
    if (!*p)
      return NS_ERROR_INVALID_ARG;
    PRInt16 nameLength = (p - challenge) - nameStart;

    while (*p && (nsCRT::IsAsciiSpace(*p) || *p == '=')) 
      ++p;
    if (!*p) 
      return NS_ERROR_INVALID_ARG;

    PRBool quoted = PR_FALSE;
    if (*p == '"') {
      ++p;
      quoted = PR_TRUE;
    }

    // value
    PRInt16 valueStart = (p - challenge);
    PRInt16 valueLength = 0;
    if (quoted) {
      while (*p && *p != '"') 
        ++p;
      if (*p != '"') 
        return NS_ERROR_INVALID_ARG;
      valueLength = (p - challenge) - valueStart;
      ++p;
    } else {
      while (*p && !nsCRT::IsAsciiSpace(*p) && *p != ',') 
        ++p; 
      valueLength = (p - challenge) - valueStart;
    }

    // extract information
    if (nameLength == 5 &&
        nsCRT::strncasecmp(challenge+nameStart, "realm", 5) == 0)
    {
      realm.Assign(challenge+valueStart, valueLength);
    }
    else if (nameLength == 6 &&
        nsCRT::strncasecmp(challenge+nameStart, "domain", 6) == 0)
    {
      domain.Assign(challenge+valueStart, valueLength);
    }
    else if (nameLength == 5 &&
        nsCRT::strncasecmp(challenge+nameStart, "nonce", 5) == 0)
    {
      nonce.Assign(challenge+valueStart, valueLength);
    }
    else if (nameLength == 6 &&
        nsCRT::strncasecmp(challenge+nameStart, "opaque", 6) == 0)
    {
      opaque.Assign(challenge+valueStart, valueLength);
    }
    else if (nameLength == 5 &&
        nsCRT::strncasecmp(challenge+nameStart, "stale", 5) == 0)
    {
      if (nsCRT::strncasecmp(challenge+valueStart, "true", 4) == 0)
        *stale = PR_TRUE;
      else
        *stale = PR_FALSE;
    }
    else if (nameLength == 9 &&
        nsCRT::strncasecmp(challenge+nameStart, "algorithm", 9) == 0)
    {
      // we want to clear the default, so we use = not |= here
      *algorithm = ALGO_SPECIFIED;
      if (valueLength == 3 &&
          nsCRT::strncasecmp(challenge+valueStart, "MD5", 3) == 0)
        *algorithm |= ALGO_MD5;
      else if (valueLength == 8 &&
          nsCRT::strncasecmp(challenge+valueStart, "MD5-sess", 8) == 0)
        *algorithm |= ALGO_MD5_SESS;
    }
    else if (nameLength == 3 &&
        nsCRT::strncasecmp(challenge+nameStart, "qop", 3) == 0)
    {
      PRInt16 ipos = valueStart;
      while (ipos < valueStart+valueLength) {
        while (ipos < valueStart+valueLength &&
               (nsCRT::IsAsciiSpace(challenge[ipos]) ||
                challenge[ipos] == ',')) 
          ipos++;
        PRInt16 algostart = ipos;
        while (ipos < valueStart+valueLength &&
               !nsCRT::IsAsciiSpace(challenge[ipos]) &&
               challenge[ipos] != ',') 
          ipos++;
        if ((ipos - algostart) == 4 &&
            nsCRT::strncasecmp(challenge+algostart, "auth", 4) == 0)
          *qop |= QOP_AUTH;
        else if ((ipos - algostart) == 8 &&
            nsCRT::strncasecmp(challenge+algostart, "auth-int", 8) == 0)
          *qop |= QOP_AUTH_INT;
      }
    }
  }
  return NS_OK;
}
