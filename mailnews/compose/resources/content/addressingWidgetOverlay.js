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

top.MAX_RECIPIENTS = 0;

var inputElementType = "";
var selectElementType = "";
var selectElementIndexTable = null;

var gNmberOfCols = 0;

var gDragService = Components.classes["@mozilla.org/widget/dragservice;1"].getService();
gDragService = gDragService.QueryInterface(Components.interfaces.nsIDragService);

/**
 * global variable inherited from MsgComposeCommands.js
 *
 
 var sPrefs;
 var gPromptService;
 var gMsgCompose;
 
 */

var test_addresses_sequence = false;
try {
  if (sPrefs)
    test_addresses_sequence = sPrefs.getBoolPref("mail.debug.test_addresses_sequence");
}
catch (ex) {}

function awGetMaxRecipients()
{
  return top.MAX_RECIPIENTS;
}

function awGetNumberOfCols()
{
  if (gNmberOfCols == 0)
  {
    var listbox = document.getElementById('addressingWidget');
    var listCols = listbox.getElementsByTagName('listcol');
    gNmberOfCols = listCols.length;
    if (!gNmberOfCols)
      gNmberOfCols = 1;  /* if no cols defined, that means we have only one! */
  }

  return gNmberOfCols;
}

function awInputElementName()
{
    if (inputElementType == "")
        inputElementType = document.getElementById("addressCol2#1").localName;
    return inputElementType;
}

function awSelectElementName()
{
    if (selectElementType == "")
        selectElementType = document.getElementById("addressCol1#1").localName;
    return selectElementType;
}

function awGetSelectItemIndex(itemData)
{
    if (selectElementIndexTable == null)
    {
      selectElementIndexTable = new Object();
      var selectElem = document.getElementById("addressCol1#1");
        for (var i = 0; i < selectElem.childNodes[0].childNodes.length; i ++)
    {
            var aData = selectElem.childNodes[0].childNodes[i].getAttribute("value");
            selectElementIndexTable[aData] = i;
        }
    }
    return selectElementIndexTable[itemData];
}

function Recipients2CompFields(msgCompFields)
{
  if (msgCompFields)
  {
    var i = 1;
    var addrTo = "";
    var addrCc = "";
    var addrBcc = "";
    var addrReply = "";
    var addrNg = "";
    var addrFollow = "";
    var addrOther = "";
    var to_Sep = "";
    var cc_Sep = "";
    var bcc_Sep = "";
    var reply_Sep = "";
    var ng_Sep = "";
    var follow_Sep = "";

    var inputField;
      while ((inputField = awGetInputElement(i)))
      {
        var fieldValue = inputField.value;

        if (fieldValue == null)
          fieldValue = inputField.getAttribute("value");

        if (fieldValue != "")
        {
          switch (awGetPopupElement(i).selectedItem.getAttribute("value"))
          {
            case "addr_to"      : addrTo += to_Sep + fieldValue; to_Sep = ",";          break;
            case "addr_cc"      : addrCc += cc_Sep + fieldValue; cc_Sep = ",";          break;
            case "addr_bcc"     : addrBcc += bcc_Sep + fieldValue; bcc_Sep = ",";       break;
            case "addr_reply"   : addrReply += reply_Sep + fieldValue; reply_Sep = ",";     break;
            case "addr_newsgroups"  : addrNg += ng_Sep + fieldValue; ng_Sep = ",";          break;
            case "addr_followup"  : addrFollow += follow_Sep + fieldValue; follow_Sep = ",";    break;
          case "addr_other"   : addrOther += sOther_header + ": " + fieldValue + "\n";     break;
          }
        }
        i ++;
      }

      msgCompFields.to = addrTo;
      msgCompFields.cc = addrCc;
      msgCompFields.bcc = addrBcc;
      msgCompFields.replyTo = addrReply;
      msgCompFields.newsgroups = addrNg;
      msgCompFields.followupTo = addrFollow;
      msgCompFields.otherRandomHeaders = addrOther;
  }
  else
    dump("Message Compose Error: msgCompFields is null (ExtractRecipients)");
}

