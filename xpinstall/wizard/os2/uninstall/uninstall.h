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
 */

#ifndef _SETUP_H_
#define _SETUP_H_

#ifdef __cplusplus
#define PR_BEGIN_EXTERN_C       extern "C" {
#define PR_END_EXTERN_C         }
#else
#define PR_BEGIN_EXTERN_C
#define PR_END_EXTERN_C
#endif

#define PR_EXTERN(type) type

typedef unsigned int PRUint32;
typedef int PRInt32;

#include <os2.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <direct.h>
#include <tchar.h>
//#include <commctrl.h>
#include "resource.h"

#define CLASS_NAME                      "Uninstall"
#define FILE_INI_UNINSTALL              "uninstall.ini"
#define FILE_LOG_INSTALL                "install_wizard.log"
#define WIZ_TEMP_DIR                    "ns_temp"

/* WTD: What To Do */
#define WTD_ASK                         0
#define WTD_CANCEL                      1
#define WTD_NO                          2
#define WTD_NO_TO_ALL                   3
#define WTD_YES                         4
#define WTD_YES_TO_ALL                  5

/* PP: Parse Path */
#define PP_FILENAME_ONLY                1
#define PP_PATH_ONLY                    2
#define PP_ROOT_ONLY                    3

#define MAX_BUF                         4096
#define ERROR_CODE_HIDE                 0
#define ERROR_CODE_SHOW                 1
#define CX_CHECKBOX                     13
#define CY_CHECKBOX                     13

/* WIZ: WIZARD defines */
#define WIZ_OK                          0
#define WIZ_MEMORY_ALLOC_FAILED         1

/* FO: File Operation */
#define FO_OK                           0
#define FO_SUCCESS                      0
#define FO_ERROR_FILE_NOT_FOUND         1
#define FO_ERROR_DESTINATION_CONFLICT   2
#define FO_ERROR_CHANGE_DIR             3

/* Mode of Setup to run in */
#define NORMAL                          0
#define SILENT                          1
#define AUTO                            2

/* OS: Operating System */
#define OS_WIN9x                        1
#define OS_WIN95_DEBUTE                 2
#define OS_WIN95                        4
#define OS_WIN98                        8
#define OS_NT                          16
#define OS_NT3                         32
#define OS_NT4                         64
#define OS_NT5                        128

typedef struct dlgUninstall
{
  BOOL  bShowDialog;
  PSZ szTitle;
  PSZ szMessage0;
} diU;

typedef struct uninstallStruct
{
  ULONG     dwMode;
  PSZ     szLogPath;
  PSZ     szLogFilename;
  PSZ     szCompanyName;
  PSZ     szProductName;
  PSZ     szDescription;
  PSZ     szUninstallKeyDescription;
  PSZ     szUninstallFilename;
  HINI      hWrMainRoot;
  PSZ     szWrMainKey;
  HINI      hWrRoot;
  PSZ     szWrKey;
  PSZ     szUserAgent;
} uninstallGen;

typedef __int64 ULONGLONG;
typedef struct sInfoLine sil;
struct sInfoLine
{
  ULONGLONG       ullLineNumber;
  PSZ           szLine;
  sil             *Next;
  sil             *Prev;
};

#endif

