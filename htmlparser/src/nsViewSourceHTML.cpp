/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * jce2@po.cwru.edu <Jason Eager>: Added pref to turn on/off 
 * Boris Zbarsky <bzbarsky@mit.edu>
 * rbs@maths.uq.edu.au
 * Andreas M. Schneider <clarence@clarence.de>
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

/**
 * MODULE NOTES:
 * @update  gess 4/8/98
 * 
 *         
 */

/*
 * Set NS_VIEWSOURCE_TOKENS_PER_BLOCK to 0 to disable multi-block
 * output.  Multi-block output helps reduce the amount of bidi
 * processing we have to do on the resulting content model.
 */
#define NS_VIEWSOURCE_TOKENS_PER_BLOCK 16

#ifdef RAPTOR_PERF_METRICS
#  define START_TIMER()                    \
    if(mParser) mParser->mParseTime.Start(PR_FALSE); \
    if(mParser) mParser->mDTDTime.Start(PR_FALSE); 

#  define STOP_TIMER()                     \
    if(mParser) mParser->mParseTime.Stop(); \
    if(mParser) mParser->mDTDTime.Stop(); 

#else
#  define STOP_TIMER() 
#  define START_TIMER()
#endif

#include "nsViewSourceHTML.h"
#include "nsCRT.h"
#include "nsParser.h"
#include "nsScanner.h"
#include "nsIParser.h"
#include "nsDTDUtils.h"
#include "nsIContentSink.h"
#include "nsIHTMLContentSink.h"
#include "nsHTMLTokenizer.h"
#include "nsHTMLEntities.h"
#include "nsIPref.h"
#include "nsUnicharUtils.h"

#include "COtherDTD.h"
#include "nsElementTable.h"

#include "prenv.h"  //this is here for debug reasons...
#include "prtypes.h"  //this is here for debug reasons...
#include "prio.h"
#include "plstr.h"
#include "prmem.h"

#ifdef RAPTOR_PERF_METRICS
#include "stopwatch.h"
Stopwatch vsTimer;
#endif


static NS_DEFINE_IID(kClassIID,     NS_VIEWSOURCE_HTML_IID);

static int gErrorThreshold = 10;

// Define this to dump the viewsource stuff to a file
//#define DUMP_TO_FILE
#ifdef DUMP_TO_FILE
#include <stdio.h>
  FILE* gDumpFile=0;
  static const char* gDumpFileName = "/tmp/viewsource.html";
//  static const char* gDumpFileName = "\\temp\\viewsource.html";
#endif // DUMP_TO_FILE

// bug 22022 - these are used to toggle 'Wrap Long Lines' on the viewsource
// window by selectively setting/unsetting the following class defined in
// viewsource.css; the setting is remembered between invocations using a pref.
static const char* kBodyId = "viewsource";
static const char* kBodyClassWrap = "wrap";

/**
 *  This method gets called as part of our COM-like interfaces.
 *  Its purpose is to create an interface to parser object
 *  of some type.
 *  
 *  @update   gess 4/8/98
 *  @param    nsIID  id of object to discover
 *  @param    aInstancePtr ptr to newly discovered interface
 *  @return   NS_xxx result code
 */
nsresult CViewSourceHTML::QueryInterface(const nsIID& aIID, void** aInstancePtr)  
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      

  if(aIID.Equals(NS_GET_IID(nsISupports)))    {  //do IUnknown...
    *aInstancePtr = (nsIDTD*)(this);                                        
  }
  else if(aIID.Equals(NS_GET_IID(nsIDTD))) {  //do IParser base class...
    *aInstancePtr = (nsIDTD*)(this);                                        
  }
  else if(aIID.Equals(kClassIID)) {  //do this class...
    *aInstancePtr = (CViewSourceHTML*)(this);                                        
  }                 
  else {
    *aInstancePtr=0;
    return NS_NOINTERFACE;
  }
  NS_ADDREF_THIS();
  return NS_OK;                                                        
}

/**
 *  This method is defined in nsIParser. It is used to 
 *  cause the COM-like construction of an nsParser.
 *  
 *  @update  gess 4/8/98
 *  @param   nsIParser** ptr to newly instantiated parser
 *  @return  NS_xxx error result
 */
nsresult NS_NewViewSourceHTML(nsIDTD** aInstancePtrResult)
{
  CViewSourceHTML* it = new CViewSourceHTML();

  if (it == 0) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(kClassIID, (void **) aInstancePtrResult);
}


NS_IMPL_ADDREF(CViewSourceHTML)
NS_IMPL_RELEASE(CViewSourceHTML)

/********************************************
 ********************************************/

class CIndirectTextToken : public CTextToken {
public:
  CIndirectTextToken() : CTextToken() {
    mIndirectString=0;
  }
  
  void SetIndirectString(const nsAString& aString) {
    mIndirectString=&aString;
  }

