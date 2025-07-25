/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Sammy Ford (sford@swbell.net)
 *    Dan Haddix (dan6992@hotmail.com)
 *    John Ratke (jratke@owc.net)
 *    Ryan Cassin (rcassin@supernova.org)
 *    Daniel Glazman (glazman@netscape.com)
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

/* Main Composer window UI control */

var editorShell;
var documentModified;
var prefAuthorString = "";
var NormalMode = 1;
var PreviewMode = 2;
// These must match enums in nsIEditorShell.idl:
var DisplayModePreview = 0;
var DisplayModeNormal = 1;
var DisplayModeAllTags = 2;
var DisplayModeSource = 3;
var PreviousNonSourceDisplayMode = 1;
var gEditorDisplayMode = 1;  // Normal Editor mode
var WebCompose = false;     // Set true for Web Composer, leave false for Messenger Composer
var docWasModified = false;  // Check if clean document, if clean then unload when user "Opens"
var gContentWindow = 0;
var gSourceContentWindow = 0;
var gHTMLSourceChanged = false;
var gContentWindowDeck;
var gFormatToolbar;
var gFormatToolbarHidden = false;
var gFormatToolbarCollapsed;
var gEditModeBar;
var gNormalModeButton;
var gTagModeButton;
var gSourceModeButton;
var gPreviewModeButton;
var gIsHTMLEditor = false;
var gColorObj = new Object();
var gDefaultTextColor = "";
var gDefaultBackgroundColor = "";
var gCSSPrefListener;
var gPrefs;

// These must be kept in synch with the XUL <options> lists
var gFontSizeNames = new Array("xx-small","x-small","small","medium","large","x-large","xx-large");

const nsIFilePicker = Components.interfaces.nsIFilePicker;

function nsButtonPrefListener()
{
  try {
    var pbi = pref.QueryInterface(Components.interfaces.nsIPrefBranchInternal);
    pbi.addObserver(this.domain, this, false);
  } catch(ex) {
    dump("Failed to observe prefs: " + ex + "\n");
  }
}

// implements nsIObserver
nsButtonPrefListener.prototype =
{
  domain: "editor.use_css",
  observe: function(subject, topic, prefName)
  {
    // verify that we're changing a button pref
    if (topic != "nsPref:changed") return;
    if (prefName.substr(0, this.domain.length) != this.domain) return;

    var button = document.getElementById("cmd_highlight");
    var mixedObj = new Object();
    if (button) {
      var prefs = GetPrefs();
      var useCSS = prefs.getBoolPref(prefName);
      var htmlEditor = editorShell.editor.QueryInterface(Components.interfaces.nsIHTMLEditor);
      if (useCSS && htmlEditor) {
        button.removeAttribute("disabled");
        var state = htmlEditor.getHighlightColorState(mixedObj);
        button.setAttribute("state", state);
      }      
      else {
        button.setAttribute("disabled", "true");
        button.setAttribute("state", "transparent");
      }
      if (htmlEditor) {
        htmlEditor.isCSSEnabled = useCSS;
      }
    }
  }
}

function AfterHighlightColorChange()
{
  var button = document.getElementById("cmd_highlight");
  var mixedObj = new Object();
  if (button) {
    var state = editorShell.editor.QueryInterface(Components.interfaces.nsIHTMLEditor).getHighlightColorState(mixedObj);
    button.setAttribute("state", state);
    onHighlightColorChange();
  }      
}

function EditorOnLoad()
{
    // See if argument was passed.
    if ( window.arguments && window.arguments[0] ) {
        // Opened via window.openDialog with URL as argument.
        // Put argument where EditorStartup expects it.
        document.getElementById( "args" ).setAttribute( "value", window.arguments[0] );
    }

    // get default character set if provided
    if ("arguments" in window && window.arguments.length > 1 && window.arguments[1]) {
      if (window.arguments[1].indexOf("charset=") != -1) {
        var arrayArgComponents = window.arguments[1].split("=");
        if (arrayArgComponents) {
          // Put argument where EditorStartup expects it.
          document.getElementById( "args" ).setAttribute("charset", arrayArgComponents[1]);
        }
      }
    }

    WebCompose = true;
    window.tryToClose = EditorCanClose;

    // Continue with normal startup.
    EditorStartup('html', document.getElementById("content-frame"));
}

function TextEditorOnLoad()
{
    // See if argument was passed.
    if ( window.arguments && window.arguments[0] ) {
        // Opened via window.openDialog with URL as argument.
        // Put argument where EditorStartup expects it.
        document.getElementById( "args" ).setAttribute( "value", window.arguments[0] );
    }
    // Continue with normal startup.
    EditorStartup('text', document.getElementById("content-frame"));
}

// This should be called by all editor users when they close their window
//  or other similar "done with editor" actions, like recycling a Mail Composer window.
function EditorCleanup()
{
  SwitchInsertCharToAnotherEditorOrClose();
}

function PageIsEmptyAndUntouched()
{
  return (editorShell != null) && editorShell.documentIsEmpty && 
         !docWasModified && !gHTMLSourceChanged;
}

function IsEditorContentHTML()
{
  return (editorShell.contentsMIMEType == "text/html");
}

function IsInHTMLSourceMode()
{
  return (gEditorDisplayMode == DisplayModeSource);
}

// are we editing HTML (i.e. neither in HTML source mode, nor editing a text file)
function IsEditingRenderedHTML()
{
    return IsEditorContentHTML() && !IsInHTMLSourceMode();
}


var DocumentReloadListener =
{
  NotifyDocumentCreated: function() {},
  NotifyDocumentWillBeDestroyed: function() {},

  NotifyDocumentStateChanged:function( isNowDirty )
  {
    var charset = editorShell.GetDocumentCharacterSet();

    // unregister the listener to prevent multiple callbacks
    editorShell.UnregisterDocumentStateListener( DocumentReloadListener );

    // update the META charset with the current presentation charset
    editorShell.SetDocumentCharacterSet(charset);
  }
};

function addEditorClickEventListener()
{
  try {
    var bodyelement = GetBodyElement();
    if (bodyelement)
      bodyelement.addEventListener("click", EditorClick, false);
  } catch (e) {}
}

var MessageComposeDocumentStateListener =
{
  NotifyDocumentCreated: function()
  {
    addEditorClickEventListener();
  },

  NotifyDocumentWillBeDestroyed: function()
  {
    // note that the editorshell seems to be gone at this point 
    // so we don't have a way to remove the click listener.
    // hopefully it is being cleaned up with all listeners
  },

  NotifyDocumentStateChanged:function()
  {
  }
};

function isPlaintextEditor()
{
  var editorflags;
  try {
    editorflags = window.editorShell.editor.flags;
  } catch(e) {}

  return (editorflags & Components.interfaces.nsIPlaintextEditor.eEditorPlaintextMask);
}

// This is called when the real editor document is created,
// before it's loaded.
var DocumentStateListener =
{
  NotifyDocumentCreated: function()
  {
    // Call EditorSetDefaultPrefsAndDoctype first so it gets the default author before initing toolbars
    EditorSetDefaultPrefsAndDoctype();
    EditorInitToolbars();
    BuildRecentMenu(true);      // Build the recent files menu and save to prefs

    // Just for convenience
    gContentWindow = window._content;
    gContentWindow.focus();

    // udpate menu items now that we have an editor to play with
    // Note: This must be AFTER gContentWindow.focus();
    window.updateCommands("create");

    if (!("InsertCharWindow" in window))
      window.InsertCharWindow = null;
    
    // We must wait until document is created to get proper Url
    // (Windows may load with local file paths)
    SetSaveAndPublishUI(GetDocumentUrl());

    // Add mouse click watcher if right type of editor
    if (!isPlaintextEditor())
      addEditorClickEventListener();
  },

    // note that the editorshell seems to be gone at this point 
    // so we don't have a way to remove the click listener.
    // hopefully it is being cleaned up with all listeners
  NotifyDocumentWillBeDestroyed: function() {},

  NotifyDocumentStateChanged:function( isNowDirty )
  {
    /* Notify our dirty detector so this window won't be closed if
       another document is opened */
    if (isNowDirty)
      docWasModified = true;

    // hack! Should not need this updateCommands, but there is some controller
    //  bug that this works around. ??
    // comment out the following line because it cause 41573 IME problem on Mac
    //gContentWindow.focus();
    window.updateCommands("create");
    window.updateCommands("save");
  }
};

function EditorStartup(editorType, editorElement)
{
  gIsHTMLEditor = (editorType == "html");

  if (gIsHTMLEditor)
  {
    gSourceContentWindow = document.getElementById("content-source");

    gEditModeBar       = document.getElementById("EditModeToolbar");
    gNormalModeButton  = document.getElementById("NormalModeButton");
    gTagModeButton     = document.getElementById("TagModeButton");
    gSourceModeButton  = document.getElementById("SourceModeButton");
    gPreviewModeButton = document.getElementById("PreviewModeButton");

    // mark first tab as selected
    document.getElementById("EditModeTabs").selectedTab = gNormalModeButton;

    // XUL elements we use when switching from normal editor to edit source
    gContentWindowDeck = document.getElementById("ContentWindowDeck");
    gFormatToolbar = document.getElementById("FormatToolbar");
  }

  // store the editor shell in the window, so that child windows can get to it.
  editorShell = editorElement.editorShell;        // this pattern exposes a JS/XBL bug that causes leaks
  editorShell.editorType = editorType;

  editorShell.webShellWindow = window;
  editorShell.contentWindow = window._content;

  // set up our global prefs object
  GetPrefsService();

  // Startup also used by other editor users, such as Message Composer
  EditorSharedStartup();

  // Commands specific to the Composer Application window,
  //  (i.e., not embedded editors)
  //  such as file-related commands, HTML Source editing, Edit Modes...
  SetupComposerWindowCommands();

  gCSSPrefListener = new nsButtonPrefListener();

  // Get url for editor content and load it.
  // the editor gets instantiated by the editor shell when the URL has finished loading.
  var url = document.getElementById("args").getAttribute("value");
  var charset = document.getElementById("args").getAttribute("charset");
  if (charset) editorShell.SetDocumentCharacterSet(charset);

  editorShell.LoadUrl(url);
}

