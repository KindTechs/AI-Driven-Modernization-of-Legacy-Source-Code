var gPrefInt = null;
var gAvailDirectories = null;
var gCurrentDirectoryServer = null;
var gCurrentDirectoryServerId = null;
var gRefresh = false;
var gNewServer = null;
var gNewServerString = null;
var gFromGlobalPref = false;
var gUpdate = false;
var gDeletedDirectories = new Array();

var gLDAPPrefsService = Components.classes[
                        "@mozilla.org/ldapprefs-service;1"].getService();
gLDAPPrefsService = gLDAPPrefsService.QueryInterface(
                    Components.interfaces.nsILDAPPrefsService);

function onEditDirectories()
{
  var args = {fromGlobalPref: gFromGlobalPref};
  window.openDialog("chrome://messenger/content/addressbook/pref-editdirectories.xul",
                    "editDirectories", "chrome,modal=yes,resizable=no", args);
  if (gRefresh)
  {
    var popup = document.getElementById("directoriesListPopup"); 
    if (popup) 
    { 
       while (popup.childNodes.length)
         popup.removeChild(popup.childNodes[0]);
    } 
    gAvailDirectories = null;
    LoadDirectories(popup);
    gRefresh = false;
  }
}

function enableAutocomplete()
{
  var autocompleteLDAP = document.getElementById("autocompleteLDAP");  
  var directoriesList =  document.getElementById("directoriesList"); 
  var directoriesListPopup = document.getElementById("directoriesListPopup");
  var editButton = document.getElementById("editButton");
//  var autocompleteSkipDirectory = document.getElementById("autocompleteSkipDirectory");

  if (autocompleteLDAP.checked) {
    directoriesList.removeAttribute("disabled");
    directoriesListPopup.removeAttribute("disabled");
    editButton.removeAttribute("disabled");
//    autocompleteSkipDirectory.removeAttribute("disabled");
  }
  else {
    directoriesList.setAttribute("disabled", true);
    directoriesListPopup.setAttribute("disabled", true);
    editButton.setAttribute("disabled", true);
//    autocompleteSkipDirectory.setAttribute("disabled", true);
  }
  // if we do not have any directories disable the dropdown list box
  if (gAvailDirectories.length < 1)
    directoriesList.setAttribute("disabled", true);
  gFromGlobalPref = true;
  LoadDirectories(directoriesListPopup);
}

function setupDirectoriesList()
{
  var override = document.getElementById("identity.overrideGlobalPref").getAttribute("value");
  var autocomplete = document.getElementById("ldapAutocomplete");
  // useGlobalFlag is set when user changes the selectedItem on the radio button and switches
  // to a different pane and switches back in Mail/news AccountSettings
  var useGlobalFlag = document.getElementById("overrideGlobalPref").getAttribute("value");
  // directoryServerFlag is set when user changes the server to None and switches
  // to a different pane and switches back in Mail/news AccountSettings
  var directoryServerFlag = document.getElementById("directoryServer").getAttribute("value");

  if(override == "true" && !useGlobalFlag)
    autocomplete.selectedItem = document.getElementById("directories");
  else
    autocomplete.selectedItem = document.getElementById("useGlobalPref");

  var directoriesList = document.getElementById("directoriesList");
  var directoryServer = 
        document.getElementById("identity.directoryServer").getAttribute('value');
  try {
    var directoryServerString = gPrefInt.getComplexValue(directoryServer + ".description",
                                                         Components.interfaces.nsISupportsWString).data;
  }
  catch(ex) {}
  if (directoryServerFlag || !directoryServerString) {
    document.getElementById("identity.directoryServer").setAttribute("value", "");
    directoryServer = "";
    var addressBookBundle = document.getElementById("bundle_addressBook");
    directoryServerString = addressBookBundle.getString("directoriesListItemNone");
  }
  directoriesList.value = directoryServer;
  directoriesList.label = directoryServerString;
  gFromGlobalPref = false;
}

function createDirectoriesList(flag) 
{
  gFromGlobalPref = flag;
  var directoriesListPopup = document.getElementById("directoriesListPopup");

  if (directoriesListPopup) {
    LoadDirectories(directoriesListPopup);
  }
}

