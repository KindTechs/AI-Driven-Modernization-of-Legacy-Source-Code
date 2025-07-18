/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
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

#include "nscore.h"
#include "nsMacEventHandler.h"
#include "nsMacTSMMessagePump.h"
#include "nsString.h"
#include <Script.h>
#include <TextServices.h>
#include <AEDataModel.h>
#include "nsTSMStrategy.h"

#include "nsCarbonHelpers.h"


//-------------------------------------------------------------------------
//
// TSM AE Handler routines UPPs 
//
//-------------------------------------------------------------------------

AEEventHandlerUPP nsMacTSMMessagePump::mPos2OffsetUPP = NULL;
AEEventHandlerUPP nsMacTSMMessagePump::mOffset2PosUPP = NULL;
AEEventHandlerUPP nsMacTSMMessagePump::mUpdateUPP = NULL;
AEEventHandlerUPP nsMacTSMMessagePump::mKeyboardUPP = NULL;

//-------------------------------------------------------------------------
//
// Constructor/Destructors
//
//-------------------------------------------------------------------------
nsMacTSMMessagePump::nsMacTSMMessagePump()
{
	OSErr	err;
	nsTSMStrategy tsmstrategy;

	mPos2OffsetUPP = NewAEEventHandlerUPP(nsMacTSMMessagePump::PositionToOffsetHandler);
	NS_ASSERTION(mPos2OffsetUPP!=NULL, "nsMacTSMMessagePump::InstallTSMAEHandlers: NewAEEventHandlerUPP[Pos2Pffset] failed");

	mOffset2PosUPP = NewAEEventHandlerUPP(nsMacTSMMessagePump::OffsetToPositionHandler);
	NS_ASSERTION(mPos2OffsetUPP!=NULL, "nsMacTSMMessagePump::InstallTSMAEHandlers: NewAEEventHandlerUPP[Pos2Pffset] failed");

  if (tsmstrategy.UseUnicodeForInputMethod()) {
    mUpdateUPP = NewAEEventHandlerUPP(nsMacTSMMessagePump::UnicodeUpdateHandler);
  } else {
	mUpdateUPP = NewAEEventHandlerUPP(nsMacTSMMessagePump::UpdateHandler);
  }
  NS_ASSERTION(mPos2OffsetUPP!=NULL, "nsMacTSMMessagePump::InstallTSMAEHandlers: NewAEEventHandlerUPP[Pos2Pffset] failed");

  err = AEInstallEventHandler(kTextServiceClass, kPos2Offset, mPos2OffsetUPP, (long)this, false);
  NS_ASSERTION(err==noErr, "nsMacTSMMessagePump::InstallTSMAEHandlers: AEInstallEventHandlers[Pos2Offset] failed");

  err = AEInstallEventHandler(kTextServiceClass, kOffset2Pos, mOffset2PosUPP, (long)this, false);
  NS_ASSERTION(err==noErr, "nsMacTSMMessagePump::InstallTSMAEHandlers: AEInstallEventHandlers[Offset2Pos] failed");

  err = AEInstallEventHandler(kTextServiceClass, kUpdateActiveInputArea, mUpdateUPP, (long)this, false);
  NS_ASSERTION(err==noErr, "nsMacTSMMessagePump::InstallTSMAEHandlers: AEInstallEventHandlers[Update] failed");

  if (tsmstrategy.UseUnicodeForKeyboard()) {
    mKeyboardUPP = NewAEEventHandlerUPP(nsMacTSMMessagePump::UnicodeNotFromInputMethodHandler);
    NS_ASSERTION(mKeyboardUPP!=NULL, "nsMacTSMMessagePump::InstallTSMAEHandlers: NewAEEventHandlerUPP[FromInputMethod] failed");

    err = AEInstallEventHandler(kTextServiceClass, kUnicodeNotFromInputMethod, mKeyboardUPP, (long)this, false);
    NS_ASSERTION(err==noErr, "nsMacTSMMessagePump::InstallTSMAEHandlers: AEInstallEventHandlers[FromInputMethod] failed");
  }

}

