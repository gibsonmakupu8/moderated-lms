cmake_minimum_required(VERSION 3.14)
project(ModeratedLMS VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(LMS_BUILD_TESTS "Build the automated tests (downloads GoogleTest if not installed)" ON)
option(LMS_COVERAGE "Instrument for line coverage (GCC/Clang)" OFF)

if(MSVC)
  add_compile_options(/W4 /permissive-)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ---- dependencies: SQLite and libsodium -----------------------------------------------------
find_package(SQLite3 REQUIRED)

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(SODIUM QUIET IMPORTED_TARGET libsodium)
endif()
if(TARGET PkgConfig::SODIUM)
  set(LMS_SODIUM_TARGET PkgConfig::SODIUM)
else()
  find_path(SODIUM_INCLUDE_DIR sodium.h)
  find_library(SODIUM_LIBRARY NAMES sodium libsodium)
  if(NOT SODIUM_INCLUDE_DIR OR NOT SODIUM_LIBRARY)
    message(FATAL_ERROR "libsodium not found. Install it (Ubuntu: sudo apt install libsodium-dev; Windows: vcpkg install libsodium).")
  endif()
  add_library(lms_sodium INTERFACE)
  target_include_directories(lms_sodium INTERFACE ${SODIUM_INCLUDE_DIR})
  target_link_libraries(lms_sodium INTERFACE ${SODIUM_LIBRARY})
  set(LMS_SODIUM_TARGET lms_sodium)
endif()

# ---- core library: domain + services + data-access layer -------------------------------------
add_library(lms_core STATIC
  src/Common.cpp src/Models.cpp src/TimeUtil.cpp src/Validation.cpp
  src/User.cpp src/LatePenalty.cpp src/PasswordHasher.cpp
  src/Schema.cpp src/Database.cpp src/SqliteRepositories.cpp src/ContentStore.cpp
  src/AuthService.cpp src/AdminService.cpp src/CourseworkService.cpp src/AssessmentService.cpp)
target_include_directories(lms_core PUBLIC include)
target_link_libraries(lms_core PUBLIC SQLite::SQLite3 ${LMS_SODIUM_TARGET})
if(LMS_COVERAGE AND NOT MSVC)
  target_compile_options(lms_core PUBLIC --coverage -O0 -g)
  target_link_libraries(lms_core PUBLIC --coverage)
endif()

# ---- presentation layer and executables --------------------------------------------------------
add_library(lms_ui STATIC src/Console.cpp src/App.cpp)
target_link_libraries(lms_ui PUBLIC lms_core)

add_executable(lms src/main.cpp)
target_link_libraries(lms PRIVATE lms_ui)

add_executable(lms_seed src/seed.cpp)  # demo / performance data (800 students)
target_link_libraries(lms_seed PRIVATE lms_core)

# ---- tests (GoogleTest) ------------------------------------------------------------------------
if(LMS_BUILD_TESTS)
  enable_testing()
  find_package(GTest QUIET)
  if(NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(googletest
      URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
  endif()
  add_executable(lms_tests
    tests/test_main.cpp tests/test_domain.cpp tests/test_repositories.cpp tests/test_auth_admin.cpp
    tests/test_coursework.cpp tests/test_assessment.cpp tests/test_performance.cpp)
  target_include_directories(lms_tests PRIVATE tests)
  target_link_libraries(lms_tests PRIVATE lms_core GTest::gtest)
  add_test(NAME lms_tests COMMAND lms_tests)
endif()
