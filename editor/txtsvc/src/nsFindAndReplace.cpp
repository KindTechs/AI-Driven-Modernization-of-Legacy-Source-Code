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

#include "nsFindAndReplace.h"
#include "nsString.h"
#include "nsUnicharUtils.h"
#include "nsCRT.h"

NS_IMPL_ISUPPORTS1(nsFindAndReplace, nsIFindAndReplace)

nsFindAndReplace::nsFindAndReplace()
  : mFindBackwards(PR_FALSE)
  , mCaseSensitive(PR_FALSE)
  , mWrapFind(PR_FALSE)
  , mEntireWord(PR_FALSE)
  , mStartBlockIndex(0)
  , mStartSelOffset(0)
  , mCurrentBlockIndex(0)
  , mCurrentSelOffset(0)
  , mWrappedOnce(PR_FALSE)
{
  NS_INIT_ISUPPORTS();
}

nsFindAndReplace::~nsFindAndReplace()
{
  /* destructor code */
}

/* [noscript] attribute nsITextServicesDocument tsDoc; */
NS_IMETHODIMP
nsFindAndReplace::GetTsDoc(nsITextServicesDocument * *aTsDoc)
{
  if (!aTsDoc)
    return NS_ERROR_NULL_POINTER;

  *aTsDoc = mTsDoc;
  NS_IF_ADDREF((*aTsDoc));

  return NS_OK;
}
NS_IMETHODIMP
nsFindAndReplace::SetTsDoc(nsITextServicesDocument * aTsDoc)
{
  mTsDoc = aTsDoc;

  return NS_OK;
}

/* attribute boolean findBackwards; */
NS_IMETHODIMP
nsFindAndReplace::GetFindBackwards(PRBool *aFindBackwards)
{
  if (!aFindBackwards)
    return NS_ERROR_NULL_POINTER;

  *aFindBackwards = mFindBackwards;

  return NS_OK;
}
NS_IMETHODIMP
nsFindAndReplace::SetFindBackwards(PRBool aFindBackwards)
{
  mFindBackwards = aFindBackwards;

  return NS_OK;
}

/* attribute boolean caseSensitive; */
NS_IMETHODIMP
nsFindAndReplace::GetCaseSensitive(PRBool *aCaseSensitive)
{
  if (!aCaseSensitive)
    return NS_ERROR_NULL_POINTER;

  *aCaseSensitive = mCaseSensitive;

  return NS_OK;
}
NS_IMETHODIMP
nsFindAndReplace::SetCaseSensitive(PRBool aCaseSensitive)
{
  mCaseSensitive = aCaseSensitive;

  return NS_OK;
}

/* attribute boolean wrapFind; */
NS_IMETHODIMP
nsFindAndReplace::GetWrapFind(PRBool *aWrapFind)
{
  if (!aWrapFind)
    return NS_ERROR_NULL_POINTER;

  *aWrapFind = mWrapFind;

  return NS_OK;
}
NS_IMETHODIMP
nsFindAndReplace::SetWrapFind(PRBool aWrapFind)
{
  mWrapFind = aWrapFind;

  return NS_OK;
}

/* attribute boolean entireWord; */
NS_IMETHODIMP
nsFindAndReplace::GetEntireWord(PRBool *aEntireWord)
{
  if (!aEntireWord)
    return NS_ERROR_NULL_POINTER;

  *aEntireWord = mEntireWord;

  return NS_OK;
}
NS_IMETHODIMP
nsFindAndReplace::SetEntireWord(PRBool aEntireWord)
{
  mEntireWord = aEntireWord;

  return NS_OK;
}

/* readonly attribute boolean replaceEnabled; */
NS_IMETHODIMP
nsFindAndReplace::GetReplaceEnabled(PRBool *aReplaceEnabled)
{
  if (!aReplaceEnabled)
    return NS_ERROR_NULL_POINTER;

  *aReplaceEnabled = PR_FALSE;

  nsresult result = NS_OK;

  if (mTsDoc)
    result = mTsDoc->CanEdit(aReplaceEnabled);
  
  return result;
}

