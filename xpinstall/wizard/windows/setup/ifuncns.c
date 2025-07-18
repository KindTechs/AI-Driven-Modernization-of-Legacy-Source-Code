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
 *     Curt Patrick <curt@netscape.com>
 */

#include "extern.h"
#include "extra.h"
#include "dialogs.h"
#include "shortcut.h"
#include "ifuncns.h"
#include "wizverreg.h"
#include "logging.h"
#include <logkeys.h>

HRESULT TimingCheck(DWORD dwTiming, LPSTR szSection, LPSTR szFile)
{
  char szBuf[MAX_BUF_TINY];

  GetPrivateProfileString(szSection, "Timing", "", szBuf, sizeof(szBuf), szFile);
  if(*szBuf != '\0')
  {
    switch(dwTiming)
    {
      case T_PRE_DOWNLOAD:
        if(lstrcmpi(szBuf, "pre download") == 0)
          return(TRUE);
        break;

      case T_POST_DOWNLOAD:
        if(lstrcmpi(szBuf, "post download") == 0)
          return(TRUE);
        break;

      case T_PRE_XPCOM:
        if(lstrcmpi(szBuf, "pre xpcom") == 0)
          return(TRUE);
        break;

      case T_POST_XPCOM:
        if(lstrcmpi(szBuf, "post xpcom") == 0)
          return(TRUE);
        break;

      case T_PRE_SMARTUPDATE:
        if(lstrcmpi(szBuf, "pre smartupdate") == 0)
          return(TRUE);
        break;

      case T_POST_SMARTUPDATE:
        if(lstrcmpi(szBuf, "post smartupdate") == 0)
          return(TRUE);
        break;

      case T_PRE_LAUNCHAPP:
        if(lstrcmpi(szBuf, "pre launchapp") == 0)
          return(TRUE);
        break;

      case T_POST_LAUNCHAPP:
        if(lstrcmpi(szBuf, "post launchapp") == 0)
          return(TRUE);
        break;

      case T_PRE_ARCHIVE:
        if(lstrcmpi(szBuf, "pre archive") == 0)
          return(TRUE);
        break;

      case T_POST_ARCHIVE:
        if(lstrcmpi(szBuf, "post archive") == 0)
          return(TRUE);
        break;

      case T_DEPEND_REBOOT:
        if(lstrcmpi(szBuf, "depend reboot") == 0)
          return(TRUE);
        break;
    }
  }
  return(FALSE);
}

char *BuildNumberedString(DWORD dwIndex, char *szInputStringPrefix, char *szInputString, char *szOutBuf, DWORD dwOutBufSize)
{
  if((szInputStringPrefix) && (*szInputStringPrefix != '\0'))
    wsprintf(szOutBuf, "%s-%s%d", szInputStringPrefix, szInputString, dwIndex);
  else
    wsprintf(szOutBuf, "%s%d", szInputString, dwIndex);

  return(szOutBuf);
}

void GetUserAgentShort(char *szUserAgent, char *szOutUAShort, DWORD dwOutUAShortSize)
{
  char *ptrFirstSpace = NULL;

  ZeroMemory(szOutUAShort, dwOutUAShortSize);
  if((szUserAgent == NULL) || (*szUserAgent == '\0'))
    return;

  ptrFirstSpace = strstr(szUserAgent, " ");
  if(ptrFirstSpace != NULL)
  {
    *ptrFirstSpace = '\0';
    lstrcpy(szOutUAShort, szUserAgent);
    *ptrFirstSpace = ' ';
  }
}

DWORD GetWinRegSubKeyProductPath(HKEY hkRootKey, char *szInKey, char *szReturnSubKey, DWORD dwReturnSubKeySize, char *szInSubSubKey, char *szInName, char *szCompare, BOOL bUseCurrentVersion)
{
  char      *szRv = NULL;
  char      szKey[MAX_BUF];
  char      szBuf[MAX_BUF];
  char      szCurrentVersion[MAX_BUF_TINY];
  HKEY      hkHandle;
  DWORD     dwIndex;
  DWORD     dwBufSize;
  DWORD     dwTotalSubKeys;
  DWORD     dwTotalValues;
  FILETIME  ftLastWriteFileTime;
  BOOL      bFoundSubKey;

  bFoundSubKey = FALSE;

  /* get the current version value for this product */
  GetWinReg(hkRootKey, szInKey, "CurrentVersion", szCurrentVersion, sizeof(szCurrentVersion));

  if(RegOpenKeyEx(hkRootKey, szInKey, 0, KEY_READ, &hkHandle) != ERROR_SUCCESS)
  {
    *szReturnSubKey = '\0';
    return(0);
  }

  dwTotalSubKeys = 0;
  dwTotalValues  = 0;
  RegQueryInfoKey(hkHandle, NULL, NULL, NULL, &dwTotalSubKeys, NULL, NULL, &dwTotalValues, NULL, NULL, NULL, NULL);
  for(dwIndex = 0; dwIndex < dwTotalSubKeys; dwIndex++)
  {
    dwBufSize = dwReturnSubKeySize;
    if(RegEnumKeyEx(hkHandle, dwIndex, szReturnSubKey, &dwBufSize, NULL, NULL, NULL, &ftLastWriteFileTime) == ERROR_SUCCESS)
    {
      if(  (*szCurrentVersion != '\0') && (lstrcmpi(szCurrentVersion, szReturnSubKey) != 0)
         ||(!bUseCurrentVersion)
        )
      {
        /* The key found is not the CurrentVersion (current UserAgent), so we can return it to be deleted.
         * We don't want to return the SubKey that is the same as the CurrentVersion because it might
         * have just been created by the current installation process.  So deleting it would be a
         * "Bad Thing" (TM).
         *
         * If it was not created by the current installation process, then it'll be left
         * around which is better than deleting something we will need later. To make sure this case is
         * not encountered, CleanupPreviousVersionRegKeys() should be called at the *end* of the
         * installation process (at least after all the .xpi files have been processed). */
        if(szInSubSubKey && (*szInSubSubKey != '\0'))
          wsprintf(szKey, "%s\\%s\\%s", szInKey, szReturnSubKey, szInSubSubKey);
        else
          wsprintf(szKey, "%s\\%s", szInKey, szReturnSubKey);

        GetWinReg(hkRootKey, szKey, szInName, szBuf, sizeof(szBuf));
        AppendBackSlash(szBuf, sizeof(szBuf));
        if(lstrcmpi(szBuf, szCompare) == 0)
        {
          bFoundSubKey = TRUE;
          /* found one subkey. break out of the for() loop */
          break;
        }
      }
    }
  }

  RegCloseKey(hkHandle);
  if(!bFoundSubKey)
    *szReturnSubKey = '\0';
  return(dwTotalSubKeys);
}

void CleanupPreviousVersionRegKeys(void)
{
  char  szRvSubKey[MAX_PATH + 1];
  char  szSubSubKey[] = "Main";
  char  szName[] = "Install Directory";
  char  szWRMSUninstall[] = "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall";
  char  szPath[MAX_BUF];
  char  szUAShort[MAX_BUF_TINY];
  char  szKey[MAX_BUF];
  char  szCleanupProduct[MAX_BUF];
  DWORD dwSubKeyCount;

  lstrcpy(szPath, sgProduct.szPath);
  if(*sgProduct.szSubPath != '\0')
  {
    AppendBackSlash(szPath, sizeof(szPath));
    lstrcat(szPath, sgProduct.szSubPath);
  }
  AppendBackSlash(szPath, sizeof(szPath));

  do
  {
    /* build product key path here */
    lstrcpy(szCleanupProduct, sgProduct.szProductNameInternal);
    wsprintf(szKey, "Software\\%s\\%s", sgProduct.szCompanyName, sgProduct.szProductNameInternal);
    dwSubKeyCount = GetWinRegSubKeyProductPath(HKEY_LOCAL_MACHINE, szKey, szRvSubKey, sizeof(szRvSubKey), szSubSubKey, szName, szPath, TRUE);

    if( (*szRvSubKey == '\0') && (*sgProduct.szProductNamePrevious != '\0') )
    {
      lstrcpy(szCleanupProduct, sgProduct.szProductNamePrevious);
      wsprintf(szKey, "Software\\%s\\%s", sgProduct.szCompanyName, sgProduct.szProductNamePrevious);
      dwSubKeyCount = GetWinRegSubKeyProductPath(HKEY_LOCAL_MACHINE, szKey, szRvSubKey, sizeof(szRvSubKey), szSubSubKey, szName, szPath, FALSE);
    }

    if(*szRvSubKey != '\0')
    {
      if(dwSubKeyCount > 1)
      {
        AppendBackSlash(szKey, sizeof(szKey));
        lstrcat(szKey, szRvSubKey);
      }
      DeleteWinRegKey(HKEY_LOCAL_MACHINE, szKey, TRUE);

      GetUserAgentShort(szRvSubKey, szUAShort, sizeof(szUAShort));
      if(*szUAShort != '\0')
      {
        /* delete uninstall key that contains product name and its user agent in parenthesis, for
         * example:
         *     Mozilla (0.8)
         */
        wsprintf(szKey, "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\%s (%s)", szCleanupProduct, szUAShort);
        DeleteWinRegKey(HKEY_LOCAL_MACHINE, szKey, TRUE);

        /* delete uninstall key that contains product name and its user agent not in parenthesis,
         * for example:
         *     Mozilla 0.8
         */
        wsprintf(szKey, "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\%s %s", szCleanupProduct, szUAShort);
        DeleteWinRegKey(HKEY_LOCAL_MACHINE, szKey, TRUE);

        /* We are not looking to delete just the product name key, for example:
         *     Mozilla
         *
         * because it might have just been created by the current installation process, so
         * deleting this would be a "Bad Thing" (TM).  Besides, we shouldn't be deleting the
         * CurrentVersion key that might have just gotten created because GetWinRegSubKeyProductPath()
         * will not return the CurrentVersion key.
         */
      }
    }

  } while(*szRvSubKey);
}

