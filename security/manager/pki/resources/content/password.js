/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *  Bob Lord <lord@netscape.com>
 *  Terry Hayes <thayes@netscape.com>
 */
const nsPK11TokenDB = "@mozilla.org/security/pk11tokendb;1";
const nsIPK11TokenDB = Components.interfaces.nsIPK11TokenDB;
const nsIDialogParamBlock = Components.interfaces.nsIDialogParamBlock;
const nsPKCS11ModuleDB = "@mozilla.org/security/pkcs11moduledb;1";
const nsIPKCS11ModuleDB = Components.interfaces.nsIPKCS11ModuleDB;
const nsIPKCS11Slot = Components.interfaces.nsIPKCS11Slot;
const nsIPK11Token = Components.interfaces.nsIPK11Token;


var params;
var tokenName="";
var pw1;

function onLoad()
{

  pw1 = document.getElementById("pw1");
  try {
     params = window.arguments[0].QueryInterface(nsIDialogParamBlock);
     tokenName = params.GetString(1);
  }catch(exception)
	 {tokenName = self.name;}

  if(tokenName=="" || tokenName=="_blank") {
     var sectokdb = Components.classes[nsPK11TokenDB].getService(nsIPK11TokenDB);
     var tokenList = sectokdb.listTokens();
     var enumElement;
     var i=0;
     var menu = document.getElementById("tokenMenu");
     try {
        for ( ; !tokenList.isDone(); tokenList.next()) {
           enumElement = tokenList.currentItem();
           var token = enumElement.QueryInterface(nsIPK11Token);
           if(token.needsLogin() || !(token.needsUserInit)) {
              var menuItemNode = document.createElement("menuitem");
              menuItemNode.setAttribute("value", token.tokenName);
              menuItemNode.setAttribute("label", token.tokenName);
              menu.firstChild.appendChild(menuItemNode);
              if (i == 0) {
                 menu.selectedItem = menuItemNode;
                 tokenName = token.tokenName;
              }
              i++;
           }
        }
     }catch(exception){}
  } else {
    var sel = document.getElementById("tokenMenu");
    sel.setAttribute("hidden", "true");
    var tag = document.getElementById("tokenName");
    tag.setAttribute("value",tokenName);
  }
	 	 
  process();
}

function onMenuChange()
{
   //get the selected token
   var list = document.getElementById("tokenMenu");
   tokenName = list.value;

   process();
}


function process()
{
   var secmoddb = Components.classes[nsPKCS11ModuleDB].getService(nsIPKCS11ModuleDB);
   var bundle = srGetStrBundle("chrome://pippki/locale/pippki.properties");

   // If the token is unitialized, don't use the old password box.
   // Otherwise, do.

   var slot = secmoddb.findSlotByName(tokenName);
   if (slot) {
     var oldpwbox = document.getElementById("oldpw");
     var msgBox = document.getElementById("message");
     var status = slot.status;
     if (status == nsIPKCS11Slot.SLOT_UNINITIALIZED
         || status == nsIPKCS11Slot.SLOT_READY) {
      
       oldpwbox.setAttribute("hidden", "true");
       msgBox.setAttribute("value", bundle.GetStringFromName("password_not_set")); 
       msgBox.setAttribute("hidden", "false");

       if (status == nsIPKCS11Slot.SLOT_READY) {
         oldpwbox.setAttribute("inited", "empty");
       } else {
         oldpwbox.setAttribute("inited", "true");
       }
      
       // Select first password field
       document.getElementById('pw1').focus();
    
     } else {
       // Select old password field
       oldpwbox.setAttribute("hidden", "false");
       msgBox.setAttribute("hidden", "true");
       oldpwbox.setAttribute("inited", "false");
       oldpwbox.focus();
     }
   }

  if (params) {
    // Return value 0 means "canceled"
    params.SetInt(1, 0);
  }
  
  checkPasswords();
}

function onP12Load()
{
  pw1 = document.getElementById("pw1");
  params = window.arguments[0].QueryInterface(nsIDialogParamBlock);
  // Select first password field
  document.getElementById('pw1').focus();
}