/* boolean Find (in wstring aFindText); */
NS_IMETHODIMP nsFindAndReplace::Find(const PRUnichar *aFindText, PRBool *aDidFind)
{
  if (!aFindText || !aDidFind)
    return NS_ERROR_NULL_POINTER;

  if (!mTsDoc)
    return NS_ERROR_NOT_INITIALIZED;

  nsAutoString findStr(aFindText);
  if (!mCaseSensitive)
    ToLowerCase(findStr);

  nsresult result = SetupDocForFind(mTsDoc, &mStartSelOffset);
  if (NS_FAILED(result))
    return result;  
  
  // find out where we started
  if (mWrapFind)
  {
    result = GetCurrentBlockIndex(mTsDoc, &mStartBlockIndex);
    if (NS_FAILED(result))
      return result;

    // and set the starting position again (hopefully, in future we won't have to do this)
    result = SetupDocForFind(mTsDoc, &mStartSelOffset);
    if (NS_FAILED(result))
      return result;  
  }

  mCurrentBlockIndex = mStartBlockIndex;
  mCurrentSelOffset  = mStartSelOffset;
  mWrappedOnce = PR_FALSE;

  return DoFind(mTsDoc, findStr, aDidFind);
}

/* boolean Replace (in wstring aFindText, in wstring aReplaceText, in boolean aAllOccurrences); */
NS_IMETHODIMP
nsFindAndReplace::Replace(const PRUnichar *aFindText, const PRUnichar *aReplaceText, PRBool aAllOccurrences, PRBool *aDidFind)
{
  if (!aFindText || !aReplaceText || !aDidFind)
    return NS_ERROR_NULL_POINTER;

  *aDidFind = PR_FALSE;

  if (!mTsDoc)
    return NS_ERROR_NOT_INITIALIZED;

  PRBool enabled;

  GetReplaceEnabled(&enabled);

  if (!enabled)
    return NS_OK;

  nsAutoString findStr(aFindText);
  PRUint32 findStrLength = findStr.Length();
  if (!mCaseSensitive)
    ToLowerCase(findStr);

  nsresult result = SetupDocForReplace(mTsDoc, findStr, &mStartSelOffset);
  if (NS_FAILED(result))
    return result;  
  
  // find out where we started
  if (mWrapFind)
  {
    result = GetCurrentBlockIndex(mTsDoc, &mStartBlockIndex);
    if (NS_FAILED(result))
      return result;

    // and set the starting position again (hopefully, in future we won't have to do this)
    result = SetupDocForReplace(mTsDoc, findStr, &mStartSelOffset);
    if (NS_FAILED(result))
      return result;  
  }

  mCurrentBlockIndex = mStartBlockIndex;
  mCurrentSelOffset  = mStartSelOffset;
  mWrappedOnce = PR_FALSE;

  nsAutoString replaceStr(aReplaceText);
  PRUint32 replaceStrLength = replaceStr.Length();
  PRBool didReplace = PR_FALSE;

  // Keep looping until DoFind() fails to find another instance
  // of the find string or has looped through the entire document.

  do
  {
    result = DoFind(mTsDoc, findStr, aDidFind);

    if (NS_FAILED(result))
      break;

    if (!*aDidFind || (didReplace && !aAllOccurrences))
      break;


    if (mWrapFind)
    {
      // If we are wrapping, and we replace any part of
      // the starting block before mStartSelOffset, we
      // need to make sure that we adjust mStartSelOffset
      // so that after we've wrapped and get to the starting
      // block again, we know where to stop.

      if (mCurrentBlockIndex == mStartBlockIndex &&
          mCurrentSelOffset < mStartSelOffset)
      {
        mStartSelOffset += replaceStrLength - findStrLength;
        if (mStartSelOffset < 0)
          mStartSelOffset = 0;
      }
    }

    if (replaceStrLength == 0)
      result = mTsDoc->DeleteSelection();
    else
    {
      result = mTsDoc->InsertText(&replaceStr);

      // DoFind() sets mCurrentSelOffset to the beginning
      // of the selection. If we are searching in the forward
      // direction, set the offset after the string we just
      // inserted so that we don't search through it when we
      // call DoFind() again.

      if (!mFindBackwards)
        mCurrentSelOffset += replaceStrLength;
    }

    if (NS_FAILED(result))
      break;

    didReplace = PR_TRUE;
  }
  while (*aDidFind);

  return result;
}

