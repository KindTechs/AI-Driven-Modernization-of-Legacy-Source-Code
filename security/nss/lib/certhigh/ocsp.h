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

/*
 * Interface to the OCSP implementation.
 *
 * $Id: ocsp.h,v 1.2.18.1 2002/04/10 03:26:38 cltbld%netscape.com Exp $
 */

#ifndef _OCSP_H_
#define _OCSP_H_


#include "plarena.h"
#include "seccomon.h"
#include "secoidt.h"
#include "keyt.h"
#include "certt.h"
#include "ocspt.h"


/************************************************************************/
SEC_BEGIN_PROTOS

/*
 * FUNCTION: CERT_EnableOCSPChecking
 *   Turns on OCSP checking for the given certificate database.
 * INPUTS:
 *   CERTCertDBHandle *handle
 *     Certificate database for which OCSP checking will be enabled.
 * RETURN:
 *   Returns SECFailure if an error occurred (likely only problem
 *   allocating memory); SECSuccess otherwise.
 */
extern SECStatus
CERT_EnableOCSPChecking(CERTCertDBHandle *handle);

/*
 * FUNCTION: CERT_DisableOCSPChecking
 *   Turns off OCSP checking for the given certificate database.
 *   This routine disables OCSP checking.  Though it will return
 *   SECFailure if OCSP checking is not enabled, it is "safe" to
 *   call it that way and just ignore the return value, if it is
 *   easier to just call it than to "remember" whether it is enabled.
 * INPUTS:
 *   CERTCertDBHandle *handle
 *     Certificate database for which OCSP checking will be disabled.
 * RETURN:
 *   Returns SECFailure if an error occurred (usually means that OCSP
 *   checking was not enabled or status contexts were not initialized --
 *   error set will be SEC_ERROR_OCSP_NOT_ENABLED); SECSuccess otherwise.
 */
extern SECStatus
CERT_DisableOCSPChecking(CERTCertDBHandle *handle);

/*
 * FUNCTION: CERT_SetOCSPDefaultResponder
 *   Specify the location and cert of the default responder.
 *   If OCSP checking is already enabled *and* use of a default responder
 *   is also already enabled, all OCSP checking from now on will go directly
 *   to the specified responder.  If OCSP checking is not enabled, or if
 *   it is but use of a default responder is not enabled, the information
 *   will be recorded and take effect whenever both are enabled.
 * INPUTS:
 *   CERTCertDBHandle *handle
 *     Cert database on which OCSP checking should use the default responder.
 *   char *url
 *     The location of the default responder (e.g. "http://foo.com:80/ocsp")
 *     Note that the location will not be tested until the first attempt
 *     to send a request there.
 *   char *name
 *     The nickname of the cert to trust (expected) to sign the OCSP responses.
 *     If the corresponding cert cannot be found, SECFailure is returned.
 * RETURN:
 *   Returns SECFailure if an error occurred; SECSuccess otherwise.
 *   The most likely error is that the cert for "name" could not be found
 *   (probably SEC_ERROR_UNKNOWN_CERT).  Other errors are low-level (no memory,
 *   bad database, etc.).
 */
extern SECStatus
CERT_SetOCSPDefaultResponder(CERTCertDBHandle *handle,
			     const char *url, const char *name);

/*
 * FUNCTION: CERT_EnableOCSPDefaultResponder
 *   Turns on use of a default responder when OCSP checking.
 *   If OCSP checking is already enabled, this will make subsequent checks
 *   go directly to the default responder.  (The location of the responder
 *   and the nickname of the responder cert must already be specified.)
 *   If OCSP checking is not enabled, this will be recorded and take effect
 *   whenever it is enabled.
 * INPUTS:
 *   CERTCertDBHandle *handle
 *     Cert database on which OCSP checking should use the default responder.
 * RETURN:
 *   Returns SECFailure if an error occurred; SECSuccess otherwise.
 *   No errors are especially likely unless the caller did not previously
 *   perform a successful call to SetOCSPDefaultResponder (in which case
 *   the error set will be SEC_ERROR_OCSP_NO_DEFAULT_RESPONDER).
 */
extern SECStatus
CERT_EnableOCSPDefaultResponder(CERTCertDBHandle *handle);

/*
 * FUNCTION: CERT_DisableOCSPDefaultResponder
 *   Turns off use of a default responder when OCSP checking.
 *   (Does nothing if use of a default responder is not enabled.)
 * INPUTS:
 *   CERTCertDBHandle *handle
 *     Cert database on which OCSP checking should stop using a default
 *     responder.
 * RETURN:
 *   Returns SECFailure if an error occurred; SECSuccess otherwise.
 *   Errors very unlikely (like random memory corruption...).
 */
