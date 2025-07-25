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
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-2001 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributors:
 */

var useCustomPrefs;
var requestReceipt;
var leaveInInbox;
var moveToSent;
var receiptSend;
var neverReturn;
var returnSome;
var notInToCcPref;
var notInToCcLabel;
var outsideDomainPref;
var outsideDomainLabel;
var otherCasesPref;
var otherCasesLabel;
var receiptArriveLabel;
var receiptRequestLabel;

function onInit() 
{
  useCustomPrefs = document.getElementById("identity.use_custom_prefs");
  requestReceipt = document.getElementById("identity.request_return_receipt_on");
  leaveInInbox = document.getElementById("leave_in_inbox");
  moveToSent = document.getElementById("move_to_sent");
  receiptSend = document.getElementById("server.mdn_report_enabled");
  neverReturn = document.getElementById("never_return");
  returnSome = document.getElementById("return_some");
  notInToCcPref = document.getElementById("server.mdn_not_in_to_cc");
  notInToCcLabel = document.getElementById("notInToCcLabel");
  outsideDomainPref = document.getElementById("server.mdn_outside_domain");
  outsideDomainLabel = document.getElementById("outsideDomainLabel");
  otherCasesPref = document.getElementById("server.mdn_other");
  otherCasesLabel = document.getElementById("otherCasesLabel");
  receiptArriveLabel = document.getElementById("receiptArriveLabel");
  receiptRequestLabel = document.getElementById("receiptRequestLabel");
  
  EnableDisableCustomSettings();
        
  return true;
}

function onSave()
{

}

function EnableDisableCustomSettings() {
  if (useCustomPrefs && (useCustomPrefs.getAttribute("value") == "false")) {
    requestReceipt.setAttribute("disabled", "true");
    leaveInInbox.setAttribute("disabled", "true");
    moveToSent.setAttribute("disabled", "true");
    neverReturn.setAttribute("disabled", "true");
    returnSome.setAttribute("disabled", "true");
    receiptArriveLabel.setAttribute("disabled", "true");
    receiptRequestLabel.setAttribute("disabled", "true");
  }
  else {
    requestReceipt.removeAttribute("disabled");
    leaveInInbox.removeAttribute("disabled");
    moveToSent.removeAttribute("disabled");
    neverReturn.removeAttribute("disabled");
    returnSome.removeAttribute("disabled");
    receiptArriveLabel.removeAttribute("disabled");
    receiptRequestLabel.removeAttribute("disabled");
  }
  EnableDisableAllowedReceipts();
  return true;
}

function EnableDisableAllowedReceipts() {
  if (receiptSend) {
    if (!neverReturn.getAttribute("disabled") && (receiptSend.getAttribute("value") != "false")) {
      notInToCcPref.removeAttribute("disabled");
      notInToCcLabel.removeAttribute("disabled");
      outsideDomainPref.removeAttribute("disabled");
      outsideDomainLabel.removeAttribute("disabled");
      otherCasesPref.removeAttribute("disabled");
      otherCasesLabel.removeAttribute("disabled");
    }
    else {
      notInToCcPref.setAttribute("disabled", "true");
      notInToCcLabel.setAttribute("disabled", "true");
      outsideDomainPref.setAttribute("disabled", "true");
      outsideDomainLabel.setAttribute("disabled", "true");
      otherCasesPref.setAttribute("disabled", "true");
      otherCasesLabel.setAttribute("disabled", "true");
    }
  }
  return true;
}
