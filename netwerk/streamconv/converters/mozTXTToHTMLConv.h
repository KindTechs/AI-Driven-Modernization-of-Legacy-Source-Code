/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * 
 * The "License" shall be the Mozilla Public License Version 1.1, except
 * Sections 6.2 and 11, but with the addition of the below defined Section 14.
 * You may obtain a copy of the Mozilla Public License Version 1.1 at
 * <http://www.mozilla.org/MPL/>. The contents of this file are subject to the
 * License; you may not use this file except in compliance with the License.
 * 
 * Section 14: MISCELLANEOUS.
 * This License represents the complete agreement concerning subject matter
 * hereof. If any provision of this License is held to be unenforceable, such
 * provision shall be reformed only to the extent necessary to make it
 * enforceable. This License shall be governed by German law provisions. Any
 * litigation relating to this License shall be subject to German jurisdiction.
 * 
 * Once Covered Code has been published under a particular version of the
 * License, You may always continue to use it under the terms of that version.
 + The Initial Developer and no one else has the right to modify the terms
 * applicable to Covered Code created under this License.
 * (End of Section 14)
 * 
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * The Original Code is the Mozilla Text to HTML converter code.
 * 
 * The Initial Developer of the Original Code is Ben Bucksch
 * <http://www.bucksch.org>. Portions created by Ben Bucksch are Copyright
 * (C) 1999, 2000 Ben Bucksch. All Rights Reserved.
 * 
 * Contributor(s):
 */

/**
  Description: Currently only functions to enhance plain text with HTML tags. See mozITXTToHTMLConv. Stream conversion is defunct.
*/

#ifndef _mozTXTToHTMLConv_h__
#define _mozTXTToHTMLConv_h__

#include "mozITXTToHTMLConv.h"
#include "nsIIOService.h"
#include "nsString.h"
#include "nsTimer.h"
#include "nsCOMPtr.h"

static NS_DEFINE_CID(kTXTToHTMLConvCID, MOZITXTTOHTMLCONV_CID);

class mozTXTToHTMLConv : public mozITXTToHTMLConv
{


//////////////////////////////////////////////////////////
public:
//////////////////////////////////////////////////////////

  mozTXTToHTMLConv();
  virtual ~mozTXTToHTMLConv();
  NS_DECL_ISUPPORTS
  static const nsIID& GetIID(void)
       { static nsIID iid = MOZITXTTOHTMLCONV_IID; return iid; }

  NS_DECL_MOZITXTTOHTMLCONV
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSISTREAMCONVERTER

/**
  see mozITXTToHTMLConv::ScanTXT
 */
  void ScanTXT(const PRUnichar * aInString, PRInt32 aInStringLength, PRUint32 whattodo, nsString& aOutString);

/**
  see mozITXTToHTMLConv::ScanHTML. We will modify aInString potentially...
 */
  void ScanHTML(nsString& aInString, PRUint32 whattodo, nsString &aOutString);

/**
  see mozITXTToHTMLConv::CiteLevelTXT
 */
  PRInt32 CiteLevelTXT(const PRUnichar * line,PRUint32& logLineStart);


  // Timing!
  MOZ_TIMER_DECLARE(mScanTXTTimer)
  MOZ_TIMER_DECLARE(mGlyphHitTimer)
  MOZ_TIMER_DECLARE(mTotalMimeTime)


//////////////////////////////////////////////////////////
protected:
//////////////////////////////////////////////////////////
  nsCOMPtr<nsIIOService> mIOService; // for performance reasons, cache the netwerk service...
/**
  Completes<ul>
  <li>Case 1: mailto: "mozilla@bucksch.org" -> "mailto:mozilla@bucksch.org"
  <li>Case 2: http:   "www.mozilla.org"     -> "http://www.mozilla.org"
  <li>Case 3: ftp:    "ftp.mozilla.org"     -> "ftp://www.mozilla.org"
  </ul>
  It does no check, if the resulting URL is valid.
  @param text (in): abbreviated URL
  @param pos (in): position of "@" (case 1) or first "." (case 2 and 3)
  @return Completed URL at success and empty string at failure
 */
  void CompleteAbbreviatedURL(const PRUnichar * aInString, PRInt32 aInLength, 
                              const PRUint32 pos, nsString& aOutString);


//////////////////////////////////////////////////////////
private:
//////////////////////////////////////////////////////////

