/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *   disttsc@bart.nl
 *   jarrod.k.gray@rose-hulman.edu
 *   Jan Varga <varga@utcru.sk>
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

// cache these services
var RDF = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService().QueryInterface(Components.interfaces.nsIRDFService);
var dragService = Components.classes["@mozilla.org/widget/dragservice;1"].getService().QueryInterface(Components.interfaces.nsIDragService);
var nsIDragService = Components.interfaces.nsIDragService;

function debugDump(msg)
{
  // uncomment for noise
  //dump(msg+"\n");
}

function CanDropOnFolderTree(index)
{
    var dragSession = null;
    var dragFolder = false;
    var flavor = false;

    dragSession = dragService.getCurrentSession();
    if (! dragSession)
        return false;

    if (dragSession.isDataFlavorSupported("text/x-moz-message-or-folder"))
        flavor = true;

    var trans = Components.classes["@mozilla.org/widget/transferable;1"].createInstance(Components.interfaces.nsITransferable);
    if (! trans)
        return false;
 
    var folderTree = GetFolderTree();
    var targetResource = GetFolderResource(folderTree, index);
    var targetUri = targetResource.Value;
    var targetFolder = targetResource.QueryInterface(Components.interfaces.nsIMsgFolder);
    var targetServer = targetFolder.server;
    var sourceServer;
    var sourceResource;

    trans.addDataFlavor("text/x-moz-message-or-folder");
   
    for (var i = 0; i < dragSession.numDropItems; i++)
    {
        dragSession.getData (trans, i);
        var dataObj = new Object();
        var bestFlavor = new Object();
        var len = new Object();
        try
        {
            trans.getAnyTransferData ( bestFlavor, dataObj, len );
        }
        catch (ex)
        {
            continue;   //no data so continue;
        }
        if (dataObj)
            dataObj = dataObj.value.QueryInterface(Components.interfaces.nsISupportsWString);
        if (! dataObj)
            continue;

        // pull the URL out of the data object
        var sourceUri = dataObj.data.substring(0, len.value);
        if (! sourceUri)
            continue;

        try
        {
            sourceResource = RDF.GetResource(sourceUri, true);
            var folder = sourceResource.QueryInterface(Components.interfaces.nsIFolder);
            if (folder)
                dragFolder = true;
        }
        catch(ex)
        {
            sourceResource = null;
            var isServer = GetFolderAttribute(folderTree, targetResource, "IsServer");
            if (isServer == "true")
            {
                debugDump("***isServer == true\n");
                return false;
            }
            // canFileMessages checks no select, and acl, for imap.
            var canFileMessages = GetFolderAttribute(folderTree, targetResource, "CanFileMessages");
            if (canFileMessages != "true")
            {
                debugDump("***canFileMessages == false\n");
                return false;
            }
            var hdr = messenger.messageServiceFromURI(sourceUri).messageURIToMsgHdr(sourceUri);
            if (hdr.folder == targetFolder)
                return false;
            break;
        }

        // we should only get here if we are dragging and dropping folders
        sourceResource = folder.QueryInterface(Components.interfaces.nsIRDFResource);
        var sourceFolder = sourceResource.QueryInterface(Components.interfaces.nsIMsgFolder);
        sourceServer = sourceFolder.server;

        if (targetUri == sourceUri)	
            return false;

        //don't allow drop on different imap servers.
        if (sourceServer != targetServer && targetServer.type == "imap")
            return false;

        //don't allow immediate child to be dropped to it's parent
        if (targetFolder.URI == sourceFolder.parent.URI)
        {
            debugDump(targetFolder.URI + "\n");
            debugDump(sourceFolder.parent.URI + "\n");     
            return false;
        }

        var isAncestor = sourceFolder.isAncestorOf(targetFolder);
        // don't allow parent to be dropped on its ancestors
        if (isAncestor)
            return false;
    }

    if (dragFolder)
    {
        //first check these conditions then proceed further
        debugDump("***isFolderFlavor == true \n");

        // no copy for folder drag
        if (dragSession.dragAction == nsIDragService.DRAGDROP_ACTION_COPY)
            return false;

        var canCreateSubfolders = GetFolderAttribute(folderTree, targetResource, "CanCreateSubfolders");
        // if cannot create subfolders then a folder cannot be dropped here     
        if (canCreateSubfolders == "false")
        {
            debugDump("***canCreateSubfolders == false \n");
            return false;
        }
        var serverType = GetFolderAttribute(folderTree, targetResource, "ServerType");

        // if we've got a folder that can't be renamed
        // allow us to drop it if we plan on dropping it on "Local Folders"
        // (but not within the same server, to prevent renaming folders on "Local Folders" that
        // should not be renamed)
        var srcCanRename = GetFolderAttribute(folderTree, sourceResource, "CanRename");
        if (srcCanRename == "false") {
            if (sourceServer == targetServer)
                return false;
            if (serverType != "none")
                return false;
        }
    }

    //message or folder
    if (flavor)
    {
        dragSession.canDrop = true;
        return true;
    }
	
    return false;
}