extern SECStatus
CERT_DisableOCSPDefaultResponder(CERTCertDBHandle *handle);

/*
 * -------------------------------------------------------
 * The Functions above are those expected to be used by a client
 * providing OCSP status checking along with every cert verification.
 * The functions below are for OCSP testing, debugging, or clients
 * or servers performing more specialized OCSP tasks.
 * -------------------------------------------------------
 */

/*
 * FUNCTION: CERT_CreateOCSPRequest
 *   Creates a CERTOCSPRequest, requesting the status of the certs in 
 *   the given list.
 * INPUTS:
 *   CERTCertList *certList
 *     A list of certs for which status will be requested.
 *     Note that all of these certificates should have the same issuer,
 *     or it's expected the response will be signed by a trusted responder.
 *     If the certs need to be broken up into multiple requests, that
 *     must be handled by the caller (and thus by having multiple calls
 *     to this routine), who knows about where the request(s) are being
 *     sent and whether there are any trusted responders in place.
 *   int64 time
 *     Indicates the time for which the certificate status is to be 
 *     determined -- this may be used in the search for the cert's issuer
 *     but has no effect on the request itself.
 *   PRBool addServiceLocator
 *     If true, the Service Locator extension should be added to the
 *     single request(s) for each cert.
 *   CERTCertificate *signerCert
 *     If non-NULL, means sign the request using this cert.  Otherwise,
 *     do not sign.
 *     XXX note that request signing is not yet supported; see comment in code
 * RETURN:
 *   A pointer to a CERTOCSPRequest structure containing an OCSP request
 *   for the cert list.  On error, null is returned, with an error set
 *   indicating the reason.  This is likely SEC_ERROR_UNKNOWN_ISSUER.
 *   (The issuer is needed to create a request for the certificate.)
 *   Other errors are low-level problems (no memory, bad database, etc.).
 */
extern CERTOCSPRequest *
CERT_CreateOCSPRequest(CERTCertList *certList, int64 time, 
		       PRBool addServiceLocator,
		       CERTCertificate *signerCert);

/*
 * FUNCTION: CERT_AddOCSPAcceptableResponses
 *   Add the AcceptableResponses extension to an OCSP Request.
 * INPUTS:
 *   CERTOCSPRequest *request
 *     The request to which the extension should be added.
 *   ...
 *     A list (of one or more) of SECOidTag -- each of the response types
 *     to be added.  The last OID *must* be SEC_OID_PKIX_OCSP_BASIC_RESPONSE.
 *     (This marks the end of the list, and it must be specified because a
 *     client conforming to the OCSP standard is required to handle the basic
 *     response type.)  The OIDs are not checked in any way.
 * RETURN:
 *   SECSuccess if the extension is added; SECFailure if anything goes wrong.
 *   All errors are internal or low-level problems (e.g. no memory).
 */
extern SECStatus
CERT_AddOCSPAcceptableResponses(CERTOCSPRequest *request, ...);

/* 
 * FUNCTION: CERT_EncodeOCSPRequest
 *   DER encodes an OCSP Request, possibly adding a signature as well.
 *   XXX Signing is not yet supported, however; see comments in code.
 * INPUTS: 
 *   PRArenaPool *arena
 *     The return value is allocated from here.
 *     If a NULL is passed in, allocation is done from the heap instead.
 *   CERTOCSPRequest *request
 *     The request to be encoded.
 *   void *pwArg
 *     Pointer to argument for password prompting, if needed.  (Definitely
 *     not needed if not signing.)
 * RETURN:
 *   Returns a NULL on error and a pointer to the SECItem with the
 *   encoded value otherwise.  Any error is likely to be low-level
 *   (e.g. no memory).
 */
extern SECItem *
CERT_EncodeOCSPRequest(PRArenaPool *arena, CERTOCSPRequest *request, 
		       void *pwArg);

/*
 * FUNCTION: CERT_DecodeOCSPRequest
 *   Decode a DER encoded OCSP Request.
 * INPUTS:
 *   SECItem *src
 *     Pointer to a SECItem holding DER encoded OCSP Request.
 * RETURN:
 *   Returns a pointer to a CERTOCSPRequest containing the decoded request.
 *   On error, returns NULL.  Most likely error is trouble decoding
 *   (SEC_ERROR_OCSP_MALFORMED_REQUEST), or low-level problem (no memory).
 */
extern CERTOCSPRequest *
CERT_DecodeOCSPRequest(SECItem *src);

/*
 * FUNCTION: CERT_DestroyOCSPRequest
 *   Frees an OCSP Request structure.
 * INPUTS:
 *   CERTOCSPRequest *request
 *     Pointer to CERTOCSPRequest to be freed.
 * RETURN:
 *   No return value; no errors.
 */
