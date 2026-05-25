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

## Настройка удаленного воркера

В Settings укажите:

- URL удаленного воркера
- Bearer токен

Значения хранятся локально; в репозитории нет предзаполненных данных.

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
