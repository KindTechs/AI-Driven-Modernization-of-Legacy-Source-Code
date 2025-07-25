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
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

// this file implements the nsAddrDatabase interface using the MDB Interface.

#include "nsAddrDatabase.h"
#include "nsIEnumerator.h"
#include "nsFileStream.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsRDFCID.h"
#include "nsUnicharUtils.h"
#include "nsAbBaseCID.h"
#include "nsIAbCard.h"
#include "nsIAbMDBCard.h"
#include "nsIAbDirectory.h"
#include "nsIAbMDBDirectory.h"
#include "nsIAddrBookSession.h"

#include "nsIServiceManager.h"
#include "nsRDFCID.h"

#include "nsIPref.h"
#include "nsMorkCID.h"
#include "nsIMdbFactoryFactory.h"
#include "nsXPIDLString.h"
#include "nsIRDFService.h"
#include "nsIProxyObjectManager.h"
#include "nsProxiedService.h"
#include "prprf.h"

#define ID_PAB_TABLE            1

const PRInt32 kAddressBookDBVersion = 1;

const char *kPabTableKind = "ns:addrbk:db:table:kind:pab";

const char *kCardRowScope = "ns:addrbk:db:row:scope:card:all";
const char *kListRowScope = "ns:addrbk:db:row:scope:list:all";
const char *kDataRowScope = "ns:addrbk:db:row:scope:data:all";

#define DATAROW_ROWID 1

#define COLUMN_STR_MAX 16

const char *kRecordKeyColumn = "RecordKey";
const char *kLowerPriEmailColumn = "LowercasePrimaryEmail";

const char *kLastRecordKeyColumn = "LastRecordKey";

const char *kMailListTotalLists = "ListTotalLists";    // total number of mail list in a mailing list
const char *kLowerListNameColumn = "LowercaseListName";

struct mdbOid gAddressBookTableOID;

static const char *kMailListAddressFormat = "Address%d";

static NS_DEFINE_CID(kCMorkFactory, NS_MORK_CID);

nsAddrDatabase::nsAddrDatabase()
    : m_mdbEnv(nsnull), m_mdbStore(nsnull),
      m_mdbPabTable(nsnull), 
      m_dbName(""), m_mdbTokensInitialized(PR_FALSE), 
      m_ChangeListeners(nsnull),
      m_PabTableKind(0),
      m_MailListTableKind(0),
      m_CardRowScopeToken(0),
      m_FirstNameColumnToken(0),
      m_LastNameColumnToken(0),
      m_DisplayNameColumnToken(0),
      m_NickNameColumnToken(0),
      m_PriEmailColumnToken(0),
      m_2ndEmailColumnToken(0),
      m_WorkPhoneColumnToken(0),
      m_HomePhoneColumnToken(0),
      m_FaxColumnToken(0),
      m_PagerColumnToken(0),
      m_CellularColumnToken(0),
      m_HomeAddressColumnToken(0),
      m_HomeAddress2ColumnToken(0),
      m_HomeCityColumnToken(0),
      m_HomeStateColumnToken(0),
      m_HomeZipCodeColumnToken(0),
      m_HomeCountryColumnToken(0),
      m_WorkAddressColumnToken(0),
      m_WorkAddress2ColumnToken(0),
      m_WorkCityColumnToken(0),
      m_WorkStateColumnToken(0),
      m_WorkZipCodeColumnToken(0),
      m_WorkCountryColumnToken(0),
      m_WebPage1ColumnToken(0),
      m_WebPage2ColumnToken(0),
      m_BirthYearColumnToken(0),
      m_BirthMonthColumnToken(0),
      m_BirthDayColumnToken(0),
      m_Custom1ColumnToken(0),
      m_Custom2ColumnToken(0),
      m_Custom3ColumnToken(0),
      m_Custom4ColumnToken(0),
      m_NotesColumnToken(0),
      m_LastModDateColumnToken(0),
      m_MailFormatColumnToken(0),
      m_AddressCharSetColumnToken(0),
      m_LastRecordKey(0),
      m_dbDirectory(nsnull)
{
   NS_INIT_ISUPPORTS();
}

nsAddrDatabase::~nsAddrDatabase()
{
    Close(PR_FALSE);    // better have already been closed.
    if (m_ChangeListeners)
    {
        // better not be any listeners, because we're going away.
        NS_ASSERTION(m_ChangeListeners->Count() == 0, "shouldn't have any listeners");
        delete m_ChangeListeners;
    }

    RemoveFromCache(this);
}

NS_IMPL_THREADSAFE_ADDREF(nsAddrDatabase)

NS_IMETHODIMP_(nsrefcnt) nsAddrDatabase::Release(void)                    
{                                                      
  // XXX FIX THIS
  NS_PRECONDITION(0 != mRefCnt, "dup release");        
  nsrefcnt count = PR_AtomicDecrement((PRInt32 *)&mRefCnt);
  NS_LOG_RELEASE(this, count,"nsAddrDatabase"); 
  if (count == 0)    // OK, the cache is no longer holding onto this, so we really want to delete it, 
  {                // after removing it from the cache.
    mRefCnt = 1; /* stabilize */
    RemoveFromCache(this);
    // clean up after ourself!
    if (m_mdbPabTable)
      m_mdbPabTable->Release();
    if (m_mdbStore)
      m_mdbStore->Release();
    if (m_mdbEnv)
    {
      m_mdbEnv->Release();
      m_mdbEnv = nsnull;
    }
    NS_DELETEXPCOM(this);                              
    return 0;                                          
  }
  return count;                                      
}

