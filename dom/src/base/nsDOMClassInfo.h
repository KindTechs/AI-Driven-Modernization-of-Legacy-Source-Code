/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Johnny Stenback <jst@netscape.com> (original author)
 *
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

#ifndef nsDOMClassInfo_h___
#define nsDOMClassInfo_h___

#include "nsIDOMClassInfo.h"
#include "nsIXPCScriptable.h"
#include "jsapi.h"
#include "nsIScriptSecurityManager.h"

class nsIDOMWindow;
class nsIDOMNSHTMLOptionCollection;
class nsIPluginInstance;
class nsIForm;

struct nsDOMClassInfoData;

typedef nsIClassInfo* (*nsDOMClassInfoConstructorFnc)
  (nsDOMClassInfoData* aData);


struct nsDOMClassInfoData
{
  const char *mName;
  union {
    nsDOMClassInfoConstructorFnc mConstructorFptr;
    nsDOMClassInfoExternalConstructorFnc mExternalConstructorFptr;
  } u;
  nsIClassInfo *mCachedClassInfo; // low bit is set to 1 if external,
                                  // so be sure to mask if necessary!
  const nsIID *mProtoChainInterface;
  const nsIID **mInterfaces;
  PRUint32 mScriptableFlags : 31; // flags must not use more than 31 bits!
  PRBool mHasClassInterface : 1;
#ifdef NS_DEBUG
  PRUint32 mDebugID;
#endif
};

struct nsExternalDOMClassInfoData : public nsDOMClassInfoData
{
  const nsCID *mConstructorCID;
};


typedef unsigned long PtrBits;

// To be used with the nsDOMClassInfoData::mCachedClassInfo pointer.
// The low bit is set when we created a generic helper for an external
// (which holds on to the nsDOMClassInfoData).
#define GET_CLEAN_CI_PTR(_ptr) (nsIClassInfo*)(PtrBits(_ptr) & ~0x1)
#define MARK_EXTERNAL(_ptr) (nsIClassInfo*)(PtrBits(_ptr) | 0x1)
#define IS_EXTERNAL(_ptr) (PtrBits(_ptr) & 0x1)


class nsDOMClassInfo : public nsIXPCScriptable,
                       public nsIClassInfo
{
public:
  nsDOMClassInfo(nsDOMClassInfoData* aData);
  virtual ~nsDOMClassInfo();

  NS_DECL_NSIXPCSCRIPTABLE

  NS_DECL_ISUPPORTS

  NS_DECL_NSICLASSINFO

  // Helper method that returns a *non* refcounted pointer to a
  // helper. So please note, don't release this pointer, if you do,
  // you better make sure you've addreffed before release.
  //
  // Whaaaaa! I wanted to name this method GetClassInfo, but nooo,
  // some of Microsoft devstudio's headers #defines GetClassInfo to
  // GetClassInfoA so I can't, those $%#@^! bastards!!! What gives
  // them the right to do that?

  static nsIClassInfo* GetClassInfoInstance(nsDOMClassInfoID aID);
  static nsIClassInfo* GetClassInfoInstance(nsDOMClassInfoData* aData);

  static void ShutDown();

  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsDOMClassInfo(aData);
  }

  static nsresult WrapNative(JSContext *cx, JSObject *scope,
                             nsISupports *native, const nsIID& aIID,
                             jsval *vp);
  static nsresult ThrowJSException(JSContext *cx, nsresult aResult);

