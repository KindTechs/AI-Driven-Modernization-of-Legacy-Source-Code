/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 */

/*  This file contains the js functions necessary to implement view navigation within the 3 pane. */

//NOTE: gMessengerBundle must be defined and set or this Overlay won't work

var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService();
promptService = promptService.QueryInterface(Components.interfaces.nsIPromptService);
var accountManager = Components.classes["@mozilla.org/messenger/account-manager;1"].getService(Components.interfaces.nsIMsgAccountManager);

// we need the account manager datasource for when trying
// to figure out which account is next in the folder pane.
var gAccountManagerDataSource = Components.classes["@mozilla.org/rdf/datasource;1?name=msgaccountmanager"].createInstance().QueryInterface(Components.interfaces.nsIRDFDataSource);

// we can't compare the name to determine the order in the folder pane
// we need to compare the value of the sort resource, 
// as that's what we use to sort on in the folder pane
var gNameProperty = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService(Components.interfaces.nsIRDFService).GetResource("http://home.netscape.com/NC-rdf#Name?sort=true");

function compareServerSortOrder(server1, server2)
{
  var sortValue1, sortValue2;

  try {
    var res1 = RDF.GetResource(server1.URI);
    sortValue1 = gAccountManagerDataSource.GetTarget(res1, gNameProperty, true).QueryInterface(Components.interfaces.nsIRDFLiteral).Value;
  }
  catch (ex) {
    dump("XXX ex " + server1.URI + "," + ex + "\n");
    sortValue1 = "";
  }

  try {
    var res2 = RDF.GetResource(server2.URI);
    sortValue2 = gAccountManagerDataSource.GetTarget(res2, gNameProperty, true).QueryInterface(Components.interfaces.nsIRDFLiteral).Value;
  }
  catch (ex) {
    dump("XXX ex " + server2.URI + "," + ex + "\n");
    sortValue2 = "";
  }

  if (sortValue1 < sortValue2)
    return -1;
  else if (sortValue1 > sortValue2)
    return 1;
  else 
    return 0;
}

function compareFolderSortKey(folder1, folder2) 
{
  return folder1.compareSortKeys(folder2);
}

function GetSubFoldersInFolderPaneOrder(folder)
{
  var subFolderEnumerator = folder.GetSubFolders();
  var done = false;
  var msgFolders = Array();

  // get all the subfolders
  while (!done) {
    try {
      var element = subFolderEnumerator.currentItem();
	  var msgFolder = element.QueryInterface(Components.interfaces.nsIMsgFolder);
      msgFolders[msgFolders.length] = msgFolder;
	  

      subFolderEnumerator.next();
    }
    catch (ex) {
      done = true;
    }
  }

  // sort the subfolders
  msgFolders.sort(compareFolderSortKey);
  return msgFolders;
}

function FindNextChildFolder(folder)
{
  // if there is unread mail in the trash, sent, drafts, unsent messages
  // or templates folders, we ignore it
  // when doing cross folder "next" navigation
  if (IsSpecialFolder(folder, MSG_FOLDER_FLAG_TRASH | MSG_FOLDER_FLAG_SENTMAIL | MSG_FOLDER_FLAG_DRAFTS | MSG_FOLDER_FLAG_QUEUE | MSG_FOLDER_FLAG_TEMPLATES))
    return null;

  if (folder.getNumUnread(false) > 0)
    return folder;

  if (folder.getNumUnread(true) > 0) {
    var subFolders = GetSubFoldersInFolderPaneOrder(folder);
    for (var i = 0; i < subFolders.length; i++) {
      folder = FindNextChildFolder(subFolders[i]);
      if (folder)
        return folder;
    }
  }

  return null;
}

