/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

// TorTLSSocket.cpp — OpenSSL TLS layer for WASM Tor sockets.
//
// This file is compiled only on Emscripten (AK_OS_EMSCRIPTEN) and provides
// synchronous C exports that the JS layer calls to perform TLS over an
// existing Tor socket id produced by tor_socket_connect().
//
// Design: buffer-BIO approach (no Asyncify re-entry from C).
//
//   int  tor_tls_connect_init(int sock_id, char const* hostname)
//     Sets up SSL_CTX, SSL, SNI, hostname verification, and a buffer BIO.
//     Does NOT start the handshake.  Returns 0 on success, -1 on failure.
//
//   int  tor_tls_connect_step(int sock_id)
//     Calls SSL_connect() once.  Returns:
//       0  = handshake complete
//       1  = SSL_ERROR_WANT_READ  (JS must fill in_buf from Tor, then call again)
//       2  = SSL_ERROR_WANT_WRITE (JS must flush out_buf to Tor, then call again)
//      -1  = fatal error
//
//   int  tor_tls_feed_incoming(int sock_id, uint8_t const* data, int len)
//     Appends bytes to in_buf (max 64 KB guard, with compaction).
//     Returns 0 on success, -1 on overflow or error.
//
//   int      tor_tls_get_outgoing_len(int sock_id)
//   uint8_t* tor_tls_get_outgoing_ptr(int sock_id)
//   void     tor_tls_clear_outgoing(int sock_id)
//
//   int  tor_tls_read_step(int sock_id, uint8_t* buf, int len)
//     SSL_read() against current in_buf.  Returns n>0, 0 (EOF), -2 (WANT_READ),
//     or -1 (fatal).
//
//   int  tor_tls_write_step(int sock_id, uint8_t const* buf, int len)
//     SSL_write(); ciphertext goes into out_buf.  Returns n>0 or -1.
//
//   void tor_tls_close(int sock_id)
//
// Custom BIO:
//   A BIO_METHOD table ("tor-socket") is registered once via bio_method().
//   bio_read_fn / bio_write_fn operate on the in_buf / out_buf byte queues in
//   TorTLSEntry.  No Asyncify imports are called from C.
//
// CA bundle:
//   Certificates are loaded from /res/cacert.pem, which must be placed at
//   Base/res/cacert.pem in the Ladybird source tree so that the Emscripten
//   --preload-file ${LADYBIRD_SOURCE_DIR}/Base/res@/res directive makes it
//   available in the Emscripten virtual filesystem at runtime.
//
//   Obtain the bundle from https://curl.se/ca/cacert.pem (the Mozilla CA
//   bundle extracted by the curl project) and copy it to Base/res/cacert.pem.

#ifdef __EMSCRIPTEN__

#    include <AK/Assertions.h>
#    include <AK/Format.h>
#    include <AK/HashMap.h>
#    include <AK/Vector.h>

#    include <emscripten.h>

#    include <openssl/bio.h>
#    include <openssl/err.h>
#    include <openssl/ssl.h>

// ── Buffer size limits ──────────────────────────────────────────────────────
static constexpr size_t MAX_IN_BUF = 65536;    // 64 KB — incoming from Tor
static constexpr size_t MAX_OUT_BUF = 262144;  // 256 KB — outgoing to Tor

// ── Per-connection TLS state ──────────────────────────────────────────────────

struct TorTLSEntry {
    SSL_CTX* ssl_ctx { nullptr };
    SSL*     ssl     { nullptr };
    AK::Vector<u8> in_buf;        // bytes from Tor waiting to be read by OpenSSL
    size_t          in_pos { 0 }; // consumed offset into in_buf
    AK::Vector<u8> out_buf;       // bytes OpenSSL produced, waiting to be sent to Tor
};

// Map from Ladybird sock_id (the integer returned by tor_socket_connect) to
// the associated TLS state.
static AK::HashMap<int, TorTLSEntry> s_tls_connections;