void ProcessFileOps(DWORD dwTiming, char *szSectionPrefix)
{
  ProcessUncompressFile(dwTiming, szSectionPrefix);
  ProcessCreateDirectory(dwTiming, szSectionPrefix);
  ProcessMoveFile(dwTiming, szSectionPrefix);
  ProcessCopyFile(dwTiming, szSectionPrefix);
  ProcessCopyFileSequential(dwTiming, szSectionPrefix);
  ProcessSelfRegisterFile(dwTiming, szSectionPrefix);
  ProcessDeleteFile(dwTiming, szSectionPrefix);
  ProcessRemoveDirectory(dwTiming, szSectionPrefix);
  if(!gbIgnoreRunAppX)
    ProcessRunApp(dwTiming, szSectionPrefix);
  ProcessWinReg(dwTiming, szSectionPrefix);
  ProcessProgramFolder(dwTiming, szSectionPrefix);
  ProcessSetVersionRegistry(dwTiming, szSectionPrefix);
}

void ProcessFileOpsForSelectedComponents(DWORD dwTiming)
{
  DWORD dwIndex0;
  siC   *siCObject = NULL;

  dwIndex0  = 0;
  siCObject = SiCNodeGetObject(dwIndex0, TRUE, AC_ALL);
  while(siCObject)
  {
    if(siCObject->dwAttributes & SIC_SELECTED)
      /* Since the archive is selected, we need to process the file ops here */
      ProcessFileOps(dwTiming, siCObject->szReferenceName);

    ++dwIndex0;
    siCObject = SiCNodeGetObject(dwIndex0, TRUE, AC_ALL);
  } /* while(siCObject) */
}

void ProcessFileOpsForAll(DWORD dwTiming)
{
  ProcessFileOps(dwTiming, NULL);
  ProcessFileOpsForSelectedComponents(dwTiming);
}

int VerifyArchive(LPSTR szArchive)
{
  void *vZip;
  int  iTestRv;

  /* Check for the existance of the from (source) file */
  if(!FileExists(szArchive))
    return(FO_ERROR_FILE_NOT_FOUND);

  if((iTestRv = ZIP_OpenArchive(szArchive, &vZip)) == ZIP_OK)
  {
    /* 1st parameter should be NULL or it will fail */
    /* It indicates extract the entire archive */
    iTestRv = ZIP_TestArchive(vZip);
    ZIP_CloseArchive(&vZip);
  }
  return(iTestRv);
}

HRESULT ProcessSetVersionRegistry(DWORD dwTiming, char *szSectionPrefix)
{
  DWORD   dwIndex;
  BOOL    bIsDirectory;
  char    szBuf[MAX_BUF];
  char    szSection[MAX_BUF_TINY];
  char    szRegistryKey[MAX_BUF];
  char    szPath[MAX_BUF];
  char    szVersion[MAX_BUF_TINY];

  dwIndex = 0;
  BuildNumberedString(dwIndex, szSectionPrefix, "Version Registry", szSection, sizeof(szSection));
  GetPrivateProfileString(szSection, "Registry Key", "", szRegistryKey, sizeof(szRegistryKey), szFileIniConfig);
  while(*szRegistryKey != '\0')
  {
    if(TimingCheck(dwTiming, szSection, szFileIniConfig))
    {
      GetPrivateProfileString(szSection, "Version", "", szVersion, sizeof(szVersion), szFileIniConfig);
      GetPrivateProfileString(szSection, "Path",    "", szBuf,     sizeof(szBuf),     szFileIniConfig);
      DecryptString(szPath, szBuf);
      if(FileExists(szPath) & FILE_ATTRIBUTE_DIRECTORY)
        bIsDirectory = TRUE;
      else
        bIsDirectory = FALSE;

      lstrcpy(szBuf, sgProduct.szPath);
      if(sgProduct.szSubPath != '\0')
      {
        AppendBackSlash(szBuf, sizeof(szBuf));
        lstrcat(szBuf, sgProduct.szSubPath);
      }

      VR_CreateRegistry(VR_DEFAULT_PRODUCT_NAME, szBuf, NULL);
      VR_Install(szRegistryKey, szPath, szVersion, bIsDirectory);
      VR_Close();
    }

    ++dwIndex;
    BuildNumberedString(dwIndex, szSectionPrefix, "Version Registry", szSection, sizeof(szSection));
    GetPrivateProfileString(szSection, "Registry Key", "", szRegistryKey, sizeof(szRegistryKey), szFileIniConfig);
  }
  return(FO_SUCCESS);
}

HRESULT FileUncompress(LPSTR szFrom, LPSTR szTo)
{
  char  szBuf[MAX_BUF];
  DWORD dwReturn;
  void  *vZip;

  dwReturn = FO_SUCCESS;
  /* Check for the existance of the from (source) file */
  if(!FileExists(szFrom))
    return(FO_ERROR_FILE_NOT_FOUND);

  /* Check for the existance of the to (destination) path */
  dwReturn = FileExists(szTo);
  if(dwReturn && !(dwReturn & FILE_ATTRIBUTE_DIRECTORY))
  {
    /* found a file with the same name as the destination path */
    return(FO_ERROR_DESTINATION_CONFLICT);
  }
  else if(!dwReturn)
  {
    lstrcpy(szBuf, szTo);
    AppendBackSlash(szBuf, sizeof(szBuf));
    CreateDirectoriesAll(szBuf, FALSE);
  }

  GetCurrentDirectory(MAX_BUF, szBuf);
  if(SetCurrentDirectory(szTo) == FALSE)
    return(FO_ERROR_CHANGE_DIR);

  if((dwReturn = ZIP_OpenArchive(szFrom, &vZip)) != ZIP_OK)
    return(dwReturn);

  /* 1st parameter should be NULL or it will fail */
  /* It indicates extract the entire archive */
  dwReturn = ExtractDirEntries(NULL, vZip);
  ZIP_CloseArchive(&vZip);

  if(SetCurrentDirectory(szBuf) == FALSE)
    return(FO_ERROR_CHANGE_DIR);

  return(dwReturn);
}

HRESULT ProcessXpcomFile()
{
  char szSource[MAX_BUF];
  char szDestination[MAX_BUF];
  DWORD dwErr;

  if(*siCFXpcomFile.szMessage != '\0')
    ShowMessage(siCFXpcomFile.szMessage, TRUE);

  if((dwErr = FileUncompress(siCFXpcomFile.szSource, siCFXpcomFile.szDestination)) != FO_SUCCESS)
  {
    char szMsg[MAX_BUF];
    char szErrorString[MAX_BUF];

    if(*siCFXpcomFile.szMessage != '\0')
      ShowMessage(siCFXpcomFile.szMessage, FALSE);

    LogISProcessXpcomFile(LIS_FAILURE, dwErr);
    GetPrivateProfileString("Strings", "Error File Uncompress", "", szErrorString, sizeof(szErrorString), szFileIniConfig);
    wsprintf(szMsg, szErrorString, siCFXpcomFile.szSource, dwErr);
    PrintError(szMsg, ERROR_CODE_HIDE);
    return(dwErr);
  }
  LogISProcessXpcomFile(LIS_SUCCESS, dwErr);

  /* copy msvcrt.dll and msvcirt.dll to the bin of the Xpcom temp dir:
   *   (c:\temp\Xpcom.ns\bin)
   * This is incase these files do not exist on the system */
  lstrcpy(szSource, siCFXpcomFile.szDestination);
  AppendBackSlash(szSource, sizeof(szSource));
  lstrcat(szSource, "ms*.dll");

  lstrcpy(szDestination, siCFXpcomFile.szDestination);
  AppendBackSlash(szDestination, sizeof(szDestination));
  lstrcat(szDestination, "bin");

  FileCopy(szSource, szDestination, TRUE, FALSE);

  if(*siCFXpcomFile.szMessage != '\0')
    ShowMessage(siCFXpcomFile.szMessage, FALSE);

  return(FO_SUCCESS);
}

HRESULT CleanupXpcomFile()
{
  if(siCFXpcomFile.bCleanup == TRUE)
    DirectoryRemove(siCFXpcomFile.szDestination, TRUE);

  return(FO_SUCCESS);
}

#define SETUP_STATE_REG_KEY "Software\\%s\\%s\\%s\\Setup"

HRESULT CleanupArgsRegistry()
{
  char  szKey[MAX_BUF];

  wsprintf(szKey, SETUP_STATE_REG_KEY, sgProduct.szCompanyName, sgProduct.szProductNameInternal,
    sgProduct.szUserAgent);
  DeleteWinRegValue(HKEY_CURRENT_USER, szKey, "browserargs");
  return(FO_SUCCESS);
}

HRESULT ProcessUncompressFile(DWORD dwTiming, char *szSectionPrefix)
{
  DWORD   dwIndex;
  BOOL    bOnlyIfExists;
  char    szBuf[MAX_BUF];
  char    szSection[MAX_BUF];
  char    szSource[MAX_BUF];
  char    szDestination[MAX_BUF];

  dwIndex = 0;
  BuildNumberedString(dwIndex, szSectionPrefix, "Uncompress File", szSection, sizeof(szSection));
  GetPrivateProfileString(szSection, "Source", "", szBuf, sizeof(szBuf), szFileIniConfig);
  while(*szBuf != '\0')
  {
    if(TimingCheck(dwTiming, szSection, szFileIniConfig))
    {
      DecryptString(szSource, szBuf);
      GetPrivateProfileString(szSection, "Destination", "", szBuf, sizeof(szBuf), szFileIniConfig);
      DecryptString(szDestination, szBuf);
      GetPrivateProfileString(szSection, "Only If Exists", "", szBuf, sizeof(szBuf), szFileIniConfig);
      if(lstrcmpi(szBuf, "TRUE") == 0)
        bOnlyIfExists = TRUE;
      else
        bOnlyIfExists = FALSE;

      if((!bOnlyIfExists) || (bOnlyIfExists && FileExists(szDestination)))
      {
        DWORD dwErr;

        GetPrivateProfileString(szSection, "Message",     "", szBuf, sizeof(szBuf), szFileIniConfig);
        ShowMessage(szBuf, TRUE);
        if((dwErr = FileUncompress(szSource, szDestination)) != FO_SUCCESS)
        {
          char szMsg[MAX_BUF];
          char szErrorString[MAX_BUF];

          ShowMessage(szBuf, FALSE);
          GetPrivateProfileString("Strings", "Error File Uncompress", "", szErrorString, sizeof(szErrorString), szFileIniConfig);
          wsprintf(szMsg, szErrorString, szSource, dwErr);
          PrintError(szMsg, ERROR_CODE_HIDE);
          return(dwErr);
        }

        ShowMessage(szBuf, FALSE);
      }
    }

    ++dwIndex;
    BuildNumberedString(dwIndex, szSectionPrefix, "Uncompress File", szSection, sizeof(szSection));
    GetPrivateProfileString(szSection, "Source", "", szBuf, sizeof(szBuf), szFileIniConfig);
  }
  return(FO_SUCCESS);
}

