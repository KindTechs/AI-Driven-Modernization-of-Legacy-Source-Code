/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com> (original author)
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

#ifndef nsNetCID_h__
#define nsNetCID_h__


/******************************************************************************
 * netwerk/base/ classes
 */

// service implementing nsIIOService.
#define NS_IOSERVICE_CLASSNAME \
    "nsIOService"
#define NS_IOSERVICE_CONTRACTID \
    "@mozilla.org/network/io-service;1"
#define NS_IOSERVICE_CID                             \
{ /* 9ac9e770-18bc-11d3-9337-00104ba0fd40 */         \
    0x9ac9e770,                                      \
    0x18bc,                                          \
    0x11d3,                                          \
    {0x93, 0x37, 0x00, 0x10, 0x4b, 0xa0, 0xfd, 0x40} \
}

// component implementing nsILoadGroup.
#define NS_LOADGROUP_CLASSNAME \
    "nsLoadGroup"
#define NS_LOADGROUP_CONTRACTID \
    "@mozilla.org/network/load-group;1"
#define NS_LOADGROUP_CID                             \
{ /* e1c61582-2a84-11d3-8cce-0060b0fc14a3 */         \
    0xe1c61582,                                      \
    0x2a84,                                          \
    0x11d3,                                          \
    {0x8c, 0xce, 0x00, 0x60, 0xb0, 0xfc, 0x14, 0xa3} \
}

// component implementing nsIURI and nsISerializable.
#define NS_SIMPLEURI_CLASSNAME \
    "nsSimpleURI"
#define NS_SIMPLEURI_CONTRACTID \
    "@mozilla.org/network/simple-uri;1"
#define NS_SIMPLEURI_CID                              \
{ /* e0da1d70-2f7b-11d3-8cd0-0060b0fc14a3 */          \
     0xe0da1d70,                                      \
     0x2f7b,                                          \
     0x11d3,                                          \
     {0x8c, 0xd0, 0x00, 0x60, 0xb0, 0xfc, 0x14, 0xa3} \
}

// component implementing nsIStandardURL, nsIFileURL, and nsISerializable.
#define NS_STANDARDURL_CLASSNAME \
    "nsStandardURL"
#define NS_STANDARDURL_CONTRACTID \
    "@mozilla.org/network/standard-url;1"
#define NS_STANDARDURL_CID                           \
{ /* de9472d0-8034-11d3-9399-00104ba0fd40 */         \
    0xde9472d0,                                      \
    0x8034,                                          \
    0x11d3,                                          \
    {0x93, 0x99, 0x00, 0x10, 0x4b, 0xa0, 0xfd, 0x40} \
}

// service implementing nsIURLParser that assumes the URL will NOT contain an
// authority section.
#define NS_NOAUTHURLPARSER_CLASSNAME \
    "nsNoAuthURLParser"
#define NS_NOAUTHURLPARSER_CONTRACTID \
    "@mozilla.org/network/url-parser;1?auth=no"
#define NS_NOAUTHURLPARSER_CID                       \
{ /* 78804a84-8173-42b6-bb94-789f0816a810 */         \
    0x78804a84,                                      \
    0x8173,                                          \
    0x42b6,                                          \
    {0xbb, 0x94, 0x78, 0x9f, 0x08, 0x16, 0xa8, 0x10} \
}

// service implementing nsIURLParser that assumes the URL will contain an
// authority section.
#define NS_AUTHURLPARSER_CLASSNAME \
    "nsAuthURLParser"
#define NS_AUTHURLPARSER_CONTRACTID \
    "@mozilla.org/network/url-parser;1?auth=yes"
#define NS_AUTHURLPARSER_CID                         \
{ /* 275d800e-3f60-4896-adb7-d7f390ce0e42 */         \
    0x275d800e,                                      \
    0x3f60,                                          \
    0x4896,                                          \
    {0xad, 0xb7, 0xd7, 0xf3, 0x90, 0xce, 0x0e, 0x42} \
}