function CompFields2Recipients(msgCompFields, msgType)
{
  if (msgCompFields) {
    var listbox = document.getElementById('addressingWidget');
    var newListBoxNode = listbox.cloneNode(false);
    var listBoxColsClone = listbox.firstChild.cloneNode(true);
    newListBoxNode.appendChild(listBoxColsClone);
    var templateNode = listbox.getElementsByTagName("listitem")[0];
    
    top.MAX_RECIPIENTS = 0;
    var msgReplyTo = msgCompFields.replyTo;
    var msgTo = msgCompFields.to;
    var msgCC = msgCompFields.cc;
    var msgBCC = msgCompFields.bcc;
    var msgRandomHeaders = msgCompFields.otherRandomHeaders;
    var msgNewsgroups = msgCompFields.newsgroups;
    var msgFollowupTo = msgCompFields.followupTo;
    if(msgReplyTo)
      awSetInputAndPopupFromArray(msgCompFields.SplitRecipients(msgReplyTo, false), 
                                  "addr_reply", newListBoxNode, templateNode);
    if(msgTo)
      awSetInputAndPopupFromArray(msgCompFields.SplitRecipients(msgTo, false), 
                                  "addr_to", newListBoxNode, templateNode);
    if(msgCC)
      awSetInputAndPopupFromArray(msgCompFields.SplitRecipients(msgCC, false),
                                  "addr_cc", newListBoxNode, templateNode);
    if(msgBCC)
      awSetInputAndPopupFromArray(msgCompFields.SplitRecipients(msgBCC, false),
                                  "addr_bcc", newListBoxNode, templateNode);
    if(msgRandomHeaders)
      awSetInputAndPopup(msgRandomHeaders, "addr_other", newListBoxNode, templateNode);
    if(msgNewsgroups)
      awSetInputAndPopup(msgNewsgroups, "addr_newsgroups", newListBoxNode, templateNode);
    if(msgFollowupTo)
      awSetInputAndPopup(msgFollowupTo, "addr_followup", newListBoxNode, templateNode);

    //If it's a new message, we need to add an extrat empty recipient.
    if (!msgTo && !msgNewsgroups)
      _awSetInputAndPopup("", "addr_to", newListBoxNode, templateNode);
    // dump("replacing child in comp fields 2 recips \n");
    var parent = listbox.parentNode;
    parent.replaceChild(newListBoxNode, listbox);
    awFitDummyRows(2);
    setTimeout("awFinishCopyNodes();", 0);
  }
}

function awSetInputAndPopupValue(inputElem, inputValue, popupElem, popupValue, rowNumber)
{
  // remove leading spaces
  while (inputValue && inputValue[0] == " " )
    inputValue = inputValue.substring(1, inputValue.length);

  inputElem.setAttribute("value", inputValue);
  inputElem.value = inputValue;

  popupElem.selectedItem = popupElem.childNodes[0].childNodes[awGetSelectItemIndex(popupValue)];

  if (rowNumber >= 0)
  {
    inputElem.setAttribute("id", "addressCol2#" + rowNumber);
    popupElem.setAttribute("id", "addressCol1#" + rowNumber);
  }

  _awSetAutoComplete(popupElem, inputElem);
}

function _awSetInputAndPopup(inputValue, popupValue, parentNode, templateNode)
{
    top.MAX_RECIPIENTS++;

    var newNode = templateNode.cloneNode(true);
    parentNode.appendChild(newNode); // we need to insert the new node before we set the value of the select element!

    var input = newNode.getElementsByTagName(awInputElementName());
    var select = newNode.getElementsByTagName(awSelectElementName());

    if (input && input.length == 1 && select && select.length == 1)
      awSetInputAndPopupValue(input[0], inputValue, select[0], popupValue, top.MAX_RECIPIENTS)
}

function awSetInputAndPopup(inputValue, popupValue, parentNode, templateNode)
{
  if ( inputValue && popupValue )
  {
    var addressArray = inputValue.split(",");

    for ( var index = 0; index < addressArray.length; index++ )
        _awSetInputAndPopup(addressArray[index], popupValue, parentNode, templateNode);
  }
}

function awSetInputAndPopupFromArray(inputArray, popupValue, parentNode, templateNode)
{
  if ( inputArray && popupValue )
  {
    for ( var index = 0; index < inputArray.count; index++ )
        _awSetInputAndPopup(inputArray.StringAt(index), popupValue, parentNode, templateNode);
  }
}

