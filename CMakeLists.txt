cmake_minimum_required(VERSION 3.24)

project(LabTester VERSION 0.2.0 LANGUAGES CXX)

if(WIN32)
    enable_language(RC)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

if(MSVC)
    add_compile_options(/utf-8)
endif()

find_package(Qt6 6.8 REQUIRED COMPONENTS
    Core
    Network
    Gui
    Quick
    Qml
    QuickDialogs2
    QuickControls2
    Sql
)

qt_standard_project_setup(REQUIRES 6.8)

set(LABTESTER_WIN_RESOURCES)
if(WIN32)
    list(APPEND LABTESTER_WIN_RESOURCES
        resources/windows/labtester.rc
    )
endif()

qt_add_executable(LabTester
    src/main.cpp

    src/app/ApplicationController.h
    src/app/ApplicationController.cpp
    src/app/TestRunWorker.h
    src/app/TestRunWorker.cpp

    src/domain/ExecutionStatus.h
    src/domain/CheckRunHistoryEntry.h
    src/domain/Student.h
    src/domain/LabWork.h
    src/domain/Submission.h
    src/domain/TestSuite.h
    src/domain/TestCaseResult.h
    src/domain/TestRunResult.h

    src/services/SubmissionService.h
    src/services/SubmissionService.cpp
    src/services/SubmissionImportService.h
    src/services/SubmissionImportService.cpp
    src/services/TestExecutionService.h
    src/services/TestExecutionService.cpp
    src/services/WorkspaceService.h
    src/services/WorkspaceService.cpp
    src/services/ReportService.h
    src/services/ReportService.cpp
    src/services/CloudSyncService.h
    src/services/CloudSyncService.cpp
    src/services/ExecutionSettingsService.h
    src/services/ExecutionSettingsService.cpp

    src/runners/ITestRunner.h
    src/runners/CppGTestRunner.h
    src/runners/CppGTestRunner.cpp
    src/runners/RemoteWorkerRunner.h
    src/runners/RemoteWorkerRunner.cpp

    src/database/DatabaseManager.h
    src/database/DatabaseManager.cpp
    src/database/SubmissionRepository.h
    src/database/SubmissionRepository.cpp
    src/database/ResultRepository.h
    src/database/ResultRepository.cpp

    src/models/SubmissionListModel.h
    src/models/SubmissionListModel.cpp
    src/models/ResultListModel.h
    src/models/ResultListModel.cpp
    src/models/HistoryListModel.h
    src/models/HistoryListModel.cpp

    ${LABTESTER_WIN_RESOURCES}
)

qt_add_executable(LabTesterWorker
    src/worker/main_worker.cpp
    src/worker/WorkerServer.h
    src/worker/WorkerServer.cpp

    src/domain/ExecutionStatus.h
    src/domain/Student.h
    src/domain/LabWork.h
    src/domain/Submission.h
    src/domain/TestCaseResult.h
    src/domain/TestRunResult.h

    src/runners/ITestRunner.h
    src/runners/CppGTestRunner.h
    src/runners/CppGTestRunner.cpp
)

qt_add_qml_module(LabTester
    URI LabTester
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/pages/DashboardPage.qml
        qml/pages/SubmissionsPage.qml
        qml/pages/ResultsPage.qml
        qml/pages/HistoryPage.qml
        qml/pages/SettingsPage.qml
        qml/pages/SyncPage.qml
        qml/components/AppButton.qml
        qml/components/AppCard.qml
        qml/components/AppComboBox.qml
        qml/components/AppTextField.qml
        qml/components/AlgorithmTestGroups.qml
        qml/components/StudentPickerField.qml
        qml/components/AddStudentDialog.qml
        qml/components/LabEditorDialog.qml
        qml/components/AppNavButton.qml
        qml/components/FilePickerField.qml
        qml/components/BatchImportDialog.qml
        qml/components/StatusBadge.qml
        qml/components/ResultCard.qml
)

qt_add_resources(LabTester "labtester_resources"
    PREFIX "/"
    FILES
        db/init.sql
        resources/icons/labtester_icon.svg
        resources/icons/labtester_icon.ico
        resources/icons/nav_students.svg
        resources/icons/nav_checks.svg
        resources/icons/nav_labs.svg
        resources/icons/nav_history.svg
        resources/icons/nav_settings.svg
        resources/icons/eye.svg
        resources/icons/eye_off.svg
        resources/icons/cloud_sync.svg
)

target_include_directories(LabTester PRIVATE src)

target_link_libraries(LabTester PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Gui
    Qt6::Quick
    Qt6::Qml
    Qt6::QuickDialogs2
    Qt6::QuickControls2
    Qt6::Sql
)

target_include_directories(LabTesterWorker PRIVATE src)

target_link_libraries(LabTesterWorker PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Sql
)

if (WIN32)
    if (MSVC)
        set_target_properties(LabTester PROPERTIES WIN32_EXECUTABLE TRUE)
    endif()
    target_link_libraries(LabTester PRIVATE dwmapi)
endif()

install(TARGETS LabTester
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
)

install(TARGETS LabTesterWorker
    RUNTIME DESTINATION bin
)

add_subdirectory(tests)
