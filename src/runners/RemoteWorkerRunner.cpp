#include "runners/RemoteWorkerRunner.h"

#include <QDateTime>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>
#include <QUrl>

#include <algorithm>
#include <atomic>
#include <exception>
#include <vector>

#include "domain/ExecutionStatus.h"
#include "services/ExecutionSettingsService.h"

namespace {

struct HttpResponse {
    bool ok {false};
    int statusCode {0};
    QString error;
    QByteArray rawBody;
    QJsonDocument jsonBody;
};

std::atomic<int> g_remoteRequestId {0};

bool isInterruptionRequested()
{
    QThread *thread = QThread::currentThread();
    return thread && thread->isInterruptionRequested();
}

QString joinUrl(const QString &base, const QString &path)
{
    QString normalizedBase = base.trimmed();
    while (normalizedBase.endsWith('/')) {
        normalizedBase.chop(1);
    }
    if (path.startsWith('/')) {
        return normalizedBase + path;
    }
    return normalizedBase + QLatin1Char('/') + path;
}

QString maskTokenForLog(const QString &token)
{
    const QString normalized = token.trimmed();
    if (normalized.isEmpty()) {
        return QStringLiteral("<empty>");
    }
    if (normalized.size() <= 8) {
        return QStringLiteral("<set:%1>").arg(normalized.size());
    }
    return QStringLiteral("%1...%2(len=%3)")
        .arg(normalized.left(4))
        .arg(normalized.right(2))
        .arg(normalized.size());
}

bool containsTimeoutText(const QString &text)
{
    const QString normalized = text.toLower();
    return normalized.contains(QStringLiteral("превышен лимит времени"))
        || normalized.contains(QStringLiteral("timeout"))
        || normalized.contains(QStringLiteral("time limit"))
        || normalized.contains(QStringLiteral("runtime error"));
}

QString visibleFailureLine(const QString &text)
{
    const QStringList lines = text.split(QRegularExpression(QStringLiteral("[\r\n]+")), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (containsTimeoutText(trimmed)) {
            return trimmed;
        }
    }
    return QString();
}

qint64 durationLimitFromText(const QString &text)
{
    const QRegularExpression re(QStringLiteral("(\\d+)\\s*(?:мс|ms)"), QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = re.match(text);
    if (!match.hasMatch()) {
        return -1;
    }
    bool ok = false;
    const qint64 value = match.captured(1).toLongLong(&ok);
    return ok ? value : -1;
}

QString previewForLog(const QByteArray &rawBody, int maxChars = 420)
{
    QString text = QString::fromUtf8(rawBody);
    if (text.trimmed().isEmpty() && !rawBody.isEmpty()) {
        text = QString::fromLatin1(rawBody);
    }
    text.replace(QStringLiteral("\r"), QStringLiteral(" "));
    text.replace(QStringLiteral("\n"), QStringLiteral(" "));
    if (text.size() > maxChars) {
        text = text.left(maxChars) + QStringLiteral("...");
    }
    return text.trimmed();
}

bool isSessionNotAttachedResponse(int statusCode, const QByteArray &rawBody)
{
    if (statusCode != 503) {
        return false;
    }

    const QString bodyText = QString::fromUtf8(rawBody).toLower();
    return bodyText.contains(QStringLiteral("session not attached"));
}

QString validateHealthResponseAsLabTesterWorker(const HttpResponse &healthResponse)
{
    if (!healthResponse.ok) {
        QString details = healthResponse.error.trimmed();
        if (isSessionNotAttachedResponse(healthResponse.statusCode, healthResponse.rawBody)) {
            details = QStringLiteral(
                "xTunnel вернул 503 Session not attached. Обычно это значит, что туннельный агент не подключен "
                "к этой сессии или отвалился."
            );
        }

        if (!details.isEmpty()) {
            details += QStringLiteral("\n");
        }
        details += previewForLog(healthResponse.rawBody, 700);
        return details.trimmed();
    }

    const QJsonObject body = healthResponse.jsonBody.object();
    const bool ok = body.value(QStringLiteral("ok")).toBool(false);
    const QString service = body.value(QStringLiteral("service")).toString().trimmed();

    if (ok && service.compare(QStringLiteral("LabTesterWorker"), Qt::CaseInsensitive) == 0) {
        return QString();
    }

    // Accept generic health endpoints (for example ASP.NET HealthChecks behind tunnel proxy).
    // In this case we continue and validate worker compatibility on /sync/lab and /run calls.
    const QString genericStatus = body.value(QStringLiteral("status")).toString().trimmed().toLower();
    if (genericStatus == QStringLiteral("healthy") || genericStatus == QStringLiteral("degraded")) {
        qInfo().noquote() << QStringLiteral(
            "[REMOTE][HEALTH] Generic health endpoint detected (status=%1). Continuing with worker API checks."
        ).arg(genericStatus);
        return QString();
    }

    return QStringLiteral(
        "Адрес отвечает, но формат /health не распознан как LabTesterWorker или совместимый generic health. "
        "Проверьте маршрутизацию xTunnel на процесс LabTesterWorker.\n"
        "response=%1"
    ).arg(previewForLog(healthResponse.rawBody, 700));
}

bool allowedLabFile(const QString &filePath)
{
    const QString lower = filePath.toLower();
    return lower.endsWith(QStringLiteral(".cpp"))
        || lower.endsWith(QStringLiteral(".cc"))
        || lower.endsWith(QStringLiteral(".cxx"))
        || lower.endsWith(QStringLiteral(".h"))
        || lower.endsWith(QStringLiteral(".hpp"))
        || lower.endsWith(QStringLiteral(".inl"))
        || lower.endsWith(QStringLiteral(".cmake"))
        || lower.endsWith(QStringLiteral(".txt"))
        || lower.endsWith(QStringLiteral(".md"))
        || lower.endsWith(QStringLiteral("cmakelists.txt"));
}

QString normalizeRelativePath(const QString &relativePath)
{
    QString normalized = QDir::fromNativeSeparators(relativePath.trimmed());
    while (normalized.startsWith(QLatin1Char('/'))) {
        normalized.remove(0, 1);
    }
    return normalized;
}

bool labRootHasRequiredFiles(const QString &labRoot)
{
    const QDir dir(labRoot);
    return dir.exists(QStringLiteral("tests/sort_test.cpp"))
        && dir.exists(QStringLiteral("tests/cc.h"));
}

void appendLabRootCandidatesForId(QStringList *candidates, int labId)
{
    if (!candidates || labId <= 0) {
        return;
    }

    const QString labFolderName = QStringLiteral("lab_%1").arg(labId);
    const QDir currentDir(QDir::currentPath());
    const QDir appDir(QCoreApplication::applicationDirPath());
    const QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

    *candidates
        << appDir.filePath(QStringLiteral("labs/%1").arg(labFolderName))
        << currentDir.filePath(QStringLiteral("labs/%1").arg(labFolderName))
        << appDir.filePath(QStringLiteral("../labs/%1").arg(labFolderName))
        << currentDir.filePath(QStringLiteral("../labs/%1").arg(labFolderName))
        << appDir.filePath(QStringLiteral("../../labs/%1").arg(labFolderName));

    if (!appDataPath.trimmed().isEmpty()) {
        const QDir appDataDir(appDataPath);
        *candidates
            << appDataDir.filePath(QStringLiteral("labs/%1").arg(labFolderName))
            << appDataDir.filePath(QStringLiteral("../labs/%1").arg(labFolderName));
    }
}

QString detectLocalLabRoot(const labtester::domain::Submission &submission)
{
    QStringList candidates;

    appendLabRootCandidatesForId(&candidates, submission.labWork.id);

    if (!submission.labWork.testSuitePath.trimmed().isEmpty()) {
        const QFileInfo suiteInfo(submission.labWork.testSuitePath);
        if (suiteInfo.exists()) {
            QDir testsDir = suiteInfo.absoluteDir();
            if (testsDir.dirName().compare(QStringLiteral("tests"), Qt::CaseInsensitive) == 0) {
                testsDir.cdUp();
                candidates << testsDir.absolutePath();
            } else {
                candidates << suiteInfo.absoluteDir().absolutePath();
            }
        }
    }
    if (!submission.labWork.referenceHeaderPath.trimmed().isEmpty()) {
        const QFileInfo headerInfo(submission.labWork.referenceHeaderPath);
        if (headerInfo.exists()) {
            QDir headerDir = headerInfo.absoluteDir();
            if (headerDir.dirName().compare(QStringLiteral("tests"), Qt::CaseInsensitive) == 0) {
                headerDir.cdUp();
                candidates << headerDir.absolutePath();
            } else {
                candidates << headerInfo.absoluteDir().absolutePath();
            }
        }
    }

    for (const QString &candidate : candidates) {
        const QFileInfo info(candidate);
        if (info.exists() && info.isDir() && labRootHasRequiredFiles(info.absoluteFilePath())) {
            return info.absoluteFilePath();
        }
    }

    return QString();
}

bool buildLabSyncPayload(
    const labtester::domain::Submission &submission,
    QJsonObject *payload,
    QString *errorMessage
)
{
    if (!payload) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Внутренняя ошибка формирования payload синхронизации.");
        }
        return false;
    }