extern void
CERT_DestroyOCSPRequest(CERTOCSPRequest *request);

/*
 * FUNCTION: CERT_DecodeOCSPResponse
 *   Decode a DER encoded OCSP Response.
 * INPUTS:
 *   SECItem *src
 *     Pointer to a SECItem holding DER encoded OCSP Response.
 * RETURN:
 *   Returns a pointer to a CERTOCSPResponse (the decoded OCSP Response);
 *   the caller is responsible for destroying it.  Or NULL if error (either
 *   response could not be decoded (SEC_ERROR_OCSP_MALFORMED_RESPONSE),
 *   it was of an unexpected type (SEC_ERROR_OCSP_UNKNOWN_RESPONSE_TYPE),
 *   or a low-level or internal error occurred).
 */
extern CERTOCSPResponse *
CERT_DecodeOCSPResponse(SECItem *src);

/*
 * FUNCTION: CERT_DestroyOCSPResponse
 *   Frees an OCSP Response structure.
 * INPUTS:
 *   CERTOCSPResponse *request
 *     Pointer to CERTOCSPResponse to be freed.
 * RETURN:
 *   No return value; no errors.
 */
extern void
CERT_DestroyOCSPResponse(CERTOCSPResponse *response);

/*
 * FUNCTION: CERT_GetEncodedOCSPResponse
 *   Creates and sends a request to an OCSP responder, then reads and
 *   returns the (encoded) response.
 * INPUTS:
 *   PRArenaPool *arena
 *     Pointer to arena from which return value will be allocated.
 *     If NULL, result will be allocated from the heap (and thus should
 *     be freed via SECITEM_FreeItem).
 *   CERTCertList *certList
 *     A list of certs for which status will be requested.
 *     Note that all of these certificates should have the same issuer,
 *     or it's expected the response will be signed by a trusted responder.
 *     If the certs need to be broken up into multiple requests, that
 *     must be handled by the caller (and thus by having multiple calls
 *     to this routine), who knows about where the request(s) are being
 *     sent and whether there are any trusted responders in place.
 *   char *location
 *     The location of the OCSP responder (a URL).
 *   int64 time
 *     Indicates the time for which the certificate status is to be 
 *     determined -- this may be used in the search for the cert's issuer
 *     but has no other bearing on the operation.
 *   PRBool addServiceLocator
 *     If true, the Service Locator extension should be added to the
 *     single request(s) for each cert.
 *   CERTCertificate *signerCert
 *     If non-NULL, means sign the request using this cert.  Otherwise,
 *     do not sign.
 *   void *pwArg
 *     Pointer to argument for password prompting, if needed.  (Definitely
 *     not needed if not signing.)
 * OUTPUTS:
 *   CERTOCSPRequest **pRequest
 *     Pointer in which to store the OCSP request created for the given
 *     list of certificates.  It is only filled in if the entire operation
 *     is successful and the pointer is not null -- and in that case the
 *     caller is then reponsible for destroying it.
 * RETURN:
 *   Returns a pointer to the SECItem holding the response.
 *   On error, returns null with error set describing the reason:
 *	SEC_ERROR_UNKNOWN_ISSUER
 *	SEC_ERROR_CERT_BAD_ACCESS_LOCATION
 *	SEC_ERROR_OCSP_BAD_HTTP_RESPONSE
 *   Other errors are low-level problems (no memory, bad database, etc.).
 */
extern SECItem *
CERT_GetEncodedOCSPResponse(PRArenaPool *arena, CERTCertList *certList,
			    char *location, int64 time,
			    PRBool addServiceLocator,
			    CERTCertificate *signerCert, void *pwArg,
			    CERTOCSPRequest **pRequest);

/*
 * FUNCTION: CERT_VerifyOCSPResponseSignature
 *   Check the signature on an OCSP Response.  Will also perform a
 *   verification of the signer's certificate.  Note, however, that a
 *   successful verification does not make any statement about the
 *   signer's *authority* to provide status for the certificate(s),
 *   that must be checked individually for each certificate.
 * INPUTS:
 *   CERTOCSPResponse *response
 *     Pointer to response structure with signature to be checked.
 *   CERTCertDBHandle *handle
 *     Pointer to CERTCertDBHandle for certificate DB to use for verification.
 *   void *pwArg
 *     Pointer to argument for password prompting, if needed.
 *   CERTCertificate *issuerCert
 *     Issuer of the certificate that generated the OCSP request.
 * OUTPUTS:
 *   CERTCertificate **pSignerCert
 *     Pointer in which to store signer's certificate; only filled-in if
 *     non-null.
 * RETURN:
 *   Returns SECSuccess when signature is valid, anything else means invalid.
 *   Possible errors set:
 *	SEC_ERROR_OCSP_MALFORMED_RESPONSE - unknown type of ResponderID
 *	SEC_ERROR_INVALID_TIME - bad format of "ProducedAt" time
 *	SEC_ERROR_UNKNOWN_SIGNER - signer's cert could not be found
 *	SEC_ERROR_BAD_SIGNATURE - the signature did not verify
 *   Other errors are any of the many possible failures in cert verification
 *   (e.g. SEC_ERROR_REVOKED_CERTIFICATE, SEC_ERROR_UNTRUSTED_ISSUER) when
 *   verifying the signer's cert, or low-level problems (no memory, etc.)
 */
