'use strict';

var Js_null = require("bs-platform/lib/js/js_null.js");
var Process = require("process");
var Puppeteer = require("puppeteer");

console.log("Executable: " + Puppeteer.executablePath());

function logHtml() {
  return Puppeteer.launch(undefined).then((function (browser) {
                      return browser.newPage();
                    })).then((function (page) {
                    var options = {
                      timeout: 25000
                    };
                    return page.goto("https://google.com", options).then((function (res) {
                                  return Js_null.getExn(res).text();
                                }));
                  })).then((function (text) {
                  return Promise.resolve((console.log(text), /* () */0));
                })).then((function () {
                return Promise.resolve((Process.exit(0), /* () */0));
              }));
}

logHtml(/* () */0);

exports.logHtml = logHtml;
/*  Not a pure module */
