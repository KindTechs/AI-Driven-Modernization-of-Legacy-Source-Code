/*
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
 *
 * Contributor(s):
 */

var gMisspelledWord;
var gSpellChecker;
var gAllowSelectWord = true;
var gPreviousReplaceWord = "";
var gFirstTime = true;
var gSendMailMessageMode = false;

// dialog initialization code
function Startup()
{
  if (!InitEditorShell())
    return;

  // Get the spellChecker shell
  gSpellChecker = editorShell.QueryInterface(Components.interfaces.nsIEditorSpellCheck);
  if (!gSpellChecker) {
    dump("SpellChecker not found!!!\n");
    window.close();
    return;
  }

  // Start the spell checker module.
  try {
   gSpellChecker.InitSpellChecker();

   // XXX: We need to read in a pref here so we can set the
   //      default language for the spellchecker!
   // gSpellChecker.SetCurrentDictionary();
  }
  catch(ex) {
   dump("*** Exception error: InitSpellChecker\n");
    window.close();
    return;
  }

  gDialog.MisspelledWordLabel = document.getElementById("MisspelledWordLabel");
  gDialog.MisspelledWord      = document.getElementById("MisspelledWord");
  gDialog.ReplaceButton       = document.getElementById("Replace");
  gDialog.IgnoreButton        = document.getElementById("Ignore");
  gDialog.StopButton          = document.getElementById("Stop");
  gDialog.CloseButton         = document.getElementById("Close");
  gDialog.ReplaceWordInput    = document.getElementById("ReplaceWordInput");
  gDialog.SuggestedList       = document.getElementById("SuggestedList");
  gDialog.LanguageMenulist    = document.getElementById("LanguageMenulist");

  // Fill in the language menulist and sync it up
  // with the spellchecker's current language.

  var curLang;

  try {
    curLang = gSpellChecker.GetCurrentDictionary();
  } catch(ex) {
    curLang = "";
  }

  InitLanguageMenu(curLang);
  
  // Get the first misspelled word and setup all UI
  NextWord();

  // When startup param is true, setup different UI when spell checking 
  //   just before sending mail message  
  gSendMailMessageMode = window.arguments[0];
  if (gSendMailMessageMode)
  {
    // If no misspelled words found, simply close dialog and send message
    if (!gMisspelledWord)
    {
      onClose();
      return;
    }

    // "Close" button becomes "Send"
    gDialog.CloseButton.setAttribute("label", GetString("Send"));
  }
  else
  {
    // Normal spell checking - hide the "Stop" button
    // (Note that this button is the "Cancel" button for
    //  Esc keybinding and related window close actions)
    gDialog.StopButton.setAttribute("hidden", "true");
  }

  // Clear flag that determines message when
  //  no misspelled word is found
  //  (different message when used for the first time)
  gFirstTime = false;
}

function InitLanguageMenu(curLang)
{

  var o1 = {};
  var o2 = {};

  // Get the list of dictionaries from
  // the spellchecker.

  try {
    gSpellChecker.GetDictionaryList(o1, o2);
  } catch(ex) {
    dump("Failed to get DictionaryList!\n");
    return;
  }

  var dictList = o1.value;
  var count    = o2.value;

  // Load the string bundles that will help us map
  // RFC 1766 strings to UI strings.

  var languageBundle;
  var regionBundle;
  var menuStr2;
  var isoStrArray;
  var defaultIndex = 0;
  var langId;

  // Try to load the language string bundle.

  try {
    languageBundle = srGetStrBundle("chrome://global/locale/languageNames.properties");
  } catch(ex) {
    languageBundle = null;
  }

  // If we have a language string bundle, try to load the region string bundle.

  if (languageBundle)
  {
    try {
      regionBundle = srGetStrBundle("chrome://global/locale/regionNames.properties");
    } catch(ex) {
      regionBundle = null;
    }
  }
  var i;
  for (i = 0; i < dictList.length; i++)
  {
    try {
      langId = dictList[i];
      isoStrArray = dictList[i].split("-");

      dictList[i] = new Array(2); // first subarray element - pretty name
      dictList[i][1] = langId;    // second subarray element - language ID

      if (languageBundle && isoStrArray[0])
        dictList[i][0] = languageBundle.GetStringFromName(isoStrArray[0].toLowerCase());

      if (regionBundle && dictList[i][0] && isoStrArray.length > 1 && isoStrArray[1])
      {
        menuStr2 = regionBundle.GetStringFromName(isoStrArray[1].toLowerCase());
        if (menuStr2)
          dictList[i][0] = dictList[i][0] + "/" + menuStr2;
      }

      if (!dictList[i][0])
        dictList[i][0] = dictList[i][1];
    } catch (ex) {
      // GetStringFromName throws an exception when
      // a key is not found in the bundle. In that
      // case, just use the original dictList string.

      dictList[i][0] = dictList[i][1];
    }
  }
  
  // note this is not locale-aware collation, just simple ASCII-based sorting
  // we really need to add loacel-aware JS collation, see bug XXXXX
  dictList.sort();

  for (i = 0; i < dictList.length; i++)
  {
    AppendLabelAndValueToMenulist(gDialog.LanguageMenulist, dictList[i][0], dictList[i][1]);
    if (curLang && dictList[i][1] == curLang)
      defaultIndex = i+2; //first two items are pre-populated and fixed
  }

  // Now make sure the correct item in the menu list is selected.

  if (dictList.length > 0)
    gDialog.LanguageMenulist.selectedIndex = defaultIndex;
}