// service implementing nsIURLParser that does not make any assumptions about
// whether or not the URL contains an authority section.
#define NS_STDURLPARSER_CLASSNAME \
    "nsStdURLParser"
#define NS_STDURLPARSER_CONTRACTID \
    "@mozilla.org/network/url-parser;1?auth=maybe"
#define NS_STDURLPARSER_CID                          \
{ /* ff41913b-546a-4bff-9201-dc9b2c032eba */         \
    0xff41913b,                                      \
    0x546a,                                          \
    0x4bff,                                          \
    {0x92, 0x01, 0xdc, 0x9b, 0x2c, 0x03, 0x2e, 0xba} \
}

// component implementing nsIRequestObserverProxy.
#define NS_REQUESTOBSERVERPROXY_CLASSNAME \
    "nsRequestObserverProxy"
#define NS_REQUESTOBSERVERPROXY_CONTRACTID \
    "@mozilla.org/network/request-observer-proxy;1"
#define NS_REQUESTOBSERVERPROXY_CID                  \
{ /* 51fa28c7-74c0-4b85-9c46-d03faa7b696b */         \
    0x51fa28c7,                                      \
    0x74c0,                                          \
    0x4b85,                                          \
    {0x9c, 0x46, 0xd0, 0x3f, 0xaa, 0x7b, 0x69, 0x6b} \
}

// component implementing nsIStreamListenerProxy.
#define NS_STREAMLISTENERPROXY_CLASSNAME \
    "nsStreamListenerProxy"
#define NS_STREAMLISTENERPROXY_CONTRACTID \
    "@mozilla.org/network/stream-listener-proxy;1"
#define NS_STREAMLISTENERPROXY_CID                   \
{ /* 96c48f15-aa8a-4da7-a9d5-e842bd76f015 */         \
    0x96c48f15,                                      \
    0xaa8a,                                          \
    0x4da7,                                          \
    {0xa9, 0xd5, 0xe8, 0x42, 0xbd, 0x76, 0xf0, 0x15} \
}

// component implementing nsIStreamProviderProxy.
#define NS_STREAMPROVIDERPROXY_CLASSNAME \
    "nsStreamProviderProxy"
#define NS_STREAMPROVIDERPROXY_CONTRACTID \
    "@mozilla.org/network/stream-provider-proxy;1"
#define NS_STREAMPROVIDERPROXY_CID                   \
{ /* ae964fcf-9c27-40f7-9bbd-78894bfc1f31 */         \
    0xae964fcf,                                      \
    0x9c27,                                          \
    0x40f7,                                          \
    {0x9b, 0xbd, 0x78, 0x89, 0x4b, 0xfc, 0x1f, 0x31} \
}

// component implementing nsISimpleStreamListener.
#define NS_SIMPLESTREAMLISTENER_CLASSNAME \
    "nsSimpleStreamListener"
#define NS_SIMPLESTREAMLISTENER_CONTRACTID \
    "@mozilla.org/network/simple-stream-listener;1"
#define NS_SIMPLESTREAMLISTENER_CID                  \
{ /* fb8cbf4e-4701-4ba1-b1d6-5388e041fb67 */         \
    0xfb8cbf4e,                                      \
    0x4701,                                          \
    0x4ba1,                                          \
    {0xb1, 0xd6, 0x53, 0x88, 0xe0, 0x41, 0xfb, 0x67} \
}

// component implementing nsISimpleStreamProvider.
#define NS_SIMPLESTREAMPROVIDER_CLASSNAME \
    "nsSimpleStreamProvider"
#define NS_SIMPLESTREAMPROVIDER_CONTRACTID \
    "@mozilla.org/network/simple-stream-provider;1"
#define NS_SIMPLESTREAMPROVIDER_CID                  \
{ /* f9f6a519-4efb-4f36-af40-2a5ec3992710 */         \
    0xf9f6a519,                                      \
    0x4efb,                                          \
    0x4f36,                                          \
    {0xaf, 0x40, 0x2a, 0x5e, 0xc3, 0x99, 0x27, 0x10} \
}

