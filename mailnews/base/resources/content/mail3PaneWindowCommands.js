/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 1998-2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributors(s):
 *   Jan Varga <varga@utcru.sk>
 *   H�kan Waara (hwaara@chello.se)
 */

var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService(Components.interfaces.nsIPromptService);
var gMessengerBundle = document.getElementById("bundle_messenger");

// Controller object for folder pane
var FolderPaneController =
{
   supportsCommand: function(command)
	{
		switch ( command )
		{
			case "cmd_delete":
			case "button_delete":
			case "cmd_selectAll":
			case "cmd_cut":
			case "cmd_copy":
			case "cmd_paste":
				return true;
				
			default:
				return false;
		}
	},

	isCommandEnabled: function(command)
	{
    if (IsFakeAccount()) 
      return false;

		switch ( command )
		{
			case "cmd_selectAll":
			case "cmd_cut":
			case "cmd_copy":
			case "cmd_paste":
				return false;
			case "cmd_delete":
			case "button_delete":
				if ( command == "cmd_delete" )
					goSetMenuValue(command, 'valueFolder');
                                var folderTree = GetFolderTree();
                                var startIndex = {};
                                var endIndex = {};
                                folderTree.treeBoxObject.selection.getRangeAt(0, startIndex, endIndex);
                                if (startIndex.value >= 0) {
                                        var canDeleteThisFolder;
					var specialFolder = null;
					var isServer = null;
					var serverType = null;
					try {
                                                var folderResource = GetFolderResource(folderTree, startIndex.value);
                                                specialFolder = GetFolderAttribute(folderTree, folderResource, "SpecialFolder");
                                                isServer = GetFolderAttribute(folderTree, folderResource, "IsServer");
                                                serverType = GetFolderAttribute(folderTree, folderResource, "ServerType");
                                                if (serverType == "nntp") {
			     	                        if ( command == "cmd_delete" ) {
					                        goSetMenuValue(command, 'valueNewsgroup');
				    	                        goSetAccessKey(command, 'valueNewsgroupAccessKey');
                                                        }
                                                }
					}
					catch (ex) {
						//dump("specialFolder failure: " + ex + "\n");
					}
                                        if (specialFolder == "Inbox" || specialFolder == "Trash" || isServer == "true")
                                                canDeleteThisFolder = false;
                                        else
                                                canDeleteThisFolder = true;
                                        return canDeleteThisFolder && isCommandEnabled(command);
                                }
				else
					return false;

			default:
				return false;
		}
	},

	doCommand: function(command)
	{
    // if the user invoked a key short cut then it is possible that we got here for a command which is
    // really disabled. kick out if the command should be disabled.
    if (!this.isCommandEnabled(command)) return;

		switch ( command )
		{
			case "cmd_delete":
			case "button_delete":
				MsgDeleteFolder();
				break;
		}
	},
	
	onEvent: function(event)
	{
		// on blur events set the menu item texts back to the normal values
		if ( event == 'blur' )
        {
			goSetMenuValue('cmd_delete', 'valueDefault');
        }
	}
};


// Controller object for thread pane
var ThreadPaneController =
{
   supportsCommand: function(command)
	{
		switch ( command )
		{
			case "cmd_selectAll":
			case "cmd_cut":
			case "cmd_copy":
			case "cmd_paste":
				return true;
				
			default:
				return false;
		}
	},

	isCommandEnabled: function(command)
	{
		switch ( command )
		{
			case "cmd_selectAll":
				return true;
			
			case "cmd_cut":
			case "cmd_copy":
			case "cmd_paste":
				return false;

			default:
				return false;
		}
	},

	doCommand: function(command)
	{
    // if the user invoked a key short cut then it is possible that we got here for a command which is
    // really disabled. kick out if the command should be disabled.
    if (!this.isCommandEnabled(command)) return;
    if (!gDBView) return;

		switch ( command )
		{
			case "cmd_selectAll":
                // if in threaded mode, the view will expand all before selecting all
                gDBView.doCommand(nsMsgViewCommandType.selectAll)
                if (gDBView.numSelected != 1) {
                    setTitleFromFolder(gDBView.msgFolder,null);
                    ClearMessagePane();
                }
                break;
		}
	},
	
	onEvent: function(event)
	{
	}
};