nsMacTSMMessagePump::~nsMacTSMMessagePump()
{
	OSErr	err;
	
	err = AERemoveEventHandler(kTextServiceClass, kPos2Offset, mPos2OffsetUPP, false);
	NS_ASSERTION(err==noErr, "nsMacTSMMessagePump::InstallTSMAEHandlers: AEInstallEventHandlers[Pos2Offset] failed");

	err = AERemoveEventHandler(kTextServiceClass, kOffset2Pos, mOffset2PosUPP, false);
	NS_ASSERTION(err==noErr, "nsMacTSMMessagePump::InstallTSMAEHandlers: AEInstallEventHandlers[Offset2Pos] failed");

	err = AERemoveEventHandler(kTextServiceClass, kUpdateActiveInputArea, mUpdateUPP, false);
	NS_ASSERTION(err==noErr, "nsMacTSMMessagePump::InstallTSMAEHandlers: AEInstallEventHandlers[Update] failed");

 	::DisposeAEEventHandlerUPP(mPos2OffsetUPP);
 	::DisposeAEEventHandlerUPP(mOffset2PosUPP);
 	::DisposeAEEventHandlerUPP(mUpdateUPP);

  nsTSMStrategy tsmstrategy;
  if (tsmstrategy.UseUnicodeForKeyboard()) {
    err = AERemoveEventHandler(kTextServiceClass, kUnicodeNotFromInputMethod, mKeyboardUPP, false);
    NS_ASSERTION(err==noErr, "nsMacTSMMessagePump::InstallTSMAEHandlers: AEInstallEventHandlers[FromInputMethod] failed");
    ::DisposeAEEventHandlerUPP(mKeyboardUPP);
  }

}

//-------------------------------------------------------------------------
nsMacTSMMessagePump* nsMacTSMMessagePump::gSingleton = nsnull;
//-------------------------------------------------------------------------
nsMacTSMMessagePump* nsMacTSMMessagePump::GetSingleton()
{
	if (nsnull == gSingleton)
	{
		gSingleton = new nsMacTSMMessagePump();
		NS_ASSERTION(gSingleton!=NULL, "nsMacTSMMessagePump::GetSingleton: Unable to create TSM Message Pump.");
	}
	return gSingleton;
}
//-------------------------------------------------------------------------
void nsMacTSMMessagePump::Shutdown()
{
	if (gSingleton) {
		delete gSingleton;
		gSingleton = nsnull;
	}
}

//-------------------------------------------------------------------------
//
// TSM AE Handler routines
//
//-------------------------------------------------------------------------