protected:
  const nsDOMClassInfoData* mData;

  static nsresult Init();
  static nsresult RegisterClassName(PRInt32 aDOMClassInfoID);
  static nsresult RegisterClassProtos(PRInt32 aDOMClassInfoID);
  static nsresult RegisterExternalClasses();

  // Checks if id is a number and returns the number, if aIsNumber is
  // non-null it's set to true if the id is a number and false if it's
  // not a number. If id is not a number this method returns -1
  static PRInt32 GetArrayIndexFromId(JSContext *cx, jsval id,
                                     PRBool *aIsNumber = nsnull);

  static inline PRBool IsReadonlyReplaceable(JSString *str)
  {
    return (str == sTop_id          ||
            str == sScrollbars_id   ||
            str == sContent_id      ||
            str == sSidebar_id      ||
            str == sMenubar_id      ||
            str == sToolbar_id      ||
            str == sLocationbar_id  ||
            str == sPersonalbar_id  ||
            str == sStatusbar_id    ||
            str == sDirectories_id  ||
            str == sControllers_id  ||
            str == sScrollX_id      ||
            str == sScrollY_id      ||
            str == sLength_id);
  }

  static inline PRBool IsWritableReplaceable(JSString *str)
  {
    return (str == sInnerHeight_id  ||
            str == sInnerWidth_id   ||
            str == sOuterHeight_id  ||
            str == sOuterWidth_id   ||
            str == sScreenX_id      ||
            str == sScreenY_id      ||
            str == sStatus_id       ||
            str == sName_id);
  }

  static JSClass sDOMConstructorProtoClass;

  static nsIXPConnect *sXPConnect;
  static nsIScriptSecurityManager *sSecMan;

  // nsIXPCScriptable code
  static nsresult DefineStaticJSStrings(JSContext *cx);

  static PRBool sIsInitialized;

  static JSString *sTop_id;
  static JSString *sScrollbars_id;
  static JSString *sLocation_id;
  static JSString *sComponents_id;
  static JSString *s_content_id;
  static JSString *sContent_id;
  static JSString *sSidebar_id;
  static JSString *sMenubar_id;
  static JSString *sToolbar_id;
  static JSString *sLocationbar_id;
  static JSString *sPersonalbar_id;
  static JSString *sStatusbar_id;
  static JSString *sDirectories_id;
  static JSString *sControllers_id;
  static JSString *sLength_id;
  static JSString *sInnerHeight_id;
  static JSString *sInnerWidth_id;
  static JSString *sOuterHeight_id;
  static JSString *sOuterWidth_id;
  static JSString *sScreenX_id;
  static JSString *sScreenY_id;
  static JSString *sStatus_id;
  static JSString *sName_id;
  static JSString *sOnmousedown_id;
  static JSString *sOnmouseup_id;
  static JSString *sOnclick_id;
  static JSString *sOncontextmenu_id;
  static JSString *sOnmouseover_id;
  static JSString *sOnmouseout_id;
  static JSString *sOnkeydown_id;
  static JSString *sOnkeyup_id;
  static JSString *sOnkeypress_id;
  static JSString *sOnmousemove_id;
  static JSString *sOnfocus_id;
  static JSString *sOnblur_id;
  static JSString *sOnsubmit_id;
  static JSString *sOnreset_id;
  static JSString *sOnchange_id;
  static JSString *sOnselect_id;
  static JSString *sOnload_id;
  static JSString *sOnunload_id;
  static JSString *sOnabort_id;
  static JSString *sOnerror_id;
  static JSString *sOnpaint_id;
  static JSString *sOnresize_id;
  static JSString *sOnscroll_id;
  static JSString *sScrollIntoView_id;
  static JSString *sScrollX_id;
  static JSString *sScrollY_id;
  static JSString *sOpen_id;
  static JSString *sItem_id;
  static JSString *sEnumerate_id;
  static JSString *sNavigator_id;
  static JSString *sDocument_id;
  static JSString *sWindow_id;

  static const JSClass *sObjectClass;

  static PRBool sDoSecurityCheckInAddProperty;
};

typedef nsDOMClassInfo nsDOMGenericSH;


// EventProp scriptable helper, this class should be the base class of
// all objects that should support things like
// obj.onclick=function{...}

class nsEventRecieverSH : public nsDOMGenericSH
{
protected:
  nsEventRecieverSH(nsDOMClassInfoData* aData) : nsDOMGenericSH(aData)
  {
  }

  virtual ~nsEventRecieverSH()
  {
  }

  static PRBool ReallyIsEventName(JSString *jsstr, jschar aFirstChar);