// DefaultController object (handles commands when one of the trees does not have focus)
var DefaultController =
{
   supportsCommand: function(command)
	{

		switch ( command )
		{
      case "cmd_createFilterFromPopup":
			case "cmd_close":
			case "cmd_reply":
			case "button_reply":
			case "cmd_replySender":
			case "cmd_replyGroup":
			case "cmd_replyall":
			case "button_replyall":
			case "cmd_forward":
			case "button_forward":
			case "cmd_forwardInline":
			case "cmd_forwardAttachment":
			case "cmd_editAsNew":
      case "cmd_createFilterFromMenu":
			case "cmd_delete":
			case "button_delete":
			case "cmd_shiftDelete":
			case "cmd_nextMsg":
      case "button_next":
			case "cmd_nextUnreadMsg":
			case "cmd_nextFlaggedMsg":
			case "cmd_nextUnreadThread":
			case "cmd_previousMsg":
			case "cmd_previousUnreadMsg":
			case "cmd_previousFlaggedMsg":
			case "cmd_viewAllMsgs":
			case "cmd_viewUnreadMsgs":
      case "cmd_viewThreadsWithUnread":
      case "cmd_viewWatchedThreadsWithUnread":
      case "cmd_viewIgnoredThreads":
      case "cmd_undo":
      case "cmd_redo":
			case "cmd_expandAllThreads":
			case "cmd_collapseAllThreads":
			case "cmd_renameFolder":
			case "cmd_sendUnsentMsgs":
			case "cmd_openMessage":
      case "button_print":
			case "cmd_print":
			case "cmd_printSetup":
			case "cmd_saveAsFile":
			case "cmd_saveAsTemplate":
            case "cmd_properties":
			case "cmd_viewPageSource":
			case "cmd_setFolderCharset":
			case "cmd_reload":
      case "button_getNewMessages":
			case "cmd_getNewMessages":
      case "cmd_getMsgsForAuthAccounts":
			case "cmd_getNextNMessages":
			case "cmd_find":
			case "cmd_findAgain":
      case "cmd_search":
      case "button_mark":
			case "cmd_markAsRead":
			case "cmd_markAllRead":
			case "cmd_markThreadAsRead":
			case "cmd_markAsFlagged":
      case "cmd_label0":
      case "cmd_label1":
      case "cmd_label2":
      case "cmd_label3":
      case "cmd_label4":
      case "cmd_label5":
      case "button_file":
			case "cmd_file":
			case "cmd_emptyTrash":
			case "cmd_compactFolder":
			case "cmd_sortByThread":
  	  case "cmd_settingsOffline":
      case "cmd_close":
      case "cmd_selectThread":
				return true;
      case "cmd_downloadFlagged":
      case "cmd_downloadSelected":
      case "cmd_synchronizeOffline":
        return(CheckOnline());

      case "cmd_watchThread":
      case "cmd_killThread":
        return(isNewsURI(GetFirstSelectedMessage()));

			default:
				return false;
		}
	},

  isCommandEnabled: function(command)
  {
    var enabled = new Object();
    enabled.value = false;
    var checkStatus = new Object();
    
    if (IsFakeAccount()) 
      return false;

    // note, all commands that get fired on a single key need to check MailAreaHasFocus() as well
    switch ( command )
    {
      case "cmd_delete":
        UpdateDeleteCommand();
        // fall through
      case "button_delete":
        if (gDBView)
          gDBView.getCommandStatus(nsMsgViewCommandType.deleteMsg, enabled, checkStatus);
        return enabled.value;
      case "cmd_shiftDelete":
        if (gDBView)
          gDBView.getCommandStatus(nsMsgViewCommandType.deleteNoTrash, enabled, checkStatus);
        return enabled.value;
      case "cmd_killThread":
        return ((GetNumSelectedMessages() == 1) && MailAreaHasFocus() && IsViewNavigationItemEnabled());
      case "cmd_watchThread":
        if (MailAreaHasFocus() && (GetNumSelectedMessages() == 1) && gDBView)
          gDBView.getCommandStatus(nsMsgViewCommandType.toggleThreadWatched, enabled, checkStatus);
        return enabled.value;
      case "cmd_createFilterFromPopup":
        var loadedFolder = GetLoadedMsgFolder();
        if (!(loadedFolder && loadedFolder.server.canHaveFilters))
          return false;
      case "cmd_createFilterFromMenu":
        loadedFolder = GetLoadedMsgFolder();
        if (!(loadedFolder && loadedFolder.server.canHaveFilters) || !(IsMessageDisplayedInMessagePane()))
          return false;
      case "cmd_reply":
      case "button_reply":
      case "cmd_replySender":
      case "cmd_replyGroup":
      case "cmd_replyall":
      case "button_replyall":
      case "cmd_forward":
      case "button_forward":
      case "cmd_forwardInline":
      case "cmd_forwardAttachment":
      case "cmd_editAsNew":
      case "cmd_openMessage":
      case "button_print":
      case "cmd_print":
      case "cmd_saveAsFile":
      case "cmd_saveAsTemplate":
      case "cmd_viewPageSource":
      case "cmd_reload":
	      if ( GetNumSelectedMessages() > 0)
        {
          if (gDBView)
          {
             gDBView.getCommandStatus(nsMsgViewCommandType.cmdRequiringMsgBody, enabled, checkStatus);
              return enabled.value;
          }
        }
        return false;
      case "cmd_printSetup":
        return true;
      case "cmd_markAsFlagged":
      case "button_file":
      case "cmd_file":
        return (GetNumSelectedMessages() > 0 );
      case "button_mark":
      case "cmd_markAsRead":
      case "cmd_markThreadAsRead":
      case "cmd_label0":
      case "cmd_label1":
      case "cmd_label2":
      case "cmd_label3":
      case "cmd_label4":
      case "cmd_label5":
        return(MailAreaHasFocus() && GetNumSelectedMessages() > 0);
      case "button_next":
        return IsViewNavigationItemEnabled();
      case "cmd_nextMsg":
      case "cmd_nextUnreadMsg":
      case "cmd_nextUnreadThread":
      case "cmd_previousMsg":
      case "cmd_previousUnreadMsg":
        return (MailAreaHasFocus() && IsViewNavigationItemEnabled());
      case "cmd_markAllRead":
        return (MailAreaHasFocus() && IsFolderSelected());
      case "cmd_find":
      case "cmd_findAgain":
        return IsMessageDisplayedInMessagePane();
        break;
      case "cmd_search":
        return IsCanSearchMessagesEnabled();
      // these are enabled on when we are in threaded mode
      case "cmd_selectThread":
        if (GetNumSelectedMessages() <= 0) return false;
      case "cmd_expandAllThreads":
      case "cmd_collapseAllThreads":
        if (!gDBView) return false;
          return (gDBView.sortType == nsMsgViewSortType.byThread);
        break;
      case "cmd_nextFlaggedMsg":
      case "cmd_previousFlaggedMsg":
        return IsViewNavigationItemEnabled();
      case "cmd_viewAllMsgs":
      case "cmd_sortByThread":
      case "cmd_viewUnreadMsgs":
      case "cmd_viewThreadsWithUnread":
      case "cmd_viewWatchedThreadsWithUnread":
      case "cmd_viewIgnoredThreads":
      case "cmd_stop":
        return true;
      case "cmd_undo":
      case "cmd_redo":
          return SetupUndoRedoCommand(command);
      case "cmd_renameFolder":
        return IsRenameFolderEnabled();
      case "cmd_sendUnsentMsgs":
        return IsSendUnsentMsgsEnabled(null);
      case "cmd_properties":
        return IsPropertiesEnabled(command);
      case "button_getNewMessages":
      case "cmd_getNewMessages":
      case "cmd_getMsgsForAuthAccounts":
        return IsGetNewMessagesEnabled();
      case "cmd_getNextNMessages":
        return IsGetNextNMessagesEnabled();
      case "cmd_emptyTrash":
        return IsEmptyTrashEnabled();
      case "cmd_compactFolder":
        return IsCompactFolderEnabled();
      case "cmd_setFolderCharset":
        return IsFolderCharsetEnabled();
      case "cmd_close":
        return true;
      case "cmd_downloadFlagged":
        return(CheckOnline());
      case "cmd_downloadSelected":
        return(MailAreaHasFocus() && IsFolderSelected() && CheckOnline() && GetNumSelectedMessages() > 0);
      case "cmd_synchronizeOffline":
        return CheckOnline() && IsAccountOfflineEnabled();       
      case "cmd_settingsOffline":
        return (MailAreaHasFocus() && IsAccountOfflineEnabled());
      default:
        return false;
    }
    return false;
  },

	doCommand: function(command)
	{
    // if the user invoked a key short cut then it is possible that we got here for a command which is
    // really disabled. kick out if the command should be disabled.
    if (!this.isCommandEnabled(command)) return;
   
		switch ( command )
		{
			case "cmd_close":
				CloseMailWindow();
				break;
      case "button_getNewMessages":
			case "cmd_getNewMessages":
				MsgGetMessage();
				break;
      case "cmd_getMsgsForAuthAccounts":
        MsgGetMessagesForAllAuthenticatedAccounts();
        break;
			case "cmd_getNextNMessages":
				MsgGetNextNMessages();
				break;
			case "cmd_reply":
				MsgReplyMessage(null);
				break;
			case "cmd_replySender":
				MsgReplySender(null);
				break;
			case "cmd_replyGroup":
				MsgReplyGroup(null);
				break;
			case "cmd_replyall":
				MsgReplyToAllMessage(null);
				break;
			case "cmd_forward":
				MsgForwardMessage(null);
				break;
			case "cmd_forwardInline":
				MsgForwardAsInline(null);
				break;
			case "cmd_forwardAttachment":
				MsgForwardAsAttachment(null);
				break;
			case "cmd_editAsNew":
				MsgEditMessageAsNew();
				break;
      case "cmd_createFilterFromMenu":
        MsgCreateFilter();
        break;        
      case "cmd_createFilterFromPopup":
        break;// This does nothing because the createfilter is invoked from the popupnode oncommand.
			case "button_delete":
			case "cmd_delete":
        SetNextMessageAfterDelete();
        gDBView.doCommand(nsMsgViewCommandType.deleteMsg);
				break;
			case "cmd_shiftDelete":
        SetNextMessageAfterDelete();
        gDBView.doCommand(nsMsgViewCommandType.deleteNoTrash);
				break;
      case "cmd_killThread":
        /* kill thread kills the thread and then does a next unread */
      	GoNextMessage(nsMsgNavigationType.toggleThreadKilled, true);
        break;
      case "cmd_watchThread":
        gDBView.doCommand(nsMsgViewCommandType.toggleThreadWatched);
        break;
      case "button_next":
			case "cmd_nextUnreadMsg":
				MsgNextUnreadMessage();
				break;
			case "cmd_nextUnreadThread":
				MsgNextUnreadThread();
				break;
			case "cmd_nextMsg":
				MsgNextMessage();
				break;
			case "cmd_nextFlaggedMsg":
				MsgNextFlaggedMessage();
				break;
			case "cmd_previousMsg":
				MsgPreviousMessage();
				break;
			case "cmd_previousUnreadMsg":
				MsgPreviousUnreadMessage();
				break;
			case "cmd_previousFlaggedMsg":
				MsgPreviousFlaggedMessage();
				break;
			case "cmd_sortByThread":
				MsgSortByThread();
				break;
			case "cmd_viewAllMsgs":
      case "cmd_viewThreadsWithUnread":
      case "cmd_viewWatchedThreadsWithUnread":
			case "cmd_viewUnreadMsgs":
      case "cmd_viewIgnoredThreads":
				SwitchView(command);
				break;
			case "cmd_undo":
				messenger.Undo(msgWindow);
				break;
			case "cmd_redo":
				messenger.Redo(msgWindow);
				break;
			case "cmd_expandAllThreads":
                gDBView.doCommand(nsMsgViewCommandType.expandAll);
				break;
			case "cmd_collapseAllThreads":
                gDBView.doCommand(nsMsgViewCommandType.collapseAll);
				break;
			case "cmd_renameFolder":
				MsgRenameFolder();
				return;
			case "cmd_sendUnsentMsgs":
				MsgSendUnsentMsgs();
				return;
			case "cmd_openMessage":
                MsgOpenSelectedMessages();
				return;
			case "cmd_printSetup":
			  goPageSetup();
			  return;
			case "cmd_print":
				PrintEnginePrint();
				return;
			case "cmd_saveAsFile":
				MsgSaveAsFile();
				return;
			case "cmd_saveAsTemplate":
				MsgSaveAsTemplate();
				return;
			case "cmd_viewPageSource":
				MsgViewPageSource();
				return;
			case "cmd_setFolderCharset":
				MsgSetFolderCharset();
				return;
			case "cmd_reload":
				MsgReload();
				return;
			case "cmd_find":
				MsgFind();
				return;
			case "cmd_findAgain":
				MsgFindAgain();
				return;
            case "cmd_properties":
                MsgFolderProperties();
                return;
      case "cmd_search":
        MsgSearchMessages();
      case "button_mark":
			case "cmd_markAsRead":
				MsgMarkMsgAsRead(null);
				return;
			case "cmd_markThreadAsRead":
				MsgMarkThreadAsRead();
				return;
			case "cmd_markAllRead":
                gDBView.doCommand(nsMsgViewCommandType.markAllRead);
				return;
      case "cmd_stop":
        MsgStop();
        return;
			case "cmd_markAsFlagged":
				MsgMarkAsFlagged(null);
				return;
      case "cmd_label0":
        gDBView.doCommand(nsMsgViewCommandType.label0);
 				return;
      case "cmd_label1":
        gDBView.doCommand(nsMsgViewCommandType.label1);
        return; 
      case "cmd_label2":
        gDBView.doCommand(nsMsgViewCommandType.label2);
        return; 
      case "cmd_label3":
        gDBView.doCommand(nsMsgViewCommandType.label3);
        return; 
      case "cmd_label4":
        gDBView.doCommand(nsMsgViewCommandType.label4);
        return; 
      case "cmd_label5":
        gDBView.doCommand(nsMsgViewCommandType.label5);
        return; 
			case "cmd_emptyTrash":
				MsgEmptyTrash();
				return;
			case "cmd_compactFolder":
				MsgCompactFolder(true);
				return;
            case "cmd_downloadFlagged":
                MsgDownloadFlagged();
                break;
            case "cmd_downloadSelected":
                MsgDownloadSelected();
                break;
            case "cmd_synchronizeOffline":
                MsgSynchronizeOffline();
                break;
            case "cmd_settingsOffline":
                MsgSettingsOffline();
                break;
            case "cmd_selectThread":
                gDBView.doCommand(nsMsgViewCommandType.selectThread);
                break;
		}
	},
	
	onEvent: function(event)
	{
		// on blur events set the menu item texts back to the normal values
		if ( event == 'blur' )
        {
			goSetMenuValue('cmd_delete', 'valueDefault');
            goSetMenuValue('cmd_undo', 'valueDefault');
            goSetMenuValue('cmd_redo', 'valueDefault');
        }
	}
};

