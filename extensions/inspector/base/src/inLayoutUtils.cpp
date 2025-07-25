/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#include "inLayoutUtils.h"

#include "nsIDOMDocumentView.h"
#include "nsIDOMAbstractView.h"
#include "nsIDOMNodeList.h"
#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsIContentViewer.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIWebNavigation.h"
#include "nsIPresShell.h"
#include "nsIViewManager.h"
#include "nsIWidget.h"
#include "nsIPresContext.h"
#include "nsIStyleContext.h" 
#include "nsXULAtoms.h"
#include "nsHTMLAtoms.h"

///////////////////////////////////////////////////////////////////////////////

nsIDOMWindowInternal*
inLayoutUtils::GetWindowFor(nsIDOMElement* aElement)
{
  nsCOMPtr<nsIDOMDocument> doc1;
  aElement->GetOwnerDocument(getter_AddRefs(doc1));
  return GetWindowFor(doc1);
}

nsIDOMWindowInternal*
inLayoutUtils::GetWindowFor(nsIDOMDocument* aDoc)
{
  nsCOMPtr<nsIDOMDocumentView> doc = do_QueryInterface(aDoc);
  if (!doc) return nsnull;
  
  nsCOMPtr<nsIDOMAbstractView> view;
  doc->GetDefaultView(getter_AddRefs(view));
  if (!view) return nsnull;
  
  nsCOMPtr<nsIDOMWindowInternal> window = do_QueryInterface(view);
  return window;
}

nsIPresShell* 
inLayoutUtils::GetPresShellFor(nsISupports* aThing)
{
  nsCOMPtr<nsIScriptGlobalObject> so = do_QueryInterface(aThing);
  nsCOMPtr<nsIDocShell> docShell;
  so->GetDocShell(getter_AddRefs(docShell));
  
  nsCOMPtr<nsIPresShell> presShell;
  docShell->GetPresShell(getter_AddRefs(presShell));

  return presShell;
}

/*static*/
nsIFrame*
inLayoutUtils::GetFrameFor(nsIDOMElement* aElement, nsIPresShell* aShell)
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(aElement);
  nsIFrame* frame = nsnull;
  aShell->GetPrimaryFrameFor(content, &frame);

  return frame;
}

nsIRenderingContext*
inLayoutUtils::GetRenderingContextFor(nsIPresShell* aShell)
{
  nsCOMPtr<nsIViewManager> viewman;
  aShell->GetViewManager(getter_AddRefs(viewman));
  nsCOMPtr<nsIWidget> widget;
  viewman->GetWidget(getter_AddRefs(widget));
  return widget->GetRenderingContext(); // AddRefs
}

nsIEventStateManager*
inLayoutUtils::GetEventStateManagerFor(nsIDOMElement *aElement)
{
  if (aElement) {
    // get the document
    nsCOMPtr<nsIDOMDocument> doc1;
    aElement->GetOwnerDocument(getter_AddRefs(doc1));
    nsCOMPtr<nsIDocument> doc;
    doc = do_QueryInterface(doc1);
  
    // use the first PresShell
    PRInt32 num = doc->GetNumberOfShells();
    if (num > 0) {
      nsCOMPtr<nsIPresShell> shell;
      doc->GetShellAt(0, getter_AddRefs(shell));
      nsCOMPtr<nsIPresContext> presContext;
      shell->GetPresContext(getter_AddRefs(presContext));
      
      nsCOMPtr<nsIEventStateManager> esm;
      presContext->GetEventStateManager(getter_AddRefs(esm));
      
      return esm;
    }
  }  
  
  return nsnull;
}

nsPoint
inLayoutUtils::GetClientOrigin(nsIFrame* aFrame)
{
  nsPoint result(0,0);
  nsIFrame* parent = aFrame;
  while (parent) {
    nsPoint origin;
    parent->GetOrigin(origin);
    result.x += origin.x;
    result.y += origin.y;
    parent->GetParent(&parent);
  }
  return result;
}