  virtual const nsAString& GetStringValue(void){
    return (const nsAString&)*mIndirectString;
  }

  const nsAString* mIndirectString;
};


/*******************************************************************
  Now define the CSharedVSCOntext class...
 *******************************************************************/

class CSharedVSContext {
public:

  CSharedVSContext() : 
    mEndNode(),
    mStartNode(),
    mTokenNode(),
    mErrorNode(),
    mITextToken(),
    mErrorToken(NS_LITERAL_STRING("error")) {
  }
  
  ~CSharedVSContext() {
  }

  static CSharedVSContext& GetSharedContext() {
    static CSharedVSContext gSharedVSContext;
    return gSharedVSContext;
  }

  nsCParserNode       mEndNode;
  nsCParserNode       mStartNode;
  nsCParserNode       mTokenNode;
  nsCParserNode       mErrorNode;
  CIndirectTextToken  mITextToken;
  CTextToken          mErrorToken;
};

enum {
  VIEW_SOURCE_START_TAG = 0,
  VIEW_SOURCE_END_TAG = 1,
  VIEW_SOURCE_COMMENT = 2,
  VIEW_SOURCE_CDATA = 3,
  VIEW_SOURCE_DOCTYPE = 4,
  VIEW_SOURCE_PI = 5,
  VIEW_SOURCE_ENTITY = 6,
  VIEW_SOURCE_TEXT = 7,
  VIEW_SOURCE_ATTRIBUTE_NAME = 8,
  VIEW_SOURCE_ATTRIBUTE_VALUE = 9,
  VIEW_SOURCE_SUMMARY = 10,
  VIEW_SOURCE_POPUP = 11,
  VIEW_SOURCE_MARKUPDECLARATION = 12
};

static const char* const kElementClasses[] = {
  "start-tag",
  "end-tag",
  "comment",
  "cdata",
  "doctype",
  "pi",
  "entity",
  "text",
  "attribute-name",
  "attribute-value",
  "summary",
  "popup",
  "markupdeclaration"  
};

static const char* const kBeforeText[] = {
  "<",
  "</",
  "",
  "",
  "",
  "",
  "&",
  "",
  "",
  "=",
  "",
  "",
  ""
};

static const char* const kAfterText[] = {
  ">",
  ">",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  ""
};

#ifdef DUMP_TO_FILE
static const char* const kDumpFileBeforeText[] = {
  "&lt;",
  "&lt;/",
  "",
  "",
  "",
  "",
  "&amp;",
  "",
  "",
  "=",
  "",
  "",
  ""
};

static const char* const kDumpFileAfterText[] = {
  "&gt;",
  "&gt;",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  ""
};
#endif // DUMP_TO_FILE

/**
 *  Default constructor
 *  
 *  @update  gess 4/9/98
 *  @param   
 *  @return  
 */
CViewSourceHTML::CViewSourceHTML() : mFilename(), mTags(), mErrors() {
  NS_INIT_REFCNT();

  mStartTag = VIEW_SOURCE_START_TAG;
  mEndTag = VIEW_SOURCE_END_TAG;
  mCommentTag = VIEW_SOURCE_COMMENT;
  mCDATATag = VIEW_SOURCE_CDATA;
  mMarkupDeclaration = VIEW_SOURCE_MARKUPDECLARATION;
  mDocTypeTag = VIEW_SOURCE_DOCTYPE;
  mPITag = VIEW_SOURCE_PI;
  mEntityTag = VIEW_SOURCE_ENTITY;
  mText = VIEW_SOURCE_TEXT;
  mKey = VIEW_SOURCE_ATTRIBUTE_NAME;
  mValue = VIEW_SOURCE_ATTRIBUTE_VALUE;
  mSummaryTag = VIEW_SOURCE_SUMMARY;
  mPopupTag = VIEW_SOURCE_POPUP;
  mSyntaxHighlight = PR_FALSE;
  mWrapLongLines = PR_FALSE;
  nsCOMPtr<nsIPref> thePrefsService(do_GetService(NS_PREF_CONTRACTID));
  if (thePrefsService) {
    thePrefsService->GetBoolPref("view_source.syntax_highlight", &mSyntaxHighlight);
    thePrefsService->GetBoolPref("view_source.wrap_long_lines", &mWrapLongLines);
  }

  mParser=0;
  mSink=0;
  mLineNumber=0;
  mTokenizer=0;
  mDocType=eHTML3_Quirks; // why?
  mHasOpenRoot=PR_FALSE;
  mHasOpenBody=PR_FALSE;

  mTokenCount=0;

#ifdef DUMP_TO_FILE
  gDumpFile = fopen(gDumpFileName,"w");
#endif // DUMP_TO_FILE

}



/**
 *  Default destructor
 *  
 *  @update  gess 4/9/98
 *  @param   
 *  @return  
 */