function awRemoveRecipients(msgCompFields, recipientType, recipientsList)
{
  if (!msgCompFields)
    return;

  var recipientArray = msgCompFields.SplitRecipients(recipientsList, false);
  if (! recipientArray)
    return;

  for ( var index = 0; index < recipientArray.count; index++ )
    for (var row = 1; row <= top.MAX_RECIPIENTS; row ++)
    {
      var popup = awGetPopupElement(row);
      if (popup.selectedItem.getAttribute("value") == recipientType)
      {
        var input = awGetInputElement(row);
        if (input.value == recipientArray.StringAt(index))
        {
          awSetInputAndPopupValue(input, "", popup, "addr_to", -1);
          break;
        }
      }
    }
}

function awAddRecipients(msgCompFields, recipientType, recipientsList)
{
  if (!msgCompFields)
    return;

  var recipientArray = msgCompFields.SplitRecipients(recipientsList, false);
  if (! recipientArray)
    return;

  for ( var index = 0; index < recipientArray.count; index++ )
  {
    for (var row = 1; row <= top.MAX_RECIPIENTS; row ++)
    {
      if (awGetInputElement(row).value == "")
        break;
    }
    if (row > top.MAX_RECIPIENTS)
      awAppendNewRow(false);

    awSetInputAndPopupValue(awGetInputElement(row), recipientArray.StringAt(index), awGetPopupElement(row), recipientType, row);

    /* be sure we still have an empty row left at the end */
    if (row == top.MAX_RECIPIENTS)
    {
      awAppendNewRow(true);
      awSetInputAndPopupValue(awGetInputElement(top.MAX_RECIPIENTS), "", awGetPopupElement(top.MAX_RECIPIENTS), "addr_to", top.MAX_RECIPIENTS);
    }
  }
}

function awTestRowSequence()
{
  /*
    This function is for debug and testing purpose only, normal user should not run it!

    Everytime we insert or delete a row, we must be sure we didn't break the ID sequence of
    the addressing widget rows. This function will run a quick test to see if the sequence still ok

    You need to define the pref mail.debug.test_addresses_sequence to true in order to activate it
  */

  if (! test_addresses_sequence)
    return true;

  /* debug code to verify the sequence still good */

  var listbox = document.getElementById('addressingWidget');
  var listitems = listbox.getElementsByTagName('listitem');
  if (listitems.length >= top.MAX_RECIPIENTS )
  {
    for (var i = 1; i <= listitems.length; i ++)
    {
      var item = listitems [i - 1];
      var inputID = item.getElementsByTagName(awInputElementName())[0].getAttribute("id").split("#")[1];
      var popupID = item.getElementsByTagName(awSelectElementName())[0].getAttribute("id").split("#")[1];
      if (inputID != i || popupID != i)
      {
        dump("#ERROR: sequence broken at row " + i + ", inputID=" + inputID + ", popupID=" + popupID + "\n");
        return false;
    }
    dump("---SEQUENCE OK---\n");
    return true;
  }
  }
  else
    dump("#ERROR: listitems.length(" + listitems.length + ") < top.MAX_RECIPIENTS(" + top.MAX_RECIPIENTS + ")\n");

  return false;
}

function awResetAllRows()
{
  var maxRecipients = top.MAX_RECIPIENTS;
  
  for (var row = 1; row <= maxRecipients ; row ++)
  {
    awGetInputElement(row).value = "";
    awGetPopupElement(row).selectedIndex = 0;
  }
}

function awCleanupRows()
{
  var maxRecipients = top.MAX_RECIPIENTS;
  var rowID = 1;

  for (var row = 1; row <= maxRecipients; row ++)
  {
    var inputElem = awGetInputElement(row);
    if (inputElem.value == "" && row < maxRecipients)
      awRemoveRow(row, 1);
    else
    {
      inputElem.setAttribute("id", "addressCol2#" + rowID);
      awGetPopupElement(row).setAttribute("id", "addressCol1#" + rowID);
      rowID ++;
    }
  }

  awTestRowSequence();
}

function awDeleteRow(rowToDelete)
{
  /* When we delete a row, we must reset the id of others row in order to not break the sequence */
  var maxRecipients = top.MAX_RECIPIENTS;
  awRemoveRow(rowToDelete);

  var numberOfCols = awGetNumberOfCols();
  for (var row = rowToDelete + 1; row <= maxRecipients; row ++)
    for (var col = 1; col <= numberOfCols; col++)
      awGetElementByCol(row, col).setAttribute("id", "addressCol" + (col) + "#" + (row-1));

  awTestRowSequence();
}