nsRect& 
inLayoutUtils::GetScreenOrigin(nsIDOMElement* aElement)
{
  nsRect* rect = new nsRect(0,0,0,0);
 
  nsCOMPtr<nsIContent> content = do_QueryInterface(aElement);
  nsCOMPtr<nsIDocument> doc;
  content->GetDocument(*getter_AddRefs(doc));

  if (doc) {
    // Get Presentation shell 0
    nsCOMPtr<nsIPresShell> presShell;
    doc->GetShellAt(0, getter_AddRefs(presShell));
    
    if (presShell) {
      // Flush all pending notifications so that our frames are uptodate
      presShell->FlushPendingNotifications(PR_FALSE);

      nsCOMPtr<nsIPresContext> presContext;
      presShell->GetPresContext(getter_AddRefs(presContext));
      
      if (presContext) {
        nsIFrame* frame = nsnull;
        nsresult rv = presShell->GetPrimaryFrameFor(content, &frame);
        
        PRInt32 offsetX = 0;
        PRInt32 offsetY = 0;
        nsCOMPtr<nsIWidget> widget;
        
        while (frame) {
          // Look for a widget so we can get screen coordinates
          nsIView* view = nsnull;
          rv = frame->GetView(presContext, &view);
          if (NS_SUCCEEDED(rv) && view) {
            rv = view->GetWidget(*getter_AddRefs(widget));
            if (widget)
              break;
          }
          
          // No widget yet, so count up the coordinates of the frame 
          nsPoint origin;
          frame->GetOrigin(origin);
          offsetX += origin.x;
          offsetY += origin.y;
      
          frame->GetParent(&frame);
        }
        
        if (widget) {
          // Get the widget's screen coordinates
          nsRect oldBox(0,0,0,0);
          widget->WidgetToScreen(oldBox, *rect);

          // Get the scale from that Presentation Context
          float p2t;
          presContext->GetPixelsToTwips(&p2t);

          // Convert screen rect to twips
          rect->x = NSIntPixelsToTwips(rect->x, p2t);
          rect->y = NSIntPixelsToTwips(rect->y, p2t);

          //  Add the offset we've counted
          rect->x += offsetX;
          rect->y += offsetY;
        }
      }
    }
  }
  
  return *rect;
}

nsIBindingManager* 
inLayoutUtils::GetBindingManagerFor(nsIDOMNode* aNode)
{
  nsCOMPtr<nsIDOMDocument> domdoc;
  aNode->GetOwnerDocument(getter_AddRefs(domdoc));
  if (domdoc) {
    nsCOMPtr<nsIDocument> doc = do_QueryInterface(domdoc);
    nsCOMPtr<nsIBindingManager> bindingManager = do_QueryInterface(domdoc);
    doc->GetBindingManager(getter_AddRefs(bindingManager));
    
    return bindingManager;
  }
  
  return nsnull;
}

nsIDOMDocument*
inLayoutUtils::GetSubDocumentFor(nsIDOMNode* aNode)
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);
  if (content) {
    nsCOMPtr<nsIDocument> doc;
    content->GetDocument(*getter_AddRefs(doc));
    if (doc) {
      nsCOMPtr<nsIPresShell> shell;
      doc->GetShellAt(0, getter_AddRefs(shell));
      if (shell) {
        nsCOMPtr<nsISupports> supports;
        shell->GetSubShellFor(content, getter_AddRefs(supports));
        nsCOMPtr<nsIDocShell> docShell = do_QueryInterface(supports);
        if (docShell) {
          nsCOMPtr<nsIContentViewer> contentViewer;
          docShell->GetContentViewer(getter_AddRefs(contentViewer));
          if (contentViewer) {
            nsCOMPtr<nsIDOMDocument> domdoc;
            contentViewer->GetDOMDocument(getter_AddRefs(domdoc));
            return domdoc;
          }
        }
      }
    }
  }
  
  return nsnull;
}

nsIDOMNode*
inLayoutUtils::GetContainerFor(nsIDOMDocument* aDoc)
{
  nsCOMPtr<nsIDOMNode> container;

  // get the doc shell for this document and look for the parent doc shell
  nsCOMPtr<nsIDOMWindowInternal> win = inLayoutUtils::GetWindowFor(aDoc);
  nsCOMPtr<nsIScriptGlobalObject> so = do_QueryInterface(win);

  nsCOMPtr<nsIDocShell> docShell;
  so->GetDocShell(getter_AddRefs(docShell));
  nsCOMPtr<nsIDocShellTreeItem> treeItem = do_QueryInterface(docShell);

  nsCOMPtr<nsIDocShellTreeItem> parentItem;
  treeItem->GetParent(getter_AddRefs(parentItem));
  if (!parentItem) return nsnull;
  nsCOMPtr<nsIDocShell> parentDocShell = do_QueryInterface(parentItem);

  // find the content node (browser, iframe, etc..) that contains this document
  nsCOMPtr<nsIPresShell> presShell;
  parentDocShell->GetPresShell(getter_AddRefs(presShell));

  nsCOMPtr<nsIContent> content;
  presShell->FindContentForShell(docShell, getter_AddRefs(content));
  nsCOMPtr<nsIDOMNode> node = do_QueryInterface(content);

  return node;
}

PRBool
inLayoutUtils::IsDocumentElement(nsIDOMNode* aNode)
{
  PRBool result = PR_FALSE;

  nsCOMPtr<nsIDOMNode> parent;
  aNode->GetParentNode(getter_AddRefs(parent));
  if (parent) {
    PRUint16 nodeType;
    parent->GetNodeType(&nodeType);
    if (nodeType == 9)
      result = PR_TRUE;
  }

  return result;
}

