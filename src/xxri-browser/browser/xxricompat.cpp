/*
 * xxricompat.cpp - see xxricompat.h.
 */

#include "xxricompat.h"

#include <QWebEngineProfile>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QWebEngineUrlRequestInterceptor>
#include <QWebEngineUrlRequestInfo>
#include <QString>
#include <QLocale>

namespace {

/*
 * Every entry here is a JavaScript standard-library function that shipped
 * after Chromium 87 and that real sites now call without a guard.  The
 * Chrome version each landed in is noted so the list can be pruned if the
 * engine is ever moved forward.
 */
const char *const kPolyfills = R"JS(
(function () {
  'use strict';

  // Array.prototype.at / String.prototype.at / TypedArray.prototype.at  (Chrome 92)
  // GitHub's bundle calls entries.at() during boot; without it the page throws
  // before any of its UI is constructed.
  function at(n) {
    n = Math.trunc(n) || 0;
    if (n < 0) n += this.length;
    if (n < 0 || n >= this.length) return undefined;
    return this[n];
  }
  var arrayLikes = [Array.prototype, String.prototype];
  var TA = Object.getPrototypeOf(Int8Array);
  if (TA && TA.prototype) arrayLikes.push(TA.prototype);
  arrayLikes.forEach(function (p) {
    if (typeof p.at !== 'function') {
      Object.defineProperty(p, 'at', {
        value: at, writable: true, enumerable: false, configurable: true
      });
    }
  });

  // Array.prototype.findLast / findLastIndex  (Chrome 97)
  if (typeof Array.prototype.findLast !== 'function') {
    Object.defineProperty(Array.prototype, 'findLast', {
      value: function (fn, thisArg) {
        for (var i = this.length - 1; i >= 0; i--)
          if (fn.call(thisArg, this[i], i, this)) return this[i];
        return undefined;
      }, writable: true, configurable: true
    });
  }
  if (typeof Array.prototype.findLastIndex !== 'function') {
    Object.defineProperty(Array.prototype, 'findLastIndex', {
      value: function (fn, thisArg) {
        for (var i = this.length - 1; i >= 0; i--)
          if (fn.call(thisArg, this[i], i, this)) return i;
        return -1;
      }, writable: true, configurable: true
    });
  }

  // Object.hasOwn  (Chrome 93)
  if (typeof Object.hasOwn !== 'function') {
    Object.defineProperty(Object, 'hasOwn', {
      value: function (o, k) { return Object.prototype.hasOwnProperty.call(o, k); },
      writable: true, configurable: true
    });
  }

  // crypto.randomUUID  (Chrome 92)
  // Backed by crypto.getRandomValues, which Chromium 87 does have, so the
  // values are as random as the engine's own CSPRNG - this is not a stub.
  if (self.crypto && typeof self.crypto.randomUUID !== 'function' &&
      typeof self.crypto.getRandomValues === 'function') {
    Object.defineProperty(self.crypto, 'randomUUID', {
      value: function () {
        var b = new Uint8Array(16);
        self.crypto.getRandomValues(b);
        b[6] = (b[6] & 0x0f) | 0x40;          // version 4
        b[8] = (b[8] & 0x3f) | 0x80;          // variant 1
        var h = [];
        for (var i = 0; i < 256; i++) h[i] = (i + 0x100).toString(16).substr(1);
        return h[b[0]] + h[b[1]] + h[b[2]] + h[b[3]] + '-' +
               h[b[4]] + h[b[5]] + '-' + h[b[6]] + h[b[7]] + '-' +
               h[b[8]] + h[b[9]] + '-' +
               h[b[10]] + h[b[11]] + h[b[12]] + h[b[13]] + h[b[14]] + h[b[15]];
      }, writable: true, configurable: true
    });
  }

  // structuredClone  (Chrome 98)
  // YouTube calls this on plain data.  A deep clone that understands the
  // structured types the engine already has is a faithful stand-in; anything
  // it cannot clone throws DataCloneError, as the real one does.
  if (typeof self.structuredClone !== 'function') {
    self.structuredClone = function (value) {
      var seen = new Map();
      function clone(v) {
        if (v === null || typeof v !== 'object') {
          if (typeof v === 'function') throw new DOMException(
            'Function object could not be cloned.', 'DataCloneError');
          return v;
        }
        if (seen.has(v)) return seen.get(v);
        var out;
        if (v instanceof Date) out = new Date(v.getTime());
        else if (v instanceof RegExp) out = new RegExp(v.source, v.flags);
        else if (v instanceof ArrayBuffer) out = v.slice(0);
        else if (ArrayBuffer.isView(v))
          out = new v.constructor(clone(v.buffer), v.byteOffset, v.length);
        else if (v instanceof Map) {
          out = new Map(); seen.set(v, out);
          v.forEach(function (val, key) { out.set(clone(key), clone(val)); });
          return out;
        } else if (v instanceof Set) {
          out = new Set(); seen.set(v, out);
          v.forEach(function (val) { out.add(clone(val)); });
          return out;
        } else if (Array.isArray(v)) { out = []; }
        else if (Object.getPrototypeOf(v) === Object.prototype ||
                 Object.getPrototypeOf(v) === null) { out = {}; }
        else throw new DOMException(
          'Object could not be cloned.', 'DataCloneError');
        seen.set(v, out);
        Object.keys(v).forEach(function (k) { out[k] = clone(v[k]); });
        return out;
      }
      return clone(value);
    };
  }

  // Error cause  (Chrome 93) - keep the second argument from being dropped
  (function () {
    try { if ('cause' in new Error('x', { cause: 1 })) return; } catch (e) {}
    [Error, TypeError, RangeError, SyntaxError, EvalError, ReferenceError,
     URIError].forEach(function (E) {
      var Orig = E;
      if (!Orig) return;
      var Wrapped = function (msg, opts) {
        var e = new Orig(msg);
        if (opts && typeof opts === 'object' && 'cause' in opts)
          Object.defineProperty(e, 'cause', {
            value: opts.cause, writable: true, configurable: true });
        return e;
      };
      Wrapped.prototype = Orig.prototype;
      try { self[Orig.name] = Wrapped; } catch (e) {}
    });
  })();

  // AbortSignal.timeout / AbortSignal.abort  (Chrome 93 / 103)
  if (self.AbortSignal) {
    if (typeof self.AbortSignal.abort !== 'function') {
      self.AbortSignal.abort = function (reason) {
        var c = new AbortController(); c.abort(reason); return c.signal;
      };
    }
    if (typeof self.AbortSignal.timeout !== 'function') {
      self.AbortSignal.timeout = function (ms) {
        var c = new AbortController();
        setTimeout(function () { c.abort(new DOMException(
          'signal timed out', 'TimeoutError')); }, ms);
        return c.signal;
      };
    }
  }

  // String.prototype.replaceAll  (Chrome 85 - present, but cheap to be sure)
  if (typeof String.prototype.replaceAll !== 'function') {
    Object.defineProperty(String.prototype, 'replaceAll', {
      value: function (find, rep) {
        if (find instanceof RegExp) {
          if (!find.global) throw new TypeError(
            'replaceAll must be called with a global RegExp');
          return this.replace(find, rep);
        }
        return this.split(find).join(rep);
      }, writable: true, configurable: true
    });
  }

  // Array.prototype.group / groupToMap are still proposals in most engines;
  // deliberately not shipped.
})();
)JS";


