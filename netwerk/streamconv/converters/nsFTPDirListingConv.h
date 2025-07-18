/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef __nsftpdirlistingdconv__h__
#define __nsftpdirlistingdconv__h__

#include "nsIStreamConverter.h"
#include "nsIChannel.h"
#include "nsIURI.h"
#include "nsString.h"

#include "nsIFactory.h"

#define NS_FTPDIRLISTINGCONVERTER_CID                         \
{ /* 14C0E880-623E-11d3-A178-0050041CAF44 */         \
    0x14c0e880,                                      \
    0x623e,                                          \
    0x11d3,                                          \
    {0xa1, 0x78, 0x00, 0x50, 0x04, 0x1c, 0xaf, 0x44}       \
}
static NS_DEFINE_CID(kFTPDirListingConverterCID, NS_FTPDIRLISTINGCONVERTER_CID);

// The nsFTPDirListingConv stream converter converts a stream of type "text/ftp-dir-SERVER_TYPE"
// (where SERVER_TYPE is one of the following):
//
// SERVER TYPES:
// generic
// unix
// dcts
// ncsa
// peter_lewis
// machten
// cms
// tcpc
// vms
// nt
// eplf
//
// nsFTPDirListingConv converts the raw ascii text directory generated via a FTP
// LIST or NLST command, to the application/http-index-format MIME-type.
// For more info see: http://www.area.com/~roeber/file_format.html

typedef enum _FTP_Server_Type {
    GENERIC,
    UNIX,
    DCTS,
    NCSA,
    PETER_LEWIS,
    MACHTEN,
    CMS,
    TCPC,
    VMS,
    NT,
    EPLF,
    OS_2,
    ERROR_TYPE
} FTP_Server_Type;

typedef enum _FTPentryType {
    Dir,
    File,
    Link
} FTPentryType;


// indexEntry is the data structure used to maintain directory entry information.
class indexEntry {
public:
    indexEntry() { mContentLen = 0; mType = File; mSupressSize = PR_FALSE; };

    nsCString       mName;              // the file or dir name
    FTPentryType    mType;              
    PRInt32         mContentLen;        // length of the file
    nsCString       mContentType;       // type of the file
    PRExplodedTime  mMDTM;              // modified time
    PRBool          mSupressSize;       // supress the size info from display
};

class nsFTPDirListingConv : public nsIStreamConverter {
public:
    // nsISupports methods
    NS_DECL_ISUPPORTS

    // nsIStreamConverter methods
    NS_DECL_NSISTREAMCONVERTER

    // nsIStreamListener methods
    NS_DECL_NSISTREAMLISTENER

    // nsIRequestObserver methods
    NS_DECL_NSIREQUESTOBSERVER

    // nsFTPDirListingConv methods
    nsFTPDirListingConv();
    virtual ~nsFTPDirListingConv();
    nsresult Init();

    // For factory creation.
    static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult) {
        nsresult rv;
        if (aOuter)
            return NS_ERROR_NO_AGGREGATION;

        nsFTPDirListingConv* _s = new nsFTPDirListingConv();
        if (_s == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;
        NS_ADDREF(_s);
        rv = _s->Init();
        if (NS_FAILED(rv)) {
            delete _s;
            return rv;
        }
        rv = _s->QueryInterface(aIID, aResult);
        NS_RELEASE(_s);
        return rv;
    }

private:
    // Get the application/http-index-format headers
    nsresult GetHeaders(nsACString& str, nsIURI* uri);

    // util parsing methods
    PRInt8   MonthNumber(const char *aCStr);
    PRBool   IsLSDate(char *aCStr);

    // date conversion/parsing methods
    PRBool   ConvertUNIXDate(char *aCStr, PRExplodedTime& outDate);
    PRBool   ConvertDOSDate(char *aCStr, PRExplodedTime& outDate);

    // line parsing methods
    nsresult ParseLSLine(char *aLine, indexEntry *aEntry);
    nsresult ParseVMSLine(char *aLine, indexEntry *aEntry);

    void     InitPRExplodedTime(PRExplodedTime& aTime);
    PRBool   ls_lCandidate(const char *aLine); 
    char*    DigestBufferLines(char *aBuffer, nsCString &aString);

    // member data
    FTP_Server_Type     mServerType;        // what kind of server is the data coming from?
    nsCAutoString       mBuffer;            // buffered data.
    PRBool              mSentHeading;       // have we sent 100, 101, 200, and 300 lines yet?


    nsIStreamListener   *mFinalListener; // this guy gets the converted data via his OnDataAvailable()
    nsIChannel          *mPartChannel;  // the channel for the given part we're processing.
                                        // one channel per part.
};

#endif /* __nsftpdirlistingdconv__h__ */
