set(SOURCES
    AutoCloseFileDescriptor.cpp
    Connection.cpp
    Decoder.cpp
    Encoder.cpp
    File.cpp
    Message.cpp
    ReceivedMessageBytes.cpp
    StartupTransport.cpp
    TransportHandle.cpp
)

if (EMSCRIPTEN)
    # AF_UNIX sockets are not available in Emscripten. WasmPageClient
    # implements Web::PageClient directly in-process — no IPC needed.
    # Compile Message.cpp and File.cpp (no socket dependency) for serialization
    # types; StubsEmscripten.cpp provides no-op stubs for TransportSocket.
    list(APPEND SOURCES Attachment.cpp Message.cpp File.cpp StubsEmscripten.cpp)
elseif (APPLE)
    list(APPEND SOURCES
        AttachmentMachPort.cpp
        TransportMachPort.cpp)
    if (NOT IOS)
        list(APPEND SOURCES
            MachBootstrapListener.cpp
            TransportBootstrapMach.cpp)
    endif()
elseif (UNIX)
    list(APPEND SOURCES
        Attachment.cpp
        TransportSocket.cpp)
else()
    list(APPEND SOURCES
        Attachment.cpp
        HandleBroker.cpp
        HandleBrokerServer.cpp
        TransportWindows.cpp)
endif()

ladybird_lib(LibIPC ipc)
target_link_libraries(LibIPC PRIVATE LibCore LibThreading LibURL)