// DEPRECATED component implementing nsIAsyncStreamListener.
#define NS_ASYNCSTREAMLISTENER_CLASSNAME \
    "nsAsyncStreamListener"
#define NS_ASYNCSTREAMLISTENER_CONTRACTID \
    "@mozilla.org/network/async-stream-listener;1"
#define NS_ASYNCSTREAMLISTENER_CID                   \
{ /* 60047bb2-91c0-11d3-8cd9-0060b0fc14a3 */         \
    0x60047bb2,                                      \
    0x91c0,                                          \
    0x11d3,                                          \
    {0x8c, 0xd9, 0x00, 0x60, 0xb0, 0xfc, 0x14, 0xa3} \
}

// component implementing nsIStreamListenerTee.
#define NS_STREAMLISTENERTEE_CLASSNAME \
    "nsStreamListenerTee"
#define NS_STREAMLISTENERTEE_CONTRACTID \
    "@mozilla.org/network/stream-listener-tee;1"
#define NS_STREAMLISTENERTEE_CID                     \
{ /* 831f8f13-7aa8-485f-b02e-77c881cc5773 */         \
    0x831f8f13,                                      \
    0x7aa8,                                          \
    0x485f,                                          \
    {0xb0, 0x2e, 0x77, 0xc8, 0x81, 0xcc, 0x57, 0x73} \
}

// A simple implementation of nsITransport that stores a segmented memory
// buffer (4k chunks).  As long as the nsITransport is referenced, the data
// remains in memory.  It can be read multiple times (only AsyncRead is
// implemented).  There can be only one writer at a time (only OpenOutputStream
// is implemented).  AsyncRead can be called while an output stream is still
// being written to.  The readers will get notified automatically as more
// data is written via the output stream.
#define NS_STORAGETRANSPORT_CLASSNAME \
    "nsStorageTransport"
#define NS_STORAGETRANSPORT_CONTRACTID \
    "@mozilla.org/network/storage-transport;1"
#define NS_STORAGETRANSPORT_CID                      \
{ /* 5e955cdb-1334-4b8f-86b5-3b0f4d54b9d2 */         \
    0x5e955cdb,                                      \
    0x1334,                                          \
    0x4b8f,                                          \
    {0x86, 0xb5, 0x3b, 0x0f, 0x4d, 0x54, 0xb9, 0xd2} \
}

// component implementing nsIStreamIOChannel.
#define NS_STREAMIOCHANNEL_CLASSNAME \
    "nsInputStreamChannel"
#define NS_STREAMIOCHANNEL_CONTRACTID \
    "@mozilla.org/network/stream-io-channel;1"
#define NS_STREAMIOCHANNEL_CID                       \
{ /* 6ddb050c-0d04-11d4-986e-00c04fa0cf4a */         \
    0x6ddb050c,                                      \
    0x0d04,                                          \
    0x11d4,                                          \
    {0x98, 0x6e, 0x00, 0xc0, 0x4f, 0xa0, 0xcf, 0x4a} \
}

// component implementing nsIStreamLoader.
#define NS_STREAMLOADER_CLASSNAME \
    "nsStreamLoader"
#define NS_STREAMLOADER_CONTRACTID \
    "@mozilla.org/network/stream-loader;1"
#define NS_STREAMLOADER_CID \
{ /* 5BA6D920-D4E9-11d3-A1A5-0050041CAF44 */         \
    0x5ba6d920,                                      \
    0xd4e9,                                          \
    0x11d3,                                          \
    { 0xa1, 0xa5, 0x0, 0x50, 0x4, 0x1c, 0xaf, 0x44 } \
}

// component implementing nsIDownloader.
#define NS_DOWNLOADER_CLASSNAME \
    "nsDownloader"
#define NS_DOWNLOADER_CONTRACTID \
    "@mozilla.org/network/downloader;1"
#define NS_DOWNLOADER_CID \
{ /* 510a86bb-6019-4ed1-bb4f-965cffd23ece */         \
    0x510a86bb,                                      \
    0x6019,                                          \
    0x4ed1,                                          \
    {0xbb, 0x4f, 0x96, 0x5c, 0xff, 0xd2, 0x3e, 0xce} \
}