function MailAreaHasFocus()
{
  //Input and TextAreas should get access to the keys that cause these commands.
  //Currently if we don't do this then we will steal the key away and you can't type them
  //in these controls. This is a bug that should be fixed and when it is we can get rid of
  //this.
  var focusedElement = top.document.commandDispatcher.focusedElement;
  if (focusedElement) 
  {
    var name = focusedElement.localName.toLowerCase();
    return ((name != "input") && (name != "textarea"));
  }

  // check if the message pane has focus 
  // see bug #129988
  if (GetMessagePane() == WhichPaneHasFocus())
    return true;

  // if there is no focusedElement,
  // and the message pane doesn't have focus
  // then a mail area can't be focused
  // see bug #128101
  return false;
}

function GetNumSelectedMessages()
{
    try {
        return gDBView.numSelected;
    }
    catch (ex) {
        return 0;
    }
}

var gLastFocusedElement=null;

function FocusRingUpdate_Mail()
{
  // WhichPaneHasFocus() uses on top.document.commandDispatcher.focusedElement
  // to determine which pane has focus
  // if the focusedElement is null, we're here on a blur.
  // nsFocusController::Blur() calls nsFocusController::SetFocusedElement(null), 
  // which will update any commands listening for "focus".
  // we really only care about nsFocusController::Focus() happens, 
  // which calls nsFocusController::SetFocusedElement(element)
  var currentFocusedElement = WhichPaneHasFocus();
  if (!currentFocusedElement)
    return;
      
	if (currentFocusedElement != gLastFocusedElement) {
    currentFocusedElement.setAttribute("focusring", "true");
    
    if (gLastFocusedElement)
      gLastFocusedElement.removeAttribute("focusring");

    gLastFocusedElement = currentFocusedElement;

    // since we just changed the pane with focus we need to update the toolbar to reflect this
    UpdateMailToolbar("focus");
  }
}