pascal OSErr nsMacTSMMessagePump::PositionToOffsetHandler(const AppleEvent *theAppleEvent, AppleEvent *reply, long handlerRefcon)
{
	OSErr 				err;
	DescType			returnedType;
	nsMacEventHandler*	eventHandler;	
	Size				actualSize;
	Point				thePoint;
	long				offset;
	short				regionClass;

	//
	// Extract the nsMacEventHandler for this TSMDocument.  It's stored as the TSMDocument's
	//	refcon
	//
	err = AEGetParamPtr(theAppleEvent, keyAETSMDocumentRefcon, typeLongInteger, &returnedType,
						&eventHandler, sizeof(eventHandler), &actualSize);
	NS_ASSERTION(err==noErr, "nsMacTSMMessagePump::PositionToOffsetHandler: AEGetParamPtr[TSMRefcon] failed");
	if (err!=noErr) 
		return err;
	
	//
	// Extract the Position parameter.
	//
	err = AEGetParamPtr(theAppleEvent, keyAECurrentPoint, typeQDPoint, &returnedType,
						&thePoint, sizeof(thePoint), &actualSize);
	NS_ASSERTION(err==noErr, "nsMacTSMMessagePump::PositionToOffsetHandler: AGGetParamPtr[Point] failed");
	if (err!=noErr) 
		return err;

	//
	// pass the request to the widget system
	//
	offset = eventHandler->HandlePositionToOffset(thePoint, &regionClass);
	
	//
	// build up the AE reply (need offset and region class)
	//
	err = AEPutParamPtr(reply, keyAEOffset, typeLongInteger, &offset, sizeof(offset));
	NS_ASSERTION(err==noErr, "nsMacTSMMessagePump::PositionToOffsetHandler: AEPutParamPtr failed");
	if (err!=noErr) 
		return err;
	
	err = AEPutParamPtr(reply, keyAERegionClass, typeShortInteger, &regionClass, sizeof(regionClass));
	NS_ASSERTION(err==noErr, "nsMacTSMMessagePump::PositionToOffsetHandler: AEPutParamPtr failed");
	if (err!=noErr) 
		return err;

	return noErr;
}
pascal OSErr nsMacTSMMessagePump::OffsetToPositionHandler(const AppleEvent *theAppleEvent, AppleEvent *reply, long handlerRefcon)
{
	OSErr 				err;
	DescType			returnedType;
	nsMacEventHandler*	eventHandler;	
	Size				actualSize;
	Point				thePoint;
	long				offset;
	nsresult			res;

	//
	// Extract the nsMacEvenbtHandler for this TSMDocument.  It's stored as the refcon.
	//
	err = AEGetParamPtr(theAppleEvent, keyAETSMDocumentRefcon, typeLongInteger, &returnedType,
						&eventHandler, sizeof(eventHandler), &actualSize);
	NS_ASSERTION(err==noErr, "nsMacTSMMessagePump::OffsetToPositionHandler: AEGetParamPtr[TSMRefcon] failed.");
	if (err!=noErr) 
		return err;
	
	//
	// Extract the Offset parameter
	//
	err = AEGetParamPtr(theAppleEvent, keyAEOffset, typeLongInteger, &returnedType,
						&offset, sizeof(offset), &actualSize);
	NS_ASSERTION(err==noErr, "nsMacTSMMessagePump::PositionToOffsetHandler: AEGetParamPtr[Offset] failed.");
	if (err!=noErr) 
		return err;
	
	//
	// Pass the OffsetToPosition request to the widgets to handle
	//
	res = eventHandler->HandleOffsetToPosition(offset, &thePoint);
	NS_ASSERTION(NS_SUCCEEDED(res), "nsMacMessagePup::PositionToOffsetHandler: OffsetToPosition handler failed.");
	if (NS_FAILED(res)) 
		return paramErr;
	
	//
	// build up the reply (point)
	//
	err = AEPutParamPtr(reply, keyAEPoint, typeQDPoint, &thePoint, sizeof(Point));
	NS_ASSERTION(err==noErr, "nsMacTSMMessagePump::PositionToOffsetHandler: AEPutParamPtr[Point] failed.");
	if (err!=noErr) 
		return err;
	
	return noErr;
}
pascal OSErr nsMacTSMMessagePump::UpdateHandler(const AppleEvent *theAppleEvent, AppleEvent *reply, long handlerRefcon)
{
	OSErr 					err;
	DescType				returnedType;
	nsMacEventHandler*		eventHandler;	
	Size					actualSize;
	AEDesc					text, hiliteRangeArray;
	ScriptCode				textScript;
	long					fixLength;
	nsresult				res;
	TextRangeArray*			hiliteRangePtr;


	//
	// refcon stores the nsMacEventHandler
	//
	err = AEGetParamPtr(theAppleEvent, keyAETSMDocumentRefcon, typeLongInteger, &returnedType,
						&eventHandler, sizeof(eventHandler), &actualSize);
	NS_ASSERTION(err==noErr, "nsMacTSMMessagePump::UpdateHandler: AEGetParamPtr[TSMRefcon] failed.");
	if (err!=noErr) 
		return err;
	
	//
	// IME update text
	//
	err = AEGetParamDesc(theAppleEvent, keyAETheData, typeChar, &text);
	NS_ASSERTION(err==noErr, "nsMacTSMMessagePump::UpdateHandler: AEGetParamDesc[Text] failed.");
	if (err!=noErr) 
		return err;
	
	//
	// get the script of text for Unicode conversion
	//
	textScript=smUninterp;
	AEDesc slr;
	err = AEGetParamDesc(theAppleEvent, keyAETSMScriptTag, typeIntlWritingCode, &slr);
	NS_ASSERTION(err==noErr, "nsMacTSMMessagePump::UpdateHandler: AEGetParamDesc[keyAETSMScriptTag] failed.");
	if (err!=noErr) 
		return err;
	
#if TARGET_CARBON
	ScriptLanguageRecord scriptLangRec;
	err = AEGetDescData(&slr, (void *) &scriptLangRec, sizeof(ScriptLanguageRecord));
	if (err!=noErr) 
		return err;
	textScript = scriptLangRec.fScript;
#else
	textScript = ((ScriptLanguageRecord *)(*(slr.dataHandle)))->fScript;
#endif
	NS_ASSERTION( (textScript < smUninterp), "Illegal script code");
	
	NS_ASSERTION(textScript == (ScriptCode)::GetScriptManagerVariable(smKeyScript) , "wrong script code");
	//
	// length of converted text
	//
	err = AEGetParamPtr(theAppleEvent, keyAEFixLength, typeLongInteger, &returnedType,
						&fixLength, sizeof(fixLength), &actualSize);
	NS_ASSERTION(err==noErr, "nsMacTSMMessagePump::UpdateHandler: AEGetParamPtr[fixlen] failed.");
  	if (err!=noErr) 
  		return err;

  	//
  	// extract the hilite ranges (optional param)
  	//
  	err = AEGetParamDesc(theAppleEvent, keyAEHiliteRange, typeTextRangeArray, &hiliteRangeArray);
	NS_ASSERTION(err==noErr||err==errAEDescNotFound, "nsMacTSMMessagePump::UpdateHandler: AEGetParamPtr[fixlen] failed.");
  	if (err==errAEDescNotFound) {
  		hiliteRangePtr=NULL;
  	} else if (err==noErr) { 
#if TARGET_CARBON
		Size hiliteRangeSize = ::AEGetDescDataSize(&hiliteRangeArray);
		hiliteRangePtr = (TextRangeArray *) NewPtr(hiliteRangeSize);
		if (!hiliteRangePtr)
			return MemError();
		err = AEGetDescData(&hiliteRangeArray, (void *) hiliteRangePtr, hiliteRangeSize);
		if (err!=noErr) {
			DisposePtr((Ptr) hiliteRangePtr);
			return err;
		}
#else
  		::HLock(hiliteRangeArray.dataHandle); 
  		hiliteRangePtr=(TextRangeArray*)*(hiliteRangeArray.dataHandle);
#endif
  	} else { 
  		return err;
  	}

#if TARGET_CARBON
	nsCAutoString mbcsText;
	Size text_size = ::AEGetDescDataSize(&text);
	mbcsText.SetCapacity(text_size+1);
	char* mbcsTextPtr = (char*)mbcsText.get();
	err = AEGetDescData(&text, (void *) mbcsTextPtr, text_size);
	if (err!=noErr) {
		DisposePtr((Ptr) hiliteRangePtr);
		return err;
	}
	mbcsTextPtr[text_size]=0;
#else
	nsCAutoString mbcsText;
	Size text_size = ::GetHandleSize(text.dataHandle);
	mbcsText.SetCapacity(text_size+1);
	char* mbcsTextPtr = (char*)mbcsText.get();
	strncpy(mbcsTextPtr, *(text.dataHandle), text_size);
	mbcsTextPtr[text_size]=0;
#endif
	
	//
	// must pass HandleUpdateInputArea a null-terminated multibyte string, the text size must include the terminator
	//
	res = eventHandler->HandleUpdateInputArea(mbcsTextPtr, text_size, textScript, fixLength, hiliteRangePtr);

	NS_ASSERTION(NS_SUCCEEDED(res), "nsMacMessagePump::UpdateHandler: HandleUpdated failed.");
	if (NS_FAILED(res))
		err = paramErr;
	
	//
	// clean up
	//
#if TARGET_CARBON
	if (hiliteRangePtr)
		DisposePtr((Ptr) hiliteRangePtr);
#else
	if (hiliteRangePtr)
		::HUnlock(hiliteRangeArray.dataHandle);
#endif

	(void)AEDisposeDesc(&text);
	(void)AEDisposeDesc(&hiliteRangeArray);

	return noErr;
}