function awClickEmptySpace(target, setFocus)
{
  if (target == null ||
      (target.localName != "listboxbody" &&
      target.localName != "listcell" &&
      target.localName != "listitem"))
    return;

  var lastInput = awGetInputElement(top.MAX_RECIPIENTS);

  if ( lastInput && lastInput.value )
    awAppendNewRow(setFocus);
  else
    if (setFocus)
      awSetFocus(top.MAX_RECIPIENTS, lastInput);
}

function awReturnHit(inputElement)
{
  var row = awGetRowByInputElement(inputElement);
  var nextInput = awGetInputElement(row+1);

  if ( !nextInput )
  {
    if ( inputElement.value )
      awAppendNewRow(true);
    else // No address entered, switch to Subject field
    {
      var subjectField = document.getElementById( 'msgSubject' );
      subjectField.select();
      subjectField.focus();
    }
  }
  else
  {
    nextInput.select();
    awSetFocus(row+1, nextInput);
  }
}

function awDeleteHit(inputElement)
{
  var row = awGetRowByInputElement(inputElement);

  /* 1. don't delete the row if it's the last one remaining, just reset it! */
  if (top.MAX_RECIPIENTS <= 1)
  {
    inputElement.value = "";
    return;
  }

  /* 2. Set the focus to the previous field if possible */
  if (row > 1)
    awSetFocus(row - 1, awGetInputElement(row - 1))
  else
    awSetFocus(1, awGetInputElement(2))   /* We have to cheat a little bit because the focus will */
                                          /* be set asynchronusly after we delete the current row, */
                                          /* therefore the row number still the same! */

  /* 3. Delete the row */
  awDeleteRow(row);
}

function awInputChanged(inputElement)
{
  dump("awInputChanged\n");
//  AutoCompleteAddress(inputElement);

  //Do we need to add a new row?
  var lastInput = awGetInputElement(top.MAX_RECIPIENTS);
  if ( lastInput && lastInput.value && !top.doNotCreateANewRow)
    awAppendNewRow(false);
  top.doNotCreateANewRow = false;
}

function awAppendNewRow(setFocus)
{
  var listbox = document.getElementById('addressingWidget');
  var listitem1 = awGetListItem(1);

  if ( listbox && listitem1 )
  {
    var lastRecipientType = awGetPopupElement(top.MAX_RECIPIENTS).selectedItem.getAttribute("value");

    var nextDummy = awGetNextDummyRow();
    var newNode = listitem1.cloneNode(true);
    if (nextDummy)
      listbox.replaceChild(newNode, nextDummy);
    else
      listbox.appendChild(newNode);

    top.MAX_RECIPIENTS++;

    var input = newNode.getElementsByTagName(awInputElementName());
    if ( input && input.length == 1 )
    {
      input[0].setAttribute("value", "");
      input[0].setAttribute("id", "addressCol2#" + top.MAX_RECIPIENTS);
    
      //this copies the autocomplete sessions list from recipient#1 
      input[0].syncSessions(document.getElementById('addressCol2#1'));

  	  // also clone the showCommentColumn setting
  	  //
  	  input[0].showCommentColumn = 
	      document.getElementById("addressCol2#1").showCommentColumn;

      // We always clone the first row.  The problem is that the first row
      // could be focused.  When we clone that row, we end up with a cloned
      // XUL textbox that has a focused attribute set.  Therefore we think
      // we're focused and don't properly refocus.  The best solution to this
      // would be to clone a template row that didn't really have any presentation,
      // rather than using the real visible first row of the listbox.
      //
      // For now we'll just put in a hack that ensures the focused attribute
      // is never copied when the node is cloned.
      if (input[0].getAttribute('focused') != '')
        input[0].removeAttribute('focused');
    }
    var select = newNode.getElementsByTagName(awSelectElementName());
    if ( select && select.length == 1 )
    {
      select[0].selectedItem = select[0].childNodes[0].childNodes[awGetSelectItemIndex(lastRecipientType)];
      select[0].setAttribute("id", "addressCol1#" + top.MAX_RECIPIENTS);
      if (input)
        _awSetAutoComplete(select[0], input[0]);
    }

    // focus on new input widget
    if (setFocus && input[0] )
      awSetFocus(top.MAX_RECIPIENTS, input[0]);
  }
}

