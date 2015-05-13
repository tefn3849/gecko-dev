/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- /
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

function debug(str) {
  dump(' -*- Shell-Remote.js: ' + str + '\n');
}

var shellRemote = {

  get homeURL() {
    return "app://system.gaiamobile.org/index-remote.html";
  },

  get manifestURL() {
    return "app://system.gaiamobile.org/manifest.webapp";
  },

  _started: false,
  hasStarted: function shellRemote_hasStarted() {
    return this._started;
  },

  start: function shellRemote_start() {
    debug("---- Starting shell remote! ----");
    this._started = true;

    let homeURL = this.homeURL;
    if (!homeURL) {
      debug("ERROR! Remote home URL undefined.");
      return;
    }
    let manifestURL = this.manifestURL;
    // <html:iframe id="remote-systemapp"
    //              mozbrowser="true" allowfullscreen="true"
    //              style="overflow: hidden; height: 100%; width: 100%; border: none;"
    //              src="data:text/html;charset=utf-8,%3C!DOCTYPE html>%3Cbody style='background:black;'>"/>
    let systemAppFrame =
      document.createElementNS('http://www.w3.org/1999/xhtml', 'html:iframe');
    systemAppFrame.setAttribute('id', 'remote-systemapp');
    systemAppFrame.setAttribute('mozbrowser', 'true');
    systemAppFrame.setAttribute('mozapp', manifestURL);
    systemAppFrame.setAttribute('allowfullscreen', 'true');
    systemAppFrame.setAttribute('style', "overflow: hidden; height: 100%; width: 100%; border: none; position: absolute; left: 0; top: 0; right: 0; bottom: 0;");
    systemAppFrame.setAttribute('src', "data:text/html;charset=utf-8,%3C!DOCTYPE html>%3Cbody");

    let container = document.getElementById('container');
    this.contentBrowser = container.appendChild(systemAppFrame);
    this.contentBrowser.src = homeURL + window.location.hash;
  },

  stop: function shellRemote_stop() {

  },

};

window.onload = function() {
  if (shellRemote.hasStarted() == false) {
    shellRemote.start();
  }
};
