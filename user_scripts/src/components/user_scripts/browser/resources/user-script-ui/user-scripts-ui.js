import {sendWithPromise} from 'chrome://resources/js/cr.m.js';
import {$} from 'chrome://resources/js/util.m.js';

document.addEventListener('DOMContentLoaded', function() {
  const urlParams = new URLSearchParams(window.location.search);
  sendWithPromise('requestSource', urlParams.get('key')).then(textContent => {
    $('content').textContent = textContent[0].content;
  });
});