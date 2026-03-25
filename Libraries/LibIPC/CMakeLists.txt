set(SOURCES
    AutoCloseFileDescriptor.cpp
    Connection.cpp
    Decoder.cpp
    Encoder.cpp
    TransportHandle.cpp
)

if (EMSCRIPTEN)
    # AF_UNIX sockets are not available in Emscripten. WasmPageClient
    # implements Web::PageClient directly in-process — no IPC needed.
    # Compile Message.cpp and File.cpp (no socket dependency) for serialization
    # types; StubsEmscripten.cpp provides no-op stubs for TransportSocket.
    list(APPEND SOURCES Attachment.cpp Message.cpp File.cpp StubsEmscripten.cpp)
elseif (UNIX)
    list(APPEND SOURCES
        Attachment.cpp
        File.cpp
        Message.cpp
        TransportSocket.cpp)
else()
    list(APPEND SOURCES
        Attachment.cpp
        FileWindows.cpp
        MessageWindows.cpp
        TransportSocketWindows.cpp)
endif()

ladybird_lib(LibIPC ipc)
target_link_libraries(LibIPC PRIVATE LibCore LibURL LibThreading)