/*
 * CSS cascade layers.
 *
 * `@layer` shipped in Chrome 99.  An engine that does not know an at-rule must
 * discard it *and everything inside it*, so on Chromium 87 a stylesheet that
 * wraps its whole content in one layer is thrown away in full.  That is not a
 * hypothetical: GitHub's design system ships
 * primer-react-brand-css.module.css at 700KB, of which 100.0% is inside a
 * single `@layer` block, so the entire marketing page rendered in Times with
 * unstyled links and overflowed its viewport horizontally.
 *
 * Layers exist to order the cascade; they do not change what a rule does.
 * Unwrapping them keeps every declaration and keeps source order, which for a
 * sheet that is one single layer is exactly equivalent.  Sheets that do not
 * use `@layer` are left completely untouched - this rewrites nothing it does
 * not have to.
 */
const char *const kLayerShim = R"JS(
(function () {
  'use strict';

  // Does this engine understand @layer?  Ask it, rather than assuming.
  function supportsLayers() {
    try {
      var s = document.createElement('style');
      s.textContent = '@layer xxri-probe{.xxri-probe-el{top:1px}}';
      (document.head || document.documentElement).appendChild(s);
      var ok = false;
      try {
        var r = s.sheet && s.sheet.cssRules;
        ok = !!(r && r.length && (r[0].cssText || '').indexOf('@layer') === 0);
      } catch (e) { ok = false; }
      s.parentNode.removeChild(s);
      return ok;
    } catch (e) { return false; }
  }
  if (supportsLayers()) return;          // nothing to do on a newer engine

  // `@layer a, b;` (an ordering statement) and `@layer name { ... }` (a block).
  // The statement is dropped; the block is replaced by its own contents.
  /*
   * Media Queries Level 4 range syntax - `(width >= 48rem)` - shipped in
   * Chrome 104.  This engine parses only `(min-width: 48rem)`, and an
   * unparseable media query drops the whole block.  That is most of a modern
   * responsive stylesheet: of GitHub's 245 media blocks in one file, 201 use
   * the range form and none use the classic one, so almost every breakpoint
   * was being discarded - which is why the page stayed in its narrow layout
   * whatever the window size, and why elements that a breakpoint was supposed
   * to constrain overflowed instead.
   *
   * The rewrite is meaning-preserving for >= and <=.  The strict forms have no
   * classic equivalent (there is no "min-width but not equal"), so they map to
   * the inclusive one - a one-pixel difference at the exact breakpoint, which
   * is the closest this engine can express.
   */
  function fixRangeMedia(css) {
    return css.replace(/@media[^{]*/g, function (prelude) {
      if (prelude.indexOf('<') === -1 && prelude.indexOf('>') === -1)
        return prelude;
      // (A <= name <= B)  ->  (min-name:A) and (max-name:B)
      prelude = prelude.replace(
        /\(\s*([\w.%-]+)\s*(<=?|>=?)\s*([a-zA-Z-]+)\s*(<=?|>=?)\s*([\w.%-]+)\s*\)/g,
        function (m, v1, o1, name, o2, v2) {
          var lo = (o1.charAt(0) === '<' ? 'min-' : 'max-') + name + ':' + v1;
          var hi = (o2.charAt(0) === '<' ? 'max-' : 'min-') + name + ':' + v2;
          return '(' + lo + ') and (' + hi + ')';
        });
      // (name >= V)
      prelude = prelude.replace(
        /\(\s*([a-zA-Z-]+)\s*(<=|>=|<|>)\s*([\w.%-]+)\s*\)/g,
        function (m, name, op, v) {
          return '(' + (op.charAt(0) === '>' ? 'min-' : 'max-') + name + ':' + v + ')';
        });
      // (V >= name)
      prelude = prelude.replace(
        /\(\s*([\w.%-]+)\s*(<=|>=|<|>)\s*([a-zA-Z-]+)\s*\)/g,
        function (m, v, op, name) {
          return '(' + (op.charAt(0) === '<' ? 'min-' : 'max-') + name + ':' + v + ')';
        });
      return prelude;
    });
  }

  function needsWork(css) {
    return css.indexOf('@layer') !== -1 ||
           /@media[^{]*[<>]/.test(css);
  }

  function unwrap(css) {
    if (!needsWork(css)) return null;
    if (css.indexOf('@layer') === -1) return fixRangeMedia(css);
    var out = '', i = 0, n = css.length, changed = false;
    while (i < n) {
      var at = css.indexOf('@layer', i);
      if (at === -1) { out += css.slice(i); break; }
      out += css.slice(i, at);
      var j = at + 6, brace = -1, semi = -1;
      while (j < n) {
        var c = css.charAt(j);
        if (c === '{') { brace = j; break; }
        if (c === ';') { semi = j; break; }
        j++;
      }
      if (semi !== -1 && brace === -1) { i = semi + 1; changed = true; continue; }
      if (brace === -1) { out += css.slice(at); break; }
      // Walk to the matching close brace, ignoring braces inside strings.
      var depth = 0, k = brace, q = 0, end = -1;
      for (; k < n; k++) {
        var ch = css.charAt(k);
        if (q) { if (ch === '\\') k++; else if (ch === q) q = 0; continue; }
        if (ch === '"' || ch === "'") { q = ch; continue; }
        if (ch === '{') depth++;
        else if (ch === '}') { depth--; if (!depth) { end = k; break; } }
      }
      if (end === -1) { out += css.slice(brace + 1); break; }
      out += css.slice(brace + 1, end);   // the layer's contents, unwrapped
      i = end + 1;
      changed = true;
    }
    return fixRangeMedia(changed ? out : css);
  }

  function repair(link) {
    if (link.dataset && link.dataset.xxriLayerFixed) return;
    var href = link.href;
    if (!href) return;
    if (link.dataset) link.dataset.xxriLayerFixed = '1';
    // force-cache: the engine has already downloaded this sheet, so this is a
    // cache read, not a second trip to the network.
    fetch(href, { cache: 'force-cache', credentials: 'omit' })
      .then(function (r) { return r.ok ? r.text() : null; })
      .then(function (css) {
        if (!css) return;
        var fixed = unwrap(css);
        if (fixed === null) return;      // no layers here - leave it alone
        var style = document.createElement('style');
        style.setAttribute('data-xxri-layer-shim', href);
        if (link.media) style.media = link.media;
        style.textContent = fixed;
        link.parentNode.insertBefore(style, link.nextSibling);
        link.disabled = true;            // the original contributed nothing
      })
      .catch(function () { /* leave the page exactly as it was */ });
  }

  function scan() {
    var links = document.querySelectorAll('link[rel~="stylesheet"][href]');
    for (var i = 0; i < links.length; i++) repair(links[i]);
  }

  if (document.readyState === 'loading')
    document.addEventListener('DOMContentLoaded', scan);
  else
    scan();
  // Sheets added later by the page get the same treatment.
  window.addEventListener('load', scan);
})();
)JS";

/*
 * User agent.
 *
 * Google, and a number of other sites, keep a server-side list of browser
 * versions and serve a cut-down page to anything older.  What comes back for
 * "Chrome/87" is a 2013-era layout, which is why the shipped browser looked
 * like an old browser even though it renders the modern page perfectly well
 * when it is given one.
 *
 * The engine is genuinely Chromium 87, and this does not pretend otherwise to
 * anything that asks the engine itself.  What it changes is the version in the
 * request header, so those version gates let the modern page through - and the
 * polyfills above are what make the modern page actually run.  110 is chosen
 * because it is the last Chrome that supported the same class of machine XXRI
 * targets, so sites that branch on it branch to code this engine can run.
 */
const char *const kUserAgent =
    "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) "
    "Chrome/110.0.0.0 Safari/537.36";
/*
 * The platform token says x86_64 rather than i686 deliberately.  A header
 * reading "Linux i686" together with "Chrome/110" describes a browser that has
 * never existed - Chrome dropped 32-bit Linux at version 48 - and an
 * impossible combination is exactly the sort of thing bot scoring looks for.
 * The version has to be raised for the version gates (see above); making the
 * rest of the string consistent with it costs nothing and stops the header
 * itself being the anomaly.
 */


/*
 * User-agent client hints.
 *
 * Chromium 87 predates them: it sends Sec-Fetch-Site/Mode/Dest on every
 * request, as every Chromium since 76 does, but no Sec-CH-UA at all, because
 * those headers arrived in Chrome 89.  Combined with a User-Agent that has to
 * claim 110 to get past the version gates (see kUserAgent), that produces a
 * request set no real browser has ever sent: a client that says it is Chrome
 * 110 and behaves like Chromium in every other header, while omitting the one
 * header every Chrome since 89 sends.  Google answers such a request with its
 * "unusual traffic" interstitial rather than with results - which was
 * reproducible here on the first search of a cold profile, while the same
 * query from a normal client on the same network returned results.
 *
 * These three headers make the claim consistent with itself.  Nothing else is
 * touched, and every other request goes through unchanged.
 */
class XxriHeaderShim : public QWebEngineUrlRequestInterceptor
{
public:
    explicit XxriHeaderShim(QObject *parent = nullptr)
        : QWebEngineUrlRequestInterceptor(parent) {}

    void interceptRequest(QWebEngineUrlRequestInfo &info) override
    {
        info.setHttpHeader(QByteArrayLiteral("sec-ch-ua"),
                           QByteArrayLiteral("\"Not A(Brand\";v=\"24\", "
                                             "\"Chromium\";v=\"110\", "
                                             "\"Google Chrome\";v=\"110\""));
        info.setHttpHeader(QByteArrayLiteral("sec-ch-ua-mobile"),
                           QByteArrayLiteral("?0"));
        info.setHttpHeader(QByteArrayLiteral("sec-ch-ua-platform"),
                           QByteArrayLiteral("\"Linux\""));
    }
};

} // namespace

