#pragma once

#include <QObject>
#include <QString>
#include <QHash>
#include <QList>
#include <QVariantList>
#include <QVariantMap>

#include "domain/ExecutionStatus.h"
#include "domain/Submission.h"
#include "models/HistoryListModel.h"
#include "models/ResultListModel.h"
#include "models/SubmissionListModel.h"

namespace labtester::services {
class SubmissionService;
class SubmissionImportService;
class TestExecutionService;
class CloudSyncService;
class ExecutionSettingsService;
}

namespace labtester::app {
class TestRunWorker;
}

class QThread;

namespace labtester::app {

class ApplicationController : public QObject {
    Q_OBJECT
    Q_PROPERTY(labtester::models::SubmissionListModel *submissionsModel READ submissionsModel CONSTANT)
    Q_PROPERTY(labtester::models::ResultListModel *resultsModel READ resultsModel CONSTANT)
    Q_PROPERTY(labtester::models::HistoryListModel *historyModel READ historyModel CONSTANT)
    Q_PROPERTY(QVariantList students READ students NOTIFY studentsChanged)
    Q_PROPERTY(QVariantList labWorks READ labWorks NOTIFY labWorksChanged)
    Q_PROPERTY(QVariantList selectedStudentSubmissions READ selectedStudentSubmissions NOTIFY selectedStudentSubmissionsChanged)
    Q_PROPERTY(int selectedStudentId READ selectedStudentId NOTIFY selectedStudentChanged)
    Q_PROPERTY(QString selectedStudentName READ selectedStudentName NOTIFY selectedStudentChanged)
    Q_PROPERTY(int totalSubmissions READ totalSubmissions NOTIFY dashboardChanged)
    Q_PROPERTY(int passedSubmissions READ passedSubmissions NOTIFY dashboardChanged)
    Q_PROPERTY(int failedSubmissions READ failedSubmissions NOTIFY dashboardChanged)
    Q_PROPERTY(int pendingSubmissions READ pendingSubmissions NOTIFY dashboardChanged)
    Q_PROPERTY(QString lastRunMessage READ lastRunMessage NOTIFY lastRunMessageChanged)
    Q_PROPERTY(bool testsRunning READ testsRunning NOTIFY testsRunningChanged)
    Q_PROPERTY(int testProgressCurrent READ testProgressCurrent NOTIFY testProgressChanged)
    Q_PROPERTY(int testProgressTotal READ testProgressTotal NOTIFY testProgressChanged)
    Q_PROPERTY(double testProgressValue READ testProgressValue NOTIFY testProgressChanged)
    Q_PROPERTY(QString testProgressText READ testProgressText NOTIFY testProgressChanged)
    Q_PROPERTY(QString cloudUsername READ cloudUsername NOTIFY cloudSettingsChanged)
    Q_PROPERTY(QString cloudPassword READ cloudPassword NOTIFY cloudSettingsChanged)
    Q_PROPERTY(bool cloudAutoSyncEnabled READ cloudAutoSyncEnabled NOTIFY cloudSettingsChanged)
    Q_PROPERTY(bool cloudSyncInProgress READ cloudSyncInProgress NOTIFY cloudSyncInProgressChanged)
    Q_PROPERTY(QString executionMode READ executionMode NOTIFY executionSettingsChanged)
    Q_PROPERTY(QString remoteExecutionUrl READ remoteExecutionUrl NOTIFY executionSettingsChanged)
    Q_PROPERTY(QString remoteExecutionToken READ remoteExecutionToken NOTIFY executionSettingsChanged)
    Q_PROPERTY(bool remoteExecutionNgrokMode READ remoteExecutionNgrokMode NOTIFY executionSettingsChanged)

public:
    explicit ApplicationController(
        services::SubmissionService &submissionService,
        services::SubmissionImportService &submissionImportService,
        services::TestExecutionService &testExecutionService,
        services::CloudSyncService &cloudSyncService,
        services::ExecutionSettingsService &executionSettingsService,
        QObject *parent = nullptr
    );
    ~ApplicationController() override;

