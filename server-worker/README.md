# LabTester Server Worker

Минимальная серверная сборка `LabTesterWorker` без GUI/QML. Она рассчитана на Ubuntu 22.04 с `CMake 3.22`, `Qt 6.2`, `g++` и `ninja`.

## Что собирается

Только HTTP worker:

- `GET /health`
- `POST /sync/lab`
- `POST /run`
- `GET /jobs`
- `GET /jobs/<id>`
- `POST /backup/snapshots`
- `GET /backup/snapshots/latest?username=<name>`

GUI-приложение, QML, SQLite/PostgreSQL и установщик Windows сюда не входят.

## Зависимости Ubuntu

На сервере или в WSL нужны пакеты:

```bash
sudo apt update
sudo apt install -y git build-essential cmake ninja-build pkg-config qt6-base-dev curl
```

Для запуска проверок студенческого кода также нужны:

```bash
sudo apt install -y g++ cmake ninja-build
```

## Сборка

Из папки `LabTester`:

```bash
chmod +x server-worker/build-package.sh
./server-worker/build-package.sh
```

Если на хосте нет Qt dev-пакетов, но есть `podman` или `docker`, можно собрать в Ubuntu 22.04 контейнере:

```bash
chmod +x server-worker/build-package-container.sh
./server-worker/build-package-container.sh
```

На выходе появится архив:

```text
LabTester/dist/labtester-worker-linux-amd64.tar.gz
```

## Локальный запуск

```bash
LABTESTER_TOKEN=test-token \
LABTESTER_PORT=20000 \
LABTESTER_MAX_PARALLEL=1 \
./dist/server-worker/run-worker.sh
```

Проверка:

```bash
curl http://127.0.0.1:20000/health
```

Ожидаемый ответ содержит:

```json
{
  "ok": true,
  "service": "LabTesterWorker"
}
```

## Развертывание на выданном сервере

Скопировать архив на сервер:

```bash
scp LabTester/dist/labtester-worker-linux-amd64.tar.gz user@your-server:/home/user/
```

На сервере:

```bash
mkdir -p /home/labtester/labtester-worker
tar -xzf /home/labtester/labtester-worker-linux-amd64.tar.gz -C /home/labtester/labtester-worker
cd /home/labtester/labtester-worker
LABTESTER_TOKEN='replace-with-long-random-token' ./run-worker.sh
```

Проверка снаружи:

```bash
curl http://your-server:20000/health
```

## Backup API

Worker хранит резервные снимки в SQLite:

```text
~/.local/share/LabTesterWorker/LabTesterWorker/backups/backup.sqlite
```

Сохранить снимок:

```bash
curl -X POST http://your-server:20000/backup/snapshots \
  -H "Authorization: Bearer $LABTESTER_TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"username":"default","label":"manual","snapshotJson":"{\"schemaVersion\":1,\"tables\":{}}"}'
```

Получить последний снимок:

```bash
curl "http://your-server:20000/backup/snapshots/latest?username=default" \
  -H "Authorization: Bearer $LABTESTER_TOKEN"
```

## User systemd без sudo

Если администратор разрешит user services:

```bash
mkdir -p ~/.config/systemd/user
nano ~/.config/systemd/user/labtester-worker.service
```

Содержимое:

```ini
[Unit]
Description=LabTester Worker
After=network.target

[Service]
WorkingDirectory=%h/labtester-worker
Environment=LABTESTER_BIND=0.0.0.0
Environment=LABTESTER_PORT=20000
Environment=LABTESTER_MAX_PARALLEL=1
Environment=LABTESTER_TOKEN=replace-with-long-random-token
ExecStart=%h/labtester-worker/run-worker.sh
Restart=always
RestartSec=5

[Install]
WantedBy=default.target
```

Запуск:

```bash
systemctl --user daemon-reload
systemctl --user enable --now labtester-worker
systemctl --user status labtester-worker
```

Чтобы сервис переживал выход пользователя из SSH, администратор должен один раз выполнить:

```bash
sudo loginctl enable-linger labtester
```

Если linger недоступен, для пилота можно запускать worker через `tmux`, `screen` или `nohup`.
