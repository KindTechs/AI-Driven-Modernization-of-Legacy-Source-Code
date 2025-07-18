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
// These are utilities to help with QA tasks.

// Includes routine to post results to a QA log file.

#include "stdafx.h"
#include "TestEmbed.h"
#include "BrowserView.h"
#include "BrowserImpl.h"
#include "BrowserFrm.h"
#include "QAUtils.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

void RvTestResultDlg(nsresult rv, CString pLine,BOOL bClearList)
{
	static CShowTestResults dlgResult ;
	if (!dlgResult)
		dlgResult.Create(IDD_RUNTESTSDLG);

	
	if (bClearList)
	{
		dlgResult.m_ListResults.DeleteAllItems();
		dlgResult.ShowWindow(true);
	}

	if (NS_FAILED(rv))
	   dlgResult.AddItemToList(pLine,false);
	else
	   dlgResult.AddItemToList(pLine,true);

}

void RvTestResult(nsresult rv, const char *pLine, int displayMethod)
{
	// note: default displayMethod = 1 in .h file

	CString strLine = pLine;
	char theOutputLine[200];

	if (NS_FAILED(rv))
	   strLine += " failed.";
	else
	   strLine += " passed.";

	strcpy(theOutputLine, strLine);
	QAOutput(theOutputLine, displayMethod);
}

void WriteToOutputFile(const char *pLine) 
{ 
    CStdioFile myFile; 
    CFileException e; 
    CString strFileName = "c:\\temp\\TestOutput.txt"; 

    if(! myFile.Open( strFileName, CStdioFile::modeCreate | CStdioFile::modeWrite
								| CStdioFile::modeNoTruncate, &e ) ) 
    { 
        CString failCause = "Unable to open file. Reason : "; 
        failCause += e.m_cause; 
        AfxMessageBox(failCause); 
    } 
    else 
    { 
		myFile.SeekToEnd();
        CString strLine = pLine; 
        strLine += "\r\n"; 

        myFile.WriteString(strLine); 

        myFile.Close(); 
    } 
} 

void QAOutput(const char *pLine, int displayMethod)
{
	// note: default displayMethod = 1 in .h file
	// displayMethod 0 = mfc dialog; 1 = output log file; 2 = both
//#if 0
   CString strLine = pLine;

   if (displayMethod == 0)
	   AfxMessageBox(strLine);
   else if (displayMethod == 1)
	   WriteToOutputFile(pLine);
   else 
   {
       WriteToOutputFile(pLine);
	   AfxMessageBox(strLine);
   }
//#endif
}

void FormatAndPrintOutput(const char *theInput, const char *theVar, int outputMode)
{
	nsCString outStr;
	CString strMsg;

	outStr = theInput;
	outStr += theVar;

	strMsg = outStr.get();

	switch (outputMode)
	{
	case 0:
		AfxMessageBox(strMsg); 
		break;
	case 1:
		WriteToOutputFile(outStr.get());
		break;
	case 2:
		WriteToOutputFile(outStr.get());
		AfxMessageBox(strMsg); 
		break;
	}
}

void FormatAndPrintOutput(const char *theInput, nsCAutoString theVar, int outputMode)
{
	nsCString outStr;
	CString strMsg;

	outStr = theInput;
	outStr += theVar;

	strMsg = outStr.get();

	switch (outputMode)
	{
	case 0:
		AfxMessageBox(strMsg); 
		break;
	case 1:
		WriteToOutputFile(outStr.get());
		break;
	case 2:
		WriteToOutputFile(outStr.get());
		AfxMessageBox(strMsg); 
		break;
	}
}

void FormatAndPrintOutput(const char *theInput, int theVar, int outputMode) 
{
	nsCString outStr;
	CString strMsg;

	outStr = theInput;
	outStr.AppendInt(theVar);

	strMsg = outStr.get();

	switch (outputMode)
	{
	case 0:
		AfxMessageBox(strMsg); 
		break;
	case 1:
		WriteToOutputFile(outStr.get());
		break;
	case 2:
		WriteToOutputFile(outStr.get());
		AfxMessageBox(strMsg); 
		break;
	}
}

