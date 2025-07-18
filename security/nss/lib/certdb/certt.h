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
 * certt.h - public data structures for the certificate library
 *
 * $Id: certt.h,v 1.16.6.1 2002/04/10 03:26:33 cltbld%netscape.com Exp $
 */
#ifndef _CERTT_H_
#define _CERTT_H_

#include "prclist.h"
#include "pkcs11t.h"
#include "seccomon.h"
#include "secmodt.h"
#include "secoidt.h"
#include "plarena.h"
#include "prcvar.h"
#include "nssilock.h"
#include "prio.h"
#include "prmon.h"

/* Stan data types */
struct NSSCertificateStr;
struct NSSTrustDomainStr;

/* Non-opaque objects */
typedef struct CERTAVAStr                        CERTAVA;
typedef struct CERTAttributeStr                  CERTAttribute;
typedef struct CERTAuthInfoAccessStr             CERTAuthInfoAccess;
typedef struct CERTAuthKeyIDStr                  CERTAuthKeyID;
typedef struct CERTBasicConstraintsStr           CERTBasicConstraints;
#ifdef NSS_CLASSIC
typedef struct CERTCertDBHandleStr               CERTCertDBHandle;
#else
typedef struct NSSTrustDomainStr                 CERTCertDBHandle;
#endif
typedef struct CERTCertExtensionStr              CERTCertExtension;
typedef struct CERTCertKeyStr                    CERTCertKey;
typedef struct CERTCertListStr                   CERTCertList;
typedef struct CERTCertListNodeStr               CERTCertListNode;
typedef struct CERTCertNicknamesStr              CERTCertNicknames;
typedef struct CERTCertTrustStr                  CERTCertTrust;
typedef struct CERTCertificateStr                CERTCertificate;
typedef struct CERTCertificateListStr            CERTCertificateList;
typedef struct CERTCertificateRequestStr         CERTCertificateRequest;
typedef struct CERTCrlStr                        CERTCrl;
typedef struct CERTCrlDistributionPointsStr      CERTCrlDistributionPoints; 
typedef struct CERTCrlEntryStr                   CERTCrlEntry;
typedef struct CERTCrlHeadNodeStr                CERTCrlHeadNode;
typedef struct CERTCrlKeyStr                     CERTCrlKey;
typedef struct CERTCrlNodeStr                    CERTCrlNode;
typedef struct CERTDERCertsStr                   CERTDERCerts;
typedef struct CERTDistNamesStr                  CERTDistNames;
typedef struct CERTGeneralNameStr                CERTGeneralName;
typedef struct CERTGeneralNameListStr            CERTGeneralNameList;
typedef struct CERTIssuerAndSNStr                CERTIssuerAndSN;
typedef struct CERTNameStr                       CERTName;
typedef struct CERTNameConstraintStr             CERTNameConstraint;
typedef struct CERTNameConstraintsStr            CERTNameConstraints;
typedef struct CERTOKDomainNameStr               CERTOKDomainName;
typedef struct CERTPublicKeyAndChallengeStr      CERTPublicKeyAndChallenge;
typedef struct CERTRDNStr                        CERTRDN;
typedef struct CERTSignedCrlStr                  CERTSignedCrl;
typedef struct CERTSignedDataStr                 CERTSignedData;
typedef struct CERTStatusConfigStr               CERTStatusConfig;
typedef struct CERTSubjectListStr                CERTSubjectList;
typedef struct CERTSubjectNodeStr                CERTSubjectNode;
typedef struct CERTSubjectPublicKeyInfoStr       CERTSubjectPublicKeyInfo;
typedef struct CERTValidityStr                   CERTValidity;
typedef struct CERTVerifyLogStr                  CERTVerifyLog;
typedef struct CERTVerifyLogNodeStr              CERTVerifyLogNode;
typedef struct CRLDistributionPointStr           CRLDistributionPoint;

/* CRL extensions type */
typedef unsigned long CERTCrlNumber;

/*
** An X.500 AVA object
*/
struct CERTAVAStr {
    SECItem type;
    SECItem value;
};