HRESULT FileMove(LPSTR szFrom, LPSTR szTo)
{
  HANDLE          hFile;
  WIN32_FIND_DATA fdFile;
  char            szFromDir[MAX_BUF];
  char            szFromTemp[MAX_BUF];
  char            szToTemp[MAX_BUF];
  char            szBuf[MAX_BUF];
  BOOL            bFound;

  /* From file path exists and To file path does not exist */
  if((FileExists(szFrom)) && (!FileExists(szTo)))
  {
    MoveFile(szFrom, szTo);

    /* log the file move command */
    wsprintf(szBuf, "%s to %s", szFrom, szTo);
    UpdateInstallLog(KEY_MOVE_FILE, szBuf, FALSE);

    return(FO_SUCCESS);
  }
  /* From file path exists and To file path exists */
  if(FileExists(szFrom) && FileExists(szTo))
  {
    /* Since the To file path exists, assume it to be a directory and proceed.      */
    /* We don't care if it's a file.  If it is a file, then config.ini needs to be  */
    /* corrected to remove the file before attempting a MoveFile().                 */
    lstrcpy(szToTemp, szTo);
    AppendBackSlash(szToTemp, sizeof(szToTemp));
    ParsePath(szFrom, szBuf, sizeof(szBuf), FALSE, PP_FILENAME_ONLY);
    lstrcat(szToTemp, szBuf);
    MoveFile(szFrom, szToTemp);

    /* log the file move command */
    wsprintf(szBuf, "%s to %s", szFrom, szToTemp);
    UpdateInstallLog(KEY_MOVE_FILE, szBuf, FALSE);

    return(FO_SUCCESS);
  }

  ParsePath(szFrom, szFromDir, sizeof(szFromDir), FALSE, PP_PATH_ONLY);

  if((hFile = FindFirstFile(szFrom, &fdFile)) == INVALID_HANDLE_VALUE)
    bFound = FALSE;
  else
    bFound = TRUE;

  while(bFound)
  {
    if((lstrcmpi(fdFile.cFileName, ".") != 0) && (lstrcmpi(fdFile.cFileName, "..") != 0))
    {
      /* create full path string including filename for source */
      lstrcpy(szFromTemp, szFromDir);
      AppendBackSlash(szFromTemp, sizeof(szFromTemp));
      lstrcat(szFromTemp, fdFile.cFileName);

      /* create full path string including filename for destination */
      lstrcpy(szToTemp, szTo);
      AppendBackSlash(szToTemp, sizeof(szToTemp));
      lstrcat(szToTemp, fdFile.cFileName);

      MoveFile(szFromTemp, szToTemp);

      /* log the file move command */
      wsprintf(szBuf, "%s to %s", szFromTemp, szToTemp);
      UpdateInstallLog(KEY_MOVE_FILE, szBuf, FALSE);
    }

    bFound = FindNextFile(hFile, &fdFile);
  }

  FindClose(hFile);
  return(FO_SUCCESS);
}

HRESULT ProcessMoveFile(DWORD dwTiming, char *szSectionPrefix)
{
  DWORD dwIndex;
  char  szBuf[MAX_BUF];
  char  szSection[MAX_BUF];
  char  szSource[MAX_BUF];
  char  szDestination[MAX_BUF];

  dwIndex = 0;
  BuildNumberedString(dwIndex, szSectionPrefix, "Move File", szSection, sizeof(szSection));
  GetPrivateProfileString(szSection, "Source", "", szBuf, sizeof(szBuf), szFileIniConfig);
  while(*szBuf != '\0')
  {
    if(TimingCheck(dwTiming, szSection, szFileIniConfig))
    {
      DecryptString(szSource, szBuf);
      GetPrivateProfileString(szSection, "Destination", "", szBuf, sizeof(szBuf), szFileIniConfig);
      DecryptString(szDestination, szBuf);
      FileMove(szSource, szDestination);
    }

    ++dwIndex;
    BuildNumberedString(dwIndex, szSectionPrefix, "Move File", szSection, sizeof(szSection));
    GetPrivateProfileString(szSection, "Source", "", szBuf, sizeof(szBuf), szFileIniConfig);
  }
  return(FO_SUCCESS);
}

HRESULT FileCopy(LPSTR szFrom, LPSTR szTo, BOOL bFailIfExists, BOOL bDnu)
{
  HANDLE          hFile;
  WIN32_FIND_DATA fdFile;
  char            szFromDir[MAX_BUF];
  char            szFromTemp[MAX_BUF];
  char            szToTemp[MAX_BUF];
  char            szBuf[MAX_BUF];
  BOOL            bFound;

  if(FileExists(szFrom))
  {
    /* The file in the From file path exists */
    ParsePath(szFrom, szBuf, sizeof(szBuf), FALSE, PP_FILENAME_ONLY);
    lstrcpy(szToTemp, szTo);
    AppendBackSlash(szToTemp, sizeof(szToTemp));
    lstrcat(szToTemp, szBuf);
    CopyFile(szFrom, szToTemp, bFailIfExists);
    wsprintf(szBuf, "%s to %s", szFrom, szToTemp);
    UpdateInstallLog(KEY_COPY_FILE, szBuf, bDnu);

    return(FO_SUCCESS);
  }

  /* The file in the From file path does not exist.  Assume to contain wild args and */
  /* proceed acordingly.                                                             */
  ParsePath(szFrom, szFromDir, sizeof(szFromDir), FALSE, PP_PATH_ONLY);

  if((hFile = FindFirstFile(szFrom, &fdFile)) == INVALID_HANDLE_VALUE)
    bFound = FALSE;
  else
    bFound = TRUE;

  while(bFound)
  {
    if((lstrcmpi(fdFile.cFileName, ".") != 0) && (lstrcmpi(fdFile.cFileName, "..") != 0))
    {
      /* create full path string including filename for source */
      lstrcpy(szFromTemp, szFromDir);
      AppendBackSlash(szFromTemp, sizeof(szFromTemp));
      lstrcat(szFromTemp, fdFile.cFileName);

      /* create full path string including filename for destination */
      lstrcpy(szToTemp, szTo);
      AppendBackSlash(szToTemp, sizeof(szToTemp));
      lstrcat(szToTemp, fdFile.cFileName);

      CopyFile(szFromTemp, szToTemp, bFailIfExists);

      /* log the file copy command */
      wsprintf(szBuf, "%s to %s", szFromTemp, szToTemp);
      UpdateInstallLog(KEY_COPY_FILE, szBuf, bDnu);
    }

    bFound = FindNextFile(hFile, &fdFile);
  }

  FindClose(hFile);
  return(FO_SUCCESS);
}

HRESULT FileCopySequential(LPSTR szSourcePath, LPSTR szDestPath, LPSTR szFilename)
{
  int             iFilenameOnlyLen;
  char            szDestFullFilename[MAX_BUF];
  char            szSourceFullFilename[MAX_BUF];
  char            szSearchFilename[MAX_BUF];
  char            szSearchDestFullFilename[MAX_BUF];
  char            szFilenameOnly[MAX_BUF];
  char            szFilenameExtensionOnly[MAX_BUF];
  char            szNumber[MAX_BUF];
  long            dwNumber;
  long            dwMaxNumber;
  LPSTR           szDotPtr;
  HANDLE          hFile;
  WIN32_FIND_DATA fdFile;
  BOOL            bFound;

  lstrcpy(szSourceFullFilename, szSourcePath);
  AppendBackSlash(szSourceFullFilename, sizeof(szSourceFullFilename));
  lstrcat(szSourceFullFilename, szFilename);

  if(FileExists(szSourceFullFilename))
  {
    /* zero out the memory */
    ZeroMemory(szSearchFilename,        sizeof(szSearchFilename));
    ZeroMemory(szFilenameOnly,          sizeof(szFilenameOnly));
    ZeroMemory(szFilenameExtensionOnly, sizeof(szFilenameExtensionOnly));

    /* parse for the filename w/o extention and also only the extension */
    if((szDotPtr = strstr(szFilename, ".")) != NULL)
    {
      *szDotPtr = '\0';
      lstrcpy(szSearchFilename, szFilename);
      lstrcpy(szFilenameOnly, szFilename);
      lstrcpy(szFilenameExtensionOnly, &szDotPtr[1]);
      *szDotPtr = '.';
    }
    else
    {
      lstrcpy(szFilenameOnly, szFilename);
      lstrcpy(szSearchFilename, szFilename);
    }

    /* create the wild arg filename to search for in the szDestPath */
    lstrcat(szSearchFilename, "*.*");
    lstrcpy(szSearchDestFullFilename, szDestPath);
    AppendBackSlash(szSearchDestFullFilename, sizeof(szSearchDestFullFilename));
    lstrcat(szSearchDestFullFilename, szSearchFilename);

    iFilenameOnlyLen = lstrlen(szFilenameOnly);
    dwNumber         = 0;
    dwMaxNumber      = 0;

    /* find the largest numbered filename in the szDestPath */
    if((hFile = FindFirstFile(szSearchDestFullFilename, &fdFile)) == INVALID_HANDLE_VALUE)
      bFound = FALSE;
    else
      bFound = TRUE;

    while(bFound)
    {
       ZeroMemory(szNumber, sizeof(szNumber));
      if((lstrcmpi(fdFile.cFileName, ".") != 0) && (lstrcmpi(fdFile.cFileName, "..") != 0))
      {
        lstrcpy(szNumber, &fdFile.cFileName[iFilenameOnlyLen]);
        dwNumber = atoi(szNumber);
        if(dwNumber > dwMaxNumber)
          dwMaxNumber = dwNumber;
      }

      bFound = FindNextFile(hFile, &fdFile);
    }

    FindClose(hFile);

    lstrcpy(szDestFullFilename, szDestPath);
    AppendBackSlash(szDestFullFilename, sizeof(szDestFullFilename));
    lstrcat(szDestFullFilename, szFilenameOnly);
    itoa(dwMaxNumber + 1, szNumber, 10);
    lstrcat(szDestFullFilename, szNumber);

    if(*szFilenameExtensionOnly != '\0')
    {
      lstrcat(szDestFullFilename, ".");
      lstrcat(szDestFullFilename, szFilenameExtensionOnly);
    }

    CopyFile(szSourceFullFilename, szDestFullFilename, TRUE);
  }

  return(FO_SUCCESS);
}