// functions for accessing the elements in the addressing widget

function awGetPopupElement(row)
{
    return document.getElementById("addressCol1#" + row);
}

function awGetInputElement(row)
{
    return document.getElementById("addressCol2#" + row);
}

function awGetElementByCol(row, col)
{
  var colID = "addressCol" + col + "#" + row;
  return document.getElementById(colID);
}

function awGetListItem(row)
{
  var listbox = document.getElementById('addressingWidget');

  if ( listbox && row > 0)
  {
    var listitems = listbox.getElementsByTagName('listitem');
    if ( listitems && listitems.length >= row )
      return listitems[row-1];
  }
  return 0;
}

function awGetRowByInputElement(inputElement)
{
  var row = 0;
  if (inputElement) {
    var listitem = inputElement.parentNode.parentNode;
    while (listitem) {
      if (listitem.localName == "listitem")
        ++row;
      listitem = listitem.previousSibling;
    }
  }
  return row;
}


// Copy Node - copy this node and insert ahead of the (before) node.  Append to end if before=0
function awCopyNode(node, parentNode, beforeNode)
{
  var newNode = node.cloneNode(true);

  if ( beforeNode )
    parentNode.insertBefore(newNode, beforeNode);
  else
    parentNode.appendChild(newNode);

    return newNode;
}

// remove row

function awRemoveRow(row)
{
  var listbox = document.getElementById('addressingWidget');

  awRemoveNodeAndChildren(listbox, awGetListItem(row));
  awFitDummyRows();

  top.MAX_RECIPIENTS --;
}

function awRemoveNodeAndChildren(parent, nodeToRemove)
{
  nodeToRemove.parentNode.removeChild(nodeToRemove);
}

function awSetFocus(row, inputElement)
{
  top.awRow = row;
  top.awInputElement = inputElement;
  top.awFocusRetry = 0;
  setTimeout("_awSetFocus();", 0);
}

function _awSetFocus()
{
  var listbox = document.getElementById('addressingWidget');
  //try
  //{
    var theNewRow = awGetListItem(top.awRow);
    //temporary patch for bug 26344
    awFinishCopyNode(theNewRow);

    //Warning: firstVisibleRow is zero base but top.awRow is one base!
    var firstVisibleRow = listbox.getIndexOfFirstVisibleRow();
    var numOfVisibleRows = listbox.getNumberOfVisibleRows();

    //Do we need to scroll in order to see the selected row?
    if (top.awRow <= firstVisibleRow)
      listbox.scrollToIndex(top.awRow - 1);
    else
      if (top.awRow - 1 >= (firstVisibleRow + numOfVisibleRows))
        listbox.scrollToIndex(top.awRow - numOfVisibleRows);

    top.awInputElement.focus();
    // stop supressing command updating and update the toolbar, since focus has changed
    SuppressComposeCommandUpdating(false);
  /*}
  catch(ex)
  {
    top.awFocusRetry ++;
    if (top.awFocusRetry < 3)
    {
      dump("_awSetFocus failed, try it again...\n");
      setTimeout("_awSetFocus();", 0);
    }
    else
      dump("_awSetFocus failed, forget about it!\n");
  }*/
}


//temporary patch for bug 26344 & 26528
function awFinishCopyNode(node)
{
    gMsgCompose.ResetNodeEventHandlers(node);
    return;
}


function awFinishCopyNodes()
{
  var listbox = document.getElementById('addressingWidget');
  awFinishCopyNode(listbox);
}

function awTabFromRecipient(element, event)
{
  //If we are le last element in the listbox, we don't want to create a new row.
  if (element == awGetInputElement(top.MAX_RECIPIENTS))
    top.doNotCreateANewRow = true;

  var row = awGetRowByInputElement(element);
  if (!event.shiftKey && row < top.MAX_RECIPIENTS) {
    var listBoxRow = row - 1; // listbox row indices are 0-based, ours are 1-based.
    var listBox = document.getElementById("addressingWidget");
    listBox.listBoxObject.ensureIndexIsVisible(listBoxRow + 1);
  }
}

