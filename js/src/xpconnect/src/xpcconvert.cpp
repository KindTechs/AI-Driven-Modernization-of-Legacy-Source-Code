/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   John Bandhauer <jband@netscape.com> (original author)
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Mike Shaver <shaver@mozilla.org>
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

/* Data conversion between native and JavaScript types. */

#include "xpcprivate.h"

//#define STRICT_CHECK_OF_UNICODE
#ifdef STRICT_CHECK_OF_UNICODE
#define ILLEGAL_RANGE(c) (0!=((c) & 0xFF80))
#else // STRICT_CHECK_OF_UNICODE
#define ILLEGAL_RANGE(c) (0!=((c) & 0xFF00))
#endif // STRICT_CHECK_OF_UNICODE

#define ILLEGAL_CHAR_RANGE(c) (0!=((c) & 0x80))
/*
* This is a table driven scheme to determine if the types of the params of the
* given method exclude that method from being reflected via XPConnect.
*
* The table can be appended and modified as requirements change. However...
*
* The table ASSUMES that all the type idenetifiers are contiguous starting
* at ZERO. And, it also ASSUMES that the additional criteria of whether or
* not a give type is reflectable are its use as a pointer and/or 'out' type.
*
* The table has a row for each type and columns for the combinations of
* that type being used as a pointer type and/or as an 'out' param.
*/

#define XPC_MK_BIT(p,o) (1 << (((p)?1:0)+((o)?2:0)))
#define XPC_IS_REFLECTABLE(f, p, o) ((f) & XPC_MK_BIT((p),(o)))
#define XPC_MK_FLAG(np_no,p_no,np_o,p_o) \
        ((uint8)((np_no) | ((p_no) << 1) | ((np_o) << 2) | ((p_o) << 3)))

/***********************************************************/
// xpt uses 5 bits for this info. We deal with the possibility that
// some new types might exist that we don't know about.

#define XPC_FLAG_COUNT (1 << 5)

/* '1' means 'reflectable'. '0' means 'not reflectable'.        */
static uint8 xpc_reflectable_flags[XPC_FLAG_COUNT] = {
    /* 'p' stands for 'pointer' and 'o' stands for 'out'        */
    /*          !p&!o, p&!o, !p&o, p&o                          */
    XPC_MK_FLAG(  1  ,  1  ,   1 ,  0 ), /* T_I8                */
    XPC_MK_FLAG(  1  ,  1  ,   1 ,  0 ), /* T_I16               */
    XPC_MK_FLAG(  1  ,  1  ,   1 ,  0 ), /* T_I32               */
    XPC_MK_FLAG(  1  ,  1  ,   1 ,  0 ), /* T_I64               */
    XPC_MK_FLAG(  1  ,  1  ,   1 ,  0 ), /* T_U8                */
    XPC_MK_FLAG(  1  ,  1  ,   1 ,  0 ), /* T_U16               */
    XPC_MK_FLAG(  1  ,  1  ,   1 ,  0 ), /* T_U32               */
    XPC_MK_FLAG(  1  ,  1  ,   1 ,  0 ), /* T_U64               */
    XPC_MK_FLAG(  1  ,  1  ,   1 ,  0 ), /* T_FLOAT             */
    XPC_MK_FLAG(  1  ,  1  ,   1 ,  0 ), /* T_DOUBLE            */
    XPC_MK_FLAG(  1  ,  1  ,   1 ,  0 ), /* T_BOOL              */
    XPC_MK_FLAG(  1  ,  1  ,   1 ,  0 ), /* T_CHAR              */
    XPC_MK_FLAG(  1  ,  1  ,   1 ,  0 ), /* T_WCHAR             */
    XPC_MK_FLAG(  0  ,  0  ,   0 ,  0 ), /* T_VOID              */
    XPC_MK_FLAG(  0  ,  1  ,   0 ,  1 ), /* T_IID               */
    XPC_MK_FLAG(  0  ,  1  ,   0 ,  0 ), /* T_DOMSTRING         */
    XPC_MK_FLAG(  0  ,  1  ,   0 ,  1 ), /* T_CHAR_STR          */
    XPC_MK_FLAG(  0  ,  1  ,   0 ,  1 ), /* T_WCHAR_STR         */
    XPC_MK_FLAG(  0  ,  1  ,   0 ,  1 ), /* T_INTERFACE         */
    XPC_MK_FLAG(  0  ,  1  ,   0 ,  1 ), /* T_INTERFACE_IS      */
    XPC_MK_FLAG(  0  ,  1  ,   0 ,  1 ), /* T_ARRAY             */
    XPC_MK_FLAG(  0  ,  1  ,   0 ,  1 ), /* T_PSTRING_SIZE_IS   */
    XPC_MK_FLAG(  0  ,  1  ,   0 ,  1 ), /* T_PWSTRING_SIZE_IS  */
    XPC_MK_FLAG(  0  ,  1  ,   0 ,  0 ), /* T_UTF8STRING        */
    XPC_MK_FLAG(  0  ,  1  ,   0 ,  0 ), /* T_CSTRING           */
    XPC_MK_FLAG(  0  ,  1  ,   0 ,  0 ), /* T_ASTRING           */
    XPC_MK_FLAG(  0  ,  0  ,   0 ,  0 ), /* 26 - reserved       */
    XPC_MK_FLAG(  0  ,  0  ,   0 ,  0 ), /* 27 - reserved       */
    XPC_MK_FLAG(  0  ,  0  ,   0 ,  0 ), /* 28 - reserved       */
    XPC_MK_FLAG(  0  ,  0  ,   0 ,  0 ), /* 29 - reserved       */
    XPC_MK_FLAG(  0  ,  0  ,   0 ,  0 ), /* 30 - reserved       */
    XPC_MK_FLAG(  0  ,  0  ,   0 ,  0 )  /* 31 - reserved       */
    };

static intN sXPCOMUCStringFinalizerIndex = -1;

/***********************************************************/

// static
JSBool
XPCConvert::IsMethodReflectable(const nsXPTMethodInfo& info)
{
    if(info.IsNotXPCOM() || info.IsHidden())
        return JS_FALSE;

    for(int i = info.GetParamCount()-1; i >= 0; i--)
    {
        const nsXPTParamInfo& param = info.GetParam(i);
        const nsXPTType& type = param.GetType();

        uint8 base_type = type.TagPart();
        NS_ASSERTION(base_type < XPC_FLAG_COUNT, "BAD TYPE");

        if(!XPC_IS_REFLECTABLE(xpc_reflectable_flags[base_type],
                               type.IsPointer(), param.IsOut()))
            return JS_FALSE;
    }
    return JS_TRUE;
}

/***************************************************************************/

static JSBool
GetISupportsFromJSObject(JSContext* cx, JSObject* obj, nsISupports** iface)
{
    JSClass* jsclass = JS_GET_CLASS(cx, obj);
    NS_ASSERTION(jsclass, "obj has no class");
    if(jsclass &&
       (jsclass->flags & JSCLASS_HAS_PRIVATE) &&
       (jsclass->flags & JSCLASS_PRIVATE_IS_NSISUPPORTS))
    {
        *iface = (nsISupports*) JS_GetPrivate(cx, obj);
        return JS_TRUE;
    }
    return JS_FALSE;
}

/***************************************************************************/

/*
* Support for 64 bit conversions where 'long long' not supported.
* (from John Fairhurst <mjf35@cam.ac.uk>)
*/

#ifdef HAVE_LONG_LONG

#define JAM_DOUBLE(cx,v,d) (d = JS_NewDouble(cx, (jsdouble)v) , \
                            d ? DOUBLE_TO_JSVAL(d) : JSVAL_ZERO)
// Win32 can't handle uint64 to double conversion
#define JAM_DOUBLE_U64(cx,v,d) JAM_DOUBLE(cx,((int64)v),d)

#else

inline jsval
JAM_DOUBLE(JSContext *cx, const int64 &v, jsdouble *dbl)
{
    jsdouble d;
    LL_L2D(d, v);
    dbl = JS_NewDouble(cx, d);
    if(!dbl)
        return JSVAL_ZERO;
    return DOUBLE_TO_JSVAL(dbl);
}

