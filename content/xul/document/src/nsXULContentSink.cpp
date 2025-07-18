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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Waterson <waterson@netscape.com>
 *   David Hyatt <hyatt@netscape.com>
 *   Brendan Eich <brendan@mozilla.org>
 *
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

/*

  An implementation for a Gecko-style content sink that knows how
  to build an RDF content model from XUL.

  For each container tag, an RDF Sequence object is created that
  contains the order set of children of that container.

  For more information on XUL, see http://www.mozilla.org/xpfe

  TO DO
  -----

*/

#include "nsCOMPtr.h"
#include "nsForwardReference.h"
#include "nsICSSLoader.h"
#include "nsICSSParser.h"
#include "nsICSSStyleSheet.h"
#include "nsIContentSink.h"
#include "nsIDOMDocument.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMXULDocument.h"
#include "nsIDocument.h"
#include "nsIDocumentLoader.h"
#include "nsIFormControl.h"
#include "nsIHTMLContent.h"
#include "nsIHTMLContentContainer.h"
#include "nsINameSpace.h"
#include "nsINameSpaceManager.h"
#include "nsINodeInfo.h"
#include "nsIParser.h"
#include "nsIPresShell.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIRDFService.h"
#include "nsIScriptContext.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptGlobalObjectOwner.h"
#include "nsIServiceManager.h"
#include "nsITextContent.h"
#include "nsIURL.h"
#include "nsIViewManager.h"
#include "nsIXMLContent.h"
#include "nsIXULContentSink.h"
#include "nsIXULDocument.h"
#include "nsIXULPrototypeDocument.h"
#include "nsIXULPrototypeCache.h"
#include "nsLayoutCID.h"
#include "nsNetUtil.h"
#include "nsRDFCID.h"
#include "nsParserUtils.h"
#include "nsVoidArray.h"
#include "nsWeakPtr.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsXULElement.h"
#include "prlog.h"
#include "prmem.h"
#include "jsapi.h"  // for JSVERSION_*, JS_VersionToString, etc.
#include "nsCRT.h"

#include "nsIFastLoadService.h"         // XXXbe temporary
#include "nsIObjectInputStream.h"       // XXXbe temporary
#include "nsXULDocument.h"              // XXXbe temporary

#include "nsIExpatSink.h"
#include "nsUnicharUtils.h"

static const char kNameSpaceSeparator = ':';
static const char kNameSpaceDef[] = "xmlns";

#ifdef PR_LOGGING
static PRLogModuleInfo* gLog;
#endif

// XXX This is sure to change. Copied from mozilla/layout/xul/content/src/nsXULAtoms.cpp
#define XUL_NAMESPACE_URI "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul"
static const char kXULNameSpaceURI[] = XUL_NAMESPACE_URI;


static NS_DEFINE_CID(kCSSParserCID,              NS_CSSPARSER_CID);
static NS_DEFINE_CID(kNameSpaceManagerCID,       NS_NAMESPACEMANAGER_CID);
static NS_DEFINE_CID(kXULPrototypeCacheCID,      NS_XULPROTOTYPECACHE_CID);

//----------------------------------------------------------------------

class XULContentSinkImpl : public nsIXULContentSink,
                           public nsIExpatSink
{
public:
    XULContentSinkImpl(nsresult& aRV);
    virtual ~XULContentSinkImpl();

    // nsISupports
    NS_DECL_ISUPPORTS
    NS_DECL_NSIEXPATSINK

    // nsIContentSink
    NS_IMETHOD WillBuildModel(void);
    NS_IMETHOD DidBuildModel(PRInt32 aQualityLevel);
    NS_IMETHOD WillInterrupt(void);
    NS_IMETHOD WillResume(void);
    NS_IMETHOD SetParser(nsIParser* aParser);  
    NS_IMETHOD FlushPendingNotifications() { return NS_OK; }
    NS_IMETHOD SetDocumentCharset(nsAString& aCharset);

    // nsIXULContentSink
    NS_IMETHOD Init(nsIDocument* aDocument, nsIXULPrototypeDocument* aPrototype);

protected:
    // pseudo-constants
    static nsrefcnt               gRefCnt;
    static nsINameSpaceManager*   gNameSpaceManager;
    static nsIXULPrototypeCache*  gXULCache;

    static nsIAtom* kClassAtom;
    static nsIAtom* kIdAtom;
    static nsIAtom* kScriptAtom;
    static nsIAtom* kStyleAtom;
    static nsIAtom* kTemplateAtom;

    static PRInt32 kNameSpaceID_XUL;

    PRUnichar* mText;
    PRInt32 mTextLength;
    PRInt32 mTextSize;
    PRBool mConstrainSize;

    // namespace management -- XXX combine with ContextStack?
    nsresult PushNameSpacesFrom(const PRUnichar** aAttributes);

    // RDF-specific parsing
    nsresult ParseTag(const PRUnichar* aText, nsINodeInfo*& aNodeInfo);
    nsresult AddAttributes(const PRUnichar** aAttributes, 
                           const PRUint32 aAttrLen, 
                           nsXULPrototypeElement* aElement);
   
    nsresult OpenRoot(const PRUnichar** aAttributes, 
                      const PRUint32 aAttrLen, 
                      nsINodeInfo *aNodeInfo);

    nsresult OpenTag(const PRUnichar** aAttributes, 
                     const PRUint32 aAttrLen, 
                     const PRUint32 aLineNumber, 
                     nsINodeInfo *aNodeInfo);
    
    nsresult OpenScript(const PRUnichar** aAttributes,
                        const PRUint32 aLineNumber);

    static PRBool IsDataInBuffer(PRUnichar* aBuffer, PRInt32 aLength);


    // Text management
    nsresult FlushText(PRBool aCreateTextNode = PR_TRUE);
    nsresult AddText(const PRUnichar* aText, PRInt32 aLength);


    
    void PopNameSpaces(void);
    nsresult GetTopNameSpace(nsCOMPtr<nsINameSpace>* aNameSpace);

    nsAutoVoidArray mNameSpaceStack;

    nsCOMPtr<nsINodeInfoManager> mNodeInfoManager;
    
    
    nsresult NormalizeAttributeString(const nsAFlatString& aText,
                                      nsINodeInfo*& aNodeInfo);
    nsresult CreateElement(nsINodeInfo *aNodeInfo, nsXULPrototypeElement** aResult);

    // Style sheets
    nsresult ProcessStyleLink(nsIContent* aElement,
                              const nsString& aHref,
                              PRBool aAlternate,
                              const nsString& aTitle,
                              const nsString& aType,
                              const nsString& aMedia);
    

    public:
    enum State { eInProlog, eInDocumentElement, eInScript, eInEpilog };
    protected:

    State mState;

    // content stack management
    class ContextStack {
    protected:
        struct Entry {
            nsXULPrototypeNode* mNode;
            // a LOT of nodes have children; preallocate for 8
            nsAutoVoidArray     mChildren;
            State               mState;
            Entry*              mNext;
        };

        Entry* mTop;
        PRInt32 mDepth;

    public:
        ContextStack();
        ~ContextStack();

        PRInt32 Depth() { return mDepth; }

        nsresult Push(nsXULPrototypeNode* aNode, State aState);
        nsresult Pop(State* aState);

        nsresult GetTopNode(nsXULPrototypeNode** aNode);
        nsresult GetTopChildren(nsVoidArray** aChildren);
    };

    friend class ContextStack;
    ContextStack mContextStack;

    nsWeakPtr              mDocument;             // [OWNER]
    nsCOMPtr<nsIURI>       mDocumentURL;          // [OWNER]

    nsCOMPtr<nsIXULPrototypeDocument> mPrototype; // [OWNER]
    nsIParser*             mParser;               // [OWNER] We use regular pointer b/c of funky exports on nsIParser
    
    nsString               mPreferredStyle;
    nsCOMPtr<nsICSSLoader> mCSSLoader;            // [OWNER]
    nsCOMPtr<nsICSSParser> mCSSParser;            // [OWNER]
};