    const QString labRoot = detectLocalLabRoot(submission);
    if (labRoot.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Не найдена папка лабораторной labs/lab_%1.").arg(submission.labWork.id);
        }
        return false;
    }

    struct LabFilePayload {
        QString relativePath;
        QByteArray content;
    };

    const QDir labDir(labRoot);
    std::vector<LabFilePayload> labFiles;
    qint64 totalBytes = 0;
    constexpr qint64 kMaxTotalBytes = 8 * 1024 * 1024;
    constexpr qint64 kMaxSingleFileBytes = 2 * 1024 * 1024;
    QCryptographicHash labSignatureHash(QCryptographicHash::Sha256);

    QDirIterator iterator(labRoot, QDir::Files, QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const QString absPath = iterator.next();
        const QFileInfo info(absPath);
        if (!allowedLabFile(absPath)) {
            continue;
        }
        if (info.size() <= 0 || info.size() > kMaxSingleFileBytes) {
            continue;
        }

        totalBytes += info.size();
        if (totalBytes > kMaxTotalBytes) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Слишком большой объем файлов лабораторной (> %1 MB).")
                    .arg(kMaxTotalBytes / (1024 * 1024));
            }
            return false;
        }

        QFile file(absPath);
        if (!file.open(QIODevice::ReadOnly)) {
            continue;
        }
        const QByteArray content = file.readAll();
        file.close();

        const QString relPath = normalizeRelativePath(labDir.relativeFilePath(absPath));
        if (relPath.isEmpty() || relPath.startsWith(QStringLiteral("../"))) {
            continue;
        }

        labFiles.push_back(LabFilePayload{relPath, content});
    }

    std::sort(labFiles.begin(), labFiles.end(), [](const LabFilePayload &left, const LabFilePayload &right) {
        return left.relativePath < right.relativePath;
    });

    QJsonArray filesArray;
    for (const LabFilePayload &labFile : labFiles) {
        QJsonObject fileObject;
        fileObject.insert(QStringLiteral("relativePath"), labFile.relativePath);
        fileObject.insert(QStringLiteral("contentBase64"), QString::fromLatin1(labFile.content.toBase64()));
        fileObject.insert(QStringLiteral("size"), static_cast<qint64>(labFile.content.size()));
        filesArray.push_back(fileObject);

        labSignatureHash.addData(labFile.relativePath.toUtf8());
        labSignatureHash.addData("\0", 1);
        labSignatureHash.addData(labFile.content);
        labSignatureHash.addData("\0", 1);
    }

    if (filesArray.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("В папке лабораторной нет подходящих файлов для синхронизации.");
        }
        return false;
    }

    payload->insert(QStringLiteral("labId"), submission.labWork.id);
    payload->insert(QStringLiteral("labTitle"), submission.labWork.title);
    payload->insert(QStringLiteral("labRootName"), QFileInfo(labRoot).fileName());
    payload->insert(QStringLiteral("labSignature"), QString::fromLatin1(labSignatureHash.result().toHex()));
    payload->insert(QStringLiteral("files"), filesArray);
    return true;
}

