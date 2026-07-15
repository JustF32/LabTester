# LabTester

LabTester — десктопное приложение для проверки и анализа лабораторных работ. В составе: QML-интерфейс, локальная SQLite, опциональный удаленный воркер и синхронизация с сервером.

## Возможности

- учет студентов, лабораторных, работ и результатов
- локальный запуск тестов (C++/GTest) или выполнение через удаленный воркер
- синхронизация работ с сервером
- скрипты сборки Windows-установщика
- примерные лабораторные и test_samples (лаб. 1-4)

## Структура репозитория

- qml/ - QML интерфейс
- src/ - код приложения
- labs/ - шаблоны и тесты лабораторных
- test_samples/ - примеры студенческих работ
- server-worker/ - сборка Linux-воркера
- scripts/ - скрипты сборки
- installer/ - Inno Setup сценарий

## Требования (Windows)

- Qt 6.x (MSVC 2022 64-bit)
- CMake, Ninja, MSVC toolchain
- PostgreSQL runtime (libpq.dll) для QPSQL
- Inno Setup 6 (для установщика)

## Сборка и запуск (Windows)

```powershell
scripts/build_run.ps1 -Configuration Debug
```

Release сборка без запуска:

```powershell
scripts/build_run.ps1 -Configuration Release -Clean -NoRun
```

## Настройка удаленного воркера (Linux)

Если вы хотите запускать тесты на удаленном сервере (изолированно), вам понадобится развернуть `LabTesterWorker`.

1. **Сервер**: Потребуется Linux-машина (например, Ubuntu 22.04). Подробные инструкции по сборке и развертыванию воркера читайте в [server-worker/README.md](server-worker/README.md).
2. **Токен**: При запуске воркера (в скрипте `run-worker.sh` или в `.env` файле) задается переменная `LABTESTER_TOKEN`. Придумайте надежный пароль/токен.

## Настройка клиента (Desktop)

В десктопном приложении не зашито никаких адресов или токенов — всё настраивается через пользовательский интерфейс!

1. Откройте вкладку **Settings** в приложении.
2. В поле **Endpoint** укажите URL вашего воркера, например: `http://81.161.118.227:20000`
3. В поле **Token** укажите тот самый `LABTESTER_TOKEN`, который вы задали на сервере.
4. Значения сохраняются локально на вашем компьютере.

## Синхронизация (опционально)

Endpoint и токен берутся из Settings. Для истории синхронизации через облачную БД нужны переменные окружения:

- LABTESTER_CLOUD_HOST
- LABTESTER_CLOUD_PORT
- LABTESTER_CLOUD_DB
- LABTESTER_CLOUD_USER
- LABTESTER_CLOUD_PASSWORD

## Server worker (Linux)

См. server-worker/README.md для сборки и развертывания.

## Установщик

```powershell
scripts/build_installer.ps1 -Configuration Release -AppVersion 0.2v
```

Готовый инсталлятор появится в dist/.

## Документация по тестам
Текущие метрики покрытия по модулям:

DatabaseManager: 85%
SubmissionRepository: 85%
ResultRepository: 85%
SubmissionService: 80%
SubmissionImportService: 90%
ExecutionSettingsService: 95%
ReportService: 100%
WorkspaceService: 100%

Общее покрытие кода составляет 76%.

АРХИТЕКТУРА АВТОМАТИЗИРОВАННЫХ ТЕСТОВ

Тестовый фреймворк построен на Google Test (gtest) и Google Mock (gmock) с использованием CTest для обнаружения и запуска тестов. Все тесты являются модульными и проверяют отдельные компоненты изолированно. Зависимости заменяются моками, что обеспечивает быстрое выполнение и детерминированность результатов.

Структура тестов:

tests/

unit/

test_common.h - общие утилиты и фикстуры

test_database_manager.cpp - тесты DatabaseManager

test_submission_repository.cpp - тесты SubmissionRepository

test_result_repository.cpp - тесты ResultRepository

test_submission_service.cpp - тесты SubmissionService

test_submission_import_service.cpp - тесты SubmissionImportService

test_execution_settings_service.cpp - тесты ExecutionSettingsService

test_report_service.cpp - тесты ReportService

test_workspace_service.cpp - тесты WorkspaceService

Ключевые фикстуры в test_common.h:

TestDatabase - создает in-memory SQLite базу данных для изолированного тестирования без операций ввода-вывода на диск. Все тесты используют эту фикстуру для работы с базой данных.

createTestStudent(id, name, group) - создает тестового студента с указанными параметрами.

createTestLabWork(id, title) - создает тестовую лабораторную работу.

