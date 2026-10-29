cmake_minimum_required(VERSION 3.20)

project(easydesk_signal VERSION 0.1.0 LANGUAGES CXX)

find_package(Boost 1.74 REQUIRED COMPONENTS json)
find_package(OpenSSL REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(Threads REQUIRED)

add_executable(easydesk-signal
    src/main.cpp
    src/config/application_config.cpp
    src/devices/device_repository.cpp
    src/hub/signaling_hub.cpp
    src/net/http_listener.cpp
    src/net/http_session.cpp
    src/net/websocket_session.cpp
    src/protocol/base64url.cpp
    src/protocol/registration.cpp
    src/protocol/registration_json.cpp
    src/security/ed25519.cpp
    src/security/random.cpp
    src/storage/sqlite_database.cpp
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