  static inline PRBool IsEventName(JSString *jsstr)
  {
    jschar *str = ::JS_GetStringChars(jsstr);

    if (str[0] == 'o' && str[1] == 'n') {
      return ReallyIsEventName(jsstr, str[2]);
    }

    return PR_FALSE;
  }

  nsresult RegisterCompileHandler(nsIXPConnectWrappedNative *wrapper,
                                  JSContext *cx, JSObject *obj, jsval id,
                                  PRBool compile, PRBool *did_compile);

public:
  NS_IMETHOD NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                        JSObject *obj, jsval id, PRUint32 flags,
                        JSObject **objp, PRBool *_retval);
  NS_IMETHOD SetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsval id, jsval *vp,
                         PRBool *_retval);
};


// Window scriptable helper

class nsWindowSH : public nsEventRecieverSH
{
protected:
  nsWindowSH(nsDOMClassInfoData* aData) : nsEventRecieverSH(aData)
  {
  }

  virtual ~nsWindowSH()
  {
  }

  static nsresult GlobalResolve(nsISupports *aNative, JSContext *cx,
                                JSObject *obj, JSString *str, PRUint32 flags,
                                PRBool *did_resolve);
  static nsresult DefineInterfaceProperty(JSContext *cx, JSObject *obj,
                                          JSString *str);

  nsresult doCheckPropertyAccess(JSContext *cx, JSObject *obj, jsval id,
                                 nsIXPConnectWrappedNative *wrapper,
                                 PRUint32 accessMode);

public:
  NS_IMETHOD PreCreate(nsISupports *nativeObj, JSContext *cx,
                       JSObject *globalObj, JSObject **parentObj);
  NS_IMETHOD GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsval id, jsval *vp, PRBool *_retval);
  NS_IMETHOD SetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsval id, jsval *vp, PRBool *_retval);
  NS_IMETHOD AddProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsval id, jsval *vp, PRBool *_retval);
  NS_IMETHOD DelProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsval id, jsval *vp, PRBool *_retval);
  NS_IMETHOD NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                        JSObject *obj, jsval id, PRUint32 flags,
                        JSObject **objp, PRBool *_retval);
  NS_IMETHOD Finalize(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                      JSObject *obj);

  static nsresult CacheDocumentProperty(JSContext *cx, JSObject *obj,
                                        nsIDOMWindow *window);

  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsWindowSH(aData);
  }
};


// Location scriptable helper

class nsLocationSH : public nsDOMGenericSH
{
protected:
  nsLocationSH(nsDOMClassInfoData* aData) : nsDOMGenericSH(aData)
  {
  }

  virtual ~nsLocationSH()
  {
  }

public:
  NS_IMETHOD CheckAccess(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsval id, PRUint32 mode,
                         jsval *vp, PRBool *_retval);

  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsLocationSH(aData);
  }
};


// DOM Node helper, this class deals with setting the parent for the
// wrappers

class nsNodeSH : public nsEventRecieverSH
{
protected:
  nsNodeSH(nsDOMClassInfoData* aData) : nsEventRecieverSH(aData)
  {
  }

  virtual ~nsNodeSH()
  {
  }

public:
  NS_IMETHOD PreCreate(nsISupports *nativeObj, JSContext *cx,
                       JSObject *globalObj, JSObject **parentObj);
  NS_IMETHOD AddProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsval id, jsval *vp, PRBool *_retval);

  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsNodeSH(aData);
  }
};


// Element helper

class nsElementSH : public nsNodeSH
{
protected:
  nsElementSH(nsDOMClassInfoData* aData) : nsNodeSH(aData)
  {
  }

  virtual ~nsElementSH()
  {
  }

public:
  NS_IMETHOD PostCreate(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                    JSObject *obj);

  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsElementSH(aData);
  }
};


// NodeList scriptable helper

class nsArraySH : public nsDOMClassInfo
{
protected:
  nsArraySH(nsDOMClassInfoData* aData) : nsDOMClassInfo(aData)
  {
  }

  virtual ~nsArraySH()
  {
  }