// ----------------------------------------------------------------
//  CharsMatch
//
//  Compare chars. Match if both are whitespace, or both are
//  non whitespace and same char.
// ----------------------------------------------------------------

#define NBSP_CHARCODE ((PRUnichar)160)
#define IS_SPACE_CHAR(c) (nsCRT::IsAsciiSpace(c) || (c) == NBSP_CHARCODE)

inline static PRBool
CharsMatch(PRUnichar c1, PRUnichar c2)
{
  return (c1 == c2) || (IS_SPACE_CHAR(c1) && IS_SPACE_CHAR(c2));
}

// ----------------------------------------------------------------
//  FindInString
//
//  Routine to search in an nsString which is smart about extra
//  whitespace, can search backwards, and do case insensitive search.
//
//  This uses a brute-force algorithm, which should be sufficient
//  for our purposes (text searching)
// 
//  aSearchStr contains the text from a content node, which can contain
//  extra white space between words, which we have to deal with.
//  The offsets passed in and back are offsets into aSearchStr,
//  and thus include extra white space.
//
//  If we are ignoring case, the strings have already been lowercased
//  at this point.
//
//  aStartOffset is the offset in the search string to start searching
//  at. If -1, it means search from the start (forwards) or end (backwards).
//
//  aSkipTrailingSpaces adjusts the matching algorithm when dealing with
//  adjacent white space. If true the algorithm matches the exact number
//  of spaces in each consecutive run of spaces in the aPatternStr, and if
//  the number of consecutive spaces for a given run in aSearchStr is longer,
//  the trailing extra spaces are ignored/skipped, but included in the length
//  of the string matched. If false, the matching algorithm will only match
//  the number of spaces in each run contained in aPatternStr.
//
//  aFoundStrLen will contain the length of the string matching aPatternStr.
//  It should be noted that aFoundStrLen can be greater than the length of
//  aPatternStr since the matched string could contain extra whitespace.
//
//  Returns -1 if the string is not found, or if the pattern is an
//  empty string, or if aStartOffset is off the end of the string.
// ----------------------------------------------------------------

static PRInt32 FindInString(const nsString &aSearchStr, const nsString &aPatternStr,
                            PRInt32 aStartOffset, PRBool aSearchBackwards,
                            PRBool aSkipTrailingSpaces, PRInt32 *aFoundStrLen)
{
  PRInt32 patternLen = aPatternStr.Length();
  PRInt32 searchStrLen = aSearchStr.Length();

  if (aFoundStrLen)
    *aFoundStrLen = 0;

  if (patternLen == 0) // pattern is empty
    return -1;

  if (aStartOffset < 0)
    aStartOffset = (aSearchBackwards) ? searchStrLen : 0;

  if (aStartOffset > searchStrLen) // bad start offset
    return -1;

  if (patternLen > searchStrLen) // pattern is longer than string to search
    return -1;

  const PRUnichar *searchBuf = aSearchStr.get();
  const PRUnichar *patternBuf = aPatternStr.get();

  const PRUnichar *searchEnd = searchBuf + searchStrLen;
  const PRUnichar *patEnd = patternBuf + patternLen;

  const PRUnichar *s = searchBuf + aStartOffset;

  if (aSearchBackwards)
    s -= patternLen;

  while ((!aSearchBackwards && s <  searchEnd) ||
         ( aSearchBackwards && s >= searchBuf))
  {
    if (CharsMatch(*patternBuf, *s)) // start potential match
    {
      const PRUnichar *t = s;
      const PRUnichar *p = patternBuf;
      PRInt32 curMatchOffset = t - searchBuf;
      PRInt32 curMatchLen = 0;

      while (p < patEnd && t < searchEnd && CharsMatch(*p, *t))
      {
        PRBool didProcessWhitespace = PR_FALSE;

        while (t != searchEnd && p != patEnd && IS_SPACE_CHAR(*p))
        {
          didProcessWhitespace = PR_TRUE;

          if (!IS_SPACE_CHAR(*t))
            break;

          p++;
          t++;
          curMatchLen++;

          if (aSkipTrailingSpaces && (p == patEnd || !IS_SPACE_CHAR(*p)))
          {
            // leaving p whitespace. Eat up additional whitespace in t
            while (t < searchEnd && IS_SPACE_CHAR(*t))
            {
              t++;
              curMatchLen++;
            }
          }
        }

        if (!didProcessWhitespace)
        {
          t++;
          p++;
          curMatchLen++;
        }
      }

      if (p == patEnd)
      {
        if (aFoundStrLen)
          *aFoundStrLen = curMatchLen;

        return curMatchOffset;
      }
    }

    if (aSearchBackwards)
      s--;
    else // searching forwards
      s++;
  }

  return -1;
}