inline jsval
JAM_DOUBLE(JSContext *cx, double v, jsdouble *dbl)
{
    dbl = JS_NewDouble(cx, (jsdouble)v);
    if(!dbl)
        return JSVAL_ZERO;
    return DOUBLE_TO_JSVAL(dbl);
}

// if !HAVE_LONG_LONG, then uint64 is a typedef of int64
#define JAM_DOUBLE_U64(cx,v,d) JAM_DOUBLE(cx,v,d)

#endif

#define FIT_32(cx,i,d) (INT_FITS_IN_JSVAL(i) ? \
                        INT_TO_JSVAL(i) : JAM_DOUBLE(cx,i,d))

// XXX will this break backwards compatability???
#define FIT_U32(cx,i,d) ((i) <= JSVAL_INT_MAX ? \
                         INT_TO_JSVAL(i) : JAM_DOUBLE(cx,i,d))

JS_STATIC_DLL_CALLBACK(void)
FinalizeXPCOMUCString(JSContext *cx, JSString *str)
{
    NS_ASSERTION(sXPCOMUCStringFinalizerIndex != -1,
                 "XPCConvert: XPCOM Unicode string finalizer called uninitialized!");

    jschar* buffer = JS_GetStringChars(str);
    nsMemory::Free(buffer);
}


static JSBool
AddXPCOMUCStringFinalizer()
{

    sXPCOMUCStringFinalizerIndex =
        JS_AddExternalStringFinalizer(FinalizeXPCOMUCString);

    if (sXPCOMUCStringFinalizerIndex == -1)
    {        
        return JS_FALSE;
    }

    return JS_TRUE;
}

//static
void
XPCConvert::RemoveXPCOMUCStringFinalizer()
{
    JS_RemoveExternalStringFinalizer(FinalizeXPCOMUCString);
    sXPCOMUCStringFinalizerIndex = -1;
}

// static
JSBool
XPCConvert::NativeData2JS(XPCCallContext& ccx, jsval* d, const void* s,
                          const nsXPTType& type, const nsID* iid,
                          JSObject* scope, nsresult* pErr)
{
    NS_PRECONDITION(s, "bad param");
    NS_PRECONDITION(d, "bad param");

    JSContext* cx = ccx.GetJSContext();

    jsdouble* dbl = nsnull;

    if(pErr)
        *pErr = NS_ERROR_XPC_BAD_CONVERT_NATIVE;

    switch(type.TagPart())
    {
    case nsXPTType::T_I8    : *d = INT_TO_JSVAL((int32)*((int8*)s));     break;
    case nsXPTType::T_I16   : *d = INT_TO_JSVAL((int32)*((int16*)s));    break;
    case nsXPTType::T_I32   : *d = FIT_32(cx,*((int32*)s),dbl);          break;
    case nsXPTType::T_I64   : *d = JAM_DOUBLE(cx,*((int64*)s),dbl);      break;
    case nsXPTType::T_U8    : *d = INT_TO_JSVAL((int32)*((uint8*)s));    break;
    case nsXPTType::T_U16   : *d = INT_TO_JSVAL((int32)*((uint16*)s));   break;
    case nsXPTType::T_U32   : *d = FIT_U32(cx,*((uint32*)s),dbl);        break;
    case nsXPTType::T_U64   : *d = JAM_DOUBLE_U64(cx,*((uint64*)s),dbl); break;
    case nsXPTType::T_FLOAT : *d = JAM_DOUBLE(cx,*((float*)s),dbl);      break;
    case nsXPTType::T_DOUBLE: *d = JAM_DOUBLE(cx,*((double*)s),dbl);     break;
    case nsXPTType::T_BOOL  : *d = *((PRBool*)s)?JSVAL_TRUE:JSVAL_FALSE; break;
    case nsXPTType::T_CHAR  :
        {
            char* p = (char*)s;
            if(!p)
                return JS_FALSE;

#ifdef STRICT_CHECK_OF_UNICODE
            NS_ASSERTION(! ILLEGAL_CHAR_RANGE(p) , "passing non ASCII data");
#endif // STRICT_CHECK_OF_UNICODE

            JSString* str;
            if(!(str = JS_NewStringCopyN(cx, p, 1)))
                return JS_FALSE;
            *d = STRING_TO_JSVAL(str);
            break;
        }
    case nsXPTType::T_WCHAR :
        {
            jschar* p = (jschar*)s;
            if(!p)
                return JS_FALSE;
            JSString* str;
            if(!(str = JS_NewUCStringCopyN(cx, p, 1)))
                return JS_FALSE;
            *d = STRING_TO_JSVAL(str);
            break;
        }
    default:
        if(!type.IsPointer())
        {
            XPC_LOG_ERROR(("XPCConvert::NativeData2JS : unsupported type"));
            return JS_FALSE;
        }

        // set the default result
        *d = JSVAL_NULL;

        switch(type.TagPart())
        {
        case nsXPTType::T_VOID:
            XPC_LOG_ERROR(("XPCConvert::NativeData2JS : void* params not supported"));
            return JS_FALSE;

        case nsXPTType::T_IID:
            {
                nsID* iid2 = *((nsID**)s);
                if(!iid2)
                    break;
                JSObject* obj;
                if(!(obj = xpc_NewIDObject(cx, scope, *iid2)))
                    return JS_FALSE;
                *d = OBJECT_TO_JSVAL(obj);
                break;
            }

        case nsXPTType::T_ASTRING:
            // Fall through to T_DOMSTRING case

        case nsXPTType::T_DOMSTRING:
            {
                const nsAString* p = *((const nsAString**)s);
                if(!p)
                    break;

                if(!p->IsVoid()) {
                    JSString *str =
                        XPCStringConvert::ReadableToJSString(cx, *p);
                    if(!str)
                        return JS_FALSE;

                    *d = STRING_TO_JSVAL(str);
                }

                // *d is defaulted to JSVAL_NULL so no need to set it
                // again if p is a "void" string

                break;
            }

        case nsXPTType::T_CHAR_STR:
            {
                char* p = *((char**)s);
                if(!p)
                    break;

#ifdef STRICT_CHECK_OF_UNICODE
                PRBool isAscii = PR_TRUE;
                char* t;
                for(t=p; *t && isAscii ; t++) {
                  if(ILLEGAL_CHAR_RANGE(*t))
                      isAscii = PR_FALSE;
                }
                NS_ASSERTION(isAscii, "passing non ASCII data");
#endif // STRICT_CHECK_OF_UNICODE
                JSString* str;
                if(!(str = JS_NewStringCopyZ(cx, p)))
                    return JS_FALSE;
                *d = STRING_TO_JSVAL(str);
                break;
            }

        case nsXPTType::T_WCHAR_STR:
            {
                jschar* p = *((jschar**)s);
                if(!p)
                    break;
                JSString* str;
                if(!(str = JS_NewUCStringCopyZ(cx, p)))
                    return JS_FALSE;
                *d = STRING_TO_JSVAL(str);
                break;
            }
        case nsXPTType::T_UTF8STRING:
            {                          
                const nsACString* cString = *((const nsACString**)s);

                if(!cString)
                    break;
                
                if(!cString->IsVoid()) 
                {
                    // XXX There is an extra copy happening here.  When the
                    // UTF8String implementation lands, it should contain
                    // a mechanism to avoid this extra copy.  Jag will change
                    // this code to use that new mechanism when he lands the
                    // UTF8String changes.
                    NS_ConvertUTF8toUCS2 unicodeString(*cString);

                    JSString* jsString = JS_NewUCStringCopyN(cx,
                                         (jschar*)unicodeString.get(),
                                         unicodeString.Length());

                    if(!jsString)
                        return JS_FALSE; 

                    *d = STRING_TO_JSVAL(jsString);
                }

                break;

            }
        case nsXPTType::T_CSTRING:
            {                          
                const nsACString* cString = *((const nsACString**)s);

                if(!cString)
                    break;
                
                if(!cString->IsVoid()) 
                {
                    PRUnichar* unicodeString = ToNewUnicode(*cString);
                    if(!unicodeString)
                        return JS_FALSE;

                    if(sXPCOMUCStringFinalizerIndex == -1 && 
                       !AddXPCOMUCStringFinalizer())
                        return JS_FALSE;

                    JSString* jsString = JS_NewExternalString(cx,
                                             (jschar*)unicodeString,
                                             cString->Length(),
                                             sXPCOMUCStringFinalizerIndex);

                    if(!jsString)
                    {
                        nsMemory::Free(unicodeString);
                        return JS_FALSE;
                    }

                    *d = STRING_TO_JSVAL(jsString);
                }

                break;
            }

        case nsXPTType::T_INTERFACE:
        case nsXPTType::T_INTERFACE_IS:
            {
                nsISupports* iface = *((nsISupports**)s);
                if(iface)
                {
                    if(iid->Equals(NS_GET_IID(nsIVariant)))
                    {
                        nsCOMPtr<nsIVariant> variant = do_QueryInterface(iface);
                        if(!variant)
                            return JS_FALSE;

                        return XPCVariant::VariantDataToJS(ccx, variant, 
                                                           scope, pErr, d);
                    }
                    // else...
                    
                    nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
                    if(!NativeInterface2JSObject(ccx, getter_AddRefs(holder),
                                                 iface, iid, scope, pErr))
                        return JS_FALSE;

                    if(holder)
                    {
                        JSObject* jsobj;
                        if(NS_FAILED(holder->GetJSObject(&jsobj)))
                            return JS_FALSE;
                        *d = OBJECT_TO_JSVAL(jsobj);
                    }
                }
                break;
            }
        default:
            NS_ASSERTION(0, "bad type");
            return JS_FALSE;
        }
    }
    return JS_TRUE;
}

