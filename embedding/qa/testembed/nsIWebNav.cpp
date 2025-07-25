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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David Epstein <depstein@netscape.com> 
 *   Ashish Bhatt <ashishbhatt@netscape.com> 
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

// File Overview....
//
// Test cases for the nsiWebNavigation Interface



#include "stdafx.h"
#include "testembed.h"
#include "qautils.h"
#include "nsiwebnav.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CNsIWebNav 

CNsIWebNav::CNsIWebNav(nsIWebNavigation *mWebNav)
{
	qaWebNav = mWebNav ;
}

CNsIWebNav::~CNsIWebNav()
{
}


/////////////////////////////////////////////////////////////////////////////
// CNsIWebNav message handlers
// ***********************************************************************
// nsIWebNavigation iface
// ***********************************************************************

// Url table for web navigation
NavElement UrlTable[] = {
   {"http://www.netscape.com/", nsIWebNavigation::LOAD_FLAGS_NONE},
   {"http://www.yahoo.com/", nsIWebNavigation::LOAD_FLAGS_NONE},
   {"http://www.oracle.com/", nsIWebNavigation::LOAD_FLAGS_IS_LINK},
   {"http://www.sun.com/", nsIWebNavigation::LOAD_FLAGS_IS_REFRESH},
   {"http://www.aol.com/", nsIWebNavigation::LOAD_FLAGS_REPLACE_HISTORY},
   {"https://www.yahoo.com/", nsIWebNavigation::LOAD_FLAGS_NONE},
};



void CNsIWebNav::OnStartTests(UINT nMenuID)
{

	switch(nMenuID)
	{
		case ID_INTERFACES_NSIWEBNAV_RUNALLTESTS :
			RunAllTests();
			break ;
		case ID_INTERFACES_NSIWEBNAV_GETCANGOBACK :
		    CanGoBackTest();
			break ;
		case ID_INTERFACES_NSIWEBNAV_GETCANGOFORWARD :
			CanGoForwardTest();
			break ;
		case ID_INTERFACES_NSIWEBNAV_GOBACK  :
			GoBackTest();
			break ;
		case ID_INTERFACES_NSIWEBNAV_GOFORWARD :
			GoForwardTest();
			break ;
		case ID_INTERFACES_NSIWEBNAV_GOTOINDEX :
			GoToIndexTest();
			break ;
		case ID_INTERFACES_NSIWEBNAV_LOADURI :
			LoadUriandReload();
			break ;
		case ID_INTERFACES_NSIWEBNAV_RELOAD  :
			ReloadTest(nsIWebNavigation::LOAD_FLAGS_NONE);
			break ;
		case ID_INTERFACES_NSIWEBNAV_STOP    :
			StopUriTest("http://www.microsoft.com/");
			break ;
		case ID_INTERFACES_NSIWEBNAV_GETDOCUMENT :
			GetDocumentTest();
			break ;
		case ID_INTERFACES_NSIWEBNAV_GETCURRENTURI :
			GetCurrentURITest();
			break ;
		case ID_INTERFACES_NSIWEBNAV_GETSESSIONHISTORY :
			GetSHTest();
			break ;
		case ID_INTERFACES_NSIWEBNAV_SETSESSIONHISTORY :
			break ;
	}

}

void CNsIWebNav::LoadUriandReload()
{
   int i=0;
   // LoadURI() & reload tests

   QAOutput("Run a few LoadURI() tests.", 2);
	
   for (i=0; i < 6; i++)
   {
       LoadUriTest(UrlTable[i].theUri, UrlTable[i].theFlag);
       switch (i)
       {
       case 0:
           ReloadTest(nsIWebNavigation::LOAD_FLAGS_NONE);
           break;
       case 1:
           ReloadTest(nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE);
           break;
       case 2:
           ReloadTest(nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY);
           break;
       // simulate shift-reload
       case 3:
           ReloadTest(nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE |
                      nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY);
           break;
       case 4:
           ReloadTest(nsIWebNavigation::LOAD_FLAGS_CHARSET_CHANGE);
           break;
	   case 5:
           ReloadTest(nsIWebNavigation::LOAD_FLAGS_NONE);
           break;
       }
   }

}


