/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code,
 * released March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *     Sean Su <ssu@netscape.com>
 *     IBM Corp.  
 */

#ifndef _EXTERN_H_
#define _EXTERN_H_

#include "setup.h"

/* external global variables */
extern HOBJECT        hInst;
extern HOBJECT        hSetupRscInst;
extern HOBJECT        hSDInst;
extern HOBJECT        hXPIStubInst;

extern HBITMAP          hbmpBoxChecked;
extern HBITMAP          hbmpBoxCheckedDisabled;
extern HBITMAP          hbmpBoxUnChecked;

extern LHANDLE           hAccelTable;

extern HWND             hDlgCurrent;
extern HWND             hDlgMessage;
extern HWND             hWndMain;

extern PSZ            szEGlobalAlloc;
extern PSZ            szEStringLoad;
extern PSZ            szEDllLoad;
extern PSZ            szEStringNull;
extern PSZ            szTempSetupPath;

extern PSZ            szSetupDir;
extern PSZ            szTempDir;
extern PSZ            szOSTempDir;
extern PSZ            szFileIniConfig;
extern PSZ            szFileIniInstall;

extern PSZ            szSiteSelectorDescription;

extern ULONG            dwWizardState;
extern ULONG            dwSetupType;

extern ULONG            dwTempSetupType;
extern ULONG            gdwUpgradeValue;
extern ULONG            gdwSiteSelectorStatus;

extern BOOL             bSDUserCanceled;
extern BOOL             bIdiArchivesExists;
extern BOOL             bCreateDestinationDir;
extern BOOL             bReboot;
extern BOOL             gbILUseTemp;
extern BOOL             gbPreviousUnfinishedDownload;
extern BOOL             gbIgnoreRunAppX;
extern BOOL             gbIgnoreProgramFolderX;
extern BOOL             gbRestrictedAccess;

extern setupGen         sgProduct;
extern diS              diSetup;
extern diW              diWelcome;
extern diL              diLicense;
extern diST             diSetupType;
extern diSC             diSelectComponents;
extern diSC             diSelectAdditionalComponents;
extern diWI             diWindowsIntegration;
extern diPF             diProgramFolder;
extern diDO             diDownloadOptions;
extern diAS             diAdvancedSettings;
extern diSI             diStartInstall;
extern diD              diDownload;
extern diR              diReboot;
extern siSD             siSDObject;
extern siCF             siCFXpcomFile;
extern siC              *siComponents;
extern ssi              *ssiSiteSelector;
extern char             *SetupFileList[];
extern installGui       sgInstallGui;
extern sems             gErrorMessageStream;
extern sysinfo          gSystemInfo;
extern dsN              *gdsnComponentDSRequirement;

#endif /* _EXTERN_H */