// This is also called by Message Composer
function EditorSharedStartup()
{
  // Just for convenience
  gContentWindow = window._content;

  // set up JS-implemented commands for Text or HTML editing
  switch (editorShell.editorType)
  {
      case "html":
      case "htmlmail":
        SetupHTMLEditorCommands();
        editorShell.contentsMIMEType = "text/html";
        if (editorShell.editorType == "htmlmail")
          editorShell.RegisterDocumentStateListener( MessageComposeDocumentStateListener );
        else
          // add a listener to be called when document is really done loading
          editorShell.RegisterDocumentStateListener( DocumentStateListener );
        break;

      case "text":
        // add listener when document is done loading (for toolbar setup)
        editorShell.RegisterDocumentStateListener( DocumentStateListener );
        // continue below

      case "textmail":
        SetupTextEditorCommands();
        editorShell.contentsMIMEType = "text/plain";
        break;

      default:
        dump("INVALID EDITOR TYPE: "+editorShell.editorType+"\n");
        SetupTextEditorCommands();
        editorShell.contentsMIMEType = "text/plain";
        break;
  }
  var isMac = (GetOS() == gMac);

  // Set platform-specific hints for how to select cells
  // Mac uses "Cmd", all others use "Ctrl"
  var tableKey = GetString(isMac ? "XulKeyMac" : "TableSelectKey");
  var dragStr = tableKey+GetString("Drag");
  var clickStr = tableKey+GetString("Click");

  var delStr = GetString(isMac ? "Clear" : "Del");

  SafeSetAttribute("menu_SelectCell", "acceltext", clickStr);
  SafeSetAttribute("menu_SelectRow", "acceltext", dragStr);
  SafeSetAttribute("menu_SelectColumn", "acceltext", dragStr);
  SafeSetAttribute("menu_SelectAllCells", "acceltext", dragStr);
  // And add "Del" or "Clear"
  SafeSetAttribute("menu_DeleteCellContents", "acceltext", delStr);

  // Set text for indent, outdent keybinding

  // hide UI that we don't have components for
  RemoveInapplicableUIElements();

  gPrefs = GetPrefs();

  // Use browser colors as initial values for editor's default colors
  var BrowserColors = GetDefaultBrowserColors();
  if (BrowserColors)
  {
    gDefaultTextColor = BrowserColors.TextColor;
    gDefaultBackgroundColor = BrowserColors.BackgroundColor;
  }

  // For new window, no default last-picked colors
  gColorObj.LastTextColor = "";
  gColorObj.LastBackgroundColor = "";
  gColorObj.LastHighlightColor = "";
}

// This method is only called by Message composer when recycling a compose window
function EditorResetFontAndColorAttributes()
{
  document.getElementById("cmd_fontFace").setAttribute("state", "");
  window.editorShell.RemoveTextProperty("font", "color");
  window.editorShell.RemoveTextProperty("font", "bgcolor");
  window.editorShell.RemoveTextProperty("font", "size");
  window.editorShell.RemoveTextProperty("small", "");
  window.editorShell.RemoveTextProperty("big", "");
  var bodyelement = GetBodyElement();
  if (bodyelement)
  {
    bodyelement.removeAttribute("text");
    bodyelement.removeAttribute("bgcolor");
    bodyelement.removeAttribute("link");
    bodyelement.removeAttribute("alink");
    bodyelement.removeAttribute("vlink");
    bodyelement.removeAttribute("background");
  }
  gColorObj.LastTextColor = "";
  gColorObj.LastBackgroundColor = "";
  gColorObj.LastHighlightColor = "";
  document.getElementById("cmd_fontColor").setAttribute("state", "");
  document.getElementById("cmd_backgroundColor").setAttribute("state", "");
  UpdateDefaultColors();
}

function _EditorNotImplemented()
{
  dump("Function not implemented\n");
}

function EditorShutdown()
{
    // nothing to do. editorShell->Shutdown is called by the nsEditorBoxObject
}

function SafeSetAttribute(nodeID, attributeName, attributeValue)
{
    var theNode = document.getElementById(nodeID);
    if (theNode)
        theNode.setAttribute(attributeName, attributeValue);
}

function DocumentHasBeenSaved()
{
  var fileurl = "";
  try {
    fileurl = GetDocumentUrl();
  } catch (e) {
    return false;
  }

  if (!fileurl || IsUrlAboutBlank(fileurl))
    return false;

  // We have a file URL already
  return true;
}

function CheckAndSaveDocument(command, allowDontSave)
{
  try {
    // if we don't have an editorShell or an editorDocument, bail
    if (!window.editorShell && !window.editorShell.editorDocument)
      return true;
  } catch (e) { return true; }

  var document = editorShell.editorDocument;
  if (!editorShell.documentModified && !gHTMLSourceChanged)
    return true;

  // call window.focus, since we need to pop up a dialog
  // and therefore need to be visible (to prevent user confusion)
  window.focus();  

  var scheme = GetScheme(GetDocumentUrl());
  var doPublish = (scheme && scheme != "file");

  var strID;
  switch (command)
  {
    case "cmd_close":
      strID = "BeforeClosing";
      break;
    case "cmd_preview":
      strID = "BeforePreview";
      break;
    case "cmd_editSendPage":
      strID = "SendPageReason";
      break;
    case "cmd_validate":
      strID = "BeforeValidate";
      break;
  }
    
  var reasonToSave = strID ? GetString(strID) : "";

  var title = window.editorShell.editorDocument.title;
  if (!title)
    title = GetString("untitled");

  var dialogTitle = GetString(doPublish ? "PublishPage" : "SaveDocument");
  var dialogMsg = GetString("SaveFilePrompt");
  dialogMsg = (dialogMsg.replace(/%title%/,title)).replace(/%reason%/,reasonToSave);

  var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService();
  promptService = promptService.QueryInterface(Components.interfaces.nsIPromptService);

  var result = {value:0};
  var promptFlags = promptService.BUTTON_TITLE_CANCEL * promptService.BUTTON_POS_1;
  var button1Title = null;
  var button3Title = null;

  if (doPublish)
  {
    promptFlags += promptService.BUTTON_TITLE_IS_STRING * promptService.BUTTON_POS_0;
    button1Title = GetString("Publish");
    button3Title = GetString("DontPublish");    
  }
  else
  {
    promptFlags += promptService.BUTTON_TITLE_SAVE * promptService.BUTTON_POS_0;
  }

  // If allowing "Don't..." button, add that
  if (allowDontSave)
    promptFlags += doPublish ?
        (promptService.BUTTON_TITLE_IS_STRING * promptService.BUTTON_POS_2)
        : (promptService.BUTTON_TITLE_DONT_SAVE * promptService.BUTTON_POS_2);
  
  result = promptService.confirmEx(window, dialogTitle, dialogMsg, promptFlags,
                          button1Title, null, button3Title, null, {value:0});

  if (result == 0)
  {
    // Save, but first finish HTML source mode
    if (gHTMLSourceChanged)
      FinishHTMLSource();

    if (doPublish)
    {
      // We save the command the user wanted to do in a global
      // and return as if user canceled because publishing is asynchronous
      // This command will be fired when publishing finishes
      gCommandAfterPublishing = command;
      goDoCommand("cmd_publish");
      return false;
    }

    // Save to local disk
    var success = SaveDocument(false, false, editorShell.contentsMIMEType);
    return success;
  }

  if (result == 2) // "Don't Save"
    return true;

  // Default or result == 1 (Cancel)
  return false;
}

// --------------------------- File menu ---------------------------


// used by openLocation. see openLocation.js for additional notes.
function delayedOpenWindow(chrome, flags, url)
{
  dump("setting timeout\n");
  setTimeout("window.openDialog('"+chrome+"','_blank','"+flags+"','"+url+"')", 10);
}

function EditorNewPlaintext()
{
  window.openDialog( "chrome://editor/content/TextEditorAppShell.xul",
                     "_blank",
                     "chrome,dialog=no,all",
                     "about:blank");
}

// Check for changes to document and allow saving before closing
// This is hooked up to the OS's window close widget (e.g., "X" for Windows)
function EditorCanClose()
{
  // Returns FALSE only if user cancels save action

  // "true" means allow "Don't Save" button
  var canClose = CheckAndSaveDocument("cmd_close", true);

  // This is our only hook into closing via the "X" in the caption
  //   or "Quit" (or other paths?)
  //   so we must shift association to another
  //   editor or close any non-modal windows now
  if (canClose && "InsertCharWindow" in window && window.InsertCharWindow)
    SwitchInsertCharToAnotherEditorOrClose();

  return canClose;
}

// --------------------------- View menu ---------------------------

function EditorSetDocumentCharacterSet(aCharset)
{
  if(editorShell)
  {
    editorShell.SetDocumentCharacterSet(aCharset);
    var docUrl = GetDocumentUrl();
    if( !IsUrlAboutBlank(docUrl))
    {
      // reloading the document will reverse any changes to the META charset, 
      // we need to put them back in, which is achieved by a dedicated listener
      editorShell.RegisterDocumentStateListener( DocumentReloadListener );
      editorShell.LoadUrl(docUrl);
    }
  }
}

// ------------------------------------------------------------------
function updateCharsetPopupMenu(menuPopup)
{
  if(editorShell.documentModified && !editorShell.documentIsEmpty)
  {
    for (var i = 0; i < menuPopup.childNodes.length; i++)
    {
      var menuItem = menuPopup.childNodes[i];
      menuItem.setAttribute('disabled', 'true');
    }
  }
}

// --------------------------- Text style ---------------------------