  enum LIMTYPE
  {
    LT_IGNORE,     // limitation not checked
    LT_DELIMITER,  // not alphanumeric and not rep[0]. End of text is also ok.
    LT_ALPHA,      // alpha char
    LT_DIGIT
  };

/**
  @param text (in): the string to search through.<p>
         If before = IGNORE,<br>
           rep is compared starting at 1. char of text (text[0]),<br>
           else starting at 2. char of text (text[1]).
         Chars after "after"-delimiter are ignored.
  @param rep (in): the string to look for
  @param aRepLen (in): the number of bytes in the string to look for
  @param before (in): limitation before rep
  @param after (in): limitation after rep
  @return true, if rep is found and limitation spec is met or rep is empty
*/
  PRBool ItMatchesDelimited(const PRUnichar * aInString, PRInt32 aInLength,
      const PRUnichar * rep, PRInt32 aRepLen, LIMTYPE before, LIMTYPE after);

/**
  @param see ItMatchesDelimited
  @return Number of ItMatchesDelimited in text
*/
  PRUint32 NumberOfMatches(const PRUnichar * aInString, PRInt32 aInStringLength,
      const PRUnichar* rep, PRInt32 aRepLen, LIMTYPE before, LIMTYPE after);

/**
  Currently only changes "<", ">" and "&". All others stay as they are.<p>
  "Char" in function name to avoid side effects with nsString(ch)
  constructors.
  @param ch (in) 
  @param aStringToAppendto (out) - the string to append the escaped
                                   string to.
*/
  void EscapeChar(const PRUnichar ch, nsString& aStringToAppendto);

/**
  See EscapeChar. Escapes the string in place.
*/
  void EscapeStr(nsString& aInString);

/**
  Currently only reverts "<", ">" and "&". All others stay as they are.<p>
  @param aInString (in) HTML string
  @param aStartPos (in) start index into the buffer
  @param aLength (in) length of the buffer
  @param aOutString (out) unescaped buffer
*/
  void UnescapeStr(const PRUnichar * aInString, PRInt32 aStartPos, PRInt32 aLength, nsString& aOutString);

/**
  <em>Note</em>: I use different strategies to pass context between the
  functions (full text and pos vs. cutted text and col0, glphyTextLen vs.
  replaceBefore/-After). It makes some sense, but is hard to understand
  (maintain) :-(.
*/

/**
  <p><em>Note:</em> replaceBefore + replaceAfter + 1 (for char at pos) chars
  in text should be replaced by outputHTML.</p>
  <p><em>Note:</em> This function should be able to process a URL on multiple
  lines, but currently, ScanForURLs is called for every line, so it can't.</p>
  @param text (in): includes possibly a URL
  @param pos (in): position in text, where either ":", "." or "@" are found
  @param whathasbeendone (in): What the calling ScanTXT did/has to do with the
              (not-linkified) text, i.e. usually the "whattodo" parameter.
              (Needed to calculate replaceBefore.) NOT what will be done with
              the content of the link.
  @param outputHTML (out): URL with HTML-a tag
  @param replaceBefore (out): Number of chars of URL before pos
  @param replaceAfter (out): Number of chars of URL after pos
  @return URL found
*/
  PRBool FindURL(const PRUnichar * aInString, PRInt32 aInLength, const PRUint32 pos,
          const PRUint32 whathasbeendone,
          nsString& outputHTML, PRInt32& replaceBefore, PRInt32& replaceAfter);