nsrefcnt XULContentSinkImpl::gRefCnt;
nsINameSpaceManager* XULContentSinkImpl::gNameSpaceManager;
nsIXULPrototypeCache* XULContentSinkImpl::gXULCache;

nsIAtom* XULContentSinkImpl::kClassAtom;
nsIAtom* XULContentSinkImpl::kIdAtom;
nsIAtom* XULContentSinkImpl::kScriptAtom;
nsIAtom* XULContentSinkImpl::kStyleAtom;
nsIAtom* XULContentSinkImpl::kTemplateAtom;

PRInt32 XULContentSinkImpl::kNameSpaceID_XUL;

//----------------------------------------------------------------------

XULContentSinkImpl::ContextStack::ContextStack()
    : mTop(nsnull), mDepth(0)
{
}

XULContentSinkImpl::ContextStack::~ContextStack()
{
    while (mTop) {
        Entry* doomed = mTop;
        mTop = mTop->mNext;
        delete doomed;
    }
}

nsresult
XULContentSinkImpl::ContextStack::Push(nsXULPrototypeNode* aNode, State aState)
{
    Entry* entry = new Entry;
    if (! entry)
        return NS_ERROR_OUT_OF_MEMORY;

    entry->mNode  = aNode;
    entry->mState = aState;
    entry->mNext  = mTop;
    mTop = entry;

    ++mDepth;
    return NS_OK;
}

nsresult
XULContentSinkImpl::ContextStack::Pop(State* aState)
{
    if (mDepth == 0)
        return NS_ERROR_UNEXPECTED;

    Entry* entry = mTop;
    mTop = mTop->mNext;
    --mDepth;

    *aState = entry->mState;
    delete entry;

    return NS_OK;
}


nsresult
XULContentSinkImpl::ContextStack::GetTopNode(nsXULPrototypeNode** aNode)
{
    if (mDepth == 0)
        return NS_ERROR_UNEXPECTED;

    *aNode = mTop->mNode;
    return NS_OK;
}


nsresult
XULContentSinkImpl::ContextStack::GetTopChildren(nsVoidArray** aChildren)
{
    if (mDepth == 0)
        return NS_ERROR_UNEXPECTED;

    *aChildren = &(mTop->mChildren);
    return NS_OK;
}


//----------------------------------------------------------------------


XULContentSinkImpl::XULContentSinkImpl(nsresult& rv)
    : mText(nsnull),
      mTextLength(0),
      mTextSize(0),
      mConstrainSize(PR_TRUE),
      mState(eInProlog),
      mParser(nsnull)
{
    NS_INIT_REFCNT();

    if (gRefCnt++ == 0) {
        rv = nsComponentManager::CreateInstance(kNameSpaceManagerCID,
                                                nsnull,
                                                NS_GET_IID(nsINameSpaceManager),
                                                (void**) &gNameSpaceManager);

        if (NS_FAILED(rv)) return;


        rv = gNameSpaceManager->RegisterNameSpace(NS_ConvertASCIItoUCS2(kXULNameSpaceURI), kNameSpaceID_XUL);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to register XUL namespace");
        if (NS_FAILED(rv)) return;

        kClassAtom          = NS_NewAtom("class");
        kIdAtom             = NS_NewAtom("id");
        kScriptAtom         = NS_NewAtom("script");
        kStyleAtom          = NS_NewAtom("style");
        kTemplateAtom       = NS_NewAtom("template");

        rv = nsServiceManager::GetService(kXULPrototypeCacheCID,
                                          NS_GET_IID(nsIXULPrototypeCache),
                                          (nsISupports**) &gXULCache);
    }

#ifdef PR_LOGGING
    if (! gLog)
        gLog = PR_NewLogModule("nsXULContentSink");
#endif

    rv = NS_OK;
}


XULContentSinkImpl::~XULContentSinkImpl()
{
    NS_IF_RELEASE(mParser); // XXX should've been released by now, unless error.

    {
        // There shouldn't be any here except in an error condition
        PRInt32 i = mNameSpaceStack.Count();

        while (0 < i--) {
            nsINameSpace* nameSpace = (nsINameSpace*)mNameSpaceStack[i];

#ifdef PR_LOGGING
            if (PR_LOG_TEST(gLog, PR_LOG_ALWAYS)) {
                nsAutoString uri;
                nsCOMPtr<nsIAtom> prefixAtom;

                nameSpace->GetNameSpaceURI(uri);
                nameSpace->GetNameSpacePrefix(*getter_AddRefs(prefixAtom));

                nsAutoString prefix;
                if (prefixAtom)
                    {
                        const PRUnichar *unicodeString;
                        prefixAtom->GetUnicode(&unicodeString);
                        prefix = unicodeString;
                    }
                else
                    {
                        prefix.Assign(NS_LITERAL_STRING("<default>"));
                    }

                char* prefixStr = ToNewCString(prefix);
                char* uriStr = ToNewCString(uri);

                PR_LOG(gLog, PR_LOG_ALWAYS,
                       ("xul: warning: unclosed namespace '%s' (%s)",
                        prefixStr, uriStr));

                nsCRT::free(prefixStr);
                nsCRT::free(uriStr);
            }
#endif

            NS_RELEASE(nameSpace);
        }
    }

    // Pop all of the elements off of the context stack, and delete
    // any remaining content elements. The context stack _should_ be
    // empty, unless something has gone wrong.
    while (mContextStack.Depth()) {
        nsresult rv;

        nsVoidArray* children;
        rv = mContextStack.GetTopChildren(&children);
        if (NS_SUCCEEDED(rv)) {
            for (PRInt32 i = children->Count() - 1; i >= 0; --i) {
                nsXULPrototypeNode* child =
                    NS_REINTERPRET_CAST(nsXULPrototypeNode*, children->ElementAt(i));

                delete child;
            }
        }

        nsXULPrototypeNode* node;
        rv = mContextStack.GetTopNode(&node);
        if (NS_SUCCEEDED(rv)) delete node;

        State state;
        mContextStack.Pop(&state);
    }

    PR_FREEIF(mText);

    if (--gRefCnt == 0) {
        NS_IF_RELEASE(gNameSpaceManager);

        NS_IF_RELEASE(kClassAtom);
        NS_IF_RELEASE(kIdAtom);
        NS_IF_RELEASE(kScriptAtom);
        NS_IF_RELEASE(kStyleAtom);
        NS_IF_RELEASE(kTemplateAtom);

        if (gXULCache) {
            nsServiceManager::ReleaseService(kXULPrototypeCacheCID, gXULCache);
            gXULCache = nsnull;
        }
    }
}