QString resolveAccessibleSourcePath(const labtester::domain::Submission &submission)
{
    const QFileInfo directInfo(submission.sourcePath.trimmed());
    if (directInfo.exists() && directInfo.isFile()) {
        return directInfo.absoluteFilePath();
    }

    const QString sourceFileName = QFileInfo(submission.sourcePath).fileName();
    if (sourceFileName.isEmpty()) {
        return QString();
    }

    const QString labName = QStringLiteral("lab%1").arg(std::max(1, submission.labWork.id));
    const QString labUnderscoreName = QStringLiteral("lab_%1").arg(std::max(1, submission.labWork.id));
    const QDir currentDir(QDir::currentPath());
    const QDir appDir(QCoreApplication::applicationDirPath());
    const QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

    QStringList sourceCandidates {
        currentDir.filePath(QStringLiteral("test_samples/%1/%2").arg(labName, sourceFileName)),
        currentDir.filePath(QStringLiteral("test_samples/%1/%2").arg(labUnderscoreName, sourceFileName)),
        currentDir.filePath(QStringLiteral("../test_samples/%1/%2").arg(labName, sourceFileName)),
        currentDir.filePath(QStringLiteral("../test_samples/%1/%2").arg(labUnderscoreName, sourceFileName)),
        appDir.filePath(QStringLiteral("../test_samples/%1/%2").arg(labName, sourceFileName)),
        appDir.filePath(QStringLiteral("../test_samples/%1/%2").arg(labUnderscoreName, sourceFileName)),
        appDir.filePath(QStringLiteral("../../test_samples/%1/%2").arg(labName, sourceFileName)),
        appDir.filePath(QStringLiteral("../../test_samples/%1/%2").arg(labUnderscoreName, sourceFileName))
    };
    if (!appDataPath.trimmed().isEmpty()) {
        const QDir appDataDir(appDataPath);
        sourceCandidates
            << appDataDir.filePath(QStringLiteral("test_samples/%1/%2").arg(labName, sourceFileName))
            << appDataDir.filePath(QStringLiteral("test_samples/%1/%2").arg(labUnderscoreName, sourceFileName))
            << appDataDir.filePath(QStringLiteral("LabTester/test_samples/%1/%2").arg(labName, sourceFileName))
            << appDataDir.filePath(QStringLiteral("LabTester/test_samples/%1/%2").arg(labUnderscoreName, sourceFileName));
    }

    for (const QString &candidate : sourceCandidates) {
        const QFileInfo sourceInfo(candidate);
        if (sourceInfo.exists() && sourceInfo.isFile()) {
            return sourceInfo.absoluteFilePath();
        }
    }

    return QString();
}

