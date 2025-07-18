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
 *   Peter Hartshorn <peter@igelaus.com.au>
 *   Ken Faulkner <faulkner@igelaus.com.au>
 *   Tony Tsui <tony@igelaus.com.au>
 *   Tomi Leppikangas <tomi.leppikangas@oulu.fi>
 *   Tim Copperfield <timecop@network.email.ne.jp>
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

#include "nsRenderingContextXlib.h"
#include "nsFontMetricsXlib.h"
#include "nsCompressedCharMap.h"
#include "xlibrgb.h"
#include "prprf.h"
#include "prmem.h"
#include "prlog.h"
#include "nsGCCache.h"
#include "imgIContainer.h"
#include "gfxIImageFrame.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"

#define NS_TO_XLIBRGB_RGB(ns) (ns & 0xff) << 16 | (ns & 0xff00) | ((ns >> 16) & 0xff)

NS_IMPL_THREADSAFE_ISUPPORTS1(nsRenderingContextXlib, nsIRenderingContext)

#ifdef PR_LOGGING 
static PRLogModuleInfo * RenderingContextXlibLM = PR_NewLogModule("RenderingContextXlib");
#endif /* PR_LOGGING */ 

/* Handle drawing 8 bit data with a 16 bit font */
static void Widen8To16AndDraw(Drawable    drawable,
                              nsXFont     *font,
                              GC           gc,
                              int          x,
                              int          y,
                              const char  *text,
                              int          text_length);


nsGCCacheXlib *nsRenderingContextXlib::gcCache = nsnull;

class GraphicsState
{
public:
  GraphicsState();
  ~GraphicsState();

  nsTransform2D           *mMatrix;
  nsCOMPtr<nsIRegion>      mClipRegion;
  nscolor                  mColor;
  nsLineStyle              mLineStyle;
  nsCOMPtr<nsIFontMetrics> mFontMetrics;
};

MOZ_DECL_CTOR_COUNTER(GraphicsState)

GraphicsState::GraphicsState() :
  mMatrix(nsnull),
  mColor(NS_RGB(0, 0, 0)),
  mLineStyle(nsLineStyle_kSolid),
  mFontMetrics(nsnull)
{
  MOZ_COUNT_CTOR(GraphicsState);

}

GraphicsState::~GraphicsState()
{
  MOZ_COUNT_DTOR(GraphicsState);
}


nsRenderingContextXlib::nsRenderingContextXlib() :
  nsRenderingContextImpl(),
  mFontMetrics(nsnull),
  mContext(nsnull),
  mSurface(nsnull),
  mOffscreenSurface(nsnull),
  mCurrentColor(NS_RGB(0, 0, 0)), /* X11 intial bg color is always _black_...
                                   * ...but we should query that from 
                                   * Xserver instead of guessing that...
                                   */
  mCurrentLineStyle(nsLineStyle_kSolid),
  mCurrentFont(nsnull),
  mP2T(1.0f),
  mClipRegion(nsnull),
  mGC(nsnull),
  mFunction(GXcopy)
{
  PR_LOG(RenderingContextXlibLM, PR_LOG_DEBUG, ("nsRenderingContextXlib::nsRenderingContextXlib()\n"));
  NS_INIT_REFCNT();

  PushState();
}

nsRenderingContextXlib::~nsRenderingContextXlib()
{
  PR_LOG(RenderingContextXlibLM, PR_LOG_DEBUG, ("nsRenderingContextXlib::~nsRenderingContextXlib()\n"));
  /* Destroy the State Machine */
  PRInt32 cnt = mStateCache.Count();

  while (--cnt >= 0) {
    PRBool clipstate;
    PopState(clipstate);
  }

  if (mTranMatrix)
    delete mTranMatrix;
  
  if (mGC)
    mGC->Release();
}