function EditorSetTextProperty(property, attribute, value)
{
  editorShell.SetTextProperty(property, attribute, value);
  gContentWindow.focus();
}

function onParagraphFormatChange(paraMenuList, commandID)
{
  if (!paraMenuList)
    return;

  var commandNode = document.getElementById(commandID);
  var state = commandNode.getAttribute("state");

  // force match with "normal"
  if (state == "body")
    state = "";

  if (state == "mixed")
  {
    //Selection is the "mixed" ( > 1 style) state
    paraMenuList.selectedItem = null;
    paraMenuList.setAttribute("label",GetString('Mixed'));
  }
  else
  {
    var menuPopup = document.getElementById("ParagraphPopup");
    var menuItems = menuPopup.childNodes;
    for (var i=0; i < menuItems.length; i++)
    {
      var menuItem = menuItems.item(i);
      if ("value" in menuItem && menuItem.value == state)
      {
        paraMenuList.selectedItem = menuItem;
        break;
      }
    }
  }
}

function doStatefulCommand(commandID, newState)
{
  var commandNode = document.getElementById(commandID);
  if (commandNode)
      commandNode.setAttribute("state", newState);
  gContentWindow.focus();   // needed for command dispatch to work
  goDoCommand(commandID);
}

function onFontFaceChange(fontFaceMenuList, commandID)
{
  var commandNode = document.getElementById(commandID);
  var state = commandNode.getAttribute("state");

  if (state == "mixed")
  {
    //Selection is the "mixed" ( > 1 style) state
    fontFaceMenuList.selectedItem = null;
    fontFaceMenuList.setAttribute("label",GetString('Mixed'));
  }
  else
  {
    var menuPopup = document.getElementById("FontFacePopup");
    var menuItems = menuPopup.childNodes;
    for (var i=0; i < menuItems.length; i++)
    {
      var menuItem = menuItems.item(i);
      if (menuItem.getAttribute("label") && ("value" in menuItem && menuItem.value.toLowerCase() == state.toLowerCase()))
      {
        fontFaceMenuList.selectedItem = menuItem;
        break;
      }
    }
  }
}

function EditorSelectFontSize()
{
  var select = document.getElementById("FontSizeSelect");
  if (select)
  {
    if (select.selectedIndex == -1)
      return;

    EditorSetFontSize(gFontSizeNames[select.selectedIndex]);
  }
}

function onFontSizeChange(fontSizeMenulist, commandID)
{
  // If we don't match anything, set to "0 (normal)"
  var newIndex = 2;
  var size = fontSizeMenulist.getAttribute("size");
  if ( size == "mixed")
  {
    // No single type selected
    newIndex = -1;
  }
  else
  {
    for (var i = 0; i < gFontSizeNames.length; i++)
    {
      if( gFontSizeNames[i] == size )
      {
        newIndex = i;
        break;
      }
    }
  }
  if (fontSizeMenulist.selectedIndex != newIndex)
    fontSizeMenulist.selectedIndex = newIndex;
}

function EditorSetFontSize(size)
{
  if( size == "0" || size == "normal" ||
      size == "medium" )
  {
    editorShell.RemoveTextProperty("font", "size");
    // Also remove big and small,
    //  else it will seem like size isn't changing correctly
    editorShell.RemoveTextProperty("small", "");
    editorShell.RemoveTextProperty("big", "");
  } else {
    // Temp: convert from new CSS size strings to old HTML size strings
    switch (size)
    {
      case "xx-small":
      case "x-small":
        size = "-2";
        break;
      case "small":
        size = "-1";
        break;
      case "large":
        size = "+1";
        break;
      case "x-large":
        size = "+2";
        break;
      case "xx-large":
        size = "+3";
        break;
    }
    editorShell.SetTextProperty("font", "size", size);
  }
  gContentWindow.focus();
}

function initFontFaceMenu(menuPopup)
{
  if (menuPopup)
  {
    var children = menuPopup.childNodes;
    if (!children) return;

    var firstHas = new Object;
    var anyHas = new Object;
    var allHas = new Object;
    allHas.value = false;

    // we need to set or clear the checkmark for each menu item since the selection
    // may be in a new location from where it was when the menu was previously opened

    // Fixed width (second menu item) is special case: old TT ("teletype") attribute
    editorShell.GetTextProperty("tt", "", "", firstHas, anyHas, allHas);
    children[1].setAttribute("checked", allHas.value);
    var fontWasFound = anyHas.value;

    // Skip over default, TT, and separator
    for (var i = 3; i < children.length; i++)
    {
      var menuItem = children[i];
      var faceType = menuItem.getAttribute("value");

      if (faceType)
      {
        editorShell.GetTextProperty("font", "face", faceType, firstHas, anyHas, allHas);

        // Check the item only if all of selection has the face...
        menuItem.setAttribute("checked", allHas.value);
        // ...but remember if ANY part of the selection has it
        fontWasFound |= anyHas.value;
      }
    }
    // Check the default item if no other item was checked
    // note that no item is checked in the case of "mixed" selection
    children[0].setAttribute("checked", !fontWasFound);
  }
}

function initFontSizeMenu(menuPopup)
{
  if (menuPopup)
  {
    var children = menuPopup.childNodes;
    if (!children) return;

    var firstHas = new Object;
    var anyHas = new Object;
    var allHas = new Object;
    allHas.value = false;

    var sizeWasFound = false;

    // we need to set or clear the checkmark for each menu item since the selection
    // may be in a new location from where it was when the menu was previously opened

    // First 2 items add <small> and <big> tags
    // While it would be better to show the number of levels,
    //  at least this tells user if either of them are set
    var menuItem = children[0];
    if (menuItem)
    {
      editorShell.GetTextProperty("small", "", "", firstHas, anyHas, allHas);
      menuItem.setAttribute("checked", allHas.value);
      sizeWasFound = anyHas.value;
    }

    menuItem = children[1];
    if (menuItem)
    {
      editorShell.GetTextProperty("big", "", "", firstHas, anyHas, allHas);
      menuItem.setAttribute("checked", allHas.value);
      sizeWasFound |= anyHas.value;
    }

    // Fixed size items start after menu separator
    var menuIndex = 3;
    // Index of the medium (default) item
    var mediumIndex = 5;

    // Scan through all supported "font size" attribute values
    for (var i = -2; i <= 3; i++)
    {
      menuItem = children[menuIndex];

      // Skip over medium since it'll be set below.
      // If font size=0 is actually set, we'll toggle it off below if
      // we enter this loop in this case.
      if (menuItem && (i != 0))
      {
        var sizeString = (i <= 0) ? String(i) : ("+" + String(i));
        editorShell.GetTextProperty("font", "size", sizeString, firstHas, anyHas, allHas);
        // Check the item only if all of selection has the size...
        menuItem.setAttribute("checked", allHas.value);
        // ...but remember if ANY of of selection had size set
        sizeWasFound |= anyHas.value;
      }
      menuIndex++;
    }

    // if no size was found, then check default (medium)
    // note that no item is checked in the case of "mixed" selection
    children[mediumIndex].setAttribute("checked", !sizeWasFound);
  }
 }

function onHighlightColorChange()
{
  var commandNode = document.getElementById("cmd_highlight");
  if (commandNode)
  {
    var color = commandNode.getAttribute("state");
    var button = document.getElementById("HighlightColorButton");
    if (button)
    {
      // No color set - get color set on page or other defaults
      if (!color)
        color = "transparent" ;

      button.setAttribute("style", "background-color:"+color+" !important");
    }
  }
}
function onFontColorChange()
{
  var commandNode = document.getElementById("cmd_fontColor");
  if (commandNode)
  {
    var color = commandNode.getAttribute("state");
    var button = document.getElementById("TextColorButton");
    if (button)
    {
      // No color set - get color set on page or other defaults
      if (!color)
        color = gDefaultTextColor;

      button.setAttribute("style", "background-color:"+color);
    }
  }
}

function onBackgroundColorChange()
{
  var commandNode = document.getElementById("cmd_backgroundColor");
  if (commandNode)
  {
    var color = commandNode.getAttribute("state");
    var button = document.getElementById("BackgroundColorButton");
    if (button)
    {
      if (!color)
        color = gDefaultBackgroundColor;

      button.setAttribute("style", "background-color:"+color);
    }
  }
}

// Call this when user changes text and/or background colors of the page
function UpdateDefaultColors()
{
  var BrowserColors = GetDefaultBrowserColors();
  var bodyelement = GetBodyElement();
  var defTextColor = gDefaultTextColor;
  var defBackColor = gDefaultBackgroundColor;

  if (bodyelement)
  {
    var color = bodyelement.getAttribute("text");
    if (color)
      gDefaultTextColor = color;
    else if (BrowserColors)
      gDefaultTextColor = BrowserColors.TextColor;

    color = bodyelement.getAttribute("bgcolor");
    if (color)
      gDefaultBackgroundColor = color;
    else if (BrowserColors)
      gDefaultBackgroundColor = BrowserColors.BackgroundColor;
  }

  // Trigger update on toolbar
  if (defTextColor != gDefaultTextColor)
  {
    goUpdateCommand("cmd_fontColor");
    onFontColorChange();
  }
  if (defBackColor != gDefaultBackgroundColor)
  {
    goUpdateCommand("cmd_backgroundColor");
    onBackgroundColorChange();
  }
}