  virtual nsresult GetItemAt(nsISupports *aNative, PRUint32 aIndex,
                             nsISupports **aResult);

public:
  NS_IMETHOD GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsval id, jsval *vp, PRBool *_retval);

  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsArraySH(aData);
  }
};


// NamedArray helper

class nsNamedArraySH : public nsArraySH
{
protected:
  nsNamedArraySH(nsDOMClassInfoData* aData) : nsArraySH(aData)
  {
  }

  virtual ~nsNamedArraySH()
  {
  }

  virtual nsresult GetNamedItem(nsISupports *aNative, const nsAString& aName,
                                nsISupports **aResult) = 0;

public:
  NS_IMETHOD GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsval id, jsval *vp, PRBool *_retval);
};


// NamedNodeMap helper

class nsNamedNodeMapSH : public nsNamedArraySH
{
protected:
  nsNamedNodeMapSH(nsDOMClassInfoData* aData) : nsNamedArraySH(aData)
  {
  }

  virtual ~nsNamedNodeMapSH()
  {
  }

  // Override nsArraySH::GetItemAt() since our list isn't a
  // nsIDOMNodeList
  virtual nsresult GetItemAt(nsISupports *aNative, PRUint32 aIndex,
                             nsISupports **aResult);

  // Override nsNamedArraySH::GetNamedItem()
  virtual nsresult GetNamedItem(nsISupports *aNative, const nsAString& aName,
                                nsISupports **aResult);

public:
  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsNamedNodeMapSH(aData);
  }
};


// HTMLCollection helper

class nsHTMLCollectionSH : public nsNamedArraySH
{
protected:
  nsHTMLCollectionSH(nsDOMClassInfoData* aData) : nsNamedArraySH(aData)
  {
  }

  virtual ~nsHTMLCollectionSH()
  {
  }

  // Override nsArraySH::GetItemAt() since our list isn't a
  // nsIDOMNodeList
  virtual nsresult GetItemAt(nsISupports *aNative, PRUint32 aIndex,
                             nsISupports **aResult);

  // Override nsNamedArraySH::GetNamedItem()
  virtual nsresult GetNamedItem(nsISupports *aNative, const nsAString& aName,
                                nsISupports **aResult);

public:
  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsHTMLCollectionSH(aData);
  }
};


// FomrControlList helper

class nsFormControlListSH : public nsHTMLCollectionSH
{
protected:
  nsFormControlListSH(nsDOMClassInfoData* aData) : nsHTMLCollectionSH(aData)
  {
  }

  virtual ~nsFormControlListSH()
  {
  }

  // Override nsNamedArraySH::GetNamedItem() since our NamedItem() can
  // return either a nsIDOMNode or a nsIHTMLCollection
  virtual nsresult GetNamedItem(nsISupports *aNative, const nsAString& aName,
                                nsISupports **aResult);

public:
  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsFormControlListSH(aData);
  }
};


// Document helper, for document.location and document.on*

class nsDocumentSH : public nsNodeSH
{
public:
  nsDocumentSH(nsDOMClassInfoData* aData) : nsNodeSH(aData)
  {
  }

  virtual ~nsDocumentSH()
  {
  }

public:
  NS_IMETHOD AddProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsval id, jsval *vp, PRBool *_retval);
  NS_IMETHOD DelProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsval id, jsval *vp, PRBool *_retval);
  NS_IMETHOD NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                        JSObject *obj, jsval id, PRUint32 flags,
                        JSObject **objp, PRBool *_retval);
  NS_IMETHOD SetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsval id, jsval *vp, PRBool *_retval);

  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsDocumentSH(aData);
  }
};


// HTMLDocument helper

class nsHTMLDocumentSH : public nsDocumentSH
{
protected:
  nsHTMLDocumentSH(nsDOMClassInfoData* aData) : nsDocumentSH(aData)
  {
  }

  virtual ~nsHTMLDocumentSH()
  {
  }