  enum modetype {
         unknown,
         RFC1738,          /* Check, if RFC1738, APPENDIX compliant,
                              like "<URL:http://www.mozilla.org>". */
         RFC2396E,         /* RFC2396, APPENDIX E allows anglebrackets (like
                              "<http://www.mozilla.org>") (without "URL:") or
                              quotation marks(like ""http://www.mozilla.org"").
                              Also allow email addresses without scheme,
                              e.g. "<mozilla@bucksch.org>" */
         freetext,         /* assume heading scheme
                              with "[a-zA-Z][a-zA-Z0-9+\-\.]*:" like "news:"
                              (see RFC2396, Section 3.1).
                              Certain characters (see code) or any whitespace
                              (including linebreaks) end the URL.
                              Other certain (punctation) characters (see code)
                              at the end are stripped off. */
         abbreviated       /* Similar to freetext, but without scheme, e.g.
	                      "www.mozilla.org", "ftp.mozilla.org" and
                              "mozilla@bucksch.org". */
      /* RFC1738 and RFC2396E type URLs may use multiple lines,
         whitespace is stripped. Special characters like ")" stay intact.*/
  };

/**
 * @param text (in), pos (in): see FindURL
 * @param check (in): Start must be conform with this mode
 * @param start (out): Position in text, where URL (including brackets or
 *             similar) starts
 * @return |check|-conform start has been found
 */
  PRBool FindURLStart(const PRUnichar * aInString, PRInt32 aInLength, const PRUint32 pos,
            	               const modetype check, PRUint32& start);

/**
 * @param text (in), pos (in): see FindURL
 * @param check (in): End must be conform with this mode
 * @param start (in): see FindURLStart
 * @param end (out): Similar to |start| param of FindURLStart
 * @return |check|-conform end has been found
 */
  PRBool FindURLEnd(const PRUnichar * aInString, PRInt32 aInStringLength, const PRUint32 pos,
           const modetype check, const PRUint32 start, PRUint32& end);

/**
 * @param text (in), pos (in), whathasbeendone (in): see FindURL
 * @param check (in): Current mode
 * @param start (in), end (in): see FindURLEnd
 * @param txtURL (out): Guessed (raw) URL.
 *             Without whitespace, but not completed.
 * @param desc (out): Link as shown to the user, but already escaped.
 *             Should be placed between the <a> and </a> tags.
 * @param replaceBefore(out), replaceAfter (out): see FindURL
 */
  void CalculateURLBoundaries(const PRUnichar * aInString, PRInt32 aInStringLength, 
     const PRUint32 pos, const PRUint32 whathasbeendone,
     const modetype check, const PRUint32 start, const PRUint32 end,
     nsString& txtURL, nsString& desc,
     PRInt32& replaceBefore, PRInt32& replaceAfter);

/**
 * @param txtURL (in), desc (in): see CalculateURLBoundaries
 * @param outputHTML (out): see FindURL
 * @return A valid URL could be found (and creation of HTML successful)
 */
  PRBool CheckURLAndCreateHTML(
       const nsString& txtURL, const nsString& desc, const modetype mode,
       nsString& outputHTML);

/**
  @param text (in): line of text possibly with tagTXT.<p>
              if col0 is true,
                starting with tagTXT<br>
              else
                starting one char before tagTXT
  @param col0 (in): tagTXT is on the beginning of the line (or paragraph).
              open must be 0 then.
  @param tagTXT (in): Tag in plaintext to search for, e.g. "*"
  @param aTagTxtLen (in): length of tagTXT.
  @param tagHTML (in): HTML-Tag to replace tagTXT with,
              without "<" and ">", e.g. "strong"
  @param attributeHTML (in): HTML-attribute to add to opening tagHTML,
              e.g. "class=txt_star"
  @param aOutString: string to APPEND the converted html into
  @param open (in/out): Number of currently open tags of type tagHTML
  @return Conversion succeeded
*/
  PRBool StructPhraseHit(const PRUnichar * aInString, PRInt32 aInStringLength, PRBool col0,
     const PRUnichar* tagTXT,
     PRInt32 aTagTxtLen, 
     const char* tagHTML, const char* attributeHTML,
     nsString& aOutputString, PRUint32& openTags);

/**
  @param text (in), col0 (in): see GlyphHit
  @param tagTXT (in): Smily, see also StructPhraseHit
  @param aTagTxtLen (in): length of tagTXT
  @param tagHTML (in): see StructPhraseHit
  @param outputHTML (out): new string containing the html for the smily
  @param glyphTextLen (out): see GlyphHit
*/
  PRBool SmilyHit(const PRUnichar * aInString, PRInt32 aLength, PRBool col0,
         const PRUnichar* tagTXT, PRInt32 aTagTxtLen, const char* tagHTML,
         nsString& outputHTML, PRInt32& glyphTextLen);

/**
  Checks, if we can replace some chars at the start of line with prettier HTML
  code.<p>
  If success is reported, replace the first glyphTextLen chars with outputHTML

  @param text (in): line of text possibly with Glyph.<p>
              If col0 is true,
                starting with Glyph <br><!-- (br not part of text) -->
              else
                starting one char before Glyph
  @param col0 (in): text starts at the beginning of the line (or paragraph)
  @param aOutString (out): APPENDS html for the glyph to this string
  @param glyphTextLen (out): Length of original text to replace
  @return see StructPhraseHit
*/
  PRBool GlyphHit(const PRUnichar * aInString, PRInt32 aInLength, PRBool col0,
       nsString& aOutString, PRInt32& glyphTextLen);

};

// It's said, that Win32 and Mac don't like static const members
const PRInt32 mozTXTToHTMLConv_lastMode = 4;
	                        // Needed (only) by mozTXTToHTMLConv::FindURL
const PRInt32 mozTXTToHTMLConv_numberOfModes = 4;  // dito; unknown not counted

#endif