// utility method to discover which block we're in. The TSDoc interface doesn't give
// us this, because it can't assume a read-only document.
nsresult
nsFindAndReplace::GetCurrentBlockIndex(nsITextServicesDocument *aDoc, PRInt32 *outBlockIndex)
{
  PRInt32  blockIndex = 0;
  PRBool   isDone = PR_FALSE;
  nsresult result = NS_OK;

  do
  {
    aDoc->PrevBlock();

    result = aDoc->IsDone(&isDone);

    if (!isDone)
      blockIndex ++;

  } while (NS_SUCCEEDED(result) && !isDone);
  
  *outBlockIndex = blockIndex;

  return result;
}

nsresult
nsFindAndReplace::SetupDocForFind(nsITextServicesDocument *aDoc, PRInt32 *outBlockOffset)
{
  nsresult  rv;

  nsITextServicesDocument::TSDBlockSelectionStatus blockStatus;
  PRInt32 selOffset;
  PRInt32 selLength;

  // Set up the TSDoc. We are going to start searching thus:
  // 
  // Searching forwards:
  //        Look forward from the end of the selection
  // Searching backwards:
  //        Look backwards from the start of the selection
  //

  if (!mFindBackwards) // searching forwards
  {
    rv = aDoc->LastSelectedBlock(&blockStatus, &selOffset, &selLength);
    if (NS_SUCCEEDED(rv) && (blockStatus != nsITextServicesDocument::eBlockNotFound))
    {
      switch (blockStatus)
      {
        case nsITextServicesDocument::eBlockOutside:  // No TB in S, but found one before/after S.
        case nsITextServicesDocument::eBlockPartial:  // S begins or ends in TB but extends outside of TB.
          // the TS doc points to the block we want.
          *outBlockOffset = selOffset + selLength;
          break;
                    
        case nsITextServicesDocument::eBlockInside:  // S extends beyond the start and end of TB.
          // we want the block after this one.
          rv = aDoc->NextBlock();
          *outBlockOffset = 0;
          break;
                
        case nsITextServicesDocument::eBlockContains: // TB contains entire S.
          *outBlockOffset = selOffset + selLength;
          break;
        
        case nsITextServicesDocument::eBlockNotFound: // There is no text block (TB) in or before the selection (S).
        default:
          NS_NOTREACHED("Shouldn't ever get this status");
      }
    }
    else  //failed to get last sel block. Just start at beginning
    {
      rv = aDoc->FirstBlock();
    }
  
  }
  else  // searching backwards
  {
    rv = aDoc->FirstSelectedBlock(&blockStatus, &selOffset, &selLength);
    if (NS_SUCCEEDED(rv) && (blockStatus != nsITextServicesDocument::eBlockNotFound))
    {
      switch (blockStatus)
      {
        case nsITextServicesDocument::eBlockOutside:    // No TB in S, but found one before/after S.
          // must be a text block before the selection, so
          // offset should be at end of block.
          *outBlockOffset = -1;
          break;
        case nsITextServicesDocument::eBlockPartial:    // S begins or ends in TB but extends outside of TB.
        case nsITextServicesDocument::eBlockContains:    // TB contains entire S.
          // the TS doc points to the block we want.
          *outBlockOffset = selOffset;
          // if (selOffset > 0 && selLength > 0)
          //   *outBlockOffset -= 1;
          break;
                
        case nsITextServicesDocument::eBlockInside:      // S extends beyond the start and end of TB.
          // we want the block before this one.
          rv = aDoc->PrevBlock();
          *outBlockOffset = -1;
          break;
        case nsITextServicesDocument::eBlockNotFound:    // There is no text block (TB) in or before the selection (S).
        default:
          NS_NOTREACHED("Shouldn't ever get this status");
      }
    }
    else
    {
      rv = aDoc->LastBlock();
    }
  }

  return rv;
}