function CanDropBeforeAfterFolderTree(index, before)
{
    return false;
}

function DropOnFolderTree(row, orientation)
{
    if (orientation != Components.interfaces.nsITreeView.inDropOn)
        return false;

    var folderTree = GetFolderTree();
    var targetResource = GetFolderResource(folderTree, row);

    var targetUri = targetResource.Value;
    debugDump("***targetUri = " + targetUri + "\n");

    var dragSession = dragService.getCurrentSession();
    if (! dragSession )
        return false;

    var trans = Components.classes["@mozilla.org/widget/transferable;1"].createInstance(Components.interfaces.nsITransferable);
    trans.addDataFlavor("text/x-moz-message-or-folder");

    var list = Components.classes["@mozilla.org/supports-array;1"].createInstance(Components.interfaces.nsISupportsArray);

    var dropMessage = true;   	
    var sourceUri;
    var sourceResource;
    var sourceFolder;
    var sourceServer;
	
    for (var i = 0; i < dragSession.numDropItems; i++)
    {
        dragSession.getData (trans, i);
        var dataObj = new Object();
        var bestFlavor = new Object();
        var len = new Object();
        trans.getAnyTransferData(bestFlavor, dataObj, len);
        if (dataObj)
            dataObj = dataObj.value.QueryInterface(Components.interfaces.nsISupportsWString);
        if (! dataObj)
            continue;

        // pull the URL out of the data object
        sourceUri = dataObj.data.substring(0, len.value);
        if (! sourceUri)
            continue;

        debugDump("    Node #" + i + ": drop " + sourceUri + " to " + targetUri + "\n");

        sourceResource = RDF.GetResource(sourceUri, true);
        // only do this for the first object, either they are all messages or they are all folders
        if (i == 0) {
            try {
                sourceFolder = sourceResource.QueryInterface(Components.interfaces.nsIMsgFolder);
                if (sourceFolder) {
                    // we are dropping a folder
                    dropMessage = false;
                }
                else {
                    dropMessage = true;
                }
            }
            catch (ex) {
                dropMessage = true;
            }
        }
        else {
            if (! dropMessage)
                dump("drag and drop of multiple folders isn't supported\n");
        }

        if (dropMessage) {
            // from the message uri, get the appropriate messenger service
            // and then from that service, get the msgDbHdr
            list.AppendElement(messenger.messageServiceFromURI(sourceUri).messageURIToMsgHdr(sourceUri));
        }
        else {
            // Prevent dropping of a node before, after, or on itself
            if (sourceResource == targetResource)	
                continue;

            list.AppendElement(sourceResource);
        }
    }

    if (list.Count() < 1)
       return false;

    var isSourceNews = false;
    isSourceNews = isNewsURI(sourceUri);
    
    var targetFolder = targetResource.QueryInterface(Components.interfaces.nsIMsgFolder);
    var targetServer = targetFolder.server;

    if (dropMessage) {
        var sourceMsgHdr = list.GetElementAt(0).QueryInterface(Components.interfaces.nsIMsgDBHdr);
        sourceFolder = sourceMsgHdr.folder;
        sourceServer = sourceFolder.server;

        try {
            if (isSourceNews) {
                // news to pop or imap is always a copy
                messenger.CopyMessages(sourceFolder, targetFolder, list, false);
            }
            else {
                var dragAction = dragSession.dragAction;
                if (dragAction == nsIDragService.DRAGDROP_ACTION_COPY)
                    messenger.CopyMessages(sourceFolder, targetFolder, list, false);
                else if (dragAction == nsIDragService.DRAGDROP_ACTION_MOVE)
                    messenger.CopyMessages(sourceFolder, targetFolder, list, true);
            }
        }
        catch (ex) {
            dump("failed to copy messages: " + ex + "\n");
        }
    }
    else {
        sourceServer = sourceFolder.server;
        try 
        {
            messenger.CopyFolders(GetFolderDatasource(), targetResource, list, (sourceServer == targetServer));
        }
        catch(ex)
        {
            dump ("Exception : CopyFolders " + ex + "\n");
        }
    }
    return true;
}