function WhichPaneHasFocus()
{
  var threadTree = GetThreadTree();
  var searchInput = GetSearchInput();
  var folderTree = GetFolderTree();
  var messagePane = GetMessagePane();
    
  if (top.document.commandDispatcher.focusedWindow == GetMessagePaneFrame())
    return messagePane;

	var currentNode = top.document.commandDispatcher.focusedElement;	
	while (currentNode) {
    if (currentNode === threadTree ||
        currentNode === searchInput || 
        currentNode === folderTree ||
        currentNode === messagePane)
      return currentNode;
    			
		currentNode = currentNode.parentNode;
  }
	
	return null;
}

function SetupCommandUpdateHandlers()
{
	var widget;
	
	// folder pane
	widget = GetFolderTree();
	if ( widget )
		widget.controllers.appendController(FolderPaneController);
	
	// thread pane
	widget = GetThreadTree();
	if ( widget )
        widget.controllers.appendController(ThreadPaneController);
		
	top.controllers.insertControllerAt(0, DefaultController);
}

function IsSendUnsentMsgsEnabled(folderResource)
{
  var identity;
  try {
    if (folderResource) {
      // if folderResource is non-null, it is
      // resource for the "Unsent Messages" folder
      // we're here because we've done a right click on the "Unsent Messages"
      // folder (context menu)
      var msgFolder = folderResource.QueryInterface(Components.interfaces.nsIMsgFolder);
      return (msgFolder.getTotalMessages(false) > 0);
    }
    else {
      var folders = GetSelectedMsgFolders();
      if (folders.length > 0) {
        identity = getIdentityForServer(folders[0].server);
      }
    }
  }
  catch (ex) {
    dump("ex = " + ex + "\n");
    identity = null;
  }

  try {
    if (!identity) {
      var am = Components.classes["@mozilla.org/messenger/account-manager;1"].getService(Components.interfaces.nsIMsgAccountManager);
      identity = am.defaultAccount.defaultIdentity;
    }

    var msgSendlater = Components.classes["@mozilla.org/messengercompose/sendlater;1"].getService(Components.interfaces.nsIMsgSendLater);
    var unsentMsgsFolder = msgSendlater.getUnsentMessagesFolder(identity);
    return (unsentMsgsFolder.getTotalMessages(false) > 0);
  }
  catch (ex) {
    dump("ex = " + ex + "\n");
  }
  return false;
}