/*
** An X.500 RDN object
*/
struct CERTRDNStr {
    CERTAVA **avas;
};

/*
** An X.500 name object
*/
struct CERTNameStr {
    PRArenaPool *arena;
    CERTRDN **rdns;
};

/*
** An X.509 validity object
*/
struct CERTValidityStr {
    PRArenaPool *arena;
    SECItem notBefore;
    SECItem notAfter;
};

/*
 * A serial number and issuer name, which is used as a database key
 */
struct CERTCertKeyStr {
    SECItem serialNumber;
    SECItem derIssuer;
};

/*
** A signed data object. Used to implement the "signed" macro used
** in the X.500 specs.
*/
struct CERTSignedDataStr {
    SECItem data;
    SECAlgorithmID signatureAlgorithm;
    SECItem signature;
};

/*
** An X.509 subject-public-key-info object
*/
struct CERTSubjectPublicKeyInfoStr {
    PRArenaPool *arena;
    SECAlgorithmID algorithm;
    SECItem subjectPublicKey;
};

struct CERTPublicKeyAndChallengeStr {
    SECItem spki;
    SECItem challenge;
};

struct CERTCertTrustStr {
    unsigned int sslFlags;
    unsigned int emailFlags;
    unsigned int objectSigningFlags;
};

/*
 * defined the types of trust that exist
 */
typedef enum {
    trustSSL = 0,
    trustEmail = 1,
    trustObjectSigning = 2,
    trustTypeNone = 3
} SECTrustType;

#define SEC_GET_TRUST_FLAGS(trust,type) \
        (((type)==trustSSL)?((trust)->sslFlags): \
	 (((type)==trustEmail)?((trust)->emailFlags): \
	  (((type)==trustObjectSigning)?((trust)->objectSigningFlags):0)))

/*
** An X.509.3 certificate extension
*/
struct CERTCertExtensionStr {
    SECItem id;
    SECItem critical;
    SECItem value;
};

struct CERTSubjectNodeStr {
    struct CERTSubjectNodeStr *next;
    struct CERTSubjectNodeStr *prev;
    SECItem certKey;
    SECItem keyID;
};

struct CERTSubjectListStr {
    PRArenaPool *arena;
    int ncerts;
    char *emailAddr;
    CERTSubjectNode *head;
    CERTSubjectNode *tail; /* do we need tail? */
    void *entry;
};

/*
** An X.509 certificate object (the unsigned form)
*/
struct CERTCertificateStr {
    /* the arena is used to allocate any data structures that have the same
     * lifetime as the cert.  This is all stuff that hangs off of the cert
     * structure, and is all freed at the same time.  I is used when the
     * cert is decoded, destroyed, and at some times when it changes
     * state
     */
    PRArenaPool *arena;

    /* The following fields are static after the cert has been decoded */
    char *subjectName;
    char *issuerName;
    CERTSignedData signatureWrap;	/* XXX */
    SECItem derCert;			/* original DER for the cert */
    SECItem derIssuer;			/* DER for issuer name */
    SECItem derSubject;			/* DER for subject name */
    SECItem derPublicKey;		/* DER for the public key */
    SECItem certKey;			/* database key for this cert */
    SECItem version;
    SECItem serialNumber;
    SECAlgorithmID signature;
    CERTName issuer;
    CERTValidity validity;
    CERTName subject;
    CERTSubjectPublicKeyInfo subjectPublicKeyInfo;
    SECItem issuerID;
    SECItem subjectID;
    CERTCertExtension **extensions;
    char *emailAddr;
    CERTCertDBHandle *dbhandle;
    SECItem subjectKeyID;	/* x509v3 subject key identifier */
    PRBool keyIDGenerated;	/* was the keyid generated? */
    unsigned int keyUsage;	/* what uses are allowed for this cert */
    unsigned int rawKeyUsage;	/* value of the key usage extension */
    PRBool keyUsagePresent;	/* was the key usage extension present */
    unsigned int nsCertType;	/* value of the ns cert type extension */