extern SECStatus
CERT_VerifyOCSPResponseSignature(CERTOCSPResponse *response,	
				 CERTCertDBHandle *handle, void *pwArg,
				 CERTCertificate **pSignerCert,
				 CERTCertificate *issuerCert);

/*
 * FUNCTION: CERT_GetOCSPAuthorityInfoAccessLocation
 *   Get the value of the URI of the OCSP responder for the given cert.
 *   This is found in the (optional) Authority Information Access extension
 *   in the cert.
 * INPUTS:
 *   CERTCertificate *cert
 *     The certificate being examined.
 * RETURN:
 *   char *
 *     A copy of the URI for the OCSP method, if found.  If either the
 *     extension is not present or it does not contain an entry for OCSP,
 *     SEC_ERROR_EXTENSION_NOT_FOUND will be set and a NULL returned.
 *     Any other error will also result in a NULL being returned.
 *     
 *     This result should be freed (via PORT_Free) when no longer in use.
 */
extern char *
CERT_GetOCSPAuthorityInfoAccessLocation(CERTCertificate *cert);

/*
 * FUNCTION: CERT_CheckOCSPStatus
 *   Checks the status of a certificate via OCSP.  Will only check status for
 *   a certificate that has an AIA (Authority Information Access) extension
 *   for OCSP *or* when a "default responder" is specified and enabled.
 *   (If no AIA extension for OCSP and no default responder in place, the
 *   cert is considered to have a good status and SECSuccess is returned.)
 * INPUTS:
 *   CERTCertDBHandle *handle
 *     certificate DB of the cert that is being checked
 *   CERTCertificate *cert
 *     the certificate being checked
 *   XXX in the long term also need a boolean parameter that specifies
 *	whether to check the cert chain, as well; for now we check only
 *	the leaf (the specified certificate)
 *   int64 time
 *     time for which status is to be determined
 *   void *pwArg
 *     argument for password prompting, if needed
 * RETURN:
 *   Returns SECSuccess if an approved OCSP responder "knows" the cert
 *   *and* returns a non-revoked status for it; SECFailure otherwise,
 *   with an error set describing the reason:
 *
 *	SEC_ERROR_OCSP_BAD_HTTP_RESPONSE
 *	SEC_ERROR_OCSP_FUTURE_RESPONSE
 *	SEC_ERROR_OCSP_MALFORMED_REQUEST
 *	SEC_ERROR_OCSP_MALFORMED_RESPONSE
 *	SEC_ERROR_OCSP_OLD_RESPONSE
 *	SEC_ERROR_OCSP_REQUEST_NEEDS_SIG
 *	SEC_ERROR_OCSP_SERVER_ERROR
 *	SEC_ERROR_OCSP_TRY_SERVER_LATER
 *	SEC_ERROR_OCSP_UNAUTHORIZED_REQUEST
 *	SEC_ERROR_OCSP_UNAUTHORIZED_RESPONSE
 *	SEC_ERROR_OCSP_UNKNOWN_CERT
 *	SEC_ERROR_OCSP_UNKNOWN_RESPONSE_STATUS
 *	SEC_ERROR_OCSP_UNKNOWN_RESPONSE_TYPE
 *
 *	SEC_ERROR_BAD_SIGNATURE
 *	SEC_ERROR_CERT_BAD_ACCESS_LOCATION
 *	SEC_ERROR_INVALID_TIME
 *	SEC_ERROR_REVOKED_CERTIFICATE
 *	SEC_ERROR_UNKNOWN_ISSUER
 *	SEC_ERROR_UNKNOWN_SIGNER
 *
 *   Other errors are any of the many possible failures in cert verification
 *   (e.g. SEC_ERROR_REVOKED_CERTIFICATE, SEC_ERROR_UNTRUSTED_ISSUER) when
 *   verifying the signer's cert, or low-level problems (error allocating
 *   memory, error performing ASN.1 decoding, etc.).
 */    
extern SECStatus 
CERT_CheckOCSPStatus(CERTCertDBHandle *handle, CERTCertificate *cert,
		     int64 time, void *pwArg);
/************************************************************************/
SEC_END_PROTOS

#endif /* _OCSP_H_ */
