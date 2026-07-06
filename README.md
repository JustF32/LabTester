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