nsresult
nsFindAndReplace::SetupDocForReplace(nsITextServicesDocument *aDoc, const nsString &aFindStr, PRInt32 *outBlockOffset)
{
  nsresult  rv;

  nsITextServicesDocument::TSDBlockSelectionStatus blockStatus;
  PRInt32 selOffset;
  PRInt32 selLength;

  // Set up the TSDoc. We are going to start replacing thus:
  // 
  // Searching forwards:
  //        Look forward from the end of the selection
  // Searching backwards:
  //        Look backwards from the start of the selection
  //

  if (!mFindBackwards) // searching forwards
  {
    rv = aDoc->LastSelectedBlock(&blockStatus, &selOffset, &selLength);
    if (NS_SUCCEEDED(rv) && (blockStatus != nsITextServicesDocument::eBlockNotFound))
    {
      switch (blockStatus)
      {
        case nsITextServicesDocument::eBlockOutside:  // No TB in S, but found one before/after S.
        case nsITextServicesDocument::eBlockPartial:  // S begins or ends in TB but extends outside of TB.
          // the TS doc points to the block we want.
          *outBlockOffset = selOffset + selLength;
          break;
                    
        case nsITextServicesDocument::eBlockInside:  // S extends beyond the start and end of TB.
          // we want the block after this one.
          rv = aDoc->NextBlock();
          *outBlockOffset = 0;
          break;
                
        case nsITextServicesDocument::eBlockContains: // TB contains entire S.
          *outBlockOffset = selOffset;
          if (selLength != (PRInt32)aFindStr.Length())
            *outBlockOffset += selLength;
          break;
        
        case nsITextServicesDocument::eBlockNotFound: // There is no text block (TB) in or before the selection (S).
        default:
          NS_NOTREACHED("Shouldn't ever get this status");
      }
    }
    else  //failed to get last sel block. Just start at beginning
    {
      rv = aDoc->FirstBlock();
    }
  
  }
  else  // searching backwards
  {
    rv = aDoc->FirstSelectedBlock(&blockStatus, &selOffset, &selLength);
    if (NS_SUCCEEDED(rv) && (blockStatus != nsITextServicesDocument::eBlockNotFound))
    {
      switch (blockStatus)
      {
        case nsITextServicesDocument::eBlockOutside:    // No TB in S, but found one before/after S.
        case nsITextServicesDocument::eBlockPartial:    // S begins or ends in TB but extends outside of TB.
          // the TS doc points to the block we want.
          *outBlockOffset = selOffset;
          break;
                
        case nsITextServicesDocument::eBlockInside:      // S extends beyond the start and end of TB.
          // we want the block before this one.
          rv = aDoc->PrevBlock();
          *outBlockOffset = -1;
          break;
            
        case nsITextServicesDocument::eBlockContains:    // TB contains entire S.
          *outBlockOffset = selOffset;
          if (selLength == (PRInt32)aFindStr.Length())
            *outBlockOffset += selLength;
         break;
 
        case nsITextServicesDocument::eBlockNotFound:    // There is no text block (TB) in or before the selection (S).
        default:
          NS_NOTREACHED("Shouldn't ever get this status");
      }
    }
    else
    {
      rv = aDoc->LastBlock();
    }
  }

  return rv;
}