CViewSourceHTML::~CViewSourceHTML(){
  mParser=0; //just to prove we destructed...
}

/**
 * 
 * @update	gess1/8/99
 * @param 
 * @return
 */
const nsIID& CViewSourceHTML::GetMostDerivedIID(void) const{
  return kClassIID;
}

/**
 * Call this method if you want the DTD to construct a fresh 
 * instance of itself. 
 * @update	gess7/23/98
 * @param 
 * @return
 */
nsresult CViewSourceHTML::CreateNewInstance(nsIDTD** aInstancePtrResult){
  return NS_NewViewSourceHTML(aInstancePtrResult);
}

/**
 * This method is called to determine if the given DTD can parse
 * a document in a given source-type. 
 * NOTE: Parsing always assumes that the end result will involve
 *       storing the result in the main content model.
 * @update	gess6/24/98
 * @param   
 * @return  TRUE if this DTD can satisfy the request; FALSE otherwise.
 */
NS_IMETHODIMP_(eAutoDetectResult)
CViewSourceHTML::CanParse(CParserContext& aParserContext,
                          const nsString& aBuffer, PRInt32 aVersion)
{
  eAutoDetectResult result=eUnknownDetect;

  if(eViewSource==aParserContext.mParserCommand) {
    if(aParserContext.mMimeType.Equals(NS_LITERAL_CSTRING(kPlainTextContentType)) ||
       aParserContext.mMimeType.Equals(NS_LITERAL_CSTRING(kTextCSSContentType)) ||
       aParserContext.mMimeType.Equals(NS_LITERAL_CSTRING(kTextJSContentType)) ||
       aParserContext.mMimeType.Equals(NS_LITERAL_CSTRING(kApplicationJSContentType))) {
      result=eValidDetect;
    }
    if(aParserContext.mMimeType.Equals(NS_LITERAL_CSTRING(kXMLTextContentType)) ||
       aParserContext.mMimeType.Equals(NS_LITERAL_CSTRING(kXMLApplicationContentType)) ||
       aParserContext.mMimeType.Equals(NS_LITERAL_CSTRING(kXHTMLApplicationContentType)) ||
       aParserContext.mMimeType.Equals(NS_LITERAL_CSTRING(kRDFTextContentType)) ||
       aParserContext.mMimeType.Equals(NS_LITERAL_CSTRING(kHTMLTextContentType)) ||
       aParserContext.mMimeType.Equals(NS_LITERAL_CSTRING(kXULTextContentType)) ||
#ifdef MOZ_SVG
       aParserContext.mMimeType.Equals(NS_LITERAL_CSTRING(kSVGTextContentType)) ||
#endif
       aParserContext.mMimeType.Equals(kSGMLTextContentType)) {
      result=ePrimaryDetect;
    }
  }
  return result;
}


/**
  * The parser uses a code sandwich to wrap the parsing process. Before
  * the process begins, WillBuildModel() is called. Afterwards the parser
  * calls DidBuildModel(). 
  * @update	rickg 03.20.2000
  * @param	aParserContext
  * @param	aSink
  * @return	error code (almost always 0)
  */
nsresult CViewSourceHTML::WillBuildModel(const CParserContext& aParserContext,
                                         nsITokenizer* aTokenizer,
                                         nsIContentSink* aSink){

  nsresult result=NS_OK;

#ifdef RAPTOR_PERF_METRICS
  vsTimer.Reset();
  NS_START_STOPWATCH(vsTimer);
#endif 

  STOP_TIMER();
  mSink=(nsIHTMLContentSink*)aSink;

  if((!aParserContext.mPrevContext) && (mSink)) {

    mFilename=aParserContext.mScanner->GetFilename();
    mTags.Truncate();
    mErrors.Assign(NS_LITERAL_STRING(" HTML 4.0 Strict-DTD validation (enabled); [Should use Transitional?].\n"));

    mDocType=aParserContext.mDocType;
    mMimeType=aParserContext.mMimeType;
    mDTDMode=aParserContext.mDTDMode;
    mParserCommand=aParserContext.mParserCommand;
    mTokenizer = aTokenizer;
    mErrorCount=0;
    mTagCount=0;

#ifdef DUMP_TO_FILE
    if (gDumpFile) {
      nsCAutoString filename;
      filename.AssignWithConversion(mFilename);

      fprintf(gDumpFile, "<html>\n");
      fprintf(gDumpFile, "<head>\n");
      fprintf(gDumpFile, "<title>");
      fprintf(gDumpFile, "Source of: ");
      fprintf(gDumpFile, filename);
      fprintf(gDumpFile, "</title>\n");
      fprintf(gDumpFile, "<link rel=\"stylesheet\" type=\"text/css\" href=\"resource:/res/viewsource.css\">\n");
      fprintf(gDumpFile, "</head>\n");
      fprintf(gDumpFile, "<body>\n");
      fprintf(gDumpFile, "<pre>\n");
    }
#endif //DUMP_TO_FILE
  }


  if(eViewSource!=aParserContext.mParserCommand)
    mDocType=ePlainText;
  else mDocType=aParserContext.mDocType;

  mLineNumber=0;
  result = mSink->WillBuildModel(); 

  START_TIMER();
  return result;
}

