#include "app/ApplicationController.h"

#include <algorithm>
#include <memory>
#include <utility>

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMetaType>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QThread>
#include <QUuid>
#include <QUrl>
#include <QVariantMap>

#include "app/TestRunWorker.h"
#include "database/DatabaseManager.h"
#include "services/CloudSyncService.h"
#include "services/ExecutionSettingsService.h"
#include "services/SubmissionImportService.h"
#include "services/SubmissionService.h"
#include "services/TestExecutionService.h"

namespace labtester::app {

namespace {

struct CloudTaskResult {
    bool success {false};
    QString message;
    QVariantMap info;
    QVariantList history;
};

struct CaseDisplayMetadata {
    QString input;
    QString expected;
};

bool isPlaceholderMetadata(const QString &value)
{
    const QString normalized = value.trimmed();
    return normalized.isEmpty()
        || normalized.contains(QStringLiteral("_test.cpp"), Qt::CaseInsensitive)
        || normalized.contains(QStringLiteral("ожидаемое поведение"), Qt::CaseInsensitive);
}

CaseDisplayMetadata structureCaseMetadata(const QString &testName)
{
    const QString suiteName = testName.section('.', 0, 0).section('/', -1);
    const QString caseName = testName.section('.', -1);

    QString structureName;
    QString testedMethods;
    if (suiteName.startsWith(QStringLiteral("Stack_"))) {
        structureName = QStringLiteral("ArrayStack");
        testedMethods = QStringLiteral("push, pop, top, size, empty");
    } else if (suiteName.startsWith(QStringLiteral("Queue_"))) {
        structureName = QStringLiteral("ArrayQueue");
        testedMethods = QStringLiteral("push, pop, front, size, empty");
    } else if (suiteName.startsWith(QStringLiteral("Deque_"))) {
        structureName = QStringLiteral("ArrayDeque");
        testedMethods = QStringLiteral("pushBack, pushFront, popBack, popFront, front, back, size, empty");
    } else if (suiteName.startsWith(QStringLiteral("LinkedList_"))) {
        structureName = QStringLiteral("LinkedList");
        testedMethods = QStringLiteral("pushBack, pushFront, insert, removeAt, get, size, empty");
    } else if (suiteName.startsWith(QStringLiteral("BinarySearchTree_"))) {
        structureName = QStringLiteral("BinarySearchTree");
        testedMethods = QStringLiteral("insert, contains, remove, min, max, inorder, size, empty");
    } else {
        return {};
    }

    const QString tier = suiteName.section('_', -1);
    QString input = QStringLiteral("Структура: %1\nПроверяемые методы: %2\nСценарий: %3.")
        .arg(structureName, testedMethods, caseName);
    QString expected = QStringLiteral("Методы структуры должны вернуть значения, заданные сценарием `%1`, и сохранить инварианты структуры.")
        .arg(caseName);

    if (caseName == QStringLiteral("StartsEmpty")) {
        input = QStringLiteral("Создается пустая структура %1; вызываются empty() и size().").arg(structureName);
        expected = QStringLiteral("empty() == true, size() == 0.");
    } else if (caseName == QStringLiteral("GrowsBeyondInitialCapacity")) {
        input = QStringLiteral("%1 получает больше элементов, чем помещается в маленький начальный буфер.").arg(structureName);
        expected = QStringLiteral("Структура расширяется, size() показывает полное количество элементов, порядок не ломается.");
    } else if (caseName == QStringLiteral("WrapAroundAfterPops")) {
        input = QStringLiteral("ArrayQueue: push 0..79, pop 0..49, затем push 80..139.");
        expected = QStringLiteral("Очередь использует кольцевой буфер: последующие pop() возвращают 50..139.");
    } else if (caseName == QStringLiteral("WrapAroundBothEnds")) {
        input = QStringLiteral("ArrayDeque: pushBack 0..79, popFront 0..39, затем pushBack 80..119.");
        expected = QStringLiteral("front() == 40, back() == 119, данные не теряются после смещения границ буфера.");
    } else if (caseName == QStringLiteral("AlternatingOppositeEnds")) {
        input = QStringLiteral("ArrayDeque: 60 циклов pushFront(i) и pushBack(-i), затем чтение с обоих концов.");
        expected = QStringLiteral("popFront() возвращает 59..0, popBack() возвращает -59..0.");
    } else if (caseName == QStringLiteral("InsertAtBeginning")) {
        input = QStringLiteral("LinkedList: pushBack(2), insert(0, 1), затем get(0), get(1).");
        expected = QStringLiteral("Список становится [1, 2].");
    } else if (caseName == QStringLiteral("InsertAtEnd")) {
        input = QStringLiteral("LinkedList: pushBack(1), insert(1, 2), затем get(1), size().");
        expected = QStringLiteral("Список становится [1, 2], size() == 2.");
    } else if (caseName == QStringLiteral("DuplicateValuesIgnored")) {
        input = QStringLiteral("BinarySearchTree: insert(5), insert(5), insert(5), затем size() и inorder().");
        expected = QStringLiteral("Дубликаты игнорируются: size() == 1, inorder() == [5].");
    } else if (caseName == QStringLiteral("PushMany")) {
        input = QStringLiteral("%1: много операций добавления в performance-тесте уровня %2.").arg(structureName, tier);
        expected = QStringLiteral("Все элементы добавлены, size() корректный, время не превышает лимит теста.");
    } else if (caseName == QStringLiteral("PopMany") || caseName == QStringLiteral("RemoveHeadMany")) {
        input = QStringLiteral("%1: много добавлений, затем массовое удаление/извлечение.").arg(structureName);
        expected = QStringLiteral("Элементы возвращаются в правильном порядке, операция укладывается в лимит времени.");
    } else if (caseName == QStringLiteral("AlternatingMany") || caseName == QStringLiteral("AlternatingHeadMany")) {
        input = QStringLiteral("%1: много циклов добавления и немедленного удаления.").arg(structureName);
        expected = QStringLiteral("Каждое удаление возвращает только что ожидаемое значение, без накопления ошибок и за лимит времени.");
    } else if (caseName == QStringLiteral("GrowthWaves")) {
        input = QStringLiteral("ArrayStack: 20 волн, в каждой 12000 push и 6000 pop.");
        expected = QStringLiteral("Стек расширяется, не теряет старые элементы и не обращается к пустому буферу.");
    } else if (caseName == QStringLiteral("RemoveRootWithTwoChildren")) {
        input = QStringLiteral("BinarySearchTree: удаление корня, у которого есть левый и правый потомок.");
        expected = QStringLiteral("BST-инвариант сохраняется, inorder() остается отсортированным.");
    } else if (caseName == QStringLiteral("ManyRandomValuesSortedInorder") || caseName == QStringLiteral("InorderMany")) {
        input = QStringLiteral("BinarySearchTree: вставка большого набора значений, затем inorder().");
        expected = QStringLiteral("inorder() возвращает отсортированную последовательность всех значений.");
    }

    return {input, expected};
}

} // namespace

ApplicationController::ApplicationController(
    services::SubmissionService &submissionService,
    services::SubmissionImportService &submissionImportService,
    services::TestExecutionService &testExecutionService,
    services::CloudSyncService &cloudSyncService,
    services::ExecutionSettingsService &executionSettingsService,
    QObject *parent
)
    : QObject(parent)
    , m_submissionService(submissionService)
    , m_submissionImportService(submissionImportService)
    , m_testExecutionService(testExecutionService)
    , m_cloudSyncService(cloudSyncService)
    , m_executionSettingsService(executionSettingsService)
    , m_submissionsModel(this)
    , m_resultsModel(this)
    , m_historyModel(this)
{
    qRegisterMetaType<domain::TestRunResult>("labtester::domain::TestRunResult");
}

ApplicationController::~ApplicationController()
{
    for (QThread *thread : std::as_const(m_testRunThreads)) {
        if (!thread) {
            continue;
        }
        thread->requestInterruption();
        thread->quit();
        thread->wait();
    }
    if (m_cloudOpThread) {
        m_cloudOpThread->requestInterruption();
        m_cloudOpThread->quit();
        m_cloudOpThread->wait();
    }
}

models::SubmissionListModel *ApplicationController::submissionsModel()
{
    return &m_submissionsModel;
}

models::ResultListModel *ApplicationController::resultsModel()
{
    return &m_resultsModel;
}

models::HistoryListModel *ApplicationController::historyModel()
{
    return &m_historyModel;
}

QVariantList ApplicationController::students() const
{
    return m_students;
}

QVariantList ApplicationController::labWorks() const
{
    return m_labWorks;
}

QVariantList ApplicationController::selectedStudentSubmissions() const
{
    return m_selectedStudentSubmissions;
}

int ApplicationController::selectedStudentId() const
{
    return m_selectedStudentId;
}

QString ApplicationController::selectedStudentName() const
{
    return m_selectedStudentName;
}

int ApplicationController::totalSubmissions() const
{
    return static_cast<int>(m_submissionsModel.submissions().size());
}

int ApplicationController::passedSubmissions() const
{
    return countByStatus(domain::ExecutionStatus::Passed);
}

int ApplicationController::failedSubmissions() const
{
    return countByStatus(domain::ExecutionStatus::Failed)
        + countByStatus(domain::ExecutionStatus::BuildError);
}

int ApplicationController::pendingSubmissions() const
{
    return countByStatus(domain::ExecutionStatus::Pending);
}

QString ApplicationController::lastRunMessage() const
{
    return m_lastRunMessage;
}

bool ApplicationController::testsRunning() const
{
    return m_testsRunning;
}

int ApplicationController::testProgressCurrent() const
{
    return m_testProgressCurrent;
}

int ApplicationController::testProgressTotal() const
{
    return m_testProgressTotal;
}

double ApplicationController::testProgressValue() const
{
    if (m_testProgressTotal <= 0) {
        return 0.0;
    }
    return static_cast<double>(m_testProgressCurrent) / static_cast<double>(m_testProgressTotal);
}

QString ApplicationController::testProgressText() const
{
    return m_testProgressText;
}

QString ApplicationController::cloudUsername() const
{
    return m_cloudSyncService.cloudUsername();
}

QString ApplicationController::cloudPassword() const
{
    return m_cloudSyncService.cloudPassword();
}

bool ApplicationController::cloudAutoSyncEnabled() const
{
    return m_cloudSyncService.autoSyncEnabled();
}

bool ApplicationController::cloudSyncInProgress() const
{
    return m_cloudSyncInProgress;
}

QString ApplicationController::executionMode() const
{
    return m_executionSettingsService.executionMode();
}

QString ApplicationController::remoteExecutionUrl() const
{
    return m_executionSettingsService.remoteEndpoint();
}

QString ApplicationController::remoteExecutionToken() const
{
    return m_executionSettingsService.remoteToken();
}

bool ApplicationController::remoteExecutionNgrokMode() const
{
    return m_executionSettingsService.remoteNgrokMode();
}

void ApplicationController::initialize()
{
    refreshCatalog();
    refreshSubmissions();
    refreshResults();
    refreshHistoryData();
    emit cloudSettingsChanged();
    emit executionSettingsChanged();
}

void ApplicationController::runTests()
{
    if (m_testsRunning) {
        m_lastRunMessage = QStringLiteral("Проверка уже выполняется.");
        emit lastRunMessageChanged();
        return;
    }

    std::vector<domain::Submission> submissions = m_testExecutionService.loadSubmissionsForExecution();
    if (submissions.empty()) {
        m_lastRunMessage = QStringLiteral("Нет работ для проверки.");
        emit lastRunMessageChanged();
        return;
    }

    std::vector<domain::Submission> runnableSubmissions;
    runnableSubmissions.reserve(submissions.size());

    int missingSourceCount = 0;
    for (const auto &submission : submissions) {
        const QFileInfo sourceInfo(submission.sourcePath);
        if (!sourceInfo.exists() || !sourceInfo.isFile()) {
            ++missingSourceCount;
            qWarning().noquote() << QStringLiteral(
                "[EXEC] skip submission=%1 because source file is unavailable: %2"
            ).arg(submission.id).arg(submission.sourcePath);
            continue;
        }
        runnableSubmissions.push_back(submission);
    }

    if (runnableSubmissions.empty()) {
        m_lastRunMessage = missingSourceCount > 0
            ? QStringLiteral("Нет доступных файлов для проверки. Недоступных работ: %1. "
                             "Удалите такие записи и импортируйте файлы заново.")
                  .arg(missingSourceCount)
            : QStringLiteral("Нет работ для проверки.");
        emit lastRunMessageChanged();
        return;
    }

    if (missingSourceCount > 0) {
        m_lastRunMessage = QStringLiteral("Пропущено работ с недоступным исходным файлом: %1. "
                                          "Остальные работы запущены.")
            .arg(missingSourceCount);
        emit lastRunMessageChanged();
    }

    submissions = std::move(runnableSubmissions);

    m_testsRunning = true;
    m_cancelRequested = false;
    m_testProgressCurrent = 0;
    m_testProgressTotal = static_cast<int>(submissions.size());
    m_testProgressText = QStringLiteral("Подготовка проверки...");
    m_expectedTestWorkers = 0;
    m_finishedTestWorkers = 0;
    m_testRunThreads.clear();
    m_testRunWorkers.clear();
    m_lastRunMessage = QStringLiteral("Проверка запущена.");
    emit testsRunningChanged();
    emit testProgressChanged();
    emit lastRunMessageChanged();

    const QString mode = m_executionSettingsService.executionMode();
    if (mode == QString::fromLatin1(services::ExecutionSettingsService::kModeHybrid)) {
        std::vector<domain::Submission> localSubmissions;
        std::vector<domain::Submission> remoteSubmissions;
        localSubmissions.reserve(submissions.size() / 2 + 1);
        remoteSubmissions.reserve(submissions.size() / 2 + 1);

        for (size_t index = 0; index < submissions.size(); ++index) {
            if ((index % 2U) == 0U) {
                localSubmissions.push_back(submissions[index]);
            } else {
                remoteSubmissions.push_back(submissions[index]);
            }
        }

        auto startHybridWorkersForMode = [this](
                                             std::vector<domain::Submission> &&modeSubmissions,
                                             const QString &modeName,
                                             int workerCount
                                         ) {
            if (modeSubmissions.empty()) {
                return;
            }

            const int effectiveWorkerCount = std::max(
                1,
                std::min(workerCount, static_cast<int>(modeSubmissions.size()))
            );
            std::vector<std::vector<domain::Submission>> chunks(
                static_cast<size_t>(effectiveWorkerCount)
            );

            for (size_t index = 0; index < modeSubmissions.size(); ++index) {
                const size_t bucket = index % static_cast<size_t>(effectiveWorkerCount);
                chunks[bucket].push_back(std::move(modeSubmissions[index]));
            }

            for (auto &chunk : chunks) {
                if (!chunk.empty()) {
                    startTestWorker(std::move(chunk), modeName);
                }
            }
        };

        constexpr int kHybridLocalWorkers = 2;
        constexpr int kHybridRemoteWorkers = 2;

        qInfo().noquote() << QStringLiteral(
            "[EXEC][HYBRID] total=%1 local=%2 remote=%3 localWorkers=%4 remoteWorkers=%5"
        ).arg(static_cast<int>(submissions.size()))
         .arg(static_cast<int>(localSubmissions.size()))
         .arg(static_cast<int>(remoteSubmissions.size()))
         .arg(std::min(kHybridLocalWorkers, static_cast<int>(localSubmissions.size())))
         .arg(std::min(kHybridRemoteWorkers, static_cast<int>(remoteSubmissions.size())));

        startHybridWorkersForMode(
            std::move(localSubmissions),
            QString::fromLatin1(services::ExecutionSettingsService::kModeLocal),
            kHybridLocalWorkers
        );
        startHybridWorkersForMode(
            std::move(remoteSubmissions),
            QString::fromLatin1(services::ExecutionSettingsService::kModeServer),
            kHybridRemoteWorkers
        );
    } else {
        startTestWorker(std::move(submissions));
    }

    if (m_expectedTestWorkers <= 0) {
        m_testsRunning = false;
        emit testsRunningChanged();
        m_lastRunMessage = QStringLiteral("Нет работ для запуска в выбранном режиме.");
        emit lastRunMessageChanged();
        resetProgressState();
    }
}

void ApplicationController::cancelTests()
{
    if (!m_testsRunning) {
        m_lastRunMessage = QStringLiteral("Проверка не запущена.");
        emit lastRunMessageChanged();
        return;
    }

    m_cancelRequested = true;
    m_testProgressText = QStringLiteral("Отмена проверки...");
    emit testProgressChanged();

    m_lastRunMessage = QStringLiteral("Запрошена отмена проверки.");
    emit lastRunMessageChanged();

    for (QThread *thread : std::as_const(m_testRunThreads)) {
        if (thread) {
            thread->requestInterruption();
        }
    }
}

void ApplicationController::startTestWorker(std::vector<domain::Submission> submissions, const QString &modeOverride)
{
    if (submissions.empty()) {
        return;
    }

    QThread *thread = new QThread(this);
    TestRunWorker *worker = new TestRunWorker(m_testExecutionService, std::move(submissions), modeOverride);
    worker->moveToThread(thread);

    m_testRunThreads.push_back(thread);
    m_testRunWorkers.push_back(worker);
    ++m_expectedTestWorkers;

    connect(thread, &QThread::started, worker, &TestRunWorker::process);
    connect(worker, &TestRunWorker::progressChanged, this, &ApplicationController::onWorkerProgressChanged);
    connect(worker, &TestRunWorker::submissionFinished, this, &ApplicationController::onWorkerSubmissionFinished);
    connect(worker, &TestRunWorker::finished, this, &ApplicationController::onWorkerFinished);
    connect(worker, &TestRunWorker::finished, thread, &QThread::quit);
    connect(thread, &QThread::finished, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    connect(thread, &QThread::finished, this, [this, thread, worker]() {
        m_testRunWorkers.removeAll(worker);
        m_testRunThreads.removeAll(thread);
    });

    thread->start();
}

void ApplicationController::reloadMockData()
{
    if (m_testsRunning) {
        m_lastRunMessage = QStringLiteral("Дождитесь завершения проверки.");
        emit lastRunMessageChanged();
        return;
    }

    m_submissionService.resetMockSubmissions();
    m_testExecutionService.clearResults();

    refreshCatalog();
    refreshSubmissions();
    refreshResults();
    refreshHistoryData();

    m_lastRunMessage = QStringLiteral("Тестовые данные обновлены");
    emit lastRunMessageChanged();
    emit dashboardChanged();
}

void ApplicationController::clearSubmissions()
{
    if (m_testsRunning) {
        m_lastRunMessage = QStringLiteral("Нельзя очищать список работ во время проверки.");
        emit lastRunMessageChanged();
        return;
    }

    m_submissionService.clearSubmissions();

    refreshSubmissions();
    refreshResults();
    refreshHistoryData();

    m_lastRunMessage = QStringLiteral("Список работ очищен.");
    emit lastRunMessageChanged();
    emit dashboardChanged();
}

void ApplicationController::clearResults()
{
    if (m_testsRunning) {
        m_lastRunMessage = QStringLiteral("Нельзя очищать результаты во время проверки.");
        emit lastRunMessageChanged();
        return;
    }

    m_testExecutionService.clearResults();
    refreshResults();
    refreshHistoryData();

    m_lastRunMessage = QStringLiteral("Список проверок очищен.");
    emit lastRunMessageChanged();
    emit dashboardChanged();
}

void ApplicationController::refreshHistory()
{
    refreshHistoryData();
}

void ApplicationController::applyHistoryFilters(const QString &groupName, int studentId, int labId)
{
    m_historyGroupFilter = groupName.trimmed();
    m_historyStudentFilter = std::max(studentId, 0);
    m_historyLabFilter = std::max(labId, 0);
    refreshHistoryData();
}

void ApplicationController::selectStudent(int studentId)
{
    if (m_selectedStudentId == studentId) {
        return;
    }

    m_selectedStudentId = studentId;
    m_selectedStudentName = findStudentName(studentId);
    refreshSelectedStudentSubmissions();
    emit selectedStudentChanged();
}

bool ApplicationController::addStudent(const QString &name, const QString &groupName)
{
    QString errorMessage;
    int createdStudentId = 0;
    if (!m_submissionService.addStudent(name, groupName, &createdStudentId, &errorMessage)) {
        m_lastRunMessage = errorMessage;
        emit lastRunMessageChanged();
        return false;
    }

    m_students = buildStudentsVariant();
    emit studentsChanged();

    if (createdStudentId > 0) {
        selectStudent(createdStudentId);
    } else {
        refreshSelectedStudentSubmissions();
    }

    m_lastRunMessage = QStringLiteral("Студент добавлен.");
    emit lastRunMessageChanged();
    return true;
}

bool ApplicationController::updateStudent(int studentId, const QString &name, const QString &groupName)
{
    if (m_testsRunning) {
        m_lastRunMessage = QStringLiteral("Нельзя менять студентов во время проверки.");
        emit lastRunMessageChanged();
        return false;
    }

    QString errorMessage;
    if (!m_submissionService.updateStudent(studentId, name, groupName, &errorMessage)) {
        m_lastRunMessage = errorMessage;
        emit lastRunMessageChanged();
        return false;
    }

    refreshCatalog();
    refreshSubmissions();
    refreshHistoryData();

    m_lastRunMessage = QStringLiteral("Данные студента обновлены.");
    emit lastRunMessageChanged();
    return true;
}

bool ApplicationController::removeStudent(int studentId)
{
    if (m_testsRunning) {
        m_lastRunMessage = QStringLiteral("Нельзя удалять студентов во время проверки.");
        emit lastRunMessageChanged();
        return false;
    }

    QString errorMessage;
    if (!m_submissionService.removeStudent(studentId, &errorMessage)) {
        m_lastRunMessage = errorMessage;
        emit lastRunMessageChanged();
        return false;
    }

    refreshCatalog();
    refreshSubmissions();
    refreshResults();
    refreshHistoryData();

    m_lastRunMessage = QStringLiteral("Студент удален.");
    emit lastRunMessageChanged();
    emit dashboardChanged();
    return true;
}

bool ApplicationController::createLabWork(
    const QString &title,
    const QString &description,
    const QString &templatePath,
    const QString &testPath,
    const QString &language
)
{
    if (m_testsRunning) {
        m_lastRunMessage = QStringLiteral("Нельзя менять лабораторные во время проверки.");
        emit lastRunMessageChanged();
        return false;
    }

    QString errorMessage;
    int createdLabId = 0;
    if (!m_submissionService.createLabWork(
            title,
            description,
            templatePath,
            testPath,
            language,
            &createdLabId,
            &errorMessage)) {
        m_lastRunMessage = errorMessage;
        emit lastRunMessageChanged();
        return false;
    }

    refreshCatalog();
    refreshSubmissions();
    refreshHistoryData();

    m_lastRunMessage = QStringLiteral("Лабораторная добавлена.");
    emit lastRunMessageChanged();
    emit dashboardChanged();
    Q_UNUSED(createdLabId);
    return true;
}

bool ApplicationController::updateLabWork(
    int labId,
    const QString &title,
    const QString &description,
    const QString &templatePath,
    const QString &testPath,
    const QString &language
)
{
    if (m_testsRunning) {
        m_lastRunMessage = QStringLiteral("Нельзя менять лабораторные во время проверки.");
        emit lastRunMessageChanged();
        return false;
    }

    QString errorMessage;
    if (!m_submissionService.updateLabWork(
            labId,
            title,
            description,
            templatePath,
            testPath,
            language,
            &errorMessage)) {
        m_lastRunMessage = errorMessage;
        emit lastRunMessageChanged();
        return false;
    }

    refreshCatalog();
    refreshSubmissions();
    refreshHistoryData();

    m_lastRunMessage = QStringLiteral("Лабораторная обновлена.");
    emit lastRunMessageChanged();
    emit dashboardChanged();
    return true;
}

bool ApplicationController::removeLabWork(int labId)
{
    if (m_testsRunning) {
        m_lastRunMessage = QStringLiteral("Нельзя менять лабораторные во время проверки.");
        emit lastRunMessageChanged();
        return false;
    }

    QString errorMessage;
    if (!m_submissionService.removeLabWork(labId, &errorMessage)) {
        m_lastRunMessage = errorMessage;
        emit lastRunMessageChanged();
        return false;
    }

    refreshCatalog();
    refreshSubmissions();
    refreshResults();
    refreshHistoryData();

    m_lastRunMessage = QStringLiteral("Лабораторная удалена.");
    emit lastRunMessageChanged();
    emit dashboardChanged();
    return true;
}

bool ApplicationController::addSubmission(const QString &filePath, int studentId, int labId)
{
    if (m_testsRunning) {
        m_lastRunMessage = QStringLiteral("Нельзя добавлять работы во время проверки.");
        emit lastRunMessageChanged();
        return false;
    }

    const services::SubmissionImportSummary summary =
        m_submissionImportService.importSingle(filePath, studentId, labId);
    if (summary.imported <= 0) {
        m_lastRunMessage = summary.toUserMessage();
        emit lastRunMessageChanged();
        return false;
    }

    refreshSubmissions();
    m_lastRunMessage = summary.toUserMessage();
    emit lastRunMessageChanged();
    emit dashboardChanged();
    return true;
}

bool ApplicationController::importDroppedFiles(const QVariantList &filePaths, int studentId, int labId)
{
    if (m_testsRunning) {
        m_lastRunMessage = QStringLiteral("Нельзя добавлять работы во время проверки.");
        emit lastRunMessageChanged();
        return false;
    }

    QStringList normalizedPaths;
    normalizedPaths.reserve(filePaths.size());
    qInfo() << "[Import] Received dropped items:" << filePaths.size()
            << "studentId=" << studentId << "labId=" << labId;
    for (const QVariant &value : filePaths) {
        QString path;
        if (value.canConvert<QUrl>()) {
            const QUrl url = value.toUrl();
            if (url.isLocalFile()) {
                path = url.toLocalFile();
            } else {
                path = url.toString();
            }
        } else {
            path = value.toString();
        }

        path = path.trimmed();
        if (path.startsWith(QStringLiteral("file:///"), Qt::CaseInsensitive)) {
            const QUrl url(path);
            if (url.isLocalFile()) {
                path = url.toLocalFile();
            }
        }

        if (!path.isEmpty()) {
            normalizedPaths.push_back(path);
            qInfo() << "[Import] normalized path:" << path;
        }
    }

    qInfo() << "[Import] normalized path count:" << normalizedPaths.size();

    const services::SubmissionImportSummary summary =
        m_submissionImportService.importDroppedFiles(normalizedPaths, studentId, labId);
    qInfo() << "[Import] summary total=" << summary.total
            << "imported=" << summary.imported
            << "skipped=" << summary.skipped
            << "errors=" << summary.errors;
    if (summary.imported <= 0) {
        m_lastRunMessage = summary.toUserMessage();
        emit lastRunMessageChanged();
        return false;
    }

    refreshSubmissions();
    m_lastRunMessage = summary.toUserMessage();
    emit lastRunMessageChanged();
    emit dashboardChanged();
    return true;
}

bool ApplicationController::importBatchSubmissions(
    const QVariantList &items,
    int fallbackStudentId,
    int fallbackLabId
)
{
    if (m_testsRunning) {
        m_lastRunMessage = QStringLiteral("Нельзя добавлять работы во время проверки.");
        emit lastRunMessageChanged();
        return false;
    }

    std::vector<services::SubmissionImportItem> entries;
    entries.reserve(static_cast<size_t>(items.size()));

    for (const QVariant &itemVariant : items) {
        const QVariantMap row = itemVariant.toMap();
        if (row.isEmpty()) {
            continue;
        }

        services::SubmissionImportItem entry;
        entry.sourcePath = row.value(QStringLiteral("filePath")).toString().trimmed();
        if (entry.sourcePath.isEmpty()) {
            entry.sourcePath = row.value(QStringLiteral("sourcePath")).toString().trimmed();
        }
        entry.studentId = row.value(QStringLiteral("studentId")).toInt();
        entry.labWorkId = row.value(QStringLiteral("labId")).toInt();
        if (entry.labWorkId <= 0) {
            entry.labWorkId = row.value(QStringLiteral("labWorkId")).toInt();
        }
        entries.push_back(entry);
    }

    const services::SubmissionImportSummary summary = m_submissionImportService.importBatch(
        entries,
        fallbackStudentId,
        fallbackLabId
    );
    if (summary.imported <= 0) {
        m_lastRunMessage = summary.toUserMessage();
        emit lastRunMessageChanged();
        return false;
    }

    refreshSubmissions();
    m_lastRunMessage = summary.toUserMessage();
    emit lastRunMessageChanged();
    emit dashboardChanged();
    return true;
}

QVariantList ApplicationController::resolveImportSources(const QVariantList &sourcePaths) const
{
    QStringList normalizedPaths;
    normalizedPaths.reserve(sourcePaths.size());

    for (const QVariant &value : sourcePaths) {
        QString path;
        if (value.canConvert<QUrl>()) {
            const QUrl url = value.toUrl();
            if (url.isLocalFile()) {
                path = url.toLocalFile();
            } else {
                path = url.toString();
            }
        } else {
            path = value.toString();
        }

        path = path.trimmed();
        if (path.startsWith(QStringLiteral("file:///"), Qt::CaseInsensitive)) {
            const QUrl url(path);
            if (url.isLocalFile()) {
                path = url.toLocalFile();
            }
        }

        if (!path.isEmpty()) {
            normalizedPaths.push_back(path);
        }
    }

    QStringList warnings;
    const QStringList resolvedPaths = m_submissionImportService.resolveImportSources(
        normalizedPaths,
        &warnings
    );

    for (const QString &warning : warnings) {
        qWarning() << "[Import] resolve warning:" << warning;
    }
    qInfo() << "[Import] resolve sources requested=" << sourcePaths.size()
            << "normalized=" << normalizedPaths.size()
            << "resolved=" << resolvedPaths.size();

    QVariantList result;
    result.reserve(resolvedPaths.size());
    for (const QString &path : resolvedPaths) {
        result.push_back(path);
    }
    return result;
}

bool ApplicationController::removeSubmission(int submissionId)
{
    if (m_testsRunning) {
        m_lastRunMessage = QStringLiteral("Нельзя удалять работы во время проверки.");
        emit lastRunMessageChanged();
        return false;
    }

    QString errorMessage;
    if (!m_submissionService.removeSubmission(submissionId, &errorMessage)) {
        m_lastRunMessage = errorMessage;
        emit lastRunMessageChanged();
        return false;
    }

    refreshSubmissions();
    refreshResults();
    refreshHistoryData();

    m_lastRunMessage = QStringLiteral("Работа удалена из активного списка.");
    emit lastRunMessageChanged();
    emit dashboardChanged();
    return true;
}

QVariantList ApplicationController::testCasesForSubmission(int submissionId) const
{
    const auto runResult = m_testExecutionService.findResultBySubmissionId(submissionId);
    if (!runResult.has_value()) {
        return {};
    }
    return buildTestCasesVariantList(*runResult);
}

QVariantList ApplicationController::testCasesForCheckRun(int checkRunId) const
{
    const auto runResult = m_testExecutionService.findResultByCheckRunId(checkRunId);
    if (!runResult.has_value()) {
        return {};
    }
    return buildTestCasesVariantList(*runResult);
}

QVariantMap ApplicationController::labWorkById(int labId) const
{
    for (const auto &item : m_labWorks) {
        const QVariantMap map = item.toMap();
        if (map.value(QStringLiteral("id")).toInt() == labId) {
            return map;
        }
    }
    return {};
}

QString ApplicationController::readTextFilePreview(const QString &path) const
{
    return readTextFilePreviewLimited(path, 12000);
}

QString ApplicationController::readTextFilePreviewLimited(const QString &path, int maxCharacters) const
{
    const QString normalizedPath = path.trimmed();
    if (normalizedPath.isEmpty()) {
        return QStringLiteral("Путь к файлу не указан.");
    }

    const int limit = std::max(maxCharacters, 200);
    const QFileInfo keyInfo(normalizedPath);
    const QString cachePathKey = keyInfo.exists()
        ? keyInfo.absoluteFilePath()
        : normalizedPath;
    const QString cacheKey = cachePathKey + QLatin1Char('|') + QString::number(limit);
    const auto cached = m_textPreviewCache.constFind(cacheKey);
    if (cached != m_textPreviewCache.cend()) {
        return *cached;
    }

    QFileInfo info(normalizedPath);
    if (!info.exists() || !info.isFile()) {
        const QString result = QStringLiteral("Файл не найден: %1").arg(normalizedPath);
        m_textPreviewCache.insert(cacheKey, result);
        return result;
    }

    QFile file(info.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QString result = QStringLiteral("Не удалось открыть файл: %1").arg(file.errorString());
        m_textPreviewCache.insert(cacheKey, result);
        return result;
    }

    const QByteArray raw = file.readAll();
    QString text = QString::fromUtf8(raw);
    if (text.contains(QChar::ReplacementCharacter)) {
        text = QString::fromLocal8Bit(raw);
    }

    if (text.size() > limit) {
        text = text.left(limit)
            + QStringLiteral("\n\n... [показаны первые %1 символов]").arg(limit);
    }
    m_textPreviewCache.insert(cacheKey, text);
    return text;
}

bool ApplicationController::setCloudUsername(const QString &username)
{
    QString errorMessage;
    if (!m_cloudSyncService.setCloudUsername(username, &errorMessage)) {
        m_lastRunMessage = errorMessage;
        emit lastRunMessageChanged();
        return false;
    }
    emit cloudSettingsChanged();
    return true;
}

bool ApplicationController::setCloudPassword(const QString &password)
{
    QString errorMessage;
    if (!m_cloudSyncService.setCloudPassword(password, &errorMessage)) {
        m_lastRunMessage = errorMessage;
        emit lastRunMessageChanged();
        return false;
    }
    emit cloudSettingsChanged();
    return true;
}

bool ApplicationController::requestCloudAccounts()
{
    if (m_cloudSyncInProgress || m_cloudOpThread) {
        m_lastRunMessage = QStringLiteral("Операция синхронизации уже выполняется.");
        emit lastRunMessageChanged();
        return false;
    }

    m_cloudSyncInProgress = true;
    emit cloudSyncInProgressChanged();

    const auto result = std::make_shared<CloudTaskResult>();
    m_cloudOpThread = QThread::create([result]() {
        labtester::database::DatabaseManager workerDb(
            QStringLiteral("LabTesterCloudAccounts_%1")
                .arg(QUuid::createUuid().toString(QUuid::WithoutBraces))
        );
        if (!workerDb.initialize()) {
            result->message = QStringLiteral("Не удалось инициализировать локальную БД.");
            return;
        }

        labtester::services::CloudSyncService workerSync(workerDb);
        result->history = workerSync.remoteAccounts(&result->message);
        result->success = result->message.trimmed().isEmpty();
    });

    connect(m_cloudOpThread, &QThread::finished, this, [this, result]() {
        if (m_cloudOpThread) {
            m_cloudOpThread->deleteLater();
            m_cloudOpThread = nullptr;
        }
        m_cloudSyncInProgress = false;
        emit cloudSyncInProgressChanged();
        emit cloudAccountsReady(result->history, result->message);
        if (!result->message.trimmed().isEmpty()) {
            m_lastRunMessage = result->message;
            emit lastRunMessageChanged();
        }
    });

    m_cloudOpThread->start();
    return true;
}

bool ApplicationController::createCloudAccount(const QString &username, const QString &password)
{
    if (m_cloudSyncInProgress || m_cloudOpThread) {
        m_lastRunMessage = QStringLiteral("Операция синхронизации уже выполняется.");
        emit lastRunMessageChanged();
        return false;
    }

    const QString resolvedUsername = username.trimmed();
    const QString resolvedPassword = password.trimmed();
    if (resolvedUsername.isEmpty() || resolvedPassword.isEmpty()) {
        m_lastRunMessage = QStringLiteral("Введите имя аккаунта и пароль.");
        emit lastRunMessageChanged();
        return false;
    }

    m_cloudSyncInProgress = true;
    emit cloudSyncInProgressChanged();

    const auto result = std::make_shared<CloudTaskResult>();
    m_cloudOpThread = QThread::create([resolvedUsername, resolvedPassword, result]() {
        labtester::database::DatabaseManager workerDb(
            QStringLiteral("LabTesterCloudCreate_%1")
                .arg(QUuid::createUuid().toString(QUuid::WithoutBraces))
        );
        if (!workerDb.initialize()) {
            result->message = QStringLiteral("Не удалось инициализировать локальную БД.");
            return;
        }

        labtester::services::CloudSyncService workerSync(workerDb);
        result->success = workerSync.createAccount(resolvedUsername, resolvedPassword, &result->message);
    });

    connect(m_cloudOpThread, &QThread::finished, this, [this, resolvedUsername, resolvedPassword, result]() {
        if (m_cloudOpThread) {
            m_cloudOpThread->deleteLater();
            m_cloudOpThread = nullptr;
        }

        m_cloudSyncInProgress = false;
        emit cloudSyncInProgressChanged();
        if (result->success) {
            QString ignored;
            m_cloudSyncService.setCloudUsername(resolvedUsername, &ignored);
            m_cloudSyncService.setCloudPassword(resolvedPassword, &ignored);
            emit cloudSettingsChanged();
            requestCloudAccounts();
        }
        m_lastRunMessage = result->message;
        emit lastRunMessageChanged();
    });

    m_cloudOpThread->start();
    return true;
}

bool ApplicationController::setCloudAutoSyncEnabled(bool enabled)
{
    QString errorMessage;
    if (!m_cloudSyncService.setAutoSyncEnabled(enabled, &errorMessage)) {
        m_lastRunMessage = errorMessage;
        emit lastRunMessageChanged();
        return false;
    }

    m_lastRunMessage = enabled
        ? QStringLiteral("Автосинхронизация включена.")
        : QStringLiteral("Автосинхронизация отключена.");
    emit lastRunMessageChanged();
    emit cloudSettingsChanged();
    return true;
}

QVariantMap ApplicationController::cloudVersionInfo(const QString &username, bool includeRemote) const
{
    return m_cloudSyncService.versionInfo(resolveCloudUsername(username), includeRemote);
}

bool ApplicationController::requestCloudVersionInfo(const QString &username, bool includeRemote)
{
    if (m_cloudSyncInProgress || m_cloudOpThread) {
        m_lastRunMessage = QStringLiteral("Облачная операция уже выполняется.");
        emit lastRunMessageChanged();
        return false;
    }

    const QString resolved = resolveCloudUsername(username);
    if (!includeRemote) {
        emit cloudVersionInfoReady(m_cloudSyncService.versionInfo(resolved, false));
        return true;
    }

    m_cloudSyncInProgress = true;
    emit cloudSyncInProgressChanged();

    const auto result = std::make_shared<CloudTaskResult>();
    m_cloudOpThread = QThread::create([resolved, result]() {
        labtester::database::DatabaseManager workerDb(
            QStringLiteral("LabTesterCloudInfo_%1")
                .arg(QUuid::createUuid().toString(QUuid::WithoutBraces))
        );
        if (!workerDb.initialize()) {
            result->info.insert(QStringLiteral("error"), QStringLiteral("Не удалось инициализировать локальную БД."));
            return;
        }

        labtester::services::CloudSyncService workerSync(workerDb);
        result->info = workerSync.versionInfo(resolved, true);
        result->success = true;
    });

    connect(m_cloudOpThread, &QThread::finished, this, [this, result]() {
        if (m_cloudOpThread) {
            m_cloudOpThread->deleteLater();
            m_cloudOpThread = nullptr;
        }

        m_cloudSyncInProgress = false;
        emit cloudSyncInProgressChanged();
        emit cloudVersionInfoReady(result->info);

        const QString error = result->info.value(QStringLiteral("error")).toString().trimmed();
        if (!error.isEmpty()) {
            m_lastRunMessage = error;
            emit lastRunMessageChanged();
        }
    });
    m_cloudOpThread->start();
    return true;
}

bool ApplicationController::requestCloudSaveHistory(const QString &username)
{
    if (m_cloudSyncInProgress || m_cloudOpThread) {
        m_lastRunMessage = QStringLiteral("Облачная операция уже выполняется.");
        emit lastRunMessageChanged();
        return false;
    }

    const QString resolved = resolveCloudUsername(username);
    if (resolved.isEmpty()) {
        m_lastRunMessage = QStringLiteral("Укажите имя пользователя.");
        emit lastRunMessageChanged();
        return false;
    }

    m_cloudSyncInProgress = true;
    emit cloudSyncInProgressChanged();

    const auto result = std::make_shared<CloudTaskResult>();
    m_cloudOpThread = QThread::create([resolved, result]() {
        labtester::database::DatabaseManager workerDb(
            QStringLiteral("LabTesterCloudHistory_%1")
                .arg(QUuid::createUuid().toString(QUuid::WithoutBraces))
        );
        if (!workerDb.initialize()) {
            result->message = QStringLiteral("Не удалось инициализировать локальную БД.");
            return;
        }

        labtester::services::CloudSyncService workerSync(workerDb);
        result->history = workerSync.remoteSaveHistory(resolved, &result->message);
        result->success = result->message.trimmed().isEmpty();
    });

    connect(m_cloudOpThread, &QThread::finished, this, [this, result]() {
        if (m_cloudOpThread) {
            m_cloudOpThread->deleteLater();
            m_cloudOpThread = nullptr;
        }

        m_cloudSyncInProgress = false;
        emit cloudSyncInProgressChanged();
        emit cloudSaveHistoryReady(result->history, result->message);

        if (!result->message.trimmed().isEmpty()) {
            m_lastRunMessage = result->message;
            emit lastRunMessageChanged();
        }
    });

    m_cloudOpThread->start();
    return true;
}

bool ApplicationController::syncToCloud(const QString &username, const QString &password)
{
    if (m_testsRunning) {
        m_lastRunMessage = QStringLiteral("Дождитесь завершения проверки перед синхронизацией.");
        emit lastRunMessageChanged();
        return false;
    }
    if (m_cloudSyncInProgress) {
        m_lastRunMessage = QStringLiteral("Синхронизация уже выполняется.");
        emit lastRunMessageChanged();
        return false;
    }

    const QString resolved = resolveCloudUsername(username);
    const QString resolvedPassword = password.trimmed().isEmpty()
        ? m_cloudSyncService.cloudPassword()
        : password.trimmed();
    if (resolved.isEmpty()) {
        m_lastRunMessage = QStringLiteral("Укажите имя пользователя для синхронизации.");
        emit lastRunMessageChanged();
        return false;
    }
    if (resolvedPassword.isEmpty()) {
        m_lastRunMessage = QStringLiteral("Введите пароль аккаунта.");
        emit lastRunMessageChanged();
        return false;
    }
    if (m_cloudOpThread) {
        m_lastRunMessage = QStringLiteral("Облачная операция уже выполняется.");
        emit lastRunMessageChanged();
        return false;
    }

    m_cloudSyncInProgress = true;
    emit cloudSyncInProgressChanged();

    const auto result = std::make_shared<CloudTaskResult>();
    m_cloudOpThread = QThread::create([resolved, resolvedPassword, result]() {
        labtester::database::DatabaseManager workerDb(
            QStringLiteral("LabTesterCloudSync_%1")
                .arg(QUuid::createUuid().toString(QUuid::WithoutBraces))
        );
        if (!workerDb.initialize()) {
            result->message = QStringLiteral("Не удалось инициализировать локальную БД.");
            return;
        }

        labtester::services::CloudSyncService workerSync(workerDb);
        result->success = workerSync.syncToCloud(resolved, resolvedPassword, &result->message);
    });

    connect(m_cloudOpThread, &QThread::finished, this, [this, resolved, resolvedPassword, result]() {
        if (m_cloudOpThread) {
            m_cloudOpThread->deleteLater();
            m_cloudOpThread = nullptr;
        }

        m_cloudSyncInProgress = false;
        emit cloudSyncInProgressChanged();
        emit cloudSettingsChanged();
        if (result->success) {
            QString ignored;
            m_cloudSyncService.setCloudUsername(resolved, &ignored);
            m_cloudSyncService.setCloudPassword(resolvedPassword, &ignored);
            emit cloudSettingsChanged();
        }

        m_lastRunMessage = result->message;
        emit lastRunMessageChanged();
    });
    m_cloudOpThread->start();
    return true;
}

bool ApplicationController::syncFromCloud(const QString &username, const QString &password)
{
    if (m_testsRunning) {
        m_lastRunMessage = QStringLiteral("Дождитесь завершения проверки перед синхронизацией.");
        emit lastRunMessageChanged();
        return false;
    }
    if (m_cloudSyncInProgress) {
        m_lastRunMessage = QStringLiteral("Синхронизация уже выполняется.");
        emit lastRunMessageChanged();
        return false;
    }

    const QString resolved = resolveCloudUsername(username);
    const QString resolvedPassword = password.trimmed().isEmpty()
        ? m_cloudSyncService.cloudPassword()
        : password.trimmed();
    if (resolved.isEmpty()) {
        m_lastRunMessage = QStringLiteral("Укажите имя пользователя для синхронизации.");
        emit lastRunMessageChanged();
        return false;
    }
    if (resolvedPassword.isEmpty()) {
        m_lastRunMessage = QStringLiteral("Введите пароль аккаунта.");
        emit lastRunMessageChanged();
        return false;
    }
    if (m_cloudOpThread) {
        m_lastRunMessage = QStringLiteral("Облачная операция уже выполняется.");
        emit lastRunMessageChanged();
        return false;
    }

    m_cloudSyncInProgress = true;
    emit cloudSyncInProgressChanged();

    const auto result = std::make_shared<CloudTaskResult>();
    m_cloudOpThread = QThread::create([resolved, resolvedPassword, result]() {
        labtester::database::DatabaseManager workerDb(
            QStringLiteral("LabTesterCloudSync_%1")
                .arg(QUuid::createUuid().toString(QUuid::WithoutBraces))
        );
        if (!workerDb.initialize()) {
            result->message = QStringLiteral("Не удалось инициализировать локальную БД.");
            return;
        }

        labtester::services::CloudSyncService workerSync(workerDb);
        result->success = workerSync.syncFromCloud(resolved, resolvedPassword, &result->message);
    });

    connect(m_cloudOpThread, &QThread::finished, this, [this, resolved, resolvedPassword, result]() {
        if (m_cloudOpThread) {
            m_cloudOpThread->deleteLater();
            m_cloudOpThread = nullptr;
        }

        m_cloudSyncInProgress = false;
        emit cloudSyncInProgressChanged();
        emit cloudSettingsChanged();

        if (result->success) {
            m_submissionService.invalidateCatalogCache();
            m_textPreviewCache.clear();
            refreshCatalog();
            m_submissionsModel.setSubmissions(m_submissionService.loadSubmissions());
            m_students = buildStudentsVariant();
            emit studentsChanged();
            refreshSelectedStudentSubmissions();
            emit dashboardChanged();
            refreshResults();
            refreshHistoryData();
            QString ignored;
            m_cloudSyncService.setCloudUsername(resolved, &ignored);
            m_cloudSyncService.setCloudPassword(resolvedPassword, &ignored);
            emit cloudSettingsChanged();
        }

        m_lastRunMessage = result->message;
        emit lastRunMessageChanged();
    });
    m_cloudOpThread->start();
    return true;
}

bool ApplicationController::setExecutionMode(const QString &mode)
{
    QString errorMessage;
    if (!m_executionSettingsService.setExecutionMode(mode, &errorMessage)) {
        m_lastRunMessage = errorMessage;
        emit lastRunMessageChanged();
        return false;
    }

    const QString normalizedMode = mode.trimmed().toLower();
    if (normalizedMode == QStringLiteral("server")) {
        m_lastRunMessage = QStringLiteral("Включен серверный режим запуска тестов.");
    } else if (normalizedMode == QStringLiteral("hybrid")) {
        m_lastRunMessage = QStringLiteral("Включен гибридный режим: локально + сервер.");
    } else {
        m_lastRunMessage = QStringLiteral("Включен локальный режим запуска тестов.");
    }
    emit lastRunMessageChanged();
    emit executionSettingsChanged();
    return true;
}

bool ApplicationController::setRemoteExecutionUrl(const QString &url)
{
    QString errorMessage;
    if (!m_executionSettingsService.setRemoteEndpoint(url, &errorMessage)) {
        m_lastRunMessage = errorMessage;
        emit lastRunMessageChanged();
        return false;
    }

    m_lastRunMessage = QStringLiteral("URL удаленного воркера сохранен.");
    emit lastRunMessageChanged();
    emit executionSettingsChanged();
    return true;
}

bool ApplicationController::setRemoteExecutionToken(const QString &token)
{
    QString errorMessage;
    if (!m_executionSettingsService.setRemoteToken(token, &errorMessage)) {
        m_lastRunMessage = errorMessage;
        emit lastRunMessageChanged();
        return false;
    }

    m_lastRunMessage = QStringLiteral("Токен удаленного воркера сохранен.");
    emit lastRunMessageChanged();
    emit executionSettingsChanged();
    return true;
}

bool ApplicationController::setRemoteExecutionNgrokMode(bool enabled)
{
    QString errorMessage;
    if (!m_executionSettingsService.setRemoteNgrokMode(enabled, &errorMessage)) {
        m_lastRunMessage = errorMessage;
        emit lastRunMessageChanged();
        return false;
    }

    m_lastRunMessage = enabled
        ? QStringLiteral("Tunnel-совместимый режим для удаленного воркера включен.")
        : QStringLiteral("Tunnel-совместимый режим для удаленного воркера выключен.");
    emit lastRunMessageChanged();
    emit executionSettingsChanged();
    return true;
}

QString ApplicationController::readRuntimeLog(int maxChars) const
{
    const QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString logPath = QDir(baseDir).filePath(QStringLiteral("startup.log"));

    QFile file(logPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QStringLiteral("Лог недоступен: %1").arg(file.errorString());
    }

    QString text = QString::fromUtf8(file.readAll());
    if (maxChars > 0 && text.size() > maxChars) {
        text = QStringLiteral("...") + text.right(maxChars);
    }
    return text;
}

bool ApplicationController::removeHistoryEntry(int checkRunId)
{
    if (checkRunId <= 0) {
        m_lastRunMessage = QStringLiteral("Не выбран запуск для удаления.");
        emit lastRunMessageChanged();
        return false;
    }
    if (m_testsRunning) {
        m_lastRunMessage = QStringLiteral("Нельзя удалять историю во время проверки.");
        emit lastRunMessageChanged();
        return false;
    }

    QString errorMessage;
    if (!m_testExecutionService.removeHistoryEntry(checkRunId, &errorMessage)) {
        m_lastRunMessage = errorMessage.isEmpty()
            ? QStringLiteral("Не удалось удалить запуск из истории.")
            : errorMessage;
        emit lastRunMessageChanged();
        return false;
    }

    refreshResults();
    refreshHistoryData();
    m_lastRunMessage = QStringLiteral("Запуск удален из истории.");
    emit lastRunMessageChanged();
    return true;
}

void ApplicationController::handleApplicationAboutToQuit()
{
}

void ApplicationController::onWorkerProgressChanged(int current, int total, const QString &label)
{
    Q_UNUSED(current);
    Q_UNUSED(total);
    const int nextIndex = std::min(m_testProgressCurrent + 1, m_testProgressTotal);
    m_testProgressText = QStringLiteral("Проверка %1/%2: %3")
        .arg(nextIndex)
        .arg(m_testProgressTotal)
        .arg(label);
    emit testProgressChanged();

    m_lastRunMessage = m_testProgressText;
    emit lastRunMessageChanged();
}

void ApplicationController::onWorkerSubmissionFinished(
    int current,
    int total,
    const labtester::domain::TestRunResult &result
)
{
    Q_UNUSED(current);
    Q_UNUSED(total);
    m_testExecutionService.applyResult(result);

    refreshSubmissions();
    refreshResults();
    refreshHistoryData();

    m_testProgressCurrent = std::min(m_testProgressCurrent + 1, m_testProgressTotal);
    m_testProgressText = QStringLiteral("Завершено %1/%2: %3")
        .arg(m_testProgressCurrent)
        .arg(m_testProgressTotal)
        .arg(result.studentName);
    emit testProgressChanged();
    m_lastRunMessage = m_testProgressText;
    emit lastRunMessageChanged();

    emit dashboardChanged();
}

void ApplicationController::onWorkerFinished()
{
    ++m_finishedTestWorkers;
    if (m_finishedTestWorkers < m_expectedTestWorkers) {
        return;
    }

    m_testsRunning = false;
    emit testsRunningChanged();

    refreshSubmissions();
    refreshResults();
    refreshHistoryData();

    if (m_cancelRequested) {
        m_lastRunMessage = QStringLiteral("Проверка отменена. Обработано работ: %1 из %2")
            .arg(m_testProgressCurrent)
            .arg(m_testProgressTotal);
    } else {
        m_lastRunMessage = QStringLiteral("Проверка завершена. Обработано работ: %1").arg(m_testProgressCurrent);
    }
    emit lastRunMessageChanged();

    m_cancelRequested = false;
    resetProgressState();
    emit dashboardChanged();
}

void ApplicationController::resetProgressState()
{
    m_testProgressCurrent = 0;
    m_testProgressTotal = 0;
    m_testProgressText.clear();
    m_expectedTestWorkers = 0;
    m_finishedTestWorkers = 0;
    emit testProgressChanged();
}

void ApplicationController::refreshCatalog()
{
    m_students = buildStudentsVariant();
    m_labWorks = buildLabWorksVariant();

    emit studentsChanged();
    emit labWorksChanged();

    bool selectedExists = false;
    if (m_selectedStudentId > 0) {
        for (const auto &item : m_students) {
            if (item.toMap().value(QStringLiteral("id")).toInt() == m_selectedStudentId) {
                selectedExists = true;
                break;
            }
        }
    }
    if (!selectedExists) {
        m_selectedStudentId = 0;
    }

    m_selectedStudentName = findStudentName(m_selectedStudentId);
    refreshSelectedStudentSubmissions();
    emit selectedStudentChanged();
}

void ApplicationController::refreshSubmissions()
{
    m_submissionsModel.setSubmissions(m_submissionService.loadOrCreateMockSubmissions());
    m_students = buildStudentsVariant();
    emit studentsChanged();
    refreshSelectedStudentSubmissions();
    emit dashboardChanged();
}

void ApplicationController::refreshResults()
{
    m_resultsModel.setResults(m_testExecutionService.loadResults());
}

void ApplicationController::refreshHistoryData()
{
    m_historyModel.setEntries(
        m_testExecutionService.loadHistory(
            m_historyGroupFilter,
            m_historyStudentFilter,
            m_historyLabFilter
        )
    );
}

void ApplicationController::refreshSelectedStudentSubmissions()
{
    m_selectedStudentSubmissions.clear();
    const auto submissions = m_submissionService.loadSubmissionsForStudent(m_selectedStudentId);
    for (const auto &submission : submissions) {
        QString displayStatus = domain::toDisplayString(submission.status);
        QString statusKey = domain::toStorageString(submission.status);
        const auto latestRun = m_testExecutionService.findResultBySubmissionId(submission.id);
        if (latestRun.has_value()) {
            displayStatus = latestRun->displayStatus.isEmpty()
                ? domain::toDisplayString(latestRun->status)
                : latestRun->displayStatus;
            statusKey = latestRun->displayStatusKey.isEmpty()
                ? domain::toStorageString(latestRun->status)
                : latestRun->displayStatusKey;
        }

        QVariantMap map;
        map.insert(QStringLiteral("submissionId"), submission.id);
        map.insert(QStringLiteral("studentName"), submission.student.name);
        map.insert(QStringLiteral("groupName"), submission.student.groupName);
        map.insert(QStringLiteral("labTitle"), submission.labWork.title);
        map.insert(QStringLiteral("language"), submission.labWork.language);
        map.insert(QStringLiteral("sourcePath"), submission.sourcePath);
        map.insert(QStringLiteral("status"), displayStatus);
        map.insert(QStringLiteral("statusKey"), statusKey);
        map.insert(QStringLiteral("createdAt"), submission.createdAt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
        m_selectedStudentSubmissions.push_back(map);
    }

    emit selectedStudentSubmissionsChanged();
}

int ApplicationController::countByStatus(domain::ExecutionStatus status) const
{
    const auto &submissions = m_submissionsModel.submissions();
    return static_cast<int>(std::count_if(
        submissions.begin(),
        submissions.end(),
        [status](const domain::Submission &submission) {
            return submission.status == status;
        }
    ));
}

QVariantList ApplicationController::buildStudentsVariant()
{
    QVariantList list;
    const auto &students = m_submissionService.loadStudents();

    for (const auto &student : students) {
        if (student.name.compare(QStringLiteral("Студент не выбран"), Qt::CaseInsensitive) == 0) {
            continue;
        }

        int worksCount = 0;
        for (const auto &submission : m_submissionService.loadSubmissions()) {
            if (submission.student.id == student.id) {
                ++worksCount;
            }
        }

        QVariantMap map;
        map.insert(QStringLiteral("id"), student.id);
        map.insert(QStringLiteral("name"), student.name);
        map.insert(QStringLiteral("group"), student.groupName);
        map.insert(QStringLiteral("worksCount"), worksCount);
        list.push_back(map);
    }
    return list;
}

QVariantList ApplicationController::buildLabWorksVariant()
{
    QVariantList list;
    const auto &labs = m_submissionService.loadLabWorks();

    for (const auto &lab : labs) {
        QVariantMap map;
        map.insert(QStringLiteral("id"), lab.id);
        map.insert(QStringLiteral("title"), lab.title);
        map.insert(QStringLiteral("language"), lab.language);
        map.insert(QStringLiteral("description"), lab.description);
        map.insert(QStringLiteral("manifestPath"), lab.manifestPath);
        map.insert(QStringLiteral("templateFile"), lab.templateFile);
        map.insert(QStringLiteral("expectedInput"), lab.expectedInput);
        map.insert(QStringLiteral("expectedOutput"), lab.expectedOutput);
        map.insert(QStringLiteral("testSuitePath"), lab.testSuitePath);
        map.insert(QStringLiteral("referenceHeaderPath"), lab.referenceHeaderPath);
        const QFileInfo suiteInfo(lab.testSuitePath);
        map.insert(
            QStringLiteral("testDirectory"),
            suiteInfo.exists() ? suiteInfo.absolutePath() : QString()
        );
        list.push_back(map);
    }
    return list;
}

QString ApplicationController::findStudentName(int studentId) const
{
    if (studentId <= 0) {
        return QStringLiteral("Студент не выбран");
    }

    for (const auto &item : m_students) {
        const QVariantMap map = item.toMap();
        if (map.value(QStringLiteral("id")).toInt() == studentId) {
            return map.value(QStringLiteral("name")).toString();
        }
    }
    return QString();
}

QString ApplicationController::submissionSourcePathById(int submissionId) const
{
    const auto submissions = m_submissionService.loadSubmissions();
    for (const auto &submission : submissions) {
        if (submission.id == submissionId) {
            return submission.sourcePath;
        }
    }
    return QString();
}

QVariantList ApplicationController::buildTestCasesVariantList(const domain::TestRunResult &runResult) const
{
    QVariantList list;
    const QString sourcePath = submissionSourcePathById(runResult.submissionId);
    const QString sourceCode = readTextFile(sourcePath);

    for (const auto &testCase : runResult.testCases) {
        const QString functionName = detectFunctionNameForTest(testCase.testName);
        QString functionCode;
        if (!sourceCode.isEmpty() && !functionName.isEmpty()) {
            const QStringList functionNames = functionName.split(QStringLiteral(" / "), Qt::SkipEmptyParts);
            QStringList codeBlocks;
            for (const QString &name : functionNames) {
                const QString codeBlock = extractFunctionCodeFromSource(sourceCode, name.trimmed());
                if (!codeBlock.isEmpty() && !codeBlocks.contains(codeBlock)) {
                    codeBlocks.push_back(codeBlock);
                }
            }
            functionCode = codeBlocks.join(QStringLiteral("\n\n"));
        }

        if (functionCode.isEmpty()) {
            if (functionName.isEmpty()) {
                functionCode = QStringLiteral("Для этого теста не удалось определить вызываемую функцию.");
            } else if (sourceCode.isEmpty()) {
                functionCode = QStringLiteral("Не удалось прочитать файл работы студента.");
            } else {
                functionCode = QStringLiteral("Функция `%1` не найдена в файле работы.").arg(functionName);
            }
        }

        QString inputData = testCase.inputData;
        QString expectedOutput = testCase.expectedOutput;
        const CaseDisplayMetadata displayMetadata = structureCaseMetadata(testCase.testName);
        if (!displayMetadata.input.isEmpty() && isPlaceholderMetadata(inputData)) {
            inputData = displayMetadata.input;
        }
        if (!displayMetadata.expected.isEmpty() && isPlaceholderMetadata(expectedOutput)) {
            expectedOutput = displayMetadata.expected;
        }

        QVariantMap map;
        map.insert(QStringLiteral("testName"), testCase.testName);
        map.insert(QStringLiteral("passed"), testCase.passed);
        map.insert(QStringLiteral("status"), testCase.passed
            ? QStringLiteral("Успешно")
            : QStringLiteral("Провалено"));
        map.insert(QStringLiteral("message"), testCase.message);
        map.insert(QStringLiteral("inputData"), inputData);
        map.insert(QStringLiteral("expectedOutput"), expectedOutput);
        map.insert(QStringLiteral("actualOutput"), testCase.actualOutput);
        map.insert(QStringLiteral("failureDetails"), testCase.failureDetails);
        map.insert(QStringLiteral("durationMs"), testCase.durationMs);
        map.insert(
            QStringLiteral("isBuildErrorCase"),
            testCase.testName == QStringLiteral("Подготовка/сборка")
                || runResult.status == domain::ExecutionStatus::BuildError
        );
        map.insert(QStringLiteral("functionName"), functionName);
        map.insert(QStringLiteral("functionCode"), functionCode);
        list.push_back(map);
    }

    return list;
}

QString ApplicationController::detectFunctionNameForTest(const QString &testName) const
{
    const QString suiteName = testName.section('.', 0, 0).section('/', -1);
    const QString caseName = testName.section('.', -1);

    if (suiteName.startsWith(QStringLiteral("BubbleSort_"))) {
        return QStringLiteral("bubblesort");
    }
    if (suiteName.startsWith(QStringLiteral("SelectionSort_"))) {
        return QStringLiteral("selectionSort");
    }
    if (suiteName.startsWith(QStringLiteral("InsertionSort_"))) {
        return QStringLiteral("insertionSort");
    }
    if (suiteName.startsWith(QStringLiteral("MergeSort_"))) {
        return QStringLiteral("mergeSort");
    }
    if (suiteName.startsWith(QStringLiteral("QuickSort_"))) {
        return QStringLiteral("quickSort");
    }
    if (suiteName.startsWith(QStringLiteral("HeapSort_"))) {
        return QStringLiteral("sorting_heap");
    }
    if (suiteName.startsWith(QStringLiteral("LexicographicSort_"))
        || suiteName.startsWith(QStringLiteral("LexSort_"))) {
        return caseName.contains(QStringLiteral("Compare"), Qt::CaseInsensitive)
            ? QStringLiteral("lexSort")
            : QStringLiteral("bubblesortForLex");
    }
    if (suiteName.startsWith(QStringLiteral("Stack_"))) {
        return QStringLiteral("ArrayStack");
    }
    if (suiteName.startsWith(QStringLiteral("Queue_"))) {
        return QStringLiteral("ArrayQueue");
    }
    if (suiteName.startsWith(QStringLiteral("Deque_"))) {
        return QStringLiteral("ArrayDeque");
    }
    if (suiteName.startsWith(QStringLiteral("LinkedList_"))) {
        return QStringLiteral("LinkedList");
    }
    if (suiteName.startsWith(QStringLiteral("BinarySearchTree_"))) {
        return QStringLiteral("BinarySearchTree");
    }
    if (suiteName.startsWith(QStringLiteral("HashFunctions_"))
        || suiteName.startsWith(QStringLiteral("HashDistribution_"))) {
        return QStringLiteral("hash1 / hash2 / hash3");
    }
    if (suiteName.startsWith(QStringLiteral("HashTable_"))) {
        return QStringLiteral("HashTable");
    }
    if (suiteName.startsWith(QStringLiteral("RabinKarp_"))) {
        return QStringLiteral("rabinKarpSearch");
    }
    if (suiteName.startsWith(QStringLiteral("KMP_"))) {
        return QStringLiteral("kmpSearch");
    }
    if (suiteName.startsWith(QStringLiteral("BoyerMoore_"))) {
        return QStringLiteral("boyerMooreSearch");
    }
    if (suiteName.startsWith(QStringLiteral("AhoCorasick_"))) {
        return QStringLiteral("ahoCorasickSearch");
    }

    if (caseName == QStringLiteral("BubbleSort_Small")
        || caseName == QStringLiteral("DotAsCharInIntArray")
        || caseName == QStringLiteral("BubbleSort_Random100")) {
        return QStringLiteral("bubblesort");
    }
    if (caseName == QStringLiteral("SelectionSort_Small")) {
        return QStringLiteral("selectionSort");
    }
    if (caseName == QStringLiteral("InsertionSort_Small")
        || caseName == QStringLiteral("Negatives")) {
        return QStringLiteral("insertionSort");
    }
    if (caseName == QStringLiteral("MergeSort_Small")
        || caseName == QStringLiteral("RevSorted")) {
        return QStringLiteral("mergeSort");
    }
    if (caseName == QStringLiteral("QuickSort_Small")
        || caseName == QStringLiteral("Sorted")) {
        return QStringLiteral("quickSort");
    }
    if (caseName == QStringLiteral("HeapSort_Small")
        || caseName == QStringLiteral("AllSame")) {
        return QStringLiteral("sorting_heap");
    }
    if (caseName == QStringLiteral("SingleElement")) {
        return QStringLiteral("bubblesort");
    }
    if (caseName == QStringLiteral("Empty")) {
        return QStringLiteral("bubblesort / selectionSort / insertionSort / mergeSort / quickSort / sorting_heap");
    }
    if (caseName == QStringLiteral("BasicStrings")) {
        return QStringLiteral("bubblesortForLex");
    }
    if (caseName == QStringLiteral("CompareFunction")) {
        return QStringLiteral("lexSort");
    }
    if (caseName.startsWith(QStringLiteral("SortsCorrectly/"))) {
        bool ok = false;
        const int index = caseName.section('/', -1).toInt(&ok);
        if (ok) {
            switch (index) {
            case 0:
                return QStringLiteral("bubblesort");
            case 1:
                return QStringLiteral("selectionSort");
            case 2:
                return QStringLiteral("insertionSort");
            case 3:
                return QStringLiteral("mergeSort");
            case 4:
                return QStringLiteral("quickSort");
            case 5:
                return QStringLiteral("sorting_heap");
            default:
                break;
            }
        }
    }

    return QString();
}

QString ApplicationController::extractFunctionCodeFromSource(
    const QString &sourceText,
    const QString &functionName
) const
{
    if (sourceText.isEmpty() || functionName.trimmed().isEmpty()) {
        return QString();
    }

    auto findMatchingClosingBrace = [&sourceText](int openBracePos) -> int {
        int depth = 0;
        for (int i = openBracePos; i < sourceText.size(); ++i) {
            const QChar ch = sourceText.at(i);
            if (ch == QChar('{')) {
                ++depth;
            } else if (ch == QChar('}')) {
                --depth;
                if (depth == 0) {
                    return i;
                }
            }
        }
        return -1;
    };

    auto extractByPosition = [&sourceText, &findMatchingClosingBrace](int signaturePos, int bracePos) -> QString {
        if (signaturePos < 0 || bracePos < 0) {
            return QString();
        }
        const int lineStartRaw = sourceText.lastIndexOf(QChar('\n'), signaturePos);
        const int lineStart = lineStartRaw < 0 ? 0 : lineStartRaw + 1;
        const int closingBracePos = findMatchingClosingBrace(bracePos);
        if (closingBracePos < 0 || closingBracePos < lineStart) {
            return QString();
        }
        return sourceText.mid(lineStart, closingBracePos - lineStart + 1).trimmed();
    };

    auto extractClassMethods = [&sourceText, &extractByPosition](const QString &className) -> QString {
        const QString cleanName = className.trimmed();
        const QStringList supportedClasses {
            QStringLiteral("ArrayStack"),
            QStringLiteral("ArrayQueue"),
            QStringLiteral("ArrayDeque"),
            QStringLiteral("LinkedList"),
            QStringLiteral("BinarySearchTree"),
            QStringLiteral("HashTable")
        };
        if (!supportedClasses.contains(cleanName)) {
            return QString();
        }

        const QString escapedClassName = QRegularExpression::escape(cleanName);
        const QRegularExpression methodDefinition(
            QStringLiteral(
                R"((^|\n)\s*(?:template\s*<[^>\n]+>\s*)?(?:(?:[\w:<>~*&]+\s+)+)?%1::(?:~?%1|[A-Za-z_][A-Za-z0-9_]*)\s*\([^;{}]*\)\s*(?:const\s*)?\{)"
            ).arg(escapedClassName),
            QRegularExpression::MultilineOption
        );

        QStringList blocks;
        QSet<int> seenPositions;
        QRegularExpressionMatchIterator matches = methodDefinition.globalMatch(sourceText);
        while (matches.hasNext()) {
            const QRegularExpressionMatch match = matches.next();
            const int signaturePos = match.capturedStart(0);
            if (signaturePos < 0 || seenPositions.contains(signaturePos)) {
                continue;
            }
            seenPositions.insert(signaturePos);

            const int bracePos = sourceText.indexOf(QChar('{'), signaturePos);
            const QString extracted = extractByPosition(signaturePos, bracePos);
            if (!extracted.isEmpty()) {
                blocks.push_back(extracted);
            }
        }
        return blocks.join(QStringLiteral("\n\n"));
    };

    const QString classMethods = extractClassMethods(functionName);
    if (!classMethods.isEmpty()) {
        return classMethods;
    }

    auto findFunctionCode = [&sourceText, &extractByPosition](const QString &name) -> QString {
        const QString cleanName = name.trimmed();
        if (cleanName.isEmpty()) {
            return QString();
        }

        const QString escapedName = QRegularExpression::escape(cleanName);
        const QRegularExpression directDefinition(
            QStringLiteral(
                R"((^|\n)\s*(?:template\s*<[^>\n]+>\s*)?(?:[\w:<>~*&]+\s+)+(?:(?:[A-Za-z_][A-Za-z0-9_]*)::)?%1\s*\([^;{}]*\)\s*(?:const\s*)?\{)"
            ).arg(escapedName),
            QRegularExpression::MultilineOption
        );

        QRegularExpressionMatch directMatch = directDefinition.match(sourceText);
        if (directMatch.hasMatch()) {
            const int signaturePos = directMatch.capturedStart(0);
            const int bracePos = sourceText.indexOf(QChar('{'), signaturePos);
            const QString extracted = extractByPosition(signaturePos, bracePos);
            if (!extracted.isEmpty()) {
                return extracted;
            }
        }

        return QString();
    };

    const QStringList ignoredCalls {
        QStringLiteral("if"),
        QStringLiteral("for"),
        QStringLiteral("while"),
        QStringLiteral("switch"),
        QStringLiteral("return"),
        QStringLiteral("sizeof"),
        QStringLiteral("static_cast"),
        QStringLiteral("dynamic_cast"),
        QStringLiteral("reinterpret_cast"),
        QStringLiteral("const_cast")
    };

    QStringList pending {functionName.trimmed()};
    QStringList seen;
    QStringList blocks;

    for (int i = 0; i < pending.size(); ++i) {
        const QString currentName = pending.at(i).trimmed();
        if (currentName.isEmpty() || seen.contains(currentName)) {
            continue;
        }

        const QString currentCode = findFunctionCode(currentName);
        if (currentCode.isEmpty()) {
            continue;
        }

        seen.push_back(currentName);
        blocks.push_back(currentCode);

        const QRegularExpression callPattern(QStringLiteral(R"(\b([A-Za-z_][A-Za-z0-9_]*)\s*\()"));
        QRegularExpressionMatchIterator calls = callPattern.globalMatch(currentCode);
        while (calls.hasNext()) {
            const QRegularExpressionMatch call = calls.next();
            const QString candidate = call.captured(1);
            const int candidateStart = call.capturedStart(1);
            const QChar previous = candidateStart > 0 ? currentCode.at(candidateStart - 1) : QChar();
            const QString previousTwo = candidateStart >= 2 ? currentCode.mid(candidateStart - 2, 2) : QString();
            if (ignoredCalls.contains(candidate)
                || previous == QChar('.')
                || previousTwo == QStringLiteral("->")
                || previousTwo == QStringLiteral("::")
                || candidate == currentName
                || seen.contains(candidate)
                || pending.contains(candidate)) {
                continue;
            }
            if (!findFunctionCode(candidate).isEmpty()) {
                pending.push_back(candidate);
            }
        }
    }

    return blocks.join(QStringLiteral("\n\n"));
}

QString ApplicationController::readTextFile(const QString &path) const
{
    const QString normalizedPath = path.trimmed();
    if (normalizedPath.isEmpty()) {
        return QString();
    }

    QFileInfo info(normalizedPath);
    if (!info.exists() || !info.isFile()) {
        return QString();
    }

    QFile file(info.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }

    const QByteArray raw = file.readAll();
    QString text = QString::fromUtf8(raw);
    if (text.contains(QChar::ReplacementCharacter)) {
        text = QString::fromLocal8Bit(raw);
    }
    return text;
}

QString ApplicationController::resolveCloudUsername(const QString &username) const
{
    const QString candidate = username.trimmed();
    if (!candidate.isEmpty()) {
        return candidate;
    }
    return m_cloudSyncService.cloudUsername().trimmed();
}

} // namespace labtester::app
