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
 * The Original Code is Mozilla Progress Dialog.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bill Law <law@netscape.com>
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

/* This file implements the nsIProgressDialog interface.  See nsIProgressDialog.idl
 *
 * The implementation consists of a JavaScript "class" named nsProgressDialog,
 * comprised of:
 *   - a JS constructor function
 *   - a prototype providing all the interface methods and implementation stuff
 *
 * In addition, this file implements an nsIModule object that registers the
 * nsProgressDialog component.
 */

/* ctor
 */
function nsProgressDialog() {
    // Initialize data properties.
    this.mParent      = null;
    this.mOperation   = null;
    this.mStartTime   = ( new Date() ).getTime();
    this.observer     = null;
    this.mLastUpdate  = Number.MIN_VALUE; // To ensure first onProgress causes update.
    this.mInterval    = 750; // Default to .75 seconds.
    this.mElapsed     = 0;
    this.mLoaded      = false;
    this.fields       = new Array;
    this.strings      = new Array;
    this.mSource      = null;
    this.mTarget      = null;
    this.mApp         = null;
    this.mDialog      = null;
    this.mDisplayName = null;
    this.mPaused      = null;
    this.mRequest     = null;
    this.mCompleted   = false;
    this.mMode        = "normal";
    this.mPercent     = 0;
    this.mRate        = 0;
    this.mBundle      = null;
}

const nsIProgressDialog = Components.interfaces.nsIProgressDialog;

