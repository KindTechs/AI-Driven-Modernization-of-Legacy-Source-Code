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

#include "msgCore.h"
#include "nsMsgSearchDBView.h"
#include "nsIMsgHdr.h"
#include "nsIMsgThread.h"
#include "nsQuickSort.h"
#include "nsIDBFolderInfo.h"
#include "nsXPIDLString.h"
#include "nsMsgBaseCID.h"
#include "nsIMsgCopyService.h"
#include "nsICopyMsgStreamListener.h"

static NS_DEFINE_CID(kMsgCopyServiceCID,    NS_MSGCOPYSERVICE_CID);
static NS_DEFINE_CID(kCopyMessageStreamListenerCID, NS_COPYMESSAGESTREAMLISTENER_CID);

nsMsgSearchDBView::nsMsgSearchDBView()
{
  /* member initializers and constructor code */
  // don't try to display messages for the search pane.
  mSuppressMsgDisplay = PR_TRUE;
}

nsMsgSearchDBView::~nsMsgSearchDBView()
{	
 /* destructor code */
}

NS_IMPL_ISUPPORTS_INHERITED2(nsMsgSearchDBView, nsMsgDBView, nsIMsgDBView, nsIMsgCopyServiceListener)

NS_IMETHODIMP nsMsgSearchDBView::Open(nsIMsgFolder *folder, nsMsgViewSortTypeValue sortType, nsMsgViewSortOrderValue sortOrder, nsMsgViewFlagsTypeValue viewFlags, PRInt32 *pCount)
{
	nsresult rv;
    m_folders = do_CreateInstance(NS_SUPPORTSARRAY_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    m_dbToUseList = do_CreateInstance(NS_SUPPORTSARRAY_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

	rv = nsMsgDBView::Open(folder, sortType, sortOrder, viewFlags, pCount);
    NS_ENSURE_SUCCESS(rv, rv);

	if (pCount)
		*pCount = 0;
	return rv;
}

NS_IMETHODIMP nsMsgSearchDBView::Close()
{
    nsresult rv;
  	PRUint32 count;

    NS_ASSERTION(m_dbToUseList, "no db used");
    //if (!m_dbToUseList) return NS_ERROR_FAILURE;

	rv = m_dbToUseList->Count(&count);
    if (NS_FAILED(rv)) return rv;
	
	for(PRUint32 i = 0; i < count; i++)
	{
		((nsIMsgDatabase*)m_dbToUseList->ElementAt(i))->RemoveListener(this);
	}
	return NS_OK;
}

NS_IMETHODIMP nsMsgSearchDBView::GetCellText(PRInt32 aRow, const PRUnichar * aColID, nsAString& aValue)
{
  if (aColID[0] == 'l' && aColID[1] == 'o') // location, need to check for "lo" not just "l" to avoid "label" column
  {
    // XXX fix me by converting Fetch* to take an nsAString& parameter
    nsXPIDLString valueText;
    nsresult rv = FetchLocation(aRow, getter_Copies(valueText));
    aValue.Assign(valueText);
    return rv;
  }
  else
    return nsMsgDBView::GetCellText(aRow, aColID, aValue);
}

nsresult nsMsgSearchDBView::FetchLocation(PRInt32 aRow, PRUnichar ** aLocationString)
{
  nsCOMPtr <nsIMsgFolder> folder;
  nsresult rv = GetFolderForViewIndex(aRow, getter_AddRefs(folder));
  NS_ENSURE_SUCCESS(rv,rv);

  rv = folder->GetPrettiestName(aLocationString);
  NS_ENSURE_SUCCESS(rv,rv);
  return NS_OK;
}

nsresult nsMsgSearchDBView::OnNewHeader(nsMsgKey newKey, nsMsgKey aParentKey, PRBool /*ensureListed*/)
{
   return NS_OK;
}

nsresult nsMsgSearchDBView::GetMsgHdrForViewIndex(nsMsgViewIndex index, nsIMsgDBHdr **msgHdr)
{
  nsresult rv;
  nsCOMPtr <nsISupports> supports = getter_AddRefs(m_folders->ElementAt(index));
	if(supports)
	{
		nsCOMPtr<nsIMsgFolder> folder = do_QueryInterface(supports);
    if (folder)
    {
      nsCOMPtr <nsIMsgDatabase> db;
      rv = folder->GetMsgDatabase(mMsgWindow, getter_AddRefs(db));
      NS_ENSURE_SUCCESS(rv, rv);
      if (db)
        rv = db->GetMsgHdrForKey(m_keys.GetAt(index), msgHdr);
    }
  }
  return rv;
}

NS_IMETHODIMP nsMsgSearchDBView::GetFolderForViewIndex(nsMsgViewIndex index, nsIMsgFolder **aFolder)
{
  return m_folders->QueryElementAt(index, NS_GET_IID(nsIMsgFolder), (void **) aFolder);
}

nsresult nsMsgSearchDBView::GetDBForViewIndex(nsMsgViewIndex index, nsIMsgDatabase **db)
{
  nsCOMPtr <nsIMsgFolder> aFolder;
  GetFolderForViewIndex(index, getter_AddRefs(aFolder));
  if (aFolder)
    return aFolder->GetMsgDatabase(nsnull, db);
  else
    return NS_MSG_INVALID_DBVIEW_INDEX;
}

NS_IMETHODIMP
nsMsgSearchDBView::OnSearchHit(nsIMsgDBHdr* aMsgHdr, nsIMsgFolder *folder)
{
  NS_ENSURE_ARG(aMsgHdr);
  NS_ENSURE_ARG(folder);
  nsresult rv = NS_OK;

  nsCOMPtr <nsISupports> supports = do_QueryInterface(folder);
  m_folders->AppendElement(supports);
  nsMsgKey msgKey;
  PRUint32 msgFlags;
  aMsgHdr->GetMessageKey(&msgKey);
  aMsgHdr->GetFlags(&msgFlags);
  m_keys.Add(msgKey);
  m_levels.Add(0);
  m_flags.Add(msgFlags);

  // this needs to be called after we add the key, since RowCountChanged() will call our GetRowCount()
  if (mTree)
    mTree->RowCountChanged(GetSize() - 1, 1);

  return rv;
}

NS_IMETHODIMP
nsMsgSearchDBView::OnSearchDone(nsresult status)
{
  //we want to set imap delete model once the search is over because setting next
  //message after deletion will happen before deleting the message and search scope
  //can change with every search.
  mDeleteModel = nsMsgImapDeleteModels::MoveToTrash;  //set to default in case it is non-imap folder
  nsCOMPtr <nsISupports> curSupports = getter_AddRefs(m_folders->ElementAt(0));
  nsCOMPtr <nsIMsgFolder> curFolder = do_QueryInterface(curSupports);
  if (curFolder)   
    GetImapDeleteModel(curFolder);
  return NS_OK;
}


// for now also acts as a way of resetting the search datasource
NS_IMETHODIMP
nsMsgSearchDBView::OnNewSearch()
{
  PRInt32 oldSize = GetSize();

  m_folders->Clear();
  m_keys.RemoveAll();
  m_levels.RemoveAll();
  m_flags.RemoveAll();

  // needs to happen after we remove the keys, since RowCountChanged() will call our GetRowCount()
  if (mTree) 
    mTree->RowCountChanged(0, -oldSize);

//    mSearchResults->Clear();
    return NS_OK;
}

nsresult nsMsgSearchDBView::GetFolders(nsISupportsArray **aFolders)
{
	NS_ENSURE_ARG_POINTER(aFolders);
	*aFolders = m_folders;
	NS_IF_ADDREF(*aFolders);
	
	return NS_OK;
}

NS_IMETHODIMP 
nsMsgSearchDBView::DoCommandWithFolder(nsMsgViewCommandTypeValue command, nsIMsgFolder *destFolder)
{
    mCommand = command;
    mDestFolder = getter_AddRefs(destFolder);

    return nsMsgDBView::DoCommandWithFolder(command, destFolder);
}

NS_IMETHODIMP nsMsgSearchDBView::DoCommand(nsMsgViewCommandTypeValue command)
{
    mCommand = command;
    return nsMsgDBView::DoCommand(command);
}

// This method just removes the specified line from the view. It does
// NOT delete it from the database.
nsresult nsMsgSearchDBView::RemoveByIndex(nsMsgViewIndex index)
{
    if (!IsValidIndex(index))
        return NS_MSG_INVALID_DBVIEW_INDEX;

    m_folders->RemoveElementAt(index);
    
    return nsMsgDBView::RemoveByIndex(index);
}

nsresult nsMsgSearchDBView::DeleteMessages(nsIMsgWindow *window, nsMsgViewIndex *indices, PRInt32 numIndices, PRBool deleteStorage)
{
    nsresult rv;
    InitializeGlobalsForDeleteAndFile(indices, numIndices);
    if (mDeleteModel != nsMsgImapDeleteModels::MoveToTrash)
      deleteStorage = PR_TRUE;
    if (!deleteStorage)
      rv = ProcessRequestsInOneFolder(window);
    else
      rv = ProcessRequestsInAllFolders(window);
    return rv;
}

nsresult 
nsMsgSearchDBView::CopyMessages(nsIMsgWindow *window, nsMsgViewIndex *indices, PRInt32 numIndices, PRBool isMove, nsIMsgFolder *destFolder)
{
    nsresult rv;
    InitializeGlobalsForDeleteAndFile(indices, numIndices);

    rv = ProcessRequestsInOneFolder(window);

    return rv;
}

nsresult
nsMsgSearchDBView::InitializeGlobalsForDeleteAndFile(nsMsgViewIndex *indices, PRInt32 numIndices)
{
  nsresult rv = NS_OK; 
  mCurIndex = 0;
                    //initialize and clear from the last usage
  if (!m_uniqueFolders)
  {
    m_uniqueFolders = do_CreateInstance(NS_SUPPORTSARRAY_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv); 
  }
  else
    m_uniqueFolders->Clear();
  
  if (!m_hdrsForEachFolder)
  {
    m_hdrsForEachFolder = do_CreateInstance(NS_SUPPORTSARRAY_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv,rv);
  }
  else
    m_hdrsForEachFolder->Clear();

  PRUint32 count=0;
  rv = m_dbToUseList->Count(&count);
  NS_ENSURE_SUCCESS(rv,rv);
  for(PRUint32 j = 0; j < count; j++)
    ((nsIMsgDatabase*)m_dbToUseList->ElementAt(j))->RemoveListener(this);
  m_dbToUseList->Clear();

  //Build unique folder list based on headers selected by the user
  for (nsMsgViewIndex i = 0; i < (nsMsgViewIndex) numIndices; i++)
  {
     nsCOMPtr <nsISupports> curSupports = getter_AddRefs(m_folders->ElementAt(indices[i]));
     if ( m_uniqueFolders->IndexOf(curSupports) < 0)
     {
        m_uniqueFolders->AppendElement(curSupports); 
        nsCOMPtr<nsIMsgFolder> curFolder = do_QueryInterface(curSupports);
        nsCOMPtr <nsIMsgDatabase> dbToUse;
        if (curFolder)
        {
           nsCOMPtr <nsIDBFolderInfo> folderInfo;
           rv = curFolder->GetDBFolderInfoAndDB(getter_AddRefs(folderInfo), getter_AddRefs(dbToUse));
           NS_ENSURE_SUCCESS(rv,rv);
           dbToUse->AddListener(this);

           nsCOMPtr <nsISupports> tmpSupports = do_QueryInterface(dbToUse);
           m_dbToUseList->AppendElement(tmpSupports);
        }
     }
  }

  PRUint32 numFolders =0; 
  rv = m_uniqueFolders->Count(&numFolders);   //group the headers selected by each folder 
  NS_ENSURE_SUCCESS(rv,rv);
  for (PRUint32 folderIndex=0; folderIndex < numFolders; folderIndex++)
  {
     nsCOMPtr <nsISupports> curSupports = getter_AddRefs(m_uniqueFolders->ElementAt(folderIndex));
     nsCOMPtr <nsIMsgFolder> curFolder = do_QueryInterface(curSupports, &rv);
     nsCOMPtr <nsISupportsArray> msgHdrsForOneFolder;
     NS_NewISupportsArray(getter_AddRefs(msgHdrsForOneFolder));
     for (nsMsgViewIndex i = 0; i < (nsMsgViewIndex) numIndices; i++) 
     {
       nsCOMPtr <nsISupports> folderSupports = getter_AddRefs(m_folders->ElementAt(indices[i]));
       nsCOMPtr <nsIMsgFolder> msgFolder = do_QueryInterface(folderSupports, &rv);
       if (NS_SUCCEEDED(rv) && msgFolder && msgFolder == curFolder) 
       {
          nsCOMPtr<nsIMsgDBHdr> msgHdr; 
          rv = GetMsgHdrForViewIndex(indices[i],getter_AddRefs(msgHdr));
          NS_ENSURE_SUCCESS(rv,rv);
          nsCOMPtr <nsISupports> hdrSupports = do_QueryInterface(msgHdr);
          msgHdrsForOneFolder->AppendElement(hdrSupports);
       }
     }
     nsCOMPtr <nsISupports> supports = do_QueryInterface(msgHdrsForOneFolder, &rv);
     if (NS_SUCCEEDED(rv) && supports)
       m_hdrsForEachFolder->AppendElement(supports);
  }
  return rv;


}

// nsIMsgCopyServiceListener methods

NS_IMETHODIMP
nsMsgSearchDBView::OnStartCopy()
{
  return NS_OK;
}

NS_IMETHODIMP
nsMsgSearchDBView::OnProgress(PRUint32 aProgress, PRUint32 aProgressMax)
{
  return NS_OK;
}

// believe it or not, these next two are msgcopyservice listener methods!
NS_IMETHODIMP
nsMsgSearchDBView::SetMessageKey(PRUint32 aMessageKey)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMsgSearchDBView::GetMessageId(nsCString* aMessageId)
{
  return NS_OK;
}
  
NS_IMETHODIMP
nsMsgSearchDBView::OnStopCopy(nsresult aStatus)
{
    nsresult rv = NS_OK;

    if (NS_SUCCEEDED(aStatus))
    {
        mCurIndex++;
        PRUint32 numFolders =0;
        rv = m_uniqueFolders->Count(&numFolders);
        if ( mCurIndex < numFolders)
          ProcessRequestsInOneFolder(mMsgWindow);
    }

    return rv;
}

// end nsIMsgCopyServiceListener methods

nsresult nsMsgSearchDBView::ProcessRequestsInOneFolder(nsIMsgWindow *window)
{
    nsresult rv = NS_OK;

    nsCOMPtr <nsISupports> curSupports = getter_AddRefs(m_uniqueFolders->ElementAt(mCurIndex));
    NS_ASSERTION(curSupports, "curSupports is null");
    nsCOMPtr<nsIMsgFolder> curFolder = do_QueryInterface(curSupports);
    nsCOMPtr <nsISupports> msgSupports = getter_AddRefs(m_hdrsForEachFolder->ElementAt(mCurIndex));
    NS_ASSERTION(msgSupports, "msgSupports is null");
    nsCOMPtr <nsISupportsArray> messageArray = do_QueryInterface(msgSupports);

    // called for delete with trash, copy and move
    if (mCommand == nsMsgViewCommandType::deleteMsg)
        curFolder->DeleteMessages(messageArray, window, PR_FALSE /* delete storage */, PR_FALSE /* is move*/, this, PR_FALSE /*allowUndo*/);
    else 
    {
      NS_ASSERTION(!(curFolder == mDestFolder), "The source folder and the destination folder are the same");
      if (NS_SUCCEEDED(rv) && curFolder != mDestFolder)
      {
         if (mCommand == nsMsgViewCommandType::moveMessages)
           mDestFolder->CopyMessages(curFolder, messageArray, PR_TRUE /* isMove */,window, this, PR_FALSE /*is_Folder*/, PR_FALSE /*allowUndo*/);
         else if (mCommand == nsMsgViewCommandType::copyMessages)
           mDestFolder->CopyMessages(curFolder, messageArray, PR_FALSE /* isMove */, window, this, PR_FALSE /*is_Folder*/, PR_FALSE /*allowUndo*/);
      }
    }
    return rv;
}

nsresult nsMsgSearchDBView::ProcessRequestsInAllFolders(nsIMsgWindow *window)
{
    PRUint32 numFolders =0;
    nsresult rv = m_uniqueFolders->Count(&numFolders);
    NS_ENSURE_SUCCESS(rv,rv);
    for (PRUint32 folderIndex=0; folderIndex < numFolders; folderIndex++)
    {
       nsCOMPtr <nsISupports> curSupports = getter_AddRefs(m_uniqueFolders->ElementAt(folderIndex));
       NS_ASSERTION (curSupports, "curSupports is null");
       nsCOMPtr<nsIMsgFolder> curFolder = do_QueryInterface(curSupports);

       nsCOMPtr <nsISupports> msgSupports = getter_AddRefs(m_hdrsForEachFolder->ElementAt(folderIndex));
       NS_ASSERTION(msgSupports, "msgSupports is null");
       nsCOMPtr <nsISupportsArray> messageArray = do_QueryInterface(msgSupports);

       curFolder->DeleteMessages(messageArray, window, PR_TRUE /* delete storage */, PR_FALSE /* is move*/, nsnull/*copyServListener*/, PR_FALSE /*allowUndo*/ );
    }

    return NS_OK;
}

NS_IMETHODIMP nsMsgSearchDBView::Sort(nsMsgViewSortTypeValue sortType, nsMsgViewSortOrderValue sortOrder)
{
    nsresult rv;
    PRInt32 rowCountBeforeSort = GetSize();

    if (!rowCountBeforeSort)
        return NS_OK;

    nsMsgKeyArray preservedSelection;
    SaveAndClearSelection(&preservedSelection);

    rv = nsMsgDBView::Sort(sortType,sortOrder);

    // the sort may have changed the number of rows
    // before we restore the selection, tell the tree
    // do this before we call restore selection
    // this is safe when there is no selection. 
    rv = AdjustRowCount(rowCountBeforeSort, GetSize());

    RestoreSelection(&preservedSelection);
    if (mTree) mTree->Invalidate();

    NS_ENSURE_SUCCESS(rv,rv);
    return rv;
}


// if nothing selected, return an NS_ERROR
NS_IMETHODIMP
nsMsgSearchDBView::GetHdrForFirstSelectedMessage(nsIMsgDBHdr **hdr)
{
  NS_ENSURE_ARG_POINTER(hdr);
  PRInt32 index;

  nsresult rv = mTreeSelection->GetCurrentIndex(&index);
  NS_ENSURE_SUCCESS(rv,rv);

  rv = GetMsgHdrForViewIndex(index, hdr);
  NS_ENSURE_SUCCESS(rv,rv);
  return NS_OK;
}
