/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

var gListBox;
var gViewButton;
var gBundle;

var gEmailAddresses;
var gCertStatusSummaries;
var gCertIssuedInfos;
var gCertExpiresInfos;
var gCerts;
var gCount;

var gSMimeContractID = "@mozilla.org/messenger-smime/smimejshelper;1";
var gISMimeJSHelper = Components.interfaces.nsISMimeJSHelper;
var gIX509Cert = Components.interfaces.nsIX509Cert;

function getStatusExplanation(value)
{
  switch (value)
  {
    case gIX509Cert.VERIFIED_OK:
      return gBundle.getString("StatusValid");

    case gIX509Cert.NOT_VERIFIED_UNKNOWN:
    case gIX509Cert.INVALID_CA:
    case gIX509Cert.USAGE_NOT_ALLOWED:
      return gBundle.getString("StatusInvalid");

    case gIX509Cert.CERT_REVOKED:
      return gBundle.getString("StatusRevoked");

    case gIX509Cert.CERT_EXPIRED:
      return gBundle.getString("StatusExpired");

    case gIX509Cert.CERT_NOT_TRUSTED:
    case gIX509Cert.ISSUER_NOT_TRUSTED:
    case gIX509Cert.ISSUER_UNKNOWN:
      return gBundle.getString("StatusUntrusted");
  }
}

function onLoad()
{
  document.getElementById("cancel").setAttribute("collapsed", "true");

  var params = window.arguments[0];
  if (!params)
    return;

  var helper = Components.classes[gSMimeContractID].createInstance(gISMimeJSHelper);

  if (!helper)
    return;

  gListBox = document.getElementById("infolist");
  gViewButton = document.getElementById("viewCertButton");
  gBundle = document.getElementById("bundle_smime_comp_info");

  gEmailAddresses = new Object();
  gCertStatusSummaries = new Object();
  gCertIssuedInfos = new Object();
  gCertExpiresInfos = new Object();
  gCerts = new Object();
  gCount = new Object();
  var canEncrypt = new Object();

  try
  {
    helper.getRecipientCertsInfo(
      params.compFields,
      gCount,
      gEmailAddresses,
      gCertStatusSummaries,
      gCertIssuedInfos,
      gCertExpiresInfos,
      gCerts,
      canEncrypt);
  }
  catch (e)
  {
    dump(e);
    return;
  }

  if (gBundle)
  {
    var yes_string = gBundle.getString("StatusYes");
    var no_string = gBundle.getString("StatusNo");
    var not_possible_string = gBundle.getString("StatusNotPossible");

    var signed_element = document.getElementById("signed");
    var encrypted_element = document.getElementById("encrypted");
    
    if (params.smFields.requireEncryptMessage)
    {
      if (params.certsAvailable && canEncrypt.value)
      {
        encrypted_element.value = yes_string;
      }
      else
      {
        encrypted_element.value = not_possible_string;
      }
    }
    else
    {
      encrypted_element.value = no_string;
    }
    
    if (params.smFields.signMessage)
    {
      if (params.certsAvailable)
      {
        signed_element.value = yes_string;
      }
      else
      {
        signed_element.value = not_possible_string;
      }
    }
    else
    {
      signed_element.value = no_string;
    }
  }

  var imax = gCount.value;
  
  for (var i = 0; i < imax; ++i)
  {
    var listitem  = document.createElement("listitem");

    listitem.appendChild(createCell(gEmailAddresses.value[i]));
    
    if (!gCerts.value[i])
    {
      listitem.appendChild(createCell(gBundle.getString("StatusNotFound")));
    }
    else
    {
      listitem.appendChild(createCell(getStatusExplanation(gCertStatusSummaries.value[i])));
      listitem.appendChild(createCell(gCertIssuedInfos.value[i]));
      listitem.appendChild(createCell(gCertExpiresInfos.value[i]));
    }

    gListBox.appendChild(listitem);
  }
}

function onSelectionChange(event)
{
  if (gListBox.selectedItems.length <= 0)
  {
    gViewButton.setAttribute("disabled", "true");
  }
  else
  {
    gViewButton.removeAttribute("disabled");
  }
}

function viewSelectedCert()
{
  if (gListBox.selectedItems.length > 0)
  {
    var selected = gListBox.selectedIndex;
    var cert = gCerts.value[selected];
    if (cert)
    {
      cert.view();
    }
  }
}

function doHelpButton()
{
  openHelp('compose_security');
}

function createCell(label)
{
  var cell = document.createElement("listcell");
  cell.setAttribute("label", label)
  return cell;
}

