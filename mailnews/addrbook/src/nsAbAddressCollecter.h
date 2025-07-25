/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *  Seth Spitzer <sspitzer@netscape.com>
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

#ifndef _nsAbAddressCollecter_H_
#define _nsAbAddressCollecter_H_

#include "nsIAbAddressCollecter.h"
#include "nsCOMPtr.h"
#include "nsIAbAddressCollecter.h"
#include "nsIAddrDatabase.h"
#include "nsAddrDatabase.h"
#include "nsIAbCard.h"

class nsIPref;

class nsAbAddressCollecter : public nsIAbAddressCollecter
{
public:
	nsAbAddressCollecter();
	virtual ~nsAbAddressCollecter();

	NS_DECL_ISUPPORTS
  NS_DECL_NSIABADDRESSCOLLECTER

	nsresult OpenHistoryAB(nsIAddrDatabase **aDatabase);
	nsresult IsDomainExcluded(const char *address, nsIPref *pPref, PRBool *bExclude);
	nsresult SetNamesForCard(nsIAbCard *senderCard, const char *fullName);
	nsresult SplitFullName (const char *fullName, char **firstName, char **lastName);
  nsresult Init();

private:
	static int PR_CALLBACK collectEmailAddressEnableSizeLimitPrefChanged(const char *newpref, void *data);
	static int PR_CALLBACK collectEmailAddressSizeLimitPrefChanged(const char *newpref, void *data);
	nsresult AddCardToCollectedAddressBook(nsIAbCard *card);

protected:
	nsCOMPtr <nsIAddrDatabase> m_historyAB;
	nsCOMPtr <nsIAbDirectory> m_historyDirectory;
	PRInt32 m_maxCABsize;
	PRBool m_sizeLimitEnabled;

};

#endif  // _nsAbAddressCollecter_H_