    /* these values can be set by the application to bypass certain checks
     * or to keep the cert in memory for an entire session.
     * XXX - need an api to set these
     */
    PRBool keepSession;			/* keep this cert for entire session*/
    PRBool timeOK;			/* is the bad validity time ok? */
    CERTOKDomainName *domainOK;		/* these domain names are ok */

    /*
     * these values can change when the cert changes state.  These state
     * changes include transitions from temp to perm or vice-versa, and
     * changes of trust flags
     */
    PRBool isperm;
    PRBool istemp;
    char *nickname;
    char *dbnickname;
    struct NSSCertificateStr *nssCertificate;	/* This is Stan stuff. */
    CERTCertTrust *trust;

    /* the reference count is modified whenever someone looks up, dups
     * or destroys a certificate
     */
    int referenceCount;

    /* The subject list is a list of all certs with the same subject name.
     * It can be modified any time a cert is added or deleted from either
     * the in-memory(temporary) or on-disk(permanent) database.
     */
    CERTSubjectList *subjectList;

    /* these fields are used by client GUI code to keep track of ssl sockets
     * that are blocked waiting on GUI feedback related to this cert.
     * XXX - these should be moved into some sort of application specific
     *       data structure.  They are only used by the browser right now.
     */
    struct SECSocketNode *socketlist;
    int socketcount;
    struct SECSocketNode *authsocketlist;
    int authsocketcount;

    /* This is PKCS #11 stuff. */
    PK11SlotInfo *slot;		/*if this cert came of a token, which is it*/
    CK_OBJECT_HANDLE pkcs11ID;	/*and which object on that token is it */
    PRBool ownSlot;		/*true if the cert owns the slot reference */
};
#define SEC_CERTIFICATE_VERSION_1		0	/* default created */
#define SEC_CERTIFICATE_VERSION_2		1	/* v2 */
#define SEC_CERTIFICATE_VERSION_3		2	/* v3 extensions */

#define SEC_CRL_VERSION_1		0	/* default */
#define SEC_CRL_VERSION_2		1	/* v2 extensions */

/*
 * used to identify class of cert in mime stream code
 */
#define SEC_CERT_CLASS_CA	1
#define SEC_CERT_CLASS_SERVER	2
#define SEC_CERT_CLASS_USER	3
#define SEC_CERT_CLASS_EMAIL	4

struct CERTDERCertsStr {
    PRArenaPool *arena;
    int numcerts;
    SECItem *rawCerts;
};

/*
** A PKCS ? Attribute
** XXX this is duplicated through out the code, it *should* be moved
** to a central location.  Where would be appropriate?
*/
struct CERTAttributeStr {
    SECItem attrType;
    SECItem **attrValue;
};

/*
** A PKCS#10 certificate-request object (the unsigned form)
*/
struct CERTCertificateRequestStr {
    PRArenaPool *arena;
    SECItem version;
    CERTName subject;
    CERTSubjectPublicKeyInfo subjectPublicKeyInfo;
    SECItem **attributes;
};
#define SEC_CERTIFICATE_REQUEST_VERSION		0	/* what we *create* */


/*
** A certificate list object.
*/
struct CERTCertificateListStr {
    SECItem *certs;
    int len;					/* number of certs */
    PRArenaPool *arena;
};

struct CERTCertListNodeStr {
    PRCList links;
    CERTCertificate *cert;
    void *appData;
};

struct CERTCertListStr {
    PRCList list;
    PRArenaPool *arena;
};

#define CERT_LIST_HEAD(l) ((CERTCertListNode *)PR_LIST_HEAD(&l->list))
#define CERT_LIST_NEXT(n) ((CERTCertListNode *)n->links.next)
#define CERT_LIST_END(n,l) (((void *)n) == ((void *)&l->list))

struct CERTCrlEntryStr {
    SECItem serialNumber;
    SECItem revocationDate;
    CERTCertExtension **extensions;    
};

