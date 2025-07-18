/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Initial Developer of the Original Code is
 * IBM Corporation.  Portions created by IBM are
 * Copyright (C) 2000 IBM Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Maha Abou El Rous <mahar@eg.ibm.com>
 *   Lina Kemmel <lkemmel@il.ibm.com>
 *   Simon Montagu <smontagu@netscape.com>
 *
 */

#ifndef nsBidiUtils_h__
#define nsBidiUtils_h__

#include "nsCOMPtr.h"
#include "nsString.h"

  /**
   * Perform Arabic shaping on a Unichar string
   * @param aString is the input string
   * @param aLen is the length of aStrong
   * @param aBuf receives the shaped output
   * @param aBuflen receives the length of aBuf
   */
  nsresult ArabicShaping(const PRUnichar* aString, PRUint32 aLen,
                         PRUnichar* aBuf, PRUint32* aBufLen);

  /**
   * Scan an nsString, converting characters in the FExx range (Arabic presentation forms) to the equivalent characters in the 06xx
   * range
   * @param aSrc is the input string
   * @param aDst is the output string
   */
  nsresult Conv_FE_06(const nsString& aSrc, nsString& aDst);

  /**
   * Scan an nsString, converting characters in the FExx range (Arabic presentation forms) to the equivalent characters in the 06xx
   * range, and also reverse the string
   * @param aSrc is the input string
   * @param aDst is the output string
   */
  nsresult Conv_FE_06_WithReverse(const nsString& aSrc, nsString& aDst);

  /**
   * Scan an nsString, converting characters in the 06xx range to the equivalent characters in the 0Fxx range (Arabic presentation
   * forms), with the option to reverse the string
   * @param aSrc is the input string
   * @param aDst is the output string
   * @param aDir indicates whether the string should be reversed
   *        IBMBIDI_TEXTDIRECTION_LTR: do not reverse the string
   *        IBMBIDI_TEXTDIRECTION_RTL: reverse the string
   */
  nsresult Conv_06_FE_WithReverse(const nsString& aSrc, nsString& aDst, PRUint32 aDir);

// --------------------------------------------------
// IBMBIDI 
// --------------------------------------------------
//
// These values are shared with Preferences dialog
//  ------------------
//  If Pref values are to be changed
//  in the XUL file of Prefs. the values
//  Must be changed here too..
//  ------------------
//
#define IBMBIDI_TEXTDIRECTION_STR       "bidi.direction"
#define IBMBIDI_TEXTTYPE_STR            "bidi.texttype"
#define IBMBIDI_CONTROLSTEXTMODE_STR    "bidi.controlstextmode"
#define IBMBIDI_CLIPBOARDTEXTMODE_STR   "bidi.clipboardtextmode"
#define IBMBIDI_NUMERAL_STR             "bidi.numeral"
#define IBMBIDI_SUPPORTMODE_STR         "bidi.support"
#define IBMBIDI_CHARSET_STR             "bidi.characterset"

#define IBMBIDI_TEXTDIRECTION       1
#define IBMBIDI_TEXTTYPE            2
#define IBMBIDI_CONTROLSTEXTMODE    3
#define IBMBIDI_CLIPBOARDTEXTMODE   4
#define IBMBIDI_NUMERAL             5
#define IBMBIDI_SUPPORTMODE         6
#define IBMBIDI_CHARSET             7