function setPassword()
{
  var pk11db = Components.classes[nsPK11TokenDB].getService(nsIPK11TokenDB);
  var token = pk11db.findTokenByName(tokenName);

  var oldpwbox = document.getElementById("oldpw");
  var initpw = oldpwbox.getAttribute("inited");
  var bundle = srGetStrBundle("chrome://pippki/locale/pippki.properties");
  
  var success = false;
  
  if (initpw == "false" || initpw == "empty") {
    try {
      var oldpw = "";
      var passok = 0;
      
      if (initpw == "empty") {
        passok = 1;
      } else {
        oldpw = oldpwbox.value;
        passok = token.checkPassword(oldpw);
      }
      
      if (passok) {
        if (initpw == "empty" && pw1.value == "") {
          // This makes no sense that we arrive here, 
          // we reached a case that should have been prevented by checkPasswords.
        } else {
          token.changePassword(oldpw, pw1.value);
          if (pw1.value == "") {
            alert(bundle.GetStringFromName("pw_erased_ok")
                  + " "
                  + bundle.GetStringFromName("pw_empty_warning"));
          } else {
            alert(bundle.GetStringFromName("pw_change_ok")); 
          }
        }
        success = true;
      } else {
        oldpwbox.focus();
        oldpwbox.setAttribute("value", "");
        alert(bundle.GetStringFromName("incorrect_pw")); 
      }
    } catch (e) {
      alert(bundle.GetStringFromName("failed_pw_change")); 
    }
  } else {
    token.initPassword(pw1.value);
    if (pw1.value == "") {
      alert(bundle.GetStringFromName("pw_not_wanted")
            + " " 
            + bundle.GetStringFromName("pw_empty_warning"));
    }
    success = true;
  }

  // Terminate dialog
  if (success) {
    if (params) {
      // Return value 1 means "successfully executed ok"
      params.SetInt(1, 1);
    }

    window.close();
  }
}

function getPassword()
{
  // grab what was entered
  params.SetString(2, pw1.value);
  // Return value
  params.SetInt(1, 1);
  // Terminate dialog
  window.close();
}

function setP12Password()
{
  // grab what was entered
  params.SetString(2, pw1.value);
  // Return value
  params.SetInt(1, 1);
  // Terminate dialog
  window.close();
}

function setPasswordStrength()
{
// Here is how we weigh the quality of the password
// number of characters
// numbers
// non-alpha-numeric chars
// upper and lower case characters

  var pw=document.getElementById('pw1').value;
//  alert("password='" + pw +"'");

//length of the password
  var pwlength=(pw.length);
  if (pwlength>5)
    pwlength=5;


//use of numbers in the password
  var numnumeric = pw.replace (/[0-9]/g, "");
  var numeric=(pw.length - numnumeric.length);
  if (numeric>3)
    numeric=3;

//use of symbols in the password
  var symbols = pw.replace (/\W/g, "");
  var numsymbols=(pw.length - symbols.length);
  if (numsymbols>3)
    numsymbols=3;

//use of uppercase in the password
  var numupper = pw.replace (/[A-Z]/g, "");
  var upper=(pw.length - numupper.length);
  if (upper>3)
    upper=3;


  var pwstrength=((pwlength*10)-20) + (numeric*10) + (numsymbols*15) + (upper*10);

  // make sure we're give a value between 0 and 100
  if ( pwstrength < 0 ) {
    pwstrength = 0;
  }
  
  if ( pwstrength > 100 ) {
    pwstrength = 100;
  }

  var mymeter=document.getElementById('pwmeter');
  mymeter.setAttribute("value",pwstrength);

  return;
}

function checkPasswords()
{
  var pw1=document.getElementById('pw1').value;
  var pw2=document.getElementById('pw2').value;

  var ok=document.getElementById('ok-button');

  var oldpwbox = document.getElementById("oldpw");
  if (oldpwbox) {
    var initpw = oldpwbox.getAttribute("inited");

    if (initpw == "empty" && pw1 == "") {
      // The token has already been initialized, therefore this dialog
      // was called with the intention to change the password.
      // The token currently uses an empty password.
      // We will not allow changing the password from empty to empty.
      ok.setAttribute("disabled","true");
      return;
    }
  }

  if (pw1 == pw2){
    ok.setAttribute("disabled","false");
  } else
  {
    ok.setAttribute("disabled","true");
  }

}