/***************************************************************************/

// static
JSBool
XPCConvert::JSData2Native(XPCCallContext& ccx, void* d, jsval s,
                          const nsXPTType& type,
                          JSBool useAllocator, const nsID* iid,
                          nsresult* pErr)
{
    NS_PRECONDITION(d, "bad param");

    JSContext* cx = ccx.GetJSContext();

    int32    ti;
    uint32   tu;
    jsdouble td;
    JSBool isDOMString = JS_TRUE;

    if(pErr)
        *pErr = NS_ERROR_XPC_BAD_CONVERT_JS;

    switch(type.TagPart())
    {
    case nsXPTType::T_I8     :
        if(!JS_ValueToECMAInt32(cx, s, &ti))
            return JS_FALSE;
        *((int8*)d)  = (int8) ti;
        break;
    case nsXPTType::T_I16    :
        if(!JS_ValueToECMAInt32(cx, s, &ti))
            return JS_FALSE;
        *((int16*)d)  = (int16) ti;
        break;
    case nsXPTType::T_I32    :
        if(!JS_ValueToECMAInt32(cx, s, (int32*)d))
            return JS_FALSE;
        break;
    case nsXPTType::T_I64    :
        if(JSVAL_IS_INT(s))
        {
            if(!JS_ValueToECMAInt32(cx, s, &ti))
                return JS_FALSE;
            LL_I2L(*((int64*)d),ti);

        }
        else
        {
            if(!JS_ValueToNumber(cx, s, &td))
                return JS_FALSE;
            LL_D2L(*((int64*)d),td);
        }
        break;
    case nsXPTType::T_U8     :
        if(!JS_ValueToECMAUint32(cx, s, &tu))
            return JS_FALSE;
        *((uint8*)d)  = (uint8) tu;
        break;
    case nsXPTType::T_U16    :
        if(!JS_ValueToECMAUint32(cx, s, &tu))
            return JS_FALSE;
        *((uint16*)d)  = (uint16) tu;
        break;
    case nsXPTType::T_U32    :
        if(!JS_ValueToECMAUint32(cx, s, (uint32*)d))
            return JS_FALSE;
        break;
    case nsXPTType::T_U64    :
        if(JSVAL_IS_INT(s))
        {
            if(!JS_ValueToECMAUint32(cx, s, &tu))
                return JS_FALSE;
            LL_UI2L(*((int64*)d),tu);
        }
        else
        {
            if(!JS_ValueToNumber(cx, s, &td))
                return JS_FALSE;
#ifdef XP_WIN
            // Note: Win32 can't handle double to uint64 directly
            *((uint64*)d) = (uint64)((int64) td);
#else
            LL_D2L(*((uint64*)d),td);
#endif
        }
        break;
    case nsXPTType::T_FLOAT  :
        if(!JS_ValueToNumber(cx, s, &td))
            return JS_FALSE;
        *((float*)d) = (float) td;
        break;
    case nsXPTType::T_DOUBLE :
        if(!JS_ValueToNumber(cx, s, (double*)d))
            return JS_FALSE;
        break;
    case nsXPTType::T_BOOL   :
        if(!JS_ValueToBoolean(cx, s, (JSBool*)d))
            return JS_FALSE;
        break;
    case nsXPTType::T_CHAR   :
        {
            char* bytes=nsnull;
            JSString* str;

            if(!(str = JS_ValueToString(cx, s))||
               !(bytes = JS_GetStringBytes(str)))
            {
                return JS_FALSE;
            }
#ifdef DEBUG
            jschar* chars=nsnull;
            if(nsnull!=(chars = JS_GetStringChars(str)))
            {
                NS_ASSERTION((! ILLEGAL_RANGE(chars[0])),"U+0080/U+0100 - U+FFFF data lost");
            }
#endif // DEBUG
            *((char*)d) = bytes[0];
            break;
        }
    case nsXPTType::T_WCHAR  :
        {
            jschar* chars=nsnull;
            JSString* str;
            if(!(str = JS_ValueToString(cx, s))||
               !(chars = JS_GetStringChars(str)))
            {
                return JS_FALSE;
            }
            *((uint16*)d)  = (uint16) chars[0];
            break;
        }
    default:
        if(!type.IsPointer())
        {
            NS_ASSERTION(0,"unsupported type");
            return JS_FALSE;
        }

        switch(type.TagPart())
        {
        case nsXPTType::T_VOID:
            XPC_LOG_ERROR(("XPCConvert::JSData2Native : void* params not supported"));
            NS_ASSERTION(0,"void* params not supported");
            return JS_FALSE;
        case nsXPTType::T_IID:
        {
            NS_ASSERTION(useAllocator,"trying to convert a JSID to nsID without allocator : this would leak");

            JSObject* obj;
            const nsID* pid=nsnull;

            if(JSVAL_IS_VOID(s) || JSVAL_IS_NULL(s))
            {
                if(type.IsReference())
                {
                    if(pErr)
                        *pErr = NS_ERROR_XPC_BAD_CONVERT_JS_NULL_REF;
                    return JS_FALSE;
                }
                // else ...
                *((const nsID**)d) = nsnull;
                return JS_TRUE;
            }

            if(!JSVAL_IS_OBJECT(s) ||
               (!(obj = JSVAL_TO_OBJECT(s))) ||
               (!(pid = xpc_JSObjectToID(cx, obj))))
            {
                return JS_FALSE;
            }
            *((const nsID**)d) = pid;
            return JS_TRUE;
        }

        case nsXPTType::T_ASTRING:        
        {            
            isDOMString = JS_FALSE;
            // Fall through to T_DOMSTRING case.
        }
        case nsXPTType::T_DOMSTRING:
        {
            static const NS_NAMED_LITERAL_STRING(sEmptyString, "");
            static const NS_NAMED_LITERAL_STRING(sVoidString, "undefined");

            const PRUnichar* chars;
            JSString* str = nsnull;
            JSBool isNewString = JS_FALSE;
            PRUint32 length;

            if(JSVAL_IS_VOID(s))
            {
                if(isDOMString) 
                {
                    chars  = sVoidString.get();
                    length = sVoidString.Length();
                }
                else
                {
                    chars = sEmptyString.get();
                    length = 0;
                }
            }
            else if(!JSVAL_IS_NULL(s))
            {
                str = JS_ValueToString(cx, s);
                if(!str)
                    return JS_FALSE;

                length = (PRUint32) JS_GetStringLength(str);
                if(length)
                {
                    chars = (const PRUnichar*) JS_GetStringChars(str);
                    if(!chars)
                        return JS_FALSE;
                    if(STRING_TO_JSVAL(str) != s)
                        isNewString = JS_TRUE;
                }
                else
                {
                    str = nsnull;
                    chars = sEmptyString.get();
                }
            }

            if(useAllocator)
            {
                if(str)
                {
                    XPCReadableJSStringWrapper *wrapper =
                        XPCStringConvert::JSStringToReadable(str);
                    if(!wrapper)
                        return JS_FALSE;

                    // Ask for the shared buffer handle, which will root the
                    // string.
                    if(isNewString && ! wrapper->GetSharedBufferHandle())
                        return JS_FALSE;

                    *((const nsAString**)d) = wrapper;
                }
                else if(JSVAL_IS_NULL(s))
                {
                    XPCReadableJSStringWrapper *wrapper =
                        new XPCReadableJSStringWrapper();
                    if(!wrapper)
                        return JS_FALSE;

                    *((const nsAString**)d) = wrapper;
                }
                else
                {
                    const nsAString *rs = new nsAutoString(chars, length);
                    if(!rs)
                        return JS_FALSE;
                    *((const nsAString**)d) = rs;
                }
            }
            else
            {
                nsAString* ws = *((nsAString**)d);

                if(JSVAL_IS_NULL(s) || (!isDOMString && JSVAL_IS_VOID(s)))
                {
                    ws->Truncate();
                    ws->SetIsVoid(PR_TRUE);
                }
                else
                    ws->Assign(chars, length);
            }
            return JS_TRUE;
        }

        case nsXPTType::T_CHAR_STR:
        {
            char* bytes=nsnull;
            JSString* str;

            if(JSVAL_IS_VOID(s) || JSVAL_IS_NULL(s))
            {
                if(type.IsReference())
                {
                    if(pErr)
                        *pErr = NS_ERROR_XPC_BAD_CONVERT_JS_NULL_REF;
                    return JS_FALSE;
                }
                // else ...
                *((char**)d) = nsnull;
                return JS_TRUE;
            }

            if(!(str = JS_ValueToString(cx, s))||
               !(bytes = JS_GetStringBytes(str)))
            {
                return JS_FALSE;
            }
#ifdef DEBUG
            jschar* chars=nsnull;
            if(nsnull != (chars = JS_GetStringChars(str)))
            {
                PRBool legalRange = PR_TRUE;
                int len = JS_GetStringLength(str);
                jschar* t;
                PRInt32 i=0;
                for(t=chars; (i< len) && legalRange ; i++,t++) {
                  if(ILLEGAL_RANGE(*t))
                      legalRange = PR_FALSE;
                }
                NS_ASSERTION(legalRange,"U+0080/U+0100 - U+FFFF data lost");
            }
#endif // DEBUG
            if(useAllocator)
            {
                int len = (JS_GetStringLength(str) + 1) * sizeof(char);
                if(!(*((void**)d) = nsMemory::Alloc(len)))
                {
                    return JS_FALSE;
                }
                memcpy(*((void**)d), bytes, len);
            }
            else
                *((char**)d) = bytes;

            return JS_TRUE;
        }

        case nsXPTType::T_WCHAR_STR:
        {
            jschar* chars=nsnull;
            JSString* str;

            if(JSVAL_IS_VOID(s) || JSVAL_IS_NULL(s))
            {
                if(type.IsReference())
                {
                    if(pErr)
                        *pErr = NS_ERROR_XPC_BAD_CONVERT_JS_NULL_REF;
                    return JS_FALSE;
                }
                // else ...
                *((jschar**)d) = nsnull;
                return JS_TRUE;
            }

            if(!(str = JS_ValueToString(cx, s))||
               !(chars = JS_GetStringChars(str)))
            {
                return JS_FALSE;
            }
            if(useAllocator)
            {
                int byte_len = (JS_GetStringLength(str)+1)*sizeof(jschar);
                if(!(*((void**)d) = nsMemory::Alloc(byte_len)))
                {
                    // XXX should report error
                    return JS_FALSE;
                }
                memcpy(*((void**)d), chars, byte_len);
            }
            else
                *((jschar**)d) = chars;

            return JS_TRUE;
        }

        case nsXPTType::T_UTF8STRING:            
        {
            jschar* chars;
            PRUint32 length;
            JSString* str;

            if(JSVAL_IS_NULL(s) || JSVAL_IS_VOID(s))
            {
                if(useAllocator) 
                {
                    nsACString *rs = new nsCString();
                    if(!rs) 
                        return JS_FALSE;

                    rs->SetIsVoid(PR_TRUE);
                    *((nsACString**)d) = rs;
                }
                else
                {
                    nsCString* rs = *((nsCString**)d);
                    rs->Truncate();
                    rs->SetIsVoid(PR_TRUE);
                }
                return JS_TRUE;
            }

            // The JS val is neither null nor void...

            if(!(str = JS_ValueToString(cx, s))||
               !(chars = JS_GetStringChars(str)))
            {
                return JS_FALSE;
            }

            length = JS_GetStringLength(str);

            if(useAllocator)
            {                
                const nsACString *rs = new NS_ConvertUCS2toUTF8((const PRUnichar*)chars, length);
                if(!rs)
                    return JS_FALSE;

                *((const nsACString**)d) = rs;
            }
            else
            {
                nsCString* rs = *((nsCString**)d);

                // XXX This code needs to change when Jag lands the new
                // UTF8String implementation.  Adopt() is a method that 
                // shouldn't be used by string consumers.
                rs->Adopt(ToNewUTF8String(nsDependentString((const PRUnichar*)chars, length)));
            }
            return JS_TRUE;
        }

        case nsXPTType::T_CSTRING:
        {
            const char* chars;            
            PRUint32 length;
            JSString* str;

            if(JSVAL_IS_NULL(s) || JSVAL_IS_VOID(s))
            {
                if(useAllocator)
                {
                    nsACString *rs = new nsCString();
                    if(!rs) 
                        return JS_FALSE;

                    rs->SetIsVoid(PR_TRUE);
                    *((nsACString**)d) = rs;
                }
                else
                {
                    nsACString* rs = *((nsACString**)d);
                    rs->Truncate();
                    rs->SetIsVoid(PR_TRUE);
                }
                return JS_TRUE;
            }

            // The JS val is neither null nor void...

            if(!(str = JS_ValueToString(cx, s)) ||
               !(chars = JS_GetStringBytes(str)))
            {
                return JS_FALSE;
            }

            length = JS_GetStringLength(str);

            if(useAllocator)
            {
                const nsACString *rs = new nsCString(chars, length);

                if(!rs)
                    return JS_FALSE;

                *((const nsACString**)d) = rs;
            }
            else
            {
                nsACString* rs = *((nsACString**)d);

                rs->Assign(nsDependentCString(chars, length));
            }
            return JS_TRUE;
        }

        case nsXPTType::T_INTERFACE:
        case nsXPTType::T_INTERFACE_IS:
        {
            JSObject* obj;
            NS_ASSERTION(iid,"can't do interface conversions without iid");

            if(iid->Equals(NS_GET_IID(nsIVariant)))
            {
                XPCVariant* variant = XPCVariant::newVariant(ccx, s);
                if(!variant)
                    return JS_FALSE;
                *((nsISupports**)d) = NS_STATIC_CAST(nsIVariant*, variant);
                return JS_TRUE;
            }
            //else ...

            if(JSVAL_IS_VOID(s) || JSVAL_IS_NULL(s))
            {
                if(type.IsReference())
                {
                    if(pErr)
                        *pErr = NS_ERROR_XPC_BAD_CONVERT_JS_NULL_REF;
                    return JS_FALSE;
                }
                // else ...
                *((nsISupports**)d) = nsnull;
                return JS_TRUE;
            }

            // only wrap JSObjects
            if(!JSVAL_IS_OBJECT(s) || !(obj = JSVAL_TO_OBJECT(s)))
            {
                if(pErr && JSVAL_IS_INT(s) && 0 == JSVAL_TO_INT(s))
                    *pErr = NS_ERROR_XPC_BAD_CONVERT_JS_ZERO_ISNOT_NULL;
                return JS_FALSE;
            }

            return JSObject2NativeInterface(ccx, (void**)d, obj, iid,
                                            nsnull, pErr);
        }
        default:
            NS_ASSERTION(0, "bad type");
            return JS_FALSE;
        }
    }
    return JS_TRUE;
}

