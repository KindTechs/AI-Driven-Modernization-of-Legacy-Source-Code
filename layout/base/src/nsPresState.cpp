#include "nsCOMPtr.h"
#include "nsIPresState.h"
#include "nsHashtable.h"
#include "nsISupportsPrimitives.h"
#include "nsIComponentManager.h"
#include "nsXPIDLString.h"

// Static IIDs/CIDs. Try to minimize these.
// None


class nsPresState: public nsIPresState
{
  NS_DECL_ISUPPORTS

  NS_IMETHOD GetStatePropertyAsSupports(const nsAString& aName, nsISupports** aResult);
  NS_IMETHOD SetStatePropertyAsSupports(const nsAString& aName, nsISupports* aValue);

  NS_IMETHOD GetStateProperty(const nsAString& aProperty, nsAString& aResult);
  NS_IMETHOD SetStateProperty(const nsAString& aProperty, const nsAString& aValue);

  NS_IMETHOD RemoveStateProperty(const nsAString& aProperty);

public:
  nsPresState();
  virtual ~nsPresState();

// Static members

// Internal member functions
protected:
  
// MEMBER VARIABLES
protected:
  // A string table that holds property/value pairs.
  nsSupportsHashtable* mPropertyTable; 
};

// Static initialization


// Implementation /////////////////////////////////////////////////////////////////

// Implement our nsISupports methods
NS_IMPL_ISUPPORTS1(nsPresState, nsIPresState)

// Constructors/Destructors
nsPresState::nsPresState(void)
:mPropertyTable(nsnull)
{
  NS_INIT_ISUPPORTS();
}

nsPresState::~nsPresState(void)
{
  delete mPropertyTable;
}

// nsIPresState Interface ////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsPresState::GetStateProperty(const nsAString& aName,
			      nsAString& aResult)
{
  aResult.SetLength(0);

  // Retrieve from hashtable.
  if (mPropertyTable) {
    nsStringKey key(aName);

    nsCOMPtr<nsISupportsString> supportsStr =
            dont_AddRef(NS_STATIC_CAST(nsISupportsString*,
                                       mPropertyTable->Get(&key)));

    if (supportsStr) {
      nsXPIDLCString data;
      supportsStr->GetData(getter_Copies(data));

      aResult.Append(NS_ConvertUTF8toUCS2(data));
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsPresState::SetStateProperty(const nsAString& aName, const nsAString& aValue)
{
  if (!mPropertyTable) {
    mPropertyTable = new nsSupportsHashtable(8);
    NS_ENSURE_TRUE(mPropertyTable, NS_ERROR_OUT_OF_MEMORY);
  }

  // Add to hashtable
  nsStringKey key(aName);

  nsCOMPtr<nsISupportsString> supportsStr(do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID));
  NS_ENSURE_TRUE(supportsStr, NS_ERROR_OUT_OF_MEMORY);

  supportsStr->SetData(NS_ConvertUCS2toUTF8(aValue).get());

  mPropertyTable->Put(&key, supportsStr);
  return NS_OK;
}

NS_IMETHODIMP
nsPresState::RemoveStateProperty(const nsAString& aName)
{
  if (!mPropertyTable)
    return NS_OK;

  nsStringKey key(aName);

  mPropertyTable->Remove(&key);
  return NS_OK;
}

NS_IMETHODIMP
nsPresState::GetStatePropertyAsSupports(const nsAString& aName, nsISupports** aResult)
{
  // Retrieve from hashtable.
  nsCOMPtr<nsISupports> supp;

  if (mPropertyTable) {
    nsStringKey key(aName);
    supp = dont_AddRef(NS_STATIC_CAST(nsISupports*, mPropertyTable->Get(&key)));
  }

  *aResult = supp;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsPresState::SetStatePropertyAsSupports(const nsAString& aName, nsISupports* aValue)
{
  if (!mPropertyTable) {
    mPropertyTable = new nsSupportsHashtable(8);
    NS_ENSURE_TRUE(mPropertyTable, NS_ERROR_OUT_OF_MEMORY);
  }

  // Add to hashtable
  nsStringKey key(aName);
  mPropertyTable->Put(&key, aValue);
  return NS_OK;
}

// Creation Routine ///////////////////////////////////////////////////////////////////////

nsresult
NS_NewPresState(nsIPresState** aResult)
{
  *aResult = new nsPresState;
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}