nsProgressDialog.prototype = {
    // Turn this on to get debugging messages.
    debug: false,

    // Chrome-related constants.
    dialogChrome:   "chrome://global/content/nsProgressDialog.xul",
    dialogFeatures: "chrome,titlebar,minimizable=yes",

    // getters/setters
    get saving()            { return this.openingWith == null || this.openingWith == ""; },
    get parent()            { return this.mParent; },
    set parent(newval)      { return this.mParent = newval; },
    get operation()         { return this.mOperation; },
    set operation(newval)   { return this.mOperation = newval; },
    get observer()          { return this.mObserver; },
    set observer(newval)    { return this.mObserver = newval; },
    get startTime()         { return this.mStartTime; },
    set startTime(newval)   { return this.mStartTime = newval/1000; }, // PR_Now() is in microseconds, so we convert.
    get lastUpdate()        { return this.mLastUpdate; },
    set lastUpdate(newval)  { return this.mLastUpdate = newval; },
    get interval()          { return this.mInterval; },
    set interval(newval)    { return this.mInterval = newval; },
    get elapsed()           { return this.mElapsed; },
    set elapsed(newval)     { return this.mElapsed = newval; },
    get loaded()            { return this.mLoaded; },
    set loaded(newval)      { return this.mLoaded = newval; },
    get source()            { return this.mSource; },
    set source(newval)      { return this.mSource = newval; },
    get target()            { return this.mTarget; },
    set target(newval)      { return this.mTarget = newval; },
    get openingWith()       { return this.mApp; },
    set openingWith(newval) { return this.mApp = newval; },
    get dialog()            { return this.mDialog; },
    set dialog(newval)      { return this.mDialog = newval; },
    get displayName()       { return this.mDisplayName; },
    set displayName(newval) { return this.mDisplayName = newval; },
    get paused()            { return this.mPaused; },
    get request()           { return this.mRequest; },
    get completed()         { return this.mCompleted; },
    get mode()              { return this.mMode; },
    get percent()           { return this.mPercent; },
    get rate()              { return this.mRate; },
    get kRate()             { return this.mRate / 1024; },

    // These setters use functions that update the dialog.
    set paused(newval)      { return this.setPaused(newval); },
    set request(newval)     { return this.setRequest(newval); },
    set completed(newval)   { return this.setCompleted(newval); },
    set mode(newval)        { return this.setMode(newval); },
    set percent(newval)     { return this.setPercent(newval); },
    set rate(newval)        { return this.setRate(newval); },

    // ---------- nsIProgressDialog methods ----------

    // open: Store aParentWindow and open the dialog.
    open: function( aParentWindow ) {
        // Save parent and "persist" operation.
        this.parent    = aParentWindow;

        // Open dialog using the WindowWatcher service.
        var ww = Components.classes["@mozilla.org/embedcomp/window-watcher;1"]
                   .getService( Components.interfaces.nsIWindowWatcher );
        this.dialog = ww.openWindow( this.parent,
                                     this.dialogChrome,
                                     null,
                                     this.dialogFeatures,
                                     this );
    },
    
    init: function( aSource, aTarget, aDisplayName, aOpeningWith, aStartTime, aOperation ) {
      this.source = aSource;
      this.target = aTarget;
      this.displayName = aDisplayName;
      this.openingWith = aOpeningWith;
      if ( aStartTime ) {
          this.startTime = aStartTime;
      }
      this.operation = aOperation;
    },

    // ---------- nsIWebProgressListener methods ----------

    // Look for STATE_STOP and update dialog to indicate completion when it happens.
    onStateChange: function( aWebProgress, aRequest, aStateFlags, aStatus ) {
        if ( aStateFlags & Components.interfaces.nsIWebProgressListener.STATE_STOP ) {
            // we are done downloading...
            this.completed = true;
        }
    },

    // Handle progress notifications.
    onProgressChange: function( aWebProgress,
                                aRequest,
                                aCurSelfProgress,
                                aMaxSelfProgress,
                                aCurTotalProgress,
                                aMaxTotalProgress ) {
        // Remember the request; this will also initialize the pause/resume stuff.
        this.request = aRequest;

        var overallProgress = aCurTotalProgress;

        // Get current time.
        var now = ( new Date() ).getTime();

        // If interval hasn't elapsed, ignore it.
        if ( now - this.lastUpdate < this.interval &&
             aMaxTotalProgress != "-1" && 
             parseInt( aCurTotalProgress ) < parseInt( aMaxTotalProgress ) ) {
            return;
        }

        // Update this time.
        this.lastUpdate = now;

        // Update elapsed time.
        this.elapsed = now - this.startTime;

        // Calculate percentage.
        if ( aMaxTotalProgress > 0) {
            this.percent = Math.floor( ( overallProgress * 100.0 ) / aMaxTotalProgress );
        } else {
            this.percent = -1;
        }

        // If dialog not loaded, then don't bother trying to update display.
        if ( !this.loaded ) {
            return;
        }

        // Update dialog's display of elapsed time.
        this.setValue( "timeElapsed", this.formatSeconds( this.elapsed / 1000 ) );

        // Now that we've set the progress and the time, update # bytes downloaded...
        // Update status (nnK of mmK bytes at xx.xK aCurTotalProgress/sec)
        var status = this.getString( "progressMsg" );

        // Insert 1 is the number of kilobytes downloaded so far.
        status = this.replaceInsert( status, 1, parseInt( overallProgress/1024 + .5 ) );

        // Insert 2 is the total number of kilobytes to be downloaded (if known).
        if ( aMaxTotalProgress != "-1" ) {
            status = this.replaceInsert( status, 2, parseInt( aMaxTotalProgress/1024 + .5 ) );
        } else {
            status = replaceInsert( status, 2, "??" );
        }
    
        // Insert 3 is the download rate.
        if ( this.elapsed ) {
            this.rate = ( aCurTotalProgress * 1000 ) / this.elapsed;
            status = this.replaceInsert( status, 3, this.rateToKRate( this.rate ) );
        } else {
            // Rate not established, yet.
            status = this.replaceInsert( status, 3, "??.?" );
        }
    
        // All 3 inserts are taken care of, now update status msg.
        this.setValue( "status", status );
    
        // Update time remaining.
        if ( this.rate && ( aMaxTotalProgress > 0 ) ) {
            // Calculate how much time to download remaining at this rate.
            var rem = Math.round( ( aMaxTotalProgress - aCurTotalProgress ) / this.rate );
            this.setValue( "timeLeft", this.formatSeconds( rem ) );
        } else {
            // We don't know how much time remains.
            this.setValue( "timeLeft", this.getString( "unknownTime" ) );
        }
    },

    // Look for error notifications and display alert to user.
    onStatusChange: function( aWebProgress, aRequest, aStatus, aMessage ) {
        // Check for error condition (only if dialog is still open).
        if ( aStatus != Components.results.NS_OK ) {
            if ( this.loaded ) {
                // Get prompt service.
                var prompter = Components.classes[ "@mozilla.org/embedcomp/prompt-service;1" ]
                                   .getService( Components.interfaces.nsIPromptService );
                // Display error alert (using text supplied by back-end).
                var title = this.getProperty( this.saving ? "savingAlertTitle" : "openingAlertTitle",
                                              [ this.fileName() ], 
                                              1 );
                prompter.alert( this.dialog, title, aMessage );
    
                // Close the dialog.
                if ( !this.completed ) {
                    this.onCancel();
                }
            } else {
                // Error occurred prior to onload even firing.
                // We can't handle this error until we're done loading, so
                // defer the handling of this call.
                this.dialog.setTimeout( function(obj,wp,req,stat,msg){obj.onStatusChange(wp,req,stat,msg)},
                                        100, this, aWebProgress, aRequest, aStatus, aMessage );
            }
        }
    },

    // Ignore onLocationChange and onSecurityChange notifications.
    onLocationChange: function( aWebProgress, aRequest, aLocation ) {
    },

    onSecurityChange: function( aWebProgress, aRequest, state ) {
    },

    // ---------- nsIObserver methods ----------
    observe: function( anObject, aTopic, aData ) {
        // Something of interest occured on the dialog.
        // Dispatch to corresponding implementation method.
        switch ( aTopic ) {
        case "onload":
            this.onLoad();
            break;
        case "oncancel":
            this.onCancel();
            break;
        case "onpause":
            this.onPause();
            break;
        case "onlaunch":
            this.onLaunch();
            break;
        case "onreveal":
            this.onReveal();
            break;
        case "onunload":
            this.onUnload();
            break;
        case "oncompleted":
            // This event comes in when setCompleted needs to be deferred because
            // the dialog isn't loaded yet.
            this.completed = true;
            break;
        default:
            break;
        }
    },

    // ---------- nsISupports methods ----------

    // This "class" supports nsIProgressDialog, nsIWebProgressListener (by virtue
    // of interface inheritance), nsIObserver, and nsISupports.
    QueryInterface: function (iid) {
        if (!iid.equals(Components.interfaces.nsIProgressDialog) &&
            !iid.equals(Components.interfaces.nsIDownload) && 
            !iid.equals(Components.interfaces.nsIWebProgressListener) &&
            !iid.equals(Components.interfaces.nsIObserver) &&
            !iid.equals(Components.interfaces.nsISupports)) {
            throw Components.results.NS_ERROR_NO_INTERFACE;
        }
        return this;
    },

    // ---------- implementation methods ----------

    // Initialize the dialog.
    onLoad: function() {
        // Note that onLoad has finished.
        this.loaded = true;

        // Fill dialog.
        this.loadDialog();

        // Position dialog.
        if ( this.dialog.opener ) {
            this.dialog.moveToAlertPosition();
        } else {
            this.dialog.centerWindowOnScreen();
        }

        // Set initial focus on "keep open" box.  If that box is hidden, or, if
        // the download is already complete, then focus is on the cancel/close
        // button.  The download may be complete if it was really short and the
        // dialog took longer to open than to download the data.
        if ( !this.completed && !this.saving ) {
            this.dialogElement( "keep" ).focus();
        } else {
            this.dialogElement( "cancel" ).focus();
        }
    },

    // load dialog with initial contents
    loadDialog: function() {
        // Check whether we're saving versus opening with a helper app.
        if ( !this.saving ) {
            // Put proper label on source field.
            this.setValue( "sourceLabel", this.getString( "openingSource" ) );

            // Target is the "opening with" application.  Hide if empty.
            if ( this.openingWith.length == 0 ) {
                this.hide( "targetRow" );
            } else {
                // Use the "with:" label.
                this.setValue( "targetLabel", this.getString( "openingTarget" ) );
                // Name of application.
                this.setValue( "target", this.openingWith );
            }
        } else {
            // Target is the destination file.
            this.setValue( "target", this.target.path );
        }

        // Set source field.
        this.setValue( "source", this.source.spec );

        var now = ( new Date() ).getTime();

        // Intialize the elapsed time.
        if ( !this.elapsed ) {
            this.elapsed = now - this.startTime;
        }

        // Update elapsed time display.
        this.setValue( "timeElapsed", this.formatSeconds( this.elapsed / 1000 ) );
        this.setValue( "timeLeft", this.getString( "unknownTime" ) );

        // Initialize the "keep open" box.  Hide this if we're opening a helper app.
        if ( !this.saving ) {
            // Hide this in this case.
            this.hide( "keep" );
        } else {
            // Initialize using last-set value from prefs.
            var prefs = Components.classes[ "@mozilla.org/preferences-service;1" ]
                            .getService( Components.interfaces.nsIPrefBranch );
            if ( prefs ) {
                this.dialogElement( "keep" ).checked = prefs.getBoolPref( "browser.download.progressDnldDialog.keepAlive" );
            }
        }

        // Initialize title.
        this.setTitle();
    },

    // Cancel button stops the download (if not completed),
    // and closes the dialog.
    onCancel: function() {
         // Cancel the download, if not completed.
         if ( !this.completed ) {
             if ( this.operation ) {
                 this.operation.cancelSave();
                 // XXX We're supposed to clean up files/directories.
             }
             if ( this.observer ) {
                 this.observer.observe( this, "oncancel", "" );
             }
             this.paused = false;
         }
        // Test whether the dialog is already closed.
        // This will be the case if we've come through onUnload.
        if ( this.dialog ) {
            // Close the dialog.
            this.dialog.close();
        }
    },

    // onunload event means the dialog has closed.
    // We go through our onCancel logic to stop the download if still in progress.
    onUnload: function() {
        // Remember "keep dialog open" setting, if visible.
        if ( this.saving ) {
            var prefs = Components.classes["@mozilla.org/preferences-service;1"]
                          .getService( Components.interfaces.nsIPrefBranch );
            if ( prefs ) {
                prefs.setBoolPref( "browser.download.progressDnldDialog.keepAlive", this.dialogElement( "keep" ).checked );
            }
         }
         this.dialog = null; // The dialog is history.
         this.onCancel();
    },

    // onpause event means the user pressed the pause/resume button
    // Toggle the pause/resume state (see the function setPause(), below).i
    onPause: function() {
         this.paused = !this.paused;
    },

    // onlaunch event means the user pressed the launch button
    // Invoke the launch method of the target file.
    onLaunch: function() {
         try {
             this.target.launch();
             this.dialog.close();
         } catch ( exception ) {
             // XXX Need code here to tell user the launch failed!
             dump( "nsProgressDialog::onLaunch failed: " + exception + "\n" );
         }
    },

    // onreveal event means the user pressed the "reveal location" button
    // Invoke the reveal method of the target file.
    onReveal: function() {
         try {
             this.target.reveal();
             this.dialog.close();
         } catch ( exception ) {
         }
    },

    // Get filename from target file.
    fileName: function() {
        return this.target ? this.target.leafName : "";
    },

    // Set the dialog title.
    setTitle: function() {
        // Start with saving/opening template.
        var title = this.saving ? this.getString( "savingTitle" ) : this.getString( "openingTitle" );

        // Use file name as insert 1.
        title = this.replaceInsert( title, 1, this.fileName() );

        // Use percentage as insert 2.
        title = this.replaceInsert( title, 2, this.percent );

        // Set <window>'s title attribute.
        if ( this.dialog ) {
            this.dialog.title = title;
        }
    },

    // Update the dialog to indicate specified percent complete.
    setPercent: function( percent ) {
        // Maximum percentage is 100.
        if ( percent > 100 ) {
            percent = 100;
        }
        // Test if percentage is changing.
        if ( this.percent != percent ) {
            this.mPercent = percent;

            // If dialog not opened yet, bail early.
            if ( !this.loaded ) {
                return this.mPercent;
            }

            if ( percent == -1 ) {
                // Progress meter needs to be in "undetermined" mode.
                this.mode = "undetermined";

                // Update progress meter percentage text.
                this.setValue( "progressText", "" );
            } else {
                // Progress meter needs to be in normal mode.
                this.mode = "normal";

                // Set progress meter thermometer.
                this.setValue( "progress", percent );

                // Update progress meter percentage text.
                this.setValue( "progressText", this.replaceInsert( this.getString( "percentMsg" ), 1, percent ) );
            }
    
            // Update title.
            this.setTitle();
        }
        return this.mPercent;
    },

    // Update download rate and dialog display.
    // Note that we don't want the displayed value to quiver
    // between essentially identical values (e.g., 99.9Kb and
    // 100.0Kb) so we only update if we see a big change.
    setRate: function( rate ) {
        if ( rate ) {
            // rate is bytes/sec
            var change = Math.abs( this.rate - rate );
            // Don't update too often!
            if ( change > this.rate / 10 ) {
                // Displayed rate changes.
                this.mRate = rate;
            }
        }
        return this.mRate;
    },

    // Handle download completion.
    setCompleted: function() {
        // If dialog hasn't loaded yet, defer this.
        if ( !this.loaded ) {
            this.dialog.setTimeout( function(obj){obj.setCompleted()}, 100, this );
            return false;
        }
        if ( !this.mCompleted ) {
            this.mCompleted = true;

            // If the "keep dialog open" box is checked, then update dialog.
            if ( this.dialog && this.dialogElement( "keep" ).checked ) {
                // Indicate completion in status area.
                this.setValue( "status", this.replaceInsert( this.getString( "completeMsg" ),
                                                             1,
                                                             this.formatSeconds( this.elapsed/1000 ) ) );

                // Put progress meter at 100%.
                this.percent = 100;

                // Set time remaining to 00:00.
                this.setValue( "timeLeft", this.formatSeconds( 0 ) );

                // Change Cancel button to Close, and give it focus.
                var cancelButton = this.dialogElement( "cancel" );
                cancelButton.label = this.getString( "close" );
                cancelButton.focus();

                // Activate reveal/launch buttons.
                this.enable( "reveal" );
                if ( this.target && !this.target.isExecutable() ) {
                    this.enable( "launch" );
                }

                // Disable the Pause/Resume buttons.
                this.dialogElement( "pauseResume" ).disabled = true;

                // Fix up dialog layout (which gets messed up sometimes).
                this.dialog.sizeToContent();
            } else if ( this.dialog ) {
                this.dialog.close();
            }
        }
        return this.mCompleted;
    },

    // Set progress meter to given mode ("normal" or "undetermined").
    setMode: function( newMode ) {
        if ( this.mode != newMode ) {
            // Need to update progress meter.
            this.dialogElement( "progress" ).setAttribute( "mode", newMode );
        }
        return this.mMode = newMode;
    },

    // Set pause/resume state.
    setPaused: function( pausing ) {
        // If state changing, then update stuff.
        if ( this.paused != pausing ) {
            var string = pausing ? "resume" : "pause";
            this.dialogElement( "pauseResume" ).label = this.getString(string);

            // If we have a request, suspend/resume it.
            if ( this.request ) {
                if ( pausing ) {
                    this.request.suspend();
                } else {
                    this.request.resume();
                }
            }
        }
        return this.mPaused = pausing;
    },

    // Set the saved nsIRequest.  The first time, we test it for
    // ftp and initialize the pause/resume stuff.
    // XXX This is broken, I think, because if we're doing something
    //     like saving a web-page-complete that has multiple images
    //     accessed via ftp: urls, then it seems like the pause/resume
    //     button should come and go, depending on what's being downloaded
    //     at a given point in time.  The old dialog didn't handle that case
    //     either, though, so I'm not sweating it for now.
    setRequest: function( aRequest ) {
        if ( this.request == null && this.loaded && aRequest ) {
            // Right now, all that supports restarting downloads is ftp (rfc959).
            try {
                ftpChannel = aRequest.QueryInterface( Components.interfaces.nsIFTPChannel );
                if ( ftpChannel ) {
                    this.dialogElement("pauseResume").label = this.getString("pause");
                    this.paused = false;
                }
            } catch ( e ) {
            }
            // This *must* come after the "this.paused = false" above, so that we
            // don't suspend or resume the first time we call that function!
            this.mRequest = aRequest;
        }
        return this.mRequest;
    },

    // Convert raw rate (bytes/sec) to Kbytes/sec (to nearest tenth).
    rateToKRate: function( rate ) {
         var kRate = rate / 1024; // KBytes/sec
         var fraction = parseInt( ( kRate - ( kRate = parseInt( kRate ) ) ) * 10 + 0.5 );
         return kRate + "." + fraction;
    },

    // Format number of seconds in hh:mm:ss form.
    formatSeconds: function( secs ) {
        // Round the number of seconds to remove fractions.
        secs = parseInt( secs + .5 );
        var hours = parseInt( secs/3600 );
        secs -= hours*3600;
        var mins = parseInt( secs/60 );
        secs -= mins*60;
        var result;
        if ( hours )
            result = this.getString( "longTimeFormat" );
        else
            result = this.getString( "shortTimeFormat" );
    
        if ( hours < 10 )
            hours = "0" + hours;
        if ( mins < 10 )
            mins = "0" + mins;
        if ( secs < 10 )
            secs = "0" + secs;
    
        // Insert hours, minutes, and seconds into result string.
        result = this.replaceInsert( result, 1, hours );
        result = this.replaceInsert( result, 2, mins );
        result = this.replaceInsert( result, 3, secs );
    
        return result;
    },

    // Get dialog element using argument as id.
    dialogElement: function( id ) {
        // Check if we've already fetched it.
        if ( !( id in this.fields ) ) {
            // No, then get it from dialog.
            try {
                this.fields[ id ] = this.dialog.document.getElementById( id );
            } catch(e) {
                this.fields[ id ] = { 
                    value: "",
                    setAttribute: function(id,val) {},
                    removeAttribute: function(id) {}
                    }
            }
        }
        return this.fields[ id ];
    },

    // Set dialog element value for given dialog element.
    setValue: function( id, val ) {
        this.dialogElement( id ).value = val;
    },

    // Enable dialgo element.
    enable: function( field ) {
        this.dialogElement( field ).removeAttribute( "disabled" );
    },

    // Get localizable string from properties file.
    getProperty: function( propertyId, strings, len ) {
        if ( !this.mBundle ) {
            this.mBundle = Components.classes[ "@mozilla.org/intl/stringbundle;1" ]
                             .getService( Components.interfaces.nsIStringBundleService )
                               .createBundle( "chrome://global/locale/nsProgressDialog.properties");
        }
        return this.mBundle.formatStringFromName( propertyId, strings, len );
    },

    // Get localizable string (from dialog <data> elements).
    getString: function ( stringId ) {
        // Check if we've fetched this string already.
        if ( !( this.strings && stringId in this.strings ) ) {
            // Presume the string is empty if we can't get it.
            this.strings[ stringId ] = "";
            // Try to get it.
            try {
                this.strings[ stringId ] = this.dialog.document.getElementById( "string."+stringId ).childNodes[0].nodeValue;
            } catch (e) {}
       }
       return this.strings[ stringId ];
    },
    
    // Replaces insert ("#n") with input text.
    replaceInsert: function( text, index, value ) {
        var result = text;
        var regExp = new RegExp( "#"+index );
        result = result.replace( regExp, value );
        return result;
    },

    // Hide a given dialog field.
    hide: function( field ) {
        this.dialogElement( field ).setAttribute( "style", "display: none;" );
        // Hide the associated separator, too.
        this.dialogElement( field+"Separator" ).setAttribute( "style", "display: none;" );
    },

    // Return input in hex, prepended with "0x" and leading zeros (to 8 digits).
    hex: function( x ) {
         var hex = Number(x).toString(16);
         return "0x" + ( "00000000" + hex ).substring( hex.length );
    },

    // Dump text (if debug is on).
    dump: function( text ) {
        if ( this.debug ) {
            dump( text );
        }
    }
}