void CNsIWebNav::RunAllTests()
{
   int i=0;

   if ( qaWebNav)
	   QAOutput("We have the web nav object.", 2);
   else {
	   QAOutput("We don't have the web nav object. No tests performed.", 2);
	   return;
   }

   // canGoBack attribute test
   CanGoBackTest();

   // GoBack test
   GoBackTest();

   // canGoForward attribute test
   CanGoForwardTest();

   // GoForward test
   GoForwardTest();

   // GotoIndex test
   GoToIndexTest();

   // LoadURI() & reload tests

   QAOutput("Run a few LoadURI() tests.", 2);

 	
   for (i=0; i < 5; i++)
   {
       LoadUriTest(UrlTable[i].theUri, UrlTable[i].theFlag);
       switch (i)
       {
       case 0:
           ReloadTest(nsIWebNavigation::LOAD_FLAGS_NONE);
           break;
       case 1:
           ReloadTest(nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE);
           break;
       case 2:
           ReloadTest(nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY);
           break;
       // simulate shift-reload
       case 3:
           ReloadTest(nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE |
                      nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY);
           break;
       case 4:
           ReloadTest(nsIWebNavigation::LOAD_FLAGS_CHARSET_CHANGE);
           break;
       }
   }

 
	// Stop() test
   StopUriTest("http://www.microsoft.com/");

   // document test
   GetDocumentTest();
   
   // uri test
   GetCurrentURITest();

   // session history test
   GetSHTest();
}


// ***********************************************************************
// Individual nsIWebNavigation tests

void CNsIWebNav::CanGoBackTest()
{
   PRBool canGoBack = PR_FALSE;
   rv =  qaWebNav->GetCanGoBack(&canGoBack);
   RvTestResult(rv, "GetCanGoBack() attribute test", 2);
   FormatAndPrintOutput("canGoBack value = ", canGoBack, 2);
}

void CNsIWebNav::GoBackTest()
{
   rv =  qaWebNav->GoBack();
   RvTestResult(rv, "GoBack() test", 2);
}

void CNsIWebNav::CanGoForwardTest()
{
   PRBool canGoForward = PR_FALSE;
   rv =  qaWebNav->GetCanGoForward(&canGoForward);
   RvTestResult(rv, "GetCanGoForward() attribute test", 2);
   FormatAndPrintOutput("canGoForward value = ", canGoForward, 2); 
}

void CNsIWebNav::GoForwardTest()
{
   rv =  qaWebNav->GoForward();
   RvTestResult(rv, "GoForward() test", 2);
}

void CNsIWebNav::GoToIndexTest()
{
   PRInt32 theIndex = 0;

   rv =  qaWebNav->GotoIndex(theIndex);
   RvTestResult(rv, "GotoIndex() test", 2);
}

void CNsIWebNav::LoadUriTest(char *theUrl, const unsigned long theFlag)
{
   char theTotalString[500];
   char theFlagName[200];

   switch(theFlag)
   {
   case nsIWebNavigation::LOAD_FLAGS_NONE:
	   strcpy(theFlagName, "LOAD_FLAGS_NONE");
	   break;
   case nsIWebNavigation::LOAD_FLAGS_MASK:
	   strcpy(theFlagName, "LOAD_FLAGS_MASK");
	   break;
   case nsIWebNavigation::LOAD_FLAGS_IS_REFRESH:
	   strcpy(theFlagName, "LOAD_FLAGS_IS_REFRESH");
	   break;
   case nsIWebNavigation::LOAD_FLAGS_IS_LINK:
	   strcpy(theFlagName, "LOAD_FLAGS_IS_LINK");
	   break;
   case nsIWebNavigation::LOAD_FLAGS_BYPASS_HISTORY:
	   strcpy(theFlagName, "LOAD_FLAGS_BYPASS_HISTORY");
	   break;
   case nsIWebNavigation::LOAD_FLAGS_REPLACE_HISTORY:
	   strcpy(theFlagName, "LOAD_FLAGS_REPLACE_HISTORY");
	   break;
   case nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE:
	   strcpy(theFlagName, "LOAD_FLAGS_BYPASS_CACHE");
	   break;
   case nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY:
	   strcpy(theFlagName, "LOAD_FLAGS_BYPASS_PROXY");
	   break;
   case nsIWebNavigation::LOAD_FLAGS_CHARSET_CHANGE:
	   strcpy(theFlagName, "LOAD_FLAGS_CHARSET_CHANGE");
	   break;
   }

   rv = qaWebNav->LoadURI(NS_ConvertASCIItoUCS2(theUrl).get(), 
                          theFlag,
                          nsnull,
                          nsnull,
                          nsnull);
   sprintf(theTotalString, "%s%s%s%s%s", "LoadURI(): ", theUrl, " w/ ", theFlagName, " test");
   RvTestResult(rv, theTotalString, 2);
}