/*static*/ nsresult
nsRenderingContextXlib::Shutdown()
{
  if (gcCache) {
    delete gcCache;
    gcCache = nsnull;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::Init(nsIDeviceContext* aContext, nsIWidget *aWindow)
{
  PR_LOG(RenderingContextXlibLM, PR_LOG_DEBUG, ("nsRenderingContextXlib::Init(DeviceContext, Widget)\n"));
  nsDrawingSurfaceXlibImpl *surf; // saves some cast stunts

  NS_ENSURE_TRUE(aContext != nsnull, NS_ERROR_NULL_POINTER);
  NS_ENSURE_TRUE(aWindow  != nsnull, NS_ERROR_NULL_POINTER);
  mContext = aContext;
  
  nsIDeviceContext *dc = mContext;     
  NS_STATIC_CAST(nsDeviceContextX *, dc)->GetXlibRgbHandle(mXlibRgbHandle);
  mDisplay = xxlib_rgb_get_display(mXlibRgbHandle);

  surf = new nsDrawingSurfaceXlibImpl();

  if (surf) {
    Drawable  win = (Drawable)aWindow->GetNativeData(NS_NATIVE_WINDOW);
    xGC      *gc  = (xGC *)aWindow->GetNativeData(NS_NATIVE_GRAPHIC);

    surf->Init(mXlibRgbHandle, 
               win, 
               gc);

    mOffscreenSurface = mSurface = surf;
    /* aWindow->GetNativeData() ref'd the gc */
    gc->Release();
  }

  return CommonInit();
}

NS_IMETHODIMP
nsRenderingContextXlib::Init(nsIDeviceContext* aContext, nsDrawingSurface aSurface)
{
  PR_LOG(RenderingContextXlibLM, PR_LOG_DEBUG, ("nsRenderingContxtXlib::Init(DeviceContext, DrawingSurface)\n"));

  NS_ENSURE_TRUE(nsnull != aContext, NS_ERROR_NULL_POINTER);
  mContext = aContext;
  
  nsIDeviceContext *dc = mContext;     
  NS_STATIC_CAST(nsDeviceContextX *, dc)->GetXlibRgbHandle(mXlibRgbHandle);
  mDisplay = xxlib_rgb_get_display(mXlibRgbHandle);

  mSurface = (nsIDrawingSurfaceXlib *)aSurface;

  return CommonInit();
}

nsresult nsRenderingContextXlib::CommonInit(void)
{
  PR_LOG(RenderingContextXlibLM, PR_LOG_DEBUG, ("nsRenderingContxtXlib::CommonInit()\n"));
  // put common stuff in here.
  int x, y;
  unsigned int width, height, border, depth;
  Window root_win;

  Drawable drawable; mSurface->GetDrawable(drawable);

  ::XGetGeometry(mDisplay, 
                 drawable, 
                 &root_win,
                 &x, 
                 &y, 
                 &width, 
                 &height, 
                 &border, 
                 &depth);

  PR_LOG(RenderingContextXlibLM, PR_LOG_DEBUG, 
         ("XGetGeometry(display=%lx,drawable=%lx) ="
         " {root_win=%lx, x=%d, y=%d, width=%d, height=%d. border=%d, depth=%d}\n",
         (long)mDisplay, (long)drawable,
         (long)root_win, (int)x, (int)y, (int)width, (int)height, (int)border, (int)depth));

  mClipRegion = new nsRegionXlib();
  if (!mClipRegion)
    return NS_ERROR_OUT_OF_MEMORY;
  mClipRegion->Init();
  mClipRegion->SetTo(0, 0, width, height);

  mContext->GetDevUnitsToAppUnits(mP2T);
  float app2dev;
  mContext->GetAppUnitsToDevUnits(app2dev);
  mTranMatrix->AddScale(app2dev, app2dev);
  return NS_OK;
}


NS_IMETHODIMP
nsRenderingContextXlib::GetHints(PRUint32& aResult)
{
  PRUint32 result = 0;

  // Most X servers implement 8 bit text rendering alot faster than
  // XChar2b rendering. In addition, we can avoid the PRUnichar to
  // XChar2b conversion. So we set this bit...
  result |= NS_RENDERING_HINT_FAST_8BIT_TEXT;

  // XXX see if we are rendering to the local display or to a remote
  // dispaly and set the NS_RENDERING_HINT_REMOTE_RENDERING accordingly

  aResult = result;
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::LockDrawingSurface(PRInt32 aX, PRInt32 aY,
                                           PRUint32 aWidth, PRUint32 aHeight,
                                           void **aBits, PRInt32 *aStride,
                                           PRInt32 *aWidthBytes, PRUint32 aFlags)
{
  PR_LOG(RenderingContextXlibLM, PR_LOG_DEBUG, ("nsRenderingContextXlib::LockDrawingSurface()\n"));
  PushState();

  return mSurface->Lock(aX, aY, aWidth, aHeight,
                        aBits, aStride, aWidthBytes, aFlags);
}

NS_IMETHODIMP
nsRenderingContextXlib::UnlockDrawingSurface(void)
{
  PR_LOG(RenderingContextXlibLM, PR_LOG_DEBUG, ("nsRenderingContextXlib::UnlockDrawingSurface()\n"));
  PRBool clipstate;
  PopState(clipstate);

  mSurface->Unlock();
  
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::SelectOffScreenDrawingSurface(nsDrawingSurface aSurface)
{
  PR_LOG(RenderingContextXlibLM, PR_LOG_DEBUG, ("nsRenderingContextXlib::SelectOffScreenDrawingSurface()\n"));
  if (nsnull == aSurface)
    mSurface = mOffscreenSurface;
  else
    mSurface = (nsIDrawingSurfaceXlib *)aSurface;

  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::GetDrawingSurface(nsDrawingSurface *aSurface)
{
  PR_LOG(RenderingContextXlibLM, PR_LOG_DEBUG, ("nsRenderingContextXlib::GetDrawingSurface()\n"));
  *aSurface = mSurface;
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::Reset()
{
  PR_LOG(RenderingContextXlibLM, PR_LOG_DEBUG, ("nsRenderingContextXlib::Reset()\n"));
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextXlib::GetDeviceContext(nsIDeviceContext *&aContext)
{
  aContext = mContext;
  NS_IF_ADDREF(aContext);
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::PushState(void)
{
  PR_LOG(RenderingContextXlibLM, PR_LOG_DEBUG, ("nsRenderingContextXlib::PushState()\n"));
  GraphicsState *state = new GraphicsState();

  if (!state)
    return NS_ERROR_OUT_OF_MEMORY;

  state->mMatrix = mTranMatrix;
  
  mStateCache.AppendElement(state);
  
  if (nsnull == mTranMatrix)
    mTranMatrix = new nsTransform2D();
  else
    mTranMatrix = new nsTransform2D(mTranMatrix);
  
  if (mClipRegion) {
    state->mClipRegion = mClipRegion;
    mClipRegion = new nsRegionXlib();
    if (!mClipRegion)
      return NS_ERROR_OUT_OF_MEMORY;
    mClipRegion->Init();
    mClipRegion->SetTo(*state->mClipRegion);
  }
  
  state->mFontMetrics = mFontMetrics;
  state->mColor       = mCurrentColor;
  state->mLineStyle   = mCurrentLineStyle;
  
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::PopState(PRBool &aClipState)
{
  PR_LOG(RenderingContextXlibLM, PR_LOG_DEBUG, ("nsRenderingContextXlib::PopState()\n"));

  PRUint32 cnt = mStateCache.Count();
  GraphicsState *state;
  
  if (cnt > 0) {
    state = (GraphicsState *)mStateCache.ElementAt(cnt - 1);
    mStateCache.RemoveElementAt(cnt - 1);
    
    if (mTranMatrix)
      delete mTranMatrix;
    mTranMatrix = state->mMatrix;
    
    mClipRegion = state->mClipRegion;
    if (mFontMetrics != state->mFontMetrics)
      SetFont(state->mFontMetrics);

    if (state->mColor != mCurrentColor)
      SetColor(state->mColor);
    if (state->mLineStyle != mCurrentLineStyle)
      SetLineStyle(state->mLineStyle);

    delete state;
  }

  if (mClipRegion)
    aClipState = mClipRegion->IsEmpty();
  else 
    aClipState = PR_TRUE;

  return NS_OK;
}

#ifdef DEBUG
#undef TRACE_SET_CLIP
#endif

#ifdef TRACE_SET_CLIP
static char *
nsClipCombine_to_string(nsClipCombine aCombine)
{
#ifdef TRACE_SET_CLIP
  printf("nsRenderingContextXlib::SetClipRect(x=%d,y=%d,w=%d,h=%d,%s)\n",
         trect.x,
         trect.y,
         trect.width,
         trect.height,
         nsClipCombine_to_string(aCombine));
#endif // TRACE_SET_CLIP

  switch(aCombine)
  {
    case nsClipCombine_kIntersect:
      return "nsClipCombine_kIntersect";
      break;

    case nsClipCombine_kUnion:
      return "nsClipCombine_kUnion";
      break;

    case nsClipCombine_kSubtract:
      return "nsClipCombine_kSubtract";
      break;

    case nsClipCombine_kReplace:
      return "nsClipCombine_kReplace";
      break;
  }

  return "something got screwed";
}
#endif // TRACE_SET_CLIP

NS_IMETHODIMP
nsRenderingContextXlib::IsVisibleRect(const nsRect& aRect, PRBool &aVisible)
{
  PR_LOG(RenderingContextXlibLM, PR_LOG_DEBUG, ("nsRenderingContextXlib::IsVisibleRect()\n"));
  aVisible = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::SetClipRect(const nsRect& aRect, nsClipCombine aCombine, PRBool &aClipState)
{
  PR_LOG(RenderingContextXlibLM, PR_LOG_DEBUG, ("nsRenderingContextXlib::SetClipRect()\n"));
  nsRect trect = aRect;
  Region rgn;
  mTranMatrix->TransformCoord(&trect.x, &trect.y,
                           &trect.width, &trect.height);
  switch(aCombine) {
  case nsClipCombine_kIntersect:
    mClipRegion->Intersect(trect.x,trect.y,trect.width,trect.height);
    break;
  case nsClipCombine_kUnion:
    mClipRegion->Union(trect.x,trect.y,trect.width,trect.height);
    break;
  case nsClipCombine_kSubtract:
    mClipRegion->Subtract(trect.x,trect.y,trect.width,trect.height);
    break;
  case nsClipCombine_kReplace:
    mClipRegion->SetTo(trect.x,trect.y,trect.width,trect.height);
    break;
  }
  aClipState = mClipRegion->IsEmpty();

  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::GetClipRect(nsRect &aRect, PRBool &aClipState)
{
  PR_LOG(RenderingContextXlibLM, PR_LOG_DEBUG, ("nsRenderingContextXlib::GetClipRext()\n"));
  PRInt32 x, y, w, h;
  if (!mClipRegion->IsEmpty()) {
    mClipRegion->GetBoundingBox(&x,&y,&w,&h);
    aRect.SetRect(x,y,w,h);
    aClipState = PR_TRUE;
  } else {
    aRect.SetRect(0,0,0,0);
    aClipState = PR_FALSE;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::SetClipRegion(const nsIRegion& aRegion, nsClipCombine aCombine, PRBool &aClipState)
{
  PR_LOG(RenderingContextXlibLM, PR_LOG_DEBUG, ("nsRenderingContextXlib::SetClipRegion()\n"));
  Region rgn;
  switch(aCombine)
  {
    case nsClipCombine_kIntersect:
      mClipRegion->Intersect(aRegion);
      break;
    case nsClipCombine_kUnion:
      mClipRegion->Union(aRegion);
      break;
    case nsClipCombine_kSubtract:
      mClipRegion->Subtract(aRegion);
      break;
    case nsClipCombine_kReplace:
      mClipRegion->SetTo(aRegion);
      break;
  }

  aClipState = mClipRegion->IsEmpty();

  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::CopyClipRegion(nsIRegion &aRegion)
{
  PR_LOG(RenderingContextXlibLM, PR_LOG_DEBUG, ("nsRenderingContextXlib::CopyClipRegion()\n"));
  aRegion.SetTo(*mClipRegion);
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::GetClipRegion(nsIRegion **aRegion)
{
  PR_LOG(RenderingContextXlibLM, PR_LOG_DEBUG, ("nsRenderingContextXlib::GetClipRegion()\n"));
  nsresult  rv = NS_OK;
  
  NS_ASSERTION(!(nsnull == aRegion), "no region ptr");
  
  if (*aRegion) {
    (*aRegion)->SetTo(*mClipRegion);
  }
  return rv;
}

void nsRenderingContextXlib::UpdateGC()
{
   PR_LOG(RenderingContextXlibLM, PR_LOG_DEBUG, ("nsRenderingContextXlib::UpdateGC()\n"));
   XGCValues     values;
   unsigned long valuesMask = 0;

   Drawable drawable; mSurface->GetDrawable(drawable);
 
   if (mGC)
     mGC->Release();
 
   memset(&values, 0, sizeof(XGCValues));
 
   unsigned long color;
   color = xxlib_rgb_xpixel_from_rgb (mXlibRgbHandle,
                                      NS_RGB(NS_GET_B(mCurrentColor),
                                             NS_GET_G(mCurrentColor),
                                             NS_GET_R(mCurrentColor)));
   values.foreground = color;
   valuesMask |= GCForeground;

   if (mCurrentFont && XFontStructPtr(*mCurrentFont)) {
     valuesMask |= GCFont;
     values.font = XFontStructPtr(*mCurrentFont)->fid;
   }
 
   values.line_style = mLineStyle;
   valuesMask |= GCLineStyle;
 
   values.function = mFunction;
   valuesMask |= GCFunction;

   Region rgn = nsnull;
   if (mClipRegion) { 
     mClipRegion->GetNativeRegion((void*&)rgn);
   }
 
   if (!gcCache) {
     gcCache = new nsGCCacheXlib();
     if (!gcCache) 
       return;
   }

   mGC = gcCache->GetGC(mDisplay, drawable,
                        valuesMask, &values, rgn);
}

NS_IMETHODIMP
nsRenderingContextXlib::SetColor(nscolor aColor)
{
  PR_LOG(RenderingContextXlibLM, PR_LOG_DEBUG, ("nsRenderingContextXlib::SetColor(nscolor)\n"));
  NS_ENSURE_TRUE(mContext != nsnull, NS_ERROR_FAILURE);

  mCurrentColor = aColor;

  PR_LOG(RenderingContextXlibLM, PR_LOG_DEBUG, ("Setting color to %d %d %d\n", NS_GET_R(aColor), NS_GET_G(aColor), NS_GET_B(aColor)));
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::GetColor(nscolor &aColor) const
{
  PR_LOG(RenderingContextXlibLM, PR_LOG_DEBUG, ("nsRenderingContextXlib::GetColor()\n"));

  aColor = mCurrentColor;
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::SetFont(const nsFont& aFont)
{
  nsCOMPtr<nsIFontMetrics> newMetrics;
  nsresult rv = mContext->GetMetricsFor(aFont, *getter_AddRefs(newMetrics));
  if (NS_SUCCEEDED(rv)) {
    rv = SetFont(newMetrics);
  }
  return rv;
}

NS_IMETHODIMP
nsRenderingContextXlib::SetFont(nsIFontMetrics *aFontMetrics)
{
  PR_LOG(RenderingContextXlibLM, PR_LOG_DEBUG, ("nsRenderingContextXlib::SetFont()\n"));

  mFontMetrics = aFontMetrics;

  if (mFontMetrics)
  {
    nsFontHandle  fontHandle;
    mFontMetrics->GetFontHandle(fontHandle);
    mCurrentFont = (nsFontXlib *)fontHandle;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::SetLineStyle(nsLineStyle aLineStyle)
{
  PR_LOG(RenderingContextXlibLM, PR_LOG_DEBUG, ("nsRenderingContextXlib::SetLineStyle()\n"));

  if (aLineStyle != mCurrentLineStyle) {
    /* XXX this isnt done in gtk, copy from there when ready
    switch(aLineStyle)
      { 
      case nsLineStyle_kSolid:
        mLineStyle = LineSolid;
        mDashes = 0;
        break;
      case nsLineStyle_kDashed:
          static char dashed[2] = {4,4};
        mDashList = dashed;
        mDashes = 2;
        break;
      case nsLineStyle_kDotted:
          static char dotted[2] = {3,1};
        mDashList = dotted;
        mDashes = 2;
        break;
    default:
        break;

    }
    */
    mCurrentLineStyle = aLineStyle ;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::GetLineStyle(nsLineStyle &aLineStyle)
{
  PR_LOG(RenderingContextXlibLM, PR_LOG_DEBUG, ("nsRenderingContextXlib::GetLineStyle()\n"));
  aLineStyle = mCurrentLineStyle;
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::GetFontMetrics(nsIFontMetrics *&aFontMetrics)
{
  PR_LOG(RenderingContextXlibLM, PR_LOG_DEBUG, ("nsRenderingContextXlib::GetFontMetrics()\n"));

  aFontMetrics = mFontMetrics;
  NS_IF_ADDREF(aFontMetrics);
  return NS_OK;
}

// add the passed in translation to the current translation
NS_IMETHODIMP
nsRenderingContextXlib::Translate(nscoord aX, nscoord aY)
{
  PR_LOG(RenderingContextXlibLM, PR_LOG_DEBUG, ("nsRenderingContextXlib::Translate()\n"));
  mTranMatrix->AddTranslation((float)aX,(float)aY);
  return NS_OK;
}

// add the passed in scale to the current scale
NS_IMETHODIMP
nsRenderingContextXlib::Scale(float aSx, float aSy)
{
  PR_LOG(RenderingContextXlibLM, PR_LOG_DEBUG, ("nsRenderingContextXlib::Scale()\n"));
  mTranMatrix->AddScale(aSx, aSy);
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::GetCurrentTransform(nsTransform2D *&aTransform)
{
  PR_LOG(RenderingContextXlibLM, PR_LOG_DEBUG, ("nsRenderingContextXlib::GetCurrentTransform()\n"));
  aTransform = mTranMatrix;
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::CreateDrawingSurface(nsRect *aBounds,
                                             PRUint32 aSurfFlags,
                                             nsDrawingSurface &aSurface)
{
  PR_LOG(RenderingContextXlibLM, PR_LOG_DEBUG, ("nsRenderingContextXlib::CreateDrawingSurface()\n"));

  if (nsnull == mSurface) {
    aSurface = nsnull;
    return NS_ERROR_FAILURE;
  }

  NS_ENSURE_TRUE(aBounds != nsnull, NS_ERROR_FAILURE);
  NS_ENSURE_TRUE((aBounds->width > 0) && (aBounds->height > 0), NS_ERROR_FAILURE);
 
  nsresult rv = NS_ERROR_FAILURE;
  nsDrawingSurfaceXlibImpl *surf = new nsDrawingSurfaceXlibImpl();

  if (surf)
  {
    NS_ADDREF(surf);
    UpdateGC();
    rv = surf->Init(mXlibRgbHandle, mGC, aBounds->width, aBounds->height, aSurfFlags);    
  }

  aSurface = (nsDrawingSurface)surf;

  return rv;
}

NS_IMETHODIMP
nsRenderingContextXlib::DestroyDrawingSurface(nsDrawingSurface aDS)
{
  PR_LOG(RenderingContextXlibLM, PR_LOG_DEBUG, ("nsRenderingContextXlib::DestroyDrawingSurface()\n"));
  nsIDrawingSurfaceXlib *surf = NS_STATIC_CAST(nsIDrawingSurfaceXlib *, aDS);;

  NS_ENSURE_TRUE(surf != nsnull, NS_ERROR_FAILURE);

  NS_IF_RELEASE(surf);

  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::DrawLine(nscoord aX0, nscoord aY0, nscoord aX1, nscoord aY1)
{
  PR_LOG(RenderingContextXlibLM, PR_LOG_DEBUG, ("nsRenderingContextXlib::DrawLine()\n"));

  nscoord diffX, diffY;

  NS_ENSURE_TRUE(mTranMatrix != nsnull, NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(mSurface    != nsnull, NS_ERROR_FAILURE);

  mTranMatrix->TransformCoord(&aX0,&aY0);
  mTranMatrix->TransformCoord(&aX1,&aY1);
  
  diffX = aX1-aX0;
  diffY = aY1-aY0;

  if (0!=diffX) {
    diffX = (diffX>0?1:-1);
  }
  if (0!=diffY) {
    diffY = (diffY>0?1:-1);
  }

  UpdateGC();
  Drawable drawable; mSurface->GetDrawable(drawable);
  ::XDrawLine(mDisplay, drawable,
              *mGC, aX0, aY0, aX1 - diffX, aY1 - diffY);

  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::DrawStdLine(nscoord aX0, nscoord aY0, nscoord aX1, nscoord aY1)
{
  PR_LOG(RenderingContextXlibLM, PR_LOG_DEBUG, ("nsRenderingContextXlib::DrawStdLine()\n"));

  nscoord diffX, diffY;

  NS_ENSURE_TRUE(mTranMatrix != nsnull, NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(mSurface    != nsnull, NS_ERROR_FAILURE);

  mTranMatrix->TransformCoord(&aX0,&aY0);
  mTranMatrix->TransformCoord(&aX1,&aY1);
  
  diffX = aX1-aX0;
  diffY = aY1-aY0;

  if (0!=diffX) {
    diffX = (diffX>0?1:-1);
  }
  if (0!=diffY) {
    diffY = (diffY>0?1:-1);
  }

  UpdateGC();
  Drawable drawable; mSurface->GetDrawable(drawable);
  ::XDrawLine(mDisplay, drawable,
              *mGC, aX0, aY0, aX1 - diffX, aY1 - diffY);

  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::DrawPolyline(const nsPoint aPoints[], PRInt32 aNumPoints)
{
  PR_LOG(RenderingContextXlibLM, PR_LOG_DEBUG, ("nsRenderingContextXlib::DrawPolyLine()\n"));

  NS_ENSURE_TRUE(mTranMatrix != nsnull, NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(mSurface    != nsnull, NS_ERROR_FAILURE);

  PRInt32  i;
  XPoint * xpoints;
  XPoint * thispoint;

  xpoints = (XPoint *) malloc(sizeof(XPoint) * aNumPoints);
  NS_ENSURE_TRUE(xpoints != nsnull, NS_ERROR_OUT_OF_MEMORY);

  for (i = 0; i < aNumPoints; i++){
    thispoint = (xpoints+i);
    thispoint->x = aPoints[i].x;
    thispoint->y = aPoints[i].y;
    mTranMatrix->TransformCoord((PRInt32*)&thispoint->x,(PRInt32*)&thispoint->y);
  }

  UpdateGC();
  Drawable drawable; mSurface->GetDrawable(drawable);
  ::XDrawLines(mDisplay,
               drawable,
               *mGC,
               xpoints, aNumPoints, CoordModeOrigin);

  free((void *)xpoints);

  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::DrawRect(const nsRect& aRect)
{
  PR_LOG(RenderingContextXlibLM, PR_LOG_DEBUG, ("nsRenderingContextXlib::DrawRext(const nsRect& aRect)\n"));
  return DrawRect(aRect.x, aRect.y, aRect.width, aRect.height);
}

NS_IMETHODIMP
nsRenderingContextXlib::DrawRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  PR_LOG(RenderingContextXlibLM, PR_LOG_DEBUG, ("nsRenderingContextXlib::DrawRect(aX, aY, aWidth, aHeight)\n"));

  NS_ENSURE_TRUE(mTranMatrix != nsnull, NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(mSurface    != nsnull, NS_ERROR_FAILURE);

  nscoord x, y, w, h; 
  
  x = aX;
  y = aY; 
  w = aWidth;
  h = aHeight;
    
  mTranMatrix->TransformCoord(&x,&y,&w,&h);
    
  // After the transform, if the numbers are huge, chop them, because
  // they're going to be converted from 32 bit to 16 bit.
  // It's all way off the screen anyway.
  ConditionRect(x,y,w,h);
   
  // Don't draw empty rectangles; also, w/h are adjusted down by one
  // so that the right number of pixels are drawn.
  if (w && h) 
  {
    UpdateGC();
    Drawable drawable; mSurface->GetDrawable(drawable);
    ::XDrawRectangle(mDisplay,
                     drawable,
                     *mGC,
                     x,
                     y,
                     w-1,
                     h-1);
  }

  return NS_OK;
}


NS_IMETHODIMP
nsRenderingContextXlib::FillRect(const nsRect& aRect)
{
  PR_LOG(RenderingContextXlibLM, PR_LOG_DEBUG, ("nsRenderingContextXlib::FillRect()\n"));
  return FillRect(aRect.x, aRect.y, aRect.width, aRect.height);
}

NS_IMETHODIMP
nsRenderingContextXlib::FillRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  PR_LOG(RenderingContextXlibLM, PR_LOG_DEBUG, ("nsRenderingContextXlib::FillRect()\n"));

  NS_ENSURE_TRUE(mTranMatrix != nsnull, NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(mSurface    != nsnull, NS_ERROR_FAILURE);

  nscoord x,y,w,h;
  x = aX;
  y = aY;
  w = aWidth;
  h = aHeight;

  mTranMatrix->TransformCoord(&x,&y,&w,&h);

  // After the transform, if the numbers are huge, chop them, because
  // they're going to be converted from 32 bit to 16 bit.
  // It's all way off the screen anyway.
  ConditionRect(x,y,w,h);

  Drawable drawable; mSurface->GetDrawable(drawable);
  PR_LOG(RenderingContextXlibLM, PR_LOG_DEBUG, ("About to fill window 0x%lxd with rect %d %d %d %d\n",
                                                drawable, x, y, w, h));
  UpdateGC();
  ::XFillRectangle(mDisplay,
                   drawable,
                   *mGC,
                   x,y,w,h);
  
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::InvertRect(const nsRect& aRect)
{
  PR_LOG(RenderingContextXlibLM, PR_LOG_DEBUG, ("nsRenderingContextXlib::InvertRect()\n"));
  return InvertRect(aRect.x, aRect.y, aRect.width, aRect.height);
}

NS_IMETHODIMP 
nsRenderingContextXlib::InvertRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  PR_LOG(RenderingContextXlibLM, PR_LOG_DEBUG, ("nsRenderingContextXlib::InvertRect()\n"));

  NS_ENSURE_TRUE(mTranMatrix != nsnull, NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(mSurface    != nsnull, NS_ERROR_FAILURE);
  
  nscoord x,y,w,h;

  x = aX;
  y = aY;
  w = aWidth;
  h = aHeight;

  mTranMatrix->TransformCoord(&x,&y,&w,&h);

  // After the transform, if the numbers are huge, chop them, because
  // they're going to be converted from 32 bit to 16 bit.
  // It's all way off the screen anyway.
  ConditionRect(x,y,w,h);

  mFunction = GXxor;
  
  UpdateGC();
  Drawable drawable; mSurface->GetDrawable(drawable);
  ::XFillRectangle(mDisplay,
                   drawable,
                   *mGC,
                   x,
                   y,
                   w,
                   h);
  
  mFunction = GXcopy;

  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::DrawPolygon(const nsPoint aPoints[], PRInt32 aNumPoints)
{
  PR_LOG(RenderingContextXlibLM, PR_LOG_DEBUG, ("nsRenderingContextXlib::DrawPolygon()\n"));

  NS_ENSURE_TRUE(mTranMatrix != nsnull, NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(mSurface    != nsnull, NS_ERROR_FAILURE);

  PRInt32 i ;
  XPoint * xpoints;
  XPoint * thispoint;
  
  xpoints = (XPoint *) malloc(sizeof(XPoint) * aNumPoints);
  NS_ENSURE_TRUE(xpoints != nsnull, NS_ERROR_OUT_OF_MEMORY);
  
  for (i = 0; i < aNumPoints; i++){
    thispoint = (xpoints+i);
    thispoint->x = aPoints[i].x;
    thispoint->y = aPoints[i].y;
    mTranMatrix->TransformCoord((PRInt32*)&thispoint->x,(PRInt32*)&thispoint->y);
  }
  
  UpdateGC();
  Drawable drawable; mSurface->GetDrawable(drawable);
  ::XDrawLines(mDisplay,
               drawable,
               *mGC,
               xpoints, aNumPoints, CoordModeOrigin);

  free((void *)xpoints);
  
  return NS_OK;    
}

NS_IMETHODIMP
nsRenderingContextXlib::FillPolygon(const nsPoint aPoints[], PRInt32 aNumPoints)
{
  PR_LOG(RenderingContextXlibLM, PR_LOG_DEBUG, ("nsRenderingContextXlib::FillPolygon()\n"));

  NS_ENSURE_TRUE(mTranMatrix != nsnull, NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(mSurface    != nsnull, NS_ERROR_FAILURE);

  PRInt32 i ;
  XPoint * xpoints;
   
  xpoints = (XPoint *) malloc(sizeof(XPoint) * aNumPoints);
  NS_ENSURE_TRUE(xpoints != nsnull, NS_ERROR_OUT_OF_MEMORY);
  
  for (i = 0; i < aNumPoints; ++i) {
    nsPoint p = aPoints[i];
    mTranMatrix->TransformCoord(&p.x, &p.y);
    xpoints[i].x = p.x;
    xpoints[i].y = p.y;
  } 
    
  UpdateGC();
  Drawable drawable; mSurface->GetDrawable(drawable);
  ::XFillPolygon(mDisplay,
                 drawable,
                 *mGC,
                 xpoints, aNumPoints, Complex, CoordModeOrigin);
               
  free((void *)xpoints);

  return NS_OK; 
}

NS_IMETHODIMP
nsRenderingContextXlib::DrawEllipse(const nsRect& aRect)
{
  PR_LOG(RenderingContextXlibLM, PR_LOG_DEBUG, ("nsRenderingContextXlib::DrawEllipse(const nsRect& aRect)\n"));
  return DrawEllipse(aRect.x, aRect.y, aRect.width, aRect.height);
}

NS_IMETHODIMP
nsRenderingContextXlib::DrawEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  PR_LOG(RenderingContextXlibLM, PR_LOG_DEBUG, ("nsRenderingContextXlib::DrawEllipse(aX, aY, aWidth, aHeight)\n"));

  NS_ENSURE_TRUE(mTranMatrix != nsnull, NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(mSurface    != nsnull, NS_ERROR_FAILURE);

  nscoord x, y, w, h;
   
  x = aX; 
  y = aY;
  w = aWidth; 
  h = aHeight;
    
  mTranMatrix->TransformCoord(&x,&y,&w,&h);
    
  UpdateGC();
  Drawable drawable; mSurface->GetDrawable(drawable);
  ::XDrawArc(mDisplay,
             drawable,
             *mGC,
             x, y, w, h, 0, 360*64);
  
  return NS_OK;  
}

NS_IMETHODIMP
nsRenderingContextXlib::FillEllipse(const nsRect& aRect)
{
  PR_LOG(RenderingContextXlibLM, PR_LOG_DEBUG, ("nsRenderingContextXlib::FillEllipse()\n"));
  return FillEllipse(aRect.x, aRect.y, aRect.width, aRect.height);
}

NS_IMETHODIMP
nsRenderingContextXlib::FillEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  PR_LOG(RenderingContextXlibLM, PR_LOG_DEBUG, ("nsRenderingContextXlib::FillEllipse()\n"));

  NS_ENSURE_TRUE(mTranMatrix != nsnull, NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(mSurface    != nsnull, NS_ERROR_FAILURE);

  nscoord x,y,w,h;
   
  x = aX; 
  y = aY;
  w = aWidth; 
  h = aHeight;
    
  mTranMatrix->TransformCoord(&x,&y,&w,&h);
    
  UpdateGC();
  Drawable drawable; mSurface->GetDrawable(drawable);
  if (w < 16 || h < 16) {
    /* Fix for bug 91816 ("bullets are not displayed correctly on certain text zooms")
     * De-uglify bullets on some X servers:
     * 1st: Draw... */
    ::XDrawArc(mDisplay,
               drawable,
               *mGC,
               x, y, w, h, 0, 360*64);
    /*  ...then fill. */
  }
  ::XFillArc(mDisplay,
             drawable,
             *mGC,
             x, y, w, h, 0, 360*64);
  
  return NS_OK;  
}

NS_IMETHODIMP
nsRenderingContextXlib::DrawArc(const nsRect& aRect,
                                float aStartAngle, float aEndAngle)
{
  PR_LOG(RenderingContextXlibLM, PR_LOG_DEBUG, ("nsRenderingContextXlib::DrawArc()\n"));
  return DrawArc(aRect.x,aRect.y,aRect.width,aRect.height,aStartAngle,aEndAngle); 
}

NS_IMETHODIMP
nsRenderingContextXlib::DrawArc(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
                                float aStartAngle, float aEndAngle)
{
  PR_LOG(RenderingContextXlibLM, PR_LOG_DEBUG, ("nsRenderingContextXlib::DrawArc()\n"));

  NS_ENSURE_TRUE(mTranMatrix != nsnull, NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(mSurface    != nsnull, NS_ERROR_FAILURE);

  nscoord x, y, w, h;
   
  x = aX; 
  y = aY;
  w = aWidth; 
  h = aHeight;
    
  mTranMatrix->TransformCoord(&x,&y,&w,&h);
    
  UpdateGC();
  Drawable drawable; mSurface->GetDrawable(drawable);
  ::XDrawArc(mDisplay,
             drawable,
             *mGC,
             x,y,w,h, NSToIntRound(aStartAngle * 64.0f),
             NSToIntRound(aEndAngle * 64.0f));
  
  return NS_OK;  
}

NS_IMETHODIMP
nsRenderingContextXlib::FillArc(const nsRect& aRect,
                                float aStartAngle, float aEndAngle)
{
  PR_LOG(RenderingContextXlibLM, PR_LOG_DEBUG, ("nsRenderingContextXlib::FillArc()\n"));
  return FillArc(aRect.x, aRect.y, aRect.width, aRect.height, aStartAngle, aEndAngle);
}

NS_IMETHODIMP
nsRenderingContextXlib::FillArc(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
                                float aStartAngle, float aEndAngle)
{
  PR_LOG(RenderingContextXlibLM, PR_LOG_DEBUG, ("nsRenderingContextXlib::FillArc()\n"));

  NS_ENSURE_TRUE(mTranMatrix != nsnull, NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(mSurface    != nsnull, NS_ERROR_FAILURE);

  nscoord x,y,w,h;
   
  x = aX; 
  y = aY;
  w = aWidth; 
  h = aHeight;
    
  mTranMatrix->TransformCoord(&x,&y,&w,&h);
    
  UpdateGC();
  Drawable drawable; mSurface->GetDrawable(drawable);
  ::XFillArc(mDisplay,
             drawable,
             *mGC,
             x,y,w,h, NSToIntRound(aStartAngle * 64.0f),
             NSToIntRound(aEndAngle * 64.0f));
  
  return NS_OK;  
}

// do the 8 to 16 bit conversion on the stack
// if the data is less than this size
#define WIDEN_8_TO_16_BUF_SIZE (1024)

// handle 8 bit data with a 16 bit font
static
int Widen8To16AndMove(const char *char_p, 
                      int char_len, 
                      XChar2b *xchar2b_p)
{
  int i;
  for (i=0; i<char_len; i++) {
    (xchar2b_p)->byte1 = 0;
    (xchar2b_p++)->byte2 = *char_p++;
  }
  return(char_len*2);
}

// handle 8 bit data with a 16 bit font
static
int Widen8To16AndGetWidth(nsXFont    *xFont,
                          const char *text,
                          int         text_length)
{
  NS_ASSERTION(!xFont->IsSingleByte(), "Widen8To16AndGetWidth: wrong string/font size");
  XChar2b  buf[WIDEN_8_TO_16_BUF_SIZE];
  XChar2b *p = buf;
  int uchar_size;
  int rawWidth;

  if (text_length > WIDEN_8_TO_16_BUF_SIZE) {
    p = (XChar2b*)malloc(text_length*sizeof(XChar2b));
    if (!p)
      return(0); // handle malloc failure
  }

  uchar_size = Widen8To16AndMove(text, text_length, p);
  rawWidth = xFont->TextWidth16(p, uchar_size/2);

  if (text_length > WIDEN_8_TO_16_BUF_SIZE) {
    free((char*)p);
  }
  return(rawWidth);
}

static 
void Widen8To16AndDraw(Drawable     drawable,
                       nsXFont     *xFont,
                       GC           gc,
                       int          x,
                       int          y,
                       const char  *text,
                       int          text_length)
{
  NS_ASSERTION(!xFont->IsSingleByte(), "Widen8To16AndDraw: wrong string/font size");
  XChar2b buf[WIDEN_8_TO_16_BUF_SIZE];
  XChar2b *p = buf;
  int uchar_size;

  if (text_length > WIDEN_8_TO_16_BUF_SIZE) {
    p = (XChar2b*)malloc(text_length*sizeof(XChar2b));
    if (!p)
      return; // handle malloc failure
  }

  uchar_size = Widen8To16AndMove(text, text_length, p);
  xFont->DrawText16(drawable, gc, x, y, p, uchar_size/2);

  if (text_length > WIDEN_8_TO_16_BUF_SIZE) {
    free((char*)p);
  }
}

NS_IMETHODIMP
nsRenderingContextXlib::GetWidth(char aC, nscoord &aWidth)
{
  // Check for the very common case of trying to get the width of a single
  // space.
  if ((aC == ' ') && (nsnull != mFontMetrics)) {
    return mFontMetrics->GetSpaceWidth(aWidth);
  }
  return GetWidth(&aC, 1, aWidth);
}

NS_IMETHODIMP
nsRenderingContextXlib::GetWidth(PRUnichar aC, nscoord& aWidth,
                                PRInt32* aFontID)
{
  return GetWidth(&aC, 1, aWidth, aFontID);
}

NS_IMETHODIMP
nsRenderingContextXlib::GetWidth(const nsString& aString,
                                 nscoord& aWidth, PRInt32* aFontID)
{
  return GetWidth(aString.get(), aString.Length(), aWidth, aFontID);
}

NS_IMETHODIMP
nsRenderingContextXlib::GetWidth(const char* aString, nscoord& aWidth)
{
  return GetWidth(aString, strlen(aString), aWidth);
}

NS_IMETHODIMP
nsRenderingContextXlib::GetWidth(const char* aString, PRUint32 aLength,
                                 nscoord& aWidth)
{
  if (0 == aLength) {
    aWidth = 0;
  }
  else {
    NS_ENSURE_TRUE(aString      != nsnull, NS_ERROR_FAILURE);
    NS_ENSURE_TRUE(mCurrentFont != nsnull, NS_ERROR_FAILURE);
    int rawWidth;
    nsXFont *xFont = mCurrentFont->GetXFont();
#ifdef USE_FREETYPE
    if (mCurrentFont->IsFreeTypeFont()) {
      PRUnichar unichars[WIDEN_8_TO_16_BUF_SIZE];
      // need to fix this for long strings
      PRUint32 len = PR_MIN(aLength, WIDEN_8_TO_16_BUF_SIZE);
      // convert 7 bit data to unicode
      // this function is only supposed to be called for ascii data
      for (PRUint32 i=0; i<len; i++) {
        unichars[i] = (PRUnichar)((unsigned char)aString[i]);
      }
      rawWidth = mCurrentFont->GetWidth(unichars, len);
    }
    else
#endif /* USE_FREETYPE */
    if (!mCurrentFont->GetXFontIs10646()) {
      NS_ASSERTION(xFont->IsSingleByte(), "wrong string/font size");
      // 8 bit data with an 8 bit font
      rawWidth = xFont->TextWidth8(aString, aLength);
    }
    else {
      NS_ASSERTION(!xFont->IsSingleByte(), "wrong string/font size");
      // we have 8 bit data but a 16 bit font
      rawWidth = Widen8To16AndGetWidth (mCurrentFont->GetXFont(), aString, aLength);
    }
    aWidth = NSToCoordRound(rawWidth * mP2T);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::GetWidth(const PRUnichar* aString, PRUint32 aLength,
                                 nscoord& aWidth, PRInt32* aFontID)
{
  if (0 == aLength) {
    aWidth = 0;
  }
  else {
    NS_ENSURE_TRUE(aString != nsnull, NS_ERROR_FAILURE);

    nsFontMetricsXlib *metrics = NS_REINTERPRET_CAST(nsFontMetricsXlib *, mFontMetrics.get());

    NS_ENSURE_TRUE(metrics != nsnull, NS_ERROR_FAILURE);

    nsFontXlib *prevFont = nsnull;
    int rawWidth = 0;
    PRUint32 start = 0;
    PRUint32 i;
    for (i = 0; i < aLength; i++) {
      PRUnichar c = aString[i];
      nsFontXlib  *currFont = nsnull;
      nsFontXlib **font = metrics->mLoadedFonts;
      nsFontXlib **end = &metrics->mLoadedFonts[metrics->mLoadedFontsCount];
      while (font < end) {
        if (CCMAP_HAS_CHAR((*font)->mCCMap, c)) {
          currFont = *font;
          goto FoundFont; // for speed -- avoid "if" statement
        }
        font++;
      }
      currFont = metrics->FindFont(c);
FoundFont:
      // XXX avoid this test by duplicating code -- erik
      if (prevFont) {
        if (currFont != prevFont) {
          rawWidth += prevFont->GetWidth(&aString[start], i - start);
          prevFont = currFont;
          start = i;
        }
      }
      else {
        prevFont = currFont;
        start = i;
      }
    }

    if (prevFont) {
      rawWidth += prevFont->GetWidth(&aString[start], i - start);
    }

    aWidth = NSToCoordRound(rawWidth * mP2T);
  }
  if (nsnull != aFontID)
    *aFontID = 0;

  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::GetTextDimensions(const char* aString, PRUint32 aLength,
                                          nsTextDimensions& aDimensions)
{
  mFontMetrics->GetMaxAscent(aDimensions.ascent);
  mFontMetrics->GetMaxDescent(aDimensions.descent);
  return GetWidth(aString, aLength, aDimensions.width);
}

NS_IMETHODIMP
nsRenderingContextXlib::GetTextDimensions(const PRUnichar* aString, PRUint32 aLength,
                                          nsTextDimensions& aDimensions, PRInt32* aFontID)
{
  aDimensions.Clear();
  if (0 < aLength) {
    NS_ENSURE_TRUE(aString != nsnull, NS_ERROR_FAILURE);

    nsFontMetricsXlib *metrics = NS_REINTERPRET_CAST(nsFontMetricsXlib *, mFontMetrics.get());

    NS_ENSURE_TRUE(metrics != nsnull, NS_ERROR_FAILURE);

    nsFontXlib* prevFont = nsnull;
    int rawWidth = 0, rawAscent = 0, rawDescent = 0;
    PRUint32 start = 0;
    PRUint32 i;
    for (i = 0; i < aLength; i++) {
      PRUnichar c = aString[i];
      nsFontXlib  *currFont = nsnull;
      nsFontXlib **font = metrics->mLoadedFonts;
      nsFontXlib **end = &metrics->mLoadedFonts[metrics->mLoadedFontsCount];
      while (font < end) {
        if (CCMAP_HAS_CHAR((*font)->mCCMap, c)) {
          currFont = *font;
          goto FoundFont; // for speed -- avoid "if" statement
        }
        font++;
      }
      currFont = metrics->FindFont(c);
FoundFont:
      // XXX avoid this test by duplicating code -- erik
      if (prevFont) {
        if (currFont != prevFont) {
          rawWidth += prevFont->GetWidth(&aString[start], i - start);
          if (rawAscent < prevFont->mMaxAscent)
            rawAscent = prevFont->mMaxAscent;
          if (rawDescent < prevFont->mMaxDescent)
            rawDescent = prevFont->mMaxDescent;
          prevFont = currFont;
          start = i;
        }
      }
      else {
        prevFont = currFont;
        start = i;
      }
    }

    if (prevFont) {
      rawWidth += prevFont->GetWidth(&aString[start], i - start);
      if (rawAscent < prevFont->mMaxAscent)
        rawAscent = prevFont->mMaxAscent;
      if (rawDescent < prevFont->mMaxDescent)
        rawDescent = prevFont->mMaxDescent;
    }

    aDimensions.width = NSToCoordRound(rawWidth * mP2T);
    aDimensions.ascent = NSToCoordRound(rawAscent * mP2T);
    aDimensions.descent = NSToCoordRound(rawDescent * mP2T);
  }
  if (nsnull != aFontID)
    *aFontID = 0;

  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::DrawString(const char *aString, PRUint32 aLength,
                                   nscoord aX, nscoord aY,
                                   const nscoord* aSpacing)
{
  nsresult res = NS_OK;

  if (aLength) {
    NS_ENSURE_TRUE(mTranMatrix  != nsnull, NS_ERROR_FAILURE);
    NS_ENSURE_TRUE(mSurface     != nsnull, NS_ERROR_FAILURE);
    NS_ENSURE_TRUE(aString      != nsnull, NS_ERROR_FAILURE);
    NS_ENSURE_TRUE(mCurrentFont != nsnull, NS_ERROR_FAILURE);

    nscoord x = aX;
    nscoord y = aY;

    UpdateGC();

    nsXFont *xFont = mCurrentFont->GetXFont();
    if (nsnull != aSpacing) {
      // Render the string, one character at a time...
      const char* end = aString + aLength;
      while (aString < end) {
        char ch = *aString++;
        nscoord xx = x;
        nscoord yy = y;
        mTranMatrix->TransformCoord(&xx, &yy);
#ifdef USE_FREETYPE
        if (mCurrentFont->IsFreeTypeFont()) {
          PRUnichar unichars[WIDEN_8_TO_16_BUF_SIZE];
          // need to fix this for long strings
          PRUint32 len = PR_MIN(aLength, WIDEN_8_TO_16_BUF_SIZE);
          // convert 7 bit data to unicode
          // this function is only supposed to be called for ascii data
          for (PRUint32 i=0; i<len; i++) {
            unichars[i] = (PRUnichar)((unsigned char)aString[i]);
          }
          res = mCurrentFont->DrawString(this, mSurface, xx, yy,
                                         unichars, len);
        }
        else
#endif /* USE_FREETYPE */
        if (!mCurrentFont->GetXFontIs10646()) {
          // 8 bit data with an 8 bit font
          NS_ASSERTION(xFont->IsSingleByte(),"wrong string/font size");
          xFont->DrawText8(mSurface->GetDrawable(), *mGC, xx, yy, &ch, 1);
        }
        else {
          // we have 8 bit data but a 16 bit font
          NS_ASSERTION(!xFont->IsSingleByte(),"wrong string/font size");
          Widen8To16AndDraw(mSurface->GetDrawable(), xFont, *mGC,
                                                xx, yy, &ch, 1);
        }
        x += *aSpacing++;
      }
    }
    else {
      mTranMatrix->TransformCoord(&x, &y);
#ifdef USE_FREETYPE
      if (mCurrentFont->IsFreeTypeFont()) {
        PRUnichar unichars[WIDEN_8_TO_16_BUF_SIZE];
        // need to fix this for long strings
        PRUint32 len = PR_MIN(aLength, WIDEN_8_TO_16_BUF_SIZE);
        // convert 7 bit data to unicode
        // this function is only supposed to be called for ascii data
        for (PRUint32 i=0; i<len; i++) {
          unichars[i] = (PRUnichar)((unsigned char)aString[i]);
        }
        res = mCurrentFont->DrawString(this, mSurface, x, y,
                                       unichars, len);
      }
      else
#endif /* USE_FREETYPE */
      if (!mCurrentFont->GetXFontIs10646()) { // keep 8 bit path fast
        // 8 bit data with an 8 bit font
        NS_ASSERTION(xFont->IsSingleByte(),"wrong string/font size");
        xFont->DrawText8(mSurface->GetDrawable(), *mGC, x, y, aString, aLength);
      }
      else {
        // we have 8 bit data but a 16 bit font
        NS_ASSERTION(!xFont->IsSingleByte(),"wrong string/font size");
        Widen8To16AndDraw(mSurface->GetDrawable(), xFont, *mGC,
                                             x, y, aString, aLength);
      }
    }
  }

#ifdef DISABLED_FOR_NOW
  //this is no longer to be done by this API, but another
  //will take it's place that will need this code again. MMP
  if (mFontMetrics)
  {
    const nsFont *font;
    mFontMetrics->GetFont(font);
    PRUint8 deco = font->decorations;

    if (deco & NS_FONT_DECORATION_OVERLINE)
      DrawLine(aX, aY, aX + aWidth, aY);

    if (deco & NS_FONT_DECORATION_UNDERLINE)
    {
      nscoord ascent,descent;

      mFontMetrics->GetMaxAscent(ascent);
      mFontMetrics->GetMaxDescent(descent);

      DrawLine(aX, aY + ascent + (descent >> 1),
               aX + aWidth, aY + ascent + (descent >> 1));
    }

    if (deco & NS_FONT_DECORATION_LINE_THROUGH)
    {
      nscoord height;

      mFontMetrics->GetHeight(height);

      DrawLine(aX, aY + (height >> 1), aX + aWidth, aY + (height >> 1));
    }
  }
#endif /* DISABLED_FOR_NOW */

  return res;
}

NS_IMETHODIMP
nsRenderingContextXlib::DrawString(const PRUnichar* aString, PRUint32 aLength,
                                   nscoord aX, nscoord aY,
                                   PRInt32 aFontID,
                                   const nscoord* aSpacing)
{
  if (aLength && mFontMetrics) {
    NS_ENSURE_TRUE(mTranMatrix != nsnull, NS_ERROR_FAILURE);
    NS_ENSURE_TRUE(mSurface    != nsnull, NS_ERROR_FAILURE);
    NS_ENSURE_TRUE(aString     != nsnull, NS_ERROR_FAILURE);

    nscoord x = aX;
    nscoord y = aY;

    mTranMatrix->TransformCoord(&x, &y);

    nsFontMetricsXlib *metrics = NS_REINTERPRET_CAST(nsFontMetricsXlib *, mFontMetrics.get());
    nsFontXlib *prevFont = nsnull;
    PRUint32 start = 0;
    PRUint32 i;
    for (i = 0; i < aLength; i++) {
      PRUnichar c = aString[i];
      nsFontXlib* currFont = nsnull;
      nsFontXlib** font = metrics->mLoadedFonts;
      nsFontXlib** lastFont = &metrics->mLoadedFonts[metrics->mLoadedFontsCount];
      while (font < lastFont) {
        if (CCMAP_HAS_CHAR((*font)->mCCMap, c)) {
          currFont = *font;
          goto FoundFont; // for speed -- avoid "if" statement
        }
        font++;
      }
      currFont = metrics->FindFont(c);
FoundFont:
      // XXX avoid this test by duplicating code -- erik
      if (prevFont) {
        if (currFont != prevFont) {
          if (aSpacing) {
            const PRUnichar* str = &aString[start];
            const PRUnichar* end = &aString[i];

            // save off mCurrentFont and set it so that we cache the GC's font correctly
            nsFontXlib *oldFont = mCurrentFont;
            mCurrentFont = prevFont;
            UpdateGC();

            while (str < end) {
              x = aX;
              y = aY;
              mTranMatrix->TransformCoord(&x, &y);
              prevFont->DrawString(this, mSurface, x, y, str, 1);
              aX += *aSpacing++;
              str++;
            }
            mCurrentFont = oldFont;
          }
          else {
            nsFontXlib *oldFont = mCurrentFont;
            mCurrentFont = prevFont;
            UpdateGC();
            x += prevFont->DrawString(this, mSurface, x, y, &aString[start],
                                      i - start);
            mCurrentFont = oldFont;
          }
          prevFont = currFont;
          start = i;
        }
      }
      else {
        prevFont = currFont;
        start = i;
      }
    }

    if (prevFont) {
      nsFontXlib *oldFont = mCurrentFont;
      mCurrentFont = prevFont;
      UpdateGC();
    
      if (aSpacing) {
        const PRUnichar* str = &aString[start];
        const PRUnichar* end = &aString[i];
        while (str < end) {
          x = aX;
          y = aY;
          mTranMatrix->TransformCoord(&x, &y);
          prevFont->DrawString(this, mSurface, x, y, str, 1);
          aX += *aSpacing++;
          str++;
        }
      }
      else {
        prevFont->DrawString(this, mSurface, x, y, &aString[start], i - start);
      }

      mCurrentFont = oldFont;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::DrawString(const nsString& aString,
                                   nscoord aX, nscoord aY,
                                   PRInt32 aFontID,
                                   const nscoord* aSpacing)
{
  return DrawString(aString.get(), aString.Length(),
                    aX, aY, aFontID, aSpacing);
}

NS_IMETHODIMP
nsRenderingContextXlib::DrawImage(nsIImage *aImage,
                                  nscoord aX, nscoord aY)
{
  PR_LOG(RenderingContextXlibLM, PR_LOG_DEBUG, ("nsRenderingContextXlib::DrawImage(aImage, aX, aY)\n"));

  nscoord width, height;

  // we have to do this here because we are doing a transform below
  width = NSToCoordRound(mP2T * aImage->GetWidth());
  height = NSToCoordRound(mP2T * aImage->GetHeight());

  return DrawImage(aImage, aX, aY, width, height);
}

NS_IMETHODIMP
nsRenderingContextXlib::DrawImage(nsIImage *aImage, const nsRect& aRect)
{
  PR_LOG(RenderingContextXlibLM, PR_LOG_DEBUG, ("nsRenderingContextXlib::DrawImage(aImage, aRect)\n"));
  return DrawImage(aImage,
                   aRect.x,
                   aRect.y,
                   aRect.width,
                   aRect.height);
}

NS_IMETHODIMP
nsRenderingContextXlib::DrawImage(nsIImage *aImage,
                                  nscoord aX, nscoord aY,
                                  nscoord aWidth, nscoord aHeight)
{
  PR_LOG(RenderingContextXlibLM, PR_LOG_DEBUG, ("nsRenderingContextXlib::DrawImage()\n"));

  nscoord x, y, w, h;

  x = aX;
  y = aY;
  w = aWidth;
  h = aHeight;

  mTranMatrix->TransformCoord(&x, &y, &w, &h);

  UpdateGC();

  return aImage->Draw(*this, mSurface,
                      x, y, w, h);
}

NS_IMETHODIMP
nsRenderingContextXlib::DrawImage(nsIImage *aImage,
                                  const nsRect& aSRect,
                                  const nsRect& aDRect)
{
  PR_LOG(RenderingContextXlibLM, PR_LOG_DEBUG, ("nsRenderingContextXlib::DrawImage()\n"));

  nsRect sr, dr;

  sr = aSRect;
  mTranMatrix->TransformCoord(&sr.x, &sr.y,
                            &sr.width, &sr.height);
  sr.x -= mTranMatrix->GetXTranslationCoord();
  sr.y -= mTranMatrix->GetYTranslationCoord();

  dr = aDRect;
  mTranMatrix->TransformCoord(&dr.x, &dr.y,
                           &dr.width, &dr.height);

  UpdateGC();

  return aImage->Draw(*this, mSurface,
                      sr.x, sr.y,
                      sr.width, sr.height,
                      dr.x, dr.y,
                      dr.width, dr.height);
}

NS_IMETHODIMP
nsRenderingContextXlib::CopyOffScreenBits(nsDrawingSurface aSrcSurf, PRInt32 aSrcX, PRInt32 aSrcY,
                                          const nsRect &aDestBounds, PRUint32 aCopyFlags)
{
  PR_LOG(RenderingContextXlibLM, PR_LOG_DEBUG, ("nsRenderingContextXlib::CopyOffScreenBits()\n"));

  PRInt32                 srcX = aSrcX;
  PRInt32                 srcY = aSrcY;
  nsRect                  drect = aDestBounds;
  nsIDrawingSurfaceXlib  *destsurf;

  NS_ENSURE_TRUE(mTranMatrix != nsnull, NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(mSurface    != nsnull, NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(aSrcSurf    != nsnull, NS_ERROR_FAILURE);
    
  if (aCopyFlags & NS_COPYBITS_TO_BACK_BUFFER) {
    destsurf = mSurface;
  }
  else
  {
    NS_ENSURE_TRUE(mOffscreenSurface != nsnull, NS_ERROR_FAILURE);
    destsurf = mOffscreenSurface;
  }
  
  if (aCopyFlags & NS_COPYBITS_XFORM_SOURCE_VALUES)
    mTranMatrix->TransformCoord(&srcX, &srcY);
  
  if (aCopyFlags & NS_COPYBITS_XFORM_DEST_VALUES)
    mTranMatrix->TransformCoord(&drect.x, &drect.y, &drect.width, &drect.height);
  
  //XXX flags are unused. that would seem to mean that there is
  //inefficiency somewhere... MMP

  UpdateGC();
  Drawable destdrawable; destsurf->GetDrawable(destdrawable);
  Drawable srcdrawable; ((nsIDrawingSurfaceXlib *)aSrcSurf)->GetDrawable(srcdrawable);

  ::XCopyArea(mDisplay,
              srcdrawable,
              destdrawable,
              *mGC,
              srcX, srcY,
              drect.width, drect.height,
              drect.x, drect.y);

  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextXlib::RetrieveCurrentNativeGraphicData(PRUint32 * ngd)
{
  PR_LOG(RenderingContextXlibLM, PR_LOG_DEBUG, ("nsRenderingContextXlib::RetrieveCurrentNativeGraphicData()\n"));
  return NS_OK;
}

#ifdef MOZ_MATHML
static
void Widen8To16AndGetTextExtents(nsXFont    *xFont,  
                                 const char *text,
                                 int         text_length,
                                 int        *lbearing,
                                 int        *rbearing,
                                 int        *width,
                                 int        *ascent,
                                 int        *descent)
{
  NS_ASSERTION(!xFont->IsSingleByte(),"wrong string/font size");
  XChar2b buf[WIDEN_8_TO_16_BUF_SIZE];
  XChar2b *p = buf;
  int uchar_size;

  if (text_length > WIDEN_8_TO_16_BUF_SIZE) {
    p = (XChar2b*)malloc(text_length*sizeof(XChar2b));
    if (!p) { // handle malloc failure
      *lbearing = 0;
      *rbearing = 0;
      *width    = 0;
      *ascent   = 0;
      *descent  = 0;
      return;
    }
  }

  uchar_size = Widen8To16AndMove(text, text_length, p);
  xFont->TextExtents16(p, uchar_size/2,
                       lbearing, 
                       rbearing, 
                       width, 
                       ascent, 
                       descent);

  if (text_length > WIDEN_8_TO_16_BUF_SIZE) {
    free((char*)p);
  }
}

NS_IMETHODIMP
nsRenderingContextXlib::GetBoundingMetrics(const char*        aString, 
                                           PRUint32           aLength,
                                           nsBoundingMetrics& aBoundingMetrics)
{
  nsresult res = NS_OK;
  aBoundingMetrics.Clear();
  if (aString && 0 < aLength) {
    NS_ENSURE_TRUE(mCurrentFont != nsnull, NS_ERROR_FAILURE);
    nsXFont *xFont = mCurrentFont->GetXFont();
#ifdef USE_FREETYPE
    if (mCurrentFont->IsFreeTypeFont()) {
      PRUnichar unichars[WIDEN_8_TO_16_BUF_SIZE];
      // need to fix this for long strings
      PRUint32 len = PR_MIN(aLength, WIDEN_8_TO_16_BUF_SIZE);
      // convert 7 bit data to unicode
      // this function is only supposed to be called for ascii data
      for (PRUint32 i=0; i<len; i++) {
        unichars[i] = (PRUnichar)((unsigned char)aString[i]);
      }
      res = mCurrentFont->GetBoundingMetrics(unichars, len,
                                            aBoundingMetrics);
    }
    else
#endif /* USE_FREETYPE */
    if (!mCurrentFont->GetXFontIs10646()) {
        // 8 bit data with an 8 bit font
        NS_ASSERTION(xFont->IsSingleByte(), "GetBoundingMetrics: wrong string/font size");
        xFont->TextExtents8(aString, aLength,
                            &aBoundingMetrics.leftBearing, 
                            &aBoundingMetrics.rightBearing, 
                            &aBoundingMetrics.width, 
                            &aBoundingMetrics.ascent, 
                            &aBoundingMetrics.descent);
    }
    else {
        // we have 8 bit data but a 16 bit font
        NS_ASSERTION(!xFont->IsSingleByte(), "GetBoundingMetrics: wrong string/font size");
        Widen8To16AndGetTextExtents(mCurrentFont->GetXFont(), aString, aLength,
                                    &aBoundingMetrics.leftBearing, 
                                    &aBoundingMetrics.rightBearing, 
                                    &aBoundingMetrics.width, 
                                    &aBoundingMetrics.ascent, 
                                    &aBoundingMetrics.descent);
    }

    aBoundingMetrics.leftBearing = NSToCoordRound(aBoundingMetrics.leftBearing * mP2T);
    aBoundingMetrics.rightBearing = NSToCoordRound(aBoundingMetrics.rightBearing * mP2T);
    aBoundingMetrics.width = NSToCoordRound(aBoundingMetrics.width * mP2T);
    aBoundingMetrics.ascent = NSToCoordRound(aBoundingMetrics.ascent * mP2T);
    aBoundingMetrics.descent = NSToCoordRound(aBoundingMetrics.descent * mP2T);
  }

  return res;
}

NS_IMETHODIMP
nsRenderingContextXlib::GetBoundingMetrics(const PRUnichar*   aString, 
                                           PRUint32           aLength,
                                           nsBoundingMetrics& aBoundingMetrics,
                                           PRInt32*           aFontID)
{
  aBoundingMetrics.Clear(); 
  if (0 < aLength) {
    NS_ENSURE_TRUE(aString != nsnull, NS_ERROR_FAILURE);

    nsFontMetricsXlib *metrics  = NS_REINTERPRET_CAST(nsFontMetricsXlib *, mFontMetrics.get());
    nsFontXlib        *prevFont = nsnull;

    nsBoundingMetrics rawbm;
    PRBool firstTime = PR_TRUE;
    PRUint32 start = 0;
    PRUint32 i;
    for (i = 0; i < aLength; i++) {
      PRUnichar c = aString[i];
      nsFontXlib  *currFont = nsnull;
      nsFontXlib **font = metrics->mLoadedFonts;
      nsFontXlib **end = &metrics->mLoadedFonts[metrics->mLoadedFontsCount];
      while (font < end) {
        if (CCMAP_HAS_CHAR((*font)->mCCMap, c)) {
          currFont = *font;
          goto FoundFont; // for speed -- avoid "if" statement
        }
        font++;
      }
      currFont = metrics->FindFont(c);
  FoundFont:
      // XXX avoid this test by duplicating code -- erik
      if (prevFont) {
        if (currFont != prevFont) {
          prevFont->GetBoundingMetrics((const PRUnichar*) &aString[start],
                                       i - start, rawbm);
          if (firstTime) {
            firstTime = PR_FALSE;
            aBoundingMetrics = rawbm;
          } 
          else {
            aBoundingMetrics += rawbm;
          }
          prevFont = currFont;
          start = i;
        }
      }
      else {
        prevFont = currFont;
        start = i;
      }
    }
    
    if (prevFont) {
      prevFont->GetBoundingMetrics((const PRUnichar*) &aString[start],
                                   i - start, rawbm);
      if (firstTime) {
        aBoundingMetrics = rawbm;
      }
      else {
        aBoundingMetrics += rawbm;
      }
    }
    // convert to app units
    aBoundingMetrics.leftBearing = NSToCoordRound(aBoundingMetrics.leftBearing * mP2T);
    aBoundingMetrics.rightBearing = NSToCoordRound(aBoundingMetrics.rightBearing * mP2T);
    aBoundingMetrics.width = NSToCoordRound(aBoundingMetrics.width * mP2T);
    aBoundingMetrics.ascent = NSToCoordRound(aBoundingMetrics.ascent * mP2T);
    aBoundingMetrics.descent = NSToCoordRound(aBoundingMetrics.descent * mP2T);
  }
  if (nsnull != aFontID)
    *aFontID = 0;

  return NS_OK;
}
#endif /* MOZ_MATHML */

NS_IMETHODIMP
nsRenderingContextXlib::DrawImage(imgIContainer *aImage, const nsRect * aSrcRect, const nsPoint * aDestPoint)
{
  PR_LOG(RenderingContextXlibLM, PR_LOG_DEBUG, ("nsRenderingContextXlib::DrawImage()\n"));
  UpdateGC();
  return nsRenderingContextImpl::DrawImage(aImage, aSrcRect, aDestPoint);
}

NS_IMETHODIMP
nsRenderingContextXlib::DrawScaledImage(imgIContainer *aImage, const nsRect * aSrcRect, const nsRect * aDestRect)
{
  PR_LOG(RenderingContextXlibLM, PR_LOG_DEBUG, ("nsRenderingContextXlib::DrawScaledImage()\n"));
  UpdateGC();
  return nsRenderingContextImpl::DrawScaledImage(aImage, aSrcRect, aDestRect);
}

NS_IMETHODIMP
nsRenderingContextXlib::GetBackbuffer(const nsRect &aRequestedSize, const nsRect &aMaxSize, nsDrawingSurface &aBackbuffer)
{
  PR_LOG(RenderingContextXlibLM, PR_LOG_DEBUG, ("nsRenderingContextXlib::GetBackbuffer()\n"));
  /* Do not cache the backbuffer. On X11 it is more efficient to allocate
   * the backbuffer as needed and it doesn't cause a performance hit. @see bug 95952 */
  return AllocateBackbuffer(aRequestedSize, aMaxSize, aBackbuffer, PR_FALSE);
}
 
NS_IMETHODIMP
nsRenderingContextXlib::ReleaseBackbuffer(void) 
{
  PR_LOG(RenderingContextXlibLM, PR_LOG_DEBUG, ("nsRenderingContextXlib::ReleaseBackbuffer()\n"));
  /* Do not cache the backbuffer. On X11 it is more efficient to allocate
   * the backbuffer as needed and it doesn't cause a performance hit. @see bug 95952 */
  return DestroyCachedBackbuffer();
}