function awTabFromMenulist(element, event)
{
  var row = awGetRowByInputElement(element);
  if (event.shiftKey && row > 1) {
    var listBoxRow = row - 1; // listbox row indices are 0-based, ours are 1-based.
    var listBox = document.getElementById("addressingWidget");
    listBox.listBoxObject.ensureIndexIsVisible(listBoxRow - 1);
  }
}

function awGetNumberOfRecipients()
{
    return top.MAX_RECIPIENTS;
}

function DragOverAddressingWidget(event)
{
  var validFlavor = false;
  var dragSession = dragSession = gDragService.getCurrentSession();

  if (dragSession.isDataFlavorSupported("text/x-moz-address")) 
    validFlavor = true;

  if (validFlavor)
    dragSession.canDrop = true;
}

function DropOnAddressingWidget(event)
{
  var dragSession = gDragService.getCurrentSession();
  
  var trans = Components.classes["@mozilla.org/widget/transferable;1"].createInstance(Components.interfaces.nsITransferable);
  trans.addDataFlavor("text/x-moz-address");

  for ( var i = 0; i < dragSession.numDropItems; ++i )
  {
    dragSession.getData ( trans, i );
    var dataObj = new Object();
    var bestFlavor = new Object();
    var len = new Object();
    trans.getAnyTransferData ( bestFlavor, dataObj, len );
    if ( dataObj )  
      dataObj = dataObj.value.QueryInterface(Components.interfaces.nsISupportsWString);
    if ( !dataObj ) 
      continue;

    // pull the address out of the data object
    var address = dataObj.data.substring(0, len.value);
    if (!address)
      continue;

    DropRecipient(event.target, address);
  }
}

function DropRecipient(target, recipient)
{
  // that will automatically set the focus on a new available row, and make sure is visible
  awClickEmptySpace(null, true);
  var lastInput = awGetInputElement(top.MAX_RECIPIENTS);
  lastInput.value = recipient;
  awAppendNewRow(true);
}

function _awSetAutoComplete(selectElem, inputElem)
{
  if (selectElem.value != 'addr_newsgroups' && selectElem.value != 'addr_followup')
    inputElem.disableAutocomplete = false;
  else
    inputElem.disableAutocomplete = true;
}

function awSetAutoComplete(rowNumber)
{
    var inputElem = awGetInputElement(rowNumber);
    var selectElem = awGetPopupElement(rowNumber);
    _awSetAutoComplete(selectElem, inputElem)
}

function awRecipientTextCommand(userAction, element)
{
  if (userAction == "typing" || userAction == "scrolling")
    awReturnHit(element);
}

// Called when an autocomplete session item is selected and the status of
// the session it was selected from is nsIAutoCompleteStatus::failureItems.
//
// As of this writing, the only way that can happen is when an LDAP 
// autocomplete session returns an error to be displayed to the user.
//
// There are hardcoded messages in here, but these are just fallbacks for
// when string bundles have already failed us.
//
function awRecipientErrorCommand(errItem, element)
{
    // remove the angle brackets from the general error message to construct 
    // the title for the alert.  someday we'll pass this info using a real
    // exception object, and then this code can go away.
    //
    var generalErrString;
    if (errItem.value != "") {
	generalErrString = errItem.value.slice(1, errItem.value.length-1);
    } else {
	generalErrString = "Unknown LDAP server problem encountered";
    }	

    // try and get the string of the specific error to contruct the complete
    // err msg, otherwise fall back to something generic.  This message is
    // handed to us as an nsISupportsWString in the param slot of the 
    // autocomplete error item, by agreement documented in 
    // nsILDAPAutoCompFormatter.idl
    //
    var specificErrString = "";
    try {
	var specificError = errItem.param.QueryInterface(
	    Components.interfaces.nsISupportsWString);
	specificErrString = specificError.data;
    } catch (ex) {
    }
    if (specificErrString == "") {
	specificErrString = "Internal error";
    }

    if (gPromptService) {
	gPromptService.alert(window, generalErrString, specificErrString);
    } else {
	window.alert(generalErrString + ": " + specificErrString);
    }
}

function awRecipientKeyPress(event, element)
{
  switch(event.keyCode) {
  case 9:
    awTabFromRecipient(element, event);
    break;
  }
}