/***************************************************************************/
// static
JSBool
XPCConvert::NativeInterface2JSObject(XPCCallContext& ccx,
                                     nsIXPConnectJSObjectHolder** dest,
                                     nsISupports* src,
                                     const nsID* iid,
                                     JSObject* scope, nsresult* pErr)
{
    NS_ASSERTION(dest, "bad param");
    NS_ASSERTION(iid, "bad param");
    NS_ASSERTION(scope, "bad param");

    *dest = nsnull;
    if(!src)
        return JS_TRUE;
    if(pErr)
        *pErr = NS_ERROR_XPC_BAD_CONVERT_NATIVE;

// #define this if we want to 'double wrap' of JSObjects.
// This is for the case where we have a JSObject wrapped for native use
// which needs to be converted to a JSObject. Originally, we were unwrapping
// and just exposing the underlying JSObject. This causes anomolies when
// JSComponents are accessed from other JS code - they don't act like
// other xpconnect wrapped components. Eventually we want to build a new
// kind of wrapper especially for JS <-> JS. For now we are building a wrapper
// around a wrapper. This is not optimal, but good enough for now.
#define XPC_DO_DOUBLE_WRAP 1

#ifndef XPC_DO_DOUBLE_WRAP
    // is this a wrapped JS object?
    if(nsXPCWrappedJSClass::IsWrappedJS(src))
    {
        // verify that this wrapper is for the right interface
        nsCOMPtr<nsISupports> wrapper;
        if(NS_FAILED(src->QueryInterface(*iid,(void**)getter_AddRefs(wrapper))))
            return JS_FALSE;
        return NS_SUCCEEDED(wrapper->QueryInterface(
                                        NS_GET_IID(nsIXPConnectJSObjectHolder),
                                        (void**) dest));
    }
    else
#endif /* XPC_DO_DOUBLE_WRAP */
    {
        XPCWrappedNativeScope* xpcscope =
            XPCWrappedNativeScope::FindInJSObjectScope(ccx, scope);
        if(!xpcscope)
            return JS_FALSE;

        AutoMarkingNativeInterfacePtr iface(ccx);
        iface = XPCNativeInterface::GetNewOrUsed(ccx, iid);
        if(!iface)
            return JS_FALSE;

        XPCWrappedNative* wrapper;
        nsresult rv = XPCWrappedNative::GetNewOrUsed(ccx, src, xpcscope,
                                                     iface, &wrapper);
        if(pErr)
            *pErr = rv;
        if(NS_SUCCEEDED(rv) && wrapper)
        {
            *dest = NS_STATIC_CAST(nsIXPConnectJSObjectHolder*, wrapper);
            return JS_TRUE;
        }
        
    }
    return JS_FALSE;
}

/***************************************************************************/

// static
JSBool
XPCConvert::JSObject2NativeInterface(XPCCallContext& ccx,
                                     void** dest, JSObject* src,
                                     const nsID* iid,
                                     nsISupports* aOuter,
                                     nsresult* pErr)
{
    NS_ASSERTION(dest, "bad param");
    NS_ASSERTION(src, "bad param");
    NS_ASSERTION(iid, "bad param");

    JSContext* cx = ccx.GetJSContext();

    *dest = nsnull;
     if(pErr)
        *pErr = NS_ERROR_XPC_BAD_CONVERT_JS;

    nsISupports* iface;

    if(!aOuter)
    {
        // Note that if we have a non-null aOuter then it means that we are
        // forcing the creation of a wrapper even if the object *is* a 
        // wrappedNative or other wise has 'nsISupportness'. 
        // This allows wrapJSAggregatedToNative to work.

        // Is this really a native xpcom object with a wrapper?
        XPCWrappedNative* wrappedNative =
                    XPCWrappedNative::GetWrappedNativeOfJSObject(cx, src);
        if(wrappedNative)
        {
            iface = wrappedNative->GetIdentityObject();
            // is the underlying object the right interface?
            if(wrappedNative->GetIID().Equals(*iid))
            {
                NS_ADDREF(iface);
                *dest = iface;
                return JS_TRUE;
            }
            else
                return NS_SUCCEEDED(iface->QueryInterface(*iid, dest));
        }
        // else...
        
        // Does the JSObject have 'nsISupportness'?
        // XXX hmm, I wonder if this matters anymore with no 
        // oldstyle DOM objects around.
        if(GetISupportsFromJSObject(cx, src, &iface))
        {
            if(iface)
                return NS_SUCCEEDED(iface->QueryInterface(*iid, dest));
            return JS_FALSE;
        
        }
    }

    // else...

    nsXPCWrappedJS* wrapper;
    nsresult rv = nsXPCWrappedJS::GetNewOrUsed(ccx, src, *iid, aOuter, &wrapper);
    if(pErr)
        *pErr = rv;
    if(NS_SUCCEEDED(rv) && wrapper)
    {
        // We need to go through the QueryInterface logic to make this return
        // the right thing for the various 'special' interfaces; e.g. 
        // nsIPropertyBag. We must use AggregatedQueryInterface in cases where 
        // there is an outer to avoid nasty recursion.
        rv = aOuter ? wrapper->AggregatedQueryInterface(*iid, dest) :
                      wrapper->QueryInterface(*iid, dest);
        if(pErr)
            *pErr = rv;
        NS_RELEASE(wrapper);
        return NS_SUCCEEDED(rv);        
    }

    // else...
    return JS_FALSE;
}

/***************************************************************************/
/***************************************************************************/

// static
nsresult
XPCConvert::ConstructException(nsresult rv, const char* message,
                               const char* ifaceName, const char* methodName,
                               nsISupports* data,
                               nsIException** exceptn)
{
    static const char format[] = "\'%s\' when calling method: [%s::%s]";
    const char * msg = message;
    char* sz = nsnull;

    if(!msg)
        if(!nsXPCException::NameAndFormatForNSResult(rv, nsnull, &msg) || ! msg)
            msg = "<error>";

    if(ifaceName && methodName)
        sz = JS_smprintf(format, msg, ifaceName, methodName);
    else
        sz = (char*) msg; // I promise to play nice after casting away const

    nsresult res = nsXPCException::NewException(sz, rv, nsnull, data, exceptn);

    if(sz && sz != msg)
        JS_smprintf_free(sz);
    return res;
}

/********************************/