static OSErr GetAppleEventTSMData(const AppleEvent *inAE, nsMacEventHandler **outEventHandler, AEDesc *outText)
{
  *outEventHandler = nsnull;
  //*outText = nsnull;

  //
  // refcon stores the nsMacEventHandler
  //
  DescType returnedType;
  Size actualSize;
  OSErr err = ::AEGetParamPtr(inAE, keyAETSMDocumentRefcon, typeLongInteger, &returnedType,
                              outEventHandler, sizeof(nsMacEventHandler *), &actualSize);
  NS_ASSERTION(err==noErr, "nsMacTSMMessagePump::GetAppleEventTSMData: AEGetParamPtr[TSMRefcon] failed.");
  if (err)
    return err;
 
  //
  // get text
  //
  err = ::AEGetParamDesc(inAE, keyAETheData, typeUnicodeText, outText);
  NS_ASSERTION(err==noErr, "nsMacTSMMessagePump::GetAppleEventTSMData: AEGetParamDesc[Text] failed.");
  return err;
}

static OSErr AETextToString(AEDesc &aAEDesc, nsString& aOutString, Size& text_size)
{
  OSErr err = noErr;
  PRUnichar* unicodeTextPtr;
  text_size = 0;
  aOutString.Truncate(0);

#if TARGET_CARBON
  text_size = ::AEGetDescDataSize(&aAEDesc) / 2;
  aOutString.SetLength(text_size + 1);
  unicodeTextPtr = (PRUnichar*)aOutString.get();
  err = AEGetDescData(&aAEDesc, (void *) unicodeTextPtr, text_size * 2);
  if (err!=noErr) 
    return err;
#else
  text_size = ::GetHandleSize(aAEDesc.dataHandle) / 2;
  aOutString.SetLength(text_size + 1);
  unicodeTextPtr = (PRUnichar*)aOutString.get();
  memcpy(unicodeTextPtr, *(aAEDesc.dataHandle), text_size * 2);
#endif

  unicodeTextPtr[text_size ] = PRUnichar('\0'); // null terminate it.
  return noErr;
}



