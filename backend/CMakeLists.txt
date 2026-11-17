cmake_minimum_required(VERSION 3.16)
project(NEXA VERSION 1.0.0 LANGUAGES CXX)

# ----------------------------------------------------------------------------
# Targets
#   nexa_engine   static library: data structures, algorithms, models,
#                 services (no Drogon dependency -> unit-testable anywhere)
#   nexa_server   Drogon HTTP/WebSocket server + MySQL repositories
#   nexa_tests    GoogleTest suite (falls back to a bundled shim if GTest
#                 is not installed, so `ctest` always works)
#   export_seed_sql  tool: writes database/seed/seed_demo.sql from the seeder
# ----------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()

option(NEXA_BUILD_SERVER "Build the Drogon server (requires Drogon + MySQL client)" ON)
option(NEXA_BUILD_TESTS  "Build unit/integration tests" ON)

if(MSVC)
    add_compile_options(/W4 /utf-8 /bigobj /permissive- /Zc:__cplusplus)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX)
else()
    add_compile_options(-Wall -Wextra)
endif()

find_package(Threads REQUIRED)

# ---------------------------------------------------------------- engine
set(NEXA_ENGINE_SOURCES
    src/services/PriorityEngine.cpp
    src/services/SpatialService.cpp
    src/services/NetworkService.cpp
    src/services/IncidentService.cpp
    src/services/ResourceService.cpp
    src/services/RankingService.cpp
    src/services/AllocationService.cpp
    src/services/RequestService.cpp
    src/services/HospitalService.cpp
    src/services/AnalyticsService.cpp
    src/services/AuthService.cpp
    src/services/SimulationService.cpp
    src/services/DsaLabService.cpp
    src/services/Bootstrap.cpp
    src/services/DemoSeeder.cpp
)
add_library(nexa_engine STATIC ${NEXA_ENGINE_SOURCES})
target_include_directories(nexa_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(nexa_engine PUBLIC Threads::Threads)

# ---------------------------------------------------------------- server
if(NEXA_BUILD_SERVER)
    find_package(Drogon CONFIG REQUIRED)
    add_executable(nexa_server
        src/main.cpp
        src/controllers/ApiSupport.cpp
        src/controllers/JsonMappers.cpp
        src/controllers/SystemControllers.cpp
        src/controllers/OperationsControllers.cpp
        src/controllers/GeoControllers.cpp
        src/controllers/InsightControllers.cpp
        src/repositories/MySqlRepositories.cpp
    )
    target_link_libraries(nexa_server PRIVATE nexa_engine Drogon::Drogon)
    # copy a starter config next to the binary (never overwrites an edited one)
    if(NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/config.json)
        configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.example.json ${CMAKE_CURRENT_BINARY_DIR}/config.json COPYONLY)
    endif()
endif()

# ---------------------------------------------------------------- tools
add_executable(export_seed_sql tools/export_seed_sql.cpp)
target_link_libraries(export_seed_sql PRIVATE nexa_engine)

# ---------------------------------------------------------------- tests
if(NEXA_BUILD_TESTS)
    enable_testing()
    set(NEXA_TEST_SOURCES
        tests/test_linear_structures.cpp
        tests/test_trees_hash_heap.cpp
        tests/test_algorithms.cpp
        tests/test_spatial_text_range.cpp
        tests/test_workflow.cpp
    )
    find_package(GTest QUIET)
    if(GTest_FOUND)
        add_executable(nexa_tests ${NEXA_TEST_SOURCES})
        target_compile_definitions(nexa_tests PRIVATE NEXA_USE_REAL_GTEST)
        target_link_libraries(nexa_tests PRIVATE nexa_engine GTest::gtest GTest::gtest_main)
        message(STATUS "NEXA tests: using GoogleTest")
    else()
        add_executable(nexa_tests ${NEXA_TEST_SOURCES} tests/support/test_main.cpp)
        target_link_libraries(nexa_tests PRIVATE nexa_engine)
        message(STATUS "NEXA tests: GoogleTest not found, using bundled shim (tests/support/mini_gtest.h)")
    endif()
    target_include_directories(nexa_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    add_test(NAME nexa_tests COMMAND nexa_tests)
endif()
