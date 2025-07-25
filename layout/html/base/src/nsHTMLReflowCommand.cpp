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
#include "nsCOMPtr.h"
#include "nsHTMLReflowCommand.h"
#include "nsHTMLParts.h"
#include "nsIFrame.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIContent.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsHTMLIIDs.h"
#include "nsFrame.h"
#include "nsContainerFrame.h"
#include "nsLayoutAtoms.h"

nsresult
NS_NewHTMLReflowCommand(nsHTMLReflowCommand**           aInstancePtrResult,
                        nsIFrame*                       aTargetFrame,
                        nsReflowType                    aReflowType,
                        nsIFrame*                       aChildFrame,
                        nsIAtom*                        aAttribute)
{
  NS_ASSERTION(aInstancePtrResult,
               "null result passed to NS_NewHTMLReflowCommand");

  *aInstancePtrResult = new nsHTMLReflowCommand(aTargetFrame, aReflowType,
                                                aChildFrame, aAttribute);
  if (!*aInstancePtrResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

#ifdef DEBUG_jesup
PRInt32 gReflows;
PRInt32 gReflowsInUse;
PRInt32 gReflowsInUseMax;
PRInt32 gReflowsZero;
PRInt32 gReflowsAuto;
PRInt32 gReflowsLarger;
PRInt32 gReflowsMaxZero;
PRInt32 gReflowsMaxAuto;
PRInt32 gReflowsMaxLarger;
class mPathStats {
public:
  mPathStats();
  ~mPathStats();
};
mPathStats::mPathStats()
{
}
mPathStats::~mPathStats()
{
  printf("nsHTMLReflowCommand->mPath stats:\n");
  printf("\tNumber created:   %d\n",gReflows);
  printf("\tNumber in-use:    %d\n",gReflowsInUseMax);

  printf("\tNumber size == 0: %d\n",gReflowsZero);
  printf("\tNumber size <= 8: %d\n",gReflowsAuto);
  printf("\tNumber size  > 8: %d\n",gReflowsLarger);
  printf("\tNum max size == 0: %d\n",gReflowsMaxZero);
  printf("\tNum max size <= 8: %d\n",gReflowsMaxAuto);
  printf("\tNum max size  > 8: %d\n",gReflowsMaxLarger);
}

// Just so constructor/destructor's get called
mPathStats gmPathStats;
#endif

// Construct a reflow command given a target frame, reflow command type,
// and optional child frame
nsHTMLReflowCommand::nsHTMLReflowCommand(nsIFrame*    aTargetFrame,
                                         nsReflowType aReflowType,
                                         nsIFrame*    aChildFrame,
                                         nsIAtom*     aAttribute)
  : mType(aReflowType), mTargetFrame(aTargetFrame), mChildFrame(aChildFrame),
    mPrevSiblingFrame(nsnull),
    mAttribute(aAttribute),
    mListName(nsnull),
    mFlags(0)
{
  MOZ_COUNT_CTOR(nsHTMLReflowCommand);
  NS_PRECONDITION(mTargetFrame != nsnull, "null target frame");
  if (nsnull!=mAttribute)
    NS_ADDREF(mAttribute);
#ifdef DEBUG_jesup
  gReflows++;
  gReflowsInUse++;
  if (gReflowsInUse > gReflowsInUseMax)
    gReflowsInUseMax = gReflowsInUse;
#endif
}

nsHTMLReflowCommand::~nsHTMLReflowCommand()
{
  MOZ_COUNT_DTOR(nsHTMLReflowCommand);
#ifdef DEBUG_jesup
  if (mPath.GetArraySize() == 0)
    gReflowsMaxZero++;
  else if (mPath.GetArraySize() <= 8)
    gReflowsMaxAuto++;
  else
    gReflowsMaxLarger++;
  gReflowsInUse--;
#endif

  NS_IF_RELEASE(mAttribute);
  NS_IF_RELEASE(mListName);
}

void nsHTMLReflowCommand::BuildPath()
{
#ifdef DEBUG_jesup
  if (mPath.Count() == 0)
    gReflowsZero++;
  else if (mPath.Count() <= 8)
    gReflowsAuto++;
  else
    gReflowsLarger++;
#endif

  mPath.Clear();

  // Construct the reflow path by walking up the through the frames'
  // parent chain until we reach either a `reflow root' or the root
  // frame in the frame hierarchy.
  nsIFrame *f = mTargetFrame;
  nsFrameState state;
  do {
    mPath.AppendElement(f);
    f->GetFrameState(&state);
  } while (!(state & NS_FRAME_REFLOW_ROOT) && (f->GetParent(&f), f != nsnull));
}

nsresult
nsHTMLReflowCommand::Dispatch(nsIPresContext*      aPresContext,
                              nsHTMLReflowMetrics& aDesiredSize,
                              const nsSize&        aMaxSize,
                              nsIRenderingContext& aRendContext)
{
  // Build the path from the target frame (index 0)
  BuildPath();

  // Send an incremental reflow notification to the first frame in the
  // path.
  nsIFrame* first = (nsIFrame*)mPath[mPath.Count() - 1];

  nsCOMPtr<nsIPresShell> shell;
  aPresContext->GetShell(getter_AddRefs(shell));

  nsIFrame* root;
  shell->GetRootFrame(&root);

  // Remove the first frame from the path and reflow it.
  mPath.RemoveElementAt(mPath.Count() - 1);

  first->WillReflow(aPresContext);
  nsContainerFrame::PositionFrameView(aPresContext, first);

  // If the first frame in the path is the root of the frame
  // hierarchy, then use all the available space. If it's simply a
  // `reflow root', then use the first frame's size as the available
  // space.
  nsSize size;
  if (first == root)
    size = aMaxSize;
  else
    first->GetSize(size);

  nsHTMLReflowState reflowState(aPresContext, first, *this,
                                &aRendContext, size);

  nsReflowStatus status;
  first->Reflow(aPresContext, aDesiredSize, reflowState, status);

  // If an incremental reflow is initiated at a frame other than the
  // root frame, then its desired size had better not change!
  NS_ASSERTION(first == root ||
               (aDesiredSize.width == size.width && aDesiredSize.height == size.height),
               "non-root frame's desired size changed during an incremental reflow");

  first->SizeTo(aPresContext, aDesiredSize.width, aDesiredSize.height);

  nsIView* view;
  first->GetView(aPresContext, &view);
  if (view)
    nsContainerFrame::SyncFrameViewAfterReflow(aPresContext, first, view, nsnull);

  first->DidReflow(aPresContext, nsnull, NS_FRAME_REFLOW_FINISHED);

  return NS_OK;
}

nsresult
nsHTMLReflowCommand::GetNext(nsIFrame*& aNextFrame, PRBool aRemove)
{
  PRInt32 count = mPath.Count();

  aNextFrame = nsnull;
  if (count > 0) {
    aNextFrame = (nsIFrame*)mPath[count - 1];
    if (aRemove) {
      mPath.RemoveElementAt(count - 1);
    }
  }
  return NS_OK;
}

nsresult
nsHTMLReflowCommand::GetTarget(nsIFrame*& aTargetFrame) const
{
  aTargetFrame = mTargetFrame;
  return NS_OK;
}

nsresult
nsHTMLReflowCommand::SetTarget(nsIFrame* aTargetFrame)
{
  mTargetFrame = aTargetFrame;
  return NS_OK;
}

nsresult
nsHTMLReflowCommand::GetType(nsReflowType& aReflowType) const
{
  aReflowType = mType;
  return NS_OK;
}

nsresult
nsHTMLReflowCommand::GetAttribute(nsIAtom *& aAttribute) const
{
  aAttribute = mAttribute;
  if (nsnull!=aAttribute)
    NS_ADDREF(aAttribute);
  return NS_OK;
}

nsresult
nsHTMLReflowCommand::GetChildFrame(nsIFrame*& aChildFrame) const
{
  aChildFrame = mChildFrame;
  return NS_OK;
}

nsresult
nsHTMLReflowCommand::GetChildListName(nsIAtom*& aListName) const
{
  aListName = mListName;
  NS_IF_ADDREF(aListName);
  return NS_OK;
}

nsresult
nsHTMLReflowCommand::SetChildListName(nsIAtom* aListName)
{
  mListName = aListName;
  NS_IF_ADDREF(mListName);
  return NS_OK;
}

nsresult
nsHTMLReflowCommand::GetPrevSiblingFrame(nsIFrame*& aSiblingFrame) const
{
  aSiblingFrame = mPrevSiblingFrame;
  return NS_OK;
}

nsresult
nsHTMLReflowCommand::List(FILE* out) const
{
#ifdef DEBUG
  static const char* kReflowCommandType[] = {
    "ContentChanged",
    "StyleChanged",
    "ReflowDirty",
    "UserDefined",
  };

  fprintf(out, "ReflowCommand@%p[%s]:",
          this, kReflowCommandType[mType]);
  if (mTargetFrame) {
    fprintf(out, " target=");
    nsFrame::ListTag(out, mTargetFrame);
  }
  if (mChildFrame) {
    fprintf(out, " child=");
    nsFrame::ListTag(out, mChildFrame);
  }
  if (mPrevSiblingFrame) {
    fprintf(out, " prevSibling=");
    nsFrame::ListTag(out, mPrevSiblingFrame);
  }
  if (mAttribute) {
    fprintf(out, " attr=");
    nsAutoString attr;
    mAttribute->ToString(attr);
    fputs(NS_LossyConvertUCS2toASCII(attr).get(), out);
  }
  if (mListName) {
    fprintf(out, " list=");
    nsAutoString attr;
    mListName->ToString(attr);
    fputs(NS_LossyConvertUCS2toASCII(attr).get(), out);
  }
  fprintf(out, "\n");

  // Show the path, but without using mPath which is in an undefined
  // state at this point.
  if (mTargetFrame) {
    PRBool didOne = PR_FALSE;
    for (nsIFrame* f = mTargetFrame; nsnull != f; f->GetParent(&f)) {
      if (f != mTargetFrame) {
        fprintf(out, " ");
        nsFrame::ListTag(out, f);
        didOne = PR_TRUE;
      }
    }
    if (didOne) {
      fprintf(out, "\n");
    }
  }
#endif
  return NS_OK;
}

nsresult
nsHTMLReflowCommand::GetFlags(PRInt32* aFlags)
{
  *aFlags = mFlags;
  return NS_OK;
}

nsresult
nsHTMLReflowCommand::SetFlags(PRInt32 aFlags)
{
  mFlags = aFlags;
  return NS_OK;
}