// ── Error printing helper ─────────────────────────────────────────────────────

static int print_openssl_errors_cb(char const* str, size_t len, void*)
{
    dbgln("[TorTLS] OpenSSL: {}", AK::StringView { str, len > 0 ? len - 1 : 0 });
    return 1;
}

// ── Custom BIO method table ───────────────────────────────────────────────────
//
// The BIO stores the sock_id as its "fd" value (via BIO_get_fd / BIO_set_fd)
// so that the read/write callbacks can route I/O to the correct TorTLSEntry
// without an additional allocation.
//
// bio_read_fn  — returns bytes from in_buf; signals WANT_READ when empty.
// bio_write_fn — appends bytes to out_buf (capped at MAX_OUT_BUF).
// No Asyncify imports are called from C.

static int bio_read_fn(BIO* bio, char* buf, int len)
{
    if (!buf || len <= 0)
        return 0;
    int sock_id = BIO_get_fd(bio, nullptr);
    auto it = s_tls_connections.find(sock_id);
    if (it == s_tls_connections.end()) {
        BIO_clear_retry_flags(bio);
        return -1;
    }
    auto& e = (*it).value;
    VERIFY(e.in_pos <= e.in_buf.size());
    size_t avail = e.in_buf.size() - e.in_pos;
    if (avail == 0) {
        // No data yet — signal WANT_READ; JS will fill in_buf and retry.
        BIO_set_retry_read(bio);
        return -1;
    }
    int n = (int)min((size_t)len, avail);
    memcpy(buf, e.in_buf.data() + e.in_pos, (size_t)n);
    e.in_pos += (size_t)n;
    if (e.in_pos >= e.in_buf.size()) {
        e.in_buf.clear_with_capacity(); // preserve allocation for next feed
        e.in_pos = 0;
    }
    return n;
}

static int bio_write_fn(BIO* bio, char const* buf, int len)
{
    if (!buf || len <= 0)
        return 0;
    int sock_id = BIO_get_fd(bio, nullptr);
    auto it = s_tls_connections.find(sock_id);
    if (it == s_tls_connections.end()) {
        BIO_clear_retry_flags(bio);
        return -1;
    }
    auto& e = (*it).value;
    if (e.out_buf.size() + (size_t)len > MAX_OUT_BUF) {
        dbgln("[TorTLS] bio_write_fn: out_buf overflow for sock_id {}", sock_id);
        BIO_clear_retry_flags(bio);
        return -1;
    }
    e.out_buf.append(reinterpret_cast<u8 const*>(buf), (size_t)len);
    return len;
}

static long bio_ctrl_fn(BIO* bio, int cmd, long /*num*/, void* ptr)
{
    switch (cmd) {
    case BIO_CTRL_FLUSH:
        return 1;
    case BIO_C_SET_FD:
        if (!ptr)
            return -1;
        BIO_set_fd(bio, *static_cast<int*>(ptr), BIO_NOCLOSE);
        BIO_set_init(bio, 1);
        return 1;
    case BIO_C_GET_FD:
        if (ptr)
            *static_cast<int*>(ptr) = BIO_get_fd(bio, nullptr);
        return BIO_get_fd(bio, nullptr);
    case BIO_CTRL_GET_CLOSE:
        return BIO_NOCLOSE;
    case BIO_CTRL_SET_CLOSE:
        return 1;
    default:
        return 0;
    }
}

static int bio_create_fn(BIO* bio)
{
    BIO_set_init(bio, 0);
    BIO_set_data(bio, nullptr);
    return 1;
}

static int bio_destroy_fn(BIO* bio)
{
    // We do not own the underlying Tor socket — tor_socket_close handles that.
    BIO_set_init(bio, 0);
    (void)bio;
    return 1;
}