function LoadDirectories(popup)
{
  var prefCount = {value:0};
  var enabled = false;
  var description = "";
  var item;
  var formElement;
  var j=0;
  var arrayOfDirectories;
  var position = 0;
  var dirType = 1;
  var directoriesList;
  var directoryDescription;
  if (!gPrefInt) { 
    try {
      gPrefInt = Components.classes["@mozilla.org/preferences-service;1"]
                           .getService(Components.interfaces.nsIPrefBranch);
    }
    catch (ex) {
      gPrefInt = null;
    }
  }
  if (!gAvailDirectories) {
  try {
    arrayOfDirectories = gLDAPPrefsService.getServerList(gPrefInt, prefCount);
  }
  catch (ex) {
    arrayOfDirectories = null;
  }
  if (arrayOfDirectories) {
    gAvailDirectories = new Array();
    for (var i = 0; i < prefCount.value; i++)
    {
      if ((arrayOfDirectories[i] != "ldap_2.servers.pab") && 
        (arrayOfDirectories[i] != "ldap_2.servers.history")) {
        try{
          position = gPrefInt.getIntPref(arrayOfDirectories[i]+".position");
        }
        catch(ex){
          position = 1;
        }
        try{
          dirType = gPrefInt.getIntPref(arrayOfDirectories[i]+".dirType");
        }
        catch(ex){
          dirType = 1;
        }
        if ((position != 0) && (dirType == 1)) {
          try{
            description = gPrefInt.getComplexValue(arrayOfDirectories[i]+".description",
                                                   Components.interfaces.nsISupportsWString).data;
          }
          catch(ex){
            description="";
          }
          if (description != "") {
            if (popup) {
              item=document.createElement("menuitem");
              item.setAttribute("label", description);
              item.setAttribute("value", arrayOfDirectories[i]);
              popup.appendChild(item);
            }
            gAvailDirectories[j] = {value:arrayOfDirectories[i], label:description};
            j++;
          }
        }
      }
    }
    if (popup && !gFromGlobalPref) 
    {
      // we are in mail/news Account settings
      item=document.createElement("menuitem");
      var addressBookBundle = document.getElementById("bundle_addressBook");
      var directoryName = addressBookBundle.getString("directoriesListItemNone");
      item.setAttribute("label", directoryName);
      item.setAttribute("value", "");
      popup.appendChild(item);
      if (gRefresh) {  
      // gRefresh is true if user edits, removes or adds a directory.
        directoriesList =  document.getElementById("directoriesList");
        directoryDescription = null;
        if(directoriesList.value != "") {
          // make sure the selected directory still exists
          try {
            directoryDescription = gPrefInt.
                     getComplexValue(directoriesList.value + ".description",
                                     Components.interfaces.nsISupportsWString).data;
          }
          catch (ex) {}
        }
        if(!directoryDescription) {
          // if selected directory doesn't exist, set it to none
          directoriesList.value = "";
          addressBookBundle = document.getElementById("bundle_addressBook");
          directoriesList.label = addressBookBundle.
                          getString("directoriesListItemNone");
        }
        else {
          directoriesList.label = directoryDescription;
          directoriesList.value = directoriesList.value;
        }
      }
    }
    if (popup && gFromGlobalPref) {
    // we are in global preferences-> Addressing pane.
      directoriesList =  document.getElementById("directoriesList");
      if (gRefresh) {
        // gRefresh is true if user edits, removes or adds a directory.
        directoryDescription = null;
        if(directoriesList.label != "") {
          // make sure the selected directory still exists
          try {
            directoryDescription = gPrefInt.
                     getComplexValue(directoriesList.value + ".description",
                                     Components.interfaces.nsISupportsWString).data;
          }
          catch (ex) {}
        }
        if(!directoryDescription) {
          // if selected directory doesn't exist, 
          // set it the first one in the list of directories 
          // if we have  atleast one directory.
          // or else set it to ""
          if (gAvailDirectories.length) {
            directoriesList.label = gAvailDirectories[0].label;
            directoriesList.value = gAvailDirectories[0].value;
            directoriesList.removeAttribute("disabled");
          }
          else {
            directoriesList.label = "";
            directoriesList.value = null;
            directoriesList.setAttribute("disabled", true);
          }
        }
        else {
          directoriesList.label = directoryDescription;
          directoriesList.value = directoriesList.value;
        }
        return;
      }
      var pref_string_title = "ldap_2.autoComplete.directoryServer";
      try {
        var directoryServer = gPrefInt.getCharPref(pref_string_title);
      }
      catch (ex)
      {
        directoryServer = "";
      }
      if (directoryServer != "")
      {
        pref_string_title = directoryServer + ".description";
        try {
          description = gPrefInt.getComplexValue(pref_string_title,
                                                 Components.interfaces.nsISupportsWString).data;
        }
        catch (ex) {
          description = "";
        } 
      }
      if ((directoryServer != "") && (description != ""))
      {
        directoriesList.label = description;
        directoriesList.value = directoryServer;
      }
      else if(gAvailDirectories.length) {
         directoriesList.label = gAvailDirectories[0].label;
         directoriesList.value = gAvailDirectories[0].value;
         gPrefInt.setCharPref("ldap_2.autoComplete.directoryServer", 
                              gAvailDirectories[0].value);
      }
      else {
        directoriesList.label = "";
        directoriesList.value = null;
        gPrefInt.setCharPref("ldap_2.autoComplete.directoryServer", "");
      }
    }
  }
  }
}

function onInitEditDirectories()
{
  var listbox = document.getElementById("directoriesList");
  gFromGlobalPref = window.arguments[0].fromGlobalPref;
  if (listbox) {
    LoadDirectoriesList(listbox);
  }
}

function LoadDirectoriesList(listbox)
{
  LoadDirectories();
  if (listbox && gAvailDirectories)
  {
    for (var i=0; i<gAvailDirectories.length; i++)
    {
      var item = document.createElement('listitem');

      item.setAttribute('label', gAvailDirectories[i].label);
      item.setAttribute('string', gAvailDirectories[i].value);

      listbox.appendChild(item);
    }
  }
}

