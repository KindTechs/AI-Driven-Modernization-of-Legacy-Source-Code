/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Christopher Blizzard.
 * Portions created Christopher Blizzard are Copyright (C) Christopher
 * Blizzard.  All Rights Reserved.
 * 
 * Contributor(s):
 */

#include <X11/X.h>
#include <X11/Xlib.h>
#include "nsIXRemoteClient.h"

class XRemoteClient : public nsIXRemoteClient
{
 public:
  XRemoteClient();
  virtual ~XRemoteClient();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIXRemoteClient
  NS_DECL_NSIXREMOTECLIENT

private:

  Window         FindWindow       (void);
  Window         CheckWindow      (Window aWindow);
  Window         CheckChildren    (Window aWindow);
  nsresult       GetLock          (Window aWindow, PRBool *aDestroyed);
  nsresult       FreeLock         (Window aWindow);
  nsresult       DoSendCommand    (Window aWindow,
				   const char *aCommand,
				   PRBool *aDestroyed);

  Display       *mDisplay;

  Atom           mMozVersionAtom;
  Atom           mMozLockAtom;
  Atom           mMozCommandAtom;
  Atom           mMozResponseAtom;
  Atom           mMozWMStateAtom;
  Atom           mMozUserAtom;

  char          *mLockData;

  PRBool         mInitialized;

};