struct CERTCrlStr {
    PRArenaPool *arena;
    SECItem version;
    SECAlgorithmID signatureAlg;
    SECItem derName;
    CERTName name;
    SECItem lastUpdate;
    SECItem nextUpdate;				/* optional for x.509 CRL  */
    CERTCrlEntry **entries;
    CERTCertExtension **extensions;    
};

struct CERTCrlKeyStr {
    SECItem derName;
    SECItem dummy;			/* The decoder can not skip a primitive,
					   this serves as a place holder for the
					   decoder to finish its task only
					*/
};

struct CERTSignedCrlStr {
    PRArenaPool *arena;
    CERTCrl crl;
    void *reserved1;
    PRBool reserved2;
    PRBool isperm;
    PRBool istemp;
    int referenceCount;
    CERTCertDBHandle *dbhandle;
    CERTSignedData signatureWrap;	/* XXX */
    char *url;
    SECItem *derCrl;
    PK11SlotInfo *slot;
    CK_OBJECT_HANDLE pkcs11ID;
};


struct CERTCrlHeadNodeStr {
    PRArenaPool *arena;
    CERTCertDBHandle *dbhandle;
    CERTCrlNode *first;
    CERTCrlNode *last;
};


struct CERTCrlNodeStr {
    CERTCrlNode *next;
    int 	type;
    CERTSignedCrl *crl;
};


/*
 * Array of X.500 Distinguished Names
 */
struct CERTDistNamesStr {
    PRArenaPool *arena;
    int nnames;
    SECItem  *names;
    void *head; /* private */
};


#define NS_CERT_TYPE_SSL_CLIENT		(0x80)	/* bit 0 */
#define NS_CERT_TYPE_SSL_SERVER		(0x40)  /* bit 1 */
#define NS_CERT_TYPE_EMAIL		(0x20)  /* bit 2 */
#define NS_CERT_TYPE_OBJECT_SIGNING	(0x10)  /* bit 3 */
#define NS_CERT_TYPE_RESERVED		(0x08)  /* bit 4 */
#define NS_CERT_TYPE_SSL_CA		(0x04)  /* bit 5 */
#define NS_CERT_TYPE_EMAIL_CA		(0x02)  /* bit 6 */
#define NS_CERT_TYPE_OBJECT_SIGNING_CA	(0x01)  /* bit 7 */

#define EXT_KEY_USAGE_TIME_STAMP        (0x8000)
#define EXT_KEY_USAGE_STATUS_RESPONDER	(0x4000)

#define NS_CERT_TYPE_APP ( NS_CERT_TYPE_SSL_CLIENT | \
			  NS_CERT_TYPE_SSL_SERVER | \
			  NS_CERT_TYPE_EMAIL | \
			  NS_CERT_TYPE_OBJECT_SIGNING )

#define NS_CERT_TYPE_CA ( NS_CERT_TYPE_SSL_CA | \
			 NS_CERT_TYPE_EMAIL_CA | \
			 NS_CERT_TYPE_OBJECT_SIGNING_CA | \
			 EXT_KEY_USAGE_STATUS_RESPONDER )
typedef enum {
    certUsageSSLClient = 0,
    certUsageSSLServer = 1,
    certUsageSSLServerWithStepUp = 2,
    certUsageSSLCA = 3,
    certUsageEmailSigner = 4,
    certUsageEmailRecipient = 5,
    certUsageObjectSigner = 6,
    certUsageUserCertImport = 7,
    certUsageVerifyCA = 8,
    certUsageProtectedObjectSigner = 9,
    certUsageStatusResponder = 10,
    certUsageAnyCA = 11
} SECCertUsage;

/*
 * Does the cert belong to the user, a peer, or a CA.
 */
typedef enum {
    certOwnerUser = 0,
    certOwnerPeer = 1,
    certOwnerCA = 2
} CERTCertOwner;

/*
 * This enum represents the state of validity times of a certificate
 */
typedef enum {
    secCertTimeValid = 0,
    secCertTimeExpired = 1,
    secCertTimeNotValidYet = 2
} SECCertTimeValidity;