/**
  * The parser uses a code sandwich to wrap the parsing process. Before
  * the process begins, WillBuildModel() is called. Afterwards the parser
  * calls DidBuildModel(). 
  * @update	gess5/18/98
  * @param	aFilename is the name of the file being parsed.
  * @return	error code (almost always 0)
  */
NS_IMETHODIMP CViewSourceHTML::BuildModel(nsIParser* aParser,nsITokenizer* aTokenizer,nsITokenObserver* anObserver,nsIContentSink* aSink) {
  nsresult result=NS_OK;

  if(aTokenizer && aParser) {

    nsITokenizer*  oldTokenizer=mTokenizer;
    mTokenizer=aTokenizer;
    nsTokenAllocator* theAllocator=mTokenizer->GetTokenAllocator();
    nsAutoString tag;

    if(!mHasOpenRoot) {
      // For the stack-allocated tokens below, it's safe to pass a null
      // token allocator, because there are no attributes on the tokens.
      PRBool didBlock = PR_FALSE;

      tag.Assign(NS_LITERAL_STRING("HTML"));
      CStartToken htmlToken(tag, eHTMLTag_html);
      nsCParserNode htmlNode(&htmlToken, 0/*stack token*/);
      mSink->OpenHTML(htmlNode);

      tag.Assign(NS_LITERAL_STRING("HEAD"));
      CStartToken headToken(tag, eHTMLTag_head);
      nsCParserNode headNode(&headToken, 0/*stack token*/);
      mSink->OpenHead(headNode);

      // Note that XUL with automatically add the prefix "Source of: "
      mSink->SetTitle(mFilename);

      if (theAllocator) {
        tag.Assign(NS_LITERAL_STRING("LINK"));
        CStartToken* theToken=NS_STATIC_CAST(CStartToken*,theAllocator->CreateTokenOfType(eToken_start,eHTMLTag_link,tag));
        if(theToken) {
          CAttributeToken *theAttr;
          nsCParserNode theNode(theToken, theAllocator);

          theAttr=(CAttributeToken*)theAllocator->CreateTokenOfType(eToken_attribute,eHTMLTag_unknown,NS_LITERAL_STRING("stylesheet"));
          theAttr->SetKey(NS_LITERAL_STRING("rel"));
          theNode.AddAttribute(theAttr);

          theAttr=(CAttributeToken*)theAllocator->CreateTokenOfType(eToken_attribute,eHTMLTag_unknown,NS_LITERAL_STRING("text/css"));
          theAttr->SetKey(NS_LITERAL_STRING("type"));
          theNode.AddAttribute(theAttr);

          theAttr=(CAttributeToken*)theAllocator->CreateTokenOfType(eToken_attribute,eHTMLTag_unknown,NS_LITERAL_STRING("resource:/res/viewsource.css"));
          theAttr->SetKey(NS_LITERAL_STRING("href"));
          theNode.AddAttribute(theAttr);

          result = mSink->AddLeaf(theNode);
          didBlock = result == NS_ERROR_HTMLPARSER_BLOCK;
        }
      }

      CEndToken endHeadToken(eHTMLTag_head);
      nsCParserNode endHeadNode(&endHeadToken, 0/*stack token*/);
      result = mSink->CloseHead(endHeadNode);
      if(NS_SUCCEEDED(result)) {
        mHasOpenRoot = PR_TRUE;
        if (didBlock) {
          result = NS_ERROR_HTMLPARSER_BLOCK;
        }
      }
    }
    if (NS_SUCCEEDED(result) && !mHasOpenBody) {
      if(theAllocator) {
        tag.Assign(NS_LITERAL_STRING("BODY"));
        CStartToken* bodyToken=NS_STATIC_CAST(CStartToken*,theAllocator->CreateTokenOfType(eToken_start, eHTMLTag_body, tag));
        if (bodyToken) {
          nsCParserNode bodyNode(bodyToken, theAllocator);
          CAttributeToken *theAttr=nsnull;
          theAttr=(CAttributeToken*)theAllocator->CreateTokenOfType(eToken_attribute,eHTMLTag_unknown,NS_ConvertASCIItoUCS2(kBodyId));
          theAttr->SetKey(NS_LITERAL_STRING("id"));
          bodyNode.AddAttribute(theAttr);

          if (mWrapLongLines) {
            theAttr=(CAttributeToken*)theAllocator->CreateTokenOfType(eToken_attribute,eHTMLTag_unknown,NS_ConvertASCIItoUCS2(kBodyClassWrap));
            theAttr->SetKey(NS_LITERAL_STRING("class"));
            bodyNode.AddAttribute(theAttr);
          }
          result = mSink->OpenBody(bodyNode);
          if(NS_SUCCEEDED(result)) mHasOpenBody=PR_TRUE;
        }
        IF_FREE(bodyToken,theAllocator);
        
        if (NS_SUCCEEDED(result)) {
          CStartToken theToken(eHTMLTag_pre);
          nsCParserNode theNode(&theToken, 0/*stack token*/);
          result=mSink->OpenContainer(theNode);
        }
      }
    }

    mSink->WillProcessTokens();

    while(NS_SUCCEEDED(result)){
      CToken* theToken=mTokenizer->PopToken();
      if(theToken) {
        result=HandleToken(theToken,aParser);
        if(NS_SUCCEEDED(result)) {
          IF_FREE(theToken, mTokenizer->GetTokenAllocator());
          if (mParser->CanInterrupt() &&
              mSink->DidProcessAToken() == NS_ERROR_HTMLPARSER_INTERRUPTED) {
            result = NS_ERROR_HTMLPARSER_INTERRUPTED;
            break;
          }
        }
        else if(NS_ERROR_HTMLPARSER_BLOCK!=result){
          mTokenizer->PushTokenFront(theToken);
        }
      }
      else break;
    }//while
   
    mTokenizer=oldTokenizer;
  }
  else result=NS_ERROR_HTMLPARSER_BADTOKENIZER;
  return result;
}


