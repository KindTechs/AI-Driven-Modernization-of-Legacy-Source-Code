var gMsgFolder;
var gServerTypeFolder = null;
var gPreselectedFolderURI = null;
var gParentMsgWindow = null;

// services used
var RDF;

// corresponds to MSG_FOLDER_FLAG_OFFLINE
const MSG_FOLDER_FLAG_OFFLINE = 0x8000000

// The folderPropsSink is the class that gets notified of an imap folder's properties

var gFolderPropsSink = {
    setFolderType: function(folderTypeString)
    {
      var typeLabel = document.getElementById("folderType.text");
      if (typeLabel)
      {
        dump("folder type = "+ folderTypeString + "\n");
        typeLabel.setAttribute("value",folderTypeString);
      }
      // get the element for the folder type label and set value on it.
    },

    setFolderTypeDescription: function(folderDescription)
    {
      var folderTypeLabel = document.getElementById("folderDescription.text");
      if (folderTypeLabel)
        folderTypeLabel.setAttribute("value", folderDescription);
      dump("folder desc = "+ folderDescription + "\n");
    },

    setFolderPermissions: function(folderPermissions)
    {
      var permissionsLabel = document.getElementById("folderPermissions.text");
      if (permissionsLabel)
        permissionsLabel.setAttribute("value",folderPermissions);
      dump("folder permissions = "+ folderPermissions + "\n");
    }
};


function folderPropsOKButtonCallback()
{
  if (gMsgFolder)
  {

    // set charset attributes
    var folderCharsetList = document.getElementById("folderCharsetList");
    gMsgFolder.charset = folderCharsetList.getAttribute("value");
    gMsgFolder.charsetOverride = document.getElementById("folderCharsetOverride").checked;

    if(document.getElementById("offline.selectForOfflineFolder").checked ||
      document.getElementById("offline.selectForOfflineNewsgroup").checked)
      gMsgFolder.setFlag(MSG_FOLDER_FLAG_OFFLINE);
    else
      gMsgFolder.clearFlag(MSG_FOLDER_FLAG_OFFLINE);
  }
  window.close();
}

function folderPropsOnLoad()
{
  dump("folder props loaded"+'\n');
  doSetOKCancel(folderPropsOKButtonCallback);
  moveToAlertPosition();

  RDF = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService(Components.interfaces.nsIRDFService);

  // look in arguments[0] for parameters
  if (window.arguments && window.arguments[0]) {
    if ( window.arguments[0].title ) {
      top.window.title = window.arguments[0].title;
    }
    if ( window.arguments[0].okCallback ) {
      top.okCallback = window.arguments[0].okCallback;
    }
  }

  // fill in folder name, based on what they selected in the folder pane
  if (window.arguments[0].preselectedURI) {
    try {
      gPreselectedFolderURI = window.arguments[0].preselectedURI;
    }
    catch (ex) {
    }
  }
  else {
    dump("passed null for preselectedURI, do nothing\n");
  }

  if(window.arguments[0].name)
  {
    var name = document.getElementById("name");
    name.value = window.arguments[0].name;

//  name.setSelectionRange(0,-1);
//  name.focusTextField();
  }

  gServerTypeFolder = window.arguments[0].serverType;

  dump("preselectfolder uri = "+gPreselectedFolderURI+'\n');
  dump("serverType = "+gServerTypeFolder+'\n');

  if (window.arguments && window.arguments[0]) {
    if (window.arguments[0].msgWindow) {
      gParentMsgWindow = window.arguments[0].msgWindow;
    }
  }

  // this hex value come from nsMsgFolderFlags.h
  var folderResource = RDF.GetResource(gPreselectedFolderURI);
    
  if(folderResource)
    gMsgFolder = folderResource.QueryInterface(Components.interfaces.nsIMsgFolder);
  if (!gMsgFolder)
    dump("no gMsgFolder preselectfolder uri = "+gPreselectedFolderURI+'\n');

  if (gMsgFolder) {
    if (gMsgFolder.flags & MSG_FOLDER_FLAG_OFFLINE) {

      if(gServerTypeFolder == "imap" || gServerTypeFolder == "pop3")
        document.getElementById("offline.selectForOfflineFolder").checked = true;

      if(gServerTypeFolder == "nntp")
        document.getElementById("offline.selectForOfflineNewsgroup").checked = true;
    }
    else {
      if(gServerTypeFolder == "imap" || gServerTypeFolder == "pop3")
        document.getElementById("offline.selectForOfflineFolder").checked = false;

      if(gServerTypeFolder == "nntp")
        document.getElementById("offline.selectForOfflineNewsgroup").checked = false;
    }

    // select the menu item 
    var folderCharsetList = document.getElementById("folderCharsetList");
    var elements = folderCharsetList.getElementsByAttribute("value", gMsgFolder.charset);
    folderCharsetList.selectedItem = elements[0];


    // set override checkbox
    document.getElementById("folderCharsetOverride").checked = gMsgFolder.charsetOverride;
  }

  if (gServerTypeFolder == "imap")
  {
    var imapFolder = gMsgFolder.QueryInterface(Components.interfaces.nsIMsgImapMailFolder);
    if (imapFolder)
      imapFolder.fillInFolderProps(gFolderPropsSink);

  }
  
  // select the initial tab
  if (window.arguments[0].tabID) {
    // set index for starting panel on the <tabpanel> element
    var folderPropTabPanel = document.getElementById("folderPropTabPanel");
    folderPropTabPanel.setAttribute("selectedIndex", window.arguments[0].tabIndex);

    try {
      var tab = document.getElementById(window.arguments[0].tabID);
      tab.setAttribute("selected", "true");
    }
    catch (ex) {}
  }
  hideShowControls(gServerTypeFolder);
}

