# Ladybird WebAssembly UI Port

set(LADYBIRD_WASM_SOURCES
    main.cpp
    TorTLSSocket.cpp
    WasmApplication.cpp
    WasmEventLoop.cpp
    WasmExports.cpp
    WasmImageCodecPlugin.cpp
    WasmPageClient.cpp
    WasmWebView.cpp
    SkiaStubs.cpp
)

add_executable(ladybird-wasm ${LADYBIRD_WASM_SOURCES})

# Alias required by UI/CMakeLists.txt framework selector check
add_executable(ladybird ALIAS ladybird-wasm)

# No-op stub expected by test-web and any target that does add_dependencies(... ladybird_build_resource_files)
add_custom_target(ladybird_build_resource_files)

target_include_directories(ladybird-wasm PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${LADYBIRD_SOURCE_DIR}
    ${LADYBIRD_SOURCE_DIR}/Services
)

if(EMSCRIPTEN AND SKIA_PREBUILT_DIR)
    # Get skia source root from the prebuilt dir (e.g., out/wasm-cpu -> skia root -> include/)
    get_filename_component(_skia_out "${SKIA_PREBUILT_DIR}" DIRECTORY)
    get_filename_component(_skia_root "${_skia_out}" DIRECTORY)
    target_include_directories(ladybird-wasm PRIVATE "${_skia_root}" "${_skia_root}/include")

    # The Skia prebuilt was compiled with emcc 3.1.44 whose WebAssemblyLowerEmscriptenEHSjLj
    # pass generates calls to saveSetjmp/testSetjmp in the bitcode.  emcc 4.x no longer
    # provides these symbols.  Provide them by linking the extracted emscripten_setjmp.o
    # from the emcc 3.x compiler-rt (placed next to the Skia prebuilts).
    # Only emscripten_setjmp.o is included — not the whole old libcompiler_rt — to avoid
    # duplicate symbol errors for tempret/getTempRet0/setTempRet0 already in emcc 4.x.
    set(_sjlj_obj "${SKIA_PREBUILT_DIR}/emscripten_setjmp.o")
    if(EXISTS "${_sjlj_obj}")
        target_link_libraries(ladybird-wasm PRIVATE "${_sjlj_obj}")
    endif()
endif()

target_link_libraries(ladybird-wasm PRIVATE
    LibWeb
    LibJS
    LibGfx
    LibCore
    LibIPC
    LibMain
    AK
)

if(EMSCRIPTEN)
    # TorTLSSocket.cpp uses OpenSSL directly; link it here since LibWeb links
    # LibTLS as PRIVATE (non-transitive).
    find_package(OpenSSL REQUIRED)
    target_link_libraries(ladybird-wasm PRIVATE OpenSSL::SSL)
endif()

if(EMSCRIPTEN)
    target_link_options(ladybird-wasm PRIVATE
        # Memory
        -sALLOW_MEMORY_GROWTH=1
        -sINITIAL_MEMORY=134217728      # 128 MB
        -sSTACK_SIZE=5242880            # 5 MB
        -sMAXIMUM_MEMORY=4294967296     # 4 GB

        # Asyncify for blocking Tor socket calls and for spin_until yielding on WASM.
        # emscripten_sleep must be explicit in ASYNCIFY_IMPORTS so the static analyser
        # propagates ASYNCIFY instrumentation to all callers (spin_until, tor_socket_*).
        # wasm_navigate is intentionally NOT in ASYNCIFY_ADD: its synchronous path only
        # queues a deferred_invoke and returns immediately, with no emscripten_sleep call.
        # Adding it to ASYNCIFY_ADD caused ccall({async:true}) to conflict with the main
        # loop's ASYNCIFY rewind state, permanently hanging the tab.
        -sASYNCIFY=1
        -sASYNCIFY_STACK_SIZE=1048576
        "-sASYNCIFY_IMPORTS=['tor_socket_connect','emscripten_sleep','ladybird_http_request']"
        # The TLS step functions are synchronous — called from JS within
        # ladybird_http_request's Asyncify context.  OpenSSL uses setjmp for
        # error recovery; with SUPPORT_LONGJMP=emscripten, setjmp compiles to
        # saveSetjmp (an Asyncify import).  If any function in the TLS call
        # chain is Asyncify-instrumented, saveSetjmp triggers nested unwind/
        # rewind cycles → "Maximum call stack size exceeded".
        # ASYNCIFY_REMOVE with wildcards covers the TLS wrappers + OpenSSL.
        "-sASYNCIFY_REMOVE=['tor_tls_*','SSL_*','ssl_*','ssl3_*','tls_*','dtls_*','BIO_*','bio_*','EVP_*','evp_*','X509*','x509*','PEM_*','pem_*','ASN1_*','asn1_*','OBJ_*','OPENSSL_*','CRYPTO_*','crypto_*','ERR_*','i2d_*','d2i_*','BN_*','EC_*','RSA_*','DH_*','ECDSA_*','ossl_*','lh_*','sk_*','RAND_*','ENGINE_*','print_openssl_*']"

        # FreeType (via Skia) uses setjmp internally; provide Emscripten's SJLJ runtime
        # so that saveSetjmp/testSetjmp are resolved during link.
        -sSUPPORT_LONGJMP=emscripten

        # Exported symbols consumed by ladybird-glue.js
        "-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap','FS','HEAPU8','stringToNewUTF8']"
        "-sEXPORTED_FUNCTIONS=['_malloc','_free','_main','_tor_tls_connect_init','_tor_tls_connect_step','_tor_tls_feed_incoming','_tor_tls_get_outgoing_len','_tor_tls_get_outgoing_ptr','_tor_tls_clear_outgoing','_tor_tls_read_step','_tor_tls_write_step','_tor_tls_close']"

        # Modular output — JS caller: createLadybirdModule({...})
        -sMODULARIZE=1
        -sEXPORT_NAME=createLadybirdModule

        # Web environment; Emscripten provides WebGL via browser APIs
        -sENVIRONMENT=web
        -sMAX_WEBGL_VERSION=2
        -sFORCE_FILESYSTEM=1
        -sASSERTIONS=0
        -sNO_DISABLE_EXCEPTION_CATCHING

        # Tor socket JS library (wires tor_socket_* to window.torBridge)
        --js-library ${CMAKE_CURRENT_SOURCE_DIR}/Resources/tor-bridge.js

        # Pre-JS glue: sets up Ladybird canvas/event wiring before Module init
        --pre-js ${CMAKE_CURRENT_SOURCE_DIR}/Resources/ladybird-glue.js

        # Preload resources (fonts etc.) from Base/res
        --preload-file ${LADYBIRD_SOURCE_DIR}/Base/res@/res
    )

    set_target_properties(ladybird-wasm PROPERTIES
        SUFFIX ".js"
        OUTPUT_NAME "index"
    )

    # Copy build outputs to a staging directory for easy copy to wanix-tor/web/ladybird/
    add_custom_command(TARGET ladybird-wasm POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${LADYBIRD_SOURCE_DIR}/Build/wasm/ladybird
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE_DIR:ladybird-wasm>/index.js
            $<TARGET_FILE_DIR:ladybird-wasm>/index.wasm
            $<TARGET_FILE_DIR:ladybird-wasm>/index.data
            ${LADYBIRD_SOURCE_DIR}/Build/wasm/ladybird/
        COMMENT "Staging WASM artifacts to Build/wasm/ladybird/"
    )
endif()