/**
 * Call this to display an error summary regarding the page.
 * 
 * @update	rickg 6June2000
 * @return  nsresult
 */
nsresult  CViewSourceHTML::GenerateSummary() {
  nsresult result=NS_OK;

  if(mErrorCount && mTagCount) {

    mErrors.Append(NS_LITERAL_STRING("\n\n "));
    mErrors.AppendInt(mErrorCount);
    mErrors.Append(NS_LITERAL_STRING(" error(s) detected -- see highlighted portions.\n"));

    result=WriteTag(mSummaryTag,mErrors,0,PR_FALSE);
  }

  return result;
}

/**
 * Call this to start a new PRE block.  See bug 86355 for why this
 * makes some pages much faster.
 */
void CViewSourceHTML::StartNewPreBlock(void){
  CEndToken endToken(eHTMLTag_pre);
  nsCParserNode endNode(&endToken, 0/*stack token*/);
  mSink->CloseContainer(endNode);

  CStartToken startToken(eHTMLTag_pre);
  nsCParserNode startNode(&startToken, 0/*stack token*/);
  mSink->OpenContainer(startNode);

#ifdef DUMP_TO_FILE
  if (gDumpFile) {
    fprintf(gDumpFile, "</pre>");
    fprintf(gDumpFile, "<pre>");
  }
#endif // DUMP_TO_FILE

  mTokenCount = 0;
}

/**
 * 
 * @update	gess5/18/98
 * @param 
 * @return
 */
NS_IMETHODIMP CViewSourceHTML::DidBuildModel(nsresult anErrorCode,PRBool aNotifySink,nsIParser* aParser,nsIContentSink* aSink){
  nsresult result= NS_OK;

  //ADD CODE HERE TO CLOSE OPEN CONTAINERS...

  if(aParser){

    mParser=(nsParser*)aParser;  //debug XXX
    STOP_TIMER();

    mSink=(nsIHTMLContentSink*)aParser->GetContentSink();
    if((aNotifySink) && (mSink)) {
        //now let's close automatically auto-opened containers...

#ifdef DUMP_TO_FILE
      if(gDumpFile) {
        fprintf(gDumpFile, "</pre>\n");
        fprintf(gDumpFile, "</body>\n");
        fprintf(gDumpFile, "</html>\n");
        fclose(gDumpFile);
      }
#endif // DUMP_TO_FILE

      if(ePlainText!=mDocType) {
        CEndToken theToken(eHTMLTag_pre);
        nsCParserNode preNode(&theToken, 0/*stack token*/);
        mSink->CloseContainer(preNode);
        
        CEndToken bodyToken(eHTMLTag_body);
        nsCParserNode bodyNode(&bodyToken, 0/*stack token*/);
        mSink->CloseBody(bodyNode);
        
        CEndToken htmlToken(eHTMLTag_html);
        nsCParserNode htmlNode(&htmlToken, 0/*stack token*/);
        mSink->CloseHTML(htmlNode);
      }
      result = mSink->DidBuildModel(1);
    }

    START_TIMER();

  }

#ifdef RAPTOR_PERF_METRICS
  NS_STOP_STOPWATCH(vsTimer);
  printf("viewsource timer: ");
  vsTimer.Print();
  printf("\n");
#endif 

  return result;
}