/*
 * Interface for getting certificate nickname strings out of the database
 */

/* these are values for the what argument below */
#define SEC_CERT_NICKNAMES_ALL		1
#define SEC_CERT_NICKNAMES_USER		2
#define SEC_CERT_NICKNAMES_SERVER	3
#define SEC_CERT_NICKNAMES_CA		4

struct CERTCertNicknamesStr {
    PRArenaPool *arena;
    void *head;
    int numnicknames;
    char **nicknames;
    int what;
    int totallen;
};

struct CERTIssuerAndSNStr {
    SECItem derIssuer;
    CERTName issuer;
    SECItem serialNumber;
};


/* X.509 v3 Key Usage Extension flags */
#define KU_DIGITAL_SIGNATURE		(0x80)	/* bit 0 */
#define KU_NON_REPUDIATION		(0x40)  /* bit 1 */
#define KU_KEY_ENCIPHERMENT		(0x20)  /* bit 2 */
#define KU_DATA_ENCIPHERMENT		(0x10)  /* bit 3 */
#define KU_KEY_AGREEMENT		(0x08)  /* bit 4 */
#define KU_KEY_CERT_SIGN		(0x04)  /* bit 5 */
#define KU_CRL_SIGN			(0x02)  /* bit 6 */
#define KU_ALL				(KU_DIGITAL_SIGNATURE | \
					 KU_NON_REPUDIATION | \
					 KU_KEY_ENCIPHERMENT | \
					 KU_DATA_ENCIPHERMENT | \
					 KU_KEY_AGREEMENT | \
					 KU_KEY_CERT_SIGN | \
					 KU_CRL_SIGN)

/* This value will not occur in certs.  It is used internally for the case
 * when the key type is not know ahead of time and either key agreement or
 * key encipherment are the correct value based on key type
 */
#define KU_KEY_AGREEMENT_OR_ENCIPHERMENT (0x4000)

/* internal bits that do not match bits in the x509v3 spec, but are used
 * for similar purposes
 */
#define KU_NS_GOVT_APPROVED		(0x8000) /*don't make part of KU_ALL!*/
/*
 * x.509 v3 Basic Constraints Extension
 * If isCA is false, the pathLenConstraint is ignored.
 * Otherwise, the following pathLenConstraint values will apply:
 *	< 0 - there is no limit to the certificate path
 *	0   - CA can issues end-entity certificates only
 *	> 0 - the number of certificates in the certificate path is
 *	      limited to this number
 */
#define CERT_UNLIMITED_PATH_CONSTRAINT -2

struct CERTBasicConstraintsStr {
    PRBool isCA;			/* on if is CA */
    int pathLenConstraint;		/* maximum number of certificates that can be
					   in the cert path.  Only applies to a CA
					   certificate; otherwise, it's ignored.
					 */
};

/* Maximum length of a certificate chain */
#define CERT_MAX_CERT_CHAIN 20

/* x.509 v3 Reason Falgs, used in CRLDistributionPoint Extension */
#define RF_UNUSED			(0x80)	/* bit 0 */
#define RF_KEY_COMPROMISE		(0x40)  /* bit 1 */
#define RF_CA_COMPROMISE		(0x20)  /* bit 2 */
#define RF_AFFILIATION_CHANGED		(0x10)  /* bit 3 */
#define RF_SUPERSEDED			(0x08)  /* bit 4 */
#define RF_CESSATION_OF_OPERATION	(0x04)  /* bit 5 */
#define RF_CERTIFICATE_HOLD		(0x02)  /* bit 6 */

/* If we needed to extract the general name field, use this */
/* General Name types */
typedef enum {
    certOtherName = 1,
    certRFC822Name = 2,
    certDNSName = 3,
    certX400Address = 4,
    certDirectoryName = 5,
    certEDIPartyName = 6,
    certURI = 7,
    certIPAddress = 8,
    certRegisterID = 9
} CERTGeneralNameType;


typedef struct OtherNameStr {
    SECItem          name;
    SECItem          oid;
}OtherName;