//----------------------------------------------------------------------
// nsISupports interface

NS_IMPL_ISUPPORTS4(XULContentSinkImpl,
                   nsIXULContentSink,
                   nsIXMLContentSink,
                   nsIContentSink,
                   nsIExpatSink)

//----------------------------------------------------------------------
// nsIContentSink interface

NS_IMETHODIMP 
XULContentSinkImpl::WillBuildModel(void)
{
#if FIXME
    if (! mParentContentSink) {
        // If we're _not_ an overlay, then notify the document that
        // the load is beginning.
        mDocument->BeginLoad();
    }
#endif

    return NS_OK;
}

NS_IMETHODIMP 
XULContentSinkImpl::DidBuildModel(PRInt32 aQualityLevel)
{
#if FIXME
    // XXX this is silly; who cares?
    PRInt32 i, ns = mDocument->GetNumberOfShells();
    for (i = 0; i < ns; i++) {
        nsCOMPtr<nsIPresShell> shell;
        mDocument->GetShellAt(i, getter_AddRefs(shell));
        if (nsnull != shell) {
            nsIViewManager* vm;
            shell->GetViewManager(&vm);
            if(vm) {
                vm->SetQuality(nsContentQuality(aQualityLevel));
            }
            NS_RELEASE(vm);
        }
    }
#endif

    nsCOMPtr<nsIDocument> doc = do_QueryReferent(mDocument);
    if (doc) {
        doc->EndLoad();
        mDocument = nsnull;
    }

    // Drop our reference to the parser to get rid of a circular
    // reference.
    NS_IF_RELEASE(mParser);
    return NS_OK;
}

NS_IMETHODIMP 
XULContentSinkImpl::WillInterrupt(void)
{
    // XXX Notify the webshell, if necessary
    return NS_OK;
}

NS_IMETHODIMP 
XULContentSinkImpl::WillResume(void)
{
    nsCOMPtr<nsIXULDocument> xuldoc(do_QueryReferent(mDocument));
    if (xuldoc)
        xuldoc->OnResumeContentSink();

    // XXX Notify the webshell, if necessary
    return NS_OK;
}

NS_IMETHODIMP 
XULContentSinkImpl::SetParser(nsIParser* aParser)
{
    NS_IF_RELEASE(mParser);
    mParser = aParser;
    NS_IF_ADDREF(mParser);
    return NS_OK;
}

nsresult
XULContentSinkImpl::ProcessStyleLink(nsIContent* aElement,
                                     const nsString& aHref,
                                     PRBool aAlternate,
                                     const nsString& aTitle,
                                     const nsString& aType,
                                     const nsString& aMedia)
{
    static const char kCSSType[] = "text/css";

    nsresult rv = NS_OK;

    if (aAlternate) { // if alternate, does it have title?
        if (aTitle.IsEmpty()) { // alternates must have title
            return NS_OK; //return without error, for now
        }
    }

    nsAutoString mimeType;
    nsAutoString params;
    nsParserUtils::SplitMimeType(aType, mimeType, params);

    if ((mimeType.IsEmpty()) || mimeType.EqualsIgnoreCase(kCSSType)) {
        nsCOMPtr<nsIURI> url;
        rv = NS_NewURI(getter_AddRefs(url), aHref, nsnull, mDocumentURL);
        if (NS_OK != rv) {
            return NS_OK; // The URL is bad, move along, don't propagate the error (for now)
        }

        // Add the style sheet reference to the prototype
        mPrototype->AddStyleSheetReference(url);

        // Nope, we need to load it asynchronously
        PRBool blockParser = PR_FALSE;
        if (! aAlternate) {
            if (!aTitle.IsEmpty()) {  // possibly preferred sheet
                if (mPreferredStyle.IsEmpty()) {
                    mPreferredStyle = aTitle;
                    mCSSLoader->SetPreferredSheet(aTitle);
                    nsIAtom* defaultStyle = NS_NewAtom("default-style");
                    if (defaultStyle) {
                        mPrototype->SetHeaderData(defaultStyle, aTitle);
                        NS_RELEASE(defaultStyle);
                    }
                }
            }
            else {  // persistent sheet, block
                blockParser = PR_TRUE;
            }
        }

        nsCOMPtr<nsIDocument> doc = do_QueryReferent(mDocument);
        if (! doc)
            return NS_ERROR_FAILURE; // doc went away!

        PRBool doneLoading;
        PRInt32 numSheets = 0;
        doc->GetNumberOfStyleSheets(&numSheets);
        rv = mCSSLoader->LoadStyleLink(aElement, url, aTitle, aMedia, kNameSpaceID_Unknown,
                                       numSheets,
                                       ((blockParser) ? mParser : nsnull),
                                       doneLoading, nsnull);
        if (NS_SUCCEEDED(rv) && blockParser && (! doneLoading)) {
          rv = NS_ERROR_HTMLPARSER_BLOCK;
        }
    }

    return rv;
}

NS_IMETHODIMP 
XULContentSinkImpl::SetDocumentCharset(nsAString& aCharset)
{
    nsCOMPtr<nsIDocument> doc = do_QueryReferent(mDocument);
    if (doc) {
        return doc->SetDocumentCharacterSet(aCharset);
    }
  
    return NS_OK;
}

//----------------------------------------------------------------------
//
// nsIXULContentSink interface
//

NS_IMETHODIMP
XULContentSinkImpl::Init(nsIDocument* aDocument, nsIXULPrototypeDocument* aPrototype)
{
    NS_PRECONDITION(aDocument != nsnull, "null ptr");
    if (! aDocument)
        return NS_ERROR_NULL_POINTER;
    
    nsresult rv;

    mDocument    = getter_AddRefs(NS_GetWeakReference(aDocument));
    mPrototype   = aPrototype;

    rv = mPrototype->GetURI(getter_AddRefs(mDocumentURL));
    if (NS_FAILED(rv)) return rv;

    // XXX this presumes HTTP header info is already set in document
    // XXX if it isn't we need to set it here...
    nsCOMPtr<nsIAtom> defaultStyle = dont_AddRef( NS_NewAtom("default-style") );
    if (! defaultStyle)
        return NS_ERROR_OUT_OF_MEMORY;

    rv = mPrototype->GetHeaderData(defaultStyle, mPreferredStyle);
    if (NS_FAILED(rv)) return rv;

    // Get the CSS loader from the document so we can load
    // stylesheets.
    nsCOMPtr<nsIHTMLContentContainer> htmlContainer = do_QueryInterface(aDocument);
    NS_ASSERTION(htmlContainer != nsnull, "not an HTML container");
    if (! htmlContainer)
        return NS_ERROR_UNEXPECTED;

    rv = htmlContainer->GetCSSLoader(*getter_AddRefs(mCSSLoader));
    if (NS_FAILED(rv)) return rv;

    rv = aDocument->GetNodeInfoManager(*getter_AddRefs(mNodeInfoManager));
    if (NS_FAILED(rv)) return rv;

    mState = eInProlog;
    return NS_OK;
}