  static nsresult ResolveImpl(JSContext *cx,
                              nsIXPConnectWrappedNative *wrapper, jsval id,
                              nsISupports **result);
  static JSBool JS_DLL_CALLBACK DocumentOpen(JSContext *cx, JSObject *obj,
                                             uintN argc, jsval *argv,
                                             jsval *rval);

public:
  NS_IMETHOD NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                        JSObject *obj, jsval id, PRUint32 flags,
                        JSObject **objp, PRBool *_retval);
  NS_IMETHOD GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsval id, jsval *vp, PRBool *_retval);

  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsHTMLDocumentSH(aData);
  }
};


// HTMLElement helper

class nsHTMLElementSH : public nsElementSH
{
protected:
  nsHTMLElementSH(nsDOMClassInfoData* aData) : nsElementSH(aData)
  {
  }

  virtual ~nsHTMLElementSH()
  {
  }

  static JSBool JS_DLL_CALLBACK ScrollIntoView(JSContext *cx, JSObject *obj,
                                               uintN argc, jsval *argv,
                                               jsval *rval);

public:
  NS_IMETHOD NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                        JSObject *obj, jsval id, PRUint32 flags,
                        JSObject **objp, PRBool *_retval);

  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsHTMLElementSH(aData);
  }
};


// HTMLFormElement helper

class nsHTMLFormElementSH : public nsHTMLElementSH
{
protected:
  nsHTMLFormElementSH(nsDOMClassInfoData* aData) : nsHTMLElementSH(aData)
  {
  }

  virtual ~nsHTMLFormElementSH()
  {
  }

  static nsresult FindNamedItem(nsIForm *aForm, JSString *str,
                                nsISupports **aResult);

public:
  NS_IMETHOD NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                        JSObject *obj, jsval id, PRUint32 flags,
                        JSObject **objp, PRBool *_retval);
  NS_IMETHOD GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsval id, jsval *vp,
                         PRBool *_retval);

  NS_IMETHOD NewEnumerate(nsIXPConnectWrappedNative *wrapper,
                          JSContext *cx, JSObject *obj,
                          PRUint32 enum_op, jsval *statep,
                          jsid *idp, PRBool *_retval);

  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsHTMLFormElementSH(aData);
  }
};


// HTMLSelectElement helper

class nsHTMLSelectElementSH : public nsHTMLElementSH
{
protected:
  nsHTMLSelectElementSH(nsDOMClassInfoData* aData) : nsHTMLElementSH(aData)
  {
  }

  virtual ~nsHTMLSelectElementSH()
  {
  }

public:
  NS_IMETHOD GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsval id, jsval *vp,
                         PRBool *_retval);
  NS_IMETHOD SetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsval id, jsval *vp, PRBool *_retval);

  static nsresult SetOption(JSContext *cx, jsval *vp, PRUint32 aIndex,
                            nsIDOMNSHTMLOptionCollection *aOptCollection);

  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsHTMLSelectElementSH(aData);
  }
};


// Base helper for external HTML object (such as a plugin or an
// applet)

class nsHTMLExternalObjSH : public nsHTMLElementSH
{
protected:
  nsHTMLExternalObjSH(nsDOMClassInfoData* aData) : nsHTMLElementSH(aData)
  {
  }

  virtual ~nsHTMLExternalObjSH()
  {
  }

  nsresult GetPluginInstance(nsIXPConnectWrappedNative *aWrapper,
                             nsIPluginInstance **aResult);

  virtual nsresult GetPluginJSObject(JSContext *cx, JSObject *obj,
                                     nsIPluginInstance *plugin_inst,
                                     JSObject **plugin_obj,
                                     JSObject **plugin_proto) = 0;

public:
  NS_IMETHOD PostCreate(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                        JSObject *obj);
};


// HTMLAppletElement helper

class nsHTMLAppletElementSH : public nsHTMLExternalObjSH
{
protected:
  nsHTMLAppletElementSH(nsDOMClassInfoData* aData) : nsHTMLExternalObjSH(aData)
  {
  }

  virtual ~nsHTMLAppletElementSH()
  {
  }

  virtual nsresult GetPluginJSObject(JSContext *cx, JSObject *obj,
                                     nsIPluginInstance *plugin_inst,
                                     JSObject **plugin_obj,
                                     JSObject **plugin_proto);

public:
  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsHTMLAppletElementSH(aData);
  }
};


