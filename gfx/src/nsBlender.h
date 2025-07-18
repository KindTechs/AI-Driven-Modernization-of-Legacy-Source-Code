 /* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 */

#ifndef nsBlender_h___
#define nsBlender_h___

#include "nsIBlender.h"

typedef enum
{
  nsLowQual = 0,
  nsLowMedQual,
  nsMedQual,
  nsHighMedQual,
  nsHighQual
} nsBlendQuality;

class IL_ColorSpace;

//----------------------------------------------------------------------

// Blender interface
class NS_GFX nsBlender : public nsIBlender
{
public:
 /** --------------------------------------------------------------------------
  * General constructor for a nsBlender object
  */
  nsBlender();

  NS_DECL_ISUPPORTS
  
  NS_IMETHOD Init(nsIDeviceContext *aContext);
  NS_IMETHOD Blend(PRInt32 aSX, PRInt32 aSY, PRInt32 aWidth, PRInt32 aHeight,nsDrawingSurface aSrc,
                   nsDrawingSurface aDest, PRInt32 aDX, PRInt32 aDY, float aSrcOpacity,
                   nsDrawingSurface aSecondSrc = nsnull, nscolor aSrcBackColor = NS_RGB(0, 0, 0),
                   nscolor aSecondSrcBackColor = NS_RGB(0, 0, 0));
  NS_IMETHOD Blend(PRInt32 aSX, PRInt32 aSY, PRInt32 aWidth, PRInt32 aHeight, nsIRenderingContext *aSrc,
                   nsIRenderingContext *aDest, PRInt32 aDX, PRInt32 aDY, float aSrcOpacity,
                   nsIRenderingContext *aSecondSrc = nsnull, nscolor aSrcBackColor = NS_RGB(0, 0, 0),
                   nscolor aSecondSrcBackColor = NS_RGB(0, 0, 0));

protected:

 /** --------------------------------------------------------------------------
  * Destructor for a nsBlender object
  */
  virtual ~nsBlender();

  //called by nsIBlender Blend() functions
  nsresult Blend(PRUint8 *aSrcBits, PRInt32 aSrcStride,
                 PRUint8 *aDestBits, PRInt32 aDestStride,
                 PRUint8 *aSecondSrcBits,
                 PRInt32 aSrcBytes, PRInt32 aLines, float aOpacity);

  /** --------------------------------------------------------------------------
   * Blend two 32 bit image arrays
   * @param aNumlines  Number of lines to blend
   * @param aNumberBytes Number of bytes per line to blend
   * @param aSImage Pointer to beginning of the source bytes
   * @param aDImage Pointer to beginning of the destination bytes
   * @param aMImage Pointer to beginning of the mask bytes
   * @param aSLSpan number of bytes per line for the source bytes
   * @param aDLSpan number of bytes per line for the destination bytes
   * @param aMLSpan number of bytes per line for the Mask bytes
   * @param aBlendQuality The quality of this blend, this is for tweening if neccesary
   */
  void Do32Blend(float aOpacity, PRInt32 aNumLines, PRInt32 aNumBytes,
                 PRUint8 *aSImage, PRUint8 *aDImage, PRUint8 *aSecondSImage,
                 PRInt32 aSLSpan, PRInt32 aDLSpan, nsBlendQuality aTheQual);

 /** --------------------------------------------------------------------------
  * Blend two 24 bit image arrays using a passed in blend value
  * @param aNumlines  Number of lines to blend
  * @param aNumberBytes Number of bytes per line to blend
  * @param aSImage Pointer to beginning of the source bytes
  * @param aDImage Pointer to beginning of the destination bytes
  * @param aMImage Pointer to beginning of the mask bytes
  * @param aSLSpan number of bytes per line for the source bytes
  * @param aDLSpan number of bytes per line for the destination bytes
  * @param aMLSpan number of bytes per line for the Mask bytes
  * @param aBlendQuality The quality of this blend, this is for tweening if neccesary
  */
  void Do24Blend(float aOpacity, PRInt32 aNumLines, PRInt32 aNumBytes,
                 PRUint8 *aSImage, PRUint8 *aDImage, PRUint8 *aSecondSImage,
                 PRInt32 aSLSpan, PRInt32 aDLSpan, nsBlendQuality aBlendQuality);

 /** --------------------------------------------------------------------------
  * Blend two 16 bit image arrays using a passed in blend value
  * @param aNumlines  Number of lines to blend
  * @param aNumberBytes Number of bytes per line to blend
  * @param aSImage Pointer to beginning of the source bytes
  * @param aDImage Pointer to beginning of the destination bytes
  * @param aMImage Pointer to beginning of the mask bytes
  * @param aSLSpan number of bytes per line for the source bytes
  * @param aDLSpan number of bytes per line for the destination bytes
  * @param aMLSpan number of bytes per line for the Mask bytes
  * @param aBlendQuality The quality of this blend, this is for tweening if neccesary
  */
  void Do16Blend(float aOpacity, PRInt32 aNumLines, PRInt32 aNumBytes,
                 PRUint8 *aSImage, PRUint8 *aDImage, PRUint8 *aSecondSImage,
                 PRInt32 aSLSpan, PRInt32 aDLSpan, nsBlendQuality aBlendQuality);


 /** --------------------------------------------------------------------------
  * Blend two 8 bit image arrays using a passed in blend value
  * @param aNumlines  Number of lines to blend
  * @param aNumberBytes Number of bytes per line to blend
  * @param aSImage Pointer to beginning of the source bytes
  * @param aDImage Pointer to beginning of the destination bytes
  * @param aMImage Pointer to beginning of the mask bytes
  * @param aSLSpan number of bytes per line for the source bytes
  * @param aDLSpan number of bytes per line for the destination bytes
  * @param aMLSpan number of bytes per line for the Mask bytes
  * @param aBlendQuality The quality of this blend, this is for tweening if neccesary
  */
  void Do8Blend(float aOpacity, PRInt32 aNumLines, PRInt32 aNumBytes,
                PRUint8 *aSImage, PRUint8 *aDImage, PRUint8 *aSecondSImage,
                PRInt32 aSLSpan, PRInt32 aDLSpan, IL_ColorSpace *aColorMap, nsBlendQuality aBlendQuality);

  nsIDeviceContext  *mContext;
};

#endif

