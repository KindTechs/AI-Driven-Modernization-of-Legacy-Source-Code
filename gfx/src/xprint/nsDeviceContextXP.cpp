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
 *   Bradley Baetz <bbaetz@cs.mcgill.ca>
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

#define FORCE_PR_LOG /* Allow logging in the release build */
#define PR_LOGGING 1
#include "prlog.h"
 
/* PostScript/Xprint print modules do not support more than one object
 * instance because they use global vars which cannot be shared between
 * multiple instances...
 * bug 119491 ("Cleanup global vars in PostScript and Xprint modules) will fix
 * that...
 */
#define WE_DO_NOT_SUPPORT_MULTIPLE_PRINT_DEVICECONTEXTS 1 
 
#include "nsDeviceContextXP.h"
#include "nsRenderingContextXp.h"
#include "nsFontMetricsXlib.h"
#include "nsIDeviceContext.h"
#include "nsIDeviceContextSpecXPrint.h"
#include "nspr.h"
#include "nsXPrintContext.h"

#ifdef PR_LOGGING
static PRLogModuleInfo *nsDeviceContextXpLM = PR_NewLogModule("nsDeviceContextXp");
#endif /* PR_LOGGING */

#ifdef WE_DO_NOT_SUPPORT_MULTIPLE_PRINT_DEVICECONTEXTS
static int instance_counter = 0;
#endif /* WE_DO_NOT_SUPPORT_MULTIPLE_PRINT_DEVICECONTEXTS */

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 */
nsDeviceContextXp :: nsDeviceContextXp()
 : nsDeviceContextX()
{ 
  mPrintContext        = nsnull;
  mSpec                = nsnull; 
  mParentDeviceContext = nsnull;
  
#ifdef WE_DO_NOT_SUPPORT_MULTIPLE_PRINT_DEVICECONTEXTS
  instance_counter++;
  NS_ASSERTION(instance_counter < 2, "Cannot have more than one print device context.");
#endif /* WE_DO_NOT_SUPPORT_MULTIPLE_PRINT_DEVICECONTEXTS */
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *        @update 12/21/98 dwc
 */
nsDeviceContextXp :: ~nsDeviceContextXp() 
{ 
  DestroyXPContext();

#ifdef WE_DO_NOT_SUPPORT_MULTIPLE_PRINT_DEVICECONTEXTS
  instance_counter--;
  NS_ASSERTION(instance_counter >= 0, "We cannot have less than zero instances.");
#endif /* WE_DO_NOT_SUPPORT_MULTIPLE_PRINT_DEVICECONTEXTS */
}


NS_IMETHODIMP
nsDeviceContextXp::SetSpec(nsIDeviceContextSpec* aSpec)
{
  nsresult  rv = NS_ERROR_FAILURE;
  PR_LOG(nsDeviceContextXpLM, PR_LOG_DEBUG, ("nsDeviceContextXp::SetSpec()\n"));

#ifdef WE_DO_NOT_SUPPORT_MULTIPLE_PRINT_DEVICECONTEXTS
  NS_ASSERTION(instance_counter < 2, "Cannot have more than one print device context.");
  if (instance_counter > 1) {
    return NS_ERROR_GFX_PRINTER_PRINT_WHILE_PREVIEW;
  }
#endif /* WE_DO_NOT_SUPPORT_MULTIPLE_PRINT_DEVICECONTEXTS */

  nsCOMPtr<nsIDeviceContextSpecXp> xpSpec;

  mSpec = aSpec;

  if (mPrintContext)
    DestroyXPContext(); // we cannot reuse that...
    
  mPrintContext = new nsXPrintContext();
  if (!mPrintContext)
    return  NS_ERROR_OUT_OF_MEMORY;
    
  xpSpec = do_QueryInterface(mSpec, &rv);
  if (NS_SUCCEEDED(rv)) {
    rv = mPrintContext->Init(this, xpSpec);
  }
 
  return rv;
}

NS_IMPL_ISUPPORTS_INHERITED1(nsDeviceContextXp,
                             DeviceContextImpl,
                             nsIDeviceContextXp)

/** ---------------------------------------------------
 *  See documentation in nsDeviceContextXp.h
 * 
 * This method is called after SetSpec
 */
NS_IMETHODIMP
nsDeviceContextXp::InitDeviceContextXP(nsIDeviceContext *aCreatingDeviceContext,nsIDeviceContext *aParentContext)
{
  // Initialization moved to SetSpec to be done after creating the Print Context
  float origscale, newscale;
  float t2d, a2d;
  int   print_resolution;

#ifdef WE_DO_NOT_SUPPORT_MULTIPLE_PRINT_DEVICECONTEXTS
  NS_ASSERTION(instance_counter < 2, "Cannot have more than one print device context.");
  if (instance_counter > 1) {
    return NS_ERROR_GFX_PRINTER_PRINT_WHILE_PREVIEW;
  }
#endif /* WE_DO_NOT_SUPPORT_MULTIPLE_PRINT_DEVICECONTEXTS */

  mPrintContext->GetPrintResolution(print_resolution);

  mPixelsToTwips = (float)NSIntPointsToTwips(72) / (float)print_resolution;
  mTwipsToPixels = 1.0f / mPixelsToTwips;

  GetTwipsToDevUnits(newscale);
  aParentContext->GetTwipsToDevUnits(origscale);

  mCPixelScale = newscale / origscale;

  aParentContext->GetTwipsToDevUnits(t2d);
  aParentContext->GetAppUnitsToDevUnits(a2d);

  mAppUnitsToDevUnits = (a2d / t2d) * mTwipsToPixels;
  mDevUnitsToAppUnits = 1.0f / mAppUnitsToDevUnits;

  NS_ASSERTION(aParentContext, "aCreatingDeviceContext cannot be NULL!!!");
  mParentDeviceContext = aParentContext;

  /* be sure we've cleaned-up old rubbish - new values will re-populate nsFontMetricsXlib soon... */
  nsFontMetricsXlib::FreeGlobals();
  nsFontMetricsXlib::EnablePrinterMode(PR_TRUE);
    
  return NS_OK;
}

/** ---------------------------------------------------
 */
NS_IMETHODIMP nsDeviceContextXp :: CreateRenderingContext(nsIRenderingContext *&aContext)
{
  nsresult rv;
   
  aContext = nsnull;

  nsCOMPtr<nsRenderingContextXp> renderingContext = new nsRenderingContextXp();
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
 */
NS_IMETHODIMP nsDeviceContextXp :: SupportsNativeWidgets(PRBool &aSupportsWidgets)
{
  aSupportsWidgets = PR_FALSE;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 */
NS_IMETHODIMP nsDeviceContextXp :: GetScrollBarDimensions(float &aWidth, 
                                        float &aHeight) const
{
  // XXX Oh, yeah.  These are hard coded.
  float scale;
  GetCanonicalPixelScale(scale);
  aWidth  = 15.f * mPixelsToTwips * scale;
  aHeight = 15.f * mPixelsToTwips * scale;

  return NS_OK;
}

void  nsDeviceContextXp :: SetDrawingSurface(nsDrawingSurface  aSurface) 
{ 
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 */
NS_IMETHODIMP nsDeviceContextXp :: GetDrawingSurface(nsIRenderingContext &aContext, nsDrawingSurface &aSurface)
{
  aSurface = nsnull;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *        @update 12/21/98 dwc
 */
NS_IMETHODIMP nsDeviceContextXp :: CheckFontExistence(const nsString& aFontName)
{
  return nsFontMetricsXlib::FamilyExists(this, aFontName);
}

NS_IMETHODIMP nsDeviceContextXp :: GetSystemFont(nsSystemFontID aID, 
                                                 nsFont *aFont) const
{
  if (mParentDeviceContext != nsnull) {
    return mParentDeviceContext->GetSystemFont(aID, aFont);
  }
  return NS_ERROR_FAILURE;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 */
NS_IMETHODIMP nsDeviceContextXp::GetDeviceSurfaceDimensions(PRInt32 &aWidth, 
                                                        PRInt32 &aHeight)
{
  float width, height;
  width  = (float) mPrintContext->GetWidth();
  height = (float) mPrintContext->GetHeight();

  aWidth  = NSToIntRound(width  * mDevUnitsToAppUnits);
  aHeight = NSToIntRound(height * mDevUnitsToAppUnits);

  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextXp::GetRect(nsRect &aRect)
{
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
NS_IMETHODIMP nsDeviceContextXp::GetClientRect(nsRect &aRect)
{
  return GetRect(aRect);
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 */
NS_IMETHODIMP nsDeviceContextXp::GetDeviceContextFor(nsIDeviceContextSpec *aDevice, nsIDeviceContext *&aContext)
{
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 */
NS_IMETHODIMP nsDeviceContextXp::BeginDocument(PRUnichar * aTitle)
{  
  PR_LOG(nsDeviceContextXpLM, PR_LOG_DEBUG, ("nsDeviceContextXp::BeginDocument()\n"));
  nsresult  rv = NS_OK;
  if (mPrintContext != nsnull) {
    rv = mPrintContext->BeginDocument(aTitle);
  } 
  return rv;
}


void nsDeviceContextXp::DestroyXPContext()
{
  PR_LOG(nsDeviceContextXpLM, PR_LOG_DEBUG, ("nsDeviceContextXp::DestroyXPContext()\n"));
  if (mPrintContext != nsnull) {   
    /* gisburn: mPrintContext cannot be reused between to print 
     * tasks as the destination print server may be a different one 
     * or the printer used on the same print server has other 
     * properties (build-in fonts for example ) than the printer 
     * previously used. */
    FlushFontCache();           
    nsRenderingContextXlib::Shutdown();
    nsFontMetricsXlib::FreeGlobals();
    
    mPrintContext = nsnull; // nsCOMPtr will call |delete mPrintContext;|
  } 
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 */
NS_IMETHODIMP nsDeviceContextXp::EndDocument(void)
{
  PR_LOG(nsDeviceContextXpLM, PR_LOG_DEBUG, ("nsDeviceContextXp::EndDocument()\n"));
  nsresult rv = NS_OK;

  if (mPrintContext != nsnull) {
    rv = mPrintContext->EndDocument();
    DestroyXPContext();
  }
  
  return rv;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 */
NS_IMETHODIMP nsDeviceContextXp::AbortDocument(void)
{
  PR_LOG(nsDeviceContextXpLM, PR_LOG_DEBUG, ("nsDeviceContextXp::AbortDocument()\n"));
  nsresult rv = NS_OK;

  if (mPrintContext != nsnull) {
    rv = mPrintContext->AbortDocument();
    DestroyXPContext();
  }
  
  return rv;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 */
NS_IMETHODIMP nsDeviceContextXp::BeginPage(void)
{
  nsresult  rv = NS_OK;
  if (mPrintContext != nsnull) {
    rv = mPrintContext->BeginPage();
  }
  return rv;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *        @update 12/21/98 dwc
 */
NS_IMETHODIMP nsDeviceContextXp::EndPage(void)
{
  nsresult  rv = NS_OK;
  if (mPrintContext != nsnull) {
    rv = mPrintContext->EndPage();
  }
  return rv;
}

/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *        @update 12/21/98 dwc
 */
NS_IMETHODIMP nsDeviceContextXp :: ConvertPixel(nscolor aColor, 
                                                        PRUint32 & aPixel)
{
  PR_LOG(nsDeviceContextXpLM, PR_LOG_DEBUG, ("nsDeviceContextXp::ConvertPixel()\n"));
  aPixel = xxlib_rgb_xpixel_from_rgb(GetXlibRgbHandle(),
                                     NS_RGB(NS_GET_B(aColor),
                                            NS_GET_G(aColor),
                                            NS_GET_R(aColor)));
  return NS_OK;
}

NS_IMETHODIMP
nsDeviceContextXp::GetPrintContext(nsXPrintContext*& aContext)
{
  aContext = mPrintContext;
  return NS_OK;
}

/* Enable hack "fix" for bug 88554/116158 ("Xprint module should avoid using
 * GFX fonts unless there is no other option...") until bug 93771 ("Mozilla
 * uses low-resolution bitmap fonts on high resolution X11 displays") gets
 * fixed. */
#define XPRINT_FONT_HACK 1

class nsFontCacheXp : public nsFontCache
{
public:
  /* override DeviceContextImpl::CreateFontCache() */
  NS_IMETHOD CreateFontMetricsInstance(nsIFontMetrics** aResult);
#ifdef XPRINT_FONT_HACK
  NS_IMETHOD GetMetricsFor(const nsFont& aFont, nsIAtom* aLangGroup, nsIFontMetrics *&aMetrics);
#endif /* XPRINT_FONT_HACK */
};


NS_IMETHODIMP nsFontCacheXp::CreateFontMetricsInstance(nsIFontMetrics** aResult)
{
  NS_PRECONDITION(aResult, "null out param");
  nsIFontMetrics *fm = new nsFontMetricsXlib();
  if (!fm)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(fm);
  *aResult = fm;
  return NS_OK;
}

#ifdef XPRINT_FONT_HACK
NS_IMETHODIMP nsFontCacheXp::GetMetricsFor(const nsFont& aFont, 
                                           nsIAtom *aLangGroup,
                                           nsIFontMetrics *&aMetrics)
{
  nsFont filteredFont(aFont);
  nsAutoString filteredName;
  
  /* We're limiting here the font search to some "common" font families
   * (which are available in PostScript printers by default) only now
   * until we have a fix for bug 93771 ("Mozilla uses 
   * low-resolution bitmap fonts on high resolution X11 displays") 
   */
  char   *inbuffer,
         *tok_lasts,
         *name;
  PRBool  first = PR_TRUE;
  
  inbuffer = PL_strdup(NS_ConvertUCS2toUTF8(filteredFont.name).get());
  if (!inbuffer)
    return NS_ERROR_OUT_OF_MEMORY;
     
  for( name = PL_strtok_r(inbuffer, ",", &tok_lasts) ;
       name != nsnull ;
       name = PL_strtok_r(nsnull,   ",", &tok_lasts) )
  {     
    /* Strip leading whitespaces */
    while( isspace(*name) && *name!='\0' )
      name++;
      
    if ((!strcasecmp(name, "serif"))      ||
        (!strcasecmp(name, "sans-serif")) ||
        (!strcasecmp(name, "courier"))    ||
        (!strcasecmp(name, "times"))      ||
        (!strcasecmp(name, "helvetica")) )
    {
      if (first)
      {
        filteredName.AssignWithConversion(name);
        first = PR_FALSE;
      }
      else
      {
        filteredName.Append(NS_LITERAL_STRING(","));
        filteredName.AppendWithConversion(name);
      }
    }  
  } 
  
  PL_strfree(inbuffer);

#ifdef DEBUG_gisburn
  printf("nsFontMetricsXlib::Init: in list = '%s', filtered list '%s'\n", 
         NS_ConvertUCS2toUTF8(filteredFont.name).get(), 
         NS_ConvertUCS2toUTF8(filteredName).get());
#endif /* DEBUG_gisburn */

  /* Replace font name with the filtered version for now. */
  filteredFont.name = filteredName;
  
  return nsFontCache::GetMetricsFor(filteredFont, aLangGroup, aMetrics);
}
#endif /* XPRINT_FONT_HACK */


/* override DeviceContextImpl::CreateFontCache() */
NS_IMETHODIMP nsDeviceContextXp::CreateFontCache()
{
  mFontCache = new nsFontCacheXp();
  if (nsnull == mFontCache) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mFontCache->Init(this);
  return NS_OK;
}