function IsRenameFolderEnabled()
{
    var folderTree = GetFolderTree();
    var selection = folderTree.treeBoxObject.selection;
    if (selection.count == 1)
    {
        var startIndex = {};
        var endIndex = {};
        selection.getRangeAt(0, startIndex, endIndex);
        var folderResource = GetFolderResource(folderTree, startIndex.value);
        var canRename = GetFolderAttribute(folderTree, folderResource, "CanRename") == "true";
        return canRename && isCommandEnabled("cmd_renameFolder");
    }
    else
        return false;
}

function IsCanSearchMessagesEnabled()
{
  var folderURI = GetSelectedFolderURI();
  var server = GetServer(folderURI);
  return server.canSearchMessages;
}
function IsFolderCharsetEnabled()
{
  return IsFolderSelected();
}

function IsPropertiesEnabled(command)
{
   try 
   {
      var serverType;
      var folderTree = GetFolderTree();
      var folderResource = GetSelectedFolderResource();
      
      serverType = GetFolderAttribute(folderTree, folderResource, "ServerType");
   
      switch (serverType)
      {
        case "none":
        case "imap":
        case "pop3":
          goSetMenuValue(command, 'valueFolder');
          break;
        case "nntp":
          goSetMenuValue(command, 'valueNewsgroup');
          break
        default:
          goSetMenuValue(command, 'valueGeneric');
      }
   }
   catch (ex) 
   {
      //properties menu failure
   }
  return IsFolderSelected();
  
}