struct CERTGeneralNameStr {
    CERTGeneralNameType type;		/* name type */
    union {
	CERTName directoryName;         /* distinguish name */
	OtherName  OthName;		/* Other Name */
	SECItem other;                  /* the rest of the name forms */
    }name;
    SECItem derDirectoryName;		/* this is saved to simplify directory name
					   comparison */
    PRCList l;
};

struct CERTGeneralNameListStr {
    PRArenaPool *arena;
    CERTGeneralName *name;
    int refCount;
    int len;
    PZLock *lock;
};

struct CERTNameConstraintStr {
    CERTGeneralName  name;
    SECItem          DERName;
    SECItem          min;
    SECItem          max;
    PRCList          l;
};


struct CERTNameConstraintsStr {
    CERTNameConstraint  *permited;
    CERTNameConstraint  *excluded;
    SECItem             **DERPermited;
    SECItem             **DERExcluded;
};


/* X.509 v3 Authority Key Identifier extension.  For the authority certificate
   issuer field, we only support URI now.
 */
struct CERTAuthKeyIDStr {
    SECItem keyID;			/* unique key identifier */
    CERTGeneralName *authCertIssuer;	/* CA's issuer name.  End with a NULL */
    SECItem authCertSerialNumber;	/* CA's certificate serial number */
    SECItem **DERAuthCertIssuer;	/* This holds the DER encoded format of
					   the authCertIssuer field. It is used
					   by the encoding engine. It should be
					   used as a read only field by the caller.
					*/
};

/* x.509 v3 CRL Distributeion Point */

/*
 * defined the types of CRL Distribution points
 */
typedef enum {
    generalName = 1,			/* only support this for now */
    relativeDistinguishedName = 2
} DistributionPointTypes;

struct CRLDistributionPointStr {
    DistributionPointTypes distPointType;
    union {
	CERTGeneralName *fullName;
	CERTRDN relativeName;
    } distPoint;
    SECItem reasons;
    CERTGeneralName *crlIssuer;
    
    /* Reserved for internal use only*/
    SECItem derDistPoint;
    SECItem derRelativeName;
    SECItem **derCrlIssuer;
    SECItem **derFullName;
    SECItem bitsmap;
};

struct CERTCrlDistributionPointsStr {
    CRLDistributionPoint **distPoints;
};

/*
 * This structure is used to keep a log of errors when verifying
 * a cert chain.  This allows multiple errors to be reported all at
 * once.
 */
struct CERTVerifyLogNodeStr {
    CERTCertificate *cert;	/* what cert had the error */
    long error;			/* what error was it? */
    unsigned int depth;		/* how far up the chain are we */
    void *arg;			/* error specific argument */
    struct CERTVerifyLogNodeStr *next; /* next in the list */
    struct CERTVerifyLogNodeStr *prev; /* next in the list */
};


struct CERTVerifyLogStr {
    PRArenaPool *arena;
    unsigned int count;
    struct CERTVerifyLogNodeStr *head;
    struct CERTVerifyLogNodeStr *tail;
};


struct CERTOKDomainNameStr {
    CERTOKDomainName *next;
    char              name[1]; /* actual length may be longer. */
};


typedef SECStatus (PR_CALLBACK *CERTStatusChecker) (CERTCertDBHandle *handle,
						    CERTCertificate *cert,
						    int64 time,
						    void *pwArg);

typedef SECStatus (PR_CALLBACK *CERTStatusDestroy) (CERTStatusConfig *handle);

struct CERTStatusConfigStr {
    CERTStatusChecker statusChecker;	/* NULL means no checking enabled */
    CERTStatusDestroy statusDestroy;	/* enabled or no, will clean up */
    void *statusContext;		/* cx specific to checking protocol */
};

struct CERTAuthInfoAccessStr {
    SECItem method;
    SECItem derLocation;
    CERTGeneralName *location;		/* decoded location */
};


/* This is the typedef for the callback passed to CERT_OpenCertDB() */
/* callback to return database name based on version number */
typedef char * (*CERTDBNameFunc)(void *arg, int dbVersion);