//  ------------------
//  Text Direction
//  ------------------
//  bidi.direction
#define IBMBIDI_TEXTDIRECTION_LTR     1 //  1 = directionLTRBidi *
#define IBMBIDI_TEXTDIRECTION_RTL     2 //  2 = directionRTLBidi
//  ------------------
//  Text Type
//  ------------------
//  bidi.texttype
#define IBMBIDI_TEXTTYPE_CHARSET      1 //  1 = charsettexttypeBidi *
#define IBMBIDI_TEXTTYPE_LOGICAL      2 //  2 = logicaltexttypeBidi
#define IBMBIDI_TEXTTYPE_VISUAL       3 //  3 = visualtexttypeBidi
//  ------------------
//  Controls Text Mode
//  ------------------
//  bidi.controlstextmode
#define IBMBIDI_CONTROLSTEXTMODE_LOGICAL   1 //  1 = logicalcontrolstextmodeBidiCmd *
#define IBMBIDI_CONTROLSTEXTMODE_VISUAL    2 //  2 = visualcontrolstextmodeBidi
#define IBMBIDI_CONTROLSTEXTMODE_CONTAINER 3 //  3 = containercontrolstextmodeBidi
//  ------------------
//  Clipboard Text Mode
//  ------------------
//  bidi.clipboardtextmode
#define IBMBIDI_CLIPBOARDTEXTMODE_LOGICAL 1 //  1 = logicalclipboardtextmodeBidi
#define IBMBIDI_CLIPBOARDTEXTMODE_VISUAL  2 //  2 = visualclipboardtextmodeBidi
#define IBMBIDI_CLIPBOARDTEXTMODE_SOURCE  3 //  3 = sourceclipboardtextmodeBidi *
//  ------------------
//  Numeral Style
//  ------------------
//  bidi.numeral
#define IBMBIDI_NUMERAL_REGULAR       1 //  1 = regularcontextnumeralBidi *
#define IBMBIDI_NUMERAL_HINDICONTEXT  2 //  2 = hindicontextnumeralBidi
#define IBMBIDI_NUMERAL_ARABIC        3 //  3 = arabicnumeralBidi
#define IBMBIDI_NUMERAL_HINDI         4 //  4 = hindinumeralBidi
//  ------------------
//  Support Mode
//  ------------------
//  bidi.support
#define IBMBIDI_SUPPORTMODE_MOZILLA     1 //  1 = mozillaBidisupport *
#define IBMBIDI_SUPPORTMODE_OSBIDI      2 //  2 = OsBidisupport
#define IBMBIDI_SUPPORTMODE_DISABLE     3 //  3 = disableBidisupport
//  ------------------
//  Charset Mode
//  ------------------
//  bidi.characterset
#define IBMBIDI_CHARSET_BIDI        1 //  1 = doccharactersetBidi *
#define IBMBIDI_CHARSET_DEFAULT     2 //  2 = defaultcharactersetBidi

#define IBMBIDI_DEFAULT_BIDI_OPTIONS              \
        ((IBMBIDI_TEXTDIRECTION_LTR<<0)         | \
         (IBMBIDI_TEXTTYPE_CHARSET<<4)          | \
         (IBMBIDI_CONTROLSTEXTMODE_LOGICAL<<8)  | \
         (IBMBIDI_CLIPBOARDTEXTMODE_SOURCE<<12) | \
         (IBMBIDI_NUMERAL_REGULAR<<16)          | \
         (IBMBIDI_SUPPORTMODE_MOZILLA<<20)      | \
         (IBMBIDI_CHARSET_BIDI<<24))


#define GET_BIDI_OPTION_DIRECTION(bo) (((bo)>>0) & 0x0000000F) /* 4 bits for DIRECTION */
#define GET_BIDI_OPTION_TEXTTYPE(bo) (((bo)>>4) & 0x0000000F) /* 4 bits for TEXTTYPE */
#define GET_BIDI_OPTION_CONTROLSTEXTMODE(bo) (((bo)>>8) & 0x0000000F) /* 4 bits for CONTROLTEXTMODE */
#define GET_BIDI_OPTION_CLIPBOARDTEXTMODE(bo) (((bo)>>12) & 0x0000000F) /* 4 bits for CLIPBOARDTEXTMODE */
#define GET_BIDI_OPTION_NUMERAL(bo) (((bo)>>16) & 0x0000000F) /* 4 bits for NUMERAL */
#define GET_BIDI_OPTION_SUPPORT(bo) (((bo)>>20) & 0x0000000F) /* 4 bits for SUPPORT */
#define GET_BIDI_OPTION_CHARACTERSET(bo) (((bo)>>24) & 0x0000000F) /* 4 bits for CHARACTERSET */