//----------------------------------------------------------------------
//
// Text buffering
//

PRBool
XULContentSinkImpl::IsDataInBuffer(PRUnichar* buffer, PRInt32 length)
{
    for (PRInt32 i = 0; i < length; ++i) {
        if (buffer[i] == ' ' ||
            buffer[i] == '\t' ||
            buffer[i] == '\n' ||
            buffer[i] == '\r')
            continue;

        return PR_TRUE;
    }
    return PR_FALSE;
}


nsresult
XULContentSinkImpl::FlushText(PRBool aCreateTextNode)
{
    nsresult rv;

    do {
        // Don't do anything if there's no text to create a node from, or
        // if they've told us not to create a text node
        if (! mTextLength)
            break;

        if (! aCreateTextNode)
            break;

        // Don't bother if there's nothing but whitespace.
        // XXX This could cause problems...
        if (! IsDataInBuffer(mText, mTextLength))
            break;

        // Don't bother if we're not in XUL document body
        if (mState != eInDocumentElement || mContextStack.Depth() == 0)
            break;

        // Trim the leading and trailing whitespace
        nsXULPrototypeText* text = new nsXULPrototypeText(/* line number */ -1);
        if (! text)
            return NS_ERROR_OUT_OF_MEMORY;

        text->mValue.SetCapacity(mTextLength + 1);
        text->mValue.Append(mText, mTextLength);
        text->mValue.Trim(" \t\n\r");

        // hook it up
        nsVoidArray* children;
        rv = mContextStack.GetTopChildren(&children);
        if (NS_FAILED(rv)) return rv;

        // transfer ownership of 'text' to the children array
        children->AppendElement(text);
    } while (0);

    // Reset our text buffer
    mTextLength = 0;
    return NS_OK;
}

//----------------------------------------------------------------------

nsresult
XULContentSinkImpl::NormalizeAttributeString(const nsAFlatString& aText,
                                             nsINodeInfo*& aNodeInfo)
{
    PRInt32 nameSpaceID = kNameSpaceID_None;

    nsAFlatString::const_iterator start, end;
    aText.BeginReading(start);
    aText.EndReading(end);

    nsAFlatString::const_iterator colon(start);

    nsCOMPtr<nsIAtom> prefix;

    if (!FindCharInReadable(kNameSpaceSeparator, colon, end)) {
        colon = start; // No ':' found, reset colon
    } else if (start != colon) {
        prefix = dont_AddRef(NS_NewAtom(Substring(start, colon)));

        nsCOMPtr<nsINameSpace> ns;
        GetTopNameSpace(address_of(ns));

        if (ns) {
            ns->FindNameSpaceID(prefix, nameSpaceID);

            if (nameSpaceID == kNameSpaceID_Unknown) {
                NS_WARNING("Undeclared prefix used in attribute name.");

                nameSpaceID = kNameSpaceID_None;
            }
        } else {
            NS_WARNING("Undeclared prefix used in attribute name.");
        }

        ++colon; // Skip over the ':'
    }

    return mNodeInfoManager->GetNodeInfo(Substring(colon, end), prefix,
                                         nameSpaceID, aNodeInfo);
}

nsresult
XULContentSinkImpl::CreateElement(nsINodeInfo *aNodeInfo,
                                  nsXULPrototypeElement** aResult)
{
    nsXULPrototypeElement* element = new nsXULPrototypeElement(/* line number */ -1);
    if (! element)
        return NS_ERROR_OUT_OF_MEMORY;

    element->mNodeInfo    = aNodeInfo;
    
    *aResult = element;
    return NS_OK;
}

nsresult
NS_NewXULContentSink(nsIXULContentSink** aResult)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;
    XULContentSinkImpl* sink = new XULContentSinkImpl(rv);
    if (! sink)
        return NS_ERROR_OUT_OF_MEMORY;

    if (NS_FAILED(rv)) {
        delete sink;
        return rv;
    }

    NS_ADDREF(sink);
    *aResult = sink;
    return NS_OK;
}

/**** BEGIN NEW APIs ****/


NS_IMETHODIMP 
XULContentSinkImpl::HandleStartElement(const PRUnichar *aName, 
                                       const PRUnichar **aAtts,
                                       PRUint32 aAttsCount, 
                                       PRUint32 aIndex, 
                                       PRUint32 aLineNumber)
{ 
  // XXX Hopefully the parser will flag this before we get here. If
  // we're in the epilog, there should be no new elements
  NS_PRECONDITION(mState != eInEpilog, "tag in XUL doc epilog");
  if (mState == eInEpilog)
      return NS_ERROR_UNEXPECTED;

  if (mState != eInScript) {
      FlushText();
  }

  // We must register namespace declarations found in the attribute
  // list of an element before creating the element. This is because
  // the namespace prefix for an element might be declared within
  // the attribute list.
  nsresult rv = PushNameSpacesFrom(aAtts);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsINodeInfo> nodeInfo;
  rv = ParseTag(aName, *getter_AddRefs(nodeInfo));
  if (NS_FAILED(rv)) {
     return rv;
  }

  switch (mState) {
  case eInProlog:
      // We're the root document element
      rv = OpenRoot(aAtts, aAttsCount, nodeInfo);
      break;

  case eInDocumentElement:
      rv = OpenTag(aAtts, aAttsCount, aLineNumber, nodeInfo);
      break;

  case eInEpilog:
  case eInScript:
      PR_LOG(gLog, PR_LOG_ALWAYS,
             ("xul: warning: unexpected tags in epilog at line %d",
             aLineNumber));
      rv = NS_ERROR_UNEXPECTED; // XXX
      break;
  }

  return rv;
}

NS_IMETHODIMP 
XULContentSinkImpl::HandleEndElement(const PRUnichar *aName)
{
    // Never EVER return anything but NS_OK or
    // NS_ERROR_HTMLPARSER_BLOCK from this method. Doing so will blow
    // the parser's little mind all over the planet.
    nsresult rv;

    nsXULPrototypeNode* node;
    rv = mContextStack.GetTopNode(&node);

    if (NS_FAILED(rv)) {
      return NS_OK;
    }

    switch (node->mType) {
    case nsXULPrototypeNode::eType_Element: {
        // Flush any text _now_, so that we'll get text nodes created
        // before popping the stack.
        FlushText();

        // Pop the context stack and do prototype hookup.
        nsVoidArray* children;
        rv = mContextStack.GetTopChildren(&children);
        if (NS_FAILED(rv)) return rv;

        nsXULPrototypeElement* element =
            NS_REINTERPRET_CAST(nsXULPrototypeElement*, node);

        PRInt32 count = children->Count();
        if (count) {
            element->mChildren = new nsXULPrototypeNode*[count];
            if (! element->mChildren)
                return NS_ERROR_OUT_OF_MEMORY;

            for (PRInt32 i = count - 1; i >= 0; --i)
                element->mChildren[i] =
                    NS_REINTERPRET_CAST(nsXULPrototypeNode*, children->ElementAt(i));

            element->mNumChildren = count;
        }
    }
    break;

    case nsXULPrototypeNode::eType_Script: {
        nsXULPrototypeScript* script =
            NS_STATIC_CAST(nsXULPrototypeScript*, node);

        // If given a src= attribute, we must ignore script tag content.
        if (! script->mSrcURI && ! script->mJSObject) {
            nsCOMPtr<nsIDocument> doc = do_QueryReferent(mDocument);

            rv = script->Compile(mText, mTextLength, mDocumentURL,
                                 script->mLineNo, doc, mPrototype);
        }

        FlushText(PR_FALSE);
    }
    break;

    default:
        NS_ERROR("didn't expect that");
        break;
    }

    rv = mContextStack.Pop(&mState);
    NS_ASSERTION(NS_SUCCEEDED(rv), "context stack corrupted");
    if (NS_FAILED(rv)) return rv;

    PopNameSpaces();

    if (mContextStack.Depth() == 0) {
        // The root element should -always- be an element, because
        // it'll have been created via XULContentSinkImpl::OpenRoot().
        NS_ASSERTION(node->mType == nsXULPrototypeNode::eType_Element, "root is not an element");
        if (node->mType != nsXULPrototypeNode::eType_Element)
            return NS_ERROR_UNEXPECTED;

        // Now that we're done parsing, set the prototype document's
        // root element. This transfers ownership of the prototype
        // element tree to the prototype document.
        nsXULPrototypeElement* element =
            NS_STATIC_CAST(nsXULPrototypeElement*, node);

        rv = mPrototype->SetRootElement(element);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to set document root");
        if (NS_FAILED(rv)) return rv;

        mState = eInEpilog;
    }

    return NS_OK;
}