function DoEnabling()
{
  if (!gMisspelledWord)
  {
    // No more misspelled words
    gDialog.MisspelledWord.setAttribute("value",GetString( gFirstTime ? "NoMisspelledWord" : "CheckSpellingDone"));

    gDialog.ReplaceButton.removeAttribute("default");
    gDialog.IgnoreButton.removeAttribute("default");

    gDialog.CloseButton.setAttribute("default","true");
    // Shouldn't have to do this if "default" is true?
    gDialog.CloseButton.focus();

    SetElementEnabledById("MisspelledWordLabel", false);
    SetElementEnabledById("ReplaceWordLabel", false);
    SetElementEnabledById("ReplaceWordInput", false);
    SetElementEnabledById("CheckWord", false);
    SetElementEnabledById("SuggestedListLabel", false);
    SetElementEnabledById("SuggestedList", false);
    SetElementEnabledById("Ignore", false);
    SetElementEnabledById("IgnoreAll", false);
    SetElementEnabledById("Replace", false);
    SetElementEnabledById("ReplaceAll", false);
    SetElementEnabledById("AddToDictionary", false);
  } else {
    SetElementEnabledById("MisspelledWordLabel", true);
    SetElementEnabledById("ReplaceWordLabel", true);
    SetElementEnabledById("ReplaceWordInput", true);
    SetElementEnabledById("CheckWord", true);
    SetElementEnabledById("SuggestedListLabel", true);
    SetElementEnabledById("SuggestedList", true);
    SetElementEnabledById("Ignore", true);
    SetElementEnabledById("IgnoreAll", true);
    SetElementEnabledById("AddToDictionary", true);

    gDialog.CloseButton.removeAttribute("default");
    SetReplaceEnable();
  }
}

function NextWord()
{
  gMisspelledWord = gSpellChecker.GetNextMisspelledWord();
  SetWidgetsForMisspelledWord();
}

function SetWidgetsForMisspelledWord()
{
  gDialog.MisspelledWord.setAttribute("value", TruncateStringAtWordEnd(gMisspelledWord, 30, true));


  // Initial replace word is misspelled word
  gDialog.ReplaceWordInput.value = gMisspelledWord;
  gPreviousReplaceWord = gMisspelledWord;

  // This sets gDialog.ReplaceWordInput to first suggested word in list
  FillSuggestedList(gMisspelledWord);

  DoEnabling();

  if (gMisspelledWord)
    SetTextboxFocus(gDialog.ReplaceWordInput);
}

function CheckWord()
{
  word = gDialog.ReplaceWordInput.value;
  if (word) 
  {
    isMisspelled = gSpellChecker.CheckCurrentWord(word);
    if (isMisspelled)
    {
      FillSuggestedList(word);
      SetReplaceEnable();
    } 
    else 
    {
      ClearListbox(gDialog.SuggestedList);
      var item = gDialog.SuggestedList.appendItem(GetString("CorrectSpelling"), "");
      if (item) item.setAttribute("disabled", "true");
      // Suppress being able to select the message text
      gAllowSelectWord = false;
    }
  }
}

function SelectSuggestedWord()
{
  if (gAllowSelectWord)
  {
    var selectedItem
    if (gDialog.SuggestedList.selectedItem)
    {
      var selValue = gDialog.SuggestedList.selectedItem.getAttribute("label");
      gDialog.ReplaceWordInput.value = selValue;
      gPreviousReplaceWord = selValue;
    }
    else
    {
      gDialog.ReplaceWordInput.value = gPreviousReplaceWord;
    }
    SetReplaceEnable();
  }
}

function ChangeReplaceWord()
{
  // Calling this triggers SelectSuggestedWord(),
  //  so temporarily suppress the effect of that
  var saveAllow = gAllowSelectWord;
  gAllowSelectWord = false;

  // Select matching word in list
  var newIndex = -1;
  var newSelectedItem;
  var replaceWord = TrimString(gDialog.ReplaceWordInput.value);
  if (replaceWord)
  {
    for (var i = 0; i < gDialog.SuggestedList.getRowCount(); i++)
    {
      var item = gDialog.SuggestedList.getItemAtIndex(i);
      if (item.getAttribute("label") == replaceWord)
      {
        newSelectedItem = item;
        break;
      }
    }
  }
  gDialog.SuggestedList.selectedItem = newSelectedItem;

  gAllowSelectWord = saveAllow;

  // Remember the new word
  gPreviousReplaceWord = gDialog.ReplaceWordInput.value;

  SetReplaceEnable();
}