function FindNextFolder()
{
  // look for the next folder, this will only look on the current account
  // and below us, in the folder pane
  // note use of gDBView restricts this function to message folders
  // otherwise you could go next unread from a server
  var folder = FindNextChildFolder(gDBView.msgFolder);
  if (folder)
    return folder;

  // didn't find folder in children
  // go up to the parent, and start at the folder after the current one
  // unless we are at a server, in which case bail out.
  for (folder = gDBView.msgFolder; !folder.isServer; ) {

    var parent = folder.parent;
    var msgFolders = GetSubFoldersInFolderPaneOrder(parent);
    for (var i = 0; i < msgFolders.length; i++)
      if (msgFolders[i].URI == folder.URI)
        break;
    
    // the current folder is at index i
    // start at the next folder after that, if there is one
    while (++i < msgFolders.length) {
      folder = FindNextChildFolder(msgFolders[i]);
      if (folder)
        return folder;
    }
 
    // none at this level after the current folder.  go up.
    folder = parent;
  }

  // nothing in the current account, start with the next account (below)
  // and try until we hit the bottom of the folder pane

  // start at the account after the current account
  var rootFolders = GetRootFoldersInFolderPaneOrder();
  for (i = 0; i < rootFolders.length; i++) {
    if (rootFolders[i].URI == gDBView.msgFolder.server.serverURI)
      break;
  }
  
  for (var j = i + 1; j < rootFolders.length; j++) {
    folder = FindNextChildFolder(rootFolders[j]);
    if (folder)
      return folder;
  }
  
  // if nothing from the current account down to the bottom
  // (of the folder pane), start again at the top.
  for (j = 0; j <= i; j++) {
    folder = FindNextChildFolder(rootFolders[j]);
    if (folder)
      return folder;
  }
  return null;
}

function GetRootFoldersInFolderPaneOrder()
{
  var allServers = accountManager.allServers;
  var numServers = allServers.Count();

  var serversMsgFolders = Array(numServers);
  for (var i = 0; i < numServers; i++)
    serversMsgFolders[i] = allServers.GetElementAt(i).QueryInterface(Components.interfaces.nsIMsgIncomingServer).rootMsgFolder;

  // sort accounts, so they are in the same order as folder pane
  serversMsgFolders.sort(compareServerSortOrder);
 
  return serversMsgFolders;
}

function CrossFolderNavigation(type, supportsFolderPane )
{
  if (type != nsMsgNavigationType.nextUnreadMessage) {
    // only do cross folder navigation for "next unread message"
    return null;
  }

  var nextMode = pref.getIntPref("mailnews.nav_crosses_folders");
  // 0: "next" goes to the next folder, without prompting
  // 1: "next" goes to the next folder, and prompts (the default)
  // 2: "next" does nothing when there are no unread messages

  // not crossing folders, don't find next
  if (nextMode == 2) return null;

  var folder = FindNextFolder();
  if (folder && (gDBView.msgFolder.URI != folder.URI)) {
    switch (nextMode) {
      case 0:
        // do this unconditionally
        gNextMessageAfterLoad = type;
        if (supportsFolderPane)
          SelectFolder(folder.URI);
        break;
      case 1:
      default:
        var promptText = gMessengerBundle.getFormattedString("advanceNextPrompt", [ folder.name ], 1); 
        if (promptService.confirm(window, promptText, promptText)) {
          gNextMessageAfterLoad = type;
          if (supportsFolderPane)
            SelectFolder(folder.URI);
        }
        break;
    }
  }

  return folder;
}


function ScrollToMessage(type, wrap, selectMessage)
{
  try {
    var treeView = gDBView.QueryInterface(Components.interfaces.nsITreeView);
    var treeSelection = treeView.selection;
    var currentIndex = treeSelection.currentIndex;

    var resultId = new Object;
    var resultIndex = new Object;
    var threadIndex = new Object;

    gDBView.viewNavigate(type, resultId, resultIndex, threadIndex, true /* wrap */);

    // only scroll and select if we found something
    if ((resultId.value != nsMsgViewIndex_None) && (resultIndex.value != nsMsgViewIndex_None)) {
        if (selectMessage){
            treeSelection.select(resultIndex.value);
        }
        EnsureRowInThreadTreeIsVisible(resultIndex.value);
        return true;
    }
    else {
        return false;
    }
  }
  catch (ex) {
    return false;
  }
}

function GoNextMessage(type, startFromBeginning)
{
  try {
    var succeeded = ScrollToMessage(type, startFromBeginning, true);
    if (!succeeded) {
      CrossFolderNavigation(type, true);
    }
  }
  catch (ex) {
    dump("GoNextMessage ex = " + ex + "\n");
  }

  SetFocusThreadPaneIfNotOnMessagePane();
}