function hideShowControls(serverType)
{
  var controls = document.getElementsByAttribute("hidable", "true");
  var len = controls.length;
  for (var i=0; i<len; i++) {
    var control = controls[i];
    var hideFor = control.getAttribute("hidefor");
    if (!hideFor)
      throw "this should not happen, things that are hidable should have hidefor set";

    var box = getEnclosingContainer(control);

    if (!box)
      throw "this should not happen, things that are hidable should be in a box";

    // hide unsupported server type
    // adding support for hiding multiple server types using hideFor="server1,server2"
    var hideForBool = false;
    var hideForTokens = hideFor.split(",");
    for (var j = 0; j < hideForTokens.length; j++) {
      if (hideForTokens[j] == serverType) {
        hideForBool = true;
        break;
      }
    }

    if (hideForBool) {
      box.setAttribute("hidden", "true");
    }
    else {
      box.removeAttribute("hidden");
    }
  }
  // hide the priviliges button if the imap folder doesn't have an admin url
  // mabye should leave this hidden by default and only show it in this case instead
  var imapFolder = gMsgFolder.QueryInterface(Components.interfaces.nsIMsgImapMailFolder);
  if (imapFolder)
  {
    var privilegesButton = document.getElementById("imap.FolderPrivileges");
    if (privilegesButton)
    {
      if (!imapFolder.hasAdminUrl)
        privilegesButton.setAttribute("hidden", "true");
    }
  }
}

function getEnclosingContainer(startNode) 
{
  var parent = startNode;
  var box; 
  while (parent && parent != document) 
  {
    var isContainer = (parent.getAttribute("iscontrolcontainer") == "true");

    // remember the FIRST container we encounter, or the first controlcontainer
    if (!box || isContainer) box=parent;
        
    // break out with a controlcontainer
    if (isContainer) break;
      parent = parent.parentNode;
  }
  return box;
}

function onOfflineFolderDownload()
{
  // we need to create a progress window and pass that in as the second parameter here.
  gMsgFolder.downloadAllForOffline(null, gParentMsgWindow);
}

function onFolderPrivileges()
{
  var imapFolder = gMsgFolder.QueryInterface(Components.interfaces.nsIMsgImapMailFolder);
  if (imapFolder)
    imapFolder.folderPrivileges(gParentMsgWindow);
}