NS_IMETHODIMP 
XULContentSinkImpl::HandleComment(const PRUnichar *aName)
{
   FlushText();
   return NS_OK;
}

NS_IMETHODIMP 
XULContentSinkImpl::HandleCDataSection(const PRUnichar *aData, PRUint32 aLength)
{
    FlushText();
    return AddText(aData, aLength);
}

NS_IMETHODIMP 
XULContentSinkImpl::HandleDoctypeDecl(const nsAString & aSubset, 
                                      const nsAString & aName, 
                                      const nsAString & aSystemId, 
                                      const nsAString & aPublicId,
                                      nsISupports* aCatalogData)
{
    return NS_OK;
}

NS_IMETHODIMP 
XULContentSinkImpl::HandleCharacterData(const PRUnichar *aData, 
                                        PRUint32 aLength)
{
  nsresult result = NS_OK;
  if (aData) {
    result = result = AddText(aData,aLength);
  }
  return result;
}

NS_IMETHODIMP 
XULContentSinkImpl::HandleProcessingInstruction(const PRUnichar *aTarget, 
                                                const PRUnichar *aData)
{
    FlushText();

    // XXX For now, we don't add the PI to the content model.
    // We just check for a style sheet PI
    const nsDependentString target(aTarget);
    nsAutoString data(aData); // XXX replace with nsDependentString - Change nsParserUtils APIs first

    nsReadingIterator<PRUnichar> targetStart, targetEnd, tmp;

    target.BeginReading(targetStart);
    target.EndReading(targetEnd);

    tmp = targetStart;

    if (FindInReadable(NS_LITERAL_STRING("xul-overlay"), targetStart, targetEnd)) {
      // Load a XUL overlay.
      nsAutoString href;
      nsParserUtils::GetQuotedAttributeValue(data, NS_LITERAL_STRING("href"), href);

      // If there was no href, we can't do
      // anything with this PI
      if (href.IsEmpty()) {
        return NS_OK;
      }

      // Add the overlay to our list of overlays that need to be processed.
      nsCOMPtr<nsIURI> url;
      nsresult rv = NS_NewURI(getter_AddRefs(url), href, nsnull, mDocumentURL);
      if (NS_FAILED(rv)) {
        // XXX This is wrong, the error message could be out of memory
        //     or something else equally bad, which we should propagate. 
        //     Bad URL should probably be "success with info" value.
        return NS_OK; // The URL is bad, move along. Don't propagate for now.
      }

      rv = mPrototype->AddOverlayReference(url);
      if (NS_FAILED(rv)) return rv;
    }
    // If it's a stylesheet PI...
    else {
      targetStart = tmp;
      if (FindInReadable(NS_LITERAL_STRING("xml-stylesheet"), targetStart, targetEnd)) {
        nsAutoString href;
        nsParserUtils::GetQuotedAttributeValue(data, NS_LITERAL_STRING("href"), href);

        // If there was no href, we can't do
        // anything with this PI
        if (href.IsEmpty())
            return NS_OK;

        nsAutoString type;
        nsParserUtils::GetQuotedAttributeValue(data, NS_LITERAL_STRING("type"), type);

        nsAutoString title;
        nsParserUtils::GetQuotedAttributeValue(data, NS_LITERAL_STRING("title"), title);

        title.CompressWhitespace();

        nsAutoString media;
        nsParserUtils::GetQuotedAttributeValue(data, NS_LITERAL_STRING("media"), media);

        ToLowerCase(media);

        nsAutoString alternate;
        nsParserUtils::GetQuotedAttributeValue(data, NS_LITERAL_STRING("alternate"), alternate);

        nsresult result =  ProcessStyleLink(nsnull /* XXX need a node here */,
                                            href, alternate.Equals(NS_LITERAL_STRING("yes")),  /* XXX ignore case? */
                                            title, type, media);
        if (NS_FAILED(result)) {
          if (result == NS_ERROR_HTMLPARSER_BLOCK && mParser) {
            mParser->BlockParser();
          }
          return result; // Important! A failure can indicate that the parser should block!
        }
      }
    }

    return NS_OK;
}

NS_IMETHODIMP
XULContentSinkImpl::ReportError(const PRUnichar* aErrorText, 
                                const PRUnichar* aSourceText)
{
  nsresult rv = NS_OK;

  // make sure to empty the context stack so that
  // <parsererror> could become the root element.
  while (mContextStack.Depth()) {
    nsVoidArray* children;
    rv = mContextStack.GetTopChildren(&children);
    if (NS_SUCCEEDED(rv)) {
      for (PRInt32 i = children->Count() - 1; i >= 0; --i) {
        nsXULPrototypeNode* child =
            NS_REINTERPRET_CAST(nsXULPrototypeNode*, children->ElementAt(i));

        delete child;
      }
    }

    State state;
    mContextStack.Pop(&state);
  }

  mState = eInProlog;

  NS_NAMED_LITERAL_STRING(name, "xmlns");
  NS_NAMED_LITERAL_STRING(value, "http://www.mozilla.org/newlayout/xml/parsererror.xml");

  const PRUnichar* atts[] = {name.get(), value.get(), nsnull};;
    
  rv = HandleStartElement(NS_LITERAL_STRING("parsererror").get(), atts, 1, -1, -1);
  NS_ENSURE_SUCCESS(rv,rv);

  rv = HandleCharacterData(aErrorText, nsCRT::strlen(aErrorText));
  NS_ENSURE_SUCCESS(rv,rv);  
  
  const PRUnichar* noAtts[] = {0, 0};
  rv = HandleStartElement(NS_LITERAL_STRING("sourcetext").get(), noAtts, 0, -1, -1);
  NS_ENSURE_SUCCESS(rv,rv);
  
  rv = HandleCharacterData(aSourceText, nsCRT::strlen(aSourceText));
  NS_ENSURE_SUCCESS(rv,rv);
  
  rv = HandleEndElement(NS_LITERAL_STRING("sourcetext").get());
  NS_ENSURE_SUCCESS(rv,rv); 
  
  rv = HandleEndElement(NS_LITERAL_STRING("parsererror").get());
  NS_ENSURE_SUCCESS(rv,rv);

  return rv;
}

