// tor-bridge.js — Emscripten JS library that wires Ladybird's tor_socket_*
// C functions to wanix-tor's window.torBridge API.
//
// Isolation: Ladybird connections live in LadybirdTor.conns (keyed by our
// own sequential ids, scoped inside the Emscripten module closure) to avoid
// colliding with the SSH session that uses window.__artiConns / window.torBridge
// directly.
//
// HEAPU8 note: after every Asyncify suspend (await) the WASM heap may have
// grown and Module.HEAPU8 been replaced. Always use
// new Uint8Array(wasmMemory.buffer) after any await.

mergeInto(LibraryManager.library, {

  // ── WASI random_get override ────────────────────────────────────────────────
  // Emscripten's default random_get uses the module-local `HEAPU8` view which
  // can be stale after ASYNCIFY stack-save grows WASM memory.  Reading from
  // `wasmMemory.buffer` always returns the live ArrayBuffer.
  random_get__deps: ['$randomFill'],
  random_get: function(buf, buf_len) {
    buf >>>= 0;
    buf_len >>>= 0;
    try {
      randomFill((new Uint8Array(wasmMemory.buffer)).subarray(buf, buf + buf_len));
      return 0;
    } catch (e) {
      if (typeof FS == 'undefined' || !(e.name === 'ErrnoError')) throw e;
      return e.errno;
    }
  },

  // ── Internal state ─────────────────────────────────────────────────────────

  $LadybirdTor: {
    nextId: 1,
    conns: {},      // id → { jsConn, connId }

    async connect(host, port) {
      const tb = (typeof window !== 'undefined') ? window.torBridge : null;
      if (!tb) {
        console.error('[LadybirdTor] window.torBridge not available');
        return -1;
      }

      const id = this.nextId++;
      try {
        const addr = host + ':' + port;
        // Use connectIsolated if available (per-domain circuit isolation).
        const connId = tb.connectIsolated
            ? await tb.connectIsolated(addr, host)
            : await tb.connectBootstrapped(addr);
        if (connId == null || connId < 0) {
          console.error('[LadybirdTor] connect failed');
          return -1;
        }
        // exposeConnection publishes the connId into the Arti connection registry.
        await tb.exposeConnection(connId);
        // Prefer the closure-scoped accessor if available, fall back to legacy.
        const registry = window.__artiConns;
        const jsConn = (registry && typeof registry.get === 'function')
            ? registry.get(connId)
            : registry && registry[connId];
        if (!jsConn) {
          console.error('[LadybirdTor] no jsConn for connId', connId,
              'registry type:', typeof registry,
              'get type:', typeof registry?.get,
              'direct:', typeof (registry && registry[connId]));
          return -1;
        }
        this.conns[id] = { jsConn, connId };
        return id;
      } catch (e) {
        console.error('[LadybirdTor] connect error');
        return -1;
      }
    },

    async read(id, bufPtr, size) {
      const entry = this.conns[id];
      if (!entry) return -1;
      try {
        const result = await entry.jsConn.read(size);
        if (!result || !result.data) return 0;  // EOF
        const n = Math.min(size, result.data.length);
        new Uint8Array(wasmMemory.buffer).set(result.data.subarray(0, n), bufPtr);
        return n;
      } catch (e) {
        console.error('[LadybirdTor] read error');
        return -1;
      }
    },

    async write(id, dataPtr, size) {
      const entry = this.conns[id];
      if (!entry) return -1;
      try {
        // Snapshot data before suspend — bufPtr may be invalidated after await.
        const data = new Uint8Array(wasmMemory.buffer).slice(dataPtr, dataPtr + size);
        const n = await entry.jsConn.write(data);
        return (n == null) ? size : n;
      } catch (e) {
        console.error('[LadybirdTor] write error');
        return -1;
      }
    },

    close(id) {
      const entry = this.conns[id];
      if (!entry) return;
      try { entry.jsConn.close(); } catch (_) { }
      delete this.conns[id];
    }
  },

  // ── LadybirdHTTP: HTTP/1.1 over Tor socket ───────────────────────────────────
  // Performs a full HTTP/1.1 request over an existing Tor TCP connection.
  // For HTTPS, a TLS handshake is performed via synchronous C exports that use
  // a buffer-BIO approach — no Asyncify re-entry from C.
  //
  // Results are stored in the $LadybirdHTTP object.  C++ retrieves them via
  // ladybird_http_get_status / ladybird_http_get_headers / ladybird_http_get_body
  // after ladybird_http_request returns, then calls ladybird_http_free_result.
  //
  // Headers are returned as a flat UTF-8 string: "Name: Value\r\nName: Value\r\n"
  // (i.e. raw HTTP header format, without a terminating blank line).
  $LadybirdHTTP__deps: ['malloc', 'free'],
  $LadybirdHTTP: {
    status: 0,
    headersPtr: 0,   // malloc'd, null-terminated "Name: Value\r\n..." string
    bodyPtr: 0,      // malloc'd body bytes
    bodyLen: 0,

    _reset() {
      if (this.headersPtr) { _free(this.headersPtr); this.headersPtr = 0; }
      if (this.bodyPtr)    { _free(this.bodyPtr);    this.bodyPtr    = 0; }
      this.status  = 0;
      this.bodyLen = 0;
    },

    // Flush any ciphertext OpenSSL wrote into out_buf to Tor.
    // MUST snapshot + clear out_buf BEFORE any await (WASM memory may grow).
    async _flushOutgoing(sockId, conn) {
      if (!conn) return;
      const outLen = _tor_tls_get_outgoing_len(sockId);
      if (outLen <= 0) return;
      const outPtr = _tor_tls_get_outgoing_ptr(sockId);
      // Snapshot BEFORE await — wasmMemory.buffer may be replaced after any await.
      const snapshot = new Uint8Array(wasmMemory.buffer, outPtr, outLen).slice();
      _tor_tls_clear_outgoing(sockId);   // clear before await
      await this._withTimeout(conn.jsConn.write(snapshot), 120000);
    },

    // Sanitize HTTP token (method, header name): strip CR/LF.
    _sanitizeToken(s) {
      return s.replace(/[\r\n]+/g, '');
    },

    // Sanitize HTTP header value: collapse obs-fold, strip bare CR/LF.
    _sanitizeHeaderValue(s) {
      return s.replace(/\r\n[ \t]/g, ' ').replace(/[\r\n]+/g, '');
    },

    // Race a promise against a timeout. Rejects with 'timeout' on expiry.
    _withTimeout(promise, ms) {
      let timer;
      return Promise.race([
        promise,
        new Promise((_, reject) => { timer = setTimeout(() => reject(new Error('timeout')), ms); }),
      ]).finally(() => clearTimeout(timer));
    },

    // HTML-escape a string for safe interpolation into HTML content/attributes.
    _htmlEscape(s) {
      return s.replace(/&/g,'&amp;').replace(/</g,'&lt;')
              .replace(/>/g,'&gt;').replace(/"/g,'&quot;');
    },

    // Decode chunked transfer-encoding body.
    _decodeChunked(data) {
      const chunks = [];
      const dec = new TextDecoder();
      let pos = 0;
      while (pos < data.length) {
        // Find CRLF that terminates the chunk-size line.
        let lineEnd = -1;
        for (let i = pos; i < data.length - 1; i++) {
          if (data[i] === 13 && data[i + 1] === 10) { lineEnd = i; break; }
        }
        if (lineEnd < 0) break;
        const sizeLine = dec.decode(data.slice(pos, lineEnd)).split(';')[0].trim();
        if (!/^[0-9a-fA-F]+$/.test(sizeLine)) break;  // reject non-hex
        const chunkSize = parseInt(sizeLine, 16);
        if (isNaN(chunkSize) || chunkSize === 0) break;
        pos = lineEnd + 2;
        if (pos + chunkSize > data.length) break;       // bounds check
        // Validate trailing CRLF if present.
        if (pos + chunkSize + 1 < data.length &&
            (data[pos + chunkSize] !== 13 || data[pos + chunkSize + 1] !== 10)) break;
        chunks.push(data.slice(pos, pos + chunkSize));
        pos += chunkSize + 2; // skip trailing CRLF after chunk data
      }
      const total = chunks.reduce((s, c) => s + c.length, 0);
      const out = new Uint8Array(total);
      let off = 0;
      for (const c of chunks) { out.set(c, off); off += c.length; }
      return out;
    },

    // Parse a raw HTTP/1.1 response byte array.
    // Returns { status, headersText, body } or null on parse error.
    _parseResponse(raw) {
      let sepIdx = -1;
      for (let i = 0; i < raw.length - 3; i++) {
        if (raw[i]===13 && raw[i+1]===10 && raw[i+2]===13 && raw[i+3]===10) {
          sepIdx = i; break;
        }
      }
      if (sepIdx < 0) return null;

      const dec = new TextDecoder('utf-8', { fatal: false });
      const headerText = dec.decode(raw.slice(0, sepIdx));
      let body = raw.slice(sepIdx + 4);

      const lines = headerText.split('\r\n');
      const m = lines[0].match(/^HTTP\/\S+\s+(\d+)(?:\s+(.*))?$/);
      if (!m) return null;
      const status = parseInt(m[1]);

      // Collect headers (skip status line).
      const headers = [];
      for (let i = 1; i < lines.length; i++) {
        const ci = lines[i].indexOf(':');
        if (ci < 0) continue;
        headers.push([lines[i].slice(0, ci).trim(), lines[i].slice(ci + 1).trim()]);
      }

      // Decode chunked body if needed.
      const te = headers.find(([k]) => k.toLowerCase() === 'transfer-encoding');
      if (te && te[1].toLowerCase().includes('chunked'))
        body = this._decodeChunked(body);

      // Build flat "Name: Value\r\n" headers string.
      const headersText = headers.map(([k, v]) => `${k}: ${v}`).join('\r\n');
      return { status, headersText, body };
    },

    // Perform a full HTTP/1.1 exchange over an existing jsConn.
    // Uses jsConn.read/write directly (no Asyncify wrapper needed here — we are
    // already inside the Asyncify.handleAsync Promise in ladybird_http_request).
    async _doHTTP(jsConn, host, method, path, requestHeaders, bodyBytes) {
      const safeMethod = this._sanitizeToken(method);
      const safePath = path.replace(/[\r\n]+/g, '');
      const safeHost = this._sanitizeToken(host);
      let req = `${safeMethod} ${safePath} HTTP/1.1\r\nHost: ${safeHost}\r\nConnection: close\r\n`;
      for (const [k, v] of requestHeaders) {
        const sk = this._sanitizeToken(k);
        const kl = sk.toLowerCase();
        // Filter headers that we control: host, connection, content-length, transfer-encoding.
        if (kl !== 'host' && kl !== 'connection'
            && kl !== 'content-length' && kl !== 'transfer-encoding')
          req += `${sk}: ${this._sanitizeHeaderValue(v)}\r\n`;
      }
      if (bodyBytes && bodyBytes.length > 0)
        req += `Content-Length: ${bodyBytes.length}\r\n`;
      req += '\r\n';

      const enc = new TextEncoder();
      const reqBytes = enc.encode(req);
      let toSend = reqBytes;
      if (bodyBytes && bodyBytes.length > 0) {
        toSend = new Uint8Array(reqBytes.length + bodyBytes.length);
        toSend.set(reqBytes);
        toSend.set(bodyBytes, reqBytes.length);
      }
      await jsConn.write(toSend);

      // Read until EOF (Connection: close ensures server closes after response).
      // 120s per-read timeout guards against servers that ignore Connection: close.
      // Tor circuits (especially over Snowflake) can have 60-90s first-byte latency.
      const MAX_RESPONSE_BYTES = 50 * 1024 * 1024; // 50 MB
      const READ_TIMEOUT_MS = 120000;
      const chunks = [];
      let total = 0;
      while (true) {
        const result = await this._withTimeout(jsConn.read(65536), READ_TIMEOUT_MS);
        if (!result || !result.data || result.data.length === 0) break;
        total += result.data.length;
        if (total > MAX_RESPONSE_BYTES) {
          console.error('[LadybirdHTTP] response exceeded size limit');
          return null;
        }
        chunks.push(new Uint8Array(result.data));
      }

      // Server closed connection without sending any bytes — typically an
      // HTTPS-only server rejecting a plaintext HTTP connection.  Synthesize a
      // 503 response so LibWeb's Fetch machinery receives a valid HTTP response
      // (status > 0) and renders an error page rather than hitting the
      // on_complete(false,...) error path which can crash via a null vtable call.
      if (total === 0) {
        const safeDispHost = this._htmlEscape(host);
        const safeDispPath = this._htmlEscape(path);
        const httpsHint = method === 'GET'
          ? ` Try <a href="https://${safeDispHost}${safeDispPath}">https://${safeDispHost}${safeDispPath}</a>.`
          : '';
        const body = `<html><head><title>Connection Error</title></head><body>` +
          `<h1>503 Service Unavailable</h1>` +
          `<p>The server closed the connection without sending a response.` +
          ` This usually means the server only accepts HTTPS connections.${httpsHint}</p>` +
          `</body></html>`;
        const bodyBytes = new TextEncoder().encode(body);
        const header = `HTTP/1.1 503 Service Unavailable\r\n` +
          `Content-Type: text/html; charset=utf-8\r\n` +
          `Content-Length: ${bodyBytes.length}\r\n` +
          `Connection: close\r\n\r\n`;
        const headerBytes = new TextEncoder().encode(header);
        const synthetic = new Uint8Array(headerBytes.length + bodyBytes.length);
        synthetic.set(headerBytes);
        synthetic.set(bodyBytes, headerBytes.length);
        console.warn('[LadybirdHTTP] empty response — synthesizing 503');
        return synthetic;
      }

      const raw = new Uint8Array(total);
      let off = 0;
      for (const ch of chunks) { raw.set(ch, off); off += ch.length; }
      return raw;
    },

    // Top-level fetch.  Called from ladybird_http_request.
    // Stores results in this._status/headersPtr/bodyPtr/bodyLen.
    async fetch(urlStr, method, reqHeaderPairs, bodyBytes) {
      this._reset();
      try {
        const url = new URL(urlStr);
        if (url.protocol !== 'http:' && url.protocol !== 'https:') {
          console.warn('[LadybirdHTTP] Unsupported protocol:', url.protocol);
          this.status = 0;
          return -1;
        }

        const isHttps = url.protocol === 'https:';
        const host = url.hostname;
        const port = parseInt(url.port) || (isHttps ? 443 : 80);
        const path = (url.pathname || '/') + (url.search || '');

        const sockId = await LadybirdTor.connect(host, port);
        if (sockId < 0) { this.status = 0; return -1; }

        const conn = LadybirdTor.conns[sockId];
        let raw;
        let transport;
        try {
          // For HTTPS, perform the TLS handshake via synchronous C exports that use
          // a buffer-BIO approach.  All async I/O is pure JS await on jsConn.read/write
          // — no ccall({async:true}), no nested Asyncify re-entry.
          if (isHttps) {
            const hostnamePtr = stringToNewUTF8(host);
            const initRet = _tor_tls_connect_init(sockId, hostnamePtr);
            _free(hostnamePtr);
            if (initRet !== 0) {
              console.error('[LadybirdHTTP] TLS init failed');
              this.status = 0; return -1;
            }

            // Handshake loop — no ccall({async:true}), no nested Asyncify.
            // If the loop exits with an error, we must clean up the TLS entry
            // (connect_init succeeded, so SSL*/SSL_CTX* are allocated).
            let handshakeOk = false;
            const HANDSHAKE_READ_TIMEOUT_MS = 120000;
            try {
              handshake: while (true) {
                // Always flush any outgoing bytes first (includes ClientHello on first iter).
                await this._flushOutgoing(sockId, conn);

                const step = _tor_tls_connect_step(sockId);
                if (step === 0) { handshakeOk = true; break handshake; }
                if (step === -1) {
                  console.error('[LadybirdHTTP] TLS handshake error');
                  this.status = 0; return -1;
                }
                // step === 1 (WANT_READ) or 2 (WANT_WRITE after flush above).
                // Re-validate conn after any future await (concurrent tor_socket_close guard).
                const freshConn = LadybirdTor.conns[sockId];
                if (!freshConn) { this.status = 0; return -1; }

                const result = await this._withTimeout(
                    freshConn.jsConn.read(16384), HANDSHAKE_READ_TIMEOUT_MS);
                if (!result || !result.data || result.data.length === 0) {
                  console.error('[LadybirdHTTP] Tor connection closed during TLS handshake');
                  this.status = 0; return -1;
                }
                const data = new Uint8Array(result.data);
                const ptr = _malloc(data.length);
                if (!ptr) { this.status = 0; return -1; }
                new Uint8Array(wasmMemory.buffer).set(data, ptr);
                const feedRet = _tor_tls_feed_incoming(sockId, ptr, data.length);
                _free(ptr);
                if (feedRet !== 0) {
                  console.error('[LadybirdHTTP] TLS feed_incoming overflow');
                  this.status = 0; return -1;
                }
              }
            } finally {
              if (!handshakeOk) _tor_tls_close(sockId);
            }

            const self = this;
            transport = {
              read: async function(size) {
                const ptr = _malloc(size);
                if (!ptr) return null;
                try {
                  while (true) {
                    // TLS 1.3 post-handshake messages (e.g. session tickets) can produce
                    // outgoing bytes from SSL_read; flush after every step call.
                    await self._flushOutgoing(sockId, LadybirdTor.conns[sockId]);
                    const n = _tor_tls_read_step(sockId, ptr, size);
                    if (n > 0) {
                      return { data: new Uint8Array(wasmMemory.buffer).slice(ptr, ptr + n) };
                    }
                    if (n === 0) return null;               // clean EOF
                    if (n !== -2) return null;              // fatal error
                    // n === -2: WANT_READ — fetch more from Tor.
                    const freshConn2 = LadybirdTor.conns[sockId];
                    if (!freshConn2) return null;
                    const result = await self._withTimeout(
                        freshConn2.jsConn.read(16384), 120000);
                    if (!result || !result.data || result.data.length === 0) return null;
                    const inData = new Uint8Array(result.data);
                    const inPtr = _malloc(inData.length);
                    if (!inPtr) return null;
                    new Uint8Array(wasmMemory.buffer).set(inData, inPtr);
                    const feedRet = _tor_tls_feed_incoming(sockId, inPtr, inData.length);
                    _free(inPtr);
                    if (feedRet !== 0) return null;
                  }
                } finally {
                  _free(ptr);
                }
              },
              write: async function(bytes) {
                if (!bytes.length) return 0;
                const ptr = _malloc(bytes.length);
                if (!ptr) return -1;
                try {
                  new Uint8Array(wasmMemory.buffer).set(bytes, ptr);
                  const n = _tor_tls_write_step(sockId, ptr, bytes.length);
                  if (n < 0) return -1;
                  // Flush ciphertext OpenSSL wrote into out_buf.
                  await self._flushOutgoing(sockId, LadybirdTor.conns[sockId]);
                  return n;
                } finally {
                  _free(ptr);
                }
              },
              close: function() { _tor_tls_close(sockId); },
            };
          } else {
            transport = conn.jsConn;
          }

          raw = await this._doHTTP(transport, host, method, path, reqHeaderPairs, bodyBytes);
        } finally {
          if (isHttps && transport) {
            try { transport.close(); } catch (_) {}
          }
          LadybirdTor.close(sockId);
        }

        if (!raw) {
          this.status = 0; return -1;
        }

        const parsed = this._parseResponse(raw);
        if (!parsed) {
          console.error('[LadybirdHTTP] failed to parse response');
          this.status = 0; return -1;
        }

        this.status = parsed.status;

        // Copy headers string to WASM heap.
        const headersUtf8 = new TextEncoder().encode(parsed.headersText);
        const hPtr = _malloc(headersUtf8.length + 1);
        if (!hPtr) { this.status = 0; return -1; }
        const heap = new Uint8Array(wasmMemory.buffer);
        heap.set(headersUtf8, hPtr);
        heap[hPtr + headersUtf8.length] = 0;
        this.headersPtr = hPtr;

        // Copy body to WASM heap.
        if (parsed.body.length > 0) {
          const bPtr = _malloc(parsed.body.length);
          if (!bPtr) { this.status = 0; return -1; }
          (new Uint8Array(wasmMemory.buffer)).set(parsed.body, bPtr);
          this.bodyPtr  = bPtr;
          this.bodyLen  = parsed.body.length;
        }

        return 0; // success
      } catch (e) {
        console.error('[LadybirdHTTP] fetch error:', e?.message || e);
        this.status = 0; return -1;
      }
    },
  },

  // ── Exported C functions ───────────────────────────────────────────────────
  // NOTE: ladybird_http_request and ladybird_http_get_* are defined via
  // EM_ASYNC_JS / EM_JS in ResourceLoader.cpp — not here — to avoid duplicate
  // symbol conflicts at Emscripten link time.

  tor_socket_connect__deps: ['$LadybirdTor'],
  tor_socket_connect: function(hostPtr, port) {
    const host = UTF8ToString(hostPtr);
    return Asyncify.handleAsync(() => LadybirdTor.connect(host, port));
  },

  tor_socket_read__deps: ['$LadybirdTor'],
  tor_socket_read: function(socketId, bufferPtr, bufferSize) {
    return Asyncify.handleAsync(() => LadybirdTor.read(socketId, bufferPtr, bufferSize));
  },

  tor_socket_write__deps: ['$LadybirdTor'],
  tor_socket_write: function(socketId, dataPtr, dataSize) {
    return Asyncify.handleAsync(() => LadybirdTor.write(socketId, dataPtr, dataSize));
  },

  tor_socket_close__deps: ['$LadybirdTor'],
  tor_socket_close: function(socketId) {
    LadybirdTor.close(socketId);
  },
});