// HTMLEmbed/ObjectElement helper

class nsHTMLPluginObjElementSH : public nsHTMLAppletElementSH
{
protected:
  nsHTMLPluginObjElementSH(nsDOMClassInfoData* aData) : nsHTMLAppletElementSH(aData)
  {
  }

  virtual ~nsHTMLPluginObjElementSH()
  {
  }

  virtual nsresult GetPluginJSObject(JSContext *cx, JSObject *obj,
                                     nsIPluginInstance *plugin_inst,
                                     JSObject **plugin_obj,
                                     JSObject **plugin_proto);

public:
  NS_IMETHOD NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                        JSObject *obj, jsval id, PRUint32 flags,
                        JSObject **objp, PRBool *_retval);

  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsHTMLPluginObjElementSH(aData);
  }
};


// HTMLOptionCollection helper

class nsHTMLOptionCollectionSH : public nsHTMLCollectionSH
{
protected:
  nsHTMLOptionCollectionSH(nsDOMClassInfoData* aData) : nsHTMLCollectionSH(aData)
  {
  }

  virtual ~nsHTMLOptionCollectionSH()
  {
  }

public:
  NS_IMETHOD SetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsval id, jsval *vp, PRBool *_retval);

  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsHTMLOptionCollectionSH(aData);
  }
};


// Plugin helper

class nsPluginSH : public nsNamedArraySH
{
protected:
  nsPluginSH(nsDOMClassInfoData* aData) : nsNamedArraySH(aData)
  {
  }

  virtual ~nsPluginSH()
  {
  }

  // Override nsArraySH::GetItemAt() since our list isn't a
  // nsIDOMNodeList
  virtual nsresult GetItemAt(nsISupports *aNative, PRUint32 aIndex,
                             nsISupports **aResult);

  // Override nsNamedArraySH::GetNamedItem()
  virtual nsresult GetNamedItem(nsISupports *aNative, const nsAString& aName,
                                nsISupports **aResult);

public:
  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsPluginSH(aData);
  }
};


// PluginArray helper

class nsPluginArraySH : public nsNamedArraySH
{
protected:
  nsPluginArraySH(nsDOMClassInfoData* aData) : nsNamedArraySH(aData)
  {
  }

  virtual ~nsPluginArraySH()
  {
  }

  // Override nsArraySH::GetItemAt() since our list isn't a
  // nsIDOMNodeList
  virtual nsresult GetItemAt(nsISupports *aNative, PRUint32 aIndex,
                             nsISupports **aResult);

  // Override nsNamedArraySH::GetNamedItem()
  virtual nsresult GetNamedItem(nsISupports *aNative, const nsAString& aName,
                                nsISupports **aResult);

public:
  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsPluginArraySH(aData);
  }
};


// MimeTypeArray helper

class nsMimeTypeArraySH : public nsNamedArraySH
{
protected:
  nsMimeTypeArraySH(nsDOMClassInfoData* aData) : nsNamedArraySH(aData)
  {
  }

  virtual ~nsMimeTypeArraySH()
  {
  }

  // Override nsArraySH::GetItemAt() since our list isn't a
  // nsIDOMNodeList
  virtual nsresult GetItemAt(nsISupports *aNative, PRUint32 aIndex,
                             nsISupports **aResult);

  // Override nsNamedArraySH::GetNamedItem()
  virtual nsresult GetNamedItem(nsISupports *aNative, const nsAString& aName,
                                nsISupports **aResult);

public:
  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsMimeTypeArraySH(aData);
  }
};


// History helper

class nsStringArraySH : public nsDOMClassInfo
{
protected:
  nsStringArraySH(nsDOMClassInfoData* aData) : nsDOMClassInfo(aData)
  {
  }

  virtual ~nsStringArraySH()
  {
  }

  virtual nsresult GetStringAt(nsISupports *aNative, PRInt32 aIndex,
                               nsAString& aResult) = 0;

public:
  NS_IMETHOD GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsval id, jsval *vp, PRBool *_retval);
};


