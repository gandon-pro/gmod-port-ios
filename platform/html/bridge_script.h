#pragma once
// Shared verbatim by WKWebView and the browser integration test.
static const char kGMPHTMLBridge[] = R"GMPJS(
(function () {
    if (window.__gmpBridge) return;
    window.__gmpBridge = true;
    var callbacks = Object.create(null), sequence = 1;
    window.__gmpComplete = function (id, args) {
        var fn = callbacks[id];
        delete callbacks[id];
        if (fn) fn.apply(null, args);
    };
    window.__gmpRegister = function (object, name) {
        window[object] = window[object] || {};
        window[object][name] = function () {
            var args = Array.from(arguments), request = null;
            if (typeof args[args.length - 1] === 'function') {
                request = sequence++;
                callbacks[request] = args.pop();
            }
            window.webkit.messageHandlers.gmp.postMessage({name: object + '.' + name, args: args, request: request});
        };
    };
    window.addEventListener('load', function () {
        var tries = 0;
        function ready() {
            if (window.gScope && window.scope) {
                window.webkit.messageHandlers.gmp.postMessage({name: '_gmp.Ready', args: []});
            } else if (++tries < 400) {
                setTimeout(ready, 25);
            } else {
                window.webkit.messageHandlers.gmp.postMessage({name: '_gmp.Error', args: ['Original menu controller did not initialize']});
            }
        }
        ready();
    });
    window.addEventListener('error', function (event) {
        var target = event.target;
        window.webkit.messageHandlers.gmp.postMessage({
            name: event.message ? '_gmp.Error' : '_gmp.ResourceError',
            args: [event.message || target.src || target.href || 'Resource failure', event.message ? 'JS' : target.tagName]
        });
    }, true);
    window.__gmpViewport = function (width) {
        var meta = document.querySelector('meta[name=viewport]');
        if (!meta) { meta = document.createElement('meta'); meta.name = 'viewport'; document.head.appendChild(meta); }
        meta.content = 'width=' + width + ',user-scalable=no';
    };
    window.__gmpNotify = function (message) {
        if (!document.body) return;
        var panel = document.getElementById('gmp-port-status');
        if (!panel) {
            panel = document.createElement('div'); panel.id = 'gmp-port-status';
            panel.style.cssText = 'position:fixed;z-index:999999;left:20px;right:20px;top:20px;padding:16px;background:#202020;color:white;font:18px sans-serif;white-space:pre-wrap';
            panel.onclick = function () { this.remove(); }; document.body.appendChild(panel);
        }
        panel.textContent = message;
    };
    window.__gmpPaintBackground = function (items, width, height) {
        if (!document.body) return;
        var panel = document.getElementById('gmp-background');
        if (!panel) {
            panel = document.createElement('div'); panel.id = 'gmp-background';
            panel.style.cssText = 'position:fixed;inset:0;overflow:hidden;pointer-events:none;z-index:-1';
            document.body.prepend(panel);
        }
        items.forEach(function (item, i) {
            var image = panel.children[i];
            if (!image) { image = document.createElement('img'); image.style.position = 'absolute'; panel.appendChild(image); }
            var url = 'asset://garrysmod/' + item.path;
            if (image.getAttribute('src') !== url) image.setAttribute('src', url);
            image.style.left = (item.x / width * 100) + '%';
            image.style.top = (item.y / height * 100) + '%';
            image.style.width = (item.w / width * 100) + '%';
            image.style.height = (item.h / height * 100) + '%';
            image.style.opacity = item.alpha / 255;
            image.style.transform = 'rotate(' + item.angle + 'deg)';
        });
        while (panel.children.length > items.length) panel.lastChild.remove();
    };
})();
)GMPJS";