// static
nsresult
XPCConvert::JSValToXPCException(XPCCallContext& ccx,
                                jsval s,
                                const char* ifaceName,
                                const char* methodName,
                                nsIException** exceptn)
{
    JSContext* cx = ccx.GetJSContext();

    if(!JSVAL_IS_PRIMITIVE(s))
    {
        // we have a JSObject
        JSObject* obj = JSVAL_TO_OBJECT(s);

        if(!obj)
        {
            NS_ASSERTION(0, "when is an object not an object?");
            return NS_ERROR_FAILURE;
        }

        // is this really a native xpcom object with a wrapper?
        XPCWrappedNative* wrapper;
        if(nsnull != (wrapper =
           XPCWrappedNative::GetWrappedNativeOfJSObject(cx,obj)))
        {
            nsISupports* supports = wrapper->GetIdentityObject();
            nsCOMPtr<nsIException> iface = do_QueryInterface(supports);
            if(iface)
            {
                // just pass through the exception (with extra ref and all)
                nsIException* temp = iface;
                NS_ADDREF(temp);
                *exceptn = temp;
                return NS_OK;
            }
            else
            {
                // it is a wrapped native, but not an exception!
                return ConstructException(NS_ERROR_XPC_JS_THREW_NATIVE_OBJECT,
                                          nsnull, ifaceName, methodName, supports,
                                          exceptn);
            }
        }
        else
        {
            // It is a JSObject, but not a wrapped native...

            // If it is an engine Error with an error report then let's
            // extract the report and build an xpcexception from that
            const JSErrorReport* report;
            if(nsnull != (report = JS_ErrorFromException(cx, s)))
            {
                const char* message = nsnull;
                JSString* str;
                if(nsnull != (str = JS_ValueToString(cx, s)))
                    message = JS_GetStringBytes(str);
                return JSErrorToXPCException(ccx, message, ifaceName,
                                             methodName, report, exceptn);
            }


            uintN ignored;
            JSBool found;

            // heuristic to see if it might be usable as an xpcexception
            if(JS_GetPropertyAttributes(cx, obj, "message", &ignored, &found) &&
               found &&
               JS_GetPropertyAttributes(cx, obj, "result", &ignored, &found) &&
               found)
            {
                // lets try to build a wrapper around the JSObject
                XPCContext* xpcc;
                if(nsnull != (xpcc = nsXPConnect::GetContext(cx)))
                {
                    nsXPCWrappedJS* jswrapper;
                    nsresult rv =
                        nsXPCWrappedJS::GetNewOrUsed(ccx, obj,
                                                NS_GET_IID(nsIException),
                                                nsnull, &jswrapper);
                    if(NS_FAILED(rv))
                        return rv;
                    *exceptn = NS_REINTERPRET_CAST(nsIException*,
                                                   jswrapper);
                    return NS_OK;
                }
            }


            // XXX we should do a check against 'js_ErrorClass' here and
            // do the right thing - even though it has no JSErrorReport,
            // The fact that it is a JSError exceptions means we can extract
            // particular info and our 'result' should reflect that.

            // otherwise we'll just try to convert it to a string

            JSString* str = JS_ValueToString(cx, s);
            if(!str)
                return NS_ERROR_FAILURE;

            return ConstructException(NS_ERROR_XPC_JS_THREW_JS_OBJECT,
                                      JS_GetStringBytes(str),
                                      ifaceName, methodName, nsnull,
                                      exceptn);
        }
    }

    if(JSVAL_IS_VOID(s) || JSVAL_IS_NULL(s))
    {
        return ConstructException(NS_ERROR_XPC_JS_THREW_NULL,
                                  nsnull, ifaceName, methodName, nsnull,
                                  exceptn);
    }

    if(JSVAL_IS_NUMBER(s))
    {
        // lets see if it looks like an nsresult
        nsresult rv;
        double number;
        JSBool isResult = JS_FALSE;

        if(JSVAL_IS_INT(s))
        {
            rv = (nsresult) JSVAL_TO_INT(s);
            if(NS_FAILED(rv))
                isResult = JS_TRUE;
            else
                number = (double) JSVAL_TO_INT(s);
        }
        else
        {
            if(JSVAL_TO_DOUBLE(s))
            {
                number = *(JSVAL_TO_DOUBLE(s));
                if(number > 0.0 &&
                   number < (double)0xffffffff &&
                   0.0 == fmod(number,1))
                {
                    rv = (nsresult) number;
                    if(NS_FAILED(rv))
                        isResult = JS_TRUE;
                }
            }
        }

        if(isResult)
            return ConstructException(rv, nsnull, ifaceName, methodName,
                                      nsnull, exceptn);
        else
        {
            nsISupportsDouble* data;
            nsCOMPtr<nsIComponentManager> cm;
            if(NS_FAILED(NS_GetComponentManager(getter_AddRefs(cm))) || !cm ||
               NS_FAILED(cm->CreateInstanceByContractID(
                                NS_SUPPORTS_DOUBLE_CONTRACTID,
                                nsnull,
                                NS_GET_IID(nsISupportsDouble),
                                (void**)&data)))
                return NS_ERROR_FAILURE;
            data->SetData(number);
            rv = ConstructException(NS_ERROR_XPC_JS_THREW_NUMBER, nsnull,
                                    ifaceName, methodName, data, exceptn);
            NS_RELEASE(data);
            return rv;
        }
    }

    // otherwise we'll just try to convert it to a string

    JSString* str = JS_ValueToString(cx, s);
    if(str)
        return ConstructException(NS_ERROR_XPC_JS_THREW_STRING,
                                  JS_GetStringBytes(str),
                                  ifaceName, methodName, nsnull,
                                  exceptn);
    return NS_ERROR_FAILURE;
}

/********************************/

// static
nsresult
XPCConvert::JSErrorToXPCException(XPCCallContext& ccx,
                                  const char* message,
                                  const char* ifaceName,
                                  const char* methodName,
                                  const JSErrorReport* report,
                                  nsIException** exceptn)
{
    nsresult rv = NS_ERROR_FAILURE;
    nsScriptError* data;
    if(report)
    {
        nsAutoString bestMessage;
        if(report && report->ucmessage)
        {
            bestMessage = (const PRUnichar *)report->ucmessage;
        }
        else if(message)
        {
            bestMessage.AssignWithConversion(message);
        }
        else
        {
            bestMessage = NS_LITERAL_STRING("JavaScript Error");
        }

        data = new nsScriptError();
        NS_ADDREF(data);
        data->Init(bestMessage.get(),
                   NS_ConvertASCIItoUCS2(report->filename).get(),
                   (const PRUnichar *)report->uclinebuf, report->lineno,
                   report->uctokenptr - report->uclinebuf, report->flags,
                   "XPConnect JavaScript");
    }
    else
        data = nsnull;

    if(data)
    {
        char* formattedMsg;
        if(NS_FAILED(data->ToString(&formattedMsg)))
            formattedMsg = nsnull;

        rv = ConstructException(NS_ERROR_XPC_JAVASCRIPT_ERROR_WITH_DETAILS,
                                formattedMsg, ifaceName, methodName, data,
                                exceptn);

        if(formattedMsg)
            nsMemory::Free(formattedMsg);
        NS_RELEASE(data);
    }
    else
    {
        rv = ConstructException(NS_ERROR_XPC_JAVASCRIPT_ERROR,
                                nsnull, ifaceName, methodName, nsnull,
                                exceptn);
    }
    return rv;
}


/***************************************************************************/

/*
** Note: on some platforms va_list is defined as an array,
** and requires array notation.
*/
#ifdef HAVE_VA_LIST_AS_ARRAY
#define VARARGS_ASSIGN(foo, bar)	foo[0] = bar[0]
#else
#define VARARGS_ASSIGN(foo, bar)	(foo) = (bar)
#endif

// We assert below that these formats all begin with "%i".
const char* XPC_ARG_FORMATTER_FORMAT_STRINGS[] = {"%ip", "%iv", "%is", nsnull};

JSBool JS_DLL_CALLBACK
XPC_JSArgumentFormatter(JSContext *cx, const char *format,
                        JSBool fromJS, jsval **vpp, va_list *app)
{
    XPCCallContext ccx(NATIVE_CALLER, cx);
    if(!ccx.IsValid())
        return JS_FALSE;

    jsval *vp;
    va_list ap;

    vp = *vpp;
    VARARGS_ASSIGN(ap, *app);

    nsXPTType type;
    const nsIID* iid;
    void* p;

    NS_ASSERTION(format[0] == '%' && format[1] == 'i', "bad format!");
    char which = format[2];

    if(fromJS)
    {
        switch(which)
        {
            case 'p':
                type = nsXPTType((uint8)(TD_INTERFACE_TYPE | XPT_TDP_POINTER));                
                iid = &NS_GET_IID(nsISupports);
                break;
            case 'v':
                type = nsXPTType((uint8)(TD_INTERFACE_TYPE | XPT_TDP_POINTER));                
                iid = &NS_GET_IID(nsIVariant);
                break;
            case 's':
                type = nsXPTType((uint8)(TD_DOMSTRING | XPT_TDP_POINTER));                
                iid = nsnull;
                p = va_arg(ap, void *);
                break;
            default:
                NS_ERROR("bad format!");
                return JS_FALSE;
        }

        if(!XPCConvert::JSData2Native(ccx, &p, vp[0], type, JS_FALSE,
                                      iid, nsnull))
            return JS_FALSE;
        
        if(which != 's')
            *va_arg(ap, void **) = p;
    }
    else
    {
        switch(which)
        {
            case 'p':
                type = nsXPTType((uint8)(TD_INTERFACE_TYPE | XPT_TDP_POINTER));                
                iid  = va_arg(ap, const nsIID*);
                break;
            case 'v':
                type = nsXPTType((uint8)(TD_INTERFACE_TYPE | XPT_TDP_POINTER));                
                iid = &NS_GET_IID(nsIVariant);
                break;
            case 's':
                type = nsXPTType((uint8)(TD_DOMSTRING | XPT_TDP_POINTER));                
                iid = nsnull;
                break;
            default:
                NS_ERROR("bad format!");
                return JS_FALSE;
        }

        // NOTE: MUST be retrieved *after* the iid in the 'p' case above.
        p = va_arg(ap, void *);

        if(!XPCConvert::NativeData2JS(ccx, &vp[0], &p, type, iid,
                                      JS_GetGlobalObject(cx), nsnull))
            return JS_FALSE;
    }
    *vpp = vp + 1;
    VARARGS_ASSIGN(*app, ap);
    return JS_TRUE;
}