function selectDirectory()
{
  var directoriesList = document.getElementById("directoriesList");
  if(directoriesList && directoriesList.selectedItems 
     && directoriesList.selectedItems.length)
  {
    gCurrentDirectoryServer = 
	  directoriesList.selectedItems[0].getAttribute('label');
    gCurrentDirectoryServerId = 
	  directoriesList.selectedItems[0].getAttribute('string');
  }
  else
  {
    gCurrentDirectoryServer = null;
    gCurrentDirectoryServerId = null;
  }

  var editButton = document.getElementById("editButton");
  var removeButton = document.getElementById("removeButton");
  if(gCurrentDirectoryServer && gCurrentDirectoryServerId) {
    editButton.removeAttribute("disabled");
    removeButton.removeAttribute("disabled");
  }
  else {
    editButton.setAttribute("disabled", true);
    removeButton.setAttribute("disabled", true);
  }
}

function newDirectory()
{
  window.openDialog("chrome://messenger/content/addressbook/pref-directory-add.xul",
                    "addDirectory", "chrome,modal=yes,resizable=no,centerscreen");
  if(gUpdate && gNewServer && gNewServerString) {
    var listbox = document.getElementById("directoriesList");
    var item = document.createElement('listitem');

    item.setAttribute('label', gNewServer);
    item.setAttribute('string', gNewServerString);

    listbox.appendChild(item);
    gNewServer = null;
    gNewServerString = null;
    window.opener.gRefresh = true;
  }
}

function editDirectory()
{
  var args = { selectedDirectory: null,
               selectedDirectoryString: null,
               result: false};
  if(gCurrentDirectoryServer && gCurrentDirectoryServerId) {
    args.selectedDirectory = gCurrentDirectoryServer;
    args.selectedDirectoryString = gCurrentDirectoryServerId;
    window.openDialog("chrome://messenger/content/addressbook/pref-directory-add.xul",
                      "editDirectory", "chrome,modal=yes,resizable=no,centerscreen", args);
  }
  if(gUpdate) 
  {
    // directory server properties have changed. So, update the  
    // LDAP Directory Servers dialog.  
    var directoriesList = document.getElementById("directoriesList"); 
    var selectedNode = directoriesList.selectedItems[0]; 
    selectedNode.setAttribute('label', gNewServer); 
    selectedNode.setAttribute('string', gNewServerString);
    
    // window.opener is either global pref window or 
    // mail/news account settings window.
    // set window.opener.gRefresh to true such that the 
    // dropdown list box gets updated 
    window.opener.gRefresh = true; 
  } 
}

function removeDirectory()
{
  var directoriesList = document.getElementById("directoriesList");
  var selectedNode = directoriesList.selectedItems[0];
    
  var nextNode = selectedNode.nextSibling;
  if (!nextNode)
    if (selectedNode.previousSibling)
      nextNode = selectedNode.previousSibling;

  if(gCurrentDirectoryServer && gCurrentDirectoryServerId) {
    var len= gDeletedDirectories.length;
    gDeletedDirectories[len] = gCurrentDirectoryServerId;
  }
  directoriesList.removeChild(selectedNode);
  if (nextNode)
    directoriesList.selectItem(nextNode)
}

//  remove all the directories that are selected for deletion from preferences
//  check if the deleted directory is selected for autocompletion in global
//  or identity prefs. If so change the pref to ""
function onAccept()
{
  var len = gDeletedDirectories.length;
  if (len) {
    try {
      var directoryServer = gPrefInt.getCharPref("ldap_2.autoComplete.directoryServer");
    }
    catch(ex)  {
      directoryServer = null;
    }
    var am = Components.classes["@mozilla.org/messenger/account-manager;1"]
                 .getService(Components.interfaces.nsIMsgAccountManager);
    if (am) { 
      var allIdentities = am.allIdentities;
      var identitiesCount = allIdentities.Count();
      var identityServer = new Array();
      var currentIdentity = null;
      var j=0;
      for (j=0; j< identitiesCount; j++) {
        currentIdentity = allIdentities.QueryElementAt(j, Components.interfaces.nsIMsgIdentity);
        identityServer[j] = {server:currentIdentity.directoryServer, deleted:false};
      }
      var deletedGlobal = false;
      for (var i=0; i< len; i++){
        if (!deletedGlobal && directoryServer && (gDeletedDirectories[i] == directoryServer)) {
          gPrefInt.setCharPref("ldap_2.autoComplete.directoryServer", "");
          deletedGlobal = true;
        }
        for (j=0; j<identitiesCount; j++){
          if (identityServer[j].server && !identityServer[j].deleted && (gDeletedDirectories[i] == identityServer[j].server)) {
            identityServer[j].server = "";
            identityServer[j].deleted = true;
          }
        }
        gPrefInt.deleteBranch(gDeletedDirectories[i]);
      }
      var svc = Components.classes["@mozilla.org/preferences-service;1"]
                        .getService(Components.interfaces.nsIPrefService);
      svc.savePrefFile(null);
    }
  }
  window.opener.gRefresh = true;
  return true;
}
