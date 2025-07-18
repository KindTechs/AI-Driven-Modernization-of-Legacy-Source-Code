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
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 *
 *   Adam Lock <adamlock@netscape.com> 
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */
 */
#include "stdafx.h"

#include "npn.h"
#include "Pluginhostctrl.h"

#include "nsPluginHostCtrl.h"

static NPError
_OpenURL(NPP npp, const char *szURL, const char *szTarget, void *pNotifyData, const char *pData, uint32 len, NPBool isFile)
{
    if (!npp)
    {
        return NPERR_INVALID_INSTANCE_ERROR;
    }

    void *postData = NULL;
    uint32 postDataLen = 0;
    if (pData)
    {
        if (!isFile)
        {
            postData = (void *) pData;
            postDataLen = len;
        }
        else
        {
            // TODO read the file specified in the postdata param into memory
        }
    }

    nsPluginHostCtrl *pCtrl = (nsPluginHostCtrl *) npp->ndata;
    ATLASSERT(pCtrl);

    // Other window targets
    if (szTarget)
    {
        CComPtr<IWebBrowserApp> cpBrowser;
        pCtrl->GetWebBrowserApp(&cpBrowser);
        if (!cpBrowser)
        {
            return NPERR_GENERIC_ERROR;
        }

        CComBSTR url(szURL);
        
        HRESULT hr;

        // Test if the input URL has a schema which means it's relative,
        // otherwise consider it to be relative to the current URL

        WCHAR szSchema[10];
        const DWORD cbSchema = sizeof(szSchema) / sizeof(szSchema[0]);
        DWORD cbSchemaUsed = 0;

        memset(szSchema, 0, cbSchema);
        hr = CoInternetParseUrl(url.m_str, PARSE_SCHEMA, 0,
            szSchema, cbSchema, &cbSchemaUsed, 0);

        if (hr != S_OK || cbSchemaUsed == 0)
        {
            // Convert relative URLs to absolute, so that they can be loaded
            // by the Browser

            CComBSTR bstrCurrentURL;
            cpBrowser->get_LocationURL(&bstrCurrentURL);
        
            if (bstrCurrentURL.Length())
            {
                USES_CONVERSION;
                DWORD cbNewURL = (url.Length() + bstrCurrentURL.Length() + 1) * sizeof(WCHAR);
                DWORD cbNewURLUsed = 0;
                WCHAR *pszNewURL = (WCHAR *) malloc(cbNewURL);
                memset(pszNewURL, 0, cbNewURL);
                ATLASSERT(pszNewURL);

                CoInternetCombineUrl(
                    bstrCurrentURL.m_str,
                    url.m_str,
                    0,
                    pszNewURL,
                    cbNewURL,
                    &cbNewURLUsed,
                    0);

                ATLASSERT(cbNewURLUsed < cbNewURL);

                url = pszNewURL;
                free(pszNewURL);
            }
        }

        CComVariant vFlags;
        CComVariant vTarget(szTarget);
        CComVariant vPostData;
        CComVariant vHeaders;

        // Initialise postdata
        if (postData)
        {
            // According to the documentation.
            // The post data specified by PostData is passed as a SAFEARRAY
            // structure. The variant should be of type VT_ARRAY and point to
            // a SAFEARRAY. The SAFEARRAY should be of element type VT_UI1,
            // dimension one, and have an element count equal to the number of
            // bytes of post data.

            SAFEARRAYBOUND saBound[1];
            saBound[0].lLbound = 0;
            saBound[0].cElements = len;
            vPostData.vt = VT_ARRAY | VT_UI1;
            vPostData.parray = SafeArrayCreate(VT_UI1, 1, saBound);
            SafeArrayLock(vPostData.parray);
            memcpy(vPostData.parray->pvData, postData, postDataLen);
            SafeArrayUnlock(vPostData.parray);
        }

        cpBrowser->Navigate(url, &vFlags, &vTarget, &vPostData, &vHeaders);
        // TODO listen to navigation & send a URL notify to plugin when completed
        return NPERR_NO_ERROR;
    }

    USES_CONVERSION;
    HRESULT hr = pCtrl->OpenURLStream(A2CT(szURL), pNotifyData, postData, postDataLen);
    return SUCCEEDED(hr) ? NPERR_NO_ERROR : NPERR_GENERIC_ERROR;
}


NPError NP_EXPORT
NPN_GetURL(NPP npp, const char* relativeURL, const char* target)
{
    return _OpenURL(npp, relativeURL, target, NULL, NULL, 0, FALSE);
}

////////////////////////////////////////////////////////////////////////
NPError NP_EXPORT
NPN_GetURLNotify(NPP         npp, 
              const char* relativeURL, 
              const char* target, 
              void*       notifyData)
{
    return _OpenURL(npp, relativeURL, target, notifyData, NULL, 0, FALSE);
}


////////////////////////////////////////////////////////////////////////
NPError NP_EXPORT
NPN_PostURLNotify(NPP         npp,
               const char *relativeURL,
               const char *target,
               uint32     len,
               const char *buf,
               NPBool     file,
               void       *notifyData)
{
    return _OpenURL(npp, relativeURL, target, notifyData, buf, len, file);
}


////////////////////////////////////////////////////////////////////////
NPError NP_EXPORT
NPN_PostURL(NPP npp,
         const char *relativeURL,
         const char *target,
         uint32     len,
         const char *buf,
         NPBool     file)
{
    return _OpenURL(npp, relativeURL, target, NULL, buf, len, file);
}


////////////////////////////////////////////////////////////////////////
NPError NP_EXPORT
NPN_NewStream(NPP npp, NPMIMEType type, const char* window, NPStream* *result)
{
    if (!npp)
    {
        return NPERR_INVALID_INSTANCE_ERROR;
    }

    return NPERR_GENERIC_ERROR;
}