// Return (and lazily initialise) the singleton BIO_METHOD* for Tor sockets.
static BIO_METHOD* bio_method()
{
    static BIO_METHOD* s_method = nullptr;
    if (!s_method) {
        int type = BIO_get_new_index() | BIO_TYPE_SOURCE_SINK;
        s_method = BIO_meth_new(type, "tor-socket");
        if (!s_method)
            return nullptr;
        BIO_meth_set_write(s_method, bio_write_fn);
        BIO_meth_set_read(s_method, bio_read_fn);
        BIO_meth_set_ctrl(s_method, bio_ctrl_fn);
        BIO_meth_set_create(s_method, bio_create_fn);
        BIO_meth_set_destroy(s_method, bio_destroy_fn);
    }
    return s_method;
}

// ── Exported C interface ──────────────────────────────────────────────────────

extern "C" {

// Forward declarations required to satisfy -Wmissing-prototypes.
int  tor_tls_connect_init(int sock_id, char const* hostname);
int  tor_tls_connect_step(int sock_id);
int  tor_tls_feed_incoming(int sock_id, uint8_t const* data, int len);
int  tor_tls_get_outgoing_len(int sock_id);
uint8_t* tor_tls_get_outgoing_ptr(int sock_id);
void tor_tls_clear_outgoing(int sock_id);
int  tor_tls_read_step(int sock_id, uint8_t* buf, int len);
int  tor_tls_write_step(int sock_id, uint8_t const* buf, int len);
void tor_tls_close(int sock_id);

// tor_tls_connect_init — create SSL_CTX / SSL, configure SNI + cert verification,
// and attach a buffer BIO.  Does NOT start the handshake.
// Returns 0 on success, -1 on failure.
EMSCRIPTEN_KEEPALIVE
int tor_tls_connect_init(int sock_id, char const* hostname)
{
    if (sock_id < 0 || !hostname) {
        dbgln("[TorTLS] tor_tls_connect_init: invalid arguments");
        return -1;
    }

    // Guard against double-init: clean up any existing entry for this sock_id.
    if (s_tls_connections.contains(sock_id)) {
        dbgln("[TorTLS] tor_tls_connect_init: closing existing TLS entry for sock_id {}", sock_id);
        tor_tls_close(sock_id);
    }

    BIO_METHOD* method = bio_method();
    if (!method) {
        dbgln("[TorTLS] tor_tls_connect_init: failed to allocate BIO method");
        return -1;
    }

    // ── Create SSL_CTX ───────────────────────────────────────────────────────
    SSL_CTX* ssl_ctx = SSL_CTX_new(TLS_client_method());
    if (!ssl_ctx) {
        ERR_print_errors_cb(print_openssl_errors_cb, nullptr);
        dbgln("[TorTLS] tor_tls_connect_init: SSL_CTX_new failed");
        return -1;
    }

    // Abort handshake if peer certificate fails to verify.
    SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_PEER, nullptr);

    // Load the Mozilla CA bundle from the Emscripten virtual filesystem.
    // This file is preloaded from Base/res/cacert.pem via CMakeLists.txt:
    //   --preload-file ${LADYBIRD_SOURCE_DIR}/Base/res@/res
    if (SSL_CTX_load_verify_file(ssl_ctx, "/res/cacert.pem") != 1) {
        ERR_print_errors_cb(print_openssl_errors_cb, nullptr);
        dbgln("[TorTLS] tor_tls_connect_init: SSL_CTX_load_verify_file(/res/cacert.pem) failed");
        SSL_CTX_free(ssl_ctx);
        return -1;
    }

    // Require TLS 1.2 as the minimum protocol version.
    if (SSL_CTX_set_min_proto_version(ssl_ctx, TLS1_2_VERSION) != 1) {
        ERR_print_errors_cb(print_openssl_errors_cb, nullptr);
        dbgln("[TorTLS] tor_tls_connect_init: SSL_CTX_set_min_proto_version failed");
        SSL_CTX_free(ssl_ctx);
        return -1;
    }

    // ── TLS hardening ────────────────────────────────────────────────────────

    // Restrict TLS 1.2 cipher suites to AEAD + forward secrecy only.
    if (SSL_CTX_set_cipher_list(ssl_ctx,
        "ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256:"
        "ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-GCM-SHA384:"
        "ECDHE-ECDSA-CHACHA20-POLY1305:ECDHE-RSA-CHACHA20-POLY1305") != 1) {
        dbgln("[TorTLS] tor_tls_connect_init: SSL_CTX_set_cipher_list failed");
        SSL_CTX_free(ssl_ctx);
        return -1;
    }

    // Restrict TLS 1.3 cipher suites.
    if (SSL_CTX_set_ciphersuites(ssl_ctx,
        "TLS_AES_128_GCM_SHA256:TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256") != 1) {
        dbgln("[TorTLS] tor_tls_connect_init: SSL_CTX_set_ciphersuites failed");
        SSL_CTX_free(ssl_ctx);
        return -1;
    }

    // Prevent session ticket linkability, TLS 1.2 renegotiation, and CRIME.
    SSL_CTX_set_options(ssl_ctx,
        SSL_OP_NO_TICKET
        | SSL_OP_NO_RENEGOTIATION
        | SSL_OP_NO_COMPRESSION);

    // Limit certificate chain depth to prevent DoS via deep chains.
    SSL_CTX_set_verify_depth(ssl_ctx, 5);

    // Require 2048-bit RSA / SHA-256 minimum throughout the chain.
    SSL_CTX_set_security_level(ssl_ctx, 2);

    // Disable session caching (each connection is independent).
    SSL_CTX_set_session_cache_mode(ssl_ctx, SSL_SESS_CACHE_OFF);

    // ── Create SSL object ────────────────────────────────────────────────────
    SSL* ssl = SSL_new(ssl_ctx);
    if (!ssl) {
        ERR_print_errors_cb(print_openssl_errors_cb, nullptr);
        dbgln("[TorTLS] tor_tls_connect_init: SSL_new failed");
        SSL_CTX_free(ssl_ctx);
        return -1;
    }

    // SNI: tell the server which hostname we are connecting to.
    if (SSL_set_tlsext_host_name(ssl, hostname) != 1) {
        ERR_print_errors_cb(print_openssl_errors_cb, nullptr);
        dbgln("[TorTLS] tor_tls_connect_init: SSL_set_tlsext_host_name failed");
        SSL_free(ssl);
        SSL_CTX_free(ssl_ctx);
        return -1;
    }

    // Peer hostname verification: ensure the certificate covers this hostname.
    if (SSL_set1_host(ssl, hostname) != 1) {
        ERR_print_errors_cb(print_openssl_errors_cb, nullptr);
        dbgln("[TorTLS] tor_tls_connect_init: SSL_set1_host failed");
        SSL_free(ssl);
        SSL_CTX_free(ssl_ctx);
        return -1;
    }

    // ── Create and attach custom buffer BIO ──────────────────────────────────
    BIO* bio = BIO_new(method);
    if (!bio) {
        ERR_print_errors_cb(print_openssl_errors_cb, nullptr);
        dbgln("[TorTLS] tor_tls_connect_init: BIO_new failed");
        SSL_free(ssl);
        SSL_CTX_free(ssl_ctx);
        return -1;
    }

    // Store sock_id in the BIO's fd field; bio_ctrl_fn handles BIO_C_SET_FD.
    BIO_ctrl(bio, BIO_C_SET_FD, 0, &sock_id);

    // SSL_set_bio transfers ownership of bio to the SSL object.
    SSL_set_bio(ssl, bio, bio);

    s_tls_connections.set(sock_id, TorTLSEntry { ssl_ctx, ssl, {}, 0, {} });
    return 0;
}