/***************************************************************************/

// array fun...

#ifdef POPULATE
#undef POPULATE
#endif

// static
JSBool
XPCConvert::NativeArray2JS(XPCCallContext& ccx,
                           jsval* d, const void** s,
                           const nsXPTType& type, const nsID* iid,
                           JSUint32 count, JSObject* scope,
                           nsresult* pErr)
{
    NS_PRECONDITION(s, "bad param");
    NS_PRECONDITION(d, "bad param");

    JSContext* cx = ccx.GetJSContext();

    // XXX add support for putting chars in a string rather than an array

    // XXX add support to indicate *which* array element was not convertable

    JSObject *array = JS_NewArrayObject(cx, count, nsnull);

    if(!array)
        return JS_FALSE;

    // root this early
    *d = OBJECT_TO_JSVAL(array);

    if(pErr)
        *pErr = NS_ERROR_XPC_BAD_CONVERT_NATIVE;

    JSUint32 i;
    jsval current;

#define POPULATE(_t)                                                         \
    PR_BEGIN_MACRO                                                           \
        for(i = 0; i < count; i++)                                           \
        {                                                                    \
            if(!NativeData2JS(ccx, &current, ((_t*)*s)+i, type, iid,         \
                              scope, pErr) ||                                \
               !JS_SetElement(cx, array, i, &current))                       \
                goto failure;                                                \
        }                                                                    \
    PR_END_MACRO

    // XXX check IsPtr - esp. to handle array of nsID (as opposed to nsID*)

    switch(type.TagPart())
    {
    case nsXPTType::T_I8            : POPULATE(int8);           break;
    case nsXPTType::T_I16           : POPULATE(int16);          break;
    case nsXPTType::T_I32           : POPULATE(int32);          break;
    case nsXPTType::T_I64           : POPULATE(int64);          break;
    case nsXPTType::T_U8            : POPULATE(uint8);          break;
    case nsXPTType::T_U16           : POPULATE(uint16);         break;
    case nsXPTType::T_U32           : POPULATE(uint32);         break;
    case nsXPTType::T_U64           : POPULATE(uint64);         break;
    case nsXPTType::T_FLOAT         : POPULATE(float);          break;
    case nsXPTType::T_DOUBLE        : POPULATE(double);         break;
    case nsXPTType::T_BOOL          : POPULATE(PRBool);         break;
    case nsXPTType::T_CHAR          : POPULATE(char);           break;
    case nsXPTType::T_WCHAR         : POPULATE(jschar);         break;
    case nsXPTType::T_VOID          : NS_ASSERTION(0,"bad type"); goto failure;
    case nsXPTType::T_IID           : POPULATE(nsID*);          break;
    case nsXPTType::T_DOMSTRING     : NS_ASSERTION(0,"bad type"); goto failure;
    case nsXPTType::T_CHAR_STR      : POPULATE(char*);          break;
    case nsXPTType::T_WCHAR_STR     : POPULATE(jschar*);        break;
    case nsXPTType::T_INTERFACE     : POPULATE(nsISupports*);   break;
    case nsXPTType::T_INTERFACE_IS  : POPULATE(nsISupports*);   break;
    case nsXPTType::T_UTF8STRING    : NS_ASSERTION(0,"bad type"); goto failure;
    case nsXPTType::T_CSTRING       : NS_ASSERTION(0,"bad type"); goto failure;
    case nsXPTType::T_ASTRING       : NS_ASSERTION(0,"bad type"); goto failure;
    default                         : NS_ASSERTION(0,"bad type"); goto failure;
    }

    if(pErr)
        *pErr = NS_OK;
    return JS_TRUE;

failure:
    return JS_FALSE;

#undef POPULATE
}

// static
JSBool
XPCConvert::JSArray2Native(XPCCallContext& ccx, void** d, jsval s,
                           JSUint32 count, JSUint32 capacity,
                           const nsXPTType& type,
                           JSBool useAllocator, const nsID* iid,
                           uintN* pErr)
{
    NS_PRECONDITION(d, "bad param");

    JSContext* cx = ccx.GetJSContext();

    // No Action, FRee memory, RElease object
    enum CleanupMode {na, fr, re};

    CleanupMode cleanupMode;

    JSObject* jsarray = nsnull;
    void* array = nsnull;
    JSUint32 initedCount;
    jsval current;

    // XXX add support for getting chars from strings

    // XXX add support to indicate *which* array element was not convertable

    if(JSVAL_IS_VOID(s) || JSVAL_IS_NULL(s))
    {
        if(0 != count)
        {
            if(pErr)
                *pErr = NS_ERROR_XPC_NOT_ENOUGH_ELEMENTS_IN_ARRAY;
            return JS_FALSE;
        }

        // If a non-zero capacity was indicated then we build an
        // empty array rather than return nsnull.
        if(0 != capacity)
            goto fill_array;

        *d = nsnull;
        return JS_TRUE;
    }

    if(!JSVAL_IS_OBJECT(s))
    {
        if(pErr)
            *pErr = NS_ERROR_XPC_CANT_CONVERT_PRIMITIVE_TO_ARRAY;
        return JS_FALSE;
    }

    jsarray = JSVAL_TO_OBJECT(s);
    if(!JS_IsArrayObject(cx, jsarray))
    {
        if(pErr)
            *pErr = NS_ERROR_XPC_CANT_CONVERT_OBJECT_TO_ARRAY;
        return JS_FALSE;
    }

    jsuint len;
    if(!JS_GetArrayLength(cx, jsarray, &len) || len < count || capacity < count)
    {
        if(pErr)
            *pErr = NS_ERROR_XPC_NOT_ENOUGH_ELEMENTS_IN_ARRAY;
        return JS_FALSE;
    }

    if(pErr)
        *pErr = NS_ERROR_XPC_BAD_CONVERT_JS;

#define POPULATE(_mode, _t)                                                  \
    PR_BEGIN_MACRO                                                           \
        cleanupMode = _mode;                                                 \
        if(nsnull == (array = nsMemory::Alloc(capacity * sizeof(_t))))       \
        {                                                                    \
            if(pErr)                                                         \
                *pErr = NS_ERROR_OUT_OF_MEMORY;                              \
            goto failure;                                                    \
        }                                                                    \
        for(initedCount = 0; initedCount < count; initedCount++)             \
        {                                                                    \
            if(!JS_GetElement(cx, jsarray, initedCount, &current) ||         \
               !JSData2Native(ccx, ((_t*)array)+initedCount, current, type,  \
                              useAllocator, iid, pErr))                      \
                goto failure;                                                \
        }                                                                    \
    PR_END_MACRO


    // XXX check IsPtr - esp. to handle array of nsID (as opposed to nsID*)

    // XXX make extra space at end of char* and wchar* and null termintate

fill_array:
    switch(type.TagPart())
    {
    case nsXPTType::T_I8            : POPULATE(na, int8);           break;
    case nsXPTType::T_I16           : POPULATE(na, int16);          break;
    case nsXPTType::T_I32           : POPULATE(na, int32);          break;
    case nsXPTType::T_I64           : POPULATE(na, int64);          break;
    case nsXPTType::T_U8            : POPULATE(na, uint8);          break;
    case nsXPTType::T_U16           : POPULATE(na, uint16);         break;
    case nsXPTType::T_U32           : POPULATE(na, uint32);         break;
    case nsXPTType::T_U64           : POPULATE(na, uint64);         break;
    case nsXPTType::T_FLOAT         : POPULATE(na, float);          break;
    case nsXPTType::T_DOUBLE        : POPULATE(na, double);         break;
    case nsXPTType::T_BOOL          : POPULATE(na, PRBool);         break;
    case nsXPTType::T_CHAR          : POPULATE(na, char);           break;
    case nsXPTType::T_WCHAR         : POPULATE(na, jschar);         break;
    case nsXPTType::T_VOID          : NS_ASSERTION(0,"bad type"); goto failure;
    case nsXPTType::T_IID           : POPULATE(fr, nsID*);          break;
    case nsXPTType::T_DOMSTRING     : NS_ASSERTION(0,"bad type"); goto failure;
    case nsXPTType::T_CHAR_STR      : POPULATE(fr, char*);          break;
    case nsXPTType::T_WCHAR_STR     : POPULATE(fr, jschar*);        break;
    case nsXPTType::T_INTERFACE     : POPULATE(re, nsISupports*);   break;
    case nsXPTType::T_INTERFACE_IS  : POPULATE(re, nsISupports*);   break;
    case nsXPTType::T_UTF8STRING    : NS_ASSERTION(0,"bad type"); goto failure;
    case nsXPTType::T_CSTRING       : NS_ASSERTION(0,"bad type"); goto failure;
    case nsXPTType::T_ASTRING       : NS_ASSERTION(0,"bad type"); goto failure;
    default                         : NS_ASSERTION(0,"bad type"); goto failure;
    }

    *d = array;
    if(pErr)
        *pErr = NS_OK;
    return JS_TRUE;

failure:
    // we may need to cleanup the partially filled array of converted stuff
    if(array)
    {
        if(cleanupMode == re)
        {
            nsISupports** a = (nsISupports**) array;
            for(PRUint32 i = 0; i < initedCount; i++)
            {
                nsISupports* p = a[i];
                NS_IF_RELEASE(p);
            }
        }
        else if(cleanupMode == fr && useAllocator)
        {
            void** a = (void**) array;
            for(PRUint32 i = 0; i < initedCount; i++)
            {
                void* p = a[i];
                if(p) nsMemory::Free(p);
            }
        }
        nsMemory::Free(array);
    }

    return JS_FALSE;

#undef POPULATE
}