NS_IMETHODIMP nsAddrDatabase::QueryInterface(REFNSIID aIID, void** aResult)
{   
    if (aResult == NULL)  
        return NS_ERROR_NULL_POINTER;  

    if (aIID.Equals(NS_GET_IID(nsIAddrDatabase)) ||
        aIID.Equals(NS_GET_IID(nsIAddrDBAnnouncer)) ||
        aIID.Equals(NS_GET_IID(nsISupports))) {
        *aResult = NS_STATIC_CAST(nsIAddrDatabase*, this);   
        NS_ADDREF_THIS();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}   

NS_IMETHODIMP nsAddrDatabase::AddListener(nsIAddrDBListener *listener)
{
  if (!listener)
    return NS_ERROR_NULL_POINTER;
  if (!m_ChangeListeners) 
    {
    m_ChangeListeners = new nsVoidArray();
    if (!m_ChangeListeners) 
            return NS_ERROR_OUT_OF_MEMORY;
  }
    PRInt32 count = m_ChangeListeners->Count();
    PRInt32 i;
    for (i = 0; i < count; i++)
    {
        nsIAddrDBListener *dbListener = (nsIAddrDBListener *)m_ChangeListeners->ElementAt(i);
        if (dbListener == listener)
            return NS_OK;
    }
    return m_ChangeListeners->AppendElement(listener);
}

NS_IMETHODIMP nsAddrDatabase::RemoveListener(nsIAddrDBListener *listener)
{
    if (!m_ChangeListeners) 
        return NS_OK;

    PRInt32 count = m_ChangeListeners->Count();
    PRInt32 i;
    for (i = 0; i < count; i++)
    {
        nsIAddrDBListener *dbListener = (nsIAddrDBListener *)m_ChangeListeners->ElementAt(i);
        if (dbListener == listener)
        {
            m_ChangeListeners->RemoveElementAt(i);
            return NS_OK;
        }
    }
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsAddrDatabase::NotifyCardAttribChange(PRUint32 abCode, nsIAddrDBListener *instigator)
{
  if (!m_ChangeListeners)
      return NS_OK;
    PRInt32 i;
    for (i = 0; i < m_ChangeListeners->Count(); i++)
    {
        nsIAddrDBListener *changeListener =
            (nsIAddrDBListener *) m_ChangeListeners->ElementAt(i);

        nsresult rv = changeListener->OnCardAttribChange(abCode, instigator); 
    NS_ENSURE_SUCCESS(rv, rv);
    }
    return NS_OK;
}

NS_IMETHODIMP nsAddrDatabase::NotifyCardEntryChange(PRUint32 abCode, nsIAbCard *card, nsIAddrDBListener *instigator)
{
  if (!m_ChangeListeners)
      return NS_OK;
  PRInt32 i;
  PRInt32 count = m_ChangeListeners->Count();
  for (i = count - 1; i >= 0; i--)
  {
    nsIAddrDBListener *changeListener = 
            (nsIAddrDBListener *) m_ChangeListeners->ElementAt(i);

    if (changeListener)
    {
      nsresult rv = changeListener->OnCardEntryChange(abCode, card, instigator); 
      NS_ENSURE_SUCCESS(rv, rv);
    }
    else
      m_ChangeListeners->RemoveElementAt(i); //remove null ptr in the list
    // since we're looping down, this is ok
  }
  return NS_OK;
}

nsresult nsAddrDatabase::NotifyListEntryChange(PRUint32 abCode, nsIAbDirectory *dir, nsIAddrDBListener *instigator)
{
  if (!m_ChangeListeners)
        return NS_OK;

    PRInt32 i;
    PRInt32 count = m_ChangeListeners->Count();
    for (i = 0; i < count; i++)
    {
        nsIAddrDBListener *changeListener = 
            (nsIAddrDBListener *) m_ChangeListeners->ElementAt(i);

        nsresult rv = changeListener->OnListEntryChange(abCode, dir, instigator); 
    NS_ENSURE_SUCCESS(rv, rv);
    }
    return NS_OK;
}


NS_IMETHODIMP nsAddrDatabase::NotifyAnnouncerGoingAway(void)
{
  if (!m_ChangeListeners)
    return NS_OK;
    // run loop backwards because listeners remove themselves from the list 
    // on this notification
    PRInt32 i;
    for (i = m_ChangeListeners->Count() - 1; i >= 0 ; i--)
    {
        nsIAddrDBListener *changeListener =
            (nsIAddrDBListener *) m_ChangeListeners->ElementAt(i);

        nsresult rv = changeListener->OnAnnouncerGoingAway(this); 
    NS_ENSURE_SUCCESS(rv, rv);
    }
  return NS_OK;
}



nsVoidArray *nsAddrDatabase::m_dbCache = NULL;

//----------------------------------------------------------------------
// GetDBCache
//----------------------------------------------------------------------

nsVoidArray/*<nsAddrDatabase>*/ *
nsAddrDatabase::GetDBCache()
{
    if (!m_dbCache)
        m_dbCache = new nsVoidArray();

    return m_dbCache;
    
}

void
nsAddrDatabase::CleanupCache()
{
    if (m_dbCache) // clean up memory leak
    {
        PRInt32 i;
        for (i = 0; i < GetDBCache()->Count(); i++)
        {
            nsAddrDatabase* pAddrDB = NS_STATIC_CAST(nsAddrDatabase*, GetDBCache()->ElementAt(i));
            if (pAddrDB)
            {
                pAddrDB->ForceClosed();
                i--;    // back up array index, since closing removes db from cache.
            }
        }
//        NS_ASSERTION(GetNumInCache() == 0, "some msg dbs left open");    // better not be any open db's.
        delete m_dbCache;
    }
    m_dbCache = nsnull; // Need to reset to NULL since it's a
              // static global ptr and maybe referenced 
              // again in other places.
}

//----------------------------------------------------------------------
// FindInCache - this addrefs the db it finds.
//----------------------------------------------------------------------
nsAddrDatabase* nsAddrDatabase::FindInCache(nsFileSpec *dbName)
{
    PRInt32 i;
    for (i = 0; i < GetDBCache()->Count(); i++)
    {
        nsAddrDatabase* pAddrDB = NS_STATIC_CAST(nsAddrDatabase*, GetDBCache()->ElementAt(i));
        if (pAddrDB->MatchDbName(dbName))
        {
            NS_ADDREF(pAddrDB);
            return pAddrDB;
        }
    }
    return nsnull;
}

//----------------------------------------------------------------------
// FindInCache
//----------------------------------------------------------------------
PRInt32 nsAddrDatabase::FindInCache(nsAddrDatabase* pAddrDB)
{
    PRInt32 i;
    for (i = 0; i < GetDBCache()->Count(); i++)
    {
        if (GetDBCache()->ElementAt(i) == pAddrDB)
        {
            return(i);
        }
    }
    return(-1);
}

PRBool nsAddrDatabase::MatchDbName(nsFileSpec* dbName)    // returns PR_TRUE if they match
{
    return (m_dbName == (*dbName)); 
}

//----------------------------------------------------------------------
// RemoveFromCache
//----------------------------------------------------------------------
void nsAddrDatabase::RemoveFromCache(nsAddrDatabase* pAddrDB)
{
    PRInt32 i = FindInCache(pAddrDB);
    if (i != -1)
    {
        GetDBCache()->RemoveElementAt(i);
    }
}

nsIMdbFactory *nsAddrDatabase::GetMDBFactory()
{
    static nsIMdbFactory *gMDBFactory = nsnull;
    if (!gMDBFactory)
    {
        nsresult rv;
    nsCOMPtr <nsIMdbFactoryFactory> factoryfactory = do_CreateInstance(kCMorkFactory, &rv);

        if (NS_SUCCEEDED(rv) && factoryfactory)
          rv = factoryfactory->GetMdbFactory(&gMDBFactory);
    }
    return gMDBFactory;
}

#ifdef XP_PC
// this code is stolen from nsFileSpecWin. Since MDB requires a native path, for 
// the time being, we'll just take the Unix/Canonical form and munge it
void nsAddrDatabase::UnixToNative(char*& ioPath)
// This just does string manipulation.  It doesn't check reality, or canonify, or
// anything
//----------------------------------------------------------------------------------------
{
    // Allow for relative or absolute.  We can do this in place, because the
    // native path is never longer.
    
    if (!ioPath || !*ioPath)
        return;
        
    char* src = ioPath;
    if (*ioPath == '/')
    {
      // Strip initial slash for an absolute path
      src++;
    }
        
    // Convert the vertical slash to a colon
    char* cp = src + 1;
    
    // If it was an absolute path, check for the drive letter
    if (*ioPath == '/' && strstr(cp, "|/") == cp)
    *cp = ':';
    
    // Convert '/' to '\'.
    while (*++cp)
    {
      if (*cp == '/')
        *cp = '\\';
    }

    if (*ioPath == '/') {
    for (cp = ioPath; *cp; ++cp)
      *cp = *(cp + 1);
  }
}
#endif /* XP_PC */

#ifdef XP_MAC
// this code is stolen from nsFileSpecMac. Since MDB requires a native path, for 
// the time being, we'll just take the Unix/Canonical form and munge it
void nsAddrDatabase::UnixToNative(char*& ioPath)
// This just does string manipulation.  It doesn't check reality, or canonify, or
// anything
//----------------------------------------------------------------------------------------
{
    // Relying on the fact that the unix path is always longer than the mac path:
    size_t len = strlen(ioPath);
    char* result = new char[len + 2]; // ... but allow for the initial colon in a partial name
    if (result)
    {
        char* dst = result;
        const char* src = ioPath;
        if (*src == '/')             // * full path
            src++;
        else if (PL_strchr(src, '/'))    // * partial path, and not just a leaf name
            *dst++ = ':';
        strcpy(dst, src);

        while ( *dst != 0)
        {
            if (*dst == '/')
                *dst++ = ':';
            else
                *dst++;
        }
        nsCRT::free(ioPath);
        ioPath = result;
    }
}
#endif /* XP_MAC */

/* caller need to delete *aDbPath */
NS_IMETHODIMP nsAddrDatabase::GetDbPath(nsFileSpec * *aDbPath)
{
    if (aDbPath)
    {
        nsFileSpec* pFilePath = new nsFileSpec();
        *pFilePath = m_dbName;
        *aDbPath = pFilePath;
        return NS_OK;
    }
    else
        return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP nsAddrDatabase::SetDbPath(nsFileSpec * aDbPath)
{
    m_dbName = (*aDbPath);
    return NS_OK;
}

NS_IMETHODIMP nsAddrDatabase::Open
(nsFileSpec* pabName, PRBool create, nsIAddrDatabase** pAddrDB, PRBool upgrading)
{
    nsAddrDatabase            *pAddressBookDB;
    nsresult                err = NS_OK;

    *pAddrDB = nsnull;

    pAddressBookDB = (nsAddrDatabase *) FindInCache(pabName);
    if (pAddressBookDB) {
        *pAddrDB = pAddressBookDB;
        return(NS_OK);
    }

    pAddressBookDB = new nsAddrDatabase();
    if (!pAddressBookDB) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    NS_ADDREF(pAddressBookDB);

    err = pAddressBookDB->OpenMDB(pabName, create);
    if (NS_SUCCEEDED(err)) 
    {
        pAddressBookDB->SetDbPath(pabName);
        GetDBCache()->AppendElement(pAddressBookDB);
        *pAddrDB = pAddressBookDB;
    }
    else 
    {
        *pAddrDB = nsnull;
        NS_IF_RELEASE(pAddressBookDB);
        pAddressBookDB = nsnull;
    }

    return err;
}

// Open the MDB database synchronously. If successful, this routine
// will set up the m_mdbStore and m_mdbEnv of the database object 
// so other database calls can work.
NS_IMETHODIMP nsAddrDatabase::OpenMDB(nsFileSpec *dbName, PRBool create)
{
  nsresult ret = NS_OK;
  nsIMdbFactory *myMDBFactory = GetMDBFactory();
  if (myMDBFactory)
  {
    ret = myMDBFactory->MakeEnv(NULL, &m_mdbEnv);
    if (NS_SUCCEEDED(ret))
    {
      nsIMdbThumb *thumb = nsnull;
      const char *pFilename = dbName->GetCString(); /* do not free */
      char    *nativeFileName = nsCRT::strdup(pFilename);
      nsIMdbHeap* dbHeap = 0;
      mdb_bool dbFrozen = mdbBool_kFalse; // not readonly, we want modifiable
      
      if (!nativeFileName)
        return NS_ERROR_OUT_OF_MEMORY;
      
      if (m_mdbEnv)
        m_mdbEnv->SetAutoClear(PR_TRUE);
      
#if defined(XP_PC) || defined(XP_MAC)
      UnixToNative(nativeFileName);
#endif
      if (!dbName->Exists()) 
        ret = NS_ERROR_FAILURE;  // check: use the right error code later
      else
      {
        mdbOpenPolicy inOpenPolicy;
        mdb_bool    canOpen;
        mdbYarn        outFormatVersion;
        nsIMdbFile* oldFile = 0;
        
        ret = myMDBFactory->OpenOldFile(m_mdbEnv, dbHeap, nativeFileName,
          dbFrozen, &oldFile);
        if ( oldFile )
        {
          if ( ret == NS_OK )
          {
            ret = myMDBFactory->CanOpenFilePort(m_mdbEnv, oldFile, // the file to investigate
              &canOpen, &outFormatVersion);
            if (ret == 0 && canOpen)
            {
              inOpenPolicy.mOpenPolicy_ScopePlan.mScopeStringSet_Count = 0;
              inOpenPolicy.mOpenPolicy_MinMemory = 0;
              inOpenPolicy.mOpenPolicy_MaxLazy = 0;
              
              ret = myMDBFactory->OpenFileStore(m_mdbEnv, dbHeap,
                oldFile, &inOpenPolicy, &thumb); 
            }
            else
              ret = NS_ERROR_FAILURE;  //check: use the right error code
          }
          NS_RELEASE(oldFile); // always release our file ref, store has own
        }
      }
      
      nsCRT::free(nativeFileName);
      
      if (NS_SUCCEEDED(ret) && thumb)
      {
        mdb_count outTotal;    // total somethings to do in operation
        mdb_count outCurrent;  // subportion of total completed so far
        mdb_bool outDone = PR_FALSE;      // is operation finished?
        mdb_bool outBroken;     // is operation irreparably dead and broken?
        do
        {
          ret = thumb->DoMore(m_mdbEnv, &outTotal, &outCurrent, &outDone, &outBroken);
          if (ret != 0)
          { 
            outDone = PR_TRUE;
            break;
          }
        }
        while (NS_SUCCEEDED(ret) && !outBroken && !outDone);
        if (NS_SUCCEEDED(ret) && outDone)
        {
          ret = myMDBFactory->ThumbToOpenStore(m_mdbEnv, thumb, &m_mdbStore);
          if (ret == NS_OK && m_mdbStore)
          {
            ret = InitExistingDB();
            create = PR_FALSE;
          }
        }
      }
      else if (create)    // ### need error code saying why open file store failed
      {
        nsIMdbFile* newFile = 0;
        ret = myMDBFactory->CreateNewFile(m_mdbEnv, dbHeap, dbName->GetCString(), &newFile);
        if ( newFile )
        {
          if (ret == NS_OK)
          {
            mdbOpenPolicy inOpenPolicy;
            
            inOpenPolicy.mOpenPolicy_ScopePlan.mScopeStringSet_Count = 0;
            inOpenPolicy.mOpenPolicy_MinMemory = 0;
            inOpenPolicy.mOpenPolicy_MaxLazy = 0;
            
            ret = myMDBFactory->CreateNewFileStore(m_mdbEnv, dbHeap,
              newFile, &inOpenPolicy, &m_mdbStore);
            if (ret == NS_OK)
              ret = InitNewDB();
          }
          NS_RELEASE(newFile); // always release our file ref, store has own
        }
      }
      NS_IF_RELEASE(thumb);
    }
  }
  //Convert the DB error to a valid nsresult error.
  if (ret == 1)
    ret = NS_ERROR_FAILURE;
  return ret;
}

NS_IMETHODIMP nsAddrDatabase::CloseMDB(PRBool commit)
{
    if (commit)
        Commit(nsAddrDBCommitType::kSessionCommit);
//???    RemoveFromCache(this);  // if we've closed it, better not leave it in the cache.
    return NS_OK;
}

// force the database to close - this'll flush out anybody holding onto
// a database without having a listener!
// This is evil in the com world, but there are times we need to delete the file.
NS_IMETHODIMP nsAddrDatabase::ForceClosed()
{
    nsresult    err = NS_OK;
    nsCOMPtr<nsIAddrDatabase> aDb(do_QueryInterface(this, &err));

    // make sure someone has a reference so object won't get deleted out from under us.
    AddRef();    
    NotifyAnnouncerGoingAway();
    // OK, remove from cache first and close the store.
    RemoveFromCache(this);

    err = CloseMDB(PR_FALSE);    // since we're about to delete it, no need to commit.
    if (m_mdbStore)
    {
        m_mdbStore->Release();
        m_mdbStore = nsnull;
    }
    Release();
    return err;
}

NS_IMETHODIMP nsAddrDatabase::Commit(PRUint32 commitType)
{
    nsresult    err = NS_OK;
    nsIMdbThumb    *commitThumb = nsnull;

  if (commitType == nsAddrDBCommitType::kLargeCommit || commitType == nsAddrDBCommitType::kSessionCommit)
  {
    mdb_percent outActualWaste = 0;
    mdb_bool outShould;
    if (m_mdbStore) 
    {
      // check how much space would be saved by doing a compress commit.
      // If it's more than 30%, go for it.
      // N.B. - I'm not sure this calls works in Mork for all cases.
      err = m_mdbStore->ShouldCompress(GetEnv(), 30, &outActualWaste, &outShould);
      if (NS_SUCCEEDED(err) && outShould)
      {
        commitType = nsAddrDBCommitType::kCompressCommit;
      }
    }
  }
    if (m_mdbStore)
    {
        switch (commitType)
        {
        case nsAddrDBCommitType::kSmallCommit:
            err = m_mdbStore->SmallCommit(GetEnv());
            break;
        case nsAddrDBCommitType::kLargeCommit:
            err = m_mdbStore->LargeCommit(GetEnv(), &commitThumb);
            break;
        case nsAddrDBCommitType::kSessionCommit:
            // comment out until persistence works.
            err = m_mdbStore->SessionCommit(GetEnv(), &commitThumb);
            break;
        case nsAddrDBCommitType::kCompressCommit:
            err = m_mdbStore->CompressCommit(GetEnv(), &commitThumb);
            break;
        }
    }
    if (commitThumb)
    {
        mdb_count outTotal = 0;    // total somethings to do in operation
        mdb_count outCurrent = 0;  // subportion of total completed so far
        mdb_bool outDone = PR_FALSE;      // is operation finished?
        mdb_bool outBroken = PR_FALSE;     // is operation irreparably dead and broken?
        while (!outDone && !outBroken && err == NS_OK)
        {
            err = commitThumb->DoMore(GetEnv(), &outTotal, &outCurrent, &outDone, &outBroken);
        }
        NS_RELEASE(commitThumb);
    }
    // ### do something with error, but clear it now because mork errors out on commits.
    if (GetEnv())
        GetEnv()->ClearErrors();
    return err;
}

NS_IMETHODIMP nsAddrDatabase::Close(PRBool forceCommit /* = TRUE */)
{
    return CloseMDB(forceCommit);
}

// set up empty tablesetc.
nsresult nsAddrDatabase::InitNewDB()
{
    nsresult err = NS_OK;

    err = InitMDBInfo();
    if (NS_SUCCEEDED(err))
    {
        err = InitPabTable();
        err = InitLastRecorKey();
        Commit(nsAddrDBCommitType::kLargeCommit);
    }
    return err;
}

nsresult nsAddrDatabase::InitPabTable()
{
    nsIMdbStore *store = GetStore();

    mdb_err mdberr = (nsresult) store->NewTableWithOid(GetEnv(), &gAddressBookTableOID, 
        m_PabTableKind, PR_FALSE, (const mdbOid*)nsnull, &m_mdbPabTable);

    return mdberr;
}

//save the last record number, store in m_DataRowScopeToken, row 1
nsresult nsAddrDatabase::InitLastRecorKey()
{
    if (!m_mdbPabTable)
        return NS_ERROR_NULL_POINTER;

    nsresult err = NS_OK;
    nsIMdbRow *pDataRow = nsnull;
    mdbOid dataRowOid;
    dataRowOid.mOid_Scope = m_DataRowScopeToken;
    dataRowOid.mOid_Id = DATAROW_ROWID;
    err  = GetStore()->NewRowWithOid(GetEnv(), &dataRowOid, &pDataRow);

    if (NS_SUCCEEDED(err) && pDataRow)
    {
        m_LastRecordKey = 0;
        err = AddIntColumn(pDataRow, m_LastRecordKeyColumnToken, 0);
        err = m_mdbPabTable->AddRow(GetEnv(), pDataRow);
        NS_RELEASE(pDataRow);
    }
    return err;
}

nsresult nsAddrDatabase::GetDataRow(nsIMdbRow **pDataRow)
{
    nsIMdbRow *pRow = nsnull;
    mdbOid dataRowOid;
    dataRowOid.mOid_Scope = m_DataRowScopeToken;
    dataRowOid.mOid_Id = DATAROW_ROWID;
    GetStore()->GetRow(GetEnv(), &dataRowOid, &pRow);
    *pDataRow = pRow;
    if (pRow)
        return NS_OK;
    else
        return NS_ERROR_FAILURE;
}

nsresult nsAddrDatabase::GetLastRecordKey()
{
    if (!m_mdbPabTable)
        return NS_ERROR_NULL_POINTER;

    nsresult err = NS_OK;
    nsCOMPtr <nsIMdbRow> pDataRow;
    err = GetDataRow(getter_AddRefs(pDataRow));

    if (NS_SUCCEEDED(err) && pDataRow)
    {
        m_LastRecordKey = 0;
        err = GetIntColumn(pDataRow, m_LastRecordKeyColumnToken, &m_LastRecordKey, 0);
        if (NS_FAILED(err))
            err = NS_ERROR_NOT_AVAILABLE;
        return NS_OK;
    }
    else
        return NS_ERROR_NOT_AVAILABLE;
    return err;
}

nsresult nsAddrDatabase::UpdateLastRecordKey()
{
    if (!m_mdbPabTable)
        return NS_ERROR_NULL_POINTER;

    nsresult err = NS_OK;
    nsCOMPtr <nsIMdbRow> pDataRow;
    err = GetDataRow(getter_AddRefs(pDataRow));

    if (NS_SUCCEEDED(err) && pDataRow)
    {
        err = AddIntColumn(pDataRow, m_LastRecordKeyColumnToken, m_LastRecordKey);
        err = m_mdbPabTable->AddRow(GetEnv(), pDataRow);
        return NS_OK;
    }
    else if (!pDataRow)
        err = InitLastRecorKey();
    else
        return NS_ERROR_NOT_AVAILABLE;
    return err;
}

nsresult nsAddrDatabase::InitExistingDB()
{
    nsresult err = NS_OK;

    err = InitMDBInfo();
    if (err == NS_OK)
    {
        err = GetStore()->GetTable(GetEnv(), &gAddressBookTableOID, &m_mdbPabTable);

        err = GetLastRecordKey();
        if (err == NS_ERROR_NOT_AVAILABLE)
            CheckAndUpdateRecordKey();
        UpdateLowercaseEmailListName();
    }
    return err;
}

nsresult nsAddrDatabase::CheckAndUpdateRecordKey()
{
    nsresult err = NS_OK;
    nsIMdbTableRowCursor* rowCursor = nsnull;
    nsIMdbRow* findRow = nsnull;
     mdb_pos    rowPos = 0;

    mdb_err merror = m_mdbPabTable->GetTableRowCursor(GetEnv(), -1, &rowCursor);

    if (!(merror == NS_OK && rowCursor))
        return NS_ERROR_FAILURE;

    nsCOMPtr <nsIMdbRow> pDataRow;
    err = GetDataRow(getter_AddRefs(pDataRow));
    if (NS_FAILED(err))
        InitLastRecorKey();

    do
    {  //add key to each card and mailing list row
        merror = rowCursor->NextRow(GetEnv(), &findRow, &rowPos);
        if (merror == NS_OK && findRow)
        {
            mdbOid rowOid;

            if (findRow->GetOid(GetEnv(), &rowOid) == NS_OK)
            {
                if (!IsDataRowScopeToken(rowOid.mOid_Scope))
                {
                    m_LastRecordKey++;
                    err = AddIntColumn(findRow, m_RecordKeyColumnToken, m_LastRecordKey);
                }
            }
        }
    } while (findRow);

    UpdateLastRecordKey();
    Commit(nsAddrDBCommitType::kLargeCommit);
    return NS_OK;
}

nsresult nsAddrDatabase::UpdateLowercaseEmailListName()
{
  nsresult err = NS_OK;
  nsIMdbTableRowCursor* rowCursor = nsnull;
  nsIMdbRow* findRow = nsnull;
  mdb_pos    rowPos = 0;
  PRBool commitRequired = PR_FALSE;
  
  mdb_err merror = m_mdbPabTable->GetTableRowCursor(GetEnv(), -1, &rowCursor);
  
  if (!(merror == NS_OK && rowCursor))
    return NS_ERROR_FAILURE;
  
  do
  {   //add lowercase primary emial to each card and mailing list row
    merror = rowCursor->NextRow(GetEnv(), &findRow, &rowPos);
    if (merror ==NS_OK && findRow)
    {
      mdbOid rowOid;
      
      if (findRow->GetOid(GetEnv(), &rowOid) == NS_OK)
      {
        nsAutoString tempString;
        if (IsCardRowScopeToken(rowOid.mOid_Scope))
        {
          err = GetStringColumn(findRow, m_LowerPriEmailColumnToken, tempString);
          if (NS_SUCCEEDED(err))
            break;
          
          err = ConvertAndAddLowercaseColumn(findRow, m_PriEmailColumnToken, 
            m_LowerPriEmailColumnToken);
          commitRequired = PR_TRUE;
        }
        else if (IsListRowScopeToken(rowOid.mOid_Scope))
        {
          err = GetStringColumn(findRow, m_LowerListNameColumnToken, tempString);
          if (NS_SUCCEEDED(err))
            break;
          
          err = ConvertAndAddLowercaseColumn(findRow, m_ListNameColumnToken, 
            m_LowerListNameColumnToken);
          commitRequired = PR_TRUE;
        }
      }
      findRow->Release();
    }
  } while (findRow);

  if (findRow)
    findRow->Release();
  rowCursor->Release();
  if (commitRequired)
    Commit(nsAddrDBCommitType::kLargeCommit);
  return NS_OK;
}

/*  
We store UTF8 strings in the database.  We need to convert the UTF8 
string into unicode string, then convert to lower case.  Before storing 
back into the database,  we need to convert the lowercase unicode string 
into UTF8 string.
*/
nsresult nsAddrDatabase::ConvertAndAddLowercaseColumn
(nsIMdbRow * row, mdb_token fromCol, mdb_token toCol)
{
    nsresult err = NS_OK;
    nsAutoString colUtf8String;

    err = GetStringColumn(row, fromCol, colUtf8String);
    if (colUtf8String.Length())
    {
        char* pUTF8String = ToNewCString(colUtf8String);
        err = AddLowercaseColumn(row, toCol, pUTF8String);
        nsCRT::free(pUTF8String);
    }
    return err;
}

/*  
Chnage the unicode string to lowercase, then convert to UTF8 string to store in db
*/
nsresult nsAddrDatabase::AddUnicodeToColumn(nsIMdbRow * row, mdb_token colToken, const PRUnichar* pUnicodeStr)
{
    nsresult err = NS_OK;
    nsAutoString displayString(pUnicodeStr);
    NS_ConvertUCS2toUTF8 displayUTF8Str(displayString);
    ToLowerCase(displayString);
    NS_ConvertUCS2toUTF8 UTF8Str(displayString);
    if (colToken == m_PriEmailColumnToken)
    {
        err = AddCharStringColumn(row, m_PriEmailColumnToken, displayUTF8Str.get());
        err = AddLowercaseColumn(row, m_LowerPriEmailColumnToken, UTF8Str.get());
    }
    else if (colToken == m_ListNameColumnToken)
    {
        err = AddCharStringColumn(row, m_ListNameColumnToken, displayUTF8Str.get());
        err = AddLowercaseColumn(row, m_LowerListNameColumnToken, UTF8Str.get());
    }
    return err;
}

// initialize the various tokens and tables in our db's env
nsresult nsAddrDatabase::InitMDBInfo()
{
    nsresult err = NS_OK;

    if (!m_mdbTokensInitialized && GetStore())
    {
        m_mdbTokensInitialized = PR_TRUE;
        err    = GetStore()->StringToToken(GetEnv(), kCardRowScope, &m_CardRowScopeToken); 
        err    = GetStore()->StringToToken(GetEnv(), kListRowScope, &m_ListRowScopeToken); 
        err    = GetStore()->StringToToken(GetEnv(), kDataRowScope, &m_DataRowScopeToken); 
        gAddressBookTableOID.mOid_Scope = m_CardRowScopeToken;
        gAddressBookTableOID.mOid_Id = ID_PAB_TABLE;
        if (NS_SUCCEEDED(err))
        { 
            GetStore()->StringToToken(GetEnv(),  kFirstNameColumn, &m_FirstNameColumnToken);
            GetStore()->StringToToken(GetEnv(),  kLastNameColumn, &m_LastNameColumnToken);
            GetStore()->StringToToken(GetEnv(),  kDisplayNameColumn, &m_DisplayNameColumnToken);
            GetStore()->StringToToken(GetEnv(),  kNicknameColumn, &m_NickNameColumnToken);
            GetStore()->StringToToken(GetEnv(),  kPriEmailColumn, &m_PriEmailColumnToken);
            GetStore()->StringToToken(GetEnv(),  kLowerPriEmailColumn, &m_LowerPriEmailColumnToken);
            GetStore()->StringToToken(GetEnv(),  k2ndEmailColumn, &m_2ndEmailColumnToken);
            GetStore()->StringToToken(GetEnv(),  kPreferMailFormatColumn, &m_MailFormatColumnToken);
            GetStore()->StringToToken(GetEnv(),  kWorkPhoneColumn, &m_WorkPhoneColumnToken);
            GetStore()->StringToToken(GetEnv(),  kHomePhoneColumn, &m_HomePhoneColumnToken);
            GetStore()->StringToToken(GetEnv(),  kFaxColumn, &m_FaxColumnToken);
            GetStore()->StringToToken(GetEnv(),  kPagerColumn, &m_PagerColumnToken);
            GetStore()->StringToToken(GetEnv(),  kCellularColumn, &m_CellularColumnToken);
            GetStore()->StringToToken(GetEnv(),  kHomeAddressColumn, &m_HomeAddressColumnToken);
            GetStore()->StringToToken(GetEnv(),  kHomeAddress2Column, &m_HomeAddress2ColumnToken);
            GetStore()->StringToToken(GetEnv(),  kHomeCityColumn, &m_HomeCityColumnToken);
            GetStore()->StringToToken(GetEnv(),  kHomeStateColumn, &m_HomeStateColumnToken);
            GetStore()->StringToToken(GetEnv(),  kHomeZipCodeColumn, &m_HomeZipCodeColumnToken);
            GetStore()->StringToToken(GetEnv(),  kHomeCountryColumn, &m_HomeCountryColumnToken);
            GetStore()->StringToToken(GetEnv(),  kWorkAddressColumn, &m_WorkAddressColumnToken);
            GetStore()->StringToToken(GetEnv(),  kWorkAddress2Column, &m_WorkAddress2ColumnToken);
            GetStore()->StringToToken(GetEnv(),  kWorkCityColumn, &m_WorkCityColumnToken);
            GetStore()->StringToToken(GetEnv(),  kWorkStateColumn, &m_WorkStateColumnToken);
            GetStore()->StringToToken(GetEnv(),  kWorkZipCodeColumn, &m_WorkZipCodeColumnToken);
            GetStore()->StringToToken(GetEnv(),  kWorkCountryColumn, &m_WorkCountryColumnToken);
            GetStore()->StringToToken(GetEnv(),  kJobTitleColumn, &m_JobTitleColumnToken);
            GetStore()->StringToToken(GetEnv(),  kDepartmentColumn, &m_DepartmentColumnToken);
            GetStore()->StringToToken(GetEnv(),  kCompanyColumn, &m_CompanyColumnToken);
            GetStore()->StringToToken(GetEnv(),  kWebPage1Column, &m_WebPage1ColumnToken);
            GetStore()->StringToToken(GetEnv(),  kWebPage2Column, &m_WebPage2ColumnToken);
            GetStore()->StringToToken(GetEnv(),  kBirthYearColumn, &m_BirthYearColumnToken);
            GetStore()->StringToToken(GetEnv(),  kBirthMonthColumn, &m_BirthMonthColumnToken);
            GetStore()->StringToToken(GetEnv(),  kBirthDayColumn, &m_BirthDayColumnToken);
            GetStore()->StringToToken(GetEnv(),  kCustom1Column, &m_Custom1ColumnToken);
            GetStore()->StringToToken(GetEnv(),  kCustom2Column, &m_Custom2ColumnToken);
            GetStore()->StringToToken(GetEnv(),  kCustom3Column, &m_Custom3ColumnToken);
            GetStore()->StringToToken(GetEnv(),  kCustom4Column, &m_Custom4ColumnToken);
            GetStore()->StringToToken(GetEnv(),  kNotesColumn, &m_NotesColumnToken);
            GetStore()->StringToToken(GetEnv(),  kLastModifiedDateColumn, &m_LastModDateColumnToken);
            GetStore()->StringToToken(GetEnv(),  kRecordKeyColumn, &m_RecordKeyColumnToken);

            GetStore()->StringToToken(GetEnv(),  kAddressCharSetColumn, &m_AddressCharSetColumnToken);
            GetStore()->StringToToken(GetEnv(),  kLastRecordKeyColumn, &m_LastRecordKeyColumnToken);

            err = GetStore()->StringToToken(GetEnv(), kPabTableKind, &m_PabTableKind); 

            GetStore()->StringToToken(GetEnv(),  kMailListName, &m_ListNameColumnToken);
            GetStore()->StringToToken(GetEnv(),  kMailListNickName, &m_ListNickNameColumnToken);
            GetStore()->StringToToken(GetEnv(),  kMailListDescription, &m_ListDescriptionColumnToken);
            GetStore()->StringToToken(GetEnv(),  kMailListTotalAddresses, &m_ListTotalColumnToken);
            GetStore()->StringToToken(GetEnv(),  kLowerListNameColumn, &m_LowerListNameColumnToken);
        }
    }
    return err;
}

////////////////////////////////////////////////////////////////////////////////

nsresult nsAddrDatabase::AddRecordKeyColumnToRow(nsIMdbRow *pRow)
{
    if (pRow)
    {
        m_LastRecordKey++;
        nsresult err = AddIntColumn(pRow, m_RecordKeyColumnToken, m_LastRecordKey);
        err = m_mdbPabTable->AddRow(GetEnv(), pRow);
        UpdateLastRecordKey();
        return NS_OK;
    }
    else
        return NS_ERROR_NULL_POINTER;
}

nsresult nsAddrDatabase::AddAttributeColumnsToRow(nsIAbCard *card, nsIMdbRow *cardRow)
{
  nsresult    err = NS_OK;
  
  if (!card && !cardRow )
    return NS_ERROR_NULL_POINTER;
  
  mdbOid rowOid, tableOid;
  m_mdbPabTable->GetOid(GetEnv(), &tableOid);
  cardRow->GetOid(GetEnv(), &rowOid);
  
  nsCOMPtr<nsIAbMDBCard> dbcard(do_QueryInterface(card, &err));
  if(NS_SUCCEEDED(err) && dbcard)
  {
    dbcard->SetDbTableID(tableOid.mOid_Id);
    dbcard->SetDbRowID(rowOid.mOid_Id);
  }
  // add the row to the singleton table.
  if (card && cardRow)
  {
    nsXPIDLString unicodeStr;
    card->GetFirstName(getter_Copies(unicodeStr));
    AddFirstName(cardRow, NS_ConvertUCS2toUTF8(unicodeStr).get());
    
    card->GetLastName(getter_Copies(unicodeStr));
    AddLastName(cardRow, NS_ConvertUCS2toUTF8(unicodeStr).get());
    
    card->GetDisplayName(getter_Copies(unicodeStr));
    AddDisplayName(cardRow, NS_ConvertUCS2toUTF8(unicodeStr).get());
    
    card->GetNickName(getter_Copies(unicodeStr));
    AddNickName(cardRow, NS_ConvertUCS2toUTF8(unicodeStr).get());
    
    card->GetPrimaryEmail(getter_Copies(unicodeStr));
    if (unicodeStr)
      AddUnicodeToColumn(cardRow, m_PriEmailColumnToken, unicodeStr);
    
    card->GetSecondEmail(getter_Copies(unicodeStr));
    Add2ndEmail(cardRow, NS_ConvertUCS2toUTF8(unicodeStr).get());
    
    PRUint32 format = nsIAbPreferMailFormat::unknown;
    card->GetPreferMailFormat(&format);
    AddPreferMailFormat(cardRow, format);
    
    card->GetWorkPhone(getter_Copies(unicodeStr));
    AddWorkPhone(cardRow, NS_ConvertUCS2toUTF8(unicodeStr).get());
    
    card->GetHomePhone(getter_Copies(unicodeStr));
    AddHomePhone(cardRow, NS_ConvertUCS2toUTF8(unicodeStr).get());
    
    card->GetFaxNumber(getter_Copies(unicodeStr));
    AddFaxNumber(cardRow, NS_ConvertUCS2toUTF8(unicodeStr).get());
    
    card->GetPagerNumber(getter_Copies(unicodeStr));
    AddPagerNumber(cardRow,NS_ConvertUCS2toUTF8(unicodeStr).get());
    
    card->GetCellularNumber(getter_Copies(unicodeStr));
    AddCellularNumber(cardRow,NS_ConvertUCS2toUTF8(unicodeStr).get());
    
    card->GetHomeAddress(getter_Copies(unicodeStr));
    AddHomeAddress(cardRow, NS_ConvertUCS2toUTF8(unicodeStr).get());
    
    card->GetHomeAddress2(getter_Copies(unicodeStr)); 
    AddHomeAddress2(cardRow, NS_ConvertUCS2toUTF8(unicodeStr).get());
    
    card->GetHomeCity(getter_Copies(unicodeStr)); 
    AddHomeCity(cardRow, NS_ConvertUCS2toUTF8(unicodeStr).get());
    
    card->GetHomeState(getter_Copies(unicodeStr)); 
    AddHomeState(cardRow, NS_ConvertUCS2toUTF8(unicodeStr).get());
    
    card->GetHomeZipCode(getter_Copies(unicodeStr)); 
    AddHomeZipCode(cardRow, NS_ConvertUCS2toUTF8(unicodeStr).get());
    
    card->GetHomeCountry(getter_Copies(unicodeStr)); 
    AddHomeCountry(cardRow, NS_ConvertUCS2toUTF8(unicodeStr).get());
    
    card->GetWorkAddress(getter_Copies(unicodeStr));  
    AddWorkAddress(cardRow, NS_ConvertUCS2toUTF8(unicodeStr).get());
      
    card->GetWorkAddress2(getter_Copies(unicodeStr)); 
    AddWorkAddress2(cardRow, NS_ConvertUCS2toUTF8(unicodeStr).get());
      
    card->GetWorkCity(getter_Copies(unicodeStr)); 
    AddWorkCity(cardRow, NS_ConvertUCS2toUTF8(unicodeStr).get());
      
    card->GetWorkState(getter_Copies(unicodeStr)); 
    AddWorkState(cardRow, NS_ConvertUCS2toUTF8(unicodeStr).get());
     
    card->GetWorkZipCode(getter_Copies(unicodeStr)); 
    AddWorkZipCode(cardRow, NS_ConvertUCS2toUTF8(unicodeStr).get());
     
    card->GetWorkCountry(getter_Copies(unicodeStr)); 
    AddWorkCountry(cardRow, NS_ConvertUCS2toUTF8(unicodeStr).get());
      
    card->GetJobTitle(getter_Copies(unicodeStr)); 
    AddJobTitle(cardRow, NS_ConvertUCS2toUTF8(unicodeStr).get());
      
    card->GetDepartment(getter_Copies(unicodeStr)); 
    AddDepartment(cardRow, NS_ConvertUCS2toUTF8(unicodeStr).get());
      
    card->GetCompany(getter_Copies(unicodeStr)); 
    AddCompany(cardRow, NS_ConvertUCS2toUTF8(unicodeStr).get());
     
    card->GetWebPage1(getter_Copies(unicodeStr)); 
    AddWebPage1(cardRow,NS_ConvertUCS2toUTF8(unicodeStr).get());
     
    card->GetWebPage2(getter_Copies(unicodeStr)); 
    AddWebPage2(cardRow, NS_ConvertUCS2toUTF8(unicodeStr).get());
     
    card->GetBirthYear(getter_Copies(unicodeStr)); 
    AddBirthYear(cardRow, NS_ConvertUCS2toUTF8(unicodeStr).get());
      
    card->GetBirthMonth(getter_Copies(unicodeStr)); 
    AddBirthMonth(cardRow, NS_ConvertUCS2toUTF8(unicodeStr).get());
      
    card->GetBirthDay(getter_Copies(unicodeStr)); 
    AddBirthDay(cardRow, NS_ConvertUCS2toUTF8(unicodeStr).get());
      
    card->GetCustom1(getter_Copies(unicodeStr)); 
    AddCustom1(cardRow,NS_ConvertUCS2toUTF8(unicodeStr).get());
     
    card->GetCustom2(getter_Copies(unicodeStr)); 
    AddCustom2(cardRow, NS_ConvertUCS2toUTF8(unicodeStr).get());
      
    card->GetCustom3(getter_Copies(unicodeStr)); 
    AddCustom3(cardRow, NS_ConvertUCS2toUTF8(unicodeStr).get());
      
    card->GetCustom4(getter_Copies(unicodeStr)); 
    AddCustom4(cardRow, NS_ConvertUCS2toUTF8(unicodeStr).get());
    
    card->GetNotes(getter_Copies(unicodeStr)); 
    AddNotes(cardRow, NS_ConvertUCS2toUTF8(unicodeStr).get());
      
  }
  return NS_OK;
}

NS_IMETHODIMP nsAddrDatabase::CreateNewCardAndAddToDB(nsIAbCard *newCard, PRBool notify /* = FALSE */)
{
    nsresult    err = NS_OK;
    nsIMdbRow    *cardRow;

    if (!newCard || !m_mdbPabTable)
        return NS_ERROR_NULL_POINTER;

    err  = GetNewRow(&cardRow);

    if (NS_SUCCEEDED(err) && cardRow)
    {
        AddAttributeColumnsToRow(newCard, cardRow);
        AddRecordKeyColumnToRow(cardRow);

    // we need to do this for dnd
    PRUint32 key = 0;
      err = GetIntColumn(cardRow, m_RecordKeyColumnToken, &key, 0);
    if (NS_SUCCEEDED(err)) {
      nsCOMPtr<nsIAbMDBCard> dbnewCard(do_QueryInterface(newCard, &err));
      if (NS_SUCCEEDED(err) && dbnewCard)
        dbnewCard->SetKey(key);
    }

        mdb_err merror = m_mdbPabTable->AddRow(GetEnv(), cardRow);
        if (merror != NS_OK) return NS_ERROR_FAILURE;
                NS_RELEASE(cardRow);

    }
    else
        return err;

    //  do notification
    if (notify)
    {
        NotifyCardEntryChange(AB_NotifyInserted, newCard, NULL);
    }
    return err;
}

NS_IMETHODIMP nsAddrDatabase::CreateNewCardAndAddToDBWithKey(nsIAbCard *newCard, PRBool notify /* = FALSE */, PRUint32 *key)
{
  nsresult    err = NS_OK;
  *key = 0;

  err = CreateNewCardAndAddToDB(newCard, notify);
  if (NS_SUCCEEDED(err))
    *key = m_LastRecordKey;

  return err;
}

NS_IMETHODIMP nsAddrDatabase::CreateNewListCardAndAddToDB(nsIAbDirectory *aList, PRUint32 listRowID, nsIAbCard *newCard, PRBool notify /* = FALSE */)
{
    if (!newCard || !m_mdbPabTable)
        return NS_ERROR_NULL_POINTER;

    nsIMdbRow* pListRow = nsnull;
    mdbOid listRowOid;
    listRowOid.mOid_Scope = m_ListRowScopeToken;
    listRowOid.mOid_Id = listRowID;
    nsresult rv = GetStore()->GetRow(GetEnv(), &listRowOid, &pListRow);
  NS_ENSURE_SUCCESS(rv,rv);
  if (!pListRow)
    return NS_OK;
  
  
  nsCOMPtr <nsISupportsArray> addressList;
  rv = aList->GetAddressLists(getter_AddRefs(addressList));
  NS_ENSURE_SUCCESS(rv,rv);

  PRUint32 count;
    addressList->Count(&count);

  nsXPIDLString newEmail;
  rv = newCard->GetPrimaryEmail(getter_Copies(newEmail));
  NS_ENSURE_SUCCESS(rv,rv);

    PRUint32 i;
  for (i = 0; i < count; i++) {
    nsCOMPtr<nsISupports> support = getter_AddRefs(addressList->ElementAt(i));
    nsCOMPtr<nsIAbCard> currentCard = do_QueryInterface(support, &rv);
    NS_ENSURE_SUCCESS(rv,rv);

    PRBool equals;
    rv = newCard->Equals(currentCard, &equals);
    NS_ENSURE_SUCCESS(rv,rv);

    if (equals) {
      // card is already in list, bail out.
      // this can happen when dropping a card on a mailing list from the directory that contains the mailing list
      return NS_OK;
    }

    nsXPIDLString currentEmail;
    rv = currentCard->GetPrimaryEmail(getter_Copies(currentEmail));
    NS_ENSURE_SUCCESS(rv,rv);

    if (!nsCRT::strcmp(newEmail.get(), currentEmail.get())) {
      // card is already in list, bail out
      // this can happen when dropping a card on a mailing list from another directory (not the one that contains the mailing list
      // or if you have multiple cards on a directory, with the same primary email address.
      return NS_OK;
    }
  }  
    
  // start from 1
  PRUint32 totalAddress = GetListAddressTotal(pListRow) + 1;
  SetListAddressTotal(pListRow, totalAddress);
  nsCOMPtr<nsIAbCard> pNewCard;
  rv = AddListCardColumnsToRow(newCard, pListRow, totalAddress, getter_AddRefs(pNewCard));
  NS_ENSURE_SUCCESS(rv,rv);

  addressList->AppendElement(newCard);

  if (notify)
    NotifyCardEntryChange(AB_NotifyInserted, newCard, nsnull); 

    return rv;
}

nsresult nsAddrDatabase::AddListCardColumnsToRow
(nsIAbCard *pCard, nsIMdbRow *pListRow, PRUint32 pos, nsIAbCard** pNewCard)
{
  if (!pCard && !pListRow )
    return NS_ERROR_NULL_POINTER;
  
  nsresult    err = NS_OK;
  nsXPIDLString email;
  pCard->GetPrimaryEmail(getter_Copies(email));
  if (email)
  {
    nsIMdbRow    *pCardRow = nsnull;
    // Please DO NOT change the 3rd param of GetRowFromAttribute() call to 
    // PR_TRUE (ie, case insensitive) without reading bugs #128535 and #121478.
    err = GetRowFromAttribute(kPriEmailColumn, NS_ConvertUCS2toUTF8(email).get(), PR_FALSE /* retain case */, &pCardRow);
    PRBool cardWasAdded = PR_FALSE;
    if (NS_FAILED(err) || !pCardRow)
    {
      //New Email, then add a new row with this email
      err  = GetNewRow(&pCardRow);
      
      if (NS_SUCCEEDED(err) && pCardRow)
      {
        AddPrimaryEmail(pCardRow, NS_ConvertUCS2toUTF8(email).get());
        err = m_mdbPabTable->AddRow(GetEnv(), pCardRow);
      }
      
      cardWasAdded = PR_TRUE;
    }
    
    NS_ENSURE_TRUE(pCardRow, NS_ERROR_NULL_POINTER);
    
    nsCOMPtr<nsIAbCard>    newCard;
    CreateABCard(pCardRow, 0, getter_AddRefs(newCard));
    NS_IF_ADDREF(*pNewCard = newCard);
    
    if (cardWasAdded) {
      NotifyCardEntryChange(AB_NotifyInserted, newCard, NULL);
    }
    
    //add a column with address row id to the list row
    mdb_token listAddressColumnToken;
    
    char columnStr[COLUMN_STR_MAX];
    PR_snprintf(columnStr, COLUMN_STR_MAX, kMailListAddressFormat, pos);
    GetStore()->StringToToken(GetEnv(),  columnStr, &listAddressColumnToken);
    
    mdbOid outOid;
    
    if (pCardRow->GetOid(GetEnv(), &outOid) == NS_OK)
    {
      //save address row ID to the list row
      err = AddIntColumn(pListRow, listAddressColumnToken, outOid.mOid_Id);
    }
    NS_RELEASE(pCardRow);

  }
  
  return NS_OK;
}

nsresult nsAddrDatabase::AddListAttributeColumnsToRow(nsIAbDirectory *list, nsIMdbRow *listRow)
{
    nsresult    err = NS_OK;

    if (!list && !listRow )
        return NS_ERROR_NULL_POINTER;

    mdbOid rowOid, tableOid;
    m_mdbPabTable->GetOid(GetEnv(), &tableOid);
    listRow->GetOid(GetEnv(), &rowOid);

    nsCOMPtr<nsIAbMDBDirectory> dblist(do_QueryInterface(list,&err));
    if (NS_SUCCEEDED(err))
        dblist->SetDbRowID(rowOid.mOid_Id);

    // add the row to the singleton table.
    if (NS_SUCCEEDED(err) && listRow)
    {
        nsXPIDLString unicodeStr;

        list->GetDirName(getter_Copies(unicodeStr));
        if (unicodeStr)
            AddUnicodeToColumn(listRow, m_ListNameColumnToken, unicodeStr);

        list->GetListNickName(getter_Copies(unicodeStr));
        AddListNickName(listRow, NS_ConvertUCS2toUTF8(unicodeStr).get());
        
        list->GetDescription(getter_Copies(unicodeStr));
        AddListDescription(listRow, NS_ConvertUCS2toUTF8(unicodeStr).get());
            

    // XXX todo, this code has problems if you manually enter duplicate emails.
        nsCOMPtr <nsISupportsArray> pAddressLists;
        list->GetAddressLists(getter_AddRefs(pAddressLists));
        PRUint32 count;
        pAddressLists->Count(&count);

      nsXPIDLString email;
        PRUint32 i, total;
        total = 0;
        for (i = 0; i < count; i++)
        {
            nsCOMPtr<nsISupports> pSupport = getter_AddRefs(pAddressLists->ElementAt(i));
            nsCOMPtr<nsIAbCard> pCard(do_QueryInterface(pSupport, &err));
            
            if (NS_FAILED(err))
                continue;

            pCard->GetPrimaryEmail(getter_Copies(email));
            PRInt32 emailLength = nsCRT::strlen(email);
            if (email && emailLength)
                total++;
        }
        SetListAddressTotal(listRow, total);

        PRUint32 pos;
        for (i = 0; i < count; i++)
        {
            nsCOMPtr<nsISupports> pSupport = getter_AddRefs(pAddressLists->ElementAt(i));
            nsCOMPtr<nsIAbCard> pCard(do_QueryInterface(pSupport, &err));
            
            if (NS_FAILED(err))
                continue;

            // start from 1
            pos = i + 1;
            pCard->GetPrimaryEmail(getter_Copies(email));
            PRInt32 emailLength = nsCRT::strlen(email);
            if (email && emailLength)
            {
                nsCOMPtr<nsIAbCard> pNewCard;
                err = AddListCardColumnsToRow(pCard, listRow, pos, getter_AddRefs(pNewCard));
                if (pNewCard)
                    pAddressLists->ReplaceElementAt(pNewCard, i);
            }
        }
    }
    return NS_OK;
}

PRUint32 nsAddrDatabase::GetListAddressTotal(nsIMdbRow* listRow)
{
    PRUint32 count = 0;
    GetIntColumn(listRow, m_ListTotalColumnToken, &count, 0);
    return count;
}

nsresult nsAddrDatabase::SetListAddressTotal(nsIMdbRow* listRow, PRUint32 total)
{
    return AddIntColumn(listRow, m_ListTotalColumnToken, total);
}


nsresult nsAddrDatabase::GetAddressRowByPos(nsIMdbRow* listRow, PRUint16 pos, nsIMdbRow** cardRow)
{
    mdb_token listAddressColumnToken;

    char columnStr[COLUMN_STR_MAX];
    PR_snprintf(columnStr, COLUMN_STR_MAX, kMailListAddressFormat, pos);
    GetStore()->StringToToken(GetEnv(),  columnStr, &listAddressColumnToken);

  nsAutoString tempString;
    mdb_id rowID;
    nsresult err = GetIntColumn(listRow, listAddressColumnToken, (PRUint32*)&rowID, 0);
    if (NS_SUCCEEDED(err))
    {
        err = GetCardRowByRowID(rowID, cardRow);
        return err;
    }
    else
    {
        return NS_ERROR_FAILURE;
    }
}

NS_IMETHODIMP nsAddrDatabase::CreateMailListAndAddToDB(nsIAbDirectory *newList, PRBool notify /* = FALSE */)
{
    nsresult    err = NS_OK;
    nsIMdbRow    *listRow;

    if (!newList || !m_mdbPabTable)
        return NS_ERROR_NULL_POINTER;
    
    err  = GetNewListRow(&listRow);

    if (NS_SUCCEEDED(err) && listRow)
    {
        AddListAttributeColumnsToRow(newList, listRow);
        AddRecordKeyColumnToRow(listRow);
        mdb_err merror = m_mdbPabTable->AddRow(GetEnv(), listRow);
        if (merror != NS_OK) return NS_ERROR_FAILURE;

        nsCOMPtr<nsIAbCard> listCard;
        CreateABListCard(listRow, getter_AddRefs(listCard));
        NotifyCardEntryChange(AB_NotifyInserted, listCard, NULL);

                NS_RELEASE(listRow);
        return NS_OK;
    }
    else 
        return NS_ERROR_FAILURE;

}

nsresult nsAddrDatabase::FindAttributeRow(nsIMdbTable* pTable, mdb_token columnToken, nsIMdbRow** row)
{
    nsIMdbTableRowCursor* rowCursor = nsnull;
    nsIMdbRow* findRow = nsnull;
    nsIMdbCell* valueCell = nsnull;
     mdb_pos    rowPos = 0;
    nsresult err = NS_ERROR_FAILURE;

    err = pTable->GetTableRowCursor(GetEnv(), -1, &rowCursor);

    if (!(err == NS_OK && rowCursor))
        return NS_ERROR_FAILURE;
    do
    {
        err = rowCursor->NextRow(GetEnv(), &findRow, &rowPos);
        if (NS_SUCCEEDED(err) && findRow)
        {
            err = findRow->GetCell(GetEnv(), columnToken, &valueCell);
            if (NS_SUCCEEDED(err) && valueCell)
            {
                *row = findRow;
                return NS_OK;
            }
                        else
                          findRow->Release();
        }
    } while (findRow);
        rowCursor->Release();
    return NS_ERROR_FAILURE;
}

void nsAddrDatabase::DeleteCardFromAllMailLists(mdb_id cardRowID)
{
    nsIMdbTableRowCursor* rowCursor;
    m_mdbPabTable->GetTableRowCursor(GetEnv(), -1, &rowCursor);

    if (rowCursor)
    {
        nsIMdbRow* pListRow = nsnull;
        mdb_pos rowPos;
        do 
        {
            mdb_err err = rowCursor->NextRow(GetEnv(), &pListRow, &rowPos);

            if (err == NS_OK && pListRow)
            {
                mdbOid rowOid;

                if (pListRow->GetOid(GetEnv(), &rowOid) == NS_OK)
                {
                    if (IsListRowScopeToken(rowOid.mOid_Scope))
                        DeleteCardFromListRow(pListRow, cardRowID);
                }
                                NS_RELEASE(pListRow);
            }
        } while (pListRow);

        rowCursor->Release();
    }
}

NS_IMETHODIMP nsAddrDatabase::DeleteCard(nsIAbCard *card, PRBool notify)
{
    if (!card || !m_mdbPabTable)
        return NS_ERROR_NULL_POINTER;

    nsresult err = NS_OK;
    PRBool bIsMailList = PR_FALSE;
    card->GetIsMailList(&bIsMailList);

    // get the right row
    nsIMdbRow* pCardRow = nsnull;
    mdbOid rowOid;
    if (bIsMailList)
        rowOid.mOid_Scope = m_ListRowScopeToken;
    else
        rowOid.mOid_Scope = m_CardRowScopeToken;

    nsCOMPtr<nsIAbMDBCard> dbcard(do_QueryInterface(card, &err));
  NS_ENSURE_SUCCESS(err, err);

    dbcard->GetDbRowID((PRUint32*)&rowOid.mOid_Id);

    err = GetStore()->GetRow(GetEnv(), &rowOid, &pCardRow);
  NS_ENSURE_SUCCESS(err,err);
  if (!pCardRow)
    return NS_OK;

  err = DeleteRow(m_mdbPabTable, pCardRow);
  
  //delete the person card from all mailing list
    if (!bIsMailList)
    DeleteCardFromAllMailLists(rowOid.mOid_Id);
    
  if (NS_SUCCEEDED(err)) {
    if (notify) 
      NotifyCardEntryChange(AB_NotifyDeleted, card, NULL);
  }
  NS_RELEASE(pCardRow);
    return NS_OK;
}

nsresult nsAddrDatabase::DeleteCardFromListRow(nsIMdbRow* pListRow, mdb_id cardRowID)
{
    if (!pListRow)
        return NS_ERROR_NULL_POINTER;

    nsresult err = NS_OK;

//    PRUint32 cardRowID;
//    card->GetDbRowID(&cardRowID);

    //todo: cut the card column and total column
    PRUint32 totalAddress = GetListAddressTotal(pListRow);
    
    PRUint32 pos;
    for (pos = 1; pos <= totalAddress; pos++)
    {
        mdb_token listAddressColumnToken;
        mdb_id rowID;

        char columnStr[COLUMN_STR_MAX];
        PR_snprintf(columnStr, COLUMN_STR_MAX, kMailListAddressFormat, pos);
        GetStore()->StringToToken(GetEnv(), columnStr, &listAddressColumnToken);

        err = GetIntColumn(pListRow, listAddressColumnToken, (PRUint32*)&rowID, 0);

        if (cardRowID == rowID)
        {
            if (pos == totalAddress)
                err = pListRow->CutColumn(GetEnv(), listAddressColumnToken);
            else
            {
                //replace the deleted one with the last one and delete the last one
                mdb_id lastRowID;
                mdb_token lastAddressColumnToken;
                PR_snprintf(columnStr, COLUMN_STR_MAX, kMailListAddressFormat, totalAddress);
                GetStore()->StringToToken(GetEnv(), columnStr, &lastAddressColumnToken);

                err = GetIntColumn(pListRow, lastAddressColumnToken, (PRUint32*)&lastRowID, 0);
                err = AddIntColumn(pListRow, listAddressColumnToken, lastRowID);
                err = pListRow->CutColumn(GetEnv(), lastAddressColumnToken);
            }
            break;
        }
    }
    return NS_OK;
}

NS_IMETHODIMP nsAddrDatabase::DeleteCardFromMailList(nsIAbDirectory *mailList, nsIAbCard *card, PRBool aNotify)
{
    if (!card || !m_mdbPabTable)
        return NS_ERROR_NULL_POINTER;

    nsresult err = NS_OK;

    // get the right row
    nsIMdbRow* pListRow = nsnull;
    mdbOid listRowOid;
    listRowOid.mOid_Scope = m_ListRowScopeToken;

    nsCOMPtr<nsIAbMDBDirectory> dbmailList(do_QueryInterface(mailList,&err));
    if(NS_FAILED(err))
        return NS_ERROR_NULL_POINTER;
    dbmailList->GetDbRowID((PRUint32*)&listRowOid.mOid_Id);

    err = GetStore()->GetRow(GetEnv(), &listRowOid, &pListRow);
  NS_ENSURE_SUCCESS(err,err);
  if (!pListRow)
    return NS_OK;

  PRUint32 cardRowID;
    
  nsCOMPtr<nsIAbMDBCard> dbcard(do_QueryInterface(card, &err));
  if(NS_FAILED(err) || !dbcard)
    return NS_ERROR_NULL_POINTER;
  dbcard->GetDbRowID(&cardRowID);
    
  err = DeleteCardFromListRow(pListRow, cardRowID);
  if (NS_SUCCEEDED(err) && aNotify) {            
    NotifyCardEntryChange(AB_NotifyDeleted, card, NULL);
  }
  NS_RELEASE(pListRow);
  return NS_OK;
}

NS_IMETHODIMP nsAddrDatabase::SetCardValue(nsIAbCard *card, const char *name, const PRUnichar *value, PRBool notify)
{
  NS_ENSURE_ARG_POINTER(card);
  NS_ENSURE_ARG_POINTER(name);
  NS_ENSURE_ARG_POINTER(value);
  
  nsresult rv = NS_OK;
  
  nsCOMPtr <nsIMdbRow> cardRow;
  mdbOid rowOid;
  rowOid.mOid_Scope = m_CardRowScopeToken;
  
  // XXX todo
  // it might be that the caller always has a nsIAbMDBCard
  nsCOMPtr<nsIAbMDBCard> dbcard = do_QueryInterface(card, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  dbcard->GetDbRowID((PRUint32*)&rowOid.mOid_Id);
  
  rv = GetStore()->GetRow(GetEnv(), &rowOid, getter_AddRefs(cardRow));
  NS_ENSURE_SUCCESS(rv, rv);
  
  if (!cardRow)
    return NS_OK;
    
  mdb_token token;
  GetStore()->StringToToken(GetEnv(), name, &token);
  
  rv = AddCharStringColumn(cardRow, token, NS_ConvertUCS2toUTF8(value).get());
  NS_ENSURE_SUCCESS(rv,rv);
  return NS_OK;
}

NS_IMETHODIMP nsAddrDatabase::GetCardValue(nsIAbCard *card, const char *name, PRUnichar **value)
{
  nsresult rv = NS_OK;
  
  nsCOMPtr <nsIMdbRow> cardRow;
  mdbOid rowOid;
  rowOid.mOid_Scope = m_CardRowScopeToken;
  
  // XXX todo
  // it might be that the caller always has a nsIAbMDBCard
  nsCOMPtr<nsIAbMDBCard> dbcard = do_QueryInterface(card, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  dbcard->GetDbRowID((PRUint32*)&rowOid.mOid_Id);
  
  rv = GetStore()->GetRow(GetEnv(), &rowOid, getter_AddRefs(cardRow));
  NS_ENSURE_SUCCESS(rv, rv);
  
  if (!cardRow) {
    *value = nsnull;
    // this can happen when adding cards when editing a mailing list
    return NS_OK;
  }
  
  mdb_token token;
  GetStore()->StringToToken(GetEnv(), name, &token);
  
  // XXX fix me
  // avoid extra copying and allocations (did dmb already do this on the trunk?)
  nsAutoString tempString;
  rv = GetStringColumn(cardRow, token, tempString);
  if (NS_FAILED(rv)) {
    // not all cards are going this column
    *value = nsnull;
    return NS_OK;
  }
  
  *value = nsCRT::strdup(tempString.get());
  if (!*value) 
    return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}

NS_IMETHODIMP nsAddrDatabase::EditCard(nsIAbCard *card, PRBool notify)
{
  // XXX make sure this isn't getting called when we're just editing one or two well known fields
  if (!card || !m_mdbPabTable)
    return NS_ERROR_NULL_POINTER;
  
  nsresult err = NS_OK;
  
  nsCOMPtr <nsIMdbRow> cardRow;
  mdbOid rowOid;
  rowOid.mOid_Scope = m_CardRowScopeToken;
  
  nsCOMPtr<nsIAbMDBCard> dbcard(do_QueryInterface(card, &err));
  NS_ENSURE_SUCCESS(err, err);
  dbcard->GetDbRowID((PRUint32*)&rowOid.mOid_Id);
  
  err = GetStore()->GetRow(GetEnv(), &rowOid, getter_AddRefs(cardRow));
  NS_ENSURE_SUCCESS(err, err);
  
  if (!cardRow)
    return NS_OK;
  
  err = AddAttributeColumnsToRow(card, cardRow);
  NS_ENSURE_SUCCESS(err, err);
  
  if (notify) 
    NotifyCardEntryChange(AB_NotifyPropertyChanged, card, nsnull);
  
  return NS_OK;
}

NS_IMETHODIMP nsAddrDatabase::ContainsCard(nsIAbCard *card, PRBool *hasCard)
{
    if (!card || !m_mdbPabTable)
        return NS_ERROR_NULL_POINTER;

    nsresult err = NS_OK;
    mdb_bool hasOid;
    mdbOid rowOid;
    PRBool bIsMailList;

    card->GetIsMailList(&bIsMailList);
    
    if (bIsMailList)
        rowOid.mOid_Scope = m_ListRowScopeToken;
    else
        rowOid.mOid_Scope = m_CardRowScopeToken;

    nsCOMPtr<nsIAbMDBCard> dbcard(do_QueryInterface(card, &err));
    NS_ENSURE_SUCCESS(err, err);
    dbcard->GetDbRowID((PRUint32*)&rowOid.mOid_Id);

    err = m_mdbPabTable->HasOid(GetEnv(), &rowOid, &hasOid);
    if (NS_SUCCEEDED(err))
    {
        *hasCard = hasOid;
    }

    return err;
}

NS_IMETHODIMP nsAddrDatabase::DeleteMailList(nsIAbDirectory *mailList, PRBool notify)
{
    if (!mailList || !m_mdbPabTable)
        return NS_ERROR_NULL_POINTER;

    nsresult err = NS_OK;

    // get the row
    nsIMdbRow* pListRow = nsnull;
    mdbOid rowOid;
    rowOid.mOid_Scope = m_ListRowScopeToken;

    nsCOMPtr<nsIAbMDBDirectory> dbmailList(do_QueryInterface(mailList,&err));
    NS_ENSURE_SUCCESS(err, err);
    dbmailList->GetDbRowID((PRUint32*)&rowOid.mOid_Id);

    err = GetStore()->GetRow(GetEnv(), &rowOid, &pListRow);
  NS_ENSURE_SUCCESS(err,err);

    if (!pListRow)
      return NS_OK;

    err = DeleteRow(m_mdbPabTable, pListRow);
    NS_RELEASE(pListRow);
    return err;
}

NS_IMETHODIMP nsAddrDatabase::EditMailList(nsIAbDirectory *mailList, nsIAbCard *listCard, PRBool notify)
{
    if (!mailList || !m_mdbPabTable)
        return NS_ERROR_NULL_POINTER;

    nsresult err = NS_OK;

    nsIMdbRow* pListRow = nsnull;
    mdbOid rowOid;
    rowOid.mOid_Scope = m_ListRowScopeToken;

    nsCOMPtr<nsIAbMDBDirectory> dbmailList(do_QueryInterface(mailList,&err));
    NS_ENSURE_SUCCESS(err, err);
    dbmailList->GetDbRowID((PRUint32*)&rowOid.mOid_Id);

    err = GetStore()->GetRow(GetEnv(), &rowOid, &pListRow);
  NS_ENSURE_SUCCESS(err, err);

  if (!pListRow)
    return NS_OK;

  err = AddListAttributeColumnsToRow(mailList, pListRow);
    NS_ENSURE_SUCCESS(err, err);

    if (notify)
    {
        NotifyListEntryChange(AB_NotifyPropertyChanged, mailList, nsnull);

    if (listCard) {
            NotifyCardEntryChange(AB_NotifyPropertyChanged, listCard, nsnull);
        }
    }

    NS_RELEASE(pListRow);
    return NS_OK;
}

NS_IMETHODIMP nsAddrDatabase::ContainsMailList(nsIAbDirectory *mailList, PRBool *hasList)
{
    if (!mailList || !m_mdbPabTable)
        return NS_ERROR_NULL_POINTER;

    mdb_err err = NS_OK;
    mdb_bool hasOid;
    mdbOid rowOid;

    rowOid.mOid_Scope = m_ListRowScopeToken;

    nsCOMPtr<nsIAbMDBDirectory> dbmailList(do_QueryInterface(mailList,&err));
    NS_ENSURE_SUCCESS(err, err);
    dbmailList->GetDbRowID((PRUint32*)&rowOid.mOid_Id);

    err = m_mdbPabTable->HasOid(GetEnv(), &rowOid, &hasOid);
    if (err == NS_OK)
        *hasList = hasOid;

    return (err == NS_OK) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsAddrDatabase::GetNewRow(nsIMdbRow * *newRow)
{
    mdb_err err = NS_OK;
    nsIMdbRow *row = nsnull;
    err  = GetStore()->NewRow(GetEnv(), m_CardRowScopeToken, &row);
    *newRow = row;

    return (err == NS_OK) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsAddrDatabase::GetNewListRow(nsIMdbRow * *newRow)
{
    mdb_err err = NS_OK;
    nsIMdbRow *row = nsnull;
    err  = GetStore()->NewRow(GetEnv(), m_ListRowScopeToken, &row);
    *newRow = row;

    return (err == NS_OK) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsAddrDatabase::AddCardRowToDB(nsIMdbRow *newRow)
{
    if (m_mdbPabTable)
    {
        mdb_err err = NS_OK;
        err = m_mdbPabTable->AddRow(GetEnv(), newRow);
        if (err == NS_OK)
        {
            AddRecordKeyColumnToRow(newRow);
            return NS_OK;
        }
        else
            return NS_ERROR_FAILURE;
    }
    else
        return NS_ERROR_FAILURE;
}
 
NS_IMETHODIMP nsAddrDatabase::AddLdifListMember(nsIMdbRow* listRow, const char* value)
{
    PRUint32 total = GetListAddressTotal(listRow);
    //add member
    nsCAutoString valueString(value);
    nsCAutoString email;
    PRInt32 emailPos = valueString.Find("mail=");
    emailPos += strlen("mail=");
    valueString.Right(email, valueString.Length() - emailPos);
    char* emailAddress = ToNewCString(email);
    nsIMdbRow    *cardRow = nsnull; 
    // Please DO NOT change the 3rd param of GetRowFromAttribute() call to 
    // PR_TRUE (ie, case insensitive) without reading bugs #128535 and #121478.
  nsresult result = GetRowFromAttribute(kPriEmailColumn, emailAddress, PR_FALSE /* retain case */, &cardRow);
    if (cardRow)
    {
        mdbOid outOid;
        mdb_id rowID = 0;
        if (cardRow->GetOid(GetEnv(), &outOid) == NS_OK)
            rowID = outOid.mOid_Id;

        // start from 1
        total += 1;
        mdb_token listAddressColumnToken;
        char columnStr[COLUMN_STR_MAX];
        PR_snprintf(columnStr, COLUMN_STR_MAX, kMailListAddressFormat, total); 
        GetStore()->StringToToken(GetEnv(), columnStr, &listAddressColumnToken);
        result = AddIntColumn(listRow, listAddressColumnToken, rowID);

        SetListAddressTotal(listRow, total);
            NS_RELEASE(cardRow);
    }
    if (emailAddress)
        Recycle(emailAddress);
    return NS_OK;
}
 

void nsAddrDatabase::GetCharStringYarn(char* str, struct mdbYarn* strYarn)
{
    strYarn->mYarn_Grow = nsnull;
    strYarn->mYarn_Buf = str;
    strYarn->mYarn_Size = PL_strlen((const char *) strYarn->mYarn_Buf) + 1;
    strYarn->mYarn_Fill = strYarn->mYarn_Size - 1;
    strYarn->mYarn_Form = 0;
}

void nsAddrDatabase::GetStringYarn(nsString* str, struct mdbYarn* strYarn)
{
    strYarn->mYarn_Buf = ToNewCString(*str);
    strYarn->mYarn_Size = PL_strlen((const char *) strYarn->mYarn_Buf) + 1;
    strYarn->mYarn_Fill = strYarn->mYarn_Size - 1;
    strYarn->mYarn_Form = 0;     
}

void nsAddrDatabase::GetIntYarn(PRUint32 nValue, struct mdbYarn* intYarn)
{
    intYarn->mYarn_Size = sizeof(intYarn->mYarn_Buf);
    intYarn->mYarn_Fill = intYarn->mYarn_Size;
    intYarn->mYarn_Form = 0;
    intYarn->mYarn_Grow = nsnull;

    PR_snprintf((char*)intYarn->mYarn_Buf, intYarn->mYarn_Size, "%lx", nValue);
    intYarn->mYarn_Fill = PL_strlen((const char *) intYarn->mYarn_Buf);
}

nsresult nsAddrDatabase::AddCharStringColumn(nsIMdbRow* cardRow, mdb_column inColumn, const char* str)
{
    struct mdbYarn yarn;

    GetCharStringYarn((char *) str, &yarn);
    mdb_err err = cardRow->AddColumn(GetEnv(),  inColumn, &yarn);

    return (err == NS_OK) ? NS_OK : NS_ERROR_FAILURE;
}

nsresult nsAddrDatabase::AddStringColumn(nsIMdbRow* cardRow, mdb_column inColumn, nsString* str)
{
    struct mdbYarn yarn;

    GetStringYarn(str, &yarn);
    mdb_err err = cardRow->AddColumn(GetEnv(),  inColumn, &yarn);

    return (err == NS_OK) ? NS_OK : NS_ERROR_FAILURE;
}

nsresult nsAddrDatabase::AddIntColumn(nsIMdbRow* cardRow, mdb_column inColumn, PRUint32 nValue)
{
    struct mdbYarn yarn;
    char    yarnBuf[100];

    yarn.mYarn_Buf = (void *) yarnBuf;
    GetIntYarn(nValue, &yarn);
    mdb_err err = cardRow->AddColumn(GetEnv(),  inColumn, &yarn);

    return (err == NS_OK) ? NS_OK : NS_ERROR_FAILURE;
}

nsresult nsAddrDatabase::AddBoolColumn(nsIMdbRow* cardRow, mdb_column inColumn, PRBool bValue)
{
    struct mdbYarn yarn;
    char    yarnBuf[100];

    yarn.mYarn_Buf = (void *) yarnBuf;
    if (bValue)
        GetIntYarn(1, &yarn);
    else
        GetIntYarn(0, &yarn);
    mdb_err err = cardRow->AddColumn(GetEnv(), inColumn, &yarn);

    return (err == NS_OK) ? NS_OK : NS_ERROR_FAILURE;
}

nsresult nsAddrDatabase::GetStringColumn(nsIMdbRow *cardRow, mdb_token outToken, nsString& str)
{
  nsresult    err = NS_ERROR_FAILURE;
  nsIMdbCell    *cardCell;
  
  if (cardRow)    
  {
    err = cardRow->GetCell(GetEnv(), outToken, &cardCell);
    if (err == NS_OK && cardCell)
    {
      struct mdbYarn yarn;
      cardCell->AliasYarn(GetEnv(), &yarn);
      NS_ConvertUTF8toUCS2 uniStr((const char*) yarn.mYarn_Buf, yarn.mYarn_Fill);
      if (uniStr.Length() > 0)
        str.Assign(uniStr);
      else
        err = NS_ERROR_FAILURE;
      cardCell->Release(); // always release ref
    }
    else
      err = NS_ERROR_FAILURE;
  }
  return err;
}

void nsAddrDatabase::YarnToUInt32(struct mdbYarn *yarn, PRUint32 *pResult)
{
    PRUint32 i, result, numChars;
    char *p = (char *) yarn->mYarn_Buf;
    if (yarn->mYarn_Fill > 8)
        numChars = 8;
    else
        numChars = yarn->mYarn_Fill;
    for (i=0, result = 0; i < numChars; i++, p++)
    {
        char C = *p;

        PRInt8 unhex = ((C >= '0' && C <= '9') ? C - '0' :
            ((C >= 'A' && C <= 'F') ? C - 'A' + 10 :
             ((C >= 'a' && C <= 'f') ? C - 'a' + 10 : -1)));
        if (unhex < 0)
            break;
        result = (result << 4) | unhex;
    }
    
    *pResult = result;
}

nsresult nsAddrDatabase::GetIntColumn
(nsIMdbRow *cardRow, mdb_token outToken, PRUint32* pValue, PRUint32 defaultValue)
{
    nsresult    err = NS_ERROR_FAILURE;
    nsIMdbCell    *cardCell;

    if (pValue)
        *pValue = defaultValue;
    if (cardRow)
    {
        err = cardRow->GetCell(GetEnv(), outToken, &cardCell);
        if (err == NS_OK && cardCell)
        {
            struct mdbYarn yarn;
            cardCell->AliasYarn(GetEnv(), &yarn);
            YarnToUInt32(&yarn, pValue);
            cardCell->Release();
        }
        else
            err = NS_ERROR_FAILURE;
    }
    return err;
}

nsresult nsAddrDatabase::GetBoolColumn(nsIMdbRow *cardRow, mdb_token outToken, PRBool* pValue)
{
    nsresult    err = NS_ERROR_FAILURE;
    nsIMdbCell    *cardCell;
    PRUint32 nValue = 0;

    if (cardRow)
    {
        err = cardRow->GetCell(GetEnv(), outToken, &cardCell);
        if (err == NS_OK && cardCell)
        {
            struct mdbYarn yarn;
            cardCell->AliasYarn(GetEnv(), &yarn);
            YarnToUInt32(&yarn, &nValue);
            cardCell->Release();
        }
        else
            err = NS_ERROR_FAILURE;
    }
    if (nValue == 0)
        *pValue = PR_FALSE;
    else
        *pValue = PR_TRUE;
    return err;
}

/*  value if UTF8 string */
NS_IMETHODIMP nsAddrDatabase::AddPrimaryEmail(nsIMdbRow * row, const char * value)
{
    if (!value)
        return NS_ERROR_NULL_POINTER;

    nsresult err = NS_OK;
    err = AddCharStringColumn(row, m_PriEmailColumnToken, value);

    if (NS_SUCCEEDED(err))
    {
        err = AddLowercaseColumn(row, m_LowerPriEmailColumnToken, value);
    }
    return err;
}

/*  value if UTF8 string */
NS_IMETHODIMP nsAddrDatabase::AddListName(nsIMdbRow * row, const char * value)
{
    if (!value)
        return NS_ERROR_NULL_POINTER;

    nsresult err = NS_OK;
    err = AddCharStringColumn(row, m_ListNameColumnToken, value);
    if (NS_SUCCEEDED(err))
    {
        err = AddLowercaseColumn(row, m_LowerListNameColumnToken, value);
    }
    return err;
}

/* 
value is UTF8 string, need to convert back to lowercase unicode then 
back to UTF8 string
*/
nsresult nsAddrDatabase::AddLowercaseColumn
(nsIMdbRow * row, mdb_token columnToken, const char* columnValue)
{
  nsresult err = NS_OK;
  if (columnValue)
  {
    nsAutoString newUnicodeString(NS_ConvertUTF8toUCS2(columnValue).get());
    ToLowerCase(newUnicodeString);
    char * utf8Str = ToNewUTF8String(newUnicodeString);
    if (utf8Str)
    {
      err = AddCharStringColumn(row, columnToken, utf8Str);
      Recycle(utf8Str);
    }    
  }
  return err;
}

nsresult nsAddrDatabase::GetCardFromDB(nsIAbCard *newCard, nsIMdbRow* cardRow)
{
    nsresult    err = NS_OK;
    if (!newCard || !cardRow)
        return NS_ERROR_NULL_POINTER;

  nsAutoString tempString;

  // FIX ME
  // there is no reason to set / copy all these attributes on the card, when we'll never even
  // ask for them.
    err = GetStringColumn(cardRow, m_FirstNameColumnToken, tempString);
    if (NS_SUCCEEDED(err) && tempString.Length())
    {
        newCard->SetFirstName(tempString.get());
    }

    err = GetStringColumn(cardRow, m_LastNameColumnToken, tempString);
    if (NS_SUCCEEDED(err) && tempString.Length())
    {
        newCard->SetLastName(tempString.get());
    }

    err = GetStringColumn(cardRow, m_DisplayNameColumnToken, tempString);
    if (NS_SUCCEEDED(err) && tempString.Length())
    {
        newCard->SetDisplayName(tempString.get());
    }

    err = GetStringColumn(cardRow, m_NickNameColumnToken, tempString);
    if (NS_SUCCEEDED(err) && tempString.Length())
    {
        newCard->SetNickName(tempString.get());
    }

    err = GetStringColumn(cardRow, m_PriEmailColumnToken, tempString);
    if (NS_SUCCEEDED(err) && tempString.Length())
    {
        newCard->SetPrimaryEmail(tempString.get());
    }

    err = GetStringColumn(cardRow, m_2ndEmailColumnToken, tempString);
    if (NS_SUCCEEDED(err) && tempString.Length())
    {
        newCard->SetSecondEmail(tempString.get());
    }

    PRUint32 format = nsIAbPreferMailFormat::unknown;
    err = GetIntColumn(cardRow, m_MailFormatColumnToken, &format, 0);
    if (NS_SUCCEEDED(err))
        newCard->SetPreferMailFormat(format);

    err = GetStringColumn(cardRow, m_WorkPhoneColumnToken, tempString);
    if (NS_SUCCEEDED(err) && tempString.Length())
    {
        newCard->SetWorkPhone(tempString.get());
    }

    err = GetStringColumn(cardRow, m_HomePhoneColumnToken, tempString);
    if (NS_SUCCEEDED(err) && tempString.Length())
    {
        newCard->SetHomePhone(tempString.get());
    }

    err = GetStringColumn(cardRow, m_FaxColumnToken, tempString);
    if (NS_SUCCEEDED(err) && tempString.Length())
    {
        newCard->SetFaxNumber(tempString.get());
    }

    err = GetStringColumn(cardRow, m_PagerColumnToken, tempString);
    if (NS_SUCCEEDED(err) && tempString.Length())
    {
        newCard->SetPagerNumber(tempString.get());
    }

    err = GetStringColumn(cardRow, m_CellularColumnToken, tempString);
    if (NS_SUCCEEDED(err) && tempString.Length())
    {
        newCard->SetCellularNumber(tempString.get());
    }

    err = GetStringColumn(cardRow, m_HomeAddressColumnToken, tempString);
    if (NS_SUCCEEDED(err) && tempString.Length())
    {
        newCard->SetHomeAddress(tempString.get());
    }

    err = GetStringColumn(cardRow, m_HomeAddress2ColumnToken, tempString);
    if (NS_SUCCEEDED(err) && tempString.Length())
    {
        newCard->SetHomeAddress2(tempString.get());
    }

    err = GetStringColumn(cardRow, m_HomeCityColumnToken, tempString);
    if (NS_SUCCEEDED(err) && tempString.Length())
    {
        newCard->SetHomeCity(tempString.get());
    }

    err = GetStringColumn(cardRow, m_HomeStateColumnToken, tempString);
    if (NS_SUCCEEDED(err) && tempString.Length())
    {
        newCard->SetHomeState(tempString.get());
    }

    err = GetStringColumn(cardRow, m_HomeZipCodeColumnToken, tempString);
    if (NS_SUCCEEDED(err) && tempString.Length())
    {
        newCard->SetHomeZipCode(tempString.get());
    }

    err = GetStringColumn(cardRow, m_HomeCountryColumnToken, tempString);
    if (NS_SUCCEEDED(err) && tempString.Length())
    {
        newCard->SetHomeCountry(tempString.get());
    }

    err = GetStringColumn(cardRow, m_WorkAddressColumnToken, tempString);
    if (NS_SUCCEEDED(err) && tempString.Length())
    {
        newCard->SetWorkAddress(tempString.get());
    }

    err = GetStringColumn(cardRow, m_WorkAddress2ColumnToken, tempString);
    if (NS_SUCCEEDED(err) && tempString.Length())
    {
        newCard->SetWorkAddress2(tempString.get());
    }

    err = GetStringColumn(cardRow, m_WorkCityColumnToken, tempString);
    if (NS_SUCCEEDED(err) && tempString.Length())
    {
        newCard->SetWorkCity(tempString.get());
    }

    err = GetStringColumn(cardRow, m_WorkStateColumnToken, tempString);
    if (NS_SUCCEEDED(err) && tempString.Length())
    {
        newCard->SetWorkState(tempString.get());
    }

    err = GetStringColumn(cardRow, m_WorkZipCodeColumnToken, tempString);
    if (NS_SUCCEEDED(err) && tempString.Length())
    {
        newCard->SetWorkZipCode(tempString.get());
    }

    err = GetStringColumn(cardRow, m_WorkCountryColumnToken, tempString);
    if (NS_SUCCEEDED(err) && tempString.Length())
    {
        newCard->SetWorkCountry(tempString.get());
    }

    err = GetStringColumn(cardRow, m_JobTitleColumnToken, tempString);
    if (NS_SUCCEEDED(err) && tempString.Length())
    {
        newCard->SetJobTitle(tempString.get());
    }

    err = GetStringColumn(cardRow, m_DepartmentColumnToken, tempString);
    if (NS_SUCCEEDED(err) && tempString.Length())
    {
        newCard->SetDepartment(tempString.get());
    }

    err = GetStringColumn(cardRow, m_CompanyColumnToken, tempString);
    if (NS_SUCCEEDED(err) && tempString.Length())
    {
        newCard->SetCompany(tempString.get());
    }

    err = GetStringColumn(cardRow, m_WebPage1ColumnToken, tempString);
    if (NS_SUCCEEDED(err) && tempString.Length())
    {
        newCard->SetWebPage1(tempString.get());
    }

    err = GetStringColumn(cardRow, m_WebPage2ColumnToken, tempString);
    if (NS_SUCCEEDED(err) && tempString.Length())
    {
        newCard->SetWebPage2(tempString.get());
    }

    err = GetStringColumn(cardRow, m_BirthYearColumnToken, tempString);
    if (NS_SUCCEEDED(err) && tempString.Length())
    {
        newCard->SetBirthYear(tempString.get());
    }

    err = GetStringColumn(cardRow, m_BirthMonthColumnToken, tempString);
    if (NS_SUCCEEDED(err) && tempString.Length())
    {
        newCard->SetBirthMonth(tempString.get());
    }

    err = GetStringColumn(cardRow, m_BirthDayColumnToken, tempString);
    if (NS_SUCCEEDED(err) && tempString.Length())
    {
        newCard->SetBirthDay(tempString.get());
    }

    err = GetStringColumn(cardRow, m_Custom1ColumnToken, tempString);
    if (NS_SUCCEEDED(err) && tempString.Length())
    {
        newCard->SetCustom1(tempString.get());
    }

    err = GetStringColumn(cardRow, m_Custom2ColumnToken, tempString);
    if (NS_SUCCEEDED(err) && tempString.Length())
    {
        newCard->SetCustom2(tempString.get());
    }

    err = GetStringColumn(cardRow, m_Custom3ColumnToken, tempString);
    if (NS_SUCCEEDED(err) && tempString.Length())
    {
        newCard->SetCustom3(tempString.get());
    }

    err = GetStringColumn(cardRow, m_Custom4ColumnToken, tempString);
    if (NS_SUCCEEDED(err) && tempString.Length())
    {
        newCard->SetCustom4(tempString.get());
    }

    err = GetStringColumn(cardRow, m_NotesColumnToken, tempString);
    if (NS_SUCCEEDED(err) && tempString.Length())
    {
        newCard->SetNotes(tempString.get());
    }
    PRUint32 key = 0;
    err = GetIntColumn(cardRow, m_RecordKeyColumnToken, &key, 0);
    if (NS_SUCCEEDED(err))
    {
        nsCOMPtr<nsIAbMDBCard> dbnewCard(do_QueryInterface(newCard, &err));
        if (NS_SUCCEEDED(err) && dbnewCard)
            dbnewCard->SetKey(key);
    }

    return err;
}

nsresult nsAddrDatabase::GetListCardFromDB(nsIAbCard *listCard, nsIMdbRow* listRow)
{
    nsresult    err = NS_OK;
    if (!listCard || !listRow)
        return NS_ERROR_NULL_POINTER;

    nsAutoString tempString;

    err = GetStringColumn(listRow, m_ListNameColumnToken, tempString);
    if (NS_SUCCEEDED(err) && tempString.Length())
    {
        listCard->SetDisplayName(tempString.get());
        listCard->SetLastName(tempString.get());
    }
    err = GetStringColumn(listRow, m_ListNickNameColumnToken, tempString);
    if (NS_SUCCEEDED(err) && tempString.Length())
    {
        listCard->SetNickName(tempString.get());
    }
    err = GetStringColumn(listRow, m_ListDescriptionColumnToken, tempString);
    if (NS_SUCCEEDED(err) && tempString.Length())
    {
        listCard->SetNotes(tempString.get());
    }
    PRUint32 key = 0;
    err = GetIntColumn(listRow, m_RecordKeyColumnToken, &key, 0);
    if (NS_SUCCEEDED(err))
    {
        nsCOMPtr<nsIAbMDBCard> dblistCard(do_QueryInterface(listCard, &err));
        if (NS_SUCCEEDED(err) && dblistCard)
            dblistCard->SetKey(key);
    }
    return err;
}

nsresult nsAddrDatabase::GetListFromDB(nsIAbDirectory *newList, nsIMdbRow* listRow)
{
    nsresult    err = NS_OK;
    if (!newList || !listRow)
        return NS_ERROR_NULL_POINTER;

    nsAutoString tempString;

    err = GetStringColumn(listRow, m_ListNameColumnToken, tempString);
    if (NS_SUCCEEDED(err) && tempString.Length())
    {
        newList->SetDirName(tempString.get());
    }
    err = GetStringColumn(listRow, m_ListNickNameColumnToken, tempString);
    if (NS_SUCCEEDED(err) && tempString.Length())
    {
        newList->SetListNickName(tempString.get());
    }
    err = GetStringColumn(listRow, m_ListDescriptionColumnToken, tempString);
    if (NS_SUCCEEDED(err) && tempString.Length())
    {
        newList->SetDescription(tempString.get());
    }

    PRUint32 totalAddress = GetListAddressTotal(listRow);
    PRUint32 pos;
    for (pos = 1; pos <= totalAddress; pos++)
    {
        mdb_token listAddressColumnToken;
        mdb_id rowID;

        char columnStr[COLUMN_STR_MAX];
        PR_snprintf(columnStr, COLUMN_STR_MAX, kMailListAddressFormat, pos);
        GetStore()->StringToToken(GetEnv(), columnStr, &listAddressColumnToken);

        nsCOMPtr <nsIMdbRow> cardRow;
        err = GetIntColumn(listRow, listAddressColumnToken, (PRUint32*)&rowID, 0);
        err = GetCardRowByRowID(rowID, getter_AddRefs(cardRow));
        
        if (cardRow)
        {
            nsCOMPtr<nsIAbCard> card;
            err = CreateABCard(cardRow, 0, getter_AddRefs(card));

            nsCOMPtr<nsIAbMDBDirectory>
            dbnewList(do_QueryInterface(newList, &err));
            if(NS_SUCCEEDED(err))
                dbnewList->AddAddressToList(card);
        }
//        NS_IF_ADDREF(card);
    }

    return err;
}

class nsAddrDBEnumerator : public nsIEnumerator 
{
public:
    NS_DECL_ISUPPORTS

    // nsIEnumerator methods:
    NS_DECL_NSIENUMERATOR

    // nsAddrDBEnumerator methods:

    nsAddrDBEnumerator(nsAddrDatabase* db);
    virtual ~nsAddrDBEnumerator();

protected:
    nsCOMPtr<nsAddrDatabase>    mDB;
    nsCOMPtr<nsIAbDirectory>    mResultList;
    nsCOMPtr<nsIAbCard>            mResultCard;
    nsIMdbTable*                mDbTable;
    nsIMdbTableRowCursor*       mRowCursor;
    nsIMdbRow*                    mCurrentRow;
     mdb_pos                        mRowPos;
    PRBool                      mDone;
    PRBool                      mCurrentRowIsList;
};

nsAddrDBEnumerator::nsAddrDBEnumerator(nsAddrDatabase* db)
    : mDB(db), mRowCursor(nsnull), mCurrentRow(nsnull), mDone(PR_FALSE)
{
    NS_INIT_ISUPPORTS();
    mDbTable = mDB->GetPabTable();
    mCurrentRowIsList = PR_FALSE;
}

nsAddrDBEnumerator::~nsAddrDBEnumerator()
{
  NS_IF_RELEASE(mRowCursor);
}

NS_IMPL_ISUPPORTS1(nsAddrDBEnumerator, nsIEnumerator)

NS_IMETHODIMP nsAddrDBEnumerator::First(void)
{
    nsresult rv = 0;
    mDone = PR_FALSE;

    if (!mDB || !mDbTable || !mDB->GetEnv())
        return NS_ERROR_NULL_POINTER;
        
    mDbTable->GetTableRowCursor(mDB->GetEnv(), -1, &mRowCursor);
    if (rv != NS_OK) return NS_ERROR_FAILURE;
    return Next();
}

NS_IMETHODIMP nsAddrDBEnumerator::Next(void)
{
    if (!mRowCursor)
    {
        mDone = PR_TRUE;
        return NS_ERROR_FAILURE;
    }
        NS_IF_RELEASE(mCurrentRow);
    nsresult rv = mRowCursor->NextRow(mDB->GetEnv(), &mCurrentRow, &mRowPos);
    if (mCurrentRow && NS_SUCCEEDED(rv))
    {
        mdbOid rowOid;

        if (mCurrentRow->GetOid(mDB->GetEnv(), &rowOid) == NS_OK)
        {
            if (mDB->IsListRowScopeToken(rowOid.mOid_Scope))
            {
                mCurrentRowIsList = PR_TRUE;
                return NS_OK;
            }
            else if (mDB->IsCardRowScopeToken(rowOid.mOid_Scope))
            {
                mCurrentRowIsList = PR_FALSE;
                return NS_OK;
            }
            else if (mDB->IsDataRowScopeToken(rowOid.mOid_Scope))
            {
                return Next();
            }
            else
                return NS_ERROR_FAILURE;
        }
    }
    else if (!mCurrentRow) 
    {
        mDone = PR_TRUE;
        return NS_ERROR_NULL_POINTER;
    }
    else if (NS_FAILED(rv)) {
        mDone = PR_TRUE;
        return NS_ERROR_FAILURE;
    }
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsAddrDBEnumerator::CurrentItem(nsISupports **aItem)
{
    if (mCurrentRow) 
    {
        nsresult rv;
        if (mCurrentRowIsList)
        {
            rv = mDB->CreateABListCard(mCurrentRow, getter_AddRefs(mResultCard));
            *aItem = mResultCard;
        }
        else
        {
            rv = mDB->CreateABCard(mCurrentRow, 0, getter_AddRefs(mResultCard));
            *aItem = mResultCard;
        }
        NS_IF_ADDREF(*aItem);
        return NS_OK;
    }
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsAddrDBEnumerator::IsDone(void)
{
    return mDone ? NS_OK : NS_ERROR_FAILURE;
}

class nsListAddressEnumerator : public nsIEnumerator 
{
public:
    NS_DECL_ISUPPORTS

    // nsIEnumerator methods:
    NS_DECL_NSIENUMERATOR

    // nsAddrDBEnumerator methods:

    nsListAddressEnumerator(nsAddrDatabase* db, mdb_id rowID);
    virtual ~nsListAddressEnumerator();

protected:
    nsCOMPtr<nsAddrDatabase>    mDB;
    nsCOMPtr<nsIAbCard>            mResultCard;
    nsIMdbTable*                mDbTable;
    nsIMdbRow*                    mListRow;
    nsIMdbRow*                    mCurrentRow;
    mdb_id                        mListRowID;
    PRBool                      mDone;
    PRUint32                    mAddressTotal;
    PRUint16                    mAddressPos;
};

nsListAddressEnumerator::nsListAddressEnumerator(nsAddrDatabase* db, mdb_id rowID)
    : mDB(db), mCurrentRow(nsnull), mListRowID(rowID), mDone(PR_FALSE)
{
    NS_INIT_ISUPPORTS();
    mDbTable = mDB->GetPabTable();
    mDB->GetListRowByRowID(mListRowID, &mListRow);
    mAddressTotal = mDB->GetListAddressTotal(mListRow);
    mAddressPos = 0;
}

nsListAddressEnumerator::~nsListAddressEnumerator()
{
  NS_IF_RELEASE(mListRow);
}

NS_IMPL_ISUPPORTS1(nsListAddressEnumerator, nsIEnumerator)

NS_IMETHODIMP nsListAddressEnumerator::First(void)
{
    mDone = PR_FALSE;

    if (!mDB || !mDbTable || !mDB->GetEnv())
        return NS_ERROR_NULL_POINTER;
    
    //got total address count and start
    if (mAddressTotal)
        return Next();
    else
        return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsListAddressEnumerator::Next(void)
{
    //go to the next address column
    if (mCurrentRow)
    {
                NS_RELEASE(mCurrentRow);
        mCurrentRow = nsnull;
    }
    mAddressPos++;
    if (mAddressPos <= mAddressTotal)
    {
        nsresult err;
        err = mDB->GetAddressRowByPos(mListRow, mAddressPos, &mCurrentRow);
        if (mCurrentRow)
            return NS_OK;
        else
        {
            mDone = PR_TRUE;
            return NS_ERROR_FAILURE;
        }
    }
    else
    {
        mDone = PR_TRUE;
        return NS_ERROR_FAILURE;
    }
}

NS_IMETHODIMP nsListAddressEnumerator::CurrentItem(nsISupports **aItem)
{
  if (mCurrentRow) 
  {
    nsresult rv;
    rv = mDB->CreateABCard(mCurrentRow, mListRowID, getter_AddRefs(mResultCard));
    NS_IF_ADDREF(*aItem = mResultCard);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsListAddressEnumerator::IsDone(void)
{
    return mDone ? NS_OK : NS_ERROR_FAILURE;
}

////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP nsAddrDatabase::EnumerateCards(nsIAbDirectory *directory, nsIEnumerator **result)
{
    nsAddrDBEnumerator* e = new nsAddrDBEnumerator(this);
    m_dbDirectory = directory;
    if (!e)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(e);
    *result = e;
    return NS_OK;
}

NS_IMETHODIMP nsAddrDatabase::GetMailingListsFromDB(nsIAbDirectory *parentDir)
{
    nsCOMPtr<nsIAbDirectory>    resultList;
    nsIMdbTable*                dbTable = nsnull;
    nsIMdbTableRowCursor*       rowCursor = nsnull;
    nsCOMPtr<nsIMdbRow> currentRow;
     mdb_pos                        rowPos;
    PRBool                      done = PR_FALSE;

    m_dbDirectory = parentDir;
    dbTable = GetPabTable();

    if (!dbTable)
        return NS_ERROR_FAILURE;

    dbTable->GetTableRowCursor(GetEnv(), -1, &rowCursor);
    if (!rowCursor)
        return NS_ERROR_FAILURE;

    while (!done)
    {
                nsresult rv = rowCursor->NextRow(GetEnv(), getter_AddRefs(currentRow), &rowPos);
        if (currentRow && NS_SUCCEEDED(rv))
        {
            mdbOid rowOid;

            if (currentRow->GetOid(GetEnv(), &rowOid) == NS_OK)
            {
                if (IsListRowScopeToken(rowOid.mOid_Scope))
                    rv = CreateABList(currentRow, getter_AddRefs(resultList));
            }
        }
        else
            done = PR_TRUE;
    }
        NS_IF_RELEASE(rowCursor);
    return NS_OK;
}

NS_IMETHODIMP nsAddrDatabase::EnumerateListAddresses(nsIAbDirectory *directory, nsIEnumerator **result)
{
    nsresult rv = NS_OK; 
    mdb_id rowID;

    nsCOMPtr<nsIAbMDBDirectory> dbdirectory(do_QueryInterface(directory,&rv));
    
    if(NS_SUCCEEDED(rv))
    {
    dbdirectory->GetDbRowID((PRUint32*)&rowID);

    nsListAddressEnumerator* e = new nsListAddressEnumerator(this, rowID);
    m_dbDirectory = directory;
    if (!e)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(e);
    *result = e;
    }
    return rv;
}

nsresult nsAddrDatabase::CreateCard(nsIMdbRow* cardRow, mdb_id listRowID, nsIAbCard **result)
{
    nsresult rv = NS_OK; 

    mdbOid outOid;
    mdb_id rowID=0;

    if (cardRow->GetOid(GetEnv(), &outOid) == NS_OK)
        rowID = outOid.mOid_Id;

    if(NS_SUCCEEDED(rv))
    {
        nsCOMPtr<nsIAbCard> personCard;
      personCard = do_CreateInstance(NS_ABMDBCARD_CONTRACTID, &rv);
      NS_ENSURE_SUCCESS(rv,rv);

        nsCOMPtr<nsIAbMDBCard> dbpersonCard (do_QueryInterface(personCard, &rv));

        if (NS_SUCCEEDED(rv) && dbpersonCard)
        {
            GetCardFromDB(personCard, cardRow);
            mdbOid tableOid;
            m_mdbPabTable->GetOid(GetEnv(), &tableOid);

            dbpersonCard->SetDbTableID(tableOid.mOid_Id);
            dbpersonCard->SetDbRowID(rowID);
            dbpersonCard->SetAbDatabase(this);
        }

        NS_IF_ADDREF(*result = personCard);
    }

    return rv;
}

nsresult nsAddrDatabase::CreateABCard(nsIMdbRow* cardRow, mdb_id listRowID, nsIAbCard **result)
{
    return CreateCard(cardRow, listRowID, result);
}

/* create a card for mailing list in the address book */
nsresult nsAddrDatabase::CreateABListCard(nsIMdbRow* listRow, nsIAbCard **result)
{
    nsresult rv = NS_OK; 

    mdbOid outOid;
    mdb_id rowID=0;

    if (listRow->GetOid(GetEnv(), &outOid) == NS_OK)
        rowID = outOid.mOid_Id;

    char* listURI = nsnull;
    char* file = nsnull;
    file = m_dbName.GetLeafName();
    listURI = PR_smprintf("%s%s/MailList%ld", kMDBDirectoryRoot, file, rowID);

    nsCOMPtr<nsIAbCard> personCard;
    nsCOMPtr<nsIAbMDBDirectory> dbm_dbDirectory(do_QueryInterface(m_dbDirectory, &rv));
    if(NS_SUCCEEDED(rv) && dbm_dbDirectory)
    {
    personCard = do_CreateInstance(NS_ABMDBCARD_CONTRACTID, &rv);
        NS_ENSURE_SUCCESS(rv,rv);

        if (personCard)
        {
            GetListCardFromDB(personCard, listRow);
            mdbOid tableOid;
            m_mdbPabTable->GetOid(GetEnv(), &tableOid);
    
            nsCOMPtr<nsIAbMDBCard> dbpersonCard(do_QueryInterface(personCard, &rv));
      if (NS_SUCCEEDED(rv) && dbpersonCard)
      {
              dbpersonCard->SetDbTableID(tableOid.mOid_Id);
              dbpersonCard->SetDbRowID(rowID);
              dbpersonCard->SetAbDatabase(this);
      }
            personCard->SetIsMailList(PR_TRUE);
            personCard->SetMailListURI(listURI);
        }
        
        NS_IF_ADDREF(*result = personCard);
    }
    if (file)
        nsCRT::free(file);
    if (listURI)
        PR_smprintf_free(listURI);

    return rv;
}

/* create a sub directory for mailing list in the address book left pane */
nsresult nsAddrDatabase::CreateABList(nsIMdbRow* listRow, nsIAbDirectory **result)
{
    nsresult rv = NS_OK; 

    if (!listRow)
        return NS_ERROR_NULL_POINTER;

    mdbOid outOid;
    mdb_id rowID=0;

    if (listRow->GetOid(GetEnv(), &outOid) == NS_OK)
        rowID = outOid.mOid_Id;

    char* listURI = nsnull;
    char* file = nsnull;

    file = m_dbName.GetLeafName();
    listURI = PR_smprintf("%s%s/MailList%ld", kMDBDirectoryRoot, file, rowID);

    nsCOMPtr<nsIAbDirectory> mailList;
    nsCOMPtr<nsIAbMDBDirectory> dbm_dbDirectory(do_QueryInterface(m_dbDirectory, &rv));
    if(NS_SUCCEEDED(rv) && dbm_dbDirectory)
    {
        rv = dbm_dbDirectory->AddDirectory(listURI, getter_AddRefs(mailList));

        nsCOMPtr<nsIAbMDBDirectory> dbmailList (do_QueryInterface(mailList, &rv));

        if (mailList)
    {
            // if we are using turbo, and we "exit" and restart with the same profile
            // the current mailing list will still be in memory, so when we do
            // GetResource() and QI, we'll get it again.
            // in that scenario, the mailList that we pass in will already be
            // be a mailing list, with a valid row and all the entries
            // in that scenario, we can skip GetListFromDB(), which would have
            // have added all the cards to the list again.
            // see bug #134743
            mdb_id existingID;
            dbmailList->GetDbRowID(&existingID);
            if (existingID != rowID) {
            GetListFromDB(mailList, listRow);
            dbmailList->SetDbRowID(rowID);
            mailList->SetIsMailList(PR_TRUE);
            }

            dbm_dbDirectory->AddMailListToDirectory(mailList);
            NS_IF_ADDREF(*result = mailList);
        }
    }

    if (file)
        nsCRT::free(file);
    if (listURI)
        PR_smprintf_free(listURI);

    return rv;
}

nsresult nsAddrDatabase::GetCardRowByRowID(mdb_id rowID, nsIMdbRow **dbRow)
{
    mdbOid rowOid;
    rowOid.mOid_Scope = m_CardRowScopeToken;
    rowOid.mOid_Id = rowID;

    return GetStore()->GetRow(GetEnv(), &rowOid, dbRow);
}

nsresult nsAddrDatabase::GetListRowByRowID(mdb_id rowID, nsIMdbRow **dbRow)
{
    mdbOid rowOid;
    rowOid.mOid_Scope = m_ListRowScopeToken;
    rowOid.mOid_Id = rowID;

    return GetStore()->GetRow(GetEnv(), &rowOid, dbRow);
}

nsresult nsAddrDatabase::GetRowFromAttribute(const char *aName, const char *aUTF8Value, PRBool aCaseInsensitive, nsIMdbRow    **aCardRow)
{
  NS_ENSURE_ARG_POINTER(aName);
  NS_ENSURE_ARG_POINTER(aUTF8Value);
  NS_ENSURE_ARG_POINTER(aCardRow);
  
  nsresult rv = NS_OK;
  
  mdb_token token;
  GetStore()->StringToToken(GetEnv(), aName, &token);
  
  NS_ConvertUTF8toUCS2 newUnicodeString(aUTF8Value);

  if (aCaseInsensitive)
    ToLowerCase(newUnicodeString);
  
  char * pUTF8Str = ToNewUTF8String(newUnicodeString);
  if (pUTF8Str)
  {
    rv = GetRowForCharColumn(pUTF8Str, token, PR_TRUE, aCardRow);
    Recycle(pUTF8Str);
  }    

  return rv;
}

NS_IMETHODIMP nsAddrDatabase::GetCardFromAttribute(nsIAbDirectory *aDirectory, const char *aName, const char *aUTF8Value, PRBool aCaseInsensitive, nsIAbCard **aCardResult)
{
  nsresult rv = NS_OK;
  NS_ENSURE_ARG_POINTER(aCardResult);
  
  m_dbDirectory = aDirectory;
  nsIMdbRow    *cardRow = nsnull;
  
  nsresult result = GetRowFromAttribute(aName, aUTF8Value, aCaseInsensitive, &cardRow);
  
  if (NS_SUCCEEDED(result) && cardRow)
  {
    rv = CreateABCard(cardRow, 0, aCardResult);
    NS_RELEASE(cardRow);
  }
  else
    *aCardResult = nsnull;
  return rv;
}

NS_IMETHODIMP nsAddrDatabase::GetDirectoryName(PRUnichar **name)
{
    if (m_dbDirectory && name)
    {
        return m_dbDirectory->GetDirName(name);
    }
    else
        return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsAddrDatabase::AddListDirNode(nsIMdbRow * listRow)
{
    nsresult rv = NS_OK;

    nsCOMPtr<nsIProxyObjectManager> proxyMgr = 
             do_GetService(NS_XPCOMPROXY_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

  static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
    NS_WITH_PROXIED_SERVICE(nsIRDFService, rdfService, kRDFServiceCID, NS_UI_THREAD_EVENTQ, &rv);
    if (NS_SUCCEEDED(rv)) 
    {
        nsCOMPtr<nsIRDFResource>    parentResource;

        char* file = m_dbName.GetLeafName();
        char *parentUri = PR_smprintf("%s%s", kMDBDirectoryRoot, file);
        rv = rdfService->GetResource( parentUri, getter_AddRefs(parentResource));
        nsCOMPtr<nsIAbDirectory> parentDir;
        rv = proxyMgr->GetProxyForObject( NS_UI_THREAD_EVENTQ, NS_GET_IID( nsIAbDirectory),
                                    parentResource, PROXY_SYNC | PROXY_ALWAYS, getter_AddRefs( parentDir));
        if (parentDir)
        {
            m_dbDirectory = parentDir;
            nsCOMPtr<nsIAbDirectory> mailList;
            rv = CreateABList(listRow, getter_AddRefs(mailList));
            if (mailList)
            {
                nsCOMPtr<nsIAbMDBDirectory> dbparentDir(do_QueryInterface(parentDir, &rv));
                if(NS_SUCCEEDED(rv))
                    dbparentDir->NotifyDirItemAdded(mailList);
            }
        }
        if (parentUri)
            PR_smprintf_free(parentUri);
        if (file)
            nsCRT::free(file);
    }
    return rv;
}

NS_IMETHODIMP nsAddrDatabase::FindMailListbyUnicodeName(const PRUnichar *listName, PRBool *exist)
{
    nsresult rv = NS_ERROR_FAILURE;
    nsAutoString unicodeString(listName);
    ToLowerCase(unicodeString);
    char* pUTF8Str = ToNewUTF8String(unicodeString);
    if (pUTF8Str)
    {
        nsIMdbRow    *pListRow = nsnull;
        rv = GetRowForCharColumn(pUTF8Str, m_LowerListNameColumnToken, PR_FALSE, &pListRow);
        if (pListRow)
        {
            *exist = PR_TRUE;
                        NS_RELEASE(pListRow);
        }
        else
            *exist = PR_FALSE;
        Recycle(pUTF8Str);
    }
    return rv;
}

NS_IMETHODIMP nsAddrDatabase::GetCardCount(PRUint32 *count)
{    
    mdb_err rv;
    mdb_count c;
    rv = m_mdbPabTable->GetCount(m_mdbEnv, &c);
    if (rv == NS_OK)
        *count = c - 1;  // Don't count LastRecordKey 

    return rv;
}

/*
    "sourceString" should be a lowercase UTF8 string
*/
nsresult nsAddrDatabase::GetRowForCharColumn
(const char *lowerUTF8String, mdb_column findColumn, PRBool bIsCard, nsIMdbRow **findRow)
{
    if (lowerUTF8String)
    {
        mdbYarn    sourceYarn;

        sourceYarn.mYarn_Buf = (void *) lowerUTF8String;
        sourceYarn.mYarn_Fill = PL_strlen(lowerUTF8String);
        sourceYarn.mYarn_Form = 0;
        sourceYarn.mYarn_Size = sourceYarn.mYarn_Fill;

        mdbOid        outRowId;
        nsIMdbStore* store = GetStore();
        nsIMdbEnv* env = GetEnv();
        
        if (bIsCard)
            store->FindRow(env, m_CardRowScopeToken,
                           findColumn, &sourceYarn,  &outRowId, findRow);
        else
            store->FindRow(env, m_ListRowScopeToken,
                        findColumn, &sourceYarn,  &outRowId, findRow);
        
        if (*findRow)
            return NS_OK;
    }    
    return NS_ERROR_FAILURE;
}

nsresult nsAddrDatabase::GetRowForCharColumn
(const PRUnichar *unicodeStr, mdb_column findColumn, PRBool bIsCard, nsIMdbRow **findRow)
{
  nsresult rv = NS_ERROR_FAILURE;
  nsAutoString unicodeString(unicodeStr);
  ToLowerCase(unicodeString);
  char* pUTF8Str = ToNewUTF8String(unicodeString);
  if (pUTF8Str)
  {
    rv = GetRowForCharColumn(pUTF8Str, findColumn, bIsCard, findRow);
    Recycle(pUTF8Str);
  }
  return rv;
}

nsresult nsAddrDatabase::DeleteRow(nsIMdbTable* dbTable, nsIMdbRow* dbRow)
{
    mdb_err err = NS_OK;

    err = dbRow->CutAllColumns(GetEnv());
    err = dbTable->CutRow(GetEnv(), dbRow);

    return (err == NS_OK) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsAddrDatabase::RemoveExtraCardsInCab(PRUint32 cardTotal, PRUint32 nCabMax)
{
    nsresult err = NS_OK;
    mdb_err merror;
    nsIMdbTableRowCursor* rowCursor = nsnull;
    nsIMdbRow* findRow = nsnull;
     mdb_pos    rowPos = 0;
    nsVoidArray delCardArray;

    merror = m_mdbPabTable->GetTableRowCursor(GetEnv(), -1, &rowCursor);

    if (!(merror == NS_OK && rowCursor))
        return NS_ERROR_FAILURE;

    do 
    {
        merror = rowCursor->NextRow(GetEnv(), &findRow, &rowPos);

        if (merror == NS_OK && findRow)
        {
            mdbOid rowOid;
            merror = findRow->GetOid(GetEnv(), &rowOid);

            if (merror == NS_OK && IsCardRowScopeToken(rowOid.mOid_Scope))
            {
                delCardArray.AppendElement(findRow);
                if (--cardTotal == nCabMax)
                    break;
            }
        }
    } while (findRow);
        NS_RELEASE(rowCursor);

    PRInt32 count = delCardArray.Count();
    PRInt32 i;
    for (i = 0; i < count; i++)
    {
        findRow = (nsIMdbRow*)delCardArray.ElementAt(i);

        nsCOMPtr<nsIAbCard>    card;
        CreateCard(findRow, 0, getter_AddRefs(card));
        //need to create the card before we delete the row
        err = DeleteRow(m_mdbPabTable, findRow);

        if (card)
            NotifyCardEntryChange(AB_NotifyDeleted, card, nsnull);

                NS_RELEASE(findRow);
    }
    return NS_OK;
}

