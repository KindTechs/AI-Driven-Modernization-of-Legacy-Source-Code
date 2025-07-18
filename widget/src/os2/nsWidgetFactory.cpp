/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Dan Rosen <dr@netscape.com>
 *
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date             Modified by            Description of modification
 * 03/23/2000       IBM Corp.              Added support for directory picker dialog.
 * 03/24/2000       IBM Corp.              Updated based on nsWinWidgetFactory.cpp.
 * 05/31/2000       IBM Corp.              Enabled timer stuff
 * 06/30/2000       sobotka@axess.com      Added nsFilePicker
 * 03/11/2001       achimha@innotek.de     converted to XPCOM module
 * 03/20/2001       achimha@innotek.de     Added class for embedded module init
 * 12/16/2001       pavlov@netscape.com    Removed timer stuff
 */

#include "nsIGenericFactory.h"
#include "nsIModule.h"
#include "nsCOMPtr.h"
#include "nsWidgetsCID.h"

//#include "nsWidgetDefs.h"

// class definition headers
#include "nsAppShell.h"
#include "nsBidiKeyboard.h"
#include "nsCanvas.h"
#include "nsDragService.h"
#include "nsILocalFile.h"
#include "nsFilePicker.h"
#include "nsLookAndFeel.h"
#include "nsScrollbar.h"
#include "nsSound.h"
#include "nsToolkit.h"
#include "nsModule.h"

// Drag & Drop, Clipboard
#include "nsClipboard.h"
#include "nsClipboardHelper.h"
#include "nsTransferable.h"
#include "nsHTMLFormatConverter.h"

#include "nsFrameWindow.h" // OS/2 only

// objects that just require generic constructors
NS_GENERIC_FACTORY_CONSTRUCTOR(nsBidiKeyboard)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsCanvas)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsClipboard)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsClipboardHelper)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsFilePicker)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsFrameWindow)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsLookAndFeel)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSound)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsToolkit)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsTransferable)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsHTMLFormatConverter)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDragService)


// custom "contructor" methods