// static
JSBool
XPCConvert::NativeStringWithSize2JS(XPCCallContext& ccx,
                                    jsval* d, const void* s,
                                    const nsXPTType& type,
                                    JSUint32 count,
                                    nsresult* pErr)
{
    NS_PRECONDITION(s, "bad param");
    NS_PRECONDITION(d, "bad param");

    JSContext* cx = ccx.GetJSContext();

    if(pErr)
        *pErr = NS_ERROR_XPC_BAD_CONVERT_NATIVE;

    if(!type.IsPointer())
    {
        XPC_LOG_ERROR(("XPCConvert::NativeStringWithSize2JS : unsupported type"));
        return JS_FALSE;
    }
    switch(type.TagPart())
    {
        case nsXPTType::T_PSTRING_SIZE_IS:
        {
            char* p = *((char**)s);
            if(!p)
                break;
            JSString* str;
            if(!(str = JS_NewStringCopyN(cx, p, count)))
                return JS_FALSE;
            *d = STRING_TO_JSVAL(str);
            break;
        }
        case nsXPTType::T_PWSTRING_SIZE_IS:
        {
            jschar* p = *((jschar**)s);
            if(!p)
                break;
            JSString* str;
            if(!(str = JS_NewUCStringCopyN(cx, p, count)))
                return JS_FALSE;
            *d = STRING_TO_JSVAL(str);
            break;
        }
        default:
            XPC_LOG_ERROR(("XPCConvert::NativeStringWithSize2JS : unsupported type"));
            return JS_FALSE;
    }
    return JS_TRUE;
}

// static
JSBool
XPCConvert::JSStringWithSize2Native(XPCCallContext& ccx, void* d, jsval s,
                                    JSUint32 count, JSUint32 capacity,
                                    const nsXPTType& type,
                                    JSBool useAllocator,
                                    uintN* pErr)
{
    NS_PRECONDITION(s, "bad param");
    NS_PRECONDITION(d, "bad param");

    JSContext* cx = ccx.GetJSContext();

    JSUint32 len;

    if(pErr)
        *pErr = NS_ERROR_XPC_BAD_CONVERT_NATIVE;

    if(capacity < count)
    {
        if(pErr)
            *pErr = NS_ERROR_XPC_NOT_ENOUGH_CHARS_IN_STRING;
        return JS_FALSE;
    }

    if(!type.IsPointer())
    {
        XPC_LOG_ERROR(("XPCConvert::JSStringWithSize2Native : unsupported type"));
        return JS_FALSE;
    }
    switch(type.TagPart())
    {
        case nsXPTType::T_PSTRING_SIZE_IS:
        {
            char* bytes=nsnull;
            JSString* str;

            if(JSVAL_IS_VOID(s) || JSVAL_IS_NULL(s))
            {
                if(0 != count)
                {
                    if(pErr)
                        *pErr = NS_ERROR_XPC_NOT_ENOUGH_CHARS_IN_STRING;
                    return JS_FALSE;
                }
                if(type.IsReference())
                {
                    if(pErr)
                        *pErr = NS_ERROR_XPC_BAD_CONVERT_JS_NULL_REF;
                    return JS_FALSE;
                }

                if(useAllocator && 0 != capacity)
                {
                    len = (capacity + 1) * sizeof(char);
                    if(!(*((void**)d) = nsMemory::Alloc(len)))
                        return JS_FALSE;
                    return JS_TRUE;
                }
                // else ...

                *((char**)d) = nsnull;
                return JS_TRUE;
            }

            if(!(str = JS_ValueToString(cx, s))||
               !(bytes = JS_GetStringBytes(str)))
            {
                return JS_FALSE;
            }

            len = JS_GetStringLength(str);
            if(len > count)
            {
                if(pErr)
                    *pErr = NS_ERROR_XPC_NOT_ENOUGH_CHARS_IN_STRING;
                return JS_FALSE;
            }

            if(len < capacity)
                len = capacity;

            if(useAllocator)
            {
                JSUint32 alloc_len = (len + 1) * sizeof(char);
                if(!(*((void**)d) = nsMemory::Alloc(alloc_len)))
                {
                    return JS_FALSE;
                }
                memcpy(*((char**)d), bytes, count);
                (*((char**)d))[count] = 0;
            }
            else
                *((char**)d) = bytes;

            return JS_TRUE;
        }

        case nsXPTType::T_PWSTRING_SIZE_IS:
        {
            jschar* chars=nsnull;
            JSString* str;

            if(JSVAL_IS_VOID(s) || JSVAL_IS_NULL(s))
            {
                if(0 != count)
                {
                    if(pErr)
                        *pErr = NS_ERROR_XPC_NOT_ENOUGH_CHARS_IN_STRING;
                    return JS_FALSE;
                }
                if(type.IsReference())
                {
                    if(pErr)
                        *pErr = NS_ERROR_XPC_BAD_CONVERT_JS_NULL_REF;
                    return JS_FALSE;
                }

                if(useAllocator && 0 != capacity)
                {
                    len = (capacity + 1) * sizeof(jschar);
                    if(!(*((void**)d) = nsMemory::Alloc(len)))
                        return JS_FALSE;
                    return JS_TRUE;
                }

                // else ...
                *((jschar**)d) = nsnull;
                return JS_TRUE;
            }

            if(!(str = JS_ValueToString(cx, s))||
               !(chars = JS_GetStringChars(str)))
            {
                return JS_FALSE;
            }

            len = JS_GetStringLength(str);
            if(len > count)
            {
                if(pErr)
                    *pErr = NS_ERROR_XPC_NOT_ENOUGH_CHARS_IN_STRING;
                return JS_FALSE;
            }
            if(len < capacity)
                len = capacity;

            if(useAllocator)
            {
                JSUint32 alloc_len = (len + 1) * sizeof(jschar);
                if(!(*((void**)d) = nsMemory::Alloc(alloc_len)))
                {
                    // XXX should report error
                    return JS_FALSE;
                }
                memcpy(*((jschar**)d), chars, alloc_len);
                (*((jschar**)d))[count] = 0;
            }
            else
                *((jschar**)d) = chars;

            return JS_TRUE;
        }
        default:
            XPC_LOG_ERROR(("XPCConvert::JSStringWithSize2Native : unsupported type"));
            return JS_FALSE;
    }
}