    models::SubmissionListModel *submissionsModel();
    models::ResultListModel *resultsModel();
    models::HistoryListModel *historyModel();
    QVariantList students() const;
    QVariantList labWorks() const;
    QVariantList selectedStudentSubmissions() const;

    int selectedStudentId() const;
    QString selectedStudentName() const;

    int totalSubmissions() const;
    int passedSubmissions() const;
    int failedSubmissions() const;
    int pendingSubmissions() const;
    QString lastRunMessage() const;
    bool testsRunning() const;
    int testProgressCurrent() const;
    int testProgressTotal() const;
    double testProgressValue() const;
    QString testProgressText() const;
    QString cloudUsername() const;
    QString cloudPassword() const;
    bool cloudAutoSyncEnabled() const;
    bool cloudSyncInProgress() const;
    QString executionMode() const;
    QString remoteExecutionUrl() const;
    QString remoteExecutionToken() const;
    bool remoteExecutionNgrokMode() const;

    Q_INVOKABLE void initialize();
    Q_INVOKABLE void runTests();
    Q_INVOKABLE void cancelTests();
    Q_INVOKABLE void reloadMockData();
    Q_INVOKABLE void clearSubmissions();
    Q_INVOKABLE void clearResults();
    Q_INVOKABLE void refreshHistory();
    Q_INVOKABLE void applyHistoryFilters(const QString &groupName, int studentId, int labId);
    Q_INVOKABLE void selectStudent(int studentId);
    Q_INVOKABLE bool addStudent(const QString &name, const QString &groupName);
    Q_INVOKABLE bool updateStudent(int studentId, const QString &name, const QString &groupName);
    Q_INVOKABLE bool removeStudent(int studentId);
    Q_INVOKABLE bool createLabWork(
        const QString &title,
        const QString &description,
        const QString &templatePath,
        const QString &testPath,
        const QString &language
    );
    Q_INVOKABLE bool updateLabWork(
        int labId,
        const QString &title,
        const QString &description,
        const QString &templatePath,
        const QString &testPath,
        const QString &language
    );
    Q_INVOKABLE bool removeLabWork(int labId);
    Q_INVOKABLE bool addSubmission(const QString &filePath, int studentId, int labId);
    Q_INVOKABLE bool removeSubmission(int submissionId);
    Q_INVOKABLE bool importDroppedFiles(const QVariantList &filePaths, int studentId, int labId);
    Q_INVOKABLE bool importBatchSubmissions(
        const QVariantList &items,
        int fallbackStudentId,
        int fallbackLabId
    );
    Q_INVOKABLE QVariantList resolveImportSources(const QVariantList &sourcePaths) const;
    Q_INVOKABLE QVariantList testCasesForSubmission(int submissionId) const;
    Q_INVOKABLE QVariantList testCasesForCheckRun(int checkRunId) const;
    Q_INVOKABLE QVariantMap labWorkById(int labId) const;
    Q_INVOKABLE QString readTextFilePreview(const QString &path) const;
    Q_INVOKABLE QString readTextFilePreviewLimited(const QString &path, int maxCharacters) const;
    Q_INVOKABLE bool setCloudUsername(const QString &username);
    Q_INVOKABLE bool setCloudPassword(const QString &password);
    Q_INVOKABLE bool setCloudAutoSyncEnabled(bool enabled);
    Q_INVOKABLE bool requestCloudAccounts();
    Q_INVOKABLE bool createCloudAccount(const QString &username, const QString &password);
    Q_INVOKABLE QVariantMap cloudVersionInfo(
        const QString &username = QString(),
        bool includeRemote = true
    ) const;
    Q_INVOKABLE bool requestCloudVersionInfo(
        const QString &username = QString(),
        bool includeRemote = true
    );
    Q_INVOKABLE bool requestCloudSaveHistory(const QString &username = QString());
    Q_INVOKABLE bool syncToCloud(const QString &username = QString(), const QString &password = QString());
    Q_INVOKABLE bool syncFromCloud(const QString &username = QString(), const QString &password = QString());
    Q_INVOKABLE bool setExecutionMode(const QString &mode);
    Q_INVOKABLE bool setRemoteExecutionUrl(const QString &url);
    Q_INVOKABLE bool setRemoteExecutionToken(const QString &token);
    Q_INVOKABLE bool setRemoteExecutionNgrokMode(bool enabled);
    Q_INVOKABLE QString readRuntimeLog(int maxChars = 120000) const;
    Q_INVOKABLE bool removeHistoryEntry(int checkRunId);
    Q_INVOKABLE void handleApplicationAboutToQuit();

signals:
    void studentsChanged();
    void labWorksChanged();
    void selectedStudentSubmissionsChanged();
    void selectedStudentChanged();
    void dashboardChanged();
    void lastRunMessageChanged();
    void testsRunningChanged();
    void testProgressChanged();
    void cloudSettingsChanged();
    void executionSettingsChanged();
    void cloudSyncInProgressChanged();
    void cloudVersionInfoReady(const QVariantMap &info);
    void cloudAccountsReady(const QVariantList &accounts, const QString &errorText);
    void cloudSaveHistoryReady(const QVariantList &history, const QString &errorText);

private slots:
    void onWorkerProgressChanged(int current, int total, const QString &label);
    void onWorkerSubmissionFinished(int current, int total, const labtester::domain::TestRunResult &result);
    void onWorkerFinished();

private:
    void refreshCatalog();
    void refreshSubmissions();
    void refreshResults();
    void refreshHistoryData();
    void refreshSelectedStudentSubmissions();
    void resetProgressState();
    void startTestWorker(std::vector<domain::Submission> submissions, const QString &modeOverride = QString());