nsresult
nsFindAndReplace::DoFind(nsITextServicesDocument *aTxtDoc, const nsString &aFindString, PRBool *aDidFind)
{
  if (!aTxtDoc || !aDidFind)
    return NS_ERROR_NULL_POINTER;
  
  *aDidFind = PR_FALSE;

  nsresult rv = NS_OK;

  // Set up the TSDoc. We are going to start searching thus:
  // 
  // Searching forwards:
  //        Look forward from the end of the selection
  // Searching backwards:
  //        Look backwards from the start of the selection
  //

  PRBool done = PR_FALSE;

  // Loop till we find a match or fail.
  while ( !done )
  {
    PRBool atExtremum = PR_FALSE;  // are we at the end (or start)

    while ( NS_SUCCEEDED(aTxtDoc->IsDone(&atExtremum)) && !atExtremum )
    {
      nsString str;
      rv = aTxtDoc->GetCurrentTextBlock(&str);
  
      if (NS_FAILED(rv))
        return rv;
  
      if (!mCaseSensitive)
        ToLowerCase(str);
      
      if (mWrapFind && mWrappedOnce &&
          mCurrentBlockIndex == mStartBlockIndex && mCurrentSelOffset != -1)
      {
        if (( mFindBackwards && (mCurrentSelOffset <= mStartSelOffset)) ||
            (!mFindBackwards && (mCurrentSelOffset >= mStartSelOffset)))
        {
          done = PR_TRUE;
          break;
        }
      }

      PRInt32 foundLength = 0;
      PRInt32 foundOffset = FindInString(str, aFindString, mCurrentSelOffset, mFindBackwards, PR_TRUE, &foundLength);
  
      if (mWrapFind && mWrappedOnce && (mCurrentBlockIndex == mStartBlockIndex))
      {
        if ((mFindBackwards && ((foundOffset + (PRInt32)aFindString.Length()) <= mStartSelOffset)) ||
            (!mFindBackwards && (foundOffset >= mStartSelOffset)))
        {
          done = PR_TRUE;
          break;
        }
      }

      if (foundOffset != -1)
      {
        mCurrentSelOffset = foundOffset;  // reset for next call to DoFind().

        // Match found.  Select it, remember where it was, and quit.
        aTxtDoc->SetSelection(foundOffset, foundLength);
        aTxtDoc->ScrollSelectionIntoView();
        done = PR_TRUE;
        *aDidFind = PR_TRUE;
        break;
      }
      else
      {
        mCurrentSelOffset = -1;  // reset for next block

        // have we already been around once?
        if (mWrappedOnce && (mCurrentBlockIndex == mStartBlockIndex))
        {
          done = PR_TRUE;
          break;
        }

        // No match found in this block, try the next (or previous) one.
        if (mFindBackwards) {
          aTxtDoc->PrevBlock();
          mCurrentBlockIndex--;
        } else {
          aTxtDoc->NextBlock();
          mCurrentBlockIndex++;
        }
      }
          
    } // while !atExtremum
      
    // At end (or matched).  Decide which it was...
    if (!done)
    {
      // Hit end without a match.  If we haven't passed this way already,
      // then reset to the first/last block (depending on search direction).
      if (!mWrappedOnce)
      {
        // Reset now.
        mWrappedOnce = PR_TRUE;
        // If not wrapping, give up.
        if ( !mWrapFind ) {
          done = PR_TRUE;
        }
        else
        {
          if ( mFindBackwards ) {
            // Reset to last block.
            rv = aTxtDoc->LastBlock();
            // ugh
            rv = GetCurrentBlockIndex(aTxtDoc, &mCurrentBlockIndex);
            rv = aTxtDoc->LastBlock();
          } else {
            // Reset to first block.
            rv = aTxtDoc->FirstBlock();
            mCurrentBlockIndex = 0;
          }
        }
      }
      else
      {
        // already wrapped.  This means no matches were found.
        done = PR_TRUE;
      }
    }
  }

  return NS_OK;
}