function GetBackgroundElementWithColor()
{
  gColorObj.Type = "";
  gColorObj.PageColor = "";
  gColorObj.TableColor = "";
  gColorObj.CellColor = "";
  gColorObj.BackgroundColor = "";

  var tagNameObj = new Object;
  var countObj = new Object;
  var element = window.editorShell.GetSelectedOrParentTableElement(tagNameObj, countObj);
  if (element && tagNameObj && tagNameObj.value)
  {
    gColorObj.BackgroundColor = GetHTMLOrCSSStyleValue(element, "bgcolor", "background-color");
    gColorObj.BackgroundColor = ConvertRGBColorIntoHEXColor(gColorObj.BackgroundColor);
    if (tagNameObj.value.toLowerCase() == "td")
    {
      gColorObj.Type = "Cell";
      gColorObj.CellColor = gColorObj.BackgroundColor;

      // Get any color that might be on parent table
      var table = GetParentTable(element);
      gColorObj.TableColor = GetHTMLOrCSSStyleValue(table, "bgcolor", "background-color");
      gColorObj.TableColor = ConvertRGBColorIntoHEXColor(gColorObj.TableColor);
    }
    else
    {
      gColorObj.Type = "Table";
      gColorObj.TableColor = gColorObj.BackgroundColor;
    }
  }
  else
  {
    var prefs = GetPrefs();
    var IsCSSPrefChecked = prefs.getBoolPref("editor.use_css");
    if (IsCSSPrefChecked && editorShell.editorType == "html")
    {
      var selection = window.editorShell.editorSelection;
      if (selection)
      {
        element = selection.focusNode;
        while (!window.editorShell.NodeIsBlock(element))
          element = element.parentNode;
      }
      else
      {
        element = GetBodyElement();
      }
    }
    else
    {
      element = GetBodyElement();
    }
    if (element)
    {
      gColorObj.Type = "Page";
      gColorObj.BackgroundColor = GetHTMLOrCSSStyleValue(element, "bgcolor", "background-color");
      if (gColorObj.BackgroundColor == "")
      {
        gColorObj.BackgroundColor = "transparent";
      }
      else
      {
        gColorObj.BackgroundColor = ConvertRGBColorIntoHEXColor(gColorObj.BackgroundColor);
      }
      gColorObj.PageColor = gColorObj.BackgroundColor;
    }
  }
  return element;
}

function SetSmiley(smileyText)
{
  editorShell.InsertText(smileyText);

  gContentWindow.focus();
}

function EditorSelectColor(colorType, mouseEvent)
{
  if (!gColorObj)
    return;

  // Shift + mouse click automatically applies last color, if available
  var useLastColor = mouseEvent ? ( mouseEvent.button == 0 && mouseEvent.shiftKey ) : false;
  var element;
  var table;
  var currentColor = "";
  var commandNode;

  if (!colorType)
    colorType = "";

  if (colorType == "Text")
  {
    gColorObj.Type = colorType;

    // Get color from command node state
    commandNode = document.getElementById("cmd_fontColor");
    currentColor = commandNode.getAttribute("state");
    gColorObj.TextColor = currentColor;

    if (useLastColor && gColorObj.LastTextColor )
      gColorObj.TextColor = gColorObj.LastTextColor;
    else
      useLastColor = false;
  }
  else if (colorType == "Highlight")
  {
    gColorObj.Type = colorType;

    // Get color from command node state
    commandNode = document.getElementById("cmd_highlight");
    currentColor = commandNode.getAttribute("state");
    gColorObj.HighlightColor = currentColor;

    if (useLastColor && gColorObj.LastHighlightColor )
      gColorObj.HighlightColor = gColorObj.LastHighlightColor;
    else
      useLastColor = false;
  }
  else
  {
    element = GetBackgroundElementWithColor();
    if (!element)
      return;

    // Get the table if we found a cell
    if (gColorObj.Type == "Table")
      table = element;
    else if (gColorObj.Type == "Cell")
      table = GetParentTable(element);

    // Save to avoid resetting if not necessary
    currentColor = gColorObj.BackgroundColor;

    if (colorType == "TableOrCell" || colorType == "Cell")
    {
      if (gColorObj.Type == "Cell")
        gColorObj.Type = colorType;
      else if (gColorObj.Type != "Table")
        return;
    }
    else if (colorType == "Table" && gColorObj.Type == "Page")
      return;

    if (colorType == "" && gColorObj.Type == "Cell")
    {
      // Using empty string for requested type means
      //  we can let user select cell or table
      gColorObj.Type = "TableOrCell";
    }

    if (useLastColor && gColorObj.LastBackgroundColor )
      gColorObj.BackgroundColor = gColorObj.LastBackgroundColor;
    else
      useLastColor = false;
  }
  // Save the type we are really requesting
  colorType = gColorObj.Type;

  if (!useLastColor)
  {
    // Avoid the JS warning
    gColorObj.NoDefault = false;

    // Launch the ColorPicker dialog
    // TODO: Figure out how to position this under the color buttons on the toolbar
    window.openDialog("chrome://editor/content/EdColorPicker.xul", "_blank", "chrome,close,titlebar,modal", "", gColorObj);

    // User canceled the dialog
    if (gColorObj.Cancel)
      return;
  }

  if (gColorObj.Type == "Text")
  {
    if (currentColor != gColorObj.TextColor)
    {
      if (gColorObj.TextColor)
        window.editorShell.SetTextProperty("font", "color", gColorObj.TextColor);
      else
        window.editorShell.RemoveTextProperty("font", "color");
    }
    // Update the command state (this will trigger color button update)
    goUpdateCommand("cmd_fontColor");
  }
  else if (gColorObj.Type == "Highlight")
  {
    if (currentColor != gColorObj.HighlightColor)
    {
      if (gColorObj.HighlightColor)
        window.editorShell.SetTextProperty("font", "bgcolor", gColorObj.HighlightColor);
      else
        window.editorShell.RemoveTextProperty("font", "bgcolor");
    }
    // Update the command state (this will trigger color button update)
    goUpdateCommand("cmd_highlight");
  }
  else if (element)
  {
    if (gColorObj.Type == "Table")
    {
      // Set background on a table
      // Note that we shouldn't trust "currentColor" because of "TableOrCell" behavior
      if (table)
      {
        var bgcolor = table.getAttribute("bgcolor");
        if (bgcolor != gColorObj.BackgroundColor)
        {
          if (gColorObj.BackgroundColor)
            window.editorShell.editor.setAttributeOrEquivalent(table, "bgcolor", gColorObj.BackgroundColor);
          else
            window.editorShell.editor.removeAttributeOrEquivalent(table, "bgcolor");
        }
      }
    }
    else if (currentColor != gColorObj.BackgroundColor)
    {
      window.editorShell.BeginBatchChanges();
      window.editorShell.SetBackgroundColor(gColorObj.BackgroundColor);
      if (gColorObj.Type == "Page" && gColorObj.BackgroundColor)
      {
        // Set all page colors not explicitly set,
        //  else you can end up with unreadable pages
        //  because viewer's default colors may not be same as page author's
        var bodyelement = GetBodyElement();
        if (bodyelement)
        {
          var defColors = GetDefaultBrowserColors();
          if (defColors)
          {
            if (!bodyelement.getAttribute("text"))
              window.editorShell.editor.setAttributeOrEquivalent(bodyelement, "text", defColors.TextColor);

            // The following attributes have no individual CSS declaration counterparts
            // Getting rid of them in favor of CSS implies CSS rules management
            if (!bodyelement.getAttribute("link"))
              window.editorShell.SetAttribute(bodyelement, "link", defColors.LinkColor);

            if (!bodyelement.getAttribute("alink"))
              window.editorShell.SetAttribute(bodyelement, "alink", defColors.LinkColor);

            if (!bodyelement.getAttribute("vlink"))
              window.editorShell.SetAttribute(bodyelement, "vlink", defColors.VisitedLinkColor);
          }
        }
      }
      window.editorShell.EndBatchChanges();
    }

    goUpdateCommand("cmd_backgroundColor");
  }
  gContentWindow.focus();
}

function GetParentTable(element)
{
  var node = element;
  while (node)
  {
    if (node.nodeName.toLowerCase() == "table")
      return node;

    node = node.parentNode;
  }
  return node;
}

function GetParentTableCell(element)
{
  var node = element;
  while (node)
  {
    if (node.nodeName.toLowerCase() == "td" || node.nodeName.toLowerCase() == "th")
      return node;

    node = node.parentNode;
  }
  return node;
}

function EditorDblClick(event)
{
  if (event.target)
  {
    // Only bring up properties if clicked on an element or selected link
    var element;
    try {
      element = event.target.QueryInterface(Components.interfaces.nsIDOMElement);
    } catch (e) {}

     //  We use "href" instead of "a" to not be fooled by named anchor
    if (!element)
      element = editorShell.GetSelectedElement("href");

    if (element)
    {
      goDoCommand("cmd_objectProperties");  
      event.preventDefault();
    }
  }
}

function EditorClick(event)
{
  if (!event)
    return;

  if (event.detail == 2)
  {
    EditorDblClick(event);
    return;
  }

  // In Show All Tags Mode,
  // single click selects entire element,
  //  except for body and table elements
  if (event.target && gEditorDisplayMode == DisplayModeAllTags)
  {
    try {
      var element = event.target.QueryInterface( Components.interfaces.nsIDOMElement);
      if (element)
      {
        var name = element.localName.toLowerCase();
        if (name != "body" && name != "table" &&
            name != "td" && name != "th" && name != "caption" && name != "tr")
        {          
          var htmlEditor = editorShell.editor.QueryInterface(Components.interfaces.nsIHTMLEditor);
          if (htmlEditor && event.target)
          {
            htmlEditor.selectElement(event.target);
            event.preventDefault();
          }
        }
      }
    } catch (e) {}
  }
}

