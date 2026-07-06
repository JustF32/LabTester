cmake_minimum_required(VERSION 3.16)

project(LabTesterServerWorker VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.2 REQUIRED COMPONENTS Core Network Sql)

set(LABTESTER_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../src")

add_executable(LabTesterWorker
    "${LABTESTER_SOURCE_ROOT}/worker/main_worker.cpp"
    "${LABTESTER_SOURCE_ROOT}/worker/WorkerServer.h"
    "${LABTESTER_SOURCE_ROOT}/worker/WorkerServer.cpp"

    "${LABTESTER_SOURCE_ROOT}/domain/ExecutionStatus.h"
    "${LABTESTER_SOURCE_ROOT}/domain/Student.h"
    "${LABTESTER_SOURCE_ROOT}/domain/LabWork.h"
    "${LABTESTER_SOURCE_ROOT}/domain/Submission.h"
    "${LABTESTER_SOURCE_ROOT}/domain/TestCaseResult.h"
    "${LABTESTER_SOURCE_ROOT}/domain/TestRunResult.h"

    "${LABTESTER_SOURCE_ROOT}/runners/ITestRunner.h"
    "${LABTESTER_SOURCE_ROOT}/runners/CppGTestRunner.h"
    "${LABTESTER_SOURCE_ROOT}/runners/CppGTestRunner.cpp"
)

target_include_directories(LabTesterWorker PRIVATE
    "${LABTESTER_SOURCE_ROOT}"
)

target_link_libraries(LabTesterWorker PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Sql
)

install(TARGETS LabTesterWorker
    RUNTIME DESTINATION bin
)