bool enrichRunPayloadWithSourceContent(
    const labtester::domain::Submission &submission,
    QJsonObject *runPayload,
    QString *warningMessage
)
{
    if (!runPayload) {
        if (warningMessage) {
            *warningMessage = QStringLiteral("Не удалось подготовить payload удаленного запуска.");
        }
        return false;
    }

    const QString sourcePath = resolveAccessibleSourcePath(submission);
    const QFileInfo sourceInfo(sourcePath);
    if (!sourceInfo.exists() || !sourceInfo.isFile()) {
        if (warningMessage) {
            *warningMessage = QStringLiteral(
                "Локальный исходник недоступен, отправляется только sourcePath."
            );
        }
        return false;
    }

    QFile sourceFile(sourceInfo.absoluteFilePath());
    if (!sourceFile.open(QIODevice::ReadOnly)) {
        if (warningMessage) {
            *warningMessage = QStringLiteral(
                "Не удалось прочитать исходник (%1), отправляется только sourcePath."
            ).arg(sourceFile.errorString());
        }
        return false;
    }

    const QByteArray sourceBytes = sourceFile.readAll();
    constexpr qint64 kMaxSourceBytes = 2 * 1024 * 1024;
    if (sourceBytes.size() > kMaxSourceBytes) {
        if (warningMessage) {
            *warningMessage = QStringLiteral(
                "Исходник больше %1 MB, отправляется только sourcePath."
            ).arg(kMaxSourceBytes / (1024 * 1024));
        }
        return false;
    }

    runPayload->insert(
        QStringLiteral("sourcePath"),
        QDir::toNativeSeparators(sourceInfo.absoluteFilePath())
    );
    runPayload->insert(QStringLiteral("sourceOriginalName"), sourceInfo.fileName());
    runPayload->insert(
        QStringLiteral("sourceContentBase64"),
        QString::fromLatin1(sourceBytes.toBase64())
    );
    return true;
}

labtester::domain::TestRunResult makeBuildErrorResult(
    const labtester::domain::Submission &submission,
    const QString &message,
    const QString &details
)
{
    labtester::domain::TestRunResult result;
    result.submissionId = submission.id;
    result.studentName = submission.student.name;
    result.labTitle = submission.labWork.title;
    result.executedAt = QDateTime::currentDateTime();
    result.totalTests = 1;
    result.passedTests = 0;
    result.failedTests = 1;
    result.status = labtester::domain::ExecutionStatus::BuildError;
    result.message = message;

    labtester::domain::TestCaseResult errorCase;
    errorCase.testName = QStringLiteral("Удаленный запуск");
    errorCase.passed = false;
    errorCase.message = message;
    errorCase.inputData = QStringLiteral("Файл: %1").arg(submission.sourcePath);
    errorCase.expectedOutput.clear();
    errorCase.actualOutput = details.isEmpty() ? message : details;
    errorCase.failureDetails = details;
    errorCase.durationMs = 0;
    result.testCases = {errorCase};
    return result;
}

bool waitForReply(QNetworkReply *reply, int timeoutMs, QString *errorMessage)
{
    if (!reply) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Ответ сервера не получен.");
        }
        return false;
    }

    QElapsedTimer timer;
    timer.start();

    while (!reply->isFinished()) {
        if (isInterruptionRequested()) {
            reply->abort();
            if (errorMessage) {
                *errorMessage = QStringLiteral("Проверка отменена пользователем.");
            }
            return false;
        }

        if (timer.elapsed() > timeoutMs) {
            reply->abort();
            if (errorMessage) {
                *errorMessage = QStringLiteral("Таймаут ожидания ответа удаленного сервера.");
            }
            return false;
        }

        QEventLoop loop;
        QTimer wakeTimer;
        wakeTimer.setSingleShot(true);
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        QObject::connect(&wakeTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
        wakeTimer.start(120);
        loop.exec();
    }

    if (reply->error() != QNetworkReply::NoError) {
        if (errorMessage) {
            *errorMessage = reply->errorString();
        }
        return false;
    }

    return true;
}

HttpResponse sendJsonRequest(
    QNetworkAccessManager &manager,
    const QString &method,
    const QUrl &url,
    const QJsonObject &payload,
    const QString &token,
    bool ngrokMode
)
{
    HttpResponse response;
    const int requestId = ++g_remoteRequestId;
    QElapsedTimer requestTimer;
    requestTimer.start();

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json; charset=utf-8"));
    request.setRawHeader("Accept", "application/json");

    const QString normalizedToken = token.trimmed();
    if (!normalizedToken.isEmpty()) {
        request.setRawHeader("Authorization", QStringLiteral("Bearer %1").arg(normalizedToken).toUtf8());
    }
    if (ngrokMode) {
        request.setRawHeader("ngrok-skip-browser-warning", "1");
    }

    QByteArray payloadBytes;
    QNetworkReply *reply = nullptr;
    if (method.compare(QStringLiteral("POST"), Qt::CaseInsensitive) == 0) {
        payloadBytes = QJsonDocument(payload).toJson(QJsonDocument::Compact);
        reply = manager.post(request, payloadBytes);
    } else {
        reply = manager.get(request);
    }

    qInfo().noquote() << QStringLiteral(
        "[REMOTE][REQ:%1] method=%2 url=%3 token=%4 ngrok=%5 payloadBytes=%6"
    ).arg(requestId)
     .arg(method.toUpper())
     .arg(url.toString())
     .arg(maskTokenForLog(token))
     .arg(ngrokMode ? QStringLiteral("1") : QStringLiteral("0"))
     .arg(payloadBytes.size());

    QString transportError;
    if (!waitForReply(reply, 30000, &transportError)) {
        response.error = transportError;
        if (reply) {
            response.statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            response.rawBody = reply->readAll();
            reply->deleteLater();
        }

        if (isSessionNotAttachedResponse(response.statusCode, response.rawBody)) {
            response.error = QStringLiteral(
                "xTunnel вернул 503 Session not attached. Туннель не прикреплен к активной сессии "
                "или агент не запущен."
            );
        }

        qWarning().noquote() << QStringLiteral(
            "[REMOTE][REQ:%1][TRANSPORT_FAIL] status=%2 elapsedMs=%3 error=%4 body=%5"
        ).arg(requestId)
         .arg(response.statusCode)
         .arg(requestTimer.elapsed())
         .arg(response.error)
         .arg(previewForLog(response.rawBody));
        return response;
    }

    response.statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    response.rawBody = reply->readAll();
    reply->deleteLater();

    qInfo().noquote() << QStringLiteral(
        "[REMOTE][REQ:%1][RESP] status=%2 elapsedMs=%3 bodyBytes=%4"
    ).arg(requestId)
     .arg(response.statusCode)
     .arg(requestTimer.elapsed())
     .arg(response.rawBody.size());

    QJsonParseError parseError;
    response.jsonBody = QJsonDocument::fromJson(response.rawBody, &parseError);
    if (parseError.error != QJsonParseError::NoError || !response.jsonBody.isObject()) {
        response.error = QStringLiteral("Ответ сервера не является корректным JSON.");
        qWarning().noquote() << QStringLiteral(
            "[REMOTE][REQ:%1][PARSE_FAIL] parseError=%2 body=%3"
        ).arg(requestId)
         .arg(parseError.errorString())
         .arg(previewForLog(response.rawBody));
        return response;
    }

    const QJsonObject body = response.jsonBody.object();
    if (body.value(QStringLiteral("ok")).isBool() && !body.value(QStringLiteral("ok")).toBool()) {
        response.error = body.value(QStringLiteral("error")).toString().trimmed();
        if (response.error.isEmpty()) {
            response.error = QStringLiteral("Удаленный сервер вернул ошибку.");
        }
        qWarning().noquote() << QStringLiteral(
            "[REMOTE][REQ:%1][API_FAIL] status=%2 error=%3 body=%4"
        ).arg(requestId)
         .arg(response.statusCode)
         .arg(response.error)
         .arg(previewForLog(response.rawBody));
        return response;
    }

    if (response.statusCode >= 400) {
        response.error = body.value(QStringLiteral("error")).toString().trimmed();
        if (response.error.isEmpty()) {
            response.error = QStringLiteral("HTTP ошибка %1").arg(response.statusCode);
        }
        qWarning().noquote() << QStringLiteral(
            "[REMOTE][REQ:%1][HTTP_FAIL] status=%2 error=%3 body=%4"
        ).arg(requestId)
         .arg(response.statusCode)
         .arg(response.error)
         .arg(previewForLog(response.rawBody));
        return response;
    }

    response.ok = true;
    return response;
}

labtester::domain::TestRunResult resultFromJson(
    const labtester::domain::Submission &submission,
    const QJsonObject &object
)
{
    labtester::domain::TestRunResult result;
    result.submissionId = object.value(QStringLiteral("submissionId")).toInt(submission.id);
    result.studentName = object.value(QStringLiteral("studentName")).toString(submission.student.name);
    result.labTitle = object.value(QStringLiteral("labTitle")).toString(submission.labWork.title);
    result.totalTests = object.value(QStringLiteral("totalTests")).toInt();
    result.passedTests = object.value(QStringLiteral("passedTests")).toInt();
    result.failedTests = object.value(QStringLiteral("failedTests")).toInt();
    result.status = labtester::domain::executionStatusFromString(
        object.value(QStringLiteral("status")).toString()
    );
    result.message = object.value(QStringLiteral("message")).toString().trimmed();

    const QString executedAtText = object.value(QStringLiteral("executedAt")).toString().trimmed();
    if (!executedAtText.isEmpty()) {
        result.executedAt = QDateTime::fromString(executedAtText, Qt::ISODate);
    }
    if (!result.executedAt.isValid()) {
        result.executedAt = QDateTime::currentDateTime();
    }

    const QJsonArray testCases = object.value(QStringLiteral("testCases")).toArray();
    result.testCases.reserve(static_cast<size_t>(testCases.size()));
    for (const auto &testCaseValue : testCases) {
        const QJsonObject testCaseObject = testCaseValue.toObject();
        labtester::domain::TestCaseResult testCase;
        testCase.testName = testCaseObject.value(QStringLiteral("testName")).toString();
        testCase.passed = testCaseObject.value(QStringLiteral("passed")).toBool();
        testCase.message = testCaseObject.value(QStringLiteral("message")).toString();
        testCase.inputData = testCaseObject.value(QStringLiteral("inputData")).toString();
        testCase.expectedOutput = testCaseObject.value(QStringLiteral("expectedOutput")).toString();
        testCase.actualOutput = testCaseObject.value(QStringLiteral("actualOutput")).toString();
        testCase.failureDetails = testCaseObject.value(QStringLiteral("failureDetails")).toString();
        testCase.durationMs = testCaseObject.value(QStringLiteral("durationMs")).toVariant().toLongLong();

        const QString visibleFailure = visibleFailureLine(
            testCase.failureDetails.isEmpty() ? testCase.message : testCase.failureDetails
        );
        if (!testCase.passed && containsTimeoutText(visibleFailure)) {
            if (testCase.expectedOutput.trimmed().isEmpty()
                || testCase.expectedOutput.contains(QStringLiteral("ожидаемое поведение"))) {
                testCase.expectedOutput = QStringLiteral("Алгоритм должен завершиться в пределах лимита времени.");
            }
            if (testCase.actualOutput.trimmed().isEmpty()
                || testCase.actualOutput.contains(QStringLiteral("фактическом выходе"))) {
                testCase.actualOutput = visibleFailure;
            }
            const qint64 visibleLimitMs = durationLimitFromText(
                testCase.actualOutput.isEmpty() ? visibleFailure : testCase.actualOutput
            );
            if (visibleLimitMs >= 0) {
                testCase.durationMs = visibleLimitMs;
            }
        }
        if (!testCase.passed
            && (testCase.actualOutput.trimmed().isEmpty()
                || testCase.actualOutput.contains(QStringLiteral("Нет данных о фактическом выходе"))
                || testCase.actualOutput.contains(QStringLiteral("Нет фактического вывода")))) {
            const QString detailedFailure = testCase.failureDetails.trimmed().isEmpty()
                ? testCase.message.trimmed()
                : testCase.failureDetails.trimmed();
            if (!detailedFailure.isEmpty()) {
                testCase.actualOutput = detailedFailure;
                testCase.failureDetails = detailedFailure;
            }
        }
        result.testCases.push_back(testCase);
    }

    if (result.totalTests <= 0) {
        result.totalTests = static_cast<int>(result.testCases.size());
    }
    if (result.passedTests < 0 || result.passedTests > result.totalTests) {
        int passedCount = 0;
        for (const auto &testCase : result.testCases) {
            if (testCase.passed) {
                ++passedCount;
            }
        }
        result.passedTests = passedCount;
    }
    if (result.failedTests < 0 || result.failedTests > result.totalTests) {
        result.failedTests = std::max(0, result.totalTests - result.passedTests);
    }
    if (result.message.isEmpty()) {
        result.message = QStringLiteral("Тесты: %1/%2").arg(result.passedTests).arg(result.totalTests);
    }

    return result;
}

} // namespace

namespace labtester::runners {

RemoteWorkerRunner::RemoteWorkerRunner(const services::ExecutionSettingsService &executionSettingsService)
    : m_executionSettingsService(executionSettingsService)
{
}

domain::TestRunResult RemoteWorkerRunner::run(const domain::Submission &submission) const
{
    try {
        const services::ExecutionSettingsService::RemoteConfig config = m_executionSettingsService.remoteConfig();
        if (config.endpoint.trimmed().isEmpty()) {
            return makeBuildErrorResult(
                submission,
                QStringLiteral("Не задан адрес удаленного воркера."),
                QStringLiteral("Укажите URL воркера в настройках.")
            );
        }
        if (config.token.trimmed().isEmpty()) {
            return makeBuildErrorResult(
                submission,
                QStringLiteral("Не задан токен удаленного воркера."),
                QStringLiteral("Укажите bearer token в настройках удаленного запуска.")
            );
        }

        qInfo().noquote() << QStringLiteral("[REMOTE][RUN] start submission=%1 endpoint=%2 token=%3")
                                 .arg(submission.id)
                                 .arg(config.endpoint)
                                 .arg(maskTokenForLog(config.token));

        QNetworkAccessManager manager;

        const HttpResponse healthResponse = sendJsonRequest(
            manager,
            QStringLiteral("GET"),
            QUrl(joinUrl(config.endpoint, QStringLiteral("/health"))),
            QJsonObject(),
            config.token,
            config.ngrokMode
        );
        const QString healthValidationError = validateHealthResponseAsLabTesterWorker(healthResponse);
        if (!healthValidationError.isEmpty()) {
            return makeBuildErrorResult(
                submission,
                QStringLiteral("Удаленный воркер недоступен по указанному адресу."),
                healthValidationError
            );
        }

        QJsonObject syncPayload;
        QString syncPayloadError;
        if (!buildLabSyncPayload(submission, &syncPayload, &syncPayloadError)) {
            return makeBuildErrorResult(
                submission,
                QStringLiteral("Не удалось подготовить синхронизацию лабораторной."),
                syncPayloadError
            );
        }

        const HttpResponse syncResponse = sendJsonRequest(
            manager,
            QStringLiteral("POST"),
            QUrl(joinUrl(config.endpoint, QStringLiteral("/sync/lab"))),
            syncPayload,
            config.token,
            config.ngrokMode
        );
        if (!syncResponse.ok) {
            return makeBuildErrorResult(
                submission,
                QStringLiteral("Не удалось синхронизировать лабораторную с воркером."),
                syncResponse.error + QStringLiteral("\n") + previewForLog(syncResponse.rawBody, 800)
            );
        }
        {
            const QJsonObject syncBody = syncResponse.jsonBody.object();
            qInfo().noquote() << QStringLiteral("[REMOTE][SYNC] submission=%1 labId=%2 filesWritten=%3")
                                     .arg(submission.id)
                                     .arg(submission.labWork.id)
                                     .arg(syncBody.value(QStringLiteral("filesWritten")).toInt());
        }

        QJsonObject runPayload {
            {QStringLiteral("submissionId"), submission.id},
            {QStringLiteral("labId"), submission.labWork.id},
            {QStringLiteral("labTitle"), submission.labWork.title},
            {QStringLiteral("labRootName"), syncPayload.value(QStringLiteral("labRootName")).toString()},
            {QStringLiteral("labSignature"), syncPayload.value(QStringLiteral("labSignature")).toString()},
            {QStringLiteral("studentName"), submission.student.name},
            {QStringLiteral("sourcePath"), submission.sourcePath}
        };

        QString sourcePayloadWarning;
        const bool sourceEmbedded = enrichRunPayloadWithSourceContent(
            submission,
            &runPayload,
            &sourcePayloadWarning
        );
        if (!sourcePayloadWarning.isEmpty()) {
            qWarning().noquote() << QStringLiteral("[REMOTE][RUN] submission=%1 sourcePayloadWarning=%2")
                                        .arg(submission.id)
                                        .arg(sourcePayloadWarning);
        }
        qInfo().noquote() << QStringLiteral("[REMOTE][RUN] submission=%1 sourceEmbedded=%2 sourcePath=%3")
                                 .arg(submission.id)
                                 .arg(sourceEmbedded ? QStringLiteral("1") : QStringLiteral("0"))
                                 .arg(runPayload.value(QStringLiteral("sourcePath")).toString());

        const HttpResponse runResponse = sendJsonRequest(
            manager,
            QStringLiteral("POST"),
            QUrl(joinUrl(config.endpoint, QStringLiteral("/run"))),
            runPayload,
            config.token,
            config.ngrokMode
        );
        if (!runResponse.ok) {
            return makeBuildErrorResult(
                submission,
                QStringLiteral("Не удалось запустить удаленную проверку."),
                runResponse.error + QStringLiteral("\n") + previewForLog(runResponse.rawBody, 800)
            );
        }

        const QJsonObject runBody = runResponse.jsonBody.object();
        const QString jobId = runBody.value(QStringLiteral("jobId")).toString().trimmed();
        if (jobId.isEmpty()) {
            return makeBuildErrorResult(
                submission,
                QStringLiteral("Удаленный сервер не вернул jobId."),
                previewForLog(runResponse.rawBody, 800)
            );
        }

        qInfo().noquote() << QStringLiteral("[REMOTE][RUN] queued submission=%1 jobId=%2")
                                 .arg(submission.id)
                                 .arg(jobId);

        QElapsedTimer pollTimer;
        pollTimer.start();
        const int maxPollMs = 10 * 60 * 1000;
        const int pollIntervalMs = 700;
        int pollCount = 0;
        QString lastState;

        while (pollTimer.elapsed() < maxPollMs) {
            if (isInterruptionRequested()) {
                return makeBuildErrorResult(
                    submission,
                    QStringLiteral("Проверка отменена пользователем."),
                    QStringLiteral("Отмена была запрошена до получения удаленного результата.")
                );
            }

            const HttpResponse jobResponse = sendJsonRequest(
                manager,
                QStringLiteral("GET"),
                QUrl(joinUrl(config.endpoint, QStringLiteral("/jobs/%1").arg(jobId))),
                QJsonObject(),
                config.token,
                config.ngrokMode
            );
            if (!jobResponse.ok) {
                return makeBuildErrorResult(
                    submission,
                    QStringLiteral("Ошибка при опросе статуса удаленной проверки."),
                    jobResponse.error + QStringLiteral("\n") + previewForLog(jobResponse.rawBody, 800)
                );
            }

            ++pollCount;
            const QJsonObject root = jobResponse.jsonBody.object();
            const QJsonObject job = root.value(QStringLiteral("job")).toObject();
            const QString state = job.value(QStringLiteral("state")).toString().trimmed().toLower();

            if (state != lastState || (pollCount % 10 == 0)) {
                qInfo().noquote() << QStringLiteral("[REMOTE][POLL] submission=%1 jobId=%2 poll=%3 state=%4 elapsedMs=%5")
                                         .arg(submission.id)
                                         .arg(jobId)
                                         .arg(pollCount)
                                         .arg(state.isEmpty() ? QStringLiteral("<empty>") : state)
                                         .arg(pollTimer.elapsed());
                lastState = state;
            }

            if (state == QStringLiteral("finished")) {
                const QJsonObject resultObject = job.value(QStringLiteral("result")).toObject();
                const domain::TestRunResult result = resultFromJson(submission, resultObject);
                qInfo().noquote() << QStringLiteral("[REMOTE][RUN] finished submission=%1 jobId=%2 status=%3")
                                         .arg(submission.id)
                                         .arg(jobId)
                                         .arg(domain::toStorageString(result.status));
                return result;
            }

            if (state == QStringLiteral("failed")) {
                const QString jobError = job.value(QStringLiteral("error")).toString().trimmed();
                const QJsonObject resultObject = job.value(QStringLiteral("result")).toObject();
                qWarning().noquote() << QStringLiteral("[REMOTE][RUN] failed submission=%1 jobId=%2 error=%3")
                                            .arg(submission.id)
                                            .arg(jobId)
                                            .arg(jobError);
                if (!resultObject.isEmpty()) {
                    return resultFromJson(submission, resultObject);
                }
                return makeBuildErrorResult(
                    submission,
                    QStringLiteral("Удаленный воркер завершил задачу с ошибкой."),
                    jobError
                );
            }

            QElapsedTimer pauseTimer;
            pauseTimer.start();
            while (pauseTimer.elapsed() < pollIntervalMs) {
                if (isInterruptionRequested()) {
                    return makeBuildErrorResult(
                        submission,
                        QStringLiteral("Проверка отменена пользователем."),
                        QStringLiteral("Отмена была запрошена до получения удаленного результата.")
                    );
                }
                QThread::msleep(40);
            }
        }

        return makeBuildErrorResult(
            submission,
            QStringLiteral("Удаленный воркер не успел вернуть результат."),
            QStringLiteral("Превышен лимит ожидания (%1 секунд).").arg(maxPollMs / 1000)
        );
    } catch (const std::exception &ex) {
        qCritical().noquote() << QStringLiteral("[REMOTE][RUN][EXCEPTION] submission=%1 what=%2")
                                      .arg(submission.id)
                                      .arg(QString::fromUtf8(ex.what()));
        return makeBuildErrorResult(
            submission,
            QStringLiteral("Критическая ошибка удаленного запуска."),
            QString::fromUtf8(ex.what())
        );
    } catch (...) {
        qCritical().noquote() << QStringLiteral("[REMOTE][RUN][EXCEPTION] submission=%1 unknown")
                                      .arg(submission.id);
        return makeBuildErrorResult(
            submission,
            QStringLiteral("Критическая ошибка удаленного запуска."),
            QStringLiteral("Неизвестное исключение в RemoteWorkerRunner.")
        );
    }
}

} // namespace labtester::runners
