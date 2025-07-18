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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
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
 
/* PostScript/Xprint print modules do not support more than one object
 * instance because they use global vars which cannot be shared between
 * multiple instances...
 * bug 119491 ("Cleanup global vars in PostScript and Xprint modules) will fix
 * that...
 */
#define WE_DO_NOT_SUPPORT_MULTIPLE_PRINT_DEVICECONTEXTS 1

#define FORCE_PR_LOG /* Allow logging in the release build */
#define PR_LOGGING 1
#include "prlog.h"

#include "nsDeviceContextPS.h"
#include "nsRenderingContextPS.h"
#include "nsString.h"
#include "nsFontMetricsPS.h"
#include "nsPostScriptObj.h"

#ifdef PR_LOGGING
static PRLogModuleInfo *nsDeviceContextPSLM = PR_NewLogModule("nsDeviceContextPS");
#endif /* PR_LOGGING */

#ifdef WE_DO_NOT_SUPPORT_MULTIPLE_PRINT_DEVICECONTEXTS
static int instance_counter = 0;
#endif /* WE_DO_NOT_SUPPORT_MULTIPLE_PRINT_DEVICECONTEXTS */

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/21/98 dwc
 */
nsDeviceContextPS :: nsDeviceContextPS()
  : DeviceContextImpl(),
  mSpec(nsnull),
  mParentDeviceContext(nsnull),
  mPSObj(nsnull)
{ 
  PR_LOG(nsDeviceContextPSLM, PR_LOG_DEBUG, ("nsDeviceContextPS::nsDeviceContextPS()\n"));

#ifdef WE_DO_NOT_SUPPORT_MULTIPLE_PRINT_DEVICECONTEXTS
  instance_counter++;
  NS_ASSERTION(instance_counter < 2, "Cannot have more than one print device context.");
#endif /* WE_DO_NOT_SUPPORT_MULTIPLE_PRINT_DEVICECONTEXTS */
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/21/98 dwc
 */
nsDeviceContextPS::~nsDeviceContextPS()
{
  PR_LOG(nsDeviceContextPSLM, PR_LOG_DEBUG, ("nsDeviceContextPS::~nsDeviceContextPS()\n"));

  if (mPSObj) {
    delete mPSObj;
    mPSObj = nsnull;
  }
  
  /* nsCOMPtr<> will dispose the objects... */
  mSpec = nsnull;
  mParentDeviceContext = nsnull;

#ifdef WE_DO_NOT_SUPPORT_MULTIPLE_PRINT_DEVICECONTEXTS
  instance_counter--;
  NS_ASSERTION(instance_counter >= 0, "We cannot have less than zero instances.");
#endif /* WE_DO_NOT_SUPPORT_MULTIPLE_PRINT_DEVICECONTEXTS */
}

NS_IMETHODIMP
nsDeviceContextPS::SetSpec(nsIDeviceContextSpec* aSpec)
{
  PR_LOG(nsDeviceContextPSLM, PR_LOG_DEBUG, ("nsDeviceContextPS::SetSpec()\n"));

  nsresult  rv = NS_ERROR_FAILURE;

#ifdef WE_DO_NOT_SUPPORT_MULTIPLE_PRINT_DEVICECONTEXTS
  NS_ASSERTION(instance_counter < 2, "Cannot have more than one print device context.");
  if (instance_counter > 1) {
    return NS_ERROR_GFX_PRINTER_PRINT_WHILE_PREVIEW;
  }
#endif /* WE_DO_NOT_SUPPORT_MULTIPLE_PRINT_DEVICECONTEXTS */

  mSpec = aSpec;
  
  nsCOMPtr<nsIDeviceContextSpecPS> psSpec;

  mPSObj = new nsPostScriptObj();
  if (!mPSObj)
    return  NS_ERROR_OUT_OF_MEMORY; 

  psSpec = do_QueryInterface(mSpec, &rv);
  if (NS_SUCCEEDED(rv)) {
    rv = mPSObj->Init(psSpec);
  }
  
  return rv;  
}

NS_IMPL_ISUPPORTS_INHERITED1(nsDeviceContextPS,
                             DeviceContextImpl,
                             nsIDeviceContextPS)

/** ---------------------------------------------------
 *  See documentation in nsDeviceContextPS.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP
nsDeviceContextPS::InitDeviceContextPS(nsIDeviceContext *aCreatingDeviceContext,nsIDeviceContext *aParentContext)
{
  PR_LOG(nsDeviceContextPSLM, PR_LOG_DEBUG, ("nsDeviceContextPS::InitDeviceContextPS()\n"));

  float origscale, newscale;
  float t2d, a2d;

#ifdef WE_DO_NOT_SUPPORT_MULTIPLE_PRINT_DEVICECONTEXTS
  NS_ASSERTION(instance_counter < 2, "Cannot have more than one print device context.");
  if (instance_counter > 1) {
    return NS_ERROR_GFX_PRINTER_PRINT_WHILE_PREVIEW;
  }
#endif /* WE_DO_NOT_SUPPORT_MULTIPLE_PRINT_DEVICECONTEXTS */

  NS_ENSURE_ARG_POINTER(aParentContext);

  mDepth = 1;     // just for arguments sake

  mTwipsToPixels = (float)72.0/(float)NSIntPointsToTwips(72);
  mPixelsToTwips = 1.0f / mTwipsToPixels;

  GetTwipsToDevUnits(newscale);
  aParentContext->GetTwipsToDevUnits(origscale);
  mCPixelScale = newscale / origscale;

  aParentContext->GetTwipsToDevUnits(t2d);
  aParentContext->GetAppUnitsToDevUnits(a2d);

  mAppUnitsToDevUnits = (a2d / t2d) * mTwipsToPixels;
  mDevUnitsToAppUnits = 1.0f / mAppUnitsToDevUnits;

  mParentDeviceContext = aParentContext;
  return NS_OK;
}

