cmake_minimum_required(VERSION 3.20)

project(easydesk_signal VERSION 0.1.0 LANGUAGES CXX)

find_package(Boost 1.74 REQUIRED)
find_package(Threads REQUIRED)

add_executable(easydesk-signal
    src/main.cpp
    src/config/application_config.cpp
    src/net/http_listener.cpp
    src/net/http_session.cpp
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
target_link_libraries(easydesk-signal PRIVATE Threads::Threads)

if(MSVC)
    target_compile_options(easydesk-signal PRIVATE /W4 /permissive- /EHsc)
else()
    target_compile_options(easydesk-signal PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
endif()

install(TARGETS easydesk-signal RUNTIME DESTINATION bin)