// tor_tls_connect_step — advance the TLS handshake state machine by one call
// to SSL_connect().
// Returns:
//   0  = handshake complete
//   1  = SSL_ERROR_WANT_READ  (JS must fill in_buf from Tor, then call again)
//   2  = SSL_ERROR_WANT_WRITE (JS must flush out_buf to Tor, then call again)
//  -1  = fatal error
EMSCRIPTEN_KEEPALIVE
int tor_tls_connect_step(int sock_id)
{
    auto it = s_tls_connections.find(sock_id);
    if (it == s_tls_connections.end()) {
        dbgln("[TorTLS] tor_tls_connect_step: unknown sock_id {}", sock_id);
        return -1;
    }

    SSL* ssl = (*it).value.ssl;
    int ret = SSL_connect(ssl);
    if (ret == 1)
        return 0; // Handshake complete.

    int err = SSL_get_error(ssl, ret);
    if (err == SSL_ERROR_WANT_READ)
        return 1;
    if (err == SSL_ERROR_WANT_WRITE)
        return 2;

    ERR_print_errors_cb(print_openssl_errors_cb, nullptr);
    dbgln("[TorTLS] tor_tls_connect_step: SSL_connect failed (sock_id={}, err={})", sock_id, err);
    return -1;
}

// tor_tls_feed_incoming — append incoming Tor bytes to in_buf so the BIO can
// serve them to OpenSSL on the next step call.
// Compacts consumed bytes before checking the 64 KB cap.
// Returns 0 on success, -1 on overflow or unknown sock_id.
EMSCRIPTEN_KEEPALIVE
int tor_tls_feed_incoming(int sock_id, uint8_t const* data, int len)
{
    if (!data || len <= 0)
        return 0;
    auto it = s_tls_connections.find(sock_id);
    if (it == s_tls_connections.end()) {
        dbgln("[TorTLS] tor_tls_feed_incoming: unknown sock_id {}", sock_id);
        return -1;
    }
    auto& e = (*it).value;

    // Compact: move unconsumed bytes to front before checking capacity.
    if (e.in_pos > 0) {
        if (e.in_buf.size() > e.in_pos) {
            size_t remaining = e.in_buf.size() - e.in_pos;
            memmove(e.in_buf.data(), e.in_buf.data() + e.in_pos, remaining);
            e.in_buf.resize(remaining);
        } else {
            e.in_buf.clear_with_capacity();
        }
        e.in_pos = 0;
    }

    VERIFY(e.in_pos <= e.in_buf.size());
    size_t pending = e.in_buf.size() - e.in_pos;
    if (pending + (size_t)len > MAX_IN_BUF) {
        dbgln("[TorTLS] tor_tls_feed_incoming: buffer overflow for sock_id {}", sock_id);
        return -1;
    }
    e.in_buf.append(data, (size_t)len);
    return 0;
}