HRESULT ProcessCopyFile(DWORD dwTiming, char *szSectionPrefix)
{
  DWORD dwIndex;
  char  szBuf[MAX_BUF];
  char  szSection[MAX_BUF];
  char  szSource[MAX_BUF];
  char  szDestination[MAX_BUF];
  BOOL  bFailIfExists;
  BOOL  bDnu;

  dwIndex = 0;
  BuildNumberedString(dwIndex, szSectionPrefix, "Copy File", szSection, sizeof(szSection));
  GetPrivateProfileString(szSection, "Source", "", szBuf, sizeof(szBuf), szFileIniConfig);
  while(*szBuf != '\0')
  {
    if(TimingCheck(dwTiming, szSection, szFileIniConfig))
    {
      DecryptString(szSource, szBuf);
      GetPrivateProfileString(szSection, "Destination", "", szBuf, sizeof(szBuf), szFileIniConfig);
      DecryptString(szDestination, szBuf);

      GetPrivateProfileString(szSection, "Do Not Uninstall", "", szBuf, sizeof(szBuf), szFileIniConfig);
      if(lstrcmpi(szBuf, "TRUE") == 0)
        bDnu = TRUE;
      else
        bDnu = FALSE;

      GetPrivateProfileString(szSection, "Fail If Exists", "", szBuf, sizeof(szBuf), szFileIniConfig);
      if(lstrcmpi(szBuf, "TRUE") == 0)
        bFailIfExists = TRUE;
      else
        bFailIfExists = FALSE;

      FileCopy(szSource, szDestination, bFailIfExists, bDnu);
    }

    ++dwIndex;
    BuildNumberedString(dwIndex, szSectionPrefix, "Copy File", szSection, sizeof(szSection));
    GetPrivateProfileString(szSection, "Source", "", szBuf, sizeof(szBuf), szFileIniConfig);
  }
  return(FO_SUCCESS);
}

HRESULT ProcessCopyFileSequential(DWORD dwTiming, char *szSectionPrefix)
{
  DWORD dwIndex;
  char  szBuf[MAX_BUF];
  char  szSection[MAX_BUF];
  char  szSource[MAX_BUF];
  char  szDestination[MAX_BUF];
  char  szFilename[MAX_BUF];

  dwIndex = 0;
  BuildNumberedString(dwIndex, szSectionPrefix, "Copy File Sequential", szSection, sizeof(szSection));
  GetPrivateProfileString(szSection, "Filename", "", szFilename, sizeof(szFilename), szFileIniConfig);
  while(*szFilename != '\0')
  {
    if(TimingCheck(dwTiming, szSection, szFileIniConfig))
    {
      GetPrivateProfileString(szSection, "Source", "", szBuf, sizeof(szBuf), szFileIniConfig);
      DecryptString(szSource, szBuf);

      GetPrivateProfileString(szSection, "Destination", "", szBuf, sizeof(szBuf), szFileIniConfig);
      DecryptString(szDestination, szBuf);

      FileCopySequential(szSource, szDestination, szFilename);
    }

    ++dwIndex;
    BuildNumberedString(dwIndex, szSectionPrefix, "Copy File Sequential", szSection, sizeof(szSection));
    GetPrivateProfileString(szSection, "Filename", "", szFilename, sizeof(szFilename), szFileIniConfig);
  }
  return(FO_SUCCESS);
}

int RegisterDll32(char *File)
{
  FARPROC   DllReg;
  HINSTANCE hLib;

  if((hLib = LoadLibraryEx(File, NULL, LOAD_WITH_ALTERED_SEARCH_PATH)) != NULL)
  {
    if((DllReg = GetProcAddress(hLib, "DllRegisterServer")) != NULL)
      DllReg();

    FreeLibrary(hLib);
    return(0);
  }

  return(1);
}


HRESULT FileSelfRegister(LPSTR szFilename, LPSTR szDestination)
{
  char            szFullFilenamePath[MAX_BUF];
  DWORD           dwRv;
  HANDLE          hFile;
  WIN32_FIND_DATA fdFile;
  BOOL            bFound;

  lstrcpy(szFullFilenamePath, szDestination);
  AppendBackSlash(szFullFilenamePath, sizeof(szFullFilenamePath));
  lstrcat(szFullFilenamePath, szFilename);

  /* From file path exists and To file path does not exist */
  if(FileExists(szFullFilenamePath))
  {
    RegisterDll32(szFullFilenamePath);
    return(FO_SUCCESS);
  }

  lstrcpy(szFullFilenamePath, szDestination);
  AppendBackSlash(szFullFilenamePath, sizeof(szFullFilenamePath));
  lstrcat(szFullFilenamePath, szFilename);

  if((hFile = FindFirstFile(szFullFilenamePath, &fdFile)) == INVALID_HANDLE_VALUE)
    bFound = FALSE;
  else
    bFound = TRUE;

  while(bFound)
  {
    if((lstrcmpi(fdFile.cFileName, ".") != 0) && (lstrcmpi(fdFile.cFileName, "..") != 0))
    {
      /* create full path string including filename for destination */
      lstrcpy(szFullFilenamePath, szDestination);
      AppendBackSlash(szFullFilenamePath, sizeof(szFullFilenamePath));
      lstrcat(szFullFilenamePath, fdFile.cFileName);

      if((dwRv = FileExists(szFullFilenamePath)) && (dwRv != FILE_ATTRIBUTE_DIRECTORY))
        RegisterDll32(szFullFilenamePath);
    }

    bFound = FindNextFile(hFile, &fdFile);
  }

  FindClose(hFile);
  return(FO_SUCCESS);
}

HRESULT ProcessSelfRegisterFile(DWORD dwTiming, char *szSectionPrefix)
{
  DWORD dwIndex;
  char  szBuf[MAX_BUF];
  char  szSection[MAX_BUF];
  char  szFilename[MAX_BUF];
  char  szDestination[MAX_BUF];

  dwIndex = 0;
  BuildNumberedString(dwIndex, szSectionPrefix, "Self Register File", szSection, sizeof(szSection));
  GetPrivateProfileString(szSection, "Destination", "", szBuf, sizeof(szBuf), szFileIniConfig);
  while(*szBuf != '\0')
  {
    if(TimingCheck(dwTiming, szSection, szFileIniConfig))
    {
      DecryptString(szDestination, szBuf);
      GetPrivateProfileString(szSection, "Filename", "", szFilename, sizeof(szFilename), szFileIniConfig);
      FileSelfRegister(szFilename, szDestination);
    }

    ++dwIndex;
    BuildNumberedString(dwIndex, szSectionPrefix, "Self Register File", szSection, sizeof(szSection));
    GetPrivateProfileString(szSection, "Destination", "", szBuf, sizeof(szBuf), szFileIniConfig);
  }
  return(FO_SUCCESS);
}

void UpdateInstallLog(LPSTR szKey, LPSTR szString, BOOL bDnu)
{
  FILE *fInstallLog;
  char szBuf[MAX_BUF];
  char szFileInstallLog[MAX_BUF];

  if(gbILUseTemp)
  {
    lstrcpy(szFileInstallLog, szTempDir);
    AppendBackSlash(szFileInstallLog, sizeof(szFileInstallLog));
  }
  else
  {
    lstrcpy(szFileInstallLog, sgProduct.szPath);
    AppendBackSlash(szFileInstallLog, sizeof(szFileInstallLog));
    lstrcat(szFileInstallLog, sgProduct.szSubPath);
    AppendBackSlash(szFileInstallLog, sizeof(szFileInstallLog));
  }

  CreateDirectoriesAll(szFileInstallLog, FALSE);
  lstrcat(szFileInstallLog, FILE_INSTALL_LOG);

  if((fInstallLog = fopen(szFileInstallLog, "a+t")) != NULL)
  {
    if(bDnu)
      wsprintf(szBuf, "     ** (*dnu*) %s%s\n", szKey, szString);
    else
      wsprintf(szBuf, "     ** %s%s\n", szKey, szString);

    fwrite(szBuf, sizeof(char), lstrlen(szBuf), fInstallLog);
    fclose(fInstallLog);
  }
}

void UpdateInstallStatusLog(LPSTR szString)
{
  FILE *fInstallLog;
  char szFileInstallStatusLog[MAX_BUF];

  if(gbILUseTemp)
  {
    lstrcpy(szFileInstallStatusLog, szTempDir);
    AppendBackSlash(szFileInstallStatusLog, sizeof(szFileInstallStatusLog));
  }
  else
  {
    lstrcpy(szFileInstallStatusLog, sgProduct.szPath);
    AppendBackSlash(szFileInstallStatusLog, sizeof(szFileInstallStatusLog));
    lstrcat(szFileInstallStatusLog, sgProduct.szSubPath);
    AppendBackSlash(szFileInstallStatusLog, sizeof(szFileInstallStatusLog));
  }

  CreateDirectoriesAll(szFileInstallStatusLog, FALSE);
  lstrcat(szFileInstallStatusLog, FILE_INSTALL_STATUS_LOG);

  if((fInstallLog = fopen(szFileInstallStatusLog, "a+t")) != NULL)
  {
    fwrite(szString, sizeof(char), lstrlen(szString), fInstallLog);
    fclose(fInstallLog);
  }
}