/*TODO: We need an oncreate hook to do enabling/disabling for the
        Format menu. There should be code like this for the
        object-specific "Properties" item
*/
// For property dialogs, we want the selected element,
//  but will accept a parent link, list, or table cell if inside one
function GetObjectForProperties()
{
  var element = editorShell.GetSelectedElement("");
  if (element)
    return element;

  // Find nearest parent of selection anchor node
  //   that is a link, list, table cell, or table

  var anchorNode = editorShell.editorSelection.anchorNode;
  if (!anchorNode) return null;
  var node;
  if (anchorNode.firstChild)
  {
    // Start at actual selected node
    var offset = editorShell.editorSelection.anchorOffset;
    // Note: If collapsed, offset points to element AFTER caret,
    //  thus node may be null
    node = anchorNode.childNodes.item(offset);
  }
  if (!node)
    node = anchorNode;

  while (node)
  {
    if (node.nodeName)
    {
      var nodeName = node.nodeName.toLowerCase();

      // Done when we hit the body
      if (nodeName == "body") break;

      if ((nodeName == "a" && node.href) ||
          nodeName == "ol" || nodeName == "ul" || nodeName == "dl" ||
          nodeName == "td" || nodeName == "th" ||
          nodeName == "table")
      {
        return node;
      }
    }
    node = node.parentNode;
  }
  return null;
}

function SetEditMode(mode)
{
  if (gIsHTMLEditor)
  {
    var bodyNode = editorShell.editorDocument.getElementsByTagName("body").item(0);
    if (!bodyNode)
    {
      dump("SetEditMode: We don't have a body node!\n");
      return;
    }
    // Switch the UI mode before inserting contents
    //   so user can't type in source window while new window is being filled
    var previousMode = gEditorDisplayMode;
    if (!SetDisplayMode(mode))
      return;

    if (mode == DisplayModeSource)
    {
      // Display the DOCTYPE as a non-editable string above edit area
      var domdoc;
      try { domdoc = window.editorShell.editorDocument; } catch (e) { dump( e + "\n");}
      if (domdoc)
      {
        var doctypeNode = document.getElementById("doctype-text");
        var dt = domdoc.doctype;
        if (doctypeNode)
        {
          if (dt)
          {
            doctypeNode.removeAttribute("collapsed");
            var doctypeText = "<!DOCTYPE " + domdoc.doctype.name;
            if (dt.publicId)
              doctypeText += " PUBLIC \"" + domdoc.doctype.publicId;
            if (dt.systemId)
              doctypeText += " "+"\"" + dt.systemId;
            doctypeText += "\">"
            doctypeNode.setAttribute("value", doctypeText);
          }
          else
            doctypeNode.setAttribute("collapsed", "true");
        }
      }
      // Get the entire document's source string

      var flags = 256; // OutputEncodeEntities;

      try { 
        var prettyPrint = gPrefs.getBoolPref("editor.prettyprint");
        if (prettyPrint)
          flags |= 2; // OutputFormatted

      } catch (e) {}

      var source = editorShell.GetContentsAs("text/html", flags);
      var start = source.search(/<html/i);
      if (start == -1) start = 0;
      gSourceContentWindow.value = source.slice(start);
      gSourceContentWindow.focus();

      // Add input handler so we know if user made any changes
      gSourceContentWindow.addEventListener("input", oninputHTMLSource, false);
      gHTMLSourceChanged = false;
    }
    else if (previousMode == DisplayModeSource)
    {
      // Only rebuild document if a change was made in source window
      if (gHTMLSourceChanged)
      {
        editorShell.BeginBatchChanges();
        try {
          // We are comming from edit source mode,
          //   so transfer that back into the document
          source = gSourceContentWindow.value;
          editorShell.RebuildDocumentFromSource(source);

          // Get the text for the <title> from the newly-parsed document
          // (must do this for proper conversion of "escaped" characters)
          var title = "";
          var titlenodelist = window.editorShell.editorDocument.getElementsByTagName("title");
          if (titlenodelist)
          {
            var titleNode = titlenodelist.item(0);
            if (titleNode && titleNode.firstChild && titleNode.firstChild.data)
              title = titleNode.firstChild.data;
          }
          if (window.editorShell.editorDocument.title != title)
          {
            window.editorShell.editorDocument.title = title;
            ResetWindowTitleWithFilename();
          }

          // reset selection to top of doc (wish we could preserve it!)
          if (bodyNode)
            editorShell.editorSelection.collapse(bodyNode, 0);

        } catch (ex) {
          dump(ex);
        }
        editorShell.EndBatchChanges();

      } else {
        // We don't need to call this again, so remove handler
        gSourceContentWindow.removeEventListener("input", oninputHTMLSource, false);
      }
      gHTMLSourceChanged = false;

      // Clear out the string buffers
      gSourceContentWindow.value = null;

      gContentWindow.focus();
    }
  }
}

function oninputHTMLSource()
{
  gHTMLSourceChanged = true;

  // Trigger update of "Save" button
  goUpdateCommand("cmd_save");

  // We don't need to call this again, so remove handler
  gSourceContentWindow.removeEventListener("input", oninputHTMLSource, false);
}

function ResetWindowTitleWithFilename()
{
  // Calling this resets the "Title [filename]" that we show on window caption
  window.editorShell.SetDocumentTitle(window.editorShell.GetDocumentTitle());
}

function CancelHTMLSource()
{
  // Don't convert source text back into the DOM document
  gSourceContentWindow.value = "";
  gHTMLSourceChanged = false;
  SetDisplayMode(PreviousNonSourceDisplayMode);
}


function FinishHTMLSource()
{
  // Switch edit modes -- converts source back into DOM document
  SetEditMode(PreviousNonSourceDisplayMode);
}

function CollapseItem(id, collapse)
{
  var item = document.getElementById(id);
  if (item)
  {
    if(collapse != (item.getAttribute("collapsed") == "true"))
      item.setAttribute("collapsed", collapse ? "true" : "");
  }
}

function SetDisplayMode(mode)
{
  if (gIsHTMLEditor)
  {
    // Already in requested mode:
    //  return false to indicate we didn't switch
    if (mode == gEditorDisplayMode)
      return false;

    gEditorDisplayMode = mode;

    // Save the last non-source mode so we can cancel source editing easily
    if (mode != DisplayModeSource)
      PreviousNonSourceDisplayMode = mode;


    // Editorshell does the style sheet loading/unloading
    editorShell.SetDisplayMode(mode);

    // Set the UI states
    var selectedTab = null;
    if (mode == DisplayModePreview) selectedTab = gPreviewModeButton;
    if (mode == DisplayModeNormal) selectedTab = gNormalModeButton;
    if (mode == DisplayModeAllTags) selectedTab = gTagModeButton;
    if (mode == DisplayModeSource) selectedTab = gSourceModeButton;
    if (selectedTab)
      document.getElementById("EditModeTabs").selectedItem = selectedTab;

    if (mode == DisplayModeSource)
    {
      // Switch to the sourceWindow (second in the deck)
      gContentWindowDeck.setAttribute("selectedIndex","1");

      //Hide the formatting toolbar if not already hidden
      gFormatToolbarHidden = gFormatToolbar.getAttribute("hidden");
      if (gFormatToolbarHidden != "true")
      {
        gFormatToolbar.setAttribute("hidden", "true");
      }

      gSourceContentWindow.focus();
    }
    else
    {
      // Switch to the normal editor (first in the deck)
      gContentWindowDeck.setAttribute("selectedIndex","0");

      // Restore menus and toolbars
      if (gFormatToolbarHidden != "true")
      {
        gFormatToolbar.setAttribute("hidden", gFormatToolbarHidden);
      }

      gContentWindow.focus();
    }

    // update commands to disable or re-enable stuff
    window.updateCommands("mode_switch");

    // We must set check on menu item since toolbar may have been used
    document.getElementById("viewPreviewMode").setAttribute("checked","false");
    document.getElementById("viewNormalMode").setAttribute("checked","false");
    document.getElementById("viewAllTagsMode").setAttribute("checked","false");
    document.getElementById("viewSourceMode").setAttribute("checked","false");

    var menuID;
    switch(mode)
    {
      case DisplayModePreview:
        menuID = "viewPreviewMode";
        break;
      case DisplayModeNormal:
        menuID = "viewNormalMode";
        break;
      case DisplayModeAllTags:
        menuID = "viewAllTagsMode";
        break;
      case DisplayModeSource:
        menuID = "viewSourceMode";
        break;
    }
    if (menuID)
      document.getElementById(menuID).setAttribute("checked","true");

    return true;
  }
  return false;
}

function EditorToggleParagraphMarks()
{
  var menuItem = document.getElementById("viewParagraphMarks");
  if (menuItem)
  {
    // Note that the 'type="checbox"' mechanism automatically
    //  toggles the "checked" state before the oncommand is called,
    //  so if "checked" is true now, it was just switched to that mode
    var checked = menuItem.getAttribute("checked");
    try {
      editorShell.DisplayParagraphMarks(checked == "true");
    }
    catch(e) { return; }
  }
}

function InitPasteAsMenu()
{
  var menuItem = document.getElementById("menu_pasteTable")
  if(menuItem)
  {
    menuItem.IsInTable  
    menuItem.setAttribute("label", GetString(IsInTable() ? "NestedTable" : "Table"));
   // menuItem.setAttribute("accesskey",GetString("ObjectPropertiesAccessKey"));
  }
  // TODO: Do enabling based on what is in the clipboard
}