pascal OSErr nsMacTSMMessagePump::UnicodeUpdateHandler(const AppleEvent *theAppleEvent, AppleEvent *reply, long handlerRefcon)
{
  OSErr                 err = noErr;
  DescType              returnedType;
  nsMacEventHandler*    eventHandler;
  Size                  actualSize;
  AEDesc                text, hiliteRangeArray;
  long                  fixLength;
  nsresult              res;
  TextRangeArray*       hiliteRangePtr;

  err = GetAppleEventTSMData(theAppleEvent, &eventHandler, &text);
  if (err) 
    return err;

  //
  // length of converted text
  //
  err = ::AEGetParamPtr(theAppleEvent, keyAEFixLength, typeLongInteger, &returnedType,
                        &fixLength, sizeof(fixLength), &actualSize);
  NS_ASSERTION(err==noErr, "nsMacTSMMessagePump::UnicodeUpdateHandler: AEGetParamPtr[fixlen] failed.");
  if (err) 
    return err;

  //
  // extract the hilite ranges (optional param)
  //
  err = ::AEGetParamDesc(theAppleEvent, keyAEHiliteRange, typeTextRangeArray, &hiliteRangeArray);
  NS_ASSERTION(err==noErr||err==errAEDescNotFound, "nsMacTSMMessagePump::UnicodeUpdateHandler: AEGetParamDesc[hiliteRangeArray] failed.");
  if (errAEDescNotFound == err)
  {
    hiliteRangePtr = NULL;
  } 
  else if (noErr == err)
  { 
#if TARGET_CARBON
    Size hiliteRangeSize = ::AEGetDescDataSize(&hiliteRangeArray);
    hiliteRangePtr = (TextRangeArray *) NewPtr(hiliteRangeSize);
    if (!hiliteRangePtr)
    {
      err = MemError();
      goto err2;
    }
    err = ::AEGetDescData(&hiliteRangeArray, (void *)hiliteRangePtr, hiliteRangeSize);
    if (noErr != err)
    {
      goto err1; 
    }
#else
    ::HLock(hiliteRangeArray.dataHandle); 
    hiliteRangePtr = (TextRangeArray*)*(hiliteRangeArray.dataHandle);
#endif
  }
  else
  {
    goto err3;
  }

  // grab the text now
  {
    nsAutoString unicodeText;
    Size text_size;
    err = AETextToString(text, unicodeText, text_size);
    if (noErr == err)
    {
      res = eventHandler->UnicodeHandleUpdateInputArea((PRUnichar*)unicodeText.get(), text_size, fixLength / 2, hiliteRangePtr);
      NS_ASSERTION(NS_SUCCEEDED(res), "nsMacMessagePump::UnicodeUpdateHandler: HandleUpdated failed.");
      if (NS_FAILED(res))
        err = paramErr;
        // fall to cleanup below
    }
  }

  //
  // clean up
  //
err1:
#if TARGET_CARBON
    if (hiliteRangePtr)
      ::DisposePtr((Ptr)hiliteRangePtr);
#else
    if (hiliteRangePtr)
      ::HUnlock(hiliteRangeArray.dataHandle);
#endif

err2:
  (void)::AEDisposeDesc(&hiliteRangeArray);
err3:
  (void)::AEDisposeDesc(&text);

  return err;
}

