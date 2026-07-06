#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${PROJECT_DIR}/build-server-worker"
DIST_ROOT="${PROJECT_DIR}/dist"
PACKAGE_DIR="${DIST_ROOT}/server-worker"
ARCHIVE_PATH="${DIST_ROOT}/labtester-worker-linux-amd64.tar.gz"

cmake -S "${SCRIPT_DIR}" -B "${BUILD_DIR}" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "${BUILD_DIR}" --target LabTesterWorker

rm -rf "${PACKAGE_DIR}"
mkdir -p "${PACKAGE_DIR}/bin" "${PACKAGE_DIR}/config"

cp "${BUILD_DIR}/LabTesterWorker" "${PACKAGE_DIR}/bin/LabTesterWorker"
cp "${SCRIPT_DIR}/labtester-worker.example.json" "${PACKAGE_DIR}/config/worker.json.example"

cat > "${PACKAGE_DIR}/run-worker.sh" <<'RUNNER'
#!/usr/bin/env bash
set -euo pipefail

APP_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

LABTESTER_BIND="${LABTESTER_BIND:-0.0.0.0}"
LABTESTER_PORT="${LABTESTER_PORT:-20000}"
LABTESTER_MAX_PARALLEL="${LABTESTER_MAX_PARALLEL:-1}"
LABTESTER_TOKEN="${LABTESTER_TOKEN:-change-this-token}"

exec "${APP_DIR}/bin/LabTesterWorker" \
  --bind "${LABTESTER_BIND}" \
  --port "${LABTESTER_PORT}" \
  --token "${LABTESTER_TOKEN}" \
  --max-parallel "${LABTESTER_MAX_PARALLEL}"
RUNNER

chmod +x "${PACKAGE_DIR}/bin/LabTesterWorker" "${PACKAGE_DIR}/run-worker.sh"

if command -v ldd >/dev/null 2>&1; then
    ldd "${PACKAGE_DIR}/bin/LabTesterWorker" > "${PACKAGE_DIR}/ldd.txt" || true
fi

tar -czf "${ARCHIVE_PATH}" -C "${PACKAGE_DIR}" .

echo "Package created: ${ARCHIVE_PATH}"
echo "Run locally:"
echo "  LABTESTER_TOKEN=test-token ${PACKAGE_DIR}/run-worker.sh"