function BuildRecentMenu(savePrefs)
{
  // Can't do anything if no prefs
  if (!gPrefs) return;

  var popup = document.getElementById("menupopup_RecentFiles");
  if (!popup || !window.editorShell ||
      !window.editorShell.editorDocument)
    return;

  // Delete existing menu
  while (popup.firstChild)
    popup.removeChild(popup.firstChild);

  // Current page is the "0" item in the list we save in prefs,
  //  but we don't include it in the menu.
  var curTitle = window.editorShell.editorDocument.title;
  var curUrl = StripPassword(GetDocumentUrl());
  var historyCount = 10;
  try { historyCount = gPrefs.getIntPref("editor.history.url_maximum"); } catch(e) {}
  var titleArray = new Array(historyCount);
  var urlArray   = new Array(historyCount);
  var menuIndex = 1;
  var arrayIndex = 0;
  var i;
  var disableMenu = true;

  if(!IsUrlAboutBlank(curUrl))
  {
    // Always put latest-opened URL at start of array
    titleArray[0] = curTitle;
    urlArray[0] = curUrl;
    arrayIndex = 1;
  }
  for (i = 0; i < historyCount; i++)
  {
    var title = GetUnicharPref("editor.history_title_"+i);
    var url = GetUnicharPref("editor.history_url_"+i);

    // Continue if URL pref is missing because 
    //  a URL not found during loading may have been removed
    // Also skip "data:" URL
    if (!url || GetScheme(url) == "data")
      continue;

    // Never show password in menu!
    url = StripPassword(url);

    // Skip over current URL
    if (url != curUrl)
    {

      // Build the menu
      AppendRecentMenuitem(popup, title, url, menuIndex);
      menuIndex++;
      disableMenu = false;

      // Save in array for prefs
      if (savePrefs && arrayIndex < historyCount)
      {
        titleArray[arrayIndex] = title;
        urlArray[arrayIndex] = url;
        arrayIndex++;
      }
    }
  }

  // Now resave the list back to prefs in the new order
  if (savePrefs)
  {
    savePrefs = false;
    for (i = 0; i < historyCount; i++)
    {
      if (!urlArray[i])
        break;
      SetUnicharPref("editor.history_title_"+i, titleArray[i]);
      SetUnicharPref("editor.history_url_"+i, urlArray[i]);
      savePrefs = true;
    }
  }
  // Force saving to file so next file opened finds these values
  if (savePrefs) {
    var prefsService = Components.classes["@mozilla.org/preferences-service;1"]
                                 .getService(Components.interfaces.nsIPrefService);
    prefsService.savePrefFile(null);
  }

  // Disable menu item if no entries
  DisableItem("menu_RecentFiles", disableMenu);
}

function AppendRecentMenuitem(menupopup, title, url, menuIndex)
{
  if (menupopup)
  {
    var menuItem = document.createElementNS(XUL_NS, "menuitem");
    if (menuItem)
    {
      var accessKey;
      if (menuIndex <= 9)
        accessKey = String(menuIndex);
      else if (menuIndex == 10)
        accessKey = "0";
      else
        accessKey = " ";

      var itemString = accessKey+" ";

      // Show "title [url]" or just the URL
      if (title)
      {
       itemString += title;
       itemString += " [";
      }
      itemString += url;
      if (title)
        itemString += "]";

      menuItem.setAttribute("label", itemString);
      menuItem.setAttribute("value", url);
      if (accessKey != " ")
        menuItem.setAttribute("accesskey", accessKey);
      menuItem.setAttribute("oncommand", "editPage(getAttribute('value'), window, false)");
      menupopup.appendChild(menuItem);
    }
  }
}

function EditorInitFileMenu()
{
  // Disable "Save" menuitem when editing remote url. User should use "Save As"
  var scheme = GetScheme(GetDocumentUrl());
  if (scheme && scheme != "file")
    SetElementEnabledById("saveMenuitem", false);
}

function EditorInitFormatMenu()
{
  try {
    InitObjectPropertiesMenuitem("objectProperties");
    InitRemoveStylesMenuitems("removeStylesMenuitem", "removeLinksMenuitem", "removeNamedAnchorsMenuitem");
  } catch(ex) {}
  // Set alignment check
}

function InitObjectPropertiesMenuitem(id)
{
  // Set strings and enable for the [Object] Properties item
  // Note that we directly do the enabling instead of
  //  using goSetCommandEnabled since we already have the menuitem
  var menuItem = document.getElementById(id);
  if (!menuItem) return null;

  var element;
  var menuStr = GetString("AdvancedProperties");
  var name;

  if (IsEditingRenderedHTML())
    element = GetObjectForProperties();

  if (element && element.nodeName)
  {
    var objStr = "";
    menuItem.setAttribute("disabled", "");
    name = element.nodeName.toLowerCase();
    switch (name)
    {
      case "img":
        // Check if img is enclosed in link
        //  (use "href" to not be fooled by named anchor)
        if (editorShell.GetElementOrParentByTagName("href", element))
          objStr = GetString("ImageAndLink");
        else
          objStr = GetString("Image");
        break;
      case "hr":
        objStr = GetString("HLine");
        break;
      case "table":
        objStr = GetString("Table");
        break;
      case "th":
        name = "td";
      case "td":
        objStr = GetString("TableCell");
        break;
      case "ol":
      case "ul":
      case "dl":
        objStr = GetString("List");
        break;
      case "li":
        objStr = GetString("ListItem");
        break;
      case "form":
        objStr = GetString("Form");
        break;
      case "input":
        var type = element.getAttribute("type");
        if (type && type.toLowerCase() == "image")
          objStr = GetString("InputImage");
        else
          objStr = GetString("InputTag");
        break;
      case "textarea":
        objStr = GetString("TextArea");
        break;
      case "select":
        objStr = GetString("Select");
        break;
      case "button":
        objStr = GetString("Button");
        break;
      case "label":
        objStr = GetString("Label");
        break;
      case "fieldset":
        objStr = GetString("FieldSet");
        break;
      case "a":
        if (element.name)
        {
          objStr = GetString("NamedAnchor");
          name = "anchor";
        }
        else if(element.href)
        {
          objStr = GetString("Link");
          name = "href";
        }
        break;
    }
    if (objStr)
      menuStr = GetString("ObjectProperties").replace(/%obj%/,objStr);
  }
  else
  {
    // We show generic "Properties" string, but disable menu item
    menuItem.setAttribute("disabled","true");
  }
  menuItem.setAttribute("label", menuStr);
  menuItem.setAttribute("accesskey",GetString("ObjectPropertiesAccessKey"));
  return name;
}

function InitParagraphMenu()
{
  var mixedObj = new Object();
  var state = editorShell.GetParagraphState(mixedObj);
  var IDSuffix;

  // PROBLEM: When we get blockquote, it masks other styles contained by it
  // We need a separate method to get blockquote state

  // We use "x" as uninitialized paragraph state
  if (!state || state == "x")
    IDSuffix = "bodyText" // No paragraph container
  else
    IDSuffix = state;

  // Set "radio" check on one item, but...
  var menuItem = document.getElementById("menu_"+IDSuffix);
  menuItem.setAttribute("checked", "true");

  // ..."bodyText" is returned if mixed selection, so remove checkmark
  if (mixedObj.value)
    menuItem.setAttribute("checked", "false");
}

function InitListMenu()
{
  var mixedObj = new Object();
  var state = editorShell.GetListState(mixedObj);
  var IDSuffix = "noList";
  if (state)
  {
    if (state == "dl")
      state = editorShell.GetListItemState(mixedObj);

    if (state)
      IDSuffix = state;
  }
  // Set enable state for the "None" menuitem
  goSetCommandEnabled("cmd_removeList", state);

  // Set "radio" check on one item, but...
  var menuItem = document.getElementById("menu_"+IDSuffix);
  if (!menuItem)
    return;

  menuItem.setAttribute("checked", "true");

  // ..."noList" is returned if mixed selection, so remove checkmark
  if (mixedObj.value)
    menuItem.setAttribute("checked", "false");

}

function InitAlignMenu()
{
  var mixedObj = new Object();
  // Note: GetAlignment DOESN'T set the "mixed" flag,
  //  but always returns "left" in mixed state
  // We'll pay attention to it here so it works when fixed
  var state = editorShell.GetAlignment(mixedObj);
  var IDSuffix;

  if (!state)
    IDSuffix = "left"
  else
    IDSuffix = state;

  var menuItem = document.getElementById("menu_"+IDSuffix);
  menuItem.setAttribute("checked", "true");

  if (mixedObj.value)
    menuItem.setAttribute("checked", "false");
}

function EditorInitToolbars()
{
  // Nothing to do now, but we might want some state updating here
  if (!IsEditorContentHTML())
  {
    //Hide the formating toolbar
    gFormatToolbar.setAttribute("hidden", "true");

    //Hide the edit mode toolbar
    gEditModeBar.setAttribute("hidden", "true");
    
    DisableItem("cmd_viewFormatToolbar", true);
    DisableItem("cmd_viewEditModeToolbar", true);
  }

}