////////////////////////////////////////////////////////////////////////
int32 NP_EXPORT
NPN_Write(NPP npp, NPStream *pstream, int32 len, void *buffer)
{
    if (!npp)
    {
        return NPERR_INVALID_INSTANCE_ERROR;
    }

    return NPERR_GENERIC_ERROR;
}


////////////////////////////////////////////////////////////////////////
NPError NP_EXPORT
NPN_DestroyStream(NPP npp, NPStream *pstream, NPError reason)
{
    if (!npp)
    {
        return NPERR_INVALID_INSTANCE_ERROR;
    }

    return NPERR_GENERIC_ERROR;
}


////////////////////////////////////////////////////////////////////////
void NP_EXPORT
NPN_Status(NPP npp, const char *message)
{
    if (!npp)
    {
        return;
    }

    nsPluginHostCtrl *pCtrl = (nsPluginHostCtrl *) npp->ndata;
    ATLASSERT(pCtrl);

    // Update the status bar in the browser
    CComPtr<IWebBrowserApp> cpBrowser;
    pCtrl->GetWebBrowserApp(&cpBrowser);
    if (cpBrowser)
    {
        USES_CONVERSION;
        cpBrowser->put_StatusText(A2OLE(message));
    }
}


////////////////////////////////////////////////////////////////////////
void * NP_EXPORT
NPN_MemAlloc (uint32 size)
{
    return malloc(size);
}


////////////////////////////////////////////////////////////////////////
void NP_EXPORT
NPN_MemFree (void *ptr)
{
    if (ptr)
    {
        free(ptr);
    }
}


////////////////////////////////////////////////////////////////////////
uint32 NP_EXPORT
NPN_MemFlush(uint32 size)
{
    return 0;
}


////////////////////////////////////////////////////////////////////////
void NP_EXPORT
NPN_ReloadPlugins(NPBool reloadPages)
{
}


////////////////////////////////////////////////////////////////////////
void NP_EXPORT
NPN_InvalidateRect(NPP npp, NPRect *invalidRect)
{
    if (!npp)
    {
        return;
    }

    // TODO - windowless plugins
}


////////////////////////////////////////////////////////////////////////
void NP_EXPORT
NPN_InvalidateRegion(NPP npp, NPRegion invalidRegion)
{
    if (!npp)
    {
        return;
    }
    // TODO - windowless plugins
}


////////////////////////////////////////////////////////////////////////
void NP_EXPORT
NPN_ForceRedraw(NPP npp)
{
    if (!npp)
    {
        return;
    }
    // TODO - windowless plugins
}

////////////////////////////////////////////////////////////////////////
NPError NP_EXPORT
NPN_GetValue(NPP npp, NPNVariable variable, void *result)
{
    if (!npp)
    {
        return NPERR_INVALID_INSTANCE_ERROR;
    }

    if (!result)
    {
        return NPERR_INVALID_PARAM;
    }

    nsPluginHostCtrl *pCtrl = (nsPluginHostCtrl *) npp->ndata;
    ATLASSERT(pCtrl);

    CComPtr<IWebBrowserApp> cpBrowser;
    pCtrl->GetWebBrowserApp(&cpBrowser);

    // Test the variable
    if (variable == NPNVnetscapeWindow)
    {
        *((HWND *) result) = pCtrl->m_wndPlugin.m_hWnd;
    }
    else if (variable == NPNVjavascriptEnabledBool)
    {
        // TODO
        *((NPBool *) result) = TRUE;
    }
    else if (variable == NPNVasdEnabledBool) // Smart update
    {
        *((NPBool *) result) = FALSE;
    }
    else if (variable == NPNVisOfflineBool)
    {
        *((NPBool *) result) = FALSE;
        if (cpBrowser)
        {
            CComQIPtr<IWebBrowser2> cpBrowser2 = cpBrowser;
            if (cpBrowser2)
            {
                VARIANT_BOOL bOffline = VARIANT_FALSE;
                cpBrowser2->get_Offline(&bOffline);
                *((NPBool *) result) = (bOffline == VARIANT_TRUE) ? TRUE : FALSE;
            }
        }
    }
    else
    {
        return NPERR_GENERIC_ERROR;
    }

    return NPERR_NO_ERROR;
}


////////////////////////////////////////////////////////////////////////
NPError NP_EXPORT
NPN_SetValue(NPP npp, NPPVariable variable, void *result)
{
    if (!npp)
    {
        return NPERR_INVALID_INSTANCE_ERROR;
    }

    // TODO windowless
    // NPPVpluginWindowBool
    // NPPVpluginTransparentBool

    return NPERR_GENERIC_ERROR;
}


////////////////////////////////////////////////////////////////////////
NPError NP_EXPORT
NPN_RequestRead(NPStream *pstream, NPByteRange *rangeList)
{
    if (!pstream || !rangeList || !pstream->ndata)
    {
        return NPERR_INVALID_PARAM;
    }

    return NPERR_GENERIC_ERROR;
}

////////////////////////////////////////////////////////////////////////
JRIEnv* NP_EXPORT
NPN_GetJavaEnv(void)
{
    return NULL;
}


////////////////////////////////////////////////////////////////////////
const char * NP_EXPORT
NPN_UserAgent(NPP npp)
{
    return NULL;
}


////////////////////////////////////////////////////////////////////////
java_lang_Class* NP_EXPORT
NPN_GetJavaClass(void* handle)
{
    return NULL;
}


////////////////////////////////////////////////////////////////////////
jref NP_EXPORT
NPN_GetJavaPeer(NPP npp)
{
    return NULL;
}