function IsViewNavigationItemEnabled()
{
	return IsFolderSelected();
}

function IsFolderSelected()
{
    var folderTree = GetFolderTree();
    var selection = folderTree.treeBoxObject.selection;
    if (selection.count == 1)
    {
        var startIndex = {};
        var endIndex = {};
        selection.getRangeAt(0, startIndex, endIndex);
        var folderResource = GetFolderResource(folderTree, startIndex.value);
        return GetFolderAttribute(folderTree, folderResource, "IsServer") != "true";
    }
    else
        return false;
}

function IsMessageDisplayedInMessagePane()
{
	return (!IsThreadAndMessagePaneSplitterCollapsed() && (GetNumSelectedMessages() > 0));
}

function MsgDeleteFolder()
{
    var folderTree = GetFolderTree();
    var selectedFolders = GetSelectedMsgFolders();
    for (var i = 0; i < selectedFolders.length; i++)
    {
        var selectedFolder = selectedFolders[i];
        var folderResource = selectedFolder.QueryInterface(Components.interfaces.nsIRDFResource);
        var specialFolder = GetFolderAttribute(folderTree, folderResource, "SpecialFolder");
        if (specialFolder != "Inbox" && specialFolder != "Trash")
        {
            var protocolInfo = Components.classes["@mozilla.org/messenger/protocol/info;1?type=" + selectedFolder.server.type].getService(Components.interfaces.nsIMsgProtocolInfo);

            // do not allow deletion of special folders on imap accounts
            if ((specialFolder == "Sent" || 
                specialFolder == "Drafts" || 
                specialFolder == "Templates") &&
                !protocolInfo.specialFoldersDeletionAllowed)
            {
                var errorMessage = gMessengerBundle.getFormattedString("specialFolderDeletionErr",
                                                    [specialFolder]);
                var specialFolderDeletionErrTitle = gMessengerBundle.getString("specialFolderDeletionErrTitle");
                promptService.alert(window, specialFolderDeletionErrTitle, errorMessage);
                continue;
            }   
            else if (isNewsURI(folderResource.Value))
            {
                var unsubscribe = ConfirmUnsubscribe(selectedFolder);
                if (unsubscribe)
                    UnSubscribe(selectedFolder);
            }
            else
            {
                var parentResource = selectedFolder.parent.QueryInterface(Components.interfaces.nsIRDFResource);
                messenger.DeleteFolders(GetFolderDatasource(), parentResource, folderResource);
            }
        }
    }
}