function EditorSetDefaultPrefsAndDoctype()
{
  var domdoc;
  try { domdoc = window.editorShell.editorDocument; } catch (e) { dump( e + "\n"); }
  if ( !domdoc )
  {
    dump("EditorSetDefaultPrefsAndDoctype: EDITOR DOCUMENT NOT FOUND\n");
    return;
  }

  // Insert a doctype element 
  // if it is missing from existing doc
  if (!domdoc.doctype)
  {
    var newdoctype = domdoc.implementation.createDocumentType("html", "-//W3C//DTD HTML 4.01 Transitional//EN","");
    if (newdoctype)
      domdoc.insertBefore(newdoctype, domdoc.firstChild);
  }
  
  // search for head; we'll need this for meta tag additions
  var headelement = 0;
  var headnodelist = domdoc.getElementsByTagName("head");
  if (headnodelist)
  {
    var sz = headnodelist.length;
    if ( sz >= 1 )
      headelement = headnodelist.item(0);
  }
  else
  {
    headelement = domdoc.createElement("head");
    if (headelement)
      domdoc.insertAfter(headelement, domdoc.firstChild);
  }

  // add title tag if not present
  var titlenodelist = window.editorShell.editorDocument.getElementsByTagName("title");
  if (headelement && titlenodelist && titlenodelist.length == 0)
  {
     titleElement = domdoc.createElement("title");
     if (titleElement)
       headelement.appendChild(titleElement);
  }

  /* only set default prefs for new documents */
  if (!IsUrlAboutBlank(GetDocumentUrl()))
    return;

  // search for author meta tag.
  // if one is found, don't do anything.
  // if not, create one and make it a child of the head tag
  //   and set its content attribute to the value of the editor.author preference.

  var nodelist = domdoc.getElementsByTagName("meta");
  if ( nodelist )
  {
    // we should do charset first since we need to have charset before
    // hitting other 8-bit char in other meta tags
    // grab charset pref and make it the default charset
    var element;
    var prefCharsetString = 0;
    try
    {
      prefCharsetString = gPrefs.getComplexValue("intl.charset.default",
                                                 Components.interfaces.nsIPrefLocalizedString).data;
    }
    catch (ex) {}
    if ( prefCharsetString && prefCharsetString != 0)
    {
        element = domdoc.createElement("meta");
        if ( element )
        {
          AddAttrToElem(domdoc, "http-equiv", "content-type", element);
          AddAttrToElem(domdoc, "content", "text/html; charset=" + prefCharsetString, element);
          headelement.appendChild( element );
        }
    }

    var node = 0;
    var listlength = nodelist.length;

    // let's start by assuming we have an author in case we don't have the pref
    var authorFound = false;
    for (var i = 0; i < listlength && !authorFound; i++)
    {
      node = nodelist.item(i);
      if ( node )
      {
        var value = node.getAttribute("name");
        if (value && value.toLowerCase() == "author")
        {
          authorFound = true;
        }
      }
    }

    var prefAuthorString = 0;
    try
    {
      prefAuthorString = gPrefs.getComplexValue("editor.author",
                                                Components.interfaces.nsISupportsWString).data;
    }
    catch (ex) {}
    if ( prefAuthorString && prefAuthorString != 0)
    {
      if ( !authorFound && headelement)
      {
        /* create meta tag with 2 attributes */
        element = domdoc.createElement("meta");
        if ( element )
        {
          AddAttrToElem(domdoc, "name", "author", element);
          AddAttrToElem(domdoc, "content", prefAuthorString, element);
          headelement.appendChild( element );
        }
      }
    }
  }

  // Get editor color prefs
  var use_custom_colors = false;
  try {
    use_custom_colors = gPrefs.getBoolPref("editor.use_custom_colors");
  }
  catch (ex) {}

  // find body node
  var bodyelement = GetBodyElement();
  if (bodyelement)
  {
    if ( use_custom_colors )
    {
      // try to get the default color values.  ignore them if we don't have them.
      var text_color;
      var link_color;
      var active_link_color;
      var followed_link_color;
      var background_color;

      try { text_color = gPrefs.getCharPref("editor.text_color"); } catch (e) {}
      try { link_color = gPrefs.getCharPref("editor.link_color"); } catch (e) {}
      try { active_link_color = gPrefs.getCharPref("editor.active_link_color"); } catch (e) {}
      try { followed_link_color = gPrefs.getCharPref("editor.followed_link_color"); } catch (e) {}
      try { background_color = gPrefs.getCharPref("editor.background_color"); } catch(e) {}

      // add the color attributes to the body tag.
      // and use them for the default text and background colors if not empty
      if (text_color)
      {
        bodyelement.setAttribute("text", text_color);
        gDefaultTextColor = text_color;
      }
      if (background_color)
      {
        bodyelement.setAttribute("bgcolor", background_color);
        gDefaultBackgroundColor = background_color
      }

      if (link_color)
        bodyelement.setAttribute("link", link_color);
      if (active_link_color)
        bodyelement.setAttribute("alink", active_link_color);
      if (followed_link_color)
        bodyelement.setAttribute("vlink", followed_link_color);
    }
    // Default image is independent of Custom colors???
    var background_image;
    try { background_image = gPrefs.getCharPref("editor.default_background_image"); } catch(e) {}

    if (background_image)
      bodyelement.setAttribute("background", background_image);
  }
  // auto-save???
}

function GetBodyElement()
{
  try {
    var bodyNodelist = window.editorShell.editorDocument.getElementsByTagName("body");
    if (bodyNodelist)
      return bodyNodelist.item(0);
  }
  catch (ex) {
    dump("no body tag found?!\n");
    //  better have one, how can we blow things up here?
  }
  return null;
}

function AddAttrToElem(dom, attr_name, attr_value, elem)
{
  var a = dom.createAttribute(attr_name);
  if ( a )
  {
    a.value = attr_value;
    elem.setAttributeNode(a);
  }
}

// --------------------------- Logging stuff ---------------------------

function EditorGetNodeFromOffsets(offsets)
{
  var node = null;
  node = editorShell.editorDocument;

  for (var i = 0; i < offsets.length; i++)
  {
    node = node.childNodes[offsets[i]];
  }

  return node;
}

function EditorSetSelectionFromOffsets(selRanges)
{
  var rangeArr, start, end, node, offset;
  var selection = editorShell.editorSelection;

  selection.removeAllRanges();

  for (var i = 0; i < selRanges.length; i++)
  {
    rangeArr = selRanges[i];
    start    = rangeArr[0];
    end      = rangeArr[1];

    var range = editorShell.editorDocument.createRange();

    node   = EditorGetNodeFromOffsets(start[0]);
    offset = start[1];

    range.setStart(node, offset);

    node   = EditorGetNodeFromOffsets(end[0]);
    offset = end[1];

    range.setEnd(node, offset);

    selection.addRange(range);
  }
}

//--------------------------------------------------------------------
function initFontStyleMenu(menuPopup)
{
  for (var i = 0; i < menuPopup.childNodes.length; i++)
  {
    var menuItem = menuPopup.childNodes[i];
    var theStyle = menuItem.getAttribute("state");
    if (theStyle)
    {
      menuItem.setAttribute("checked", theStyle);
    }
  }
}

//--------------------------------------------------------------------
function onButtonUpdate(button, commmandID)
{
  var commandNode = document.getElementById(commmandID);
  var state = commandNode.getAttribute("state");
  button.checked = state == "true";
}

//--------------------------------------------------------------------
function onStateButtonUpdate(button, commmandID, onState)
{
  var commandNode = document.getElementById(commmandID);
  var state = commandNode.getAttribute("state");

  button.checked = state == onState;
}

// --------------------------- Status calls ---------------------------
function getColorAndSetColorWell(ColorPickerID, ColorWellID)
{
  var colorWell;
  if (ColorWellID)
    colorWell = document.getElementById(ColorWellID);

  var colorPicker = document.getElementById(ColorPickerID);
  if (colorPicker)
  {
    // Extract color from colorPicker and assign to colorWell.
    var color = colorPicker.getAttribute("color");

    if (colorWell && color)
    {
      // Use setAttribute so colorwell can be a XUL element, such as button
      colorWell.setAttribute("style", "background-color: " + color);
    }
  }
  return color;
}

//-----------------------------------------------------------------------------------
function IsSpellCheckerInstalled()
{
  return "@mozilla.org/spellchecker;1" in Components.classes;
}

//-----------------------------------------------------------------------------------
function IsFindInstalled()
{
  return "@mozilla.org/appshell/component/find;1" in Components.classes;
}

//-----------------------------------------------------------------------------------
function RemoveInapplicableUIElements()
{
  // For items that are in their own menu block, remove associated separator
  // (we can't use "hidden" since class="hide-in-IM" CSS rule interferes)

   // if no find, remove find ui
  if (!IsFindInstalled())
  {
    HideItem("menu_find");
    HideItem("menu_findnext");
    HideItem("menu_replace");
    HideItem("menu_find");
    RemoveItem("sep_find");
  }

   // if no spell checker, remove spell checker ui
  if (!IsSpellCheckerInstalled())
  {
    HideItem("spellingButton");
    HideItem("menu_checkspelling");
    RemoveItem("sep_checkspelling");
  }
  else
  {
    SetElementEnabled(document.getElementById("menu_checkspelling"), true);
    SetElementEnabled(document.getElementById("spellingButton"), true);
    SetElementEnabled(document.getElementById("checkspellingkb"), true);
  }

  // Remove menu items (from overlay shared with HTML editor) in PlainText editor
  if (editorShell.editorType == "text")
  {
    HideItem("insertAnchor");
    HideItem("insertImage");
    HideItem("insertHline");
    HideItem("insertTable");
    HideItem("insertHTML");
    HideItem("insertFormMenu");
    HideItem("fileExportToText");
    HideItem("viewFormatToolbar");
    HideItem("viewEditModeToolbar");
  }
}

function HideItem(id)
{
  var item = document.getElementById(id);
  if (item)
    item.setAttribute("hidden", "true");
}

function RemoveItem(id)
{
  var item = document.getElementById(id);
  if (item)
    item.parentNode.removeChild(item);
}

// Command Updating Strategy:
//   Don't update on on selection change, only when menu is displayed,
//   with this "oncreate" hander:
function EditorInitTableMenu()
{
  try {
    InitJoinCellMenuitem("menu_JoinTableCells");
  } catch (ex) {}

  // Set enable states for all table commands
  goUpdateTableMenuItems(document.getElementById("composerTableMenuItems"));
}

function InitJoinCellMenuitem(id)
{
  // Change text on the "Join..." item depending if we
  //   are joining selected cells or just cell to right
  // TODO: What to do about normal selection that crosses
  //       table border? Try to figure out all cells
  //       included in the selection?
  var menuText;
  var menuItem = document.getElementById(id);
  if (!menuItem) return;

  // Use "Join selected cells if there's more than 1 cell selected
  var tagNameObj = new Object;
  var countObj = new Object;
  var foundElement;
  
  if (IsEditingRenderedHTML())
    foundElement = window.editorShell.GetSelectedOrParentTableElement(tagNameObj, countObj);
    
  if (foundElement && countObj.value > 1)
    menuText = GetString("JoinSelectedCells");
  else
    menuText = GetString("JoinCellToRight");

  menuItem.setAttribute("label",menuText);
  menuItem.setAttribute("accesskey",GetString("JoinCellAccesskey"));
}