/**
 * Use this id you want to stop the building content model
 * --------------[ Sets DTD to STOP mode ]----------------
 * It's recommended to use this method in accordance with
 * the parser's terminate() method.
 *
 * @update	harishd 07/22/99
 * @param 
 * @return
 */
NS_IMETHODIMP_(void)  
CViewSourceHTML::Terminate() {
}

NS_IMETHODIMP_(PRInt32)  
CViewSourceHTML::GetType() {
  return NS_IPARSER_FLAG_HTML;
}

NS_IMETHODIMP 
CViewSourceHTML::CollectSkippedContent(PRInt32 aTag, nsAString& aContent, PRInt32 &aLineNo)
{
  return NS_OK;
}

/**
 * 
 * @update	gess5/18/98
 * @param 
 * @return
 */
NS_IMETHODIMP CViewSourceHTML::WillResumeParse(nsIContentSink* aSink){
  nsresult result = NS_OK;
  if(mSink) {
    result = mSink->WillResume();
  }
  return result;
}

/**
 * 
 * @update	gess5/18/98
 * @param 
 * @return
 */
NS_IMETHODIMP CViewSourceHTML::WillInterruptParse(nsIContentSink* aSink){
  nsresult result = NS_OK;
  if(mSink) {
    result = mSink->WillInterrupt();
  }
  return result;
}

/**
 * Called by the parser to enable/disable dtd verification of the
 * internal context stack.
 * @update	gess 7/23/98
 * @param 
 * @return
 */
void CViewSourceHTML::SetVerification(PRBool aEnabled)
{
}

/**
 *  This method is called to determine whether or not a tag
 *  of one type can contain a tag of another type.
 *  
 *  @update  gess 3/25/98
 *  @param   aParent -- int tag of parent container
 *  @param   aChild -- int tag of child container
 *  @return  PR_TRUE if parent can contain child
 */
PRBool CViewSourceHTML::CanContain(PRInt32 aParent,PRInt32 aChild) const{
  PRBool result=PR_TRUE;
  return result;
}

/**
 * Give rest of world access to our tag enums, so that CanContain(), etc,
 * become useful.
 */
NS_IMETHODIMP
CViewSourceHTML::StringTagToIntTag(const nsAString &aTag,
                                   PRInt32* aIntTag) const
{
  *aIntTag = nsHTMLTags::LookupTag(aTag);

  return NS_OK;
}

NS_IMETHODIMP_(const PRUnichar *)
CViewSourceHTML::IntTagToStringTag(PRInt32 aIntTag) const
{
  const PRUnichar *str_ptr = nsHTMLTags::GetStringValue((nsHTMLTag)aIntTag);

  NS_ASSERTION(str_ptr, "Bad tag enum passed to COtherDTD::IntTagToStringTag()"
               "!!");

  return str_ptr;
}

NS_IMETHODIMP
CViewSourceHTML::ConvertEntityToUnicode(const nsAString& aEntity,
                                        PRInt32* aUnicode) const
{
  *aUnicode = nsHTMLEntities::EntityToUnicode(aEntity);

  return NS_OK;
}


PRBool CViewSourceHTML::IsBlockElement(PRInt32 aTagID,PRInt32 aParentID) const {
  PRBool result=PR_FALSE;
  return result;
}

PRBool CViewSourceHTML::IsInlineElement(PRInt32 aTagID,PRInt32 aParentID) const {
  PRBool result=PR_FALSE;
  return result;
}

/**
 *  This method gets called to determine whether a given 
 *  tag is itself a container
 *  
 *  @update  gess 3/25/98
 *  @param   aTag -- tag to test for containership
 *  @return  PR_TRUE if given tag can contain other tags
 */
PRBool CViewSourceHTML::IsContainer(PRInt32 aTag) const{
  PRBool result=PR_TRUE;
  return result;
}

/**
 *  This method gets called when a tag needs to write it's attributes
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  result status
 */
nsresult CViewSourceHTML::WriteAttributes(PRInt32 attrCount) {
  nsresult result=NS_OK;
  
  if(attrCount){ //go collect the attributes...

    CSharedVSContext& theContext=CSharedVSContext::GetSharedContext();

    int attr=0;
    for(attr=0;attr<attrCount;attr++){
      CToken* theToken=mTokenizer->PeekToken();
      if(theToken)  {
        eHTMLTokenTypes theType=eHTMLTokenTypes(theToken->GetTokenType());
        if(eToken_attribute==theType){
          mTokenizer->PopToken(); //pop it for real...
          theContext.mTokenNode.AddAttribute(theToken);  //and add it to the node.

          CAttributeToken* theAttrToken=(CAttributeToken*)theToken;
          const nsAString& theKey=theAttrToken->GetKey();

          result=WriteTag(mKey,theKey,0,PR_FALSE);
          const nsAString& theValue=theAttrToken->GetValue();

          if((0<theValue.Length()) || (theAttrToken->mHasEqualWithoutValue)){
            result=WriteTag(mValue,theValue,0,PR_FALSE);
          }
        } 
      }
      else return kEOF;
    }
  }

  return result;
}