void UpdateJSProxyInfo()
{
  FILE *fJSFile;
  char szBuf[MAX_BUF];
  char szJSFile[MAX_BUF];

  if((*diAdvancedSettings.szProxyServer != '\0') || (*diAdvancedSettings.szProxyPort != '\0'))
  {
    lstrcpy(szJSFile, sgProduct.szPath);
    if(*sgProduct.szSubPath != '\0')
    {
      AppendBackSlash(szJSFile, sizeof(szJSFile));
      lstrcat(szJSFile, sgProduct.szSubPath);
    }
    AppendBackSlash(szJSFile, sizeof(szJSFile));
    lstrcat(szJSFile, "defaults\\pref\\");
    CreateDirectoriesAll(szJSFile, TRUE);
    lstrcat(szJSFile, FILE_ALL_JS);

    if((fJSFile = fopen(szJSFile, "a+t")) != NULL)
    {
      ZeroMemory(szBuf, sizeof(szBuf));
      if(*diAdvancedSettings.szProxyServer != '\0')
      {
        if(diAdditionalOptions.dwUseProtocol == UP_FTP)
          wsprintf(szBuf,
                   "pref(\"network.proxy.ftp\", \"%s\");\n",
                   diAdvancedSettings.szProxyServer);
        else
          wsprintf(szBuf,
                   "pref(\"network.proxy.http\", \"%s\");\n",
                   diAdvancedSettings.szProxyServer);
      }

      if(*diAdvancedSettings.szProxyPort != '\0')
      {
        if(diAdditionalOptions.dwUseProtocol == UP_FTP)
          wsprintf(szBuf,
                   "pref(\"network.proxy.ftp_port\", %s);\n",
                   diAdvancedSettings.szProxyPort);
        else
          wsprintf(szBuf,
                   "pref(\"network.proxy.http_port\", %s);\n",
                   diAdvancedSettings.szProxyPort);
      }

      lstrcat(szBuf, "pref(\"network.proxy.type\", 1);\n");

      fwrite(szBuf, sizeof(char), lstrlen(szBuf), fJSFile);
      fclose(fJSFile);
    }
  }
}

HRESULT CreateDirectoriesAll(char* szPath, BOOL bLogForUninstall)
{
  int     i;
  int     iLen = lstrlen(szPath);
  char    szCreatePath[MAX_BUF];
  HRESULT hrResult = 0;

  ZeroMemory(szCreatePath, MAX_BUF);
  memcpy(szCreatePath, szPath, iLen);
  for(i = 0; i < iLen; i++)
  {
    if((iLen > 1) &&
      ((i != 0) && ((szPath[i] == '\\') || (szPath[i] == '/'))) &&
      (!((szPath[0] == '\\') && (i == 1)) && !((szPath[1] == ':') && (i == 2))))
    {
      szCreatePath[i] = '\0';
      if(FileExists(szCreatePath) == FALSE)
      {
        hrResult = CreateDirectory(szCreatePath, NULL);

        if(bLogForUninstall)
          UpdateInstallLog(KEY_CREATE_FOLDER, szCreatePath, FALSE);
      }
      szCreatePath[i] = szPath[i];
    }
  }
  return(hrResult);
}

HRESULT ProcessCreateDirectory(DWORD dwTiming, char *szSectionPrefix)
{
  DWORD dwIndex;
  char  szBuf[MAX_BUF];
  char  szSection[MAX_BUF];
  char  szDestination[MAX_BUF];

  dwIndex = 0;
  BuildNumberedString(dwIndex, szSectionPrefix, "Create Directory", szSection, sizeof(szSection));
  GetPrivateProfileString(szSection, "Destination", "", szBuf, sizeof(szBuf), szFileIniConfig);
  while(*szBuf != '\0')
  {
    if(TimingCheck(dwTiming, szSection, szFileIniConfig))
    {
      DecryptString(szDestination, szBuf);
      AppendBackSlash(szDestination, sizeof(szDestination));
      CreateDirectoriesAll(szDestination, TRUE);
    }

    ++dwIndex;
    BuildNumberedString(dwIndex, szSectionPrefix, "Create Directory", szSection, sizeof(szSection));
    GetPrivateProfileString(szSection, "Destination", "", szBuf, sizeof(szBuf), szFileIniConfig);
  }
  return(FO_SUCCESS);
}