function Ignore()
{
  NextWord();
}

function IgnoreAll()
{
  if (gMisspelledWord) {
    gSpellChecker.IgnoreWordAllOccurrences(gMisspelledWord);
  }
  NextWord();
}

function Replace()
{
  var newWord = gDialog.ReplaceWordInput.value;
  if (gMisspelledWord && gMisspelledWord != newWord)
  {
    editorShell.BeginBatchChanges();
    var isMisspelled = gSpellChecker.ReplaceWord(gMisspelledWord, newWord, false);
    editorShell.EndBatchChanges();
  }
  NextWord();
}

function ReplaceAll()
{
  var newWord = gDialog.ReplaceWordInput.value;
  if (gMisspelledWord && gMisspelledWord != newWord)
  {
    editorShell.BeginBatchChanges();
    isMisspelled = gSpellChecker.ReplaceWord(gMisspelledWord, newWord, true);
    editorShell.EndBatchChanges();
  }
  NextWord();
}

function AddToDictionary()
{
  if (gMisspelledWord) {
    gSpellChecker.AddWordToDictionary(gMisspelledWord);
  }
  NextWord();
}

function EditDictionary()
{
  window.openDialog("chrome://editor/content/EdDictionary.xul", "_blank", "chrome,close,titlebar,modal", "", gMisspelledWord);
}

function SelectLanguage()
{
  try {
    var item = gDialog.LanguageMenulist.selectedItem;
    if (item.value != "more-cmd")
      gSpellChecker.SetCurrentDictionary(item.value);
    else
      window.openDialog( getBrowserURL(), "_blank", "chrome,all,dialog=no,modal", xlateURL('urn:clienturl:composer:spellcheckers'));
  } catch (ex) {
    dump(ex);
  }
}

function Recheck()
{
  //TODO: Should we bother to add a "Recheck" method to interface?
  try {
    var curLang = gSpellChecker.GetCurrentDictionary();

    gSpellChecker.UninitSpellChecker();
    gSpellChecker.InitSpellChecker();
    gSpellChecker.SetCurrentDictionary(curLang);
    gMisspelledWord = gSpellChecker.GetNextMisspelledWord();
    SetWidgetsForMisspelledWord();
  } catch(ex) {
    dump(ex);
  }
}

function FillSuggestedList(misspelledWord)
{
  var list = gDialog.SuggestedList;

  // Clear the current contents of the list
  gAllowSelectWord = false;
  ClearListbox(list);
  var item;

  if (misspelledWord.length > 0)
  {
    // Get suggested words until an empty string is returned
    var count = 0;
    var firstWord = 0;
    do {
      var word = gSpellChecker.GetSuggestedWord();
      if (count==0)
        firstWord = word;
      if (word.length > 0)
      {
        list.appendItem(word, "");
        count++;
      }
    } while (word.length > 0);

    if (count == 0)
    {
      // No suggestions - show a message but don't let user select it
      item = list.appendItem(GetString("NoSuggestedWords"));
      if (item) item.setAttribute("disabled", "true");
      gAllowSelectWord = false;
    } else {
      gAllowSelectWord = true;
      // Initialize with first suggested list by selecting it
      gDialog.SuggestedList.selectedIndex = 0;
    }
  } 
  else
  {
    item = list.appendItem("", "");
    if (item)
      item.setAttribute("disabled", "true");
  }
}

function SetReplaceEnable()
{
  // Enable "Change..." buttons only if new word is different than misspelled
  var newWord = gDialog.ReplaceWordInput.value;
  var enable = newWord.length > 0 && newWord != gMisspelledWord;
  SetElementEnabledById("Replace", enable);
  SetElementEnabledById("ReplaceAll", enable);
  if (enable)
  {
    gDialog.ReplaceButton.setAttribute("default","true");
    gDialog.IgnoreButton.removeAttribute("default");
  }
  else
  {
    gDialog.IgnoreButton.setAttribute("default","true");
    gDialog.ReplaceButton.removeAttribute("default");
  }
}

function doDefault()
{
  if (gDialog.ReplaceButton.getAttribute("default") == "true")
    Replace();
  else if (gDialog.IgnoreButton.getAttribute("default") == "true")
    Ignore();
  else if (gDialog.CloseButton.getAttribute("default") == "true")
    onClose();

  return false;
}

function CancelSpellCheck()
{
  gSpellChecker.UninitSpellChecker();

  // Signal to calling window that we canceled
  window.opener.cancelSendMessage = true;
  return true;
}

function onClose()
{
  gSpellChecker.UninitSpellChecker();
  window.opener.cancelSendMessage = false;
  window.close();
}