//----------------------------------------------------------------------
//
// Namespace management
//

nsresult
XULContentSinkImpl::PushNameSpacesFrom(const PRUnichar** aAttributes)
{
    nsCOMPtr<nsINameSpace> nameSpace;

    if (mNameSpaceStack.Count() > 0) {
        nameSpace =
            (nsINameSpace*)mNameSpaceStack.ElementAt(mNameSpaceStack.Count() - 1);
    } else {
        gNameSpaceManager->CreateRootNameSpace(*getter_AddRefs(nameSpace));
        if (! nameSpace)
            return NS_ERROR_OUT_OF_MEMORY;
    }

    static const NS_NAMED_LITERAL_STRING(kNameSpaceDef, "xmlns");
    static const PRUint32 xmlns_len = 5; // kNameSpaceDef.Length();
    NS_ASSERTION(kNameSpaceDef.Length() == xmlns_len,
                 "xmlns_len incorrectly set!");

    for (; *aAttributes; aAttributes += 2) {
        nsDependentString key(aAttributes[0]);

        // Look for "xmlns" at the start of the attribute name

        PRUint32 key_len = key.Length();

        if (key_len >= xmlns_len &&
            Substring(key, 0, xmlns_len).Equals(kNameSpaceDef)) {
            nsCOMPtr<nsIAtom> prefixAtom;

            // If key_len > xmlns_len we have a xmlns:foo type attribute,
            // extract the prefix. If not, we have a xmlns attribute in
            // which case there is no prefix.

            if (key_len > xmlns_len) {
                nsDependentString::const_iterator start, end;

                key.BeginReading(start);
                key.EndReading(end);

                start.advance(xmlns_len);

                if (*start == ':' && ++start != end) {
                    prefixAtom =
                        dont_AddRef(NS_NewAtom(Substring(start, end)));
                } else {
                    NS_WARNING("Bad XML namespace declaration 'xmlns:' "
                               "found!");
                }
            }

            nsDependentString value(aAttributes[1]);

            nsCOMPtr<nsINameSpace> child;
            nsresult rv =
                nameSpace->CreateChildNameSpace(prefixAtom, value,
                                                *getter_AddRefs(child));
            NS_ENSURE_SUCCESS(rv, rv);

            nameSpace = child;
        }
    }

    nsINameSpace *tmp = nameSpace;
    mNameSpaceStack.AppendElement(tmp);
    NS_ADDREF(tmp);

    return NS_OK;
}

void
XULContentSinkImpl::PopNameSpaces(void)
{
    if (0 < mNameSpaceStack.Count()) {
        PRInt32 i = mNameSpaceStack.Count() - 1;
        nsINameSpace* nameSpace = (nsINameSpace*)mNameSpaceStack[i];
        mNameSpaceStack.RemoveElementAt(i);

        // Releasing the most deeply nested namespace will recursively
        // release intermediate parent namespaces until the next
        // reference is held on the namespace stack.
        NS_RELEASE(nameSpace);
    }
}

nsresult
XULContentSinkImpl::GetTopNameSpace(nsCOMPtr<nsINameSpace>* aNameSpace)
{
    PRInt32 count = mNameSpaceStack.Count();
    if (count == 0)
        return NS_ERROR_UNEXPECTED;

    *aNameSpace = dont_QueryInterface(NS_REINTERPRET_CAST(nsINameSpace*, mNameSpaceStack[count - 1]));
    return NS_OK;
}

//----------------------------------------------------------------------

nsresult
XULContentSinkImpl::ParseTag(const PRUnichar* aText, 
                             nsINodeInfo*& aNodeInfo)
{
    // Split the tag into prefix and tag substrings.

    nsDependentString text(aText);

    nsDependentString::const_iterator start, end;
    text.BeginReading(start);
    text.EndReading(end);

    nsDependentString::const_iterator colon(start);

    nsCOMPtr<nsIAtom> prefix;

    if (!FindCharInReadable(kNameSpaceSeparator, colon, end)) {
        colon = start; // No ':' found, reset colon
    } else if (colon != start) {
        prefix = dont_AddRef(NS_NewAtom(Substring(start, colon)));

        ++colon; // Step over ':'
    }

    nsCOMPtr<nsINameSpace> ns;
    GetTopNameSpace(address_of(ns));

    PRInt32 namespaceID = kNameSpaceID_None;

    if (ns) {
        ns->FindNameSpaceID(prefix, namespaceID);

        if (namespaceID == kNameSpaceID_Unknown) {
            NS_WARNING("Undeclared prefix used in tag name!");

            namespaceID = kNameSpaceID_None;
        }
    } else {
        NS_WARNING("Undeclared prefix used in tag name!");
    }

    return mNodeInfoManager->GetNodeInfo(Substring(colon, end), prefix,
                                         namespaceID, aNodeInfo);
}

nsresult
XULContentSinkImpl::OpenRoot(const PRUnichar** aAttributes, 
                             const PRUint32 aAttrLen, 
                             nsINodeInfo *aNodeInfo)
{
    NS_ASSERTION(mState == eInProlog, "how'd we get here?");
    if (mState != eInProlog)
        return NS_ERROR_UNEXPECTED;

    nsresult rv;

    if (aNodeInfo->Equals(kScriptAtom, kNameSpaceID_HTML) || 
        aNodeInfo->Equals(kScriptAtom, kNameSpaceID_XUL)) {
        PR_LOG(gLog, PR_LOG_ALWAYS,
               ("xul: script tag not allowed as root content element"));

        return NS_ERROR_UNEXPECTED;
    }

    // Create the element
    nsXULPrototypeElement* element;
    rv = CreateElement(aNodeInfo, &element);

    if (NS_FAILED(rv)) {
#ifdef PR_LOGGING
        nsAutoString anodeC;
        aNodeInfo->GetName(anodeC);
        PR_LOG(gLog, PR_LOG_ALWAYS,
               ("xul: unable to create element '%s' at line %d",
                NS_ConvertUCS2toUTF8(anodeC).get(),
                -1)); // XXX pass in line number
#endif

        return rv;
    }

    // Push the element onto the context stack, so that child
    // containers will hook up to us as their parent.
    rv = mContextStack.Push(element, mState);
    if (NS_FAILED(rv)) {
        delete element;
        return rv;
    }

    // Add the attributes
    rv = AddAttributes(aAttributes, aAttrLen, element);
    if (NS_FAILED(rv)) return rv;

    mState = eInDocumentElement;
    return NS_OK;
}