HRESULT FileDelete(LPSTR szDestination)
{
  HANDLE          hFile;
  WIN32_FIND_DATA fdFile;
  char            szBuf[MAX_BUF];
  char            szPathOnly[MAX_BUF];
  BOOL            bFound;

  if(FileExists(szDestination))
  {
    /* The file in the From file path exists */
    DeleteFile(szDestination);
    return(FO_SUCCESS);
  }

  /* The file in the From file path does not exist.  Assume to contain wild args and */
  /* proceed acordingly.                                                             */
  ParsePath(szDestination, szPathOnly, sizeof(szPathOnly), FALSE, PP_PATH_ONLY);

  if((hFile = FindFirstFile(szDestination, &fdFile)) == INVALID_HANDLE_VALUE)
    bFound = FALSE;
  else
    bFound = TRUE;

  while(bFound)
  {
    if(!(fdFile.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
    {
      lstrcpy(szBuf, szPathOnly);
      AppendBackSlash(szBuf, sizeof(szBuf));
      lstrcat(szBuf, fdFile.cFileName);

      DeleteFile(szBuf);
    }

    bFound = FindNextFile(hFile, &fdFile);
  }

  FindClose(hFile);
  return(FO_SUCCESS);
}

HRESULT ProcessDeleteFile(DWORD dwTiming, char *szSectionPrefix)
{
  DWORD dwIndex;
  char  szBuf[MAX_BUF];
  char  szSection[MAX_BUF];
  char  szDestination[MAX_BUF];

  dwIndex = 0;
  BuildNumberedString(dwIndex, szSectionPrefix, "Delete File", szSection, sizeof(szSection));
  GetPrivateProfileString(szSection, "Destination", "", szBuf, sizeof(szBuf), szFileIniConfig);
  while(*szBuf != '\0')
  {
    if(TimingCheck(dwTiming, szSection, szFileIniConfig))
    {
      DecryptString(szDestination, szBuf);
      FileDelete(szDestination);
    }

    ++dwIndex;
    BuildNumberedString(dwIndex, szSectionPrefix, "Delete File", szSection, sizeof(szSection));
    GetPrivateProfileString(szSection, "Destination", "", szBuf, sizeof(szBuf), szFileIniConfig);
  }
  return(FO_SUCCESS);
}

HRESULT DirectoryRemove(LPSTR szDestination, BOOL bRemoveSubdirs)
{
  HANDLE          hFile;
  WIN32_FIND_DATA fdFile;
  char            szDestTemp[MAX_BUF];
  BOOL            bFound;

  if(!FileExists(szDestination))
    return(FO_SUCCESS);

  if(bRemoveSubdirs == TRUE)
  {
    lstrcpy(szDestTemp, szDestination);
    AppendBackSlash(szDestTemp, sizeof(szDestTemp));
    lstrcat(szDestTemp, "*");

    bFound = TRUE;
    hFile = FindFirstFile(szDestTemp, &fdFile);
    while((hFile != INVALID_HANDLE_VALUE) && (bFound == TRUE))
    {
      if((lstrcmpi(fdFile.cFileName, ".") != 0) && (lstrcmpi(fdFile.cFileName, "..") != 0))
      {
        /* create full path */
        lstrcpy(szDestTemp, szDestination);
        AppendBackSlash(szDestTemp, sizeof(szDestTemp));
        lstrcat(szDestTemp, fdFile.cFileName);

        if(fdFile.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
          DirectoryRemove(szDestTemp, bRemoveSubdirs);
        }
        else
        {
          DeleteFile(szDestTemp);
        }
      }

      bFound = FindNextFile(hFile, &fdFile);
    }

    FindClose(hFile);
  }
  
  RemoveDirectory(szDestination);
  return(FO_SUCCESS);
}

HRESULT ProcessRemoveDirectory(DWORD dwTiming, char *szSectionPrefix)
{
  DWORD dwIndex;
  char  szBuf[MAX_BUF];
  char  szSection[MAX_BUF];
  char  szDestination[MAX_BUF];
  BOOL  bRemoveSubdirs;

  dwIndex = 0;
  BuildNumberedString(dwIndex, szSectionPrefix, "Remove Directory", szSection, sizeof(szSection));
  GetPrivateProfileString(szSection, "Destination", "", szBuf, sizeof(szBuf), szFileIniConfig);
  while(*szBuf != '\0')
  {
    if(TimingCheck(dwTiming, szSection, szFileIniConfig))
    {
      DecryptString(szDestination, szBuf);
      GetPrivateProfileString(szSection, "Remove subdirs", "", szBuf, sizeof(szBuf), szFileIniConfig);
      bRemoveSubdirs = FALSE;
      if(lstrcmpi(szBuf, "TRUE") == 0)
        bRemoveSubdirs = TRUE;

      DirectoryRemove(szDestination, bRemoveSubdirs);
    }

    ++dwIndex;
    BuildNumberedString(dwIndex, szSectionPrefix, "Remove Directory", szSection, sizeof(szSection));
    GetPrivateProfileString(szSection, "Destination", "", szBuf, sizeof(szBuf), szFileIniConfig);
  }
  return(FO_SUCCESS);
}

HRESULT ProcessRunApp(DWORD dwTiming, char *szSectionPrefix)
{
  DWORD dwIndex;
  char  szBuf[MAX_BUF];
  char  szSection[MAX_BUF];
  char  szTarget[MAX_BUF];
  char  szParameters[MAX_BUF];
  char  szWorkingDir[MAX_BUF];
  BOOL  bRunApp;
  BOOL  bWait;

  dwIndex = 0;
  BuildNumberedString(dwIndex, szSectionPrefix, "RunApp", szSection, sizeof(szSection));
  GetPrivateProfileString(szSection, "Target", "", szBuf, sizeof(szBuf), szFileIniConfig);
  while(*szBuf != '\0')
  {
    if(TimingCheck(dwTiming, szSection, szFileIniConfig))
    {
      DecryptString(szTarget, szBuf);
      GetPrivateProfileString(szSection, "Parameters", "", szBuf, sizeof(szBuf), szFileIniConfig);
      DecryptString(szParameters, szBuf);

      // If we are given a criterion to test against, we expect also to be told whether we should run
      //    the app when that criterion is true or when it is false.  If we are not told, we assume that
      //    we are to run the app when the criterion is true.
      bRunApp = TRUE;
      GetPrivateProfileString(szSection, "Criterion ID", "", szBuf, sizeof(szBuf), szFileIniConfig);
      if(lstrcmpi(szBuf, "RecaptureHP") == 0)
      {
        GetPrivateProfileString(szSection, "Run App If Criterion", "", szBuf, sizeof(szBuf), szFileIniConfig);
        if(lstrcmpi(szBuf, "FALSE") == 0)
        {
          if(diAdditionalOptions.bRecaptureHomepage == TRUE)
             bRunApp = FALSE;
        }
        else
        {
          if(diAdditionalOptions.bRecaptureHomepage == FALSE)
             bRunApp = FALSE;
        }
      }

      GetPrivateProfileString(szSection, "WorkingDir", "", szBuf, sizeof(szBuf), szFileIniConfig);
      DecryptString(szWorkingDir, szBuf);

      GetPrivateProfileString(szSection, "Wait", "", szBuf, sizeof(szBuf), szFileIniConfig);
      if(lstrcmpi(szBuf, "FALSE") == 0)
        bWait = FALSE;
      else
        bWait = TRUE;

      if ((bRunApp == TRUE) && FileExists(szTarget))
      {
        if((dwTiming == T_DEPEND_REBOOT) && (NeedReboot() == TRUE))
        {
          lstrcat(szTarget, " ");
          lstrcat(szTarget, szParameters);
          SetWinReg(HKEY_CURRENT_USER,
                    "Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce",
                    TRUE,
                    "Netscape",
                    TRUE,
                    REG_SZ,
                    szTarget,
                    lstrlen(szTarget),
                    FALSE,
                    FALSE);
        }
        else
        {
          GetPrivateProfileString(szSection, "Message", "", szBuf, sizeof(szBuf), szFileIniConfig);
          if ( szBuf[0] != '\0' )
            ShowMessage(szBuf, TRUE);  
          WinSpawn(szTarget, szParameters, szWorkingDir, SW_SHOWNORMAL, bWait);
          if ( szBuf[0] != '\0' )
            ShowMessage(szBuf, FALSE);  
        }
      }
    }

    ++dwIndex;
    BuildNumberedString(dwIndex, szSectionPrefix, "RunApp", szSection, sizeof(szSection));
    GetPrivateProfileString(szSection, "Target", "", szBuf, sizeof(szBuf), szFileIniConfig);
  }
  return(FO_SUCCESS);
}

DWORD ParseRestrictedAccessKey(LPSTR szKey)
{
  DWORD dwKey;

  if(lstrcmpi(szKey, "ONLY_RESTRICTED") == 0)
    dwKey = RA_ONLY_RESTRICTED;
  else if(lstrcmpi(szKey, "ONLY_NONRESTRICTED") == 0)
    dwKey = RA_ONLY_NONRESTRICTED;
  else
    dwKey = RA_IGNORE;

  return(dwKey);
}

HKEY ParseRootKey(LPSTR szRootKey)
{
  HKEY hkRootKey;

  if(lstrcmpi(szRootKey, "HKEY_CURRENT_CONFIG") == 0)
    hkRootKey = HKEY_CURRENT_CONFIG;
  else if(lstrcmpi(szRootKey, "HKEY_CURRENT_USER") == 0)
    hkRootKey = HKEY_CURRENT_USER;
  else if(lstrcmpi(szRootKey, "HKEY_LOCAL_MACHINE") == 0)
    hkRootKey = HKEY_LOCAL_MACHINE;
  else if(lstrcmpi(szRootKey, "HKEY_USERS") == 0)
    hkRootKey = HKEY_USERS;
  else if(lstrcmpi(szRootKey, "HKEY_PERFORMANCE_DATA") == 0)
    hkRootKey = HKEY_PERFORMANCE_DATA;
  else if(lstrcmpi(szRootKey, "HKEY_DYN_DATA") == 0)
    hkRootKey = HKEY_DYN_DATA;
  else /* HKEY_CLASSES_ROOT */
    hkRootKey = HKEY_CLASSES_ROOT;

  return(hkRootKey);
}

char *ParseRootKeyString(HKEY hkKey, LPSTR szRootKey, DWORD dwRootKeyBufSize)
{
  if(!szRootKey)
    return(NULL);

  ZeroMemory(szRootKey, dwRootKeyBufSize);
  if((hkKey == HKEY_CURRENT_CONFIG) &&
    ((long)dwRootKeyBufSize > lstrlen("HKEY_CURRENT_CONFIG")))
    lstrcpy(szRootKey, "HKEY_CURRENT_CONFIG");
  else if((hkKey == HKEY_CURRENT_USER) &&
         ((long)dwRootKeyBufSize > lstrlen("HKEY_CURRENT_USER")))
    lstrcpy(szRootKey, "HKEY_CURRENT_USER");
  else if((hkKey == HKEY_LOCAL_MACHINE) &&
         ((long)dwRootKeyBufSize > lstrlen("HKEY_LOCAL_MACHINE")))
    lstrcpy(szRootKey, "HKEY_LOCAL_MACHINE");
  else if((hkKey == HKEY_USERS) &&
         ((long)dwRootKeyBufSize > lstrlen("HKEY_USERS")))
    lstrcpy(szRootKey, "HKEY_USERS");
  else if((hkKey == HKEY_PERFORMANCE_DATA) &&
         ((long)dwRootKeyBufSize > lstrlen("HKEY_PERFORMANCE_DATA")))
    lstrcpy(szRootKey, "HKEY_PERFORMANCE_DATA");
  else if((hkKey == HKEY_DYN_DATA) &&
         ((long)dwRootKeyBufSize > lstrlen("HKEY_DYN_DATA")))
    lstrcpy(szRootKey, "HKEY_DYN_DATA");
  else if((long)dwRootKeyBufSize > lstrlen("HKEY_CLASSES_ROOT"))
    lstrcpy(szRootKey, "HKEY_CLASSES_ROOT");

  return(szRootKey);
}

BOOL ParseRegType(LPSTR szType, DWORD *dwType)
{
  BOOL bSZ;

  if(lstrcmpi(szType, "REG_SZ") == 0)
  {
    /* Unicode NULL terminated string */
    *dwType = REG_SZ;
    bSZ     = TRUE;
  }
  else if(lstrcmpi(szType, "REG_EXPAND_SZ") == 0)
  {
    /* Unicode NULL terminated string
     * (with environment variable references) */
    *dwType = REG_EXPAND_SZ;
    bSZ     = TRUE;
  }
  else if(lstrcmpi(szType, "REG_BINARY") == 0)
  {
    /* Free form binary */
    *dwType = REG_BINARY;
    bSZ     = FALSE;
  }
  else if(lstrcmpi(szType, "REG_DWORD") == 0)
  {
    /* 32bit number */
    *dwType = REG_DWORD;
    bSZ     = FALSE;
  }
  else if(lstrcmpi(szType, "REG_DWORD_LITTLE_ENDIAN") == 0)
  {
    /* 32bit number
     * (same as REG_DWORD) */
    *dwType = REG_DWORD_LITTLE_ENDIAN;
    bSZ     = FALSE;
  }
  else if(lstrcmpi(szType, "REG_DWORD_BIG_ENDIAN") == 0)
  {
    /* 32bit number */
    *dwType = REG_DWORD_BIG_ENDIAN;
    bSZ     = FALSE;
  }
  else if(lstrcmpi(szType, "REG_LINK") == 0)
  {
    /* Symbolic link (unicode) */
    *dwType = REG_LINK;
    bSZ     = TRUE;
  }
  else if(lstrcmpi(szType, "REG_MULTI_SZ") == 0)
  {
    /* Multiple Unicode strings */
    *dwType = REG_MULTI_SZ;
    bSZ     = TRUE;
  }
  else /* Default is REG_NONE */
  {
    /* no value type */
    *dwType = REG_NONE;
    bSZ     = TRUE;
  }

  return(bSZ);
}

BOOL WinRegKeyExists(HKEY hkRootKey, LPSTR szKey)
{
  HKEY  hkResult;
  DWORD dwErr;
  BOOL  bKeyExists = FALSE;

  if((dwErr = RegOpenKeyEx(hkRootKey, szKey, 0, KEY_READ, &hkResult)) == ERROR_SUCCESS)
  {
    bKeyExists = TRUE;
    RegCloseKey(hkResult);
  }

  return(bKeyExists);
}

BOOL WinRegNameExists(HKEY hkRootKey, LPSTR szKey, LPSTR szName)
{
  HKEY  hkResult;
  DWORD dwErr;
  DWORD dwSize;
  char  szBuf[MAX_BUF];
  BOOL  bNameExists = FALSE;

  ZeroMemory(szBuf, sizeof(szBuf));
  if((dwErr = RegOpenKeyEx(hkRootKey, szKey, 0, KEY_READ, &hkResult)) == ERROR_SUCCESS)
  {
    dwSize = sizeof(szBuf);
    dwErr  = RegQueryValueEx(hkResult, szName, 0, NULL, szBuf, &dwSize);

    if((*szBuf != '\0') && (dwErr == ERROR_SUCCESS))
      bNameExists = TRUE;

    RegCloseKey(hkResult);
  }

  return(bNameExists);
}

void DeleteWinRegKey(HKEY hkRootKey, LPSTR szKey, BOOL bAbsoluteDelete)
{
  HKEY      hkResult;
  DWORD     dwErr;
  DWORD     dwTotalSubKeys;
  DWORD     dwTotalValues;
  DWORD     dwSubKeySize;
  FILETIME  ftLastWriteFileTime;
  char      szSubKey[MAX_BUF_TINY];
  char      szNewKey[MAX_BUF];
  long      lRv;

  dwErr = RegOpenKeyEx(hkRootKey, szKey, 0, KEY_QUERY_VALUE, &hkResult);
  if(dwErr == ERROR_SUCCESS)
  {
    dwTotalSubKeys = 0;
    dwTotalValues  = 0;
    RegQueryInfoKey(hkResult, NULL, NULL, NULL, &dwTotalSubKeys, NULL, NULL, &dwTotalValues, NULL, NULL, NULL, NULL);
    RegCloseKey(hkResult);

    if(((dwTotalSubKeys == 0) && (dwTotalValues == 0)) || bAbsoluteDelete)
    {
      if(dwTotalSubKeys && bAbsoluteDelete)
      {
        do
        {
          dwSubKeySize = sizeof(szSubKey);
          lRv = 0;
          if(RegOpenKeyEx(hkRootKey, szKey, 0, KEY_READ, &hkResult) == ERROR_SUCCESS)
          {
            if((lRv = RegEnumKeyEx(hkResult, 0, szSubKey, &dwSubKeySize, NULL, NULL, NULL, &ftLastWriteFileTime)) == ERROR_SUCCESS)
            {
              RegCloseKey(hkResult);
              lstrcpy(szNewKey, szKey);
              AppendBackSlash(szNewKey, sizeof(szNewKey));
              lstrcat(szNewKey, szSubKey);
              DeleteWinRegKey(hkRootKey, szNewKey, bAbsoluteDelete);
            }
            else
              RegCloseKey(hkResult);
          }
        } while(lRv != ERROR_NO_MORE_ITEMS);
      }

      dwErr = RegDeleteKey(hkRootKey, szKey);
    }
  }
}

void DeleteWinRegValue(HKEY hkRootKey, LPSTR szKey, LPSTR szName)
{
  HKEY    hkResult;
  DWORD   dwErr;

  dwErr = RegOpenKeyEx(hkRootKey, szKey, 0, KEY_WRITE, &hkResult);
  if(dwErr == ERROR_SUCCESS)
  {
    if(*szName == '\0')
      dwErr = RegDeleteValue(hkResult, NULL);
    else
      dwErr = RegDeleteValue(hkResult, szName);

    RegCloseKey(hkResult);
  }
}

DWORD GetWinReg(HKEY hkRootKey, LPSTR szKey, LPSTR szName, LPSTR szReturnValue, DWORD dwReturnValueSize)
{
  HKEY  hkResult;
  DWORD dwErr;
  DWORD dwSize;
  DWORD dwType;
  char  szBuf[MAX_BUF];

  ZeroMemory(szBuf, sizeof(szBuf));
  ZeroMemory(szReturnValue, dwReturnValueSize);

  if((dwErr = RegOpenKeyEx(hkRootKey, szKey, 0, KEY_READ, &hkResult)) == ERROR_SUCCESS)
  {
    dwSize = sizeof(szBuf);
    dwErr  = RegQueryValueEx(hkResult, szName, 0, &dwType, szBuf, &dwSize);

    if((dwType == REG_MULTI_SZ) && (*szBuf != '\0'))
    {
      DWORD dwCpSize;

      dwCpSize = dwReturnValueSize < dwSize ? (dwReturnValueSize - 1) : dwSize;
      memcpy(szReturnValue, szBuf, dwCpSize);
    }
    else if((*szBuf != '\0') && (dwErr == ERROR_SUCCESS))
      ExpandEnvironmentStrings(szBuf, szReturnValue, dwReturnValueSize);
    else
      *szReturnValue = '\0';

    RegCloseKey(hkResult);
  }

  return(dwType);
}

void SetWinReg(HKEY hkRootKey,
               LPSTR szKey,
               BOOL bOverwriteKey,
               LPSTR szName,
               BOOL bOverwriteName,
               DWORD dwType,
               LPBYTE lpbData,
               DWORD dwSize,
               BOOL bLogForUninstall,
               BOOL bDnu)
{
  HKEY    hkResult;
  DWORD   dwErr;
  DWORD   dwDisp;
  BOOL    bKeyExists;
  BOOL    bNameExists;
  char    szBuf[MAX_BUF];
  char    szRootKey[MAX_BUF_TINY];


  bKeyExists  = WinRegKeyExists(hkRootKey, szKey);
  bNameExists = WinRegNameExists(hkRootKey, szKey, szName);
  dwErr       = RegOpenKeyEx(hkRootKey, szKey, 0, KEY_WRITE, &hkResult);

  if(dwErr != ERROR_SUCCESS)
  {
    dwErr = RegCreateKeyEx(hkRootKey, szKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hkResult, &dwDisp);
    /* log the win reg command */
    if(bLogForUninstall &&
       ParseRootKeyString(hkRootKey, szRootKey, sizeof(szRootKey)))
    {
      wsprintf(szBuf, "%s\\%s []", szRootKey, szKey);
      UpdateInstallLog(KEY_CREATE_REG_KEY, szBuf, bDnu);
    }
  }

  if(dwErr == ERROR_SUCCESS)
  {
    if((bNameExists == FALSE) ||
      ((bNameExists == TRUE) && (bOverwriteName == TRUE)))
    {
      dwErr = RegSetValueEx(hkResult, szName, 0, dwType, lpbData, dwSize);
      /* log the win reg command */
      if(bLogForUninstall &&
         ParseRootKeyString(hkRootKey, szRootKey, sizeof(szRootKey)))
      {
        if(ParseRegType(szBuf, &dwType))
        {
          wsprintf(szBuf, "%s\\%s [%s]", szRootKey, szKey, szName);
          UpdateInstallLog(KEY_STORE_REG_STRING, szBuf, bDnu);
        }
        else
        {
          wsprintf(szBuf, "%s\\%s [%s]", szRootKey, szKey, szName);
          UpdateInstallLog(KEY_STORE_REG_NUMBER, szBuf, bDnu);
        }
      }
    }

    RegCloseKey(hkResult);
  }
}

/* Name: AppendWinRegString
 *
 * Arguments:
 *
 * HKEY hkRootKey -- root key, e.g., HKEY_LOCAL_MACHINE
 * LPSTR szKey -- subkey
 * LPSTR szName -- value name
 * DWORD dwType -- value type, should be REG_SZ
 * LPBYTE lpbData -- value data
 * BYTE delimiter -- e.g., ':'. If 0, then don't apply delimiter
 * DWORD dwSize -- size of the value data
 * BOOL bLogForUninstall -- if true, update install log
 * BOOL bDnu -- what to update the install log with
 *
 * Description:
 *
 * This function should be called to append a string (REG_SZ) to the
 * string already stored in the specified key. If the key does not
 * exist, then simply store the key (ignoring the delimiter). If the
 * key does exist, read the current value, append the delimiter (if
 * not zero), and append the data passed in.
 *
 * Return Value: void
 *
 * Original Code: Clone of SetWinReg(), syd@netscape.com 6/11/2001
 *
 */

void AppendWinReg(HKEY hkRootKey,
               LPSTR szKey,
               LPSTR szName,
               DWORD dwType,
               LPBYTE lpbData,
               BYTE delimiter,
               DWORD dwSize,
               BOOL bLogForUninstall,
               BOOL bDnu)
{
  HKEY    hkResult;
  DWORD   dwErr;
  DWORD   dwDisp;
  BOOL    bKeyExists;
  BOOL    bNameExists;
  char    szBuf[MAX_BUF];
  char    szRootKey[MAX_BUF_TINY]; 

  bKeyExists  = WinRegKeyExists(hkRootKey, szKey);
  bNameExists = WinRegNameExists(hkRootKey, szKey, szName);
  dwErr       = RegOpenKeyEx(hkRootKey, szKey, 0, KEY_WRITE, &hkResult);

  if (dwType != REG_SZ) // this function is void. How do we pass errors to caller?
      return;

  if(dwErr != ERROR_SUCCESS)
  {
    dwErr = RegCreateKeyEx(hkRootKey, szKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hkResult, &dwDisp);
    /* log the win reg command */
    if(bLogForUninstall &&
       ParseRootKeyString(hkRootKey, szRootKey, sizeof(szRootKey)))
    {
      wsprintf(szBuf, "%s\\%s []", szRootKey, szKey);
      UpdateInstallLog(KEY_CREATE_REG_KEY, szBuf, bDnu);
    }
  }

  if(dwErr == ERROR_SUCCESS)
  {
    if((bNameExists == FALSE))
    {
      /* first time, so just write it, ignoring the delimiter */

      dwErr = RegSetValueEx(hkResult, szName, 0, dwType, lpbData, dwSize);
      /* log the win reg command */
      if(bLogForUninstall &&
         ParseRootKeyString(hkRootKey, szRootKey, sizeof(szRootKey)))
      {
        if(ParseRegType(szBuf, &dwType))
        {
          wsprintf(szBuf, "%s\\%s [%s]", szRootKey, szKey, szName);
          UpdateInstallLog(KEY_STORE_REG_STRING, szBuf, bDnu);
        }
        else
        {
          wsprintf(szBuf, "%s\\%s [%s]", szRootKey, szKey, szName);
          UpdateInstallLog(KEY_STORE_REG_NUMBER, szBuf, bDnu);
        }
      }
    } else {
      /* already exists, so read the prrevious value, append the delimiter if 
         specified, append the new value, and rewrite the key */
      
      GetWinReg(hkRootKey, szKey, szName, szBuf, sizeof(szBuf));  // func is void, assume success
      if ( delimiter != 0 ) {
          char delim[ 2 ];
          delim[0] = delimiter;
          delim[1] = '\0';
          strcat( szBuf, delim );
      }
      strcat( szBuf, lpbData );
      RegCloseKey(hkResult);
      SetWinReg(hkRootKey, szKey, TRUE, szName, TRUE, dwType, szBuf, strlen( szBuf ) + 1, bLogForUninstall, bDnu);
      return;
    }

    RegCloseKey(hkResult);
  }
}

HRESULT ProcessWinReg(DWORD dwTiming, char *szSectionPrefix)
{
  char    szBuf[MAX_BUF];
  char    szKey[MAX_BUF];
  char    szName[MAX_BUF];
  char    szValue[MAX_BUF];
  char    szDecrypt[MAX_BUF];
  char    szOverwriteKey[MAX_BUF];
  char    szOverwriteName[MAX_BUF];
  char    szSection[MAX_BUF];
  HKEY    hRootKey;
  BOOL    bDnu;
  BOOL    bOverwriteKey;
  BOOL    bOverwriteName;
  BOOL    bOSDetected;
  DWORD   dwIndex;
  DWORD   dwType;
  DWORD   dwSize;
  __int64 iiNum;

  dwIndex = 0;
  BuildNumberedString(dwIndex, szSectionPrefix, "Windows Registry", szSection, sizeof(szSection));
  GetPrivateProfileString(szSection, "Root Key", "", szBuf, sizeof(szBuf), szFileIniConfig);
  while(*szBuf != '\0')
  {
    if(TimingCheck(dwTiming, szSection, szFileIniConfig))
    {
      hRootKey = ParseRootKey(szBuf);

      GetPrivateProfileString(szSection, "Key",                 "", szBuf,           sizeof(szBuf),          szFileIniConfig);
      GetPrivateProfileString(szSection, "Decrypt Key",         "", szDecrypt,       sizeof(szDecrypt),      szFileIniConfig);
      GetPrivateProfileString(szSection, "Overwrite Key",       "", szOverwriteKey,  sizeof(szOverwriteKey), szFileIniConfig);
      ZeroMemory(szKey, sizeof(szKey));
      if(lstrcmpi(szDecrypt, "TRUE") == 0)
        DecryptString(szKey, szBuf);
      else
        lstrcpy(szKey, szBuf);

      if(lstrcmpi(szOverwriteKey, "FALSE") == 0)
        bOverwriteKey = FALSE;
      else
        bOverwriteKey = TRUE;

      GetPrivateProfileString(szSection, "Name",                "", szBuf,           sizeof(szBuf),           szFileIniConfig);
      GetPrivateProfileString(szSection, "Decrypt Name",        "", szDecrypt,       sizeof(szDecrypt),       szFileIniConfig);
      GetPrivateProfileString(szSection, "Overwrite Name",      "", szOverwriteName, sizeof(szOverwriteName), szFileIniConfig);
      ZeroMemory(szName, sizeof(szName));
      if(lstrcmpi(szDecrypt, "TRUE") == 0)
        DecryptString(szName, szBuf);
      else
        lstrcpy(szName, szBuf);

      if(lstrcmpi(szOverwriteName, "FALSE") == 0)
        bOverwriteName = FALSE;
      else
        bOverwriteName = TRUE;

      GetPrivateProfileString(szSection, "Name Value",          "", szBuf,           sizeof(szBuf), szFileIniConfig);
      GetPrivateProfileString(szSection, "Decrypt Name Value",  "", szDecrypt,       sizeof(szDecrypt), szFileIniConfig);
      ZeroMemory(szValue, sizeof(szValue));
      if(lstrcmpi(szDecrypt, "TRUE") == 0)
        DecryptString(szValue, szBuf);
      else
        lstrcpy(szValue, szBuf);

      GetPrivateProfileString(szSection, "Size",                "", szBuf,           sizeof(szBuf), szFileIniConfig);
      if(*szBuf != '\0')
        dwSize = atoi(szBuf);
      else
        dwSize = 0;

      GetPrivateProfileString(szSection,
                              "Do Not Uninstall",
                              "",
                              szBuf,
                              sizeof(szBuf),
                              szFileIniConfig);
      if(lstrcmpi(szBuf, "TRUE") == 0)
        bDnu = TRUE;
      else
        bDnu = FALSE;

      /* Read the OS key to see if there are restrictions on which OS to
       * the Windows registry key for */
      GetPrivateProfileString(szSection,
                              "OS",
                              "",
                              szBuf,
                              sizeof(szBuf),
                              szFileIniConfig);
      /* If there is no OS key value set, then assume all OS is valid.
       * If there are any, then compare against the global OS value to
       * make sure there's a match. */
      bOSDetected = TRUE;
      if((*szBuf != '\0') &&
        ((gSystemInfo.dwOSType & ParseOSType(szBuf)) == 0))
        bOSDetected = FALSE;

      if(bOSDetected)
      {
        ZeroMemory(szBuf, sizeof(szBuf));
        GetPrivateProfileString(szSection,
                                "Type",
                                "",
                                szBuf,
                                sizeof(szBuf),
                                szFileIniConfig);
      if(ParseRegType(szBuf, &dwType))
      {
        /* create/set windows registry key here (string value)! */
        SetWinReg(hRootKey, szKey, bOverwriteKey, szName, bOverwriteName,
                  dwType, (CONST LPBYTE)szValue, lstrlen(szValue), TRUE, bDnu);
      }
      else
      {
        iiNum = _atoi64(szValue);
        /* create/set windows registry key here (binary/dword value)! */
        SetWinReg(hRootKey, szKey, bOverwriteKey, szName, bOverwriteName,
                  dwType, (CONST LPBYTE)&iiNum, dwSize, TRUE, bDnu);
        }
      }
    }

    ++dwIndex;
    BuildNumberedString(dwIndex, szSectionPrefix, "Windows Registry", szSection, sizeof(szSection));
    GetPrivateProfileString(szSection, "Root Key", "", szBuf, sizeof(szBuf), szFileIniConfig);
  }
  return(FO_SUCCESS);
}

HRESULT ProcessProgramFolder(DWORD dwTiming, char *szSectionPrefix)
{
  DWORD dwIndex0;
  DWORD dwIndex1;
  DWORD dwIconId;
  DWORD dwRestrictedAccess;
  char  szIndex1[MAX_BUF];
  char  szBuf[MAX_BUF];
  char  szSection0[MAX_BUF];
  char  szSection1[MAX_BUF];
  char  szProgramFolder[MAX_BUF];
  char  szFile[MAX_BUF];
  char  szArguments[MAX_BUF];
  char  szWorkingDir[MAX_BUF];
  char  szDescription[MAX_BUF];
  char  szIconPath[MAX_BUF];

  dwIndex0 = 0;
  BuildNumberedString(dwIndex0, szSectionPrefix, "Program Folder", szSection0, sizeof(szSection0));
  GetPrivateProfileString(szSection0, "Program Folder", "", szBuf, sizeof(szBuf), szFileIniConfig);
  while(*szBuf != '\0')
  {
    if(TimingCheck(dwTiming, szSection0, szFileIniConfig))
    {
      DecryptString(szProgramFolder, szBuf);

      dwIndex1 = 0;
      itoa(dwIndex1, szIndex1, 10);
      lstrcpy(szSection1, szSection0);
      lstrcat(szSection1, "-Shortcut");
      lstrcat(szSection1, szIndex1);
      GetPrivateProfileString(szSection1, "File", "", szBuf, sizeof(szBuf), szFileIniConfig);
      while(*szBuf != '\0')
      {
        DecryptString(szFile, szBuf);
        GetPrivateProfileString(szSection1, "Arguments",    "", szBuf, sizeof(szBuf), szFileIniConfig);
        DecryptString(szArguments, szBuf);
        GetPrivateProfileString(szSection1, "Working Dir",  "", szBuf, sizeof(szBuf), szFileIniConfig);
        DecryptString(szWorkingDir, szBuf);
        GetPrivateProfileString(szSection1, "Description",  "", szBuf, sizeof(szBuf), szFileIniConfig);
        DecryptString(szDescription, szBuf);
        GetPrivateProfileString(szSection1, "Icon Path",    "", szBuf, sizeof(szBuf), szFileIniConfig);
        DecryptString(szIconPath, szBuf);
        GetPrivateProfileString(szSection1, "Icon Id",      "", szBuf, sizeof(szBuf), szFileIniConfig);
        if(*szBuf != '\0')
          dwIconId = atol(szBuf);
        else
          dwIconId = 0;

        GetPrivateProfileString(szSection1, "Restricted Access",    "", szBuf, sizeof(szBuf), szFileIniConfig);
        dwRestrictedAccess = ParseRestrictedAccessKey(szBuf);
        if((dwRestrictedAccess == RA_IGNORE) ||
          ((dwRestrictedAccess == RA_ONLY_RESTRICTED) && gbRestrictedAccess) ||
          ((dwRestrictedAccess == RA_ONLY_NONRESTRICTED) && !gbRestrictedAccess))
        {
          CreateALink(szFile, szProgramFolder, szDescription, szWorkingDir, szArguments, szIconPath, dwIconId);
          lstrcpy(szBuf, szProgramFolder);
          AppendBackSlash(szBuf, sizeof(szBuf));
          lstrcat(szBuf, szDescription);
          UpdateInstallLog(KEY_WINDOWS_SHORTCUT, szBuf, FALSE);
        }

        ++dwIndex1;
        itoa(dwIndex1, szIndex1, 10);
        lstrcpy(szSection1, szSection0);
        lstrcat(szSection1, "-Shortcut");
        lstrcat(szSection1, szIndex1);
        GetPrivateProfileString(szSection1, "File", "", szBuf, sizeof(szBuf), szFileIniConfig);
      }
    }

    ++dwIndex0;
    BuildNumberedString(dwIndex0, szSectionPrefix, "Program Folder", szSection0, sizeof(szSection0));
    GetPrivateProfileString(szSection0, "Program Folder", "", szBuf, sizeof(szBuf), szFileIniConfig);
  }
  return(FO_SUCCESS);
}

HRESULT ProcessProgramFolderShowCmd()
{
  DWORD dwIndex0;
  int   iShowFolder;
  char  szBuf[MAX_BUF];
  char  szSection0[MAX_BUF];
  char  szProgramFolder[MAX_BUF];

  dwIndex0 = 0;
  BuildNumberedString(dwIndex0, NULL, "Program Folder", szSection0, sizeof(szSection0));
  GetPrivateProfileString(szSection0, "Program Folder", "", szBuf, sizeof(szBuf), szFileIniConfig);
  while(*szBuf != '\0')
  {
    DecryptString(szProgramFolder, szBuf);
    GetPrivateProfileString(szSection0, "Show Folder", "", szBuf, sizeof(szBuf), szFileIniConfig);

    if(strcmpi(szBuf, "HIDE") == 0)
      iShowFolder = SW_HIDE;
    else if(strcmpi(szBuf, "MAXIMIZE") == 0)
      iShowFolder = SW_MAXIMIZE;
    else if(strcmpi(szBuf, "MINIMIZE") == 0)
      iShowFolder = SW_MINIMIZE;
    else if(strcmpi(szBuf, "RESTORE") == 0)
      iShowFolder = SW_RESTORE;
    else if(strcmpi(szBuf, "SHOW") == 0)
      iShowFolder = SW_SHOW;
    else if(strcmpi(szBuf, "SHOWMAXIMIZED") == 0)
      iShowFolder = SW_SHOWMAXIMIZED;
    else if(strcmpi(szBuf, "SHOWMINIMIZED") == 0)
      iShowFolder = SW_SHOWMINIMIZED;
    else if(strcmpi(szBuf, "SHOWMINNOACTIVE") == 0)
      iShowFolder = SW_SHOWMINNOACTIVE;
    else if(strcmpi(szBuf, "SHOWNA") == 0)
      iShowFolder = SW_SHOWNA;
    else if(strcmpi(szBuf, "SHOWNOACTIVATE") == 0)
      iShowFolder = SW_SHOWNOACTIVATE;
    else if(strcmpi(szBuf, "SHOWNORMAL") == 0)
      iShowFolder = SW_SHOWNORMAL;

    if(iShowFolder != SW_HIDE)
      if(sgProduct.dwMode != SILENT)
        WinSpawn(szProgramFolder, NULL, NULL, iShowFolder, TRUE);

    ++dwIndex0;
    BuildNumberedString(dwIndex0, NULL, "Program Folder", szSection0, sizeof(szSection0));
    GetPrivateProfileString(szSection0, "Program Folder", "", szBuf, sizeof(szBuf), szFileIniConfig);
  }
  return(FO_SUCCESS);
}

