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
 *   Mike Judge <mjudge@netscape.com>
 *   Chak Nanga <chak@netscape.com> 
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

#ifndef _EDITORFRM_H_
#define _EDITORFRM_H_

#include "nsICommandParams.h"
#include "nsIEditingSession.h"
#include "nsICommandManager.h"
#include "nsIScriptGlobalObject.h"
#include "nsISimpleEnumerator.h"
	
class CEditorFrame : public CBrowserFrame
{	
public:
    CEditorFrame(PRUint32 chromeMask);
    virtual ~CEditorFrame();

protected: 
    DECLARE_DYNAMIC(CEditorFrame)

public:
    BOOL InitEditor();
    NS_METHOD MakeEditable();
    NS_METHOD DoCommand(nsICommandParams *aCommandParams);
    NS_METHOD IsCommandEnabled(const nsAString &aCommand, PRBool *retval);
    NS_METHOD GetCommandState(nsICommandParams *aCommandParams);

// Generated message map functions
protected:
	//{{AFX_MSG(CEditorFrame)
    afx_msg void OnBold();
    afx_msg void OnUpdateBold(CCmdUI* pCmdUI);
    afx_msg void OnItalics();
    afx_msg void OnUpdateItalics(CCmdUI* pCmdUI);
    afx_msg void OnUnderline();
    afx_msg void OnUpdateUnderline(CCmdUI* pCmdUI);
	afx_msg void OnIndent();
	afx_msg void OnUpdateIndent(CCmdUI* pCmdUI);
	afx_msg void OnOutdent();
	afx_msg void OnUpdateOutdent(CCmdUI* pCmdUI);
	afx_msg void OnFontred();
	afx_msg void OnUpdateFontred(CCmdUI* pCmdUI);
	afx_msg void OnFontblack();
	afx_msg void OnUpdateFontblack(CCmdUI* pCmdUI);
	afx_msg void OnBgcolor();
	afx_msg void OnUpdateBgcolor(CCmdUI* pCmdUI);
	afx_msg void OnNobgcolor();
	afx_msg void OnUpdateNobgcolor(CCmdUI* pCmdUI);
	afx_msg void OnFontsizeincrease();
	afx_msg void OnFontsizedecrease();
	afx_msg void OnArial();
	afx_msg void OnTimes();
	afx_msg void OnCourier();
	afx_msg void OnAlignleft();
	afx_msg void OnUpdateAlignleft(CCmdUI* pCmdUI);
	afx_msg void OnAlignright();
	afx_msg void OnUpdateAlignright(CCmdUI* pCmdUI);
	afx_msg void OnAligncenter();
	afx_msg void OnUpdateAligncenter(CCmdUI* pCmdUI);
	afx_msg void OnInsertlink();
	afx_msg void OnEditUndo();
	afx_msg void OnEditRedo();
	afx_msg void OnUpdateEditRedo(CCmdUI* pCmdUI);
	afx_msg void OnUpdateEditUndo(CCmdUI* pCmdUI);
	//}}AFX_MSG

    DECLARE_MESSAGE_MAP()

private:
    NS_METHOD ExecuteStyleCommand(const nsAString &aCommand);
    NS_METHOD ExecuteNoParam(const nsAString &aCommand);
    NS_METHOD MakeCommandParams(const nsAString &aCommand,nsICommandParams **aParams);
    NS_METHOD ExecuteAttribParam(const nsAString &aCommand, const nsAString &aAttribute);
    NS_METHOD GetAttributeParamValue(const nsAString &aCommand, nsString &aValue);

    void UpdateStyleToolBarBtn(const nsAString &aCommand, CCmdUI* pCmdUI);

private:
    nsCOMPtr<nsICommandManager> mCommandManager;
};

#endif //_EDITORFRM_H_