nsresult
XULContentSinkImpl::OpenTag(const PRUnichar** aAttributes, 
                            const PRUint32 aAttrLen,
                            const PRUint32 aLineNumber,
                            nsINodeInfo *aNodeInfo)
{
    nsresult rv;

    // Create the element
    nsXULPrototypeElement* element;
    rv = CreateElement(aNodeInfo, &element);

    if (NS_FAILED(rv)) {
#ifdef PR_LOGGING
        nsAutoString anodeC;
        aNodeInfo->GetName(anodeC);
        PR_LOG(gLog, PR_LOG_ALWAYS,
               ("xul: unable to create element '%s' at line %d",
                NS_ConvertUCS2toUTF8(anodeC).get(),
                aLineNumber));
#endif

        return rv;
    }

    // Link this element to its parent.
    nsVoidArray* children;
    rv = mContextStack.GetTopChildren(&children);
    if (NS_FAILED(rv)) {
        delete element;
        return rv;
    }

    // Add the attributes
    rv = AddAttributes(aAttributes, aAttrLen, element);
    if (NS_FAILED(rv)) return rv;

    children->AppendElement(element);

    if (aNodeInfo->Equals(kScriptAtom, kNameSpaceID_HTML) || 
        aNodeInfo->Equals(kScriptAtom, kNameSpaceID_XUL)) {
        // Do scripty things now.  OpenScript will push the
        // nsPrototypeScriptElement onto the stack, so we're done after this.
        return OpenScript(aAttributes, aLineNumber);
    }

    // Push the element onto the context stack, so that child
    // containers will hook up to us as their parent.
    rv = mContextStack.Push(element, mState);
    if (NS_FAILED(rv)) return rv;

    mState = eInDocumentElement;
    return NS_OK;
}

nsresult
XULContentSinkImpl::OpenScript(const PRUnichar** aAttributes,
                               const PRUint32 aLineNumber)
{
  nsresult rv = NS_OK;
  PRBool isJavaScript = PR_TRUE;
  const char* jsVersionString = nsnull;

  // Look for SRC attribute and look for a LANGUAGE attribute
  nsAutoString src;
  while (*aAttributes) {
      const nsDependentString key(aAttributes[0]);
      if (key.Equals(NS_LITERAL_STRING("src"),
                     nsCaseInsensitiveStringComparator())) {
          src.Assign(aAttributes[1]);
      }
      else if (key.Equals(NS_LITERAL_STRING("type"),
                          nsCaseInsensitiveStringComparator())) {
          nsAutoString  type(aAttributes[1]);
          nsAutoString  mimeType;
          nsAutoString  params;
          nsParserUtils::SplitMimeType(type, mimeType, params);

          isJavaScript = mimeType.EqualsIgnoreCase("application/x-javascript") ||
                         mimeType.EqualsIgnoreCase("text/javascript");
          if (isJavaScript) {
              JSVersion jsVersion = JSVERSION_DEFAULT;
              if (params.Find("version=", PR_TRUE) == 0) {
                  if (params.Length() != 11 || params[8] != '1' || params[9] != '.')
                      jsVersion = JSVERSION_UNKNOWN;
                  else switch (params[10]) {
                      case '0': jsVersion = JSVERSION_1_0; break;
                      case '1': jsVersion = JSVERSION_1_1; break;
                      case '2': jsVersion = JSVERSION_1_2; break;
                      case '3': jsVersion = JSVERSION_1_3; break;
                      case '4': jsVersion = JSVERSION_1_4; break;
                      case '5': jsVersion = JSVERSION_1_5; break;
                      default:  jsVersion = JSVERSION_UNKNOWN;
                  }
              }
              jsVersionString = JS_VersionToString(jsVersion);
          }
      }
      else if (key.Equals(NS_LITERAL_STRING("language"),
                          nsCaseInsensitiveStringComparator())) {
        nsAutoString  lang(aAttributes[1]);
        isJavaScript = nsParserUtils::IsJavaScriptLanguage(lang, &jsVersionString);
      }
      aAttributes += 2;
  }

  // Don't process scripts that aren't JavaScript
  if (isJavaScript) {
      nsXULPrototypeScript* script =
          new nsXULPrototypeScript(aLineNumber,
                                   jsVersionString);
      if (! script)
          return NS_ERROR_OUT_OF_MEMORY;

      // If there is a SRC attribute...
      if (! src.IsEmpty()) {
          // Use the SRC attribute value to load the URL
          rv = NS_NewURI(getter_AddRefs(script->mSrcURI), src, nsnull, mDocumentURL);
          if (NS_FAILED(rv)) {
              delete script;
              return rv;
          }
      }

      // XXXbe temporary, until we serialize/deserialize everything from
      //       the nsXULPrototypeDocument on down...
      nsCOMPtr<nsIFastLoadService> fastLoadService;
      nsCOMPtr<nsIObjectInputStream> objectInput;
      nsXULDocument::GetFastLoadService(getter_AddRefs(fastLoadService));
      if (fastLoadService)
          fastLoadService->GetInputStream(getter_AddRefs(objectInput));

      if (objectInput) {
          PRBool useXULCache = PR_TRUE;
          if (script->mSrcURI) {
              // NB: we must check the XUL script cache early, to avoid
              // multiple deserialization attempts for a given script, which
              // would exhaust the multiplexed stream containing the singly
              // serialized script.  Note that nsXULDocument::LoadScript
              // checks the XUL script cache too, in order to handle the
              // serialization case.
              //
              // We need do this only for <script src='strres.js'> and the
              // like, i.e., out-of-line scripts that are included by several
              // different XUL documents multiplexed in the FastLoad file.
              gXULCache->GetEnabled(&useXULCache);

              if (useXULCache) {
                  gXULCache->GetScript(script->mSrcURI,
                                       NS_REINTERPRET_CAST(void**, &script->mJSObject));
              }
          }

          if (! script->mJSObject) {
              nsCOMPtr<nsIScriptGlobalObjectOwner> globalOwner(do_QueryInterface(mPrototype));
              nsCOMPtr<nsIScriptGlobalObject> globalObject;
              globalOwner->GetScriptGlobalObject(getter_AddRefs(globalObject));
              NS_ASSERTION(globalObject != nsnull, "no prototype global object!");

              nsCOMPtr<nsIScriptContext> scriptContext;
              globalObject->GetContext(getter_AddRefs(scriptContext));
              NS_ASSERTION(scriptContext != nsnull,
                           "no prototype script context!");

              // Keep track of FastLoad failure via rv2, so we can
              // AbortFastLoads if things look bad.
              nsresult rv2 = NS_OK;
              if (script->mSrcURI) {
                  nsCAutoString spec;
                  script->mSrcURI->GetAsciiSpec(spec);
                  rv2 = fastLoadService->StartMuxedDocument(script->mSrcURI,
                                                            spec.get(),
                                                            nsIFastLoadService::NS_FASTLOAD_READ);
                  if (NS_SUCCEEDED(rv2))
                      rv2 = fastLoadService->SelectMuxedDocument(script->mSrcURI);
              } else {
                  // An inline script: check FastLoad multiplexing direction
                  // and skip script->Deserialize if we're not reading from a
                  // muxed stream to get inline objects that are contained in
                  // the current document.
                  PRInt32 direction;
                  fastLoadService->GetDirection(&direction);
                  if (direction != nsIFastLoadService::NS_FASTLOAD_READ)
                      rv2 = NS_ERROR_NOT_AVAILABLE;
              }

              // Don't reflect errors into rv: mJSObject will be null
              // after any error, which suffices to cause the script to
              // be reloaded (from the src= URI, if any) and recompiled.
              // We're better off slow-loading than bailing out due to a
              // FastLoad error.
              if (NS_SUCCEEDED(rv2))
                  rv2 = script->Deserialize(objectInput, scriptContext);

              if (NS_SUCCEEDED(rv2) && script->mSrcURI) {
                  rv2 = fastLoadService->EndMuxedDocument(script->mSrcURI);
                  if (NS_SUCCEEDED(rv2)) {
                      // We must reselect the including XUL document now,
                      // in case the next script in it is an inline one
                      // whose data comes from its stream, not from the
                      // now-exhausted document that was multiplexed for
                      // script->mSrcURI.  Inline scripts are compiled in
                      // XULContentSinkImpl::CloseContainer.
                      rv2 = fastLoadService->SelectMuxedDocument(mDocumentURL);
                  }
              }

              if (NS_SUCCEEDED(rv2)) {
                  if (useXULCache && script->mSrcURI) {
                      PRBool isChrome = PR_FALSE;
                      script->mSrcURI->SchemeIs("chrome", &isChrome);
                      if (isChrome) {
                          gXULCache->PutScript(script->mSrcURI,
                                               NS_REINTERPRET_CAST(void*, script->mJSObject));
                      }
                  }
              } else {
                  // If script->mSrcURI is not in the FastLoad multiplex,
                  // rv2 will be NS_ERROR_NOT_AVAILABLE and we'll try to
                  // update the FastLoad file to hold a serialization of
                  // this script, once it has finished loading.
                  if (rv2 != NS_ERROR_NOT_AVAILABLE)
                      nsXULDocument::AbortFastLoads();
              }
          }
      }

      nsVoidArray* children;
      rv = mContextStack.GetTopChildren(&children);
      if (NS_FAILED(rv)) {
          delete script;
          return rv;
      }

      children->AppendElement(script);

      mConstrainSize = PR_FALSE;

      mContextStack.Push(script, mState);
      mState = eInScript;
  }

  return NS_OK;
}