#define SET_BIDI_OPTION_DIRECTION(bo, dir) {(bo)=((bo) & 0xFFFFFFF0)|(((dir)& 0x0000000F)<<0);}
#define SET_BIDI_OPTION_TEXTTYPE(bo, tt) {(bo)=((bo) & 0xFFFFFF0F)|(((tt)& 0x0000000F)<<4);}
#define SET_BIDI_OPTION_CONTROLSTEXTMODE(bo, cotm) {(bo)=((bo) & 0xFFFFF0FF)|(((cotm)& 0x0000000F)<<8);}
#define SET_BIDI_OPTION_CLIPBOARDTEXTMODE(bo, cltm) {(bo)=((bo) & 0xFFFF0FFF)|(((cltm)& 0x0000000F)<<12);}
#define SET_BIDI_OPTION_NUMERAL(bo, num) {(bo)=((bo) & 0xFFF0FFFF)|(((num)& 0x0000000F)<<16);}
#define SET_BIDI_OPTION_SUPPORT(bo, sup) {(bo)=((bo) & 0xFF0FFFFF)|(((sup)& 0x0000000F)<<20);}
#define SET_BIDI_OPTION_CHARACTERSET(bo, cs) {(bo)=((bo) & 0xF0FFFFFF)|(((cs)& 0x0000000F)<<24);}

/* Constants related to the position of numerics in the codepage */
#define START_HINDI_DIGITS              0x0660
#define END_HINDI_DIGITS                0x0669
#define START_ARABIC_DIGITS             0x0030
#define END_ARABIC_DIGITS               0x0039
#define START_FARSI_DIGITS              0x06f0
#define END_FARSI_DIGITS                0x06f9
#define IS_HINDI_DIGIT(u)   ( ( (u) >= START_HINDI_DIGITS )  && ( (u) <= END_HINDI_DIGITS ) )
#define IS_ARABIC_DIGIT(u)  ( ( (u) >= START_ARABIC_DIGITS ) && ( (u) <= END_ARABIC_DIGITS ) )
#define IS_FARSI_DIGIT(u)  ( ( (u) >= START_FARSI_DIGITS ) && ( (u) <= END_FARSI_DIGITS ) )
#define IS_ARABIC_SEPARATOR(u) ( ( (u) == 0x066A ) || ( (u) == 0x066B ) || ( (u) == 0x066C ) )

#define IS_BIDI_DIACRITIC(u) ( \
  ( (u) >= 0x0591 && (u) <= 0x05A1) || ( (u) >= 0x05A3 && (u) <= 0x05B9) \
    || ( (u) >= 0x05BB && (u) <= 0x05BD) || ( (u) == 0x05BF) || ( (u) == 0x05C1) \
    || ( (u) == 0x05C2) || ( (u) == 0x05C4) \
    || ( (u) >= 0x064B && (u) <= 0x0652) || ( (u) == 0x0670) \
    || ( (u) >= 0x06D7 && (u) <= 0x06E4) || ( (u) == 0x06E7) || ( (u) == 0x06E8) \
    || ( (u) >= 0x06EA && (u) <= 0x06ED) )

#define IS_HEBREW_CHAR(c) (((0x0590 <= (c)) && ((c)<= 0x05FF)) || (((c) >= 0xfb1d) && ((c) <= 0xfb4f)))
#define IS_06_CHAR(c) ((0x0600 <= (c)) && ((c)<= 0x06FF))
#define IS_FE_CHAR(c) (((0xfb50 <= (c)) && ((c)<= 0xfbFF)) \
                       || ((0xfe70 <= (c)) && ((c)<= 0xfeFC)))
#define IS_ARABIC_CHAR(c) ((0x0600 <= (c)) && ((c)<= 0x06FF))
#define IS_ARABIC_ALPHABETIC(c) (IS_ARABIC_CHAR(c) && \
                                !(IS_HINDI_DIGIT(c) || IS_FARSI_DIGIT(c) || IS_ARABIC_SEPARATOR(c)))

#define CHAR_IS_BIDI(c) ( (IS_HINDI_DIGIT(c) ) || (IS_HEBREW_CHAR(c) ) \
                        || (IS_06_CHAR(c) ) || (IS_FE_CHAR(c) ) )
#endif  /* nsBidiUtils_h__ */