// History helper

class nsHistorySH : public nsStringArraySH
{
protected:
  nsHistorySH(nsDOMClassInfoData* aData) : nsStringArraySH(aData)
  {
  }

  virtual ~nsHistorySH()
  {
  }

  virtual nsresult GetStringAt(nsISupports *aNative, PRInt32 aIndex,
                               nsAString& aResult);

public:
  NS_IMETHOD GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsval id, jsval *vp, PRBool *_retval);

  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsHistorySH(aData);
  }
};


// MediaList helper

class nsMediaListSH : public nsStringArraySH
{
protected:
  nsMediaListSH(nsDOMClassInfoData* aData) : nsStringArraySH(aData)
  {
  }

  virtual ~nsMediaListSH()
  {
  }

  virtual nsresult GetStringAt(nsISupports *aNative, PRInt32 aIndex,
                               nsAString& aResult);

public:
  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsMediaListSH(aData);
  }
};


// StyleSheetList helper

class nsStyleSheetListSH : public nsArraySH
{
protected:
  nsStyleSheetListSH(nsDOMClassInfoData* aData) : nsArraySH(aData)
  {
  }

  virtual ~nsStyleSheetListSH()
  {
  }

  // Override nsArraySH::GetItemAt() since our list isn't a
  // nsIDOMNodeList
  virtual nsresult GetItemAt(nsISupports *aNative, PRUint32 aIndex,
                             nsISupports **aResult);

public:
  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsStyleSheetListSH(aData);
  }
};


// CSSStyleDeclaration helper

class nsCSSStyleDeclSH : public nsStringArraySH
{
protected:
  nsCSSStyleDeclSH(nsDOMClassInfoData* aData) : nsStringArraySH(aData)
  {
  }

  virtual ~nsCSSStyleDeclSH()
  {
  }

  virtual nsresult GetStringAt(nsISupports *aNative, PRInt32 aIndex,
                               nsAString& aResult);

public:
  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsCSSStyleDeclSH(aData);
  }
};


// CSSRuleList helper

class nsCSSRuleListSH : public nsArraySH
{
protected:
  nsCSSRuleListSH(nsDOMClassInfoData* aData) : nsArraySH(aData)
  {
  }

  virtual ~nsCSSRuleListSH()
  {
  }

  // Override nsArraySH::GetItemAt() since our list isn't a
  // nsIDOMNodeList
  virtual nsresult GetItemAt(nsISupports *aNative, PRUint32 aIndex,
                             nsISupports **aResult);

public:
  static nsIClassInfo *doCreate(nsDOMClassInfoData* aData)
  {
    return new nsCSSRuleListSH(aData);
  }
};


// Event handler 'this' translator class, this is called by XPConnect
// when a "function interface" (nsIDOMEventListener) is called, this
// class extracts 'this' fomr the first argument to the called
// function (nsIDOMEventListener::HandleEvent(in nsIDOMEvent)), this
// class will pass back nsIDOMEvent::currentTarget to be used as
// 'this'.

class nsEventListenerThisTranslator : public nsIXPCFunctionThisTranslator
{
public:
  nsEventListenerThisTranslator()
  {
    NS_INIT_ISUPPORTS();
  }

  virtual ~nsEventListenerThisTranslator()
  {
  }

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIXPCFunctionThisTranslator
  NS_DECL_NSIXPCFUNCTIONTHISTRANSLATOR
};


void InvalidateContextAndWrapperCache();


/**
 * nsIClassInfo helper macros
 */

#define NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(_class)                          \
  if (aIID.Equals(NS_GET_IID(nsIClassInfo))) {                                \
    foundInterface =                                                          \
      nsDOMClassInfo::GetClassInfoInstance(eDOMClassInfo_##_class##_id);      \
    NS_ENSURE_TRUE(foundInterface, NS_ERROR_OUT_OF_MEMORY);                   \
                                                                              \
    *aInstancePtr = foundInterface;                                           \
                                                                              \
    return NS_OK;                                                             \
  } else

#endif /* nsDOMClassInfo_h___ */