function BeginDragFolderTree(event)
{
    debugDump("BeginDragFolderTree\n");

    if (event.originalTarget.localName != "treechildren")
      return false;

    var folderTree = GetFolderTree();
    var row = {};
    var col = {};
    var elt = {};
    folderTree.treeBoxObject.getCellAt(event.clientX, event.clientY, row, col, elt);
    if (row.value == -1)
      return false;

    var folderResource = GetFolderResource(folderTree, row.value);

    if (GetFolderAttribute(folderTree, folderResource, "IsServer") == "true")
    {
      debugDump("***IsServer == true\n");
      return false;
    }

    // do not allow the drag when news is the source
    if (GetFolderAttribute(folderTree, folderResource, "ServerType") == "nntp") 
    {
      debugDump("***ServerType == nntp\n");
      return false;
    }

    var selectedFolders = GetSelectedFolders();
    return BeginDragTree(event, folderTree, selectedFolders, "text/x-moz-message-or-folder");
}

function BeginDragThreadPane(event)
{
    debugDump("BeginDragThreadPane\n");

    var threadTree = GetThreadTree();
    var selectedMessages = GetSelectedMessages();

    //A message can be dragged from one window and dropped on another window
    //therefore setNextMessageAfterDelete() here 
    //no major disadvantage even if it is a copy operation

    SetNextMessageAfterDelete();
    return BeginDragTree(event, threadTree, selectedMessages, "text/x-moz-message-or-folder");
}

function BeginDragTree(event, tree, selArray, flavor)
{
    var dragStarted = false;

    var transArray = Components.classes["@mozilla.org/supports-array;1"].createInstance(Components.interfaces.nsISupportsArray);
    if ( !transArray ) 
      return false;

    // let's build the drag region
    var region = null;
    try {
      region = Components.classes["@mozilla.org/gfx/region;1"].createInstance(Components.interfaces.nsIScriptableRegion);
      region.init();
      var obo = tree.treeBoxObject;
      var bo = obo.treeBody.boxObject;
      var obosel= obo.selection;

      var rowX = bo.x;
      var rowY = bo.y;
      var rowHeight = obo.rowHeight;
      var rowWidth = bo.width;

      //add a rectangle for each visible selected row
      for (var i = obo.getFirstVisibleRow(); i <= obo.getLastVisibleRow(); i ++)
      {
        if (obosel.isSelected(i))
          region.unionRect(rowX, rowY, rowWidth, rowHeight);
        rowY = rowY + rowHeight;
      }
      
      //and finally, clip the result to be sure we don't spill over...
      region.intersectRect(bo.x, bo.y, bo.width, bo.height);
    } catch(ex) {
      dump("Error while building selection region: " + ex + "\n");
      region = null;
    }
    
    var count = selArray.length;
    debugDump("selArray.length = " + count + "\n");
    for (i = 0; i < count; ++i ) {
        var trans = Components.classes["@mozilla.org/widget/transferable;1"].createInstance(Components.interfaces.nsITransferable);
        if (!trans) 
          return false;

        var genTextData = Components.classes["@mozilla.org/supports-wstring;1"].createInstance(Components.interfaces.nsISupportsWString);
        if (!genTextData) 
          return false;

        trans.addDataFlavor(flavor);

        // get id (url)
        var id = selArray[i];
        genTextData.data = id;
        debugDump("    ID #" + i + " = " + id + "\n");

        trans.setTransferData ( flavor, genTextData, id.length * 2 );  // doublebyte byte data

        // put it into the transferable as an |nsISupports|
        var genTrans = trans.QueryInterface(Components.interfaces.nsISupports);
        transArray.AppendElement(genTrans);
    }

    dragService.invokeDragSession ( event.target, transArray, region, nsIDragService.DRAGDROP_ACTION_COPY +
    nsIDragService.DRAGDROP_ACTION_MOVE );

    dragStarted = true;

    return(!dragStarted);  // don't propagate the event if a drag has begun
}