static NS_IMETHODIMP nsAppShellConstructor (nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
  nsresult rv;
  nsISupports *inst = nsnull;

  if ( NULL == aResult )
  {
    rv = NS_ERROR_NULL_POINTER;
    return rv;
  }
  *aResult = NULL;
  if (NULL != aOuter)
  {
    rv = NS_ERROR_NO_AGGREGATION;
    return rv;
  }

  // need to do this in order to keep initial window on the screen
  // and be able to close browser  
  nsIAppShell *pShell = nsnull;
  NS_CreateAppshell(&pShell);
  inst = (nsISupports*)pShell;

  if (inst == NULL)
  {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  NS_ADDREF(inst);
  rv = inst->QueryInterface(aIID, aResult);
  NS_RELEASE(inst);

  return rv;
}

static NS_IMETHODIMP nsWidgetModuleDataConstructor (nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
  nsresult rv;
  nsISupports *inst = nsnull;

  if ( NULL == aResult )
  {
    rv = NS_ERROR_NULL_POINTER;
    return rv;
  }
  *aResult = NULL;
  if (NULL != aOuter)
  {
    rv = NS_ERROR_NO_AGGREGATION;
    return rv;
  }

  // we need to create an object, store it in a global
  // pointer and call its init method. This object is only
  // instantiated in the embedding case - for the retail
  // browser this is done in NS_CreateAppshell
  gWidgetModuleData = new nsWidgetModuleData();
  gWidgetModuleData->Init(nsnull);
  inst = (nsISupports*)gWidgetModuleData;

  if (inst == NULL)
  {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  NS_ADDREF(inst);
  rv = inst->QueryInterface(aIID, aResult);
  NS_RELEASE(inst);

  return rv;
}

static NS_IMETHODIMP nsHorizScrollbarConstructor (nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
  nsresult rv;
  nsISupports *inst = nsnull;

  if ( NULL == aResult )
  {
    rv = NS_ERROR_NULL_POINTER;
    return rv;
  }
  *aResult = NULL;
  if (NULL != aOuter)
  {
    rv = NS_ERROR_NO_AGGREGATION;
    return rv;
  }
  
  inst = (nsISupports*)(nsBaseWidget*)(nsWindow*)new nsScrollbar(PR_FALSE);
  if (inst == NULL)
  {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  NS_ADDREF(inst);
  rv = inst->QueryInterface(aIID, aResult);
  NS_RELEASE(inst);

  return rv;
}

static NS_IMETHODIMP nsVertScrollbarConstructor (nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
  nsresult rv;
  nsISupports *inst = nsnull;

  if ( NULL == aResult )
  {
    rv = NS_ERROR_NULL_POINTER;
    return rv;
  }
  *aResult = NULL;
  if (NULL != aOuter)
  {
    rv = NS_ERROR_NO_AGGREGATION;
    return rv;
  }
  
  inst = (nsISupports*)(nsBaseWidget*)(nsWindow*)new nsScrollbar(PR_TRUE);
  if (inst == NULL)
  {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  NS_ADDREF(inst);
  rv = inst->QueryInterface(aIID, aResult);
  NS_RELEASE(inst);

  return rv;
}


// component definition, will be exported using XPCOM
static const nsModuleComponentInfo components[] =
{
  { "OS/2 AppShell",
    NS_APPSHELL_CID,
    "@mozilla.org/widget/appshell/os2;1",
    nsAppShellConstructor },
  { "OS/2 Embedded Module Data Init",
    NS_MODULEDATAOS2_CID,
    "@mozilla.org/widget/widgetmoduledata/os2;1",
    nsWidgetModuleDataConstructor },
  { "OS/2 Bidi Keyboard",
    NS_BIDIKEYBOARD_CID,
    "@mozilla.org/widget/bidikeyboard;1",
    nsBidiKeyboardConstructor },
  { "OS/2 Canvas",
    NS_CHILD_CID,
    "@mozilla.org/widget/child_window/os2;1",
    nsCanvasConstructor },
  { "OS/2 Clipboard",
    NS_CLIPBOARD_CID,
    "@mozilla.org/widget/clipboard;1",
    nsClipboardConstructor },
  { "Clipboard Helper",
    NS_CLIPBOARDHELPER_CID,
    "@mozilla.org/widget/clipboardhelper;1",
    nsClipboardHelperConstructor },
  { "OS/2 Drag Service",
    NS_DRAGSERVICE_CID,
    "@mozilla.org/widget/dragservice;1",
    nsDragServiceConstructor },
  { "OS/2 File Picker",
    NS_FILEPICKER_CID,
    "@mozilla.org/filepicker;1",
    nsFilePickerConstructor },
  { "OS/2 Look And Feel",
    NS_LOOKANDFEEL_CID,
    "@mozilla.org/widget/lookandfeel/os2;1",
    nsLookAndFeelConstructor },
  { "OS/2 Horiz Scrollbar",
    NS_HORZSCROLLBAR_CID,
    "@mozilla.org/widget/horizscroll/os2;1",
    nsHorizScrollbarConstructor },
  { "OS/2 Vert Scrollbar",
    NS_VERTSCROLLBAR_CID,
    "@mozilla.org/widget/vertscroll/os2;1",
    nsVertScrollbarConstructor },
  { "OS/2 Sound",
    NS_SOUND_CID,
    "@mozilla.org/sound;1",
    nsSoundConstructor },
  { "OS/2 Toolkit",
    NS_TOOLKIT_CID,
    "@mozilla.org/widget/toolkit/os2;1",
    nsToolkitConstructor },
  { "OS/2 Frame Window",
    NS_WINDOW_CID,
    "@mozilla.org/widget/window/os2;1",
    nsFrameWindowConstructor },
  { "OS/2 Transferable",
    NS_TRANSFERABLE_CID,
    "@mozilla.org/widget/transferable;1",
    nsTransferableConstructor },
  { "OS/2 HTML Format Converter",
    NS_HTMLFORMATCONVERTER_CID,
    "@mozilla.org/widget/htmlformatconverter/os2;1",
    nsHTMLFormatConverterConstructor }
};

PR_STATIC_CALLBACK(void)
nsWidgetOS2ModuleDtor(nsIModule *self)
{
  // what shall we do here?
}

NS_IMPL_NSGETMODULE_WITH_DTOR(nsWidgetOS2Module,
                              components,
                              nsWidgetOS2ModuleDtor)