/**
 *  This method gets called when a tag needs to be sent out
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  result status
 */
nsresult CViewSourceHTML::WriteTag(PRInt32 aTagType,const nsAString & aText,PRInt32 attrCount,PRBool aNewlineRequired) {
  static nsString       theString;

  nsresult result=NS_OK;

  CSharedVSContext& theContext=CSharedVSContext::GetSharedContext();

  nsTokenAllocator* theAllocator=mTokenizer->GetTokenAllocator();
  NS_ASSERTION(0!=theAllocator,"Error: no allocator");
  if(0==theAllocator)
    return NS_ERROR_FAILURE;

  if (kBeforeText[aTagType][0] != 0) {
    nsAutoString beforeText;
    beforeText.AssignWithConversion(kBeforeText[aTagType]);
    theContext.mITextToken.SetIndirectString(beforeText);
    nsCParserNode theNode(&theContext.mITextToken, 0/*stack token*/);
    mSink->AddLeaf(theNode);
  }
#ifdef DUMP_TO_FILE
  if (gDumpFile && kDumpFileBeforeText[aTagType][0])
    fprintf(gDumpFile, kDumpFileBeforeText[aTagType]);
#endif // DUMP_TO_FILE
  
  if (mSyntaxHighlight && aTagType != mText) {
    CStartToken* theTagToken=NS_STATIC_CAST(CStartToken*,theAllocator->CreateTokenOfType(eToken_start,eHTMLTag_span,NS_LITERAL_STRING("SPAN")));

    theContext.mStartNode.Init(theTagToken, theAllocator);
    CAttributeToken* theAttr=(CAttributeToken*)theAllocator->CreateTokenOfType(eToken_attribute,eHTMLTag_unknown,NS_ConvertASCIItoUCS2(kElementClasses[aTagType]));
    theAttr->SetKey(NS_LITERAL_STRING("class"));
    theContext.mStartNode.AddAttribute(theAttr);
    mSink->OpenContainer(theContext.mStartNode);  //emit <starttag>...
#ifdef DUMP_TO_FILE
    if (gDumpFile) {
      fprintf(gDumpFile, "<span class=\"");
      fprintf(gDumpFile, kElementClasses[aTagType]);
      fprintf(gDumpFile, "\">");
    }
#endif // DUMP_TO_FILE
  }

  STOP_TIMER();

  theContext.mITextToken.SetIndirectString(aText);  //now emit the tag name...

  nsCParserNode theNode(&theContext.mITextToken, 0/*stack token*/);
  mSink->AddLeaf(theNode);
#ifdef DUMP_TO_FILE
  if (gDumpFile) {
    nsCAutoString cstr;
    cstr.AssignWithConversion(aText);
    fprintf(gDumpFile, cstr);
  }
#endif // DUMP_TO_FILE

  if (mSyntaxHighlight && aTagType != mText) {
    theContext.mStartNode.ReleaseAll(); 
    CEndToken theEndToken(eHTMLTag_span);
    theContext.mEndNode.Init(&theEndToken, 0/*stack token*/);
    mSink->CloseContainer(theContext.mEndNode);  //emit </starttag>...
#ifdef DUMP_TO_FILE
    if (gDumpFile)
      fprintf(gDumpFile, "</span>");
#endif //DUMP_TO_FILE
  }

  if(attrCount){
    result=WriteAttributes(attrCount);
  }

  if (kAfterText[aTagType][0] != 0) {
    nsAutoString afterText;
    afterText.AssignWithConversion(kAfterText[aTagType]);
    theContext.mITextToken.SetIndirectString(afterText);
    nsCParserNode theNode(&theContext.mITextToken, 0/*stack token*/);
    mSink->AddLeaf(theNode);
  }
#ifdef DUMP_TO_FILE
  if (gDumpFile && kDumpFileAfterText[aTagType][0])
    fprintf(gDumpFile, kDumpFileAfterText[aTagType]);
#endif // DUMP_TO_FILE


  START_TIMER();

  return result;
}

/**
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- token object to be put into content model
 *  @return  0 if all is well; non-zero is an error
 */