// component implementing nsIURIChecker.
#define NS_URICHECKER_CLASSNAME \
    "nsURIChecker"
#define NS_URICHECKER_CONTRACT_ID \
    "@mozilla.org/network/urichecker;1"
#define NS_URICHECKER_CID \
{ /* cf3a0e06-1dd1-11b2-a904-ac1d6da77a02 */         \
    0xcf3a0e06,                                      \
    0x1dd1,                                          \
    0x11b2,                                          \
    {0xa9, 0x04, 0xac, 0x1d, 0x6d, 0xa7, 0x7a, 0x02} \
}

// component implementing nsIResumableEntityID
#define NS_RESUMABLEENTITYID_CLASSNAME \
    "nsResumableEntityID"
#define NS_RESUMABLEENTITYID_CONTRACTID \
    "@mozilla.org/network/resumable-entity-id;1"
#define NS_RESUMABLEENTITYID_CID \
{ /* e744a9a6-1dd1-11b2-b95c-e5d67a34e6b3 */         \
    0xe744a9a6,                                      \
    0x1d11,                                          \
    0x11b2,                                          \
    {0xb9, 0x5c, 0xe5, 0xd6, 0x7a, 0x34, 0xe6, 0xb3} \
}     


/******************************************************************************
 * netwerk/cache/ classes
 */

// service implementing nsICacheService.
#define NS_CACHESERVICE_CLASSNAME \
    "nsCacheService"
#define NS_CACHESERVICE_CONTRACTID \
    "@mozilla.org/network/cache-service;1"
#define NS_CACHESERVICE_CID                          \
{ /* 6c84aec9-29a5-4264-8fbc-bee8f922ea67 */         \
    0x6c84aec9,                                      \
    0x29a5,                                          \
    0x4264,                                          \
    {0x8f, 0xbc, 0xbe, 0xe8, 0xf9, 0x22, 0xea, 0x67} \
}


/******************************************************************************
 * netwerk/protocol/http/ classes
 */

// This protocol handler implements both HTTP and HTTPS
#define NS_HTTPPROTOCOLHANDLER_CID \
{ /* 4f47e42e-4d23-4dd3-bfda-eb29255e9ea3 */         \
    0x4f47e42e,                                      \
    0x4d23,                                          \
    0x4dd3,                                          \
    {0xbf, 0xda, 0xeb, 0x29, 0x25, 0x5e, 0x9e, 0xa3} \
}

#define NS_HTTPBASICAUTH_CID \
{ /* fca3766a-434a-4ae7-83cf-0909e18a093a */         \
    0xfca3766a,                                      \
    0x434a,                                          \
    0x4ae7,                                          \
    {0x83, 0xcf, 0x09, 0x09, 0xe1, 0x8a, 0x09, 0x3a} \
}

#define NS_HTTPDIGESTAUTH_CID \
{ /* 17491ba4-1dd2-11b2-aae3-de6b92dab620 */         \
    0x17491ba4,                                      \
    0x1dd2,                                          \
    0x11b2,                                          \
    {0xaa, 0xe3, 0xde, 0x6b, 0x92, 0xda, 0xb6, 0x20} \
}

/******************************************************************************
 * netwerk/protocol/res/ classes
 */

#define NS_RESPROTOCOLHANDLER_CID                    \
{ /* e64f152a-9f07-11d3-8cda-0060b0fc14a3 */         \
    0xe64f152a,                                      \
    0x9f07,                                          \
    0x11d3,                                          \
    {0x8c, 0xda, 0x00, 0x60, 0xb0, 0xfc, 0x14, 0xa3} \
}


/******************************************************************************
 * netwerk/dns/ classes
 */

/* ContractID of the XPCOM package that implements nsIIDNService */
#define NS_IDNSERVICE_CONTRACTID \
    "@i-dns.net/IDNService;1"


#endif // nsNetCID_h__