void XxriCompat::install(QWebEngineProfile *profile)
{
    if (!profile)
        return;

    // XXRI_BROWSER_UA overrides the header outright.  The version this engine
    // has to claim is a judgement call that different networks answer
    // differently, so it is settable without a rebuild.
    const QByteArray uaOverride = qgetenv("XXRI_BROWSER_UA");
    profile->setHttpUserAgent(uaOverride.isEmpty()
                              ? QString::fromLatin1(kUserAgent)
                              : QString::fromUtf8(uaOverride));

    /*
     * Accept-Language.
     *
     * This engine sends none at all - confirmed by pointing the booted browser
     * at a header echo, which listed Accept, Accept-Encoding, Host, the
     * Sec-* set and User-Agent, and nothing else.  Every real browser sends
     * one, so its absence is itself an anomaly, and it also leaves servers to
     * guess the language from the address.  Taken from the system locale, with
     * English as the fallback every site understands.
     */
    QString lang = QLocale::system().name().replace(QLatin1Char('_'),
                                                    QLatin1Char('-'));
    if (lang.isEmpty() || lang.startsWith(QLatin1Char('C')))
        lang = QStringLiteral("en-US");
    const QString primary = lang.section(QLatin1Char('-'), 0, 0);
    QString accept = lang + QStringLiteral(",") + primary
            + QStringLiteral(";q=0.9");
    if (primary != QLatin1String("en"))
        accept += QStringLiteral(",en-US;q=0.8,en;q=0.7");
    profile->setHttpAcceptLanguage(accept);

    // DocumentCreation, main world: the polyfills have to exist before the
    // page's own scripts run, and they have to be on the same globals the page
    // sees, so an isolated world would be useless here.
    QWebEngineScript shim;
    shim.setName(QStringLiteral("xxri-compat"));
    shim.setInjectionPoint(QWebEngineScript::DocumentCreation);
    shim.setWorldId(QWebEngineScript::MainWorld);
    shim.setRunsOnSubFrames(true);
    shim.setSourceCode(QString::fromUtf8(kPolyfills));

    QWebEngineScriptCollection *scripts = profile->scripts();
    if (scripts && !scripts->contains(shim))
        scripts->insert(shim);

    // The cascade-layer repair runs once the document exists, because it needs
    // to see the page's <link> elements.
    QWebEngineScript layers;
    layers.setName(QStringLiteral("xxri-css-layers"));
    layers.setInjectionPoint(QWebEngineScript::DocumentReady);
    layers.setWorldId(QWebEngineScript::MainWorld);
    layers.setRunsOnSubFrames(true);
    layers.setSourceCode(QString::fromUtf8(kLayerShim));
    if (scripts && !scripts->contains(layers))
        scripts->insert(layers);

    // install() is idempotent: it is called once per profile, and the shim is
    // parented to the profile so it lives exactly as long as it.
    if (!profile->findChild<XxriHeaderShim *>())
        profile->setUrlRequestInterceptor(new XxriHeaderShim(profile));
}