NS_IMETHODIMP CViewSourceHTML::HandleToken(CToken* aToken,nsIParser* aParser) {
  nsresult        result=NS_OK;
  CHTMLToken*     theToken= (CHTMLToken*)(aToken);
  eHTMLTokenTypes theType= (eHTMLTokenTypes)theToken->GetTokenType();
 
  mParser=(nsParser*)aParser;
  mSink=(nsIHTMLContentSink*)aParser->GetContentSink();
 
  CSharedVSContext& theContext=CSharedVSContext::GetSharedContext();
  theContext.mTokenNode.Init(theToken, mTokenizer->GetTokenAllocator());

  eHTMLTags theParent=(mTags.Length()) ? (eHTMLTags)mTags.Last() : eHTMLTag_unknown;
  eHTMLTags theChild=(eHTMLTags)aToken->GetTypeID();
  
  switch(theType) {
    
    case eToken_start:
      {
        mTagCount++;

        const nsAString& startValue = aToken->GetStringValue();
        result=WriteTag(mStartTag,startValue,aToken->GetAttributeCount(),PR_TRUE);

        if((ePlainText!=mDocType) && mParser && (NS_OK==result)) {
          result = mSink->NotifyTagObservers(&theContext.mTokenNode);
        }
      }
      break;

    case eToken_end:
      {
        if(theParent==theChild) {
          mTags.Truncate(mTags.Length()-1);
        }

        const nsAString& endValue = aToken->GetStringValue();
        result=WriteTag(mEndTag,endValue,aToken->GetAttributeCount(),PR_TRUE);
      }
      break;

    case eToken_cdatasection:
      {
        nsAutoString theStr;
        theStr.Assign(NS_LITERAL_STRING("<!"));
        theStr.Append(aToken->GetStringValue());
        theStr.Append(NS_LITERAL_STRING(">"));
        result=WriteTag(mCDATATag,theStr,0,PR_TRUE);
      }
      break;

    case eToken_markupDecl:
      {
        nsAutoString theStr;
        theStr.Assign(NS_LITERAL_STRING("<!"));
        theStr.Append(aToken->GetStringValue());
        theStr.Append(NS_LITERAL_STRING(">"));
        result=WriteTag(mMarkupDeclaration,theStr,0,PR_TRUE);
      }
      break;

    case eToken_comment: 
      {
        nsAutoString theStr;
        aToken->AppendSourceTo(theStr);
        result=WriteTag(mCommentTag,theStr,0,PR_TRUE);
      }
      break;

    case eToken_doctypeDecl:
      {
        const nsAString& doctypeValue = aToken->GetStringValue();
        result=WriteTag(mDocTypeTag,doctypeValue,0,PR_TRUE);
      }
      break;

    case eToken_newline:
      {
        const nsAString& newlineValue = aToken->GetStringValue();
        mLineNumber++; 
        result=WriteTag(mText,newlineValue,0,PR_FALSE);
        mTokenCount++;
        if (NS_VIEWSOURCE_TOKENS_PER_BLOCK > 0 &&
            mTokenCount > NS_VIEWSOURCE_TOKENS_PER_BLOCK)
          StartNewPreBlock();
      }
      break;

    case eToken_whitespace:
      {
        const nsAString& wsValue = aToken->GetStringValue();
        result=WriteTag(mText,wsValue,0,PR_FALSE);
        mTokenCount++;
        if (NS_VIEWSOURCE_TOKENS_PER_BLOCK > 0 &&
            mTokenCount > NS_VIEWSOURCE_TOKENS_PER_BLOCK &&
            !wsValue.IsEmpty()) {
          PRUnichar ch = wsValue.Last();
          if (ch == kLF || ch == kCR)
            StartNewPreBlock();
        }
      }
      break;

    case eToken_text:
      {
        const nsAString& str = aToken->GetStringValue();         
        result=WriteTag(mText,str,aToken->GetAttributeCount(),PR_TRUE);
        mTokenCount++;
        if (NS_VIEWSOURCE_TOKENS_PER_BLOCK > 0 &&
            mTokenCount > NS_VIEWSOURCE_TOKENS_PER_BLOCK && !str.IsEmpty()) {
          PRUnichar ch = str.Last();
          if (ch == kLF || ch == kCR)
            StartNewPreBlock();
        }
      }

      break;

    case eToken_entity:
      {
        nsAutoString theStr;
        theStr.Assign(aToken->GetStringValue());
        if(!theStr.Equals(NS_LITERAL_STRING("XI"), nsCaseInsensitiveStringComparator())) {
          PRUnichar theChar=theStr.CharAt(0);
          if((nsCRT::IsAsciiDigit(theChar)) || ('X'==theChar) || ('x'==theChar)){
            theStr.Assign(NS_LITERAL_STRING("#") + theStr);
          }
        }
        result=WriteTag(mEntityTag,theStr,0,PR_FALSE);
      }
      break;

    case eToken_instruction:
      result=WriteTag(mPITag,aToken->GetStringValue(),0,PR_TRUE);

    default:
      result=NS_OK;
  }//switch

  theContext.mTokenNode.ReleaseAll(); 

  return result;
}

