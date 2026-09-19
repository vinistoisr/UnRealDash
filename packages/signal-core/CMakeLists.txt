cmake_minimum_required(VERSION 3.25)
project(signal_core LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(MSVC)
    string(REPLACE "/EHsc" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(REPLACE "/GR" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(REPLACE "/W3" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    add_compile_options(/EHs-c- /GR- /W4 /WX /permissive-)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS _HAS_EXCEPTIONS=0)
else()
    add_compile_options(-fno-exceptions -fno-rtti -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror -ffp-contract=off)
endif()
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "signal_core: C++20, exceptions OFF, RTTI OFF, warnings as errors")
set(signal_sources
    "${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/UnRealDash/Source/SignalCore/Private/Acquisition.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/UnRealDash/Source/SignalCore/Private/Connector.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/UnRealDash/Source/SignalCore/Private/MemoryConnector.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/UnRealDash/Source/SignalCore/Private/BinaryTelemetryV1.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/UnRealDash/Source/SignalCore/Private/DefinitionPack.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/UnRealDash/Source/SignalCore/Private/Units.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/UnRealDash/Source/SignalCore/Private/SignalRegistry.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/UnRealDash/Source/SignalCore/Private/SnapshotExchange.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/UnRealDash/Source/SignalCore/Private/ExpirySchedule.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/UnRealDash/Source/SignalCore/Private/RuleEngine.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/UnRealDash/Source/SignalCore/Private/Interpolator.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/UnRealDash/Source/SignalCore/Private/Recording.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/UnRealDash/Source/SignalCore/Private/JsonLinesText.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/UnRealDash/Source/SignalCore/Private/Scenarios.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/UnRealDash/Source/SignalCore/Private/Errors.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/UnRealDash/Source/SignalCore/Private/Reconnect.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/UnRealDash/Source/SignalCore/Private/BinaryTelemetryConnector.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/UnRealDash/Source/SignalCore/Private/TcpTransport.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/UnRealDash/Source/SignalCore/Private/FileTransport.cpp"
)
file(GLOB disk_sources "${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/UnRealDash/Source/SignalCore/Private/*.cpp")
foreach(source IN LISTS signal_sources)
    cmake_path(NORMAL_PATH source)
    list(APPEND normalized_sources "${source}")
endforeach()
set(raw_disk_sources ${disk_sources})
set(disk_sources)
foreach(source IN LISTS raw_disk_sources)
    cmake_path(NORMAL_PATH source)
    list(APPEND disk_sources "${source}")
endforeach()
foreach(source IN LISTS disk_sources)
    if(NOT source IN_LIST normalized_sources)
        message(FATAL_ERROR "Unlisted SignalCore source: ${source}")
    endif()
endforeach()
foreach(source IN LISTS normalized_sources)
    if(NOT source IN_LIST disk_sources)
        message(FATAL_ERROR "Missing SignalCore source: ${source}")
    endif()
endforeach()
add_library(signal_core STATIC ${signal_sources})
target_include_directories(signal_core PUBLIC ../../runtime/UnRealDash/Source/SignalCore/Public PRIVATE ../../runtime/UnRealDash/Source/SignalCore/Private)
# SYSTEM so the vendored header is exempt from this project's warnings-as-errors settings.
target_include_directories(signal_core SYSTEM PRIVATE ../../third_party/fast_float)
option(SIGNAL_CORE_BUILD_TOOLS_AND_TESTS "Build standalone signal-core tools and tests" ${PROJECT_IS_TOP_LEVEL})
if(SIGNAL_CORE_BUILD_TOOLS_AND_TESTS)
    find_package(Threads REQUIRED)
    file(GLOB test_sources CONFIGURE_DEPENDS tests/*.cpp)
    add_executable(signal-core-tests ${test_sources})
    target_include_directories(signal-core-tests SYSTEM PRIVATE ../../third_party/doctest)
    target_compile_definitions(signal-core-tests PRIVATE DOCTEST_CONFIG_NO_EXCEPTIONS DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS)
    target_link_libraries(signal-core-tests PRIVATE signal_core Threads::Threads)
    add_executable(signal-core-scenario tools/scenario-dump.cpp)
    target_link_libraries(signal-core-scenario PRIVATE signal_core)
    add_executable(signal-core-smoke-subset tools/smoke-subset.cpp)
    target_link_libraries(signal-core-smoke-subset PRIVATE signal_core)
    enable_testing()
    add_test(NAME smoke-subset COMMAND signal-core-smoke-subset)
    add_executable(signal-core-telemetry-fuzz tools/telemetry-fuzz.cpp)
    target_link_libraries(signal-core-telemetry-fuzz PRIVATE signal_core)
    foreach(source IN LISTS test_sources)
        get_filename_component(name "${source}" NAME)
        if(NOT name STREQUAL "main.cpp")
            add_test(NAME "${name}" COMMAND signal-core-tests "--source-file=*${name}")
            if(name STREQUAL "test_acquisition.cpp" OR name STREQUAL "test_threading_stress.cpp")
                set_tests_properties("${name}" PROPERTIES TIMEOUT 60)
            endif()
        endif()
    endforeach()
endif()