    int countByStatus(domain::ExecutionStatus status) const;
    QVariantList buildStudentsVariant();
    QVariantList buildLabWorksVariant();
    QString findStudentName(int studentId) const;
    QString submissionSourcePathById(int submissionId) const;
    QVariantList buildTestCasesVariantList(const domain::TestRunResult &runResult) const;
    QString detectFunctionNameForTest(const QString &testName) const;
    QString extractFunctionCodeFromSource(const QString &sourceText, const QString &functionName) const;
    QString readTextFile(const QString &path) const;
    QString resolveCloudUsername(const QString &username) const;

    services::SubmissionService &m_submissionService;
    services::SubmissionImportService &m_submissionImportService;
    services::TestExecutionService &m_testExecutionService;
    services::CloudSyncService &m_cloudSyncService;
    services::ExecutionSettingsService &m_executionSettingsService;
    models::SubmissionListModel m_submissionsModel;
    models::ResultListModel m_resultsModel;
    models::HistoryListModel m_historyModel;

    QVariantList m_students;
    QVariantList m_labWorks;
    QVariantList m_selectedStudentSubmissions;
    int m_selectedStudentId {0};
    QString m_selectedStudentName;
    QString m_lastRunMessage;
    bool m_testsRunning {false};
    bool m_cancelRequested {false};
    int m_testProgressCurrent {0};
    int m_testProgressTotal {0};
    QString m_testProgressText;
    bool m_cloudSyncInProgress {false};
    QString m_historyGroupFilter;
    int m_historyStudentFilter {0};
    int m_historyLabFilter {0};
    QList<QThread *> m_testRunThreads;
    QList<TestRunWorker *> m_testRunWorkers;
    int m_expectedTestWorkers {0};
    int m_finishedTestWorkers {0};
    QThread *m_cloudOpThread {nullptr};
    mutable QHash<QString, QString> m_textPreviewCache;
};

} // namespace labtester::app