// tor_tls_get_outgoing_len — number of ciphertext bytes OpenSSL has produced
// and that JS must send to Tor.
EMSCRIPTEN_KEEPALIVE
int tor_tls_get_outgoing_len(int sock_id)
{
    auto it = s_tls_connections.find(sock_id);
    if (it == s_tls_connections.end())
        return 0;
    return (int)(*it).value.out_buf.size();
}

// tor_tls_get_outgoing_ptr — pointer into WASM linear memory for out_buf.
// Must be read (and tor_tls_clear_outgoing called) BEFORE any await in JS,
// because a WASM memory growth can invalidate this pointer.
// NOTE: tor_tls_clear_outgoing uses clear() (not clear_with_capacity) which
// preserves the backing allocation — the returned pointer remains valid until
// the next append or the entry is destroyed.
EMSCRIPTEN_KEEPALIVE
uint8_t* tor_tls_get_outgoing_ptr(int sock_id)
{
    auto it = s_tls_connections.find(sock_id);
    if (it == s_tls_connections.end())
        return nullptr;
    return (*it).value.out_buf.data();
}

// tor_tls_clear_outgoing — discard all bytes in out_buf after JS has sent them.
// Uses clear() which resets size to 0 but preserves the backing allocation.
EMSCRIPTEN_KEEPALIVE
void tor_tls_clear_outgoing(int sock_id)
{
    auto it = s_tls_connections.find(sock_id);
    if (it == s_tls_connections.end())
        return;
    (*it).value.out_buf.clear();
}