function InitRemoveStylesMenuitems(removeStylesId, removeLinksId, removeNamedAnchorsId)
{
  // Change wording of menuitems depending on selection
  var stylesItem = document.getElementById(removeStylesId);
  var linkItem = document.getElementById(removeLinksId);
  var namedAnchorsItem = document.getElementById(removeNamedAnchorsId);

  var isCollapsed = editorShell.editorSelection.isCollapsed;
  if (stylesItem)
  {
    stylesItem.setAttribute("label", isCollapsed ? GetString("StopTextStyles") : GetString("RemoveTextStyles"));
    stylesItem.setAttribute("accesskey", GetString("RemoveTextStylesAccesskey"));
  }
  if (linkItem)
  {
    linkItem.setAttribute("label", isCollapsed ? GetString("StopLinks") : GetString("RemoveLinks"));
    linkItem.setAttribute("accesskey", GetString("RemoveLinksAccesskey"));
    // Note: disabling text style is a pain since there are so many - forget it!

    // Disable if not in a link, but always allow "Remove"
    //  if selection isn't collapsed since we only look at anchor node
    DisableItem(removeLinksId, isCollapsed && !window.editorShell.GetElementOrParentByTagName("href", null));
  }
  if (namedAnchorsItem)
  {
    // Disable if selection is collapsed
    DisableItem(removeNamedAnchorsId, isCollapsed);
  }
}

function goUpdateTableMenuItems(commandset)
{
  var enabled = false;
  var enabledIfTable = false;

  if (window.editorShell && window.editorShell.documentEditable && IsEditingRenderedHTML())
  {
    var selectedCountObj = new Object();
    var tagNameObj = new Object();
    var element = editorShell.GetSelectedOrParentTableElement(tagNameObj, selectedCountObj);
    if (element)
    {
      // Value when we need to have a selected table or inside a table
      enabledIfTable = true;

      // All others require being inside a cell or selected cell
      enabled = (tagNameObj.value == "td");
    }
  }

  // Loop through command nodes
  for (var i = 0; i < commandset.childNodes.length; i++)
  {
    var commandID = commandset.childNodes[i].getAttribute("id");
    if (commandID)
    {
      if (commandID == "cmd_InsertTable" ||
          commandID == "cmd_JoinTableCells" ||
          commandID == "cmd_SplitTableCell" ||
          commandID == "cmd_ConvertToTable")
      {
        // Call the update method in the command class
        goUpdateCommand(commandID);
      }
      // Directly set with the values calculated here
      else if (commandID == "cmd_DeleteTable" ||
               commandID == "cmd_NormalizeTable" ||
               commandID == "cmd_editTable" ||
               commandID == "cmd_TableOrCellColor" ||
               commandID == "cmd_SelectTable")
      {
        goSetCommandEnabled(commandID, enabledIfTable);
      } else {
        goSetCommandEnabled(commandID, enabled);
      }
    }
  }
}

//-----------------------------------------------------------------------------------
// Helpers for inserting and editing tables:

function IsInTable()
{
  return (window.editorShell && window.editorShell.documentEditable && IsEditingRenderedHTML() &&
          null != window.editorShell.GetElementOrParentByTagName("table", null));
}

function IsInTableCell()
{
  return (window.editorShell && window.editorShell.documentEditable && IsEditingRenderedHTML() &&
          null != window.editorShell.GetElementOrParentByTagName("td", null));
}

function IsSelectionInOneCell()
{
  var selection = window.editorShell.editorSelection;

  if (selection && selection.rangeCount == 1)
  {
    // We have a "normal" single-range selection
    if (!selection.isCollapsed &&
       selection.anchorNode != selection.focusNode)
    {
      // Check if both nodes are within the same cell
      var anchorCell = window.editorShell.GetElementOrParentByTagName("td", selection.anchorNode);
      var focusCell = window.editorShell.GetElementOrParentByTagName("td", selection.focusNode);
      return (focusCell != null && anchorCell != null && (focusCell == anchorCell));
    }
    // Collapsed selection or anchor == focus (thus must be in 1 cell)
    return true;
  }
  return false;
}

// Call this with insertAllowed = true to allow inserting if not in existing table,
//   else use false to do nothing if not in a table
function EditorInsertOrEditTable(insertAllowed)
{
  if (IsInTable()) {
    // Edit properties of existing table
    window.openDialog("chrome://editor/content/EdTableProps.xul", "_blank", "chrome,close,titlebar,modal", "","TablePanel");
    gContentWindow.focus();
  } else if(insertAllowed) {
    if (editorShell.editorSelection.isCollapsed)
      // If we have a caret, insert a blank table...
      EditorInsertTable();
    else
      // else convert the selection into a table
      goDoCommand("cmd_ConvertToTable");
  }
}

function EditorInsertTable()
{
  // Insert a new table
  window.openDialog("chrome://editor/content/EdInsertTable.xul", "_blank", "chrome,close,titlebar,modal", "");
  gContentWindow.focus();
}

function EditorTableCellProperties()
{
  var cell = editorShell.GetElementOrParentByTagName("td", null);
  if (cell) {
    // Start Table Properties dialog on the "Cell" panel
    window.openDialog("chrome://editor/content/EdTableProps.xul", "_blank", "chrome,close,titlebar,modal", "", "CellPanel");
    gContentWindow.focus();
  }
}

function GetNumberOfContiguousSelectedRows()
{
  var cell = editorShell.GetFirstSelectedCell();

  if (!cell)
    return 0;

  var rows = 1;
  var lastIndex = editorShell.GetRowIndex(cell);

  do {
    cell = editorShell.GetNextSelectedCell();
    if (cell)
    {
      var index = editorShell.GetRowIndex(cell);
      if (index == lastIndex + 1)
      {
        lastIndex = index;
        rows++;
      }
    }
  }
  while (cell);

  return rows;
}

function GetNumberOfContiguousSelectedColumns()
{
  var cell = editorShell.GetFirstSelectedCell();

  if (!cell)
    return 0;

  var columns = 1;
  var lastIndex = editorShell.GetColumnIndex(cell);

  do {
    cell = editorShell.GetNextSelectedCell();
    if (cell)
    {
      var index = editorShell.GetColumnIndex(cell);
      if (index == lastIndex +1)
      {
        lastIndex = index;
        columns++;
      }
    }
  }
  while (cell);

  return columns;
}

function EditorOnFocus()
{
  // Current window already has the InsertCharWindow
  if ("InsertCharWindow" in window && window.InsertCharWindow) return;

  // Find window with an InsertCharsWindow and switch association to this one
  var windowWithDialog = FindEditorWithInsertCharDialog();
  if (windowWithDialog)
  {
    // Switch the dialog to current window
    // this sets focus to dialog, so bring focus back to editor window
    if (SwitchInsertCharToThisWindow(windowWithDialog))
      window.focus();
  }
}

function SwitchInsertCharToThisWindow(windowWithDialog)
{
  if (windowWithDialog && "InsertCharWindow" in windowWithDialog &&
      windowWithDialog.InsertCharWindow)
  {
    // Move dialog association to the current window
    window.InsertCharWindow = windowWithDialog.InsertCharWindow;
    windowWithDialog.InsertCharWindow = null;

    // Switch the dialog's editorShell and opener to current window's
    window.InsertCharWindow.editorShell = window.editorShell;
    window.InsertCharWindow.opener = window;

    // Bring dialog to the forground
    window.InsertCharWindow.focus();
    return true;
  }
  return false;
}

function FindEditorWithInsertCharDialog()
{
  // Find window with an InsertCharsWindow and switch association to this one
  var windowManager = Components.classes['@mozilla.org/rdf/datasource;1?name=window-mediator'].getService();
  var windowManagerInterface = windowManager.QueryInterface( Components.interfaces.nsIWindowMediator);
  var enumerator = windowManagerInterface.getEnumerator( null );

  while ( enumerator.hasMoreElements()  )
  {
    var  tempWindow = enumerator.getNext();

    if (tempWindow != window && "InsertCharWindow" in tempWindow &&
        tempWindow.InsertCharWindow)
    {
      return tempWindow;
    }
  }
  return null;
}

function EditorFindOrCreateInsertCharWindow()
{
  if ("InsertCharWindow" in window && window.InsertCharWindow)
    window.InsertCharWindow.focus();
  else
  {
    // Since we switch the dialog during EditorOnFocus(),
    //   this should really never be found, but it's good to be sure
    var windowWithDialog = FindEditorWithInsertCharDialog();
    if (windowWithDialog)
    {
      SwitchInsertCharToThisWindow(windowWithDialog);
    }
    else
    {
      // The dialog will set window.InsertCharWindow to itself
      window.openDialog("chrome://editor/content/EdInsertChars.xul", "_blank", "chrome,close,titlebar", "");
    }
  }
}

// Find another HTML editor window to associate with the InsertChar dialog
//   or close it if none found  (May be a mail composer)
function SwitchInsertCharToAnotherEditorOrClose()
{
  if ("InsertCharWindow" in window && window.InsertCharWindow)
  {
    var windowManager = Components.classes['@mozilla.org/rdf/datasource;1?name=window-mediator'].getService();
    var windowManagerInterface = windowManager.QueryInterface( Components.interfaces.nsIWindowMediator);
    var enumerator = windowManagerInterface.getEnumerator( null );

    // TODO: Fix this to search for command controllers and look for "cmd_InsertChars"
    // For now, detect just Web Composer and HTML Mail Composer
    while ( enumerator.hasMoreElements()  )
    {
      var  tempWindow = enumerator.getNext();
      if (tempWindow != window && tempWindow != window.InsertCharWindow && 
          ("editorShell" in tempWindow) && tempWindow.editorShell)
      {
        var type = tempWindow.editorShell.editorType;
        if (type == "html" || type == "text" || type == "htmlmail")
	      {
          tempWindow.InsertCharWindow = window.InsertCharWindow;
          window.InsertCharWindow = null;

          tempWindow.InsertCharWindow.editorShell = tempWindow.editorShell;
          tempWindow.InsertCharWindow.opener = tempWindow;
          return;
	      }
      }
    }
    // Didn't find another editor - close the dialog
    window.InsertCharWindow.close();
  }
}