function awRecipientKeyDown(event, element)
{
  switch(event.keyCode) {
  case 46:
  case 8:
    /* do not query directly the value of the text field else the autocomplete widget could potentially
       alter it value while doing some internal cleanup, instead, query the value through the first child
    */
    if (!element.value)
      awDeleteHit(element);
    event.preventBubble();  //We need to stop the event else the listbox will receive it and the function
                            //awKeyDown will be executed!
    break;
  }
}

function awKeyDown(event, listboxElement)
{
  switch(event.keyCode) {
  case 46:
  case 8:
    /* Warning, the listboxElement.selectedItems will change everytime we delete a row */
    var selItems = listboxElement.selectedItems;
    var length = listboxElement.selectedItems.length;
    for (var i = 1; i <= length; i++) {
      var inputs = listboxElement.selectedItems[0].getElementsByTagName(awInputElementName());
      if (inputs && inputs.length == 1)
        awDeleteHit(inputs[0]);
    }
    break;
  }
}

function awMenulistKeyPress(event, element)
{
  switch(event.keyCode) {
  case 9:
    awTabFromMenulist(element, event);
    break;
  }
}

/* ::::::::::: addressing widget dummy rows ::::::::::::::::: */

var gAWContentHeight = 0;
var gAWRowHeight = 0;

function awFitDummyRows()
{
  awCalcContentHeight();
  awCreateOrRemoveDummyRows();
}

function awCreateOrRemoveDummyRows()
{
  var listbox = document.getElementById("addressingWidget");
  var listboxHeight = listbox.boxObject.height;

  // remove rows to remove scrollbar
  var kids = listbox.childNodes;
  for (var i = kids.length-1; gAWContentHeight > listboxHeight && i >= 0; --i) {
    if (kids[i].hasAttribute("_isDummyRow")) {
      gAWContentHeight -= gAWRowHeight;
      listbox.removeChild(kids[i]);
    }
  }

  // add rows to fill space
  if (gAWRowHeight) {
    while (gAWContentHeight+gAWRowHeight < listboxHeight) {
      awCreateDummyItem(listbox);
      gAWContentHeight += gAWRowHeight;
    }
  }
}

function awCalcContentHeight()
{
  var listbox = document.getElementById("addressingWidget");
  var items = listbox.getElementsByTagName("listitem");

  gAWContentHeight = 0;
  if (items.length > 0) {
    // all rows are forced to a uniform height in xul listboxes, so
    // find the first listitem with a boxObject and use it as precedent
    var i = 0;
    do {
      gAWRowHeight = items[i].boxObject.height;
      ++i;
    } while (i < items.length && !gAWRowHeight);
    gAWContentHeight = gAWRowHeight*items.length;
  }
}

function awCreateDummyItem(aParent)
{
  var titem = document.createElement("listitem");
  titem.setAttribute("_isDummyRow", "true");
  titem.setAttribute("class", "dummy-row");

  for (var i = awGetNumberOfCols(); i > 0; i--)
  awCreateDummyCell(titem);

  if (aParent)
    aParent.appendChild(titem);

  return titem;
}

function awCreateDummyCell(aParent)
{
  var cell = document.createElement("listcell");
  cell.setAttribute("class", "addressingWidgetCell dummy-row-cell");
  if (aParent)
    aParent.appendChild(cell);

  return cell;
}

function awGetNextDummyRow()
{
  // gets the next row from the top down
  var listbox = document.getElementById("addressingWidget");
  var kids = listbox.childNodes;
  for (var i = 0; i < kids.length; ++i) {
    if (kids[i].hasAttribute("_isDummyRow"))
      return kids[i];
  }
  return null;
}

function awSizerListen()
{
  // when splitter is clicked, fill in necessary dummy rows each time the mouse is moved
  awCalcContentHeight(); // precalculate
  document.addEventListener("mousemove", awSizerMouseMove, true);
  document.addEventListener("mouseup", awSizerMouseUp, false);
}

function awSizerMouseMove()
{
  awCreateOrRemoveDummyRows(2);
}

function awSizerMouseUp()
{
  document.removeEventListener("mousemove", awSizerMouseUp, false);
  document.removeEventListener("mouseup", awSizerMouseUp, false);
}

function awDocumentKeyPress(event)
{
  try {
    var id = event.target.getAttribute('id');
    if (id.substr(0, 11) == 'addressCol1')
      awMenulistKeyPress(event, event.target);
  } catch (e) { }
}