void CNsIWebNav::ReloadTest(const unsigned long theFlag)
{
   char theTotalString[500];
   char theFlagName[200];

   switch(theFlag)
  {
  case nsIWebNavigation::LOAD_FLAGS_NONE:
      strcpy(theFlagName, "LOAD_FLAGS_NONE");
      break;
  case nsIWebNavigation::LOAD_FLAGS_MASK:
      strcpy(theFlagName, "LOAD_FLAGS_MASK");
      break;
  case nsIWebNavigation::LOAD_FLAGS_IS_REFRESH:
      strcpy(theFlagName, "LOAD_FLAGS_IS_REFRESH");
      break;
  case nsIWebNavigation::LOAD_FLAGS_IS_LINK:
      strcpy(theFlagName, "LOAD_FLAGS_IS_LINK");
      break;
  case nsIWebNavigation::LOAD_FLAGS_BYPASS_HISTORY:
      strcpy(theFlagName, "LOAD_FLAGS_BYPASS_HISTORY");
      break;
  case nsIWebNavigation::LOAD_FLAGS_REPLACE_HISTORY:
      strcpy(theFlagName, "LOAD_FLAGS_REPLACE_HISTORY");
      break;
  case nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE:
      strcpy(theFlagName, "LOAD_FLAGS_BYPASS_CACHE");
      break;
  case nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY:
      strcpy(theFlagName, "LOAD_FLAGS_BYPASS_PROXY");
      break;
  case nsIWebNavigation::LOAD_FLAGS_CHARSET_CHANGE:
      strcpy(theFlagName, "LOAD_FLAGS_CHARSET_CHANGE");
      break;
  case nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE | \
      nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY:
      strcpy(theFlagName, "nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE | "
                          "nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY");
      break;
  } 

   rv =  qaWebNav->Reload(theFlag);
   sprintf(theTotalString, "%s%s%s%s", "Reload(): ", " w/ ", theFlagName, " test");
   RvTestResult(rv, theTotalString, 2);
}

void CNsIWebNav::StopUriTest(char *theUrl)
{
   char theTotalString[200];

   qaWebNav->LoadURI(NS_ConvertASCIItoUCS2(theUrl).get(), 
                     nsIWebNavigation::LOAD_FLAGS_NONE,
                     nsnull,
                     nsnull,
                     nsnull);
   rv = qaWebNav->Stop(nsIWebNavigation::STOP_ALL);
   sprintf(theTotalString, "%s%s%s", "Stop(): ", theUrl, " test");
   RvTestResult(rv, theTotalString, 2);
}

void CNsIWebNav::GetDocumentTest()
{
   nsCOMPtr<nsIDOMDocument> theDocument;
   nsCOMPtr<nsIDOMDocumentType> theDocType;
   
   rv =  qaWebNav->GetDocument(getter_AddRefs(theDocument));
   if (!theDocument) {
	  QAOutput("We didn't get the document. Test failed.", 2);
	  return;
   }
   else
	  RvTestResult(rv, "GetDocument() test", 2);

   rv = theDocument->GetDoctype(getter_AddRefs(theDocType));
   RvTestResult(rv, "nsIDOMDocument::GetDoctype() for nsIWebNav test", 2);
}

void CNsIWebNav::GetCurrentURITest()
{
   nsCOMPtr<nsIURI> theUri;

   rv =  qaWebNav->GetCurrentURI(getter_AddRefs(theUri));
   if (!theUri) {
      QAOutput("We didn't get the URI. Test failed.", 2);
	  return;
   }
   else
	  RvTestResult(rv, "GetCurrentURI() test", 2);

   nsCAutoString uriString;
   rv = theUri->GetSpec(uriString);
   RvTestResult(rv, "nsIURI::GetSpec() for nsIWebNav test", 1);

   FormatAndPrintOutput("the nsIWebNav uri = ", uriString, 2);
}

void CNsIWebNav::GetSHTest()
{
   PRInt32 numOfElements;

   nsCOMPtr<nsISHistory> theSessionHistory;
   rv =  qaWebNav->GetSessionHistory(getter_AddRefs(theSessionHistory));
   if (!theSessionHistory) {
      QAOutput("We didn't get the session history. Test failed.", 2);
	  return;
   }
   else
	  RvTestResult(rv, "GetSessionHistory() test", 2);

   rv = theSessionHistory->GetCount(&numOfElements);
   RvTestResult(rv, "nsISHistory::GetCount() for nsIWebNav test", 1);
 
  FormatAndPrintOutput("the sHist entry count = ", numOfElements, 2);
}
