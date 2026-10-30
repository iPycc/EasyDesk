cmake_minimum_required(VERSION 3.20)

project(easydesk_signal VERSION 0.1.0 LANGUAGES CXX)

find_package(Boost 1.74 REQUIRED COMPONENTS json)
find_package(OpenSSL REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(Threads REQUIRED)

add_executable(easydesk-signal
    src/application/main.cpp
    src/application/config/application_config.cpp
    src/application/hub/signaling_hub.cpp
    src/application/session/assist_code_registry.cpp
    src/application/session/session_attempt_limiter.cpp
    src/infrastructure/devices/device_repository.cpp
    src/infrastructure/storage/sqlite_database.cpp
    src/protocol/cbor/deterministic_cbor.cpp
    src/protocol/encoding/base64url.cpp
    src/protocol/registration/registration.cpp
    src/protocol/registration/registration_json.cpp
    src/protocol/session/session_accept.cpp
    src/protocol/session/session_capability.cpp
    src/protocol/session/session_envelope.cpp
    src/protocol/session/session_envelope_json.cpp
    src/protocol/session/session_payload.cpp
    src/protocol/session/session_request.cpp
    src/protocol/session/session_verifier.cpp
    src/protocol/signaling/signed_envelope.cpp
    src/protocol/signaling/signed_envelope_json.cpp
    src/security/crypto/ed25519.cpp
    src/security/crypto/random.cpp
    src/transport/net/http_listener.cpp
    src/transport/net/http_session.cpp
    src/transport/net/websocket_session.cpp
)

target_compile_features(easydesk-signal PRIVATE cxx_std_20)
target_compile_definitions(easydesk-signal PRIVATE
    BOOST_ERROR_CODE_HEADER_ONLY
    BOOST_SYSTEM_NO_DEPRECATED
)
target_include_directories(easydesk-signal PRIVATE
    ${Boost_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(easydesk-signal PRIVATE
    Boost::json
    OpenSSL::Crypto
    SQLite::SQLite3
    Threads::Threads
)

if(MSVC)
    target_compile_options(easydesk-signal PRIVATE /W4 /permissive- /EHsc)
else()
    target_compile_options(easydesk-signal PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
endif()

install(TARGETS easydesk-signal RUNTIME DESTINATION bin)