nsresult
XULContentSinkImpl::AddAttributes(const PRUnichar** aAttributes, 
                                  const PRUint32 aAttrLen, 
                                  nsXULPrototypeElement* aElement)
{
  // Add tag attributes to the element
  nsresult rv;

  // Create storage for the attributes
  nsXULPrototypeAttribute* attrs = nsnull;
  if (aAttrLen > 0) {
    attrs = new nsXULPrototypeAttribute[aAttrLen];
    if (! attrs)
      return NS_ERROR_OUT_OF_MEMORY;
  }

  aElement->mAttributes    = attrs;
  aElement->mNumAttributes = aAttrLen;

  // Copy the attributes into the prototype
  for (; *aAttributes; aAttributes += 2) {
      rv = NormalizeAttributeString(nsDependentString(aAttributes[0]),
                                    *getter_AddRefs(attrs->mNodeInfo));

      if (NS_FAILED(rv)) {
#ifdef PR_LOGGING
          nsAutoString qnameC;
          qnameC.Assign(aAttributes[0]);
          PR_LOG(gLog, PR_LOG_ALWAYS,
                 ("xul: unable to parse attribute '%s' at line %d",
                  NS_ConvertUCS2toUTF8(qnameC).get(), -1)); // XXX pass in line number
#endif
          // Bring it. We'll just fail to copy an attribute that we
          // can't parse. And that's one less attribute to worry
          // about.
          --(aElement->mNumAttributes);
          continue;
      }
      attrs->mValue.SetValue(nsDependentString(aAttributes[1]));

#ifdef PR_LOGGING
      if (PR_LOG_TEST(gLog, PR_LOG_DEBUG)) {
          nsAutoString extraWhiteSpace;
          PRInt32 cnt = mContextStack.Depth();
          while (--cnt >= 0)
              extraWhiteSpace.Append(NS_LITERAL_STRING("  "));
          nsAutoString qnameC,valueC;
          qnameC.Assign(aAttributes[0]);
          valueC.Assign(aAttributes[1]);
          PR_LOG(gLog, PR_LOG_DEBUG,
                 ("xul: %.5d. %s    %s=%s",
                  -1, // XXX pass in line number
                  NS_ConvertUCS2toUTF8(extraWhiteSpace).get(),
                  NS_ConvertUCS2toUTF8(qnameC).get(),
                  NS_ConvertUCS2toUTF8(valueC).get()));
      }
#endif
      ++attrs;
  }

  // XUL elements may require some additional work to compute
  // derived information.
  if (aElement->mNodeInfo->NamespaceEquals(kNameSpaceID_XUL)) {
      nsAutoString value;

      // Compute the element's class list if the element has a 'class' attribute.
      rv = aElement->GetAttr(kNameSpaceID_None, kClassAtom, value);
      if (NS_FAILED(rv)) return rv;

      if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
          rv = nsClassList::ParseClasses(&aElement->mClassList, value);
          if (NS_FAILED(rv)) return rv;
      }

      // Parse the element's 'style' attribute
      rv = aElement->GetAttr(kNameSpaceID_None, kStyleAtom, value);
      if (NS_FAILED(rv)) return rv;

      if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
          if (! mCSSParser) {
              rv = nsComponentManager::CreateInstance(kCSSParserCID,
                                                      nsnull,
                                                      NS_GET_IID(nsICSSParser),
                                                      getter_AddRefs(mCSSParser));

              if (NS_FAILED(rv)) return rv;
          }

          rv = mCSSParser->ParseStyleAttribute(value, mDocumentURL,
                                 getter_AddRefs(aElement->mInlineStyleRule));

          NS_ASSERTION(NS_SUCCEEDED(rv), "unable to parse style rule");
          if (NS_FAILED(rv)) return rv;
      }
  }

  return NS_OK;
}

nsresult
XULContentSinkImpl::AddText(const PRUnichar* aText, 
                            PRInt32 aLength)
{
  // Create buffer when we first need it
  if (0 == mTextSize) {
      mText = (PRUnichar *) PR_MALLOC(sizeof(PRUnichar) * 4096);
      if (nsnull == mText) {
          return NS_ERROR_OUT_OF_MEMORY;
      }
      mTextSize = 4096;
  }

  // Copy data from string into our buffer; flush buffer when it fills up
  PRInt32 offset = 0;
  while (0 != aLength) {
    PRInt32 amount = mTextSize - mTextLength;
    if (amount > aLength) {
        amount = aLength;
    }
    if (0 == amount) {
      if (mConstrainSize) {
        nsresult rv = FlushText();
        if (NS_OK != rv) {
            return rv;
        }
      }
      else {
        mTextSize += aLength;
        mText = (PRUnichar *) PR_REALLOC(mText, sizeof(PRUnichar) * mTextSize);
        if (nsnull == mText) {
            return NS_ERROR_OUT_OF_MEMORY;
        }
      }
    }
    memcpy(&mText[mTextLength],aText + offset, sizeof(PRUnichar) * amount);
    
    mTextLength += amount;
    offset += amount;
    aLength -= amount;
  }

  return NS_OK;
}