pascal OSErr nsMacTSMMessagePump::UnicodeNotFromInputMethodHandler(const AppleEvent *theAppleEvent, AppleEvent *reply, long handlerRefcon)
{
  OSErr               err;
  nsMacEventHandler*  eventHandler;
  AEDesc              text;
  DescType            returnedType;
  Size                actualSize;
  EventRecord         event;

  err = GetAppleEventTSMData(theAppleEvent, &eventHandler, &text);
  if (err != noErr) 
    return err;
  
  err = ::AEGetParamPtr(theAppleEvent, keyAETSMEventRecord, typeLowLevelEventRecord, &returnedType,
                        &event, sizeof(event), &actualSize);
  NS_ASSERTION(err==noErr, "nsMacTSMMessagePump::UnicodeNotFromInputMethodHandler: AEGetParamPtr[event] failed.");
  if (noErr != err)
  {
    (void)::AEDisposeDesc(&text);
    return err;
  }

  nsAutoString unicodeText;
  Size text_size;
  err = AETextToString(text, unicodeText, text_size);
  if (noErr == err)
  {
    nsresult res;
    res = eventHandler->HandleUKeyEvent((PRUnichar*)unicodeText.get(), text_size, event);
    NS_ASSERTION(NS_SUCCEEDED(res), "nsMacMessagePump::UnicodeNotFromInputMethodHandler: HandleUpdated failed.");
    if (NS_FAILED(res)) {
      err = paramErr;
      // fall to cleanup below
    }
  }

  (void)::AEDisposeDesc(&text);
  return err;
}