createTestSubmission(id, studentId, labId) - создает тестовую работу студента.

createTestResult(submissionId, passed) - создает тестовый результат выполнения с предопределенными значениями.

Стратегия мокирования:

Репозитории мокаются для симуляции операций с базой данных без реального обращения к диску. Для этого используется TestDatabase с in-memory SQLite.

Файловая система тестируется с использованием QTemporaryDir, который создает временные директории, автоматически удаляемые после завершения теста.

Внешние зависимости изолируются для обеспечения надежности и воспроизводимости тестов.

ЛОКАЛЬНЫЙ ЗАПУСК ТЕСТОВ

Требования:
CMake 3.24 или выше
Qt6 6.8.0 или выше
MSVC 2022 (Windows) или GCC 11+ (Linux)
Google Test (автоматически загружается при конфигурации CMake)

Сборка и запуск тестов в Windows (PowerShell):

cmake -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:/Qt/6.8.0/msvc2022_64" -DBUILD_TESTS=ON
cmake --build build --config Debug -j 8
cd build
ctest --output-on-failure -C Debug

Запуск конкретных тестов:

ctest -R "DatabaseManagerTest" -C Debug - запускает только тесты DatabaseManagerTest
ctest -R "SubmissionRepositoryTest" -C Debug - запускает только тесты SubmissionRepositoryTest
ctest -R "SubmissionServiceTest" -C Debug - запускает только тесты SubmissionServiceTest

Запуск конкретного тестового метода:

build/tests/Debug/labtester_tests.exe --gtest_filter=DatabaseManagerTest.InitializeCreatesTables

Сборка и запуск тестов в Linux:

cmake -B build -DCMAKE_PREFIX_PATH=/usr/lib/x86_64-linux-gnu/cmake/Qt6 -DBUILD_TESTS=ON
cmake --build build -j $(nproc)
cd build
ctest --output-on-failure

Запуск тестов с детальным выводом:

ctest --verbose -C Debug

Логи тестов сохраняются в:
build/Testing/Temporary/LastTest.log - полный вывод тестов
build/startup.log - логи запуска приложения

ДОБАВЛЕНИЕ НОВЫХ СЦЕНАРИЕВ

Для добавления нового тестового сценария необходимо выполнить следующие шаги:

Определить, к какому модулю относится тестируемый функционал.

Открыть соответствующий файл тестов в tests/unit/ или создать новый файл с именем test_[имя_модуля].cpp.

Подключить заголовочный файл test_common.h для использования общих фикстур и утилит.

Создать класс теста, унаследованный от ::testing::Test, с методами SetUp и TearDown при необходимости.

Реализовать тестовые методы с использованием макросов TEST_F или TEST.

Пример добавления нового теста в существующий файл:

TEST_F(SubmissionServiceTest, AddStudent_ValidData_ReturnsSuccess) {
int createdId = 0;
bool result = m_service->addStudent("New Student", "Group C", &createdId, nullptr);
EXPECT_TRUE(result);
EXPECT_GT(createdId, 0);
}

Пример создания нового файла тестов:

```#include "test_common.h"
#include "services/NewService.h"

using namespace labtester::services;
using namespace labtester::test;

class NewServiceTest : public ::testing::Test {
protected:
void SetUp() override {
m_testDb = std::make_unique<TestDatabase>();
m_service = std::make_unique<NewService>(*m_testDb);
}

std::unique_ptr<TestDatabase> m_testDb;
std::unique_ptr<NewService> m_service;
};

TEST_F(NewServiceTest, MethodName_ReturnsExpectedResult) {
auto input = "test";
auto result = m_service->methodName(input);
EXPECT_EQ(result, "expected");
}
```
Соглашения по именованию:

Набор тестов: [ИмяКласса]Test (например, DatabaseManagerTest)
Тестовый метод: [ИмяМетода][Сценарий][ОжидаемыйРезультат] (например, AddStudent_ValidData_ReturnsSuccess)
Имя файла: test_[имя_модуля].cpp (например, test_database_manager.cpp)

Лучшие практики при добавлении тестов:

Используйте структуру Arrange-Act-Assert для каждого теста.
Один тест должен проверять одно поведение.
Имена тестов должны описывать что тестируется.
Каждый тест должен быть независимым от других.
Используйте фикстуры для настройки и очистки тестового окружения.
Модульные тесты должны выполняться за миллисекунды.
Для работы с файлами используйте QTemporaryDir.
Для работы с базой данных используйте TestDatabase.

Новые тесты автоматически обнаруживаются системой сборки при размещении в директории tests/unit/ с суффиксом .cpp в имени файла.