/*
 * types of cert packages that we can decode
 */
typedef enum {
    certPackageNone = 0,
    certPackageCert = 1,
    certPackagePKCS7 = 2,
    certPackageNSCertSeq = 3,
    certPackageNSCertWrap = 4
} CERTPackageType;

/*
 * these types are for the PKIX Certificate Policies extension
 */
typedef struct {
    SECOidTag oid;
    SECItem qualifierID;
    SECItem qualifierValue;
} CERTPolicyQualifier;

typedef struct {
    SECOidTag oid;
    SECItem policyID;
    CERTPolicyQualifier **policyQualifiers;
} CERTPolicyInfo;

typedef struct {
    PRArenaPool *arena;
    CERTPolicyInfo **policyInfos;
} CERTCertificatePolicies;

typedef struct {
    SECItem organization;
    SECItem **noticeNumbers;
} CERTNoticeReference;

typedef struct {
    PRArenaPool *arena;
    CERTNoticeReference noticeReference;
    SECItem derNoticeReference;
    SECItem displayText;
} CERTUserNotice;

typedef struct {
    PRArenaPool *arena;
    SECItem **oids;
} CERTOidSequence;


/* XXX Lisa thinks the template declarations belong in cert.h, not here? */

#include "secasn1t.h"	/* way down here because I expect template stuff to
			 * move out of here anyway */

SEC_BEGIN_PROTOS

extern const SEC_ASN1Template CERT_CertificateRequestTemplate[];
extern const SEC_ASN1Template CERT_CertificateTemplate[];
extern const SEC_ASN1Template SEC_SignedCertificateTemplate[];
extern const SEC_ASN1Template CERT_CertExtensionTemplate[];
extern const SEC_ASN1Template CERT_SequenceOfCertExtensionTemplate[];
extern const SEC_ASN1Template SECKEY_PublicKeyTemplate[];
extern const SEC_ASN1Template CERT_SubjectPublicKeyInfoTemplate[];
extern const SEC_ASN1Template CERT_ValidityTemplate[];
extern const SEC_ASN1Template CERT_PublicKeyAndChallengeTemplate[];
extern const SEC_ASN1Template SEC_CertSequenceTemplate[];

extern const SEC_ASN1Template CERT_IssuerAndSNTemplate[];
extern const SEC_ASN1Template CERT_NameTemplate[];
extern const SEC_ASN1Template CERT_SetOfSignedCrlTemplate[];
extern const SEC_ASN1Template CERT_RDNTemplate[];
extern const SEC_ASN1Template CERT_SignedDataTemplate[];
extern const SEC_ASN1Template CERT_CrlTemplate[];

/*
** XXX should the attribute stuff be centralized for all of ns/security?
*/
extern const SEC_ASN1Template CERT_AttributeTemplate[];
extern const SEC_ASN1Template CERT_SetOfAttributeTemplate[];

/* These functions simply return the address of the above-declared templates.
** This is necessary for Windows DLLs.  Sigh.
*/
SEC_ASN1_CHOOSER_DECLARE(CERT_CertificateRequestTemplate)
SEC_ASN1_CHOOSER_DECLARE(CERT_CertificateTemplate)
SEC_ASN1_CHOOSER_DECLARE(CERT_CrlTemplate)
SEC_ASN1_CHOOSER_DECLARE(CERT_IssuerAndSNTemplate)
SEC_ASN1_CHOOSER_DECLARE(CERT_NameTemplate)
SEC_ASN1_CHOOSER_DECLARE(CERT_SetOfSignedCrlTemplate)
SEC_ASN1_CHOOSER_DECLARE(CERT_SignedDataTemplate)
SEC_ASN1_CHOOSER_DECLARE(CERT_SubjectPublicKeyInfoTemplate)
SEC_ASN1_CHOOSER_DECLARE(SEC_SignedCertificateTemplate)

SEC_END_PROTOS

#endif /* _CERTT_H_ */