function SetFocusThreadPaneIfNotOnMessagePane()
{
  var focusedElement = WhichPaneHasFocus();

  if((focusedElement != GetThreadTree()) &&
     (focusedElement != GetMessagePane()))
     SetFocusThreadPane();
}

// 3pane related commands.  Need to go in own file.  Putting here for the moment.
function MsgNextMessage()
{
	GoNextMessage(nsMsgNavigationType.nextMessage, false );
}

function MsgNextUnreadMessage()
{
	GoNextMessage(nsMsgNavigationType.nextUnreadMessage, true);
}
function MsgNextFlaggedMessage()
{
	GoNextMessage(nsMsgNavigationType.nextFlagged, true);
}

function MsgNextUnreadThread()
{
	//First mark the current thread as read.  Then go to the next one.
	MsgMarkThreadAsRead();
	GoNextMessage(nsMsgNavigationType.nextUnreadThread, true);
}

function MsgPreviousMessage()
{
    GoNextMessage(nsMsgNavigationType.previousMessage, false);
}

function MsgPreviousUnreadMessage()
{
	GoNextMessage(nsMsgNavigationType.previousUnreadMessage, true);
}

function MsgPreviousFlaggedMessage()
{
	GoNextMessage(nsMsgNavigationType.previousFlagged, true);
}