// tor_tls_read_step — read up to `len` decrypted bytes from the TLS stream.
// Calls SSL_read() synchronously against the current in_buf.
// Returns:
//   n > 0  = bytes read
//   0      = clean TLS EOF
//  -2      = SSL_ERROR_WANT_READ (JS must feed more bytes from Tor)
//  -1      = fatal error
EMSCRIPTEN_KEEPALIVE
int tor_tls_read_step(int sock_id, uint8_t* buf, int len)
{
    auto it = s_tls_connections.find(sock_id);
    if (it == s_tls_connections.end()) {
        dbgln("[TorTLS] tor_tls_read_step: unknown sock_id {}", sock_id);
        return -1;
    }

    SSL* ssl = (*it).value.ssl;
    int ret = SSL_read(ssl, buf, len);
    if (ret > 0)
        return ret;

    switch (SSL_get_error(ssl, ret)) {
    case SSL_ERROR_ZERO_RETURN:
        return 0; // Clean TLS shutdown.
    case SSL_ERROR_WANT_READ:
    case SSL_ERROR_WANT_WRITE:
        return -2; // Transient; JS must feed more data.
    default:
        ERR_print_errors_cb(print_openssl_errors_cb, nullptr);
        dbgln("[TorTLS] tor_tls_read_step: SSL_read failed (sock_id={})", sock_id);
        return -1;
    }
}

// tor_tls_write_step — write `len` application bytes into the TLS stream.
// Ciphertext goes into out_buf; JS must flush it to Tor via tor_tls_get_outgoing_*.
// Returns the number of bytes written (> 0) or -1 on error.
EMSCRIPTEN_KEEPALIVE
int tor_tls_write_step(int sock_id, uint8_t const* buf, int len)
{
    auto it = s_tls_connections.find(sock_id);
    if (it == s_tls_connections.end()) {
        dbgln("[TorTLS] tor_tls_write_step: unknown sock_id {}", sock_id);
        return -1;
    }

    SSL* ssl = (*it).value.ssl;
    int ret = SSL_write(ssl, buf, len);
    if (ret > 0)
        return ret;

    ERR_print_errors_cb(print_openssl_errors_cb, nullptr);
    dbgln("[TorTLS] tor_tls_write_step: SSL_write failed (sock_id={})", sock_id);
    return -1;
}

// tor_tls_close — release all TLS resources for sock_id.
// The underlying raw Tor socket is NOT closed here — the JS
// caller's `finally { LadybirdTor.close(sockId) }` block handles that.
EMSCRIPTEN_KEEPALIVE
void tor_tls_close(int sock_id)
{
    auto it = s_tls_connections.find(sock_id);
    if (it == s_tls_connections.end())
        return;

    TorTLSEntry& entry = (*it).value;

    if (entry.ssl) {
        // Quiet shutdown — skip waiting for peer close_notify.
        // The raw Tor socket is closing immediately after this.
        SSL_set_quiet_shutdown(entry.ssl, 1);
        SSL_shutdown(entry.ssl);
        SSL_free(entry.ssl); // also frees the BIO chain attached to this SSL*
        entry.ssl = nullptr;
    }

    if (entry.ssl_ctx) {
        SSL_CTX_free(entry.ssl_ctx);
        entry.ssl_ctx = nullptr;
    }

    s_tls_connections.remove(it);
}

} // extern "C"

#endif // __EMSCRIPTEN__
