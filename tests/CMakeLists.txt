cmake_minimum_required(VERSION 3.14)
project(LabTesterTests)

# Добавляем Google Test
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Флаг для покрытия кода
option(ENABLE_COVERAGE "Enable coverage reporting" OFF)

# Добавляем тестовую директорию
set(TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_database_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_submission_repository.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_result_repository.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_submission_service.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_submission_import_service.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_execution_settings_service.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_report_service.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/test_workspace_service.cpp
)

# Основной тестовый исполняемый файл
add_executable(labtester_tests
    ${TEST_SOURCES}
)

target_link_libraries(labtester_tests
    labtester_core
    GTest::gtest_main
    GTest::gmock_main
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Sql
)

target_include_directories(labtester_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/tests
)

# Включение покрытия кода
if(ENABLE_COVERAGE)
    target_compile_options(labtester_tests PRIVATE --coverage)
    target_link_options(labtester_tests PRIVATE --coverage)
endif()

# Автоматическое обнаружение тестов
include(GoogleTest)
gtest_discover_tests(labtester_tests)

# Интеграционные тесты (отдельная цель)
add_executable(labtester_integration_tests
    ${CMAKE_CURRENT_SOURCE_DIR}/integration/test_database_integration.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/integration/test_sync_integration.cpp
)

target_link_libraries(labtester_integration_tests
    labtester_core
    GTest::gtest_main
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Sql
)

gtest_discover_tests(labtester_integration_tests)

# Добавляем цель для генерации отчета о покрытии
if(ENABLE_COVERAGE)
    find_program(LCOV_PATH lcov REQUIRED)
    find_program(GENHTML_PATH genhtml REQUIRED)
    
    add_custom_target(coverage
        COMMAND ${LCOV_PATH} --capture --directory . --output-file coverage.info
        COMMAND ${LCOV_PATH} --remove coverage.info '/usr/*' '*/gtest/*' '*/tests/*' --output-file coverage_filtered.info
        COMMAND ${GENHTML_PATH} coverage_filtered.info --output-directory coverage_report
        COMMAND echo "Coverage report generated in coverage_report/index.html"
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Generating coverage report"
        DEPENDS labtester_tests labtester_integration_tests
    )
endif()

# Добавляем цель для быстрого запуска тестов
add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS labtester_tests labtester_integration_tests
)