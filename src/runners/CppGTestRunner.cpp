#include "runners/CppGTestRunner.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QStringList>
#include <QThread>
#include <QUuid>
#include <QXmlStreamReader>

namespace {

struct ProcessRunResult {
    bool started {false};
    bool interrupted {false};
    int exitCode {-1};
    QString stdOut;
    QString stdErr;
    QString errorText;
};

struct UTestLayout {
    QString cmakeSourceDir;
    QString suitePath;
    QString headerPath;
};

struct CaseMetadata {
    QString inputData;
    QString expectedOutput;
};

QString tailText(const QString &text, int maxChars = 2200)
{
    if (text.size() <= maxChars) {
        return text;
    }
    return QStringLiteral("...%1").arg(text.right(maxChars));
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

QString findExecutable(const QString &name)
{
    return QStandardPaths::findExecutable(name);
}

bool looksLikeMingwToolPath(const QString &path)
{
    const QString normalized = QDir::fromNativeSeparators(path).toLower();
    return normalized.contains(QStringLiteral("/mingw"))
        || normalized.contains(QStringLiteral("/msys"));
}

QString firstExistingFile(const QStringList &candidates)
{
    for (const QString &candidate : candidates) {
        if (candidate.trimmed().isEmpty()) {
            continue;
        }

        QFileInfo info(candidate);
        if (info.exists() && info.isFile()) {
            return QDir::toNativeSeparators(info.absoluteFilePath());
        }
    }
    return QString();
}

QString findPreferredCMakeExecutable()
{
    const QString fromEnv = qEnvironmentVariable("LABTESTER_CMAKE").trimmed();
    if (!fromEnv.isEmpty() && QFileInfo::exists(fromEnv)) {
        return QDir::toNativeSeparators(QFileInfo(fromEnv).absoluteFilePath());
    }

    const QString fromPath = findExecutable(QStringLiteral("cmake"));
    if (!fromPath.isEmpty() && !looksLikeMingwToolPath(fromPath)) {
        return QDir::toNativeSeparators(fromPath);
    }

    QStringList preferredCandidates;

    const QStringList qtBases = {
        qEnvironmentVariable("LABTESTER_QT_BASE").trimmed(),
        qEnvironmentVariable("QT_BASE_DIR").trimmed(),
        QStringLiteral("C:/Qt"),
        QStringLiteral("D:/Qt"),
        QStringLiteral("E:/Qt")
    };

    for (const QString &qtBase : qtBases) {
        if (qtBase.trimmed().isEmpty() || !QFileInfo::exists(qtBase)) {
            continue;
        }

        QDir baseDir(qtBase);
        preferredCandidates << baseDir.filePath(QStringLiteral("Tools/CMake_64/bin/cmake.exe"));
        preferredCandidates << baseDir.filePath(QStringLiteral("Tools/CMake/bin/cmake.exe"));
    }

    const QString programFiles = qEnvironmentVariable("ProgramFiles").trimmed();
    const QString programFilesX86 = qEnvironmentVariable("ProgramFiles(x86)").trimmed();
    if (!programFiles.isEmpty()) {
        preferredCandidates << QDir(programFiles).filePath(QStringLiteral("CMake/bin/cmake.exe"));
    }
    if (!programFilesX86.isEmpty()) {
        preferredCandidates << QDir(programFilesX86).filePath(QStringLiteral("CMake/bin/cmake.exe"));
    }

    const QString preferred = firstExistingFile(preferredCandidates);
    if (!preferred.isEmpty()) {
        return preferred;
    }

    return fromPath;
}

QString quoteCmdArg(const QString &value)
{
    QString escaped = value;
    escaped.replace('"', QStringLiteral("\"\""));
    return QStringLiteral("\"%1\"").arg(escaped);
}

QString findVcVars64Bat()
{
    const QString programFilesX86 = qEnvironmentVariable("ProgramFiles(x86)");
    const QString vswherePath = QDir(programFilesX86).filePath(
        QStringLiteral("Microsoft Visual Studio/Installer/vswhere.exe")
    );

    if (QFileInfo::exists(vswherePath)) {
        QProcess vswhereProcess;
        vswhereProcess.start(
            vswherePath,
            {
                QStringLiteral("-latest"),
                QStringLiteral("-products"), QStringLiteral("*"),
                QStringLiteral("-requires"), QStringLiteral("Microsoft.VisualStudio.Component.VC.Tools.x86.x64"),
                QStringLiteral("-property"), QStringLiteral("installationPath")
            }
        );
        if (vswhereProcess.waitForFinished(5000) && vswhereProcess.exitCode() == 0) {
            const QString installPath = QString::fromLocal8Bit(vswhereProcess.readAllStandardOutput()).trimmed();
            if (!installPath.isEmpty()) {
                const QString candidate = QDir(installPath).filePath(
                    QStringLiteral("VC/Auxiliary/Build/vcvars64.bat")
                );
                if (QFileInfo::exists(candidate)) {
                    return QDir::toNativeSeparators(candidate);
                }
            }
        }
    }

    const QStringList fallbackPaths = {
        QStringLiteral("C:/Program Files/Microsoft Visual Studio/18/Community/VC/Auxiliary/Build/vcvars64.bat"),
        QStringLiteral("C:/Program Files/Microsoft Visual Studio/18/Professional/VC/Auxiliary/Build/vcvars64.bat"),
        QStringLiteral("C:/Program Files/Microsoft Visual Studio/18/Enterprise/VC/Auxiliary/Build/vcvars64.bat"),
        QStringLiteral("C:/Program Files/Microsoft Visual Studio/17/Community/VC/Auxiliary/Build/vcvars64.bat"),
        QStringLiteral("C:/Program Files/Microsoft Visual Studio/17/Professional/VC/Auxiliary/Build/vcvars64.bat"),
        QStringLiteral("C:/Program Files/Microsoft Visual Studio/17/Enterprise/VC/Auxiliary/Build/vcvars64.bat"),
        QStringLiteral("C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat")
    };

    for (const auto &candidate : fallbackPaths) {
        if (QFileInfo::exists(candidate)) {
            return QDir::toNativeSeparators(candidate);
        }
    }

    return QString();
}

bool isInterruptionRequested()
{
    QThread *thread = QThread::currentThread();
    return thread && thread->isInterruptionRequested();
}

void terminateProcessTree(QProcess &process)
{
    if (process.state() == QProcess::NotRunning) {
        return;
    }

#ifdef Q_OS_WIN
    const qint64 processId = process.processId();
    if (processId > 0) {
        QProcess::execute(
            QStringLiteral("taskkill"),
            {
                QStringLiteral("/PID"),
                QString::number(processId),
                QStringLiteral("/T"),
                QStringLiteral("/F")
            }
        );
    }
#endif

    process.kill();
    process.waitForFinished(3000);
}

ProcessRunResult runProcess(
    const QString &program,
    const QStringList &arguments,
    const QString &workingDirectory,
    int timeoutMs
)
{
    ProcessRunResult result;
    if (isInterruptionRequested()) {
        result.interrupted = true;
        result.errorText = QStringLiteral("Выполнение отменено пользователем.");
        return result;
    }

    QProcess process;
    process.setProgram(program);
    process.setArguments(arguments);
    process.setWorkingDirectory(workingDirectory);
    process.setProcessChannelMode(QProcess::SeparateChannels);

    process.start();
    if (!process.waitForStarted(15000)) {
        result.errorText = process.errorString();
        return result;
    }
    result.started = true;

    QElapsedTimer timer;
    timer.start();
    while (process.state() != QProcess::NotRunning) {
        if (process.waitForFinished(100)) {
            break;
        }

        if (isInterruptionRequested()) {
            terminateProcessTree(process);
            result.interrupted = true;
            result.errorText = QStringLiteral("Выполнение отменено пользователем.");
            result.stdOut = QString::fromLocal8Bit(process.readAllStandardOutput());
            result.stdErr = QString::fromLocal8Bit(process.readAllStandardError());
            return result;
        }

        if (timer.elapsed() > timeoutMs) {
            terminateProcessTree(process);
            result.errorText = QStringLiteral("Превышено время ожидания выполнения процесса.");
            result.stdOut = QString::fromLocal8Bit(process.readAllStandardOutput());
            result.stdErr = QString::fromLocal8Bit(process.readAllStandardError());
            return result;
        }
    }

    result.exitCode = process.exitCode();
    result.stdOut = QString::fromLocal8Bit(process.readAllStandardOutput());
    result.stdErr = QString::fromLocal8Bit(process.readAllStandardError());
    return result;
}

ProcessRunResult runProcessWithVcVars(
    const QString &program,
    const QStringList &arguments,
    const QString &workingDirectory,
    int timeoutMs,
    const QString &vcvarsPath
)
{
    if (vcvarsPath.isEmpty()) {
        return runProcess(program, arguments, workingDirectory, timeoutMs);
    }

    QDir workDir(workingDirectory);
    workDir.mkpath(QStringLiteral("."));

    const QString scriptPath = workDir.filePath(
        QStringLiteral("labtester_cmd_wrapper_%1.cmd")
            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces))
    );
    QFile script(scriptPath);
    if (!script.open(QIODevice::WriteOnly | QIODevice::Text)) {
        ProcessRunResult failed;
        failed.errorText = QStringLiteral("Не удалось создать временный cmd-скрипт.");
        return failed;
    }

    QStringList commandParts;
    commandParts.reserve(arguments.size() + 1);
    commandParts << quoteCmdArg(program);
    for (const auto &arg : arguments) {
        commandParts << quoteCmdArg(arg);
    }

    QString scriptText;
    scriptText += QStringLiteral("@echo off\r\n");
    scriptText += QStringLiteral("setlocal\r\n");
    scriptText += QStringLiteral("call %1\r\n").arg(quoteCmdArg(vcvarsPath));
    scriptText += QStringLiteral("if errorlevel 1 exit /b %errorlevel%\r\n");
    scriptText += commandParts.join(' ') + QStringLiteral("\r\n");
    scriptText += QStringLiteral("exit /b %errorlevel%\r\n");

    script.write(scriptText.toUtf8());
    script.close();

    const QString cmdScriptArg = QDir::toNativeSeparators(scriptPath);
    const ProcessRunResult wrapped = runProcess(
        QStringLiteral("cmd"),
        {QStringLiteral("/d"), QStringLiteral("/s"), QStringLiteral("/c"), cmdScriptArg},
        workingDirectory,
        timeoutMs
    );

    script.remove();
    return wrapped;
}

QString escapedPathForCmake(const QString &path)
{
    QString normalized = QDir::fromNativeSeparators(path);
    normalized.replace(QStringLiteral("\""), QStringLiteral("\\\""));
    return normalized;
}

bool hasSuiteAndHeaderInDir(const QString &directoryPath)
{
    const QDir dir(directoryPath);
    return dir.exists(QStringLiteral("sort_test.cpp"))
        && dir.exists(QStringLiteral("cc.h"));
}

void appendLabTestRootCandidates(QStringList *candidates, int labId)
{
    if (!candidates || labId <= 0) {
        return;
    }

    const QDir currentDir(QDir::currentPath());
    const QDir appDir(QCoreApplication::applicationDirPath());
    const QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString labTestsRelative = QStringLiteral("labs/lab_%1/tests").arg(labId);

    if (!appDataPath.trimmed().isEmpty()) {
        *candidates
            << QDir(appDataPath).filePath(QStringLiteral("LabTesterWorker/labs/lab_%1/tests").arg(labId))
            << QDir(appDataPath).filePath(QStringLiteral("labs/lab_%1/tests").arg(labId));
    }

    *candidates
        << currentDir.filePath(labTestsRelative)
        << currentDir.filePath(QStringLiteral("../") + labTestsRelative)
        << appDir.filePath(QStringLiteral("../") + labTestsRelative)
        << appDir.filePath(QStringLiteral("../../") + labTestsRelative);
}

UTestLayout resolveUTestLayout(const labtester::domain::Submission &submission)
{
    UTestLayout layout;

    QStringList candidateRoots;

    if (!submission.labWork.testSuitePath.isEmpty()) {
        const QFileInfo suiteInfo(submission.labWork.testSuitePath);
        if (suiteInfo.exists()) {
            candidateRoots << suiteInfo.absolutePath();
            layout.suitePath = suiteInfo.absoluteFilePath();
        }
    }
    if (!submission.labWork.referenceHeaderPath.isEmpty()) {
        const QFileInfo headerInfo(submission.labWork.referenceHeaderPath);
        if (headerInfo.exists()) {
            candidateRoots << headerInfo.absolutePath();
            layout.headerPath = headerInfo.absoluteFilePath();
        }
    }

    const QDir currentDir(QDir::currentPath());
    const QDir appDir(QCoreApplication::applicationDirPath());
    appendLabTestRootCandidates(&candidateRoots, submission.labWork.id);

    candidateRoots
        << currentDir.filePath(QStringLiteral("UTEST"))
        << currentDir.filePath(QStringLiteral("../UTEST"))
        << appDir.filePath(QStringLiteral("../UTEST"))
        << appDir.filePath(QStringLiteral("../../UTEST"));

    QSet<QString> uniqueRoots;
    for (const auto &candidate : candidateRoots) {
        const QString absoluteRoot = QDir(candidate).absolutePath();
        if (uniqueRoots.contains(absoluteRoot)) {
            continue;
        }
        uniqueRoots.insert(absoluteRoot);

        if (!hasSuiteAndHeaderInDir(absoluteRoot)) {
            continue;
        }

        const QFileInfo suiteInfo(QDir(absoluteRoot).filePath(QStringLiteral("sort_test.cpp")));
        const QFileInfo headerInfo(QDir(absoluteRoot).filePath(QStringLiteral("cc.h")));
        layout.suitePath = suiteInfo.absoluteFilePath();
        layout.headerPath = headerInfo.absoluteFilePath();

        if (QFileInfo::exists(QDir(absoluteRoot).filePath(QStringLiteral("CMakeLists.txt")))) {
            layout.cmakeSourceDir = absoluteRoot;
            return layout;
        }
    }

    return layout;
}

QString resolveBuildDirectory(const QString &identity)
{
    const QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString basePath = appDataPath.isEmpty() ? QDir::currentPath() : appDataPath;
    const QString key = QString::fromLatin1(
        QCryptographicHash::hash(identity.toUtf8(), QCryptographicHash::Md5).toHex().left(10)
    );
    QDir dir(basePath);
    dir.mkpath(QStringLiteral("LabTester/utest_build/%1").arg(key));
    return dir.filePath(QStringLiteral("LabTester/utest_build/%1").arg(key));
}

QString ensureStandaloneCMakeSource(
    const UTestLayout &layout,
    const QString &buildDir,
    QString *errorMessage
)
{
    if (layout.suitePath.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Не найден файл sort_test.cpp.");
        }
        return QString();
    }
    if (layout.headerPath.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Не найден файл cc.h.");
        }
        return QString();
    }

    const QString generatedSourceDir = QDir(buildDir).filePath(QStringLiteral("generated_source"));
    QDir sourceDir(generatedSourceDir);
    if (!sourceDir.mkpath(QStringLiteral("."))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Не удалось создать временную папку CMake проекта.");
        }
        return QString();
    }

    const QString suitePath = QDir::toNativeSeparators(QFileInfo(layout.suitePath).absoluteFilePath());
    const QString suiteDir = QDir::toNativeSeparators(QFileInfo(layout.suitePath).absolutePath());
    const QString headerDir = QDir::toNativeSeparators(QFileInfo(layout.headerPath).absolutePath());

    QString cmakeScript;
    cmakeScript += QStringLiteral("cmake_minimum_required(VERSION 3.14)\n");
    cmakeScript += QStringLiteral("project(LabTesterGeneratedTests LANGUAGES CXX)\n\n");
    cmakeScript += QStringLiteral("set(CMAKE_CXX_STANDARD 17)\n");
    cmakeScript += QStringLiteral("set(CMAKE_CXX_STANDARD_REQUIRED ON)\n\n");
    cmakeScript += QStringLiteral("if(MSVC)\n");
    cmakeScript += QStringLiteral("    add_compile_options(/utf-8)\n");
    cmakeScript += QStringLiteral("endif()\n\n");
    cmakeScript += QStringLiteral("set(LABTESTER_STUDENT_SOURCE \"\" CACHE FILEPATH \"Path to student source file\")\n");
    cmakeScript += QStringLiteral("if(NOT EXISTS \"${LABTESTER_STUDENT_SOURCE}\")\n");
    cmakeScript += QStringLiteral("    message(FATAL_ERROR \"LABTESTER_STUDENT_SOURCE not found: ${LABTESTER_STUDENT_SOURCE}\")\n");
    cmakeScript += QStringLiteral("endif()\n\n");
    cmakeScript += QStringLiteral("include(FetchContent)\n");
    cmakeScript += QStringLiteral("FetchContent_Declare(\n");
    cmakeScript += QStringLiteral("    googletest\n");
    cmakeScript += QStringLiteral("    URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip\n");
    cmakeScript += QStringLiteral(")\n");
    cmakeScript += QStringLiteral("set(gtest_force_shared_crt ON CACHE BOOL \"\" FORCE)\n");
    cmakeScript += QStringLiteral("FetchContent_MakeAvailable(googletest)\n\n");
    cmakeScript += QStringLiteral("add_executable(sort_test\n");
    cmakeScript += QStringLiteral("    \"%1\"\n").arg(escapedPathForCmake(suitePath));
    cmakeScript += QStringLiteral("    \"${LABTESTER_STUDENT_SOURCE}\"\n");
    cmakeScript += QStringLiteral(")\n\n");
    cmakeScript += QStringLiteral("target_include_directories(sort_test PRIVATE\n");
    cmakeScript += QStringLiteral("    \"%1\"\n").arg(escapedPathForCmake(suiteDir));
    cmakeScript += QStringLiteral("    \"%1\"\n").arg(escapedPathForCmake(headerDir));
    cmakeScript += QStringLiteral(")\n\n");
    cmakeScript += QStringLiteral("target_link_libraries(sort_test PRIVATE GTest::gtest_main)\n\n");
    cmakeScript += QStringLiteral("include(GoogleTest)\n");
    cmakeScript += QStringLiteral("gtest_discover_tests(sort_test)\n");

    QSaveFile file(QDir(generatedSourceDir).filePath(QStringLiteral("CMakeLists.txt")));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Не удалось создать CMakeLists.txt во временном проекте.");
        }
        return QString();
    }
    file.write(cmakeScript.toUtf8());
    if (!file.commit()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Не удалось сохранить временный CMakeLists.txt.");
        }
        return QString();
    }

    return generatedSourceDir;
}

QString createReportDirectory(int submissionId)
{
    const QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString basePath = appDataPath.isEmpty() ? QDir::currentPath() : appDataPath;
    QDir dir(basePath);
    dir.mkpath(QStringLiteral("LabTester/test_reports"));

    const QString folderName = QStringLiteral("submission_%1_%2")
        .arg(submissionId)
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz")));
    dir.mkpath(QStringLiteral("LabTester/test_reports/%1").arg(folderName));
    return dir.filePath(QStringLiteral("LabTester/test_reports/%1").arg(folderName));
}

QString resolveBuiltExecutable(const QString &buildDir)
{
    const QStringList candidates = {
        QDir(buildDir).filePath(QStringLiteral("sort_test.exe")),
        QDir(buildDir).filePath(QStringLiteral("sort_test")),
        QDir(buildDir).filePath(QStringLiteral("Debug/sort_test.exe")),
        QDir(buildDir).filePath(QStringLiteral("Release/sort_test.exe")),
        QDir(buildDir).filePath(QStringLiteral("RelWithDebInfo/sort_test.exe"))
    };

    for (const auto &candidate : candidates) {
        if (QFileInfo::exists(candidate)) {
            return QDir::toNativeSeparators(QFileInfo(candidate).absoluteFilePath());
        }
    }
    return QString();
}

CaseMetadata metadataForCase(const QString &testName)
{
    const QString suiteName = testName.section('.', 0, 0).section('/', -1);
    const QString caseName = testName.section('.', -1);
    auto metadata = [](const QString &input, const QString &expected) {
        return CaseMetadata{input, expected};
    };

    if (suiteName.startsWith(QStringLiteral("Stack_"))) {
        if (caseName == QStringLiteral("StartsEmpty")) {
            return metadata(QStringLiteral("ArrayStack stack; вызовы empty(), size()"), QStringLiteral("empty() == true, size() == 0"));
        }
        if (caseName == QStringLiteral("PushIncreasesSize")) {
            return metadata(QStringLiteral("push(10), push(20), затем size() и top()"), QStringLiteral("size() == 2, top() == 20"));
        }
        if (caseName == QStringLiteral("PopReturnsLast")) {
            return metadata(QStringLiteral("push(1), push(2), push(3), затем два pop()"), QStringLiteral("pop() возвращает 3, затем 2"));
        }
        if (caseName == QStringLiteral("TopDoesNotRemove")) {
            return metadata(QStringLiteral("push(7), push(8), два вызова top()"), QStringLiteral("top() оба раза 8, size() остается 2"));
        }
        if (caseName == QStringLiteral("HandlesNegativeValues")) {
            return metadata(QStringLiteral("push(-5), push(0), push(-10), затем pop()"), QStringLiteral("значения возвращаются в порядке -10, 0, -5"));
        }
        if (caseName == QStringLiteral("BecomesEmptyAfterPops")) {
            return metadata(QStringLiteral("push(1), push(2), затем два pop()"), QStringLiteral("после извлечения всех элементов empty() == true, size() == 0"));
        }
        if (caseName == QStringLiteral("MaintainsOrderForTenValues")) {
            return metadata(QStringLiteral("push(0..9), затем pop() до конца"), QStringLiteral("значения возвращаются в обратном порядке 9..0"));
        }
        if (caseName == QStringLiteral("AlternatingPushPop")) {
            return metadata(QStringLiteral("чередование push/pop: 1, затем 2/3/4"), QStringLiteral("стек сохраняет LIFO-порядок при смешанных операциях"));
        }
        if (caseName == QStringLiteral("Duplicates")) {
            return metadata(QStringLiteral("push(6), push(6), push(6)"), QStringLiteral("три pop() возвращают 6"));
        }
        if (caseName == QStringLiteral("WideRangeValues")) {
            return metadata(QStringLiteral("push(INT_MAX), push(-INT_MAX)"), QStringLiteral("pop() возвращает -INT_MAX, затем INT_MAX"));
        }
        if (caseName == QStringLiteral("GrowsBeyondInitialCapacity")) {
            return metadata(QStringLiteral("push 128 элементов: 0, 3, 6, ..."), QStringLiteral("size() == 128, pop() возвращает значения в обратном порядке"));
        }
        if (caseName == QStringLiteral("ReusesAfterEmptying")) {
            return metadata(QStringLiteral("30 push, полная очистка pop, затем push(100..109)"), QStringLiteral("top() == 109, size() == 10"));
        }
        if (caseName == QStringLiteral("LongAlternatingSequence")) {
            return metadata(QStringLiteral("100 циклов push(i), push(i+1000), pop()"), QStringLiteral("после каждого pop() возвращается i+1000, в конце остаются 0..99"));
        }
        if (caseName == QStringLiteral("PartialDrainKeepsOlderValues")) {
            return metadata(QStringLiteral("push(1..50), затем pop() значений 50..26"), QStringLiteral("top() == 25, size() == 25"));
        }
        if (caseName == QStringLiteral("ManyDuplicateBlocks")) {
            return metadata(QStringLiteral("push 30 значений i % 3"), QStringLiteral("pop() возвращает тот же шаблон в обратном порядке"));
        }
        if (caseName == QStringLiteral("PushMany")) {
            return metadata(QStringLiteral("300000 вызовов ArrayStack::push(i)"), QStringLiteral("size() == 300000 за лимит времени"));
        }
        if (caseName == QStringLiteral("PopMany")) {
            return metadata(QStringLiteral("300000 push, затем 300000 pop"), QStringLiteral("size() == 300000 перед pop; pop() возвращает 299999..0 за лимит времени"));
        }
        if (caseName == QStringLiteral("AlternatingMany")) {
            return metadata(QStringLiteral("250000 циклов push(i), pop()"), QStringLiteral("каждый pop() возвращает i за лимит времени"));
        }
        if (caseName == QStringLiteral("TopMany")) {
            return metadata(QStringLiteral("10000 push, затем 200000 вызовов top()"), QStringLiteral("top() работает быстро и не удаляет элементы"));
        }
        if (caseName == QStringLiteral("GrowthWaves")) {
            return metadata(QStringLiteral("20 волн: 12000 push, затем 6000 pop"), QStringLiteral("стек корректно расширяется и не теряет элементы"));
        }
    }

    if (suiteName.startsWith(QStringLiteral("Queue_"))) {
        if (caseName == QStringLiteral("StartsEmpty")) {
            return metadata(QStringLiteral("ArrayQueue queue; вызовы empty(), size()"), QStringLiteral("empty() == true, size() == 0"));
        }
        if (caseName == QStringLiteral("PushIncreasesSize")) {
            return metadata(QStringLiteral("push(10), push(20), затем size() и front()"), QStringLiteral("size() == 2, front() == 10"));
        }
        if (caseName == QStringLiteral("PopReturnsFirst")) {
            return metadata(QStringLiteral("push(1), push(2), push(3), затем два pop()"), QStringLiteral("pop() возвращает 1, затем 2"));
        }
        if (caseName == QStringLiteral("FrontDoesNotRemove")) {
            return metadata(QStringLiteral("push(5), push(6), два вызова front()"), QStringLiteral("front() оба раза 5, size() остается 2"));
        }
        if (caseName == QStringLiteral("HandlesNegativeValues")) {
            return metadata(QStringLiteral("push(-1), push(-2), затем pop()"), QStringLiteral("очередь возвращает -1, затем -2"));
        }
        if (caseName == QStringLiteral("BecomesEmptyAfterPops")) {
            return metadata(QStringLiteral("push(1), push(2), затем два pop()"), QStringLiteral("после извлечения всех элементов empty() == true"));
        }
        if (caseName == QStringLiteral("MaintainsOrderForTenValues")) {
            return metadata(QStringLiteral("push(0..9), затем pop() до конца"), QStringLiteral("значения возвращаются в FIFO-порядке 0..9"));
        }
        if (caseName == QStringLiteral("AlternatingPushPop")) {
            return metadata(QStringLiteral("смешанные операции push/pop над 1,2,3,4"), QStringLiteral("очередь сохраняет FIFO-порядок"));
        }
        if (caseName == QStringLiteral("Duplicates")) {
            return metadata(QStringLiteral("push(7), push(7), затем pop()"), QStringLiteral("оба pop() возвращают 7"));
        }
        if (caseName == QStringLiteral("WideRangeValues")) {
            return metadata(QStringLiteral("push(INT_MAX), push(-INT_MAX)"), QStringLiteral("pop() возвращает INT_MAX, затем -INT_MAX"));
        }
        if (caseName == QStringLiteral("GrowsBeyondInitialCapacity")) {
            return metadata(QStringLiteral("push 150 элементов 0..149"), QStringLiteral("size() == 150, pop() возвращает 0..149"));
        }
        if (caseName == QStringLiteral("WrapAroundAfterPops")) {
            return metadata(QStringLiteral("push 0..79, pop 0..49, push 80..139"), QStringLiteral("последующие pop() возвращают 50..139"));
        }
        if (caseName == QStringLiteral("ReusesAfterEmptying")) {
            return metadata(QStringLiteral("30 push/pop до пустой очереди, затем push(100..109)"), QStringLiteral("front() == 100, size() == 10"));
        }
        if (caseName == QStringLiteral("PartialDrainKeepsFront")) {
            return metadata(QStringLiteral("push 1..60, затем pop 1..35"), QStringLiteral("front() == 36, size() == 25"));
        }
        if (caseName == QStringLiteral("ManyDuplicateBlocks")) {
            return metadata(QStringLiteral("push 45 значений i % 5"), QStringLiteral("pop() возвращает тот же шаблон 0,1,2,3,4..."));
        }
        if (caseName == QStringLiteral("PushMany")) {
            return metadata(QStringLiteral("250000 вызовов ArrayQueue::push(i)"), QStringLiteral("size() == 250000 за лимит времени"));
        }
        if (caseName == QStringLiteral("PopMany")) {
            return metadata(QStringLiteral("160000 push, затем 160000 pop"), QStringLiteral("pop() возвращает 0..159999 за лимит времени"));
        }
        if (caseName == QStringLiteral("AlternatingMany")) {
            return metadata(QStringLiteral("220000 циклов push(i), pop()"), QStringLiteral("каждый pop() возвращает i за лимит времени"));
        }
        if (caseName == QStringLiteral("WrapAroundMany")) {
            return metadata(QStringLiteral("100000 push, 70000 pop, затем push 100000..179999"), QStringLiteral("последующие pop() возвращают 70000..179999 за лимит времени"));
        }
        if (caseName == QStringLiteral("FrontMany")) {
            return metadata(QStringLiteral("10000 push, затем 200000 вызовов front()"), QStringLiteral("front() работает быстро и не удаляет элементы"));
        }
    }

    if (suiteName.startsWith(QStringLiteral("Deque_"))) {
        if (caseName == QStringLiteral("StartsEmpty")) {
            return metadata(QStringLiteral("ArrayDeque deque; вызовы empty(), size()"), QStringLiteral("empty() == true, size() == 0"));
        }
        if (caseName == QStringLiteral("PushBackAndFront")) {
            return metadata(QStringLiteral("pushFront(1), pushBack(2)"), QStringLiteral("front() == 1, back() == 2"));
        }
        if (caseName == QStringLiteral("PopFront")) {
            return metadata(QStringLiteral("pushBack(1), pushBack(2), popFront() два раза"), QStringLiteral("возвращается 1, затем 2"));
        }
        if (caseName == QStringLiteral("PopBack")) {
            return metadata(QStringLiteral("pushBack(1), pushBack(2), popBack() два раза"), QStringLiteral("возвращается 2, затем 1"));
        }
        if (caseName == QStringLiteral("MixedEnds")) {
            return metadata(QStringLiteral("pushFront(1), pushBack(2), pushBack(3), затем pop с разных концов"), QStringLiteral("возвращается 1, затем 3, затем 2"));
        }
        if (caseName == QStringLiteral("FrontBackDoNotRemove")) {
            return metadata(QStringLiteral("pushBack(4), pushBack(5), вызовы front()/back()"), QStringLiteral("front() == 4, back() == 5, size() == 2"));
        }
        if (caseName == QStringLiteral("HandlesNegativeValues")) {
            return metadata(QStringLiteral("pushFront(-1), pushBack(-2)"), QStringLiteral("popFront() == -1, popBack() == -2"));
        }
        if (caseName == QStringLiteral("BecomesEmptyAfterPops")) {
            return metadata(QStringLiteral("добавить два элемента и удалить оба"), QStringLiteral("после удаления empty() == true"));
        }
        if (caseName == QStringLiteral("MaintainsBackOrder")) {
            return metadata(QStringLiteral("pushBack(0..9), затем popFront()"), QStringLiteral("возвращается 0..9"));
        }
        if (caseName == QStringLiteral("MaintainsFrontOrder")) {
            return metadata(QStringLiteral("pushFront(0..9), затем popFront()"), QStringLiteral("возвращается 9..0"));
        }
        if (caseName == QStringLiteral("GrowsBeyondInitialCapacity")) {
            return metadata(QStringLiteral("150 добавлений через pushFront/pushBack"), QStringLiteral("size() == 150"));
        }
        if (caseName == QStringLiteral("WrapAroundBothEnds")) {
            return metadata(QStringLiteral("pushBack 0..79, popFront 0..39, pushBack 80..119"), QStringLiteral("front() == 40, back() == 119"));
        }
        if (caseName == QStringLiteral("ReusesAfterEmptying")) {
            return metadata(QStringLiteral("pushFront 30 элементов, очистка, затем pushFront(10), pushBack(20)"), QStringLiteral("front() == 10, back() == 20"));
        }
        if (caseName == QStringLiteral("AlternatingOppositeEnds")) {
            return metadata(QStringLiteral("60 циклов pushFront(i), pushBack(-i)"), QStringLiteral("popFront() возвращает 59..0, popBack() возвращает -59..0"));
        }
        if (caseName == QStringLiteral("SizeAfterMixedOperations")) {
            return metadata(QStringLiteral("pushBack 0..99, 25 popFront и 25 popBack"), QStringLiteral("size() == 50, front() == 25, back() == 74"));
        }
        if (caseName == QStringLiteral("PushBackMany")) {
            return metadata(QStringLiteral("220000 вызовов pushBack(i)"), QStringLiteral("size() == 220000 за лимит времени"));
        }
        if (caseName == QStringLiteral("PushFrontMany")) {
            return metadata(QStringLiteral("120000 вызовов pushFront(i)"), QStringLiteral("size() == 120000 за лимит времени"));
        }
        if (caseName == QStringLiteral("PopBothEndsMany")) {
            return metadata(QStringLiteral("180000 pushBack, затем popFront и popBack"), QStringLiteral("дек становится пустым за лимит времени"));
        }
        if (caseName == QStringLiteral("AlternatingMany")) {
            return metadata(QStringLiteral("120000 циклов pushFront(i), pushBack(-i), popFront(), popBack()"), QStringLiteral("значения возвращаются с правильных концов за лимит времени"));
        }
        if (caseName == QStringLiteral("WrapAroundMany")) {
            return metadata(QStringLiteral("80000 pushBack, 60000 popFront, 80000 pushFront"), QStringLiteral("size() == 100000 за лимит времени"));
        }
    }

    if (suiteName.startsWith(QStringLiteral("LinkedList_"))) {
        if (caseName == QStringLiteral("StartsEmpty")) {
            return metadata(QStringLiteral("LinkedList list; вызовы empty(), size()"), QStringLiteral("empty() == true, size() == 0"));
        }
        if (caseName == QStringLiteral("PushBack")) {
            return metadata(QStringLiteral("pushBack(1), pushBack(2), get(0), get(1)"), QStringLiteral("get(0) == 1, get(1) == 2"));
        }
        if (caseName == QStringLiteral("PushFront")) {
            return metadata(QStringLiteral("pushFront(1), pushFront(2)"), QStringLiteral("список хранит [2, 1]"));
        }
        if (caseName == QStringLiteral("InsertMiddle")) {
            return metadata(QStringLiteral("pushBack(1), pushBack(3), insert(1, 2)"), QStringLiteral("список хранит [1, 2, 3]"));
        }
        if (caseName == QStringLiteral("RemoveFirst")) {
            return metadata(QStringLiteral("pushBack(1), pushBack(2), removeAt(0)"), QStringLiteral("removeAt(0) == 1, первым становится 2"));
        }
        if (caseName == QStringLiteral("RemoveLast")) {
            return metadata(QStringLiteral("pushBack(1), pushBack(2), removeAt(1)"), QStringLiteral("removeAt(1) == 2, size() == 1"));
        }
        if (caseName == QStringLiteral("RemoveMiddle")) {
            return metadata(QStringLiteral("pushBack(1), pushBack(2), pushBack(3), removeAt(1)"), QStringLiteral("removeAt(1) == 2, список [1, 3]"));
        }
        if (caseName == QStringLiteral("HandlesNegativeValues")) {
            return metadata(QStringLiteral("pushBack(-1), pushBack(-2)"), QStringLiteral("get(0) == -1, get(1) == -2"));
        }
        if (caseName == QStringLiteral("BecomesEmptyAfterRemove")) {
            return metadata(QStringLiteral("pushBack(5), removeAt(0)"), QStringLiteral("removeAt(0) == 5, список пуст"));
        }
        if (caseName == QStringLiteral("MaintainsOrderForTenValues")) {
            return metadata(QStringLiteral("pushBack(0..9), затем get(0..9)"), QStringLiteral("get(i) == i"));
        }
        if (caseName == QStringLiteral("InsertAtBeginning")) {
            return metadata(QStringLiteral("pushBack(2), insert(0, 1)"), QStringLiteral("список хранит [1, 2]"));
        }
        if (caseName == QStringLiteral("InsertAtEnd")) {
            return metadata(QStringLiteral("pushBack(1), insert(1, 2)"), QStringLiteral("список хранит [1, 2], size() == 2"));
        }
        if (caseName == QStringLiteral("ManyHeadOperations")) {
            return metadata(QStringLiteral("100 pushFront, затем 100 removeAt(0)"), QStringLiteral("значения возвращаются 99..0"));
        }
        if (caseName == QStringLiteral("TailUpdatesAfterRemoveLast")) {
            return metadata(QStringLiteral("pushBack(1), pushBack(2), removeAt(1), pushBack(3)"), QStringLiteral("get(1) == 3"));
        }
        if (caseName == QStringLiteral("ComplexMixedOperations")) {
            return metadata(QStringLiteral("pushBack(1), pushBack(4), insert(1,2), insert(2,3), removeAt(1)"), QStringLiteral("после удаления список [1, 3, 4]"));
        }
        if (caseName == QStringLiteral("PushFrontMany")) {
            return metadata(QStringLiteral("120000 вызовов pushFront(i)"), QStringLiteral("size() == 120000 за лимит времени"));
        }
        if (caseName == QStringLiteral("PushBackMany")) {
            return metadata(QStringLiteral("80000 вызовов pushBack(i)"), QStringLiteral("size() == 80000 за лимит времени"));
        }
        if (caseName == QStringLiteral("RemoveHeadMany")) {
            return metadata(QStringLiteral("80000 pushBack, затем 80000 removeAt(0)"), QStringLiteral("removeAt(0) возвращает 0..79999 за лимит времени"));
        }
        if (caseName == QStringLiteral("AlternatingHeadMany")) {
            return metadata(QStringLiteral("100000 циклов pushFront(i), removeAt(0)"), QStringLiteral("removeAt(0) возвращает i за лимит времени"));
        }
        if (caseName == QStringLiteral("SequentialGetModerate")) {
            return metadata(QStringLiteral("6000 pushBack, затем get(0..5999)"), QStringLiteral("сумма значений равна 17997000"));
        }
    }

    if (suiteName.startsWith(QStringLiteral("BinarySearchTree_"))) {
        if (caseName == QStringLiteral("StartsEmpty")) {
            return metadata(QStringLiteral("BinarySearchTree tree; вызовы empty(), size()"), QStringLiteral("empty() == true, size() == 0"));
        }
        if (caseName == QStringLiteral("InsertAndContains")) {
            return metadata(QStringLiteral("insert(5), insert(3), insert(7), затем contains"), QStringLiteral("contains(5), contains(3), contains(7) == true"));
        }
        if (caseName == QStringLiteral("DoesNotContainMissing")) {
            return metadata(QStringLiteral("insert(5), проверка contains(4)"), QStringLiteral("contains(4) == false"));
        }
        if (caseName == QStringLiteral("MinMax")) {
            return metadata(QStringLiteral("insert: 5,3,8,2"), QStringLiteral("min() == 2, max() == 8"));
        }
        if (caseName == QStringLiteral("InorderSorted")) {
            return metadata(QStringLiteral("insert: 5,2,8,1,3"), QStringLiteral("inorder() возвращает [1,2,3,5,8]"));
        }
        if (caseName == QStringLiteral("RemoveLeaf")) {
            return metadata(QStringLiteral("insert 5,3,7; remove(3)"), QStringLiteral("contains(3) == false, size() == 2"));
        }
        if (caseName == QStringLiteral("RemoveNodeWithOneChild")) {
            return metadata(QStringLiteral("дерево с узлом, у которого один потомок; remove этого узла"), QStringLiteral("потомок остается доступен через contains"));
        }
        if (caseName == QStringLiteral("RemoveNodeWithTwoChildren")) {
            return metadata(QStringLiteral("дерево с корнем/узлом с двумя потомками; remove"), QStringLiteral("inorder() остается отсортированным без удаленного значения"));
        }
        if (caseName == QStringLiteral("HandlesNegativeValues")) {
            return metadata(QStringLiteral("insert отрицательных и положительных значений"), QStringLiteral("min() корректно находит отрицательный минимум, contains работает"));
        }
        if (caseName == QStringLiteral("SizeAfterOperations")) {
            return metadata(QStringLiteral("несколько insert/remove"), QStringLiteral("size() отражает фактическое количество узлов"));
        }
        if (caseName == QStringLiteral("DuplicateValuesIgnored")) {
            return metadata(QStringLiteral("insert(5), insert(5), insert(5)"), QStringLiteral("дубликаты игнорируются: size() == 1, inorder() == [5]"));
        }
        if (caseName == QStringLiteral("RemoveRootWithTwoChildren")) {
            return metadata(QStringLiteral("удаление корня с двумя потомками"), QStringLiteral("новое дерево сохраняет BST-инвариант и отсортированный inorder"));
        }
        if (caseName == QStringLiteral("RemoveMissingDoesNotChangeTree")) {
            return metadata(QStringLiteral("remove значения, которого нет в дереве"), QStringLiteral("дерево и size() не меняются"));
        }
        if (caseName == QStringLiteral("ManyRandomValuesSortedInorder")) {
            return metadata(QStringLiteral("много значений в случайном порядке"), QStringLiteral("inorder() возвращает отсортированный набор"));
        }
        if (caseName == QStringLiteral("RemoveManyKeepsSearchValid")) {
            return metadata(QStringLiteral("много insert, затем серия remove"), QStringLiteral("contains и inorder остаются корректными"));
        }
        if (caseName == QStringLiteral("InsertRandomMany")) {
            return metadata(QStringLiteral("45000 случайных insert в BST"), QStringLiteral("size() == 45000 за лимит времени"));
        }
        if (caseName == QStringLiteral("ContainsRandomMany")) {
            return metadata(QStringLiteral("45000 insert, затем много contains"), QStringLiteral("поиск работает корректно и быстро"));
        }
        if (caseName == QStringLiteral("InorderMany")) {
            return metadata(QStringLiteral("45000 insert, затем inorder()"), QStringLiteral("inorder() возвращает 45000 отсортированных значений за лимит времени"));
        }
        if (caseName == QStringLiteral("RemoveRandomMany")) {
            return metadata(QStringLiteral("40000 insert, затем 20000 remove"), QStringLiteral("size() == 20000 за лимит времени"));
        }
        if (caseName == QStringLiteral("MixedOperationsMany")) {
            return metadata(QStringLiteral("смешанные insert/contains/remove"), QStringLiteral("BST сохраняет корректность и укладывается в лимит времени"));
        }
    }

    if (caseName == QStringLiteral("BubbleSort_Small")) {
        return {QStringLiteral("[5,2,9,1,5,6,3,8,4,7]"), QStringLiteral("[1,2,3,4,5,5,6,7,8,9]")};
    }
    if (caseName == QStringLiteral("SelectionSort_Small")) {
        return {QStringLiteral("[5,2,9,1,5,6,3,8,4,7]"), QStringLiteral("[1,2,3,4,5,5,6,7,8,9]")};
    }
    if (caseName == QStringLiteral("InsertionSort_Small")) {
        return {QStringLiteral("[5,2,9,1,5,6,3,8,4,7]"), QStringLiteral("[1,2,3,4,5,5,6,7,8,9]")};
    }
    if (caseName == QStringLiteral("MergeSort_Small")) {
        return {QStringLiteral("[5,2,9,1,5,6,3,8,4,7]"), QStringLiteral("[1,2,3,4,5,5,6,7,8,9]")};
    }
    if (caseName == QStringLiteral("QuickSort_Small")) {
        return {QStringLiteral("[5,2,9,1,5,6,3,8,4,7]"), QStringLiteral("[1,2,3,4,5,5,6,7,8,9]")};
    }
    if (caseName == QStringLiteral("HeapSort_Small")) {
        return {QStringLiteral("[5,2,9,1,5,6,3,8,4,7]"), QStringLiteral("[1,2,3,4,5,5,6,7,8,9]")};
    }
    if (caseName == QStringLiteral("DotAsCharInIntArray")) {
        return {QStringLiteral("[5,2,9,'.',5,6,3,8,4,7]"), QStringLiteral("[2,3,4,5,5,6,7,8,9,46]")};
    }
    if (caseName == QStringLiteral("Empty")) {
        return {QStringLiteral("пустой массив"), QStringLiteral("корректная обработка без падения")};
    }
    if (caseName == QStringLiteral("SingleElement")) {
        return {QStringLiteral("[42]"), QStringLiteral("[42]")};
    }
    if (caseName == QStringLiteral("Sorted")) {
        return {QStringLiteral("[1,2,3,4,5]"), QStringLiteral("[1,2,3,4,5]")};
    }
    if (caseName == QStringLiteral("RevSorted")) {
        return {QStringLiteral("[5,4,3,2,1]"), QStringLiteral("[1,2,3,4,5]")};
    }
    if (caseName == QStringLiteral("AllSame")) {
        return {QStringLiteral("[7,7,7,7,7]"), QStringLiteral("[7,7,7,7,7]")};
    }
    if (caseName == QStringLiteral("Negatives")) {
        return {QStringLiteral("[10,-5,0,23,-100,50]"), QStringLiteral("[-100,-5,0,10,23,50]")};
    }
    if (caseName == QStringLiteral("BubbleSort_Random100")) {
        return {QStringLiteral("100 случайных чисел (seed=42)"), QStringLiteral("массив совпадает с std::sort")};
    }
    if (caseName.startsWith(QStringLiteral("SortsCorrectly/"))) {
        return {QStringLiteral("[64,34,25,12,22,11,90]"), QStringLiteral("[11,12,22,25,34,64,90]")};
    }
    if (caseName == QStringLiteral("BasicStrings")) {
        return {QStringLiteral("[banana, apple, cherry, apricot]"), QStringLiteral("[apple, apricot, banana, cherry]")};
    }
    if (caseName == QStringLiteral("CompareFunction")) {
        return {QStringLiteral("пары строк для сравнения"), QStringLiteral("лексикографический порядок true/false")};
    }

    return {
        QStringLiteral("См. входные данные в sort_test.cpp"),
        QStringLiteral("См. ожидаемое поведение в sort_test.cpp")
    };
}

std::vector<labtester::domain::TestCaseResult> parseGTestReport(
    const QString &xmlPath,
    QString *errorMessage
)
{
    std::vector<labtester::domain::TestCaseResult> parsedCases;
    QFile file(xmlPath);
    if (!file.exists()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("XML-отчет gtest не найден.");
        }
        return parsedCases;
    }
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Не удалось открыть XML-отчет: %1").arg(file.errorString());
        }
        return parsedCases;
    }

    QXmlStreamReader xml(&file);
    QString currentSuiteName;

    while (!xml.atEnd()) {
        xml.readNext();

        if (!xml.isStartElement()) {
            continue;
        }

        if (xml.name() == QStringLiteral("testsuite")) {
            currentSuiteName = xml.attributes().value(QStringLiteral("name")).toString();
            continue;
        }

        if (xml.name() != QStringLiteral("testcase")) {
            continue;
        }

        const auto attributes = xml.attributes();
        const QString caseName = attributes.value(QStringLiteral("name")).toString();
        const QString className = attributes.value(QStringLiteral("classname")).toString();
        QString fullName = caseName;
        if (!className.isEmpty()) {
            fullName = className + QStringLiteral(".") + caseName;
        } else if (!currentSuiteName.isEmpty()) {
            fullName = currentSuiteName + QStringLiteral(".") + caseName;
        }

        const double durationSeconds = attributes.value(QStringLiteral("time")).toString().toDouble();
        qint64 durationMs = static_cast<qint64>(durationSeconds * 1000.0 + 0.5);
        QString inputData = attributes.value(QStringLiteral("input_data")).toString();
        QString expectedOutput = attributes.value(QStringLiteral("expected_output")).toString();
        QString actualOutput = attributes.value(QStringLiteral("actual_output")).toString();
        QString durationMsOverride;

        bool passed = true;
        QString failureMessage;
        QString failureDetails;

        while (!(xml.isEndElement() && xml.name() == QStringLiteral("testcase")) && !xml.atEnd()) {
            xml.readNext();
            if (!xml.isStartElement()) {
                continue;
            }

            if (xml.name() == QStringLiteral("failure")) {
                passed = false;
                if (failureMessage.isEmpty()) {
                    failureMessage = xml.attributes().value(QStringLiteral("message")).toString().trimmed();
                }
                const QString failureText = xml.readElementText(QXmlStreamReader::IncludeChildElements).trimmed();
                if (!failureText.isEmpty()) {
                    failureDetails = failureText;
                }
            } else if (xml.name() == QStringLiteral("properties")) {
                while (!(xml.isEndElement() && xml.name() == QStringLiteral("properties")) && !xml.atEnd()) {
                    xml.readNext();
                    if (!xml.isStartElement() || xml.name() != QStringLiteral("property")) {
                        continue;
                    }

                    const auto propAttrs = xml.attributes();
                    const QString propName = propAttrs.value(QStringLiteral("name")).toString();
                    const QString propValue = propAttrs.value(QStringLiteral("value")).toString();

                    if (propName == QStringLiteral("input_data")) {
                        inputData = propValue;
                    } else if (propName == QStringLiteral("expected_output")) {
                        expectedOutput = propValue;
                    } else if (propName == QStringLiteral("actual_output")) {
                        actualOutput = propValue;
                    } else if (propName == QStringLiteral("duration_ms")) {
                        durationMsOverride = propValue;
                    }

                    xml.skipCurrentElement();
                }
            } else if (xml.name() == QStringLiteral("skipped")) {
                passed = false;
                if (failureMessage.isEmpty()) {
                    failureMessage = QStringLiteral("Тест пропущен.");
                }
                const QString skippedText = xml.readElementText(QXmlStreamReader::IncludeChildElements).trimmed();
                if (!skippedText.isEmpty()) {
                    failureDetails = skippedText;
                }
            }
        }

        const CaseMetadata metadata = metadataForCase(fullName);
        if (inputData.isEmpty()) {
            inputData = metadata.inputData;
        }
        if (expectedOutput.isEmpty()) {
            expectedOutput = metadata.expectedOutput;
        }
        if (!durationMsOverride.isEmpty()) {
            bool ok = false;
            const qint64 overriddenDurationMs = durationMsOverride.toLongLong(&ok);
            if (ok && overriddenDurationMs >= 0) {
                durationMs = overriddenDurationMs;
            }
        }
        const QString detailedFailure = failureDetails.trimmed().isEmpty()
            ? failureMessage.trimmed()
            : failureDetails.trimmed();
        const QString visibleFailure = visibleFailureLine(detailedFailure);
        if (!passed && containsTimeoutText(visibleFailure)) {
            if (expectedOutput.isEmpty() || expectedOutput == metadata.expectedOutput) {
                expectedOutput = QStringLiteral("Алгоритм должен завершиться в пределах лимита времени.");
            }
            if (actualOutput.isEmpty()) {
                actualOutput = visibleFailure;
            }
            const qint64 visibleLimitMs = durationLimitFromText(visibleFailure);
            if (durationMsOverride.isEmpty() && visibleLimitMs >= 0) {
                durationMs = visibleLimitMs;
            }
        }
        if (actualOutput.isEmpty()) {
            actualOutput = passed
                ? expectedOutput
                : (detailedFailure.isEmpty()
                    ? QStringLiteral("Нет данных о фактическом выходе.")
                    : detailedFailure);
        }

        labtester::domain::TestCaseResult testCase;
        testCase.testName = fullName;
        testCase.passed = passed;
        testCase.message = passed
            ? QStringLiteral("Тест пройден.")
            : (failureMessage.isEmpty() ? QStringLiteral("Тест завершился ошибкой.") : failureMessage);
        testCase.inputData = inputData;
        testCase.expectedOutput = expectedOutput;
        testCase.actualOutput = actualOutput;
        testCase.failureDetails = detailedFailure;
        testCase.durationMs = durationMs;
        parsedCases.push_back(testCase);
    }

    if (xml.hasError()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Ошибка разбора XML-отчета: %1").arg(xml.errorString());
        }
        return {};
    }

    return parsedCases;
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
    errorCase.testName = QStringLiteral("Подготовка/сборка");
    errorCase.passed = false;
    errorCase.message = message;
    errorCase.inputData = QStringLiteral("Исходный файл: %1").arg(submission.sourcePath);
    errorCase.expectedOutput.clear();
    errorCase.actualOutput = details.isEmpty()
        ? message
        : details;
    errorCase.failureDetails = details;
    errorCase.durationMs = 0;
    result.testCases = {errorCase};

    return result;
}

} // namespace

namespace labtester::runners {

domain::TestRunResult CppGTestRunner::run(const domain::Submission &submission) const
{
    QFileInfo sourceInfo(submission.sourcePath);
    if (!sourceInfo.exists() || !sourceInfo.isFile()) {
        return makeBuildErrorResult(
            submission,
            QStringLiteral("Файл работы не найден."),
            QStringLiteral("Путь к файлу недоступен: %1\n"
                           "Возможная причина: работа была импортирована/синхронизирована с другого устройства, "
                           "и локальный путь больше не существует. Удалите такую работу и импортируйте файл заново.")
                .arg(submission.sourcePath)
        );
    }

    const UTestLayout utestLayout = resolveUTestLayout(submission);
    if (utestLayout.suitePath.isEmpty() || utestLayout.headerPath.isEmpty()) {
        return makeBuildErrorResult(
            submission,
            QStringLiteral("Тестовый набор не найден."),
            QStringLiteral("Ожидались файлы sort_test.cpp и cc.h для выбранной лабораторной.")
        );
    }

    const QString cmakeExecutable = findPreferredCMakeExecutable();
    if (cmakeExecutable.isEmpty()) {
        return makeBuildErrorResult(
            submission,
            QStringLiteral("cmake не найден в PATH."),
            QStringLiteral("Добавьте cmake в PATH и перезапустите приложение.")
        );
    }

    const QString vcvarsPath = findVcVars64Bat();

    const QString sourceIdentity = utestLayout.cmakeSourceDir.isEmpty()
        ? (utestLayout.suitePath
            + QStringLiteral("|")
            + utestLayout.headerPath
            + QStringLiteral("|")
            + sourceInfo.absoluteFilePath())
        : (utestLayout.cmakeSourceDir + QStringLiteral("|") + sourceInfo.absoluteFilePath());
    const QString buildDir = resolveBuildDirectory(sourceIdentity);
    QDir().mkpath(buildDir);

    QString cmakeSourceDir = utestLayout.cmakeSourceDir;
    if (cmakeSourceDir.isEmpty()) {
        QString generationError;
        cmakeSourceDir = ensureStandaloneCMakeSource(utestLayout, buildDir, &generationError);
        if (cmakeSourceDir.isEmpty()) {
            return makeBuildErrorResult(
                submission,
                QStringLiteral("Не удалось подготовить проект тестов."),
                generationError
            );
        }
    }

    QStringList configureArgs = {
        QStringLiteral("-S"), QDir::toNativeSeparators(cmakeSourceDir),
        QStringLiteral("-B"), QDir::toNativeSeparators(buildDir),
        QStringLiteral("-DLABTESTER_STUDENT_SOURCE=%1").arg(
            escapedPathForCmake(sourceInfo.absoluteFilePath())
        ),
        QStringLiteral("-DCMAKE_BUILD_TYPE=Debug")
    };

    const bool hasCache = QFileInfo::exists(QDir(buildDir).filePath(QStringLiteral("CMakeCache.txt")));
    if (!hasCache) {
        const QString ninjaExecutable = findExecutable(QStringLiteral("ninja"));
        if (!ninjaExecutable.isEmpty()) {
            configureArgs << QStringLiteral("-G")
                          << QStringLiteral("Ninja")
                          << QStringLiteral("-DCMAKE_MAKE_PROGRAM=%1")
                                 .arg(escapedPathForCmake(ninjaExecutable));
        }
    }

    const ProcessRunResult configureRun = runProcessWithVcVars(
        cmakeExecutable,
        configureArgs,
        buildDir,
        180000,
        vcvarsPath
    );
    if (configureRun.interrupted) {
        return makeBuildErrorResult(
            submission,
            QStringLiteral("Проверка отменена пользователем."),
            tailText(configureRun.stdErr + QStringLiteral("\n") + configureRun.stdOut)
        );
    }
    if (!configureRun.started || configureRun.exitCode != 0) {
        return makeBuildErrorResult(
            submission,
            QStringLiteral("Не удалось сконфигурировать проект тестов."),
            tailText(configureRun.stdErr + QStringLiteral("\n") + configureRun.stdOut
                + QStringLiteral("\n") + configureRun.errorText)
        );
    }

    const ProcessRunResult buildRun = runProcessWithVcVars(
        cmakeExecutable,
        {
            QStringLiteral("--build"),
            QDir::toNativeSeparators(buildDir),
            QStringLiteral("--config"),
            QStringLiteral("Debug")
        },
        buildDir,
        240000,
        vcvarsPath
    );
    if (buildRun.interrupted) {
        return makeBuildErrorResult(
            submission,
            QStringLiteral("Проверка отменена пользователем."),
            tailText(buildRun.stdErr + QStringLiteral("\n") + buildRun.stdOut)
        );
    }
    if (!buildRun.started || buildRun.exitCode != 0) {
        return makeBuildErrorResult(
            submission,
            QStringLiteral("Сборка тестов завершилась с ошибкой."),
            tailText(buildRun.stdErr + QStringLiteral("\n") + buildRun.stdOut
                + QStringLiteral("\n") + buildRun.errorText)
        );
    }

    const QString testExecutable = resolveBuiltExecutable(buildDir);
    if (testExecutable.isEmpty()) {
        return makeBuildErrorResult(
            submission,
            QStringLiteral("Собранный исполняемый файл тестов не найден."),
            QStringLiteral("Ожидался sort_test(.exe) в каталоге: %1").arg(buildDir)
        );
    }

    const QString reportDir = createReportDirectory(submission.id);
    const QString reportPath = QDir(reportDir).filePath(QStringLiteral("gtest_report.xml"));

    const ProcessRunResult testRun = runProcessWithVcVars(
        testExecutable,
        {
            QStringLiteral("--gtest_color=no"),
            QStringLiteral("--gtest_output=xml:%1").arg(QDir::toNativeSeparators(reportPath))
        },
        buildDir,
        180000,
        vcvarsPath
    );
    if (testRun.interrupted) {
        return makeBuildErrorResult(
            submission,
            QStringLiteral("Проверка отменена пользователем."),
            tailText(testRun.stdErr + QStringLiteral("\n") + testRun.stdOut)
        );
    }
    if (!testRun.started) {
        return makeBuildErrorResult(
            submission,
            QStringLiteral("Не удалось запустить тесты."),
            tailText(testRun.errorText + QStringLiteral("\n")
                + testRun.stdErr + QStringLiteral("\n")
                + testRun.stdOut)
        );
    }

    QString parseError;
    std::vector<domain::TestCaseResult> testCases = parseGTestReport(reportPath, &parseError);
    if (testCases.empty()) {
        return makeBuildErrorResult(
            submission,
            QStringLiteral("Не удалось получить результаты тестов."),
            tailText(parseError + QStringLiteral("\n")
                + testRun.stdErr + QStringLiteral("\n")
                + testRun.stdOut)
        );
    }

    domain::TestRunResult result;
    result.submissionId = submission.id;
    result.studentName = submission.student.name;
    result.labTitle = submission.labWork.title;
    result.executedAt = QDateTime::currentDateTime();
    result.testCases = std::move(testCases);
    result.totalTests = static_cast<int>(result.testCases.size());

    int passedTests = 0;
    for (const auto &testCase : result.testCases) {
        if (testCase.passed) {
            ++passedTests;
        }
    }
    result.passedTests = passedTests;
    result.failedTests = result.totalTests - result.passedTests;
    result.status = result.failedTests == 0
        ? domain::ExecutionStatus::Passed
        : domain::ExecutionStatus::Failed;

    result.message = QStringLiteral("Тесты: %1/%2").arg(result.passedTests).arg(result.totalTests);
    return result;
}

} // namespace labtester::runners