function GetFolderNameFromUri(uri, tree)
{
	var folderResource = RDF.GetResource(uri);

	var db = tree.database;

	var nameProperty = RDF.GetResource('http://home.netscape.com/NC-rdf#Name');

	var nameResult;
	try {
		nameResult = db.GetTarget(folderResource, nameProperty , true);
	}
	catch (ex) {
		return "";
	}

	nameResult = nameResult.QueryInterface(Components.interfaces.nsIRDFLiteral);
	return nameResult.Value;
}

/* XXX hiding the search bar while it is focus kills the keyboard so we focus the thread pane */
function SearchBarToggled()
{
  var searchBox = document.getElementById('searchBox');
  if (searchBox)
  {
    var attribValue = searchBox.getAttribute("hidden") ;
    if (attribValue == "true")
    {
      /*come out of quick search view */
      if (gDBView && gDBView.isSearchView)
        onClearSearch();
    }
    else
    {
      /*we have to initialize searchInput because we cannot do it when searchBox is hidden */
      var searchInput = GetSearchInput();
      searchInput.value="";
    }
  }

  for (var currentNode = top.document.commandDispatcher.focusedElement; currentNode; currentNode = currentNode.parentNode) {
    if (currentNode.getAttribute("hidden") == "true") {
      SetFocusThreadPane();
      return;
    }
  }
}

function SwitchPaneFocus(event)
{
  var focusedElement = WhichPaneHasFocus();
  var folderTree = GetFolderTree();
  var threadTree = GetThreadTree();
  var searchInput = GetSearchInput();
  var messagePane = GetMessagePane();

  if (event && event.shiftKey)
  {
    if (focusedElement == threadTree && searchInput.parentNode.getAttribute('hidden') != 'true')
      searchInput.focus();
    else if ((focusedElement == threadTree || focusedElement == searchInput) && !IsFolderPaneCollapsed())
      folderTree.focus();
    else if (focusedElement != messagePane && !IsThreadAndMessagePaneSplitterCollapsed())
      SetFocusMessagePane();
    else 
      threadTree.focus();
  }
  else
  {
    if (focusedElement == searchInput)
      threadTree.focus();
    else if (focusedElement == threadTree && !IsThreadAndMessagePaneSplitterCollapsed())
      SetFocusMessagePane();
    else if (focusedElement != folderTree && !IsFolderPaneCollapsed())
      folderTree.focus();
    else if (searchInput.parentNode.getAttribute('hidden') != 'true')
      searchInput.focus();
    else
      threadTree.focus();
  }
}

function SetFocusFolderPane()
{
    var folderTree = GetFolderTree();
    folderTree.focus();
}

function SetFocusThreadPane()
{
    var threadTree = GetThreadTree();
    threadTree.focus();
}

function SetFocusMessagePane()
{
    var messagePaneFrame = GetMessagePaneFrame();
    messagePaneFrame.focus();
}

function is_collapsed(element) 
{
  return (element.getAttribute('state') == 'collapsed');
}

function isCommandEnabled(cmd)
{
  var selectedFolders = GetSelectedMsgFolders();
  var numFolders = selectedFolders.length;
  if(numFolders !=1)
    return false;

  var folder = selectedFolders[0];
  if (!folder)
    return false;
  else
    return folder.isCommandEnabled(cmd);

}

function IsFakeAccount() {
  try {
    var folderResource = GetSelectedFolderResource();
    return (folderResource.Value == "http://home.netscape.com/NC-rdf#PageTitleFakeAccount");
  }
  catch(ex) {
  }
  return false;
}