/** ---------------------------------------------------
 *  Create a Postscript RenderingContext.  This will create a RenderingContext
 *  from the deligate RenderingContext and intall that into our Postscript RenderingContext.
 *	@update 12/21/98 dwc
 *  @param aContext -- our newly created Postscript RenderingContextPS
 *  @return -- NS_OK if everything succeeded.
 */
NS_IMETHODIMP nsDeviceContextPS::CreateRenderingContext(nsIRenderingContext *&aContext)
{
  PR_LOG(nsDeviceContextPSLM, PR_LOG_DEBUG, ("nsDeviceContextPS::CreateRenderingContext()\n"));

  nsresult rv;
   
  aContext = nsnull;

  nsCOMPtr<nsRenderingContextPS> renderingContext = new nsRenderingContextPS();
  if (!renderingContext)
    return NS_ERROR_OUT_OF_MEMORY;
     
  rv = renderingContext->Init(this);

  if (NS_SUCCEEDED(rv)) {
    aContext = renderingContext;
    NS_ADDREF(aContext);
  }

  return rv;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP nsDeviceContextPS::SupportsNativeWidgets(PRBool &aSupportsWidgets)
{
  PR_LOG(nsDeviceContextPSLM, PR_LOG_DEBUG, ("nsDeviceContextPS::SupportsNativeWidgets()\n"));

  aSupportsWidgets = PR_FALSE;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP nsDeviceContextPS::GetScrollBarDimensions(float &aWidth, float &aHeight) const
{
  PR_LOG(nsDeviceContextPSLM, PR_LOG_DEBUG, ("nsDeviceContextPS::GetScrollBarDimensions()\n"));

  //XXX: Hardcoded values for Postscript
  float scale;
  GetCanonicalPixelScale(scale);
  aWidth  = 20.f * scale;
  aHeight = 20.f * scale;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP nsDeviceContextPS::GetDrawingSurface(nsIRenderingContext &aContext, nsDrawingSurface &aSurface)
{
  PR_LOG(nsDeviceContextPSLM, PR_LOG_DEBUG, ("nsDeviceContextPS::GetDrawingSurface()\n"));

  aSurface = nsnull;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP nsDeviceContextPS::GetDepth(PRUint32& aDepth)
{
  PR_LOG(nsDeviceContextPSLM, PR_LOG_DEBUG, ("nsDeviceContextPS::GetDepth(mDepth=%d)\n", 24));

  /* PostScript module uses 24bit RGB images */
  return(24);
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP nsDeviceContextPS::CheckFontExistence(const nsString& aFontName)
{
  PR_LOG(nsDeviceContextPSLM, PR_LOG_DEBUG, ("nsDeviceContextPS::CheckFontExistence()\n"));

  // XXX this needs to find out if this font is supported for postscript
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextPS::GetSystemFont(nsSystemFontID aID, nsFont *aFont) const
{
  PR_LOG(nsDeviceContextPSLM, PR_LOG_DEBUG, ("nsDeviceContextPS::GetSystemFont()\n"));

  if (mParentDeviceContext != nsnull) {
    return mParentDeviceContext->GetSystemFont(aID, aFont);
  }
  return NS_ERROR_FAILURE;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP nsDeviceContextPS::GetDeviceSurfaceDimensions(PRInt32 &aWidth, PRInt32 &aHeight)
{
  PR_LOG(nsDeviceContextPSLM, PR_LOG_DEBUG, ("nsDeviceContextPS::GetDeviceSurfaceDimensions()\n"));

  nsresult rv = NS_ERROR_FAILURE;

  if (mPSObj && mPSObj->mPrintSetup) {
    aWidth  = NSToIntRound(mPSObj->mPrintSetup->width  * mDevUnitsToAppUnits); 
    aHeight = NSToIntRound(mPSObj->mPrintSetup->height * mDevUnitsToAppUnits); 
    rv = NS_OK;
  }
  return rv;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 */
NS_IMETHODIMP nsDeviceContextPS::GetRect(nsRect &aRect)
{
  PR_LOG(nsDeviceContextPSLM, PR_LOG_DEBUG, ("nsDeviceContextPS::GetRect()\n"));

  PRInt32 width, height;
  nsresult rv;
  rv = GetDeviceSurfaceDimensions(width, height);
  aRect.x = 0;
  aRect.y = 0;
  aRect.width = width;
  aRect.height = height;
  return rv;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 */
NS_IMETHODIMP nsDeviceContextPS::GetClientRect(nsRect &aRect)
{
  PR_LOG(nsDeviceContextPSLM, PR_LOG_DEBUG, ("nsDeviceContextPS::GetClientRect()\n"));

  return GetRect(aRect);
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP nsDeviceContextPS::GetDeviceContextFor(nsIDeviceContextSpec *aDevice, nsIDeviceContext *&aContext)
{
  PR_LOG(nsDeviceContextPSLM, PR_LOG_DEBUG, ("nsDeviceContextPS::GetDeviceContextFor()\n"));

  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP nsDeviceContextPS::BeginDocument(PRUnichar * aTitle)
{
  PR_LOG(nsDeviceContextPSLM, PR_LOG_DEBUG, ("nsDeviceContextPS::BeginDocument()\n"));

  NS_ENSURE_TRUE(mPSObj != nsnull, NS_ERROR_NULL_POINTER);

  mPSObj->settitle(aTitle); 
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP nsDeviceContextPS::EndDocument(void)
{
  PR_LOG(nsDeviceContextPSLM, PR_LOG_DEBUG, ("nsDeviceContextPS::EndDocument()\n"));

  NS_ENSURE_TRUE(mPSObj != nsnull, NS_ERROR_NULL_POINTER);
  
  /* Finish the document and print it... */  
  mPSObj->end_document();

  delete mPSObj;
  mPSObj = nsnull;

  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 */
NS_IMETHODIMP nsDeviceContextPS::AbortDocument(void)
{
  PR_LOG(nsDeviceContextPSLM, PR_LOG_DEBUG, ("nsDeviceContextPS::AbortDocument()\n"));

  NS_ENSURE_TRUE(mPSObj != nsnull, NS_ERROR_NULL_POINTER);
  
  delete mPSObj;
  mPSObj = nsnull;
  
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP nsDeviceContextPS::BeginPage(void)
{
  PR_LOG(nsDeviceContextPSLM, PR_LOG_DEBUG, ("nsDeviceContextPS::BeginPage()\n"));

  NS_ENSURE_TRUE(mPSObj != nsnull, NS_ERROR_NULL_POINTER);

  // begin the page
  mPSObj->begin_page(); 
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP nsDeviceContextPS::EndPage(void)
{
  PR_LOG(nsDeviceContextPSLM, PR_LOG_DEBUG, ("nsDeviceContextPS::EndPage()\n"));

  NS_ENSURE_TRUE(mPSObj != nsnull, NS_ERROR_NULL_POINTER);

  // end the page
  mPSObj->end_page();
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/21/98 dwc
 */
NS_IMETHODIMP nsDeviceContextPS::ConvertPixel(nscolor aColor, PRUint32 & aPixel)
{
  PR_LOG(nsDeviceContextPSLM, PR_LOG_DEBUG, ("nsDeviceContextPS::ConvertPixel()\n"));

  aPixel = aColor;
  return NS_OK;
}

class nsFontCachePS : public nsFontCache
{
public:
  /* override DeviceContextImpl::CreateFontCache() */
  NS_IMETHODIMP CreateFontMetricsInstance(nsIFontMetrics** aResult);
};


NS_IMETHODIMP nsFontCachePS::CreateFontMetricsInstance(nsIFontMetrics** aResult)
{
  NS_PRECONDITION(aResult, "null out param");
  nsIFontMetrics *fm = new nsFontMetricsPS();
  if (!fm)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(fm);
  *aResult = fm;
  return NS_OK;
}

/* override DeviceContextImpl::CreateFontCache() */
NS_IMETHODIMP nsDeviceContextPS::CreateFontCache()
{
  PR_LOG(nsDeviceContextPSLM, PR_LOG_DEBUG, ("nsDeviceContextPS::CreateFontCache()\n"));

  mFontCache = new nsFontCachePS();
  if (!mFontCache) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  
  return mFontCache->Init(this);
}