// stringMsg is returned in case embeddor wishes to use it in the calling method.
void RequestName(nsIRequest *request, nsCString &stringMsg,
						   int displayMethod)
{
	nsresult rv;

	rv = request->GetName(stringMsg);

	if(NS_SUCCEEDED(rv))
	{
		FormatAndPrintOutput("nsIRequest: The request name = ", stringMsg.get(), displayMethod);
	}
	else
		QAOutput("nsIRequest: We didn't get the request name.", displayMethod);

}

void WebProgDOMWindowTest(nsIWebProgress *progress, const char *inString,
								  int displayMethod)
{
	nsresult rv;
	nsCString totalStr1, totalStr2;
	nsCOMPtr<nsIDOMWindow> theDOMWindow;

	totalStr1 = inString;
	totalStr1 += ": Didn't get the DOMWindow. Test failed.";

	totalStr2 = inString;
	totalStr2 += ": nsIWebProgress:DOMWindow attribute test";

	rv = progress->GetDOMWindow(getter_AddRefs(theDOMWindow));
	if (!theDOMWindow)
		QAOutput(totalStr1.get(), displayMethod);
	else
		RvTestResult(rv, totalStr2.get(), displayMethod);
}

void GetTheUri(nsIURI *theUri, int displayMethod)
{
	nsresult rv;
	nsCAutoString uriString;

	rv = theUri->GetSpec(uriString);
    RvTestResult(rv, "nsIURI::GetSpec() test", displayMethod);
    FormatAndPrintOutput("the uri = ", uriString, displayMethod);
}

// used for web progress listener in BrowserImplWebPrgrsLstnr.cpp
void onStateChangeString(char *theStateType, char *theDocType, 
						 nsCString stringMsg, PRUint32 status, int displayMode)
{
	nsCString totalMsg;

	totalMsg = "OnStateChange(): ";
	totalMsg += theStateType;
	totalMsg += ", ";
	totalMsg += theDocType;
	totalMsg += ", ";
	totalMsg += stringMsg;
	totalMsg += ", status (hex) = ";
	totalMsg.AppendInt(status, 16);
	QAOutput(totalMsg.get(), displayMode);
}

/////////////////////////////////////////////////////////////////////////////
// CShowTestResults dialog


CShowTestResults::CShowTestResults(CWnd* pParent /*=NULL*/)
	: CDialog(CShowTestResults::IDD, pParent)
{

	//{{AFX_DATA_INIT(CShowTestResults)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT

}


void CShowTestResults::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CShowTestResults)
	DDX_Control(pDX, IDC_LIST1, m_ListResults);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CShowTestResults, CDialog)
	//{{AFX_MSG_MAP(CShowTestResults)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CShowTestResults message handlers

BOOL CShowTestResults::OnInitDialog() 
{
	
	CDialog::OnInitDialog();
	
	m_ListResults.InsertColumn(0,"Test Case",LVCFMT_LEFT,360);
	m_ListResults.InsertColumn(1,"Result",LVCFMT_LEFT,100);
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}


void CShowTestResults::AddItemToList(LPCTSTR szTestCaseName, BOOL bResult) 
{
	LV_ITEM lvitem ;
		
	lvitem.mask = LVIF_TEXT;
	lvitem.iItem = m_ListResults.GetItemCount();
	lvitem.iSubItem = 0;
	lvitem.pszText = (LPTSTR)szTestCaseName ;
	//Insert the main item
	m_ListResults.InsertItem(&lvitem);

	if (bResult)
	m_ListResults.SetItemText(m_ListResults.GetItemCount()-1,1,"Passed");
	else
	m_ListResults.SetItemText(m_ListResults.GetItemCount()-1,1,"Failed");

	//Insert the sub item
	//m_ListResults.InsertItem(&lvitem);

}