// This Component's module implementation.  All the code below is used to get this
// component registered and accessible via XPCOM.
var module = {
    // registerSelf: Register this component.
    registerSelf: function (compMgr, fileSpec, location, type) {
        compMgr = compMgr.QueryInterface( Components.interfaces.nsIComponentManagerObsolete );
        compMgr.registerComponentWithType( this.cid,
                                           "Mozilla Download Progress Dialog",
                                           this.contractId,
                                           fileSpec,
                                           location,
                                           true,
                                           true,
                                           type );
    },

    // getClassObject: Return this component's factory object.
    getClassObject: function (compMgr, cid, iid) {
        if (!cid.equals(this.cid))
            throw Components.results.NS_ERROR_NO_INTERFACE;

        if (!iid.equals(Components.interfaces.nsIFactory))
            throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

        return this.factory;
    },

    /* CID for this class */
    cid: Components.ID("{F5D248FD-024C-4f30-B208-F3003B85BC92}"),

    /* Contract ID for this class */
    contractId: "@mozilla.org/progressdialog;1",

    /* factory object */
    factory: {
        // createInstance: Return a new nsProgressDialog object.
        createInstance: function (outer, iid) {
            if (outer != null)
                throw Components.results.NS_ERROR_NO_AGGREGATION;

            return (new nsProgressDialog()).QueryInterface(iid);
        }
    },

    // canUnload: n/a (returns true)
    canUnload: function(compMgr) {
        return true;
    }
};

// NSGetModule: Return the nsIModule object.
function NSGetModule(compMgr, fileSpec) {
    return module;
}
