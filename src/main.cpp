#include <QCoreApplication>

#if defined(__MINGW32__)
int __argc;
char **__argv;
#endif

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QGuiApplication>
#include <QIcon>
#include <QKeyEvent>
#include <QAbstractNativeEventFilter>
#include <QMutex>
#include <QMutexLocker>
#include <QPointer>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlError>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QStringConverter>
#include <QtSql/QSqlDatabase>
#include <QTextStream>
#include <QTimer>
#include <QVariant>
#include <memory>

#include "app/ApplicationController.h"
#include "database/DatabaseManager.h"
#include "database/ResultRepository.h"
#include "database/SubmissionRepository.h"
#include "runners/CppGTestRunner.h"
#include "runners/RemoteWorkerRunner.h"
#include "services/ReportService.h"
#include "services/CloudSyncService.h"
#include "services/ExecutionSettingsService.h"
#include "services/SubmissionImportService.h"
#include "services/SubmissionService.h"
#include "services/TestExecutionService.h"
#include "services/WorkspaceService.h"

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dwmapi.h>

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

#ifndef DWMWA_BORDER_COLOR
#define DWMWA_BORDER_COLOR 34
#endif

#ifndef DWMWA_CAPTION_COLOR
#define DWMWA_CAPTION_COLOR 35
#endif

#ifndef DWMWA_TEXT_COLOR
#define DWMWA_TEXT_COLOR 36
#endif
#endif

namespace {

void appendStartupLog(const QString &line);

QString firstExistingPostgresBinFromRoot(const QString &rootPath)
{
    QDir root(rootPath);
    if (!root.exists()) {
        return {};
    }

    const QFileInfoList dirs = root.entryInfoList(
        QDir::Dirs | QDir::NoDotAndDotDot,
        QDir::Name | QDir::Reversed
    );
    for (const QFileInfo &dirInfo : dirs) {
        const QString binPath = QDir(dirInfo.absoluteFilePath()).filePath(QStringLiteral("bin"));
        if (QFileInfo::exists(QDir(binPath).filePath(QStringLiteral("libpq.dll")))) {
            return QDir::toNativeSeparators(binPath);
        }
    }
    return {};
}

QString detectPostgresBinPath()
{
    const QString appDir = QCoreApplication::applicationDirPath().trimmed();
    if (!appDir.isEmpty()) {
        const QString bundledLibpqPath = QDir(appDir).filePath(QStringLiteral("libpq.dll"));
        if (QFileInfo::exists(bundledLibpqPath)) {
            return QDir::toNativeSeparators(appDir);
        }
    }

    const QStringList envBinCandidates {
        QString::fromLocal8Bit(qgetenv("LABTESTER_PG_BIN")).trimmed(),
        QString::fromLocal8Bit(qgetenv("PG_BIN")).trimmed(),
        QString::fromLocal8Bit(qgetenv("PGBIN")).trimmed(),
    };
    for (const QString &candidate : envBinCandidates) {
        if (candidate.isEmpty()) {
            continue;
        }
        const QString libpqPath = QDir(candidate).filePath(QStringLiteral("libpq.dll"));
        if (QFileInfo::exists(libpqPath)) {
            return QDir::toNativeSeparators(candidate);
        }
    }

    const QString postgresHome = QString::fromLocal8Bit(qgetenv("POSTGRESQL_HOME")).trimmed();
    if (!postgresHome.isEmpty()) {
        const QString binPath = QDir(postgresHome).filePath(QStringLiteral("bin"));
        if (QFileInfo::exists(QDir(binPath).filePath(QStringLiteral("libpq.dll")))) {
            return QDir::toNativeSeparators(binPath);
        }
    }

    const QString fromProgramFiles = firstExistingPostgresBinFromRoot(QStringLiteral("C:/Program Files/PostgreSQL"));
    if (!fromProgramFiles.isEmpty()) {
        return fromProgramFiles;
    }
    return firstExistingPostgresBinFromRoot(QStringLiteral("C:/Program Files (x86)/PostgreSQL"));
}

void ensurePostgresRuntimePath()
{
    const QString postgresBin = detectPostgresBinPath();
    if (postgresBin.isEmpty()) {
        appendStartupLog(QStringLiteral("postgresBin=NOT_FOUND"));
        return;
    }

    QString pathValue = QString::fromLocal8Bit(qgetenv("PATH"));
    const Qt::CaseSensitivity cs = Qt::CaseInsensitive;
    if (!pathValue.contains(postgresBin, cs)) {
        if (!pathValue.isEmpty() && !pathValue.endsWith(QLatin1Char(';'))) {
            pathValue.append(QLatin1Char(';'));
        }
        pathValue.append(postgresBin);
        qputenv("PATH", pathValue.toLocal8Bit());
    }
    appendStartupLog(QStringLiteral("postgresBin=%1").arg(postgresBin));
}

class NavigationKeyFilter final : public QObject
{
public:
    explicit NavigationKeyFilter(QObject *rootObject, QObject *parent = nullptr)
        : QObject(parent)
        , m_rootObject(rootObject)
    {
        qDebug() << "[NAV][Filter] created, rootObject=" << m_rootObject.data();
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        Q_UNUSED(watched);

        if (!m_rootObject
            || (event->type() != QEvent::KeyPress && event->type() != QEvent::ShortcutOverride)) {
            return QObject::eventFilter(watched, event);
        }

        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (event->type() == QEvent::KeyPress && keyEvent->isAutoRepeat()) {
            return QObject::eventFilter(watched, event);
        }

        const auto modifiers = keyEvent->modifiers();
        const bool allowShiftOnly = modifiers == Qt::ShiftModifier;
        if (modifiers != Qt::NoModifier && !allowShiftOnly) {
            return QObject::eventFilter(watched, event);
        }

        const QString keyText = keyEvent->text().trimmed().toLower();
        QObject *focusObject = QGuiApplication::focusObject();
        const bool isTextInputFocused = focusObject
            && focusObject->property("cursorPosition").isValid()
            && (focusObject->property("selectedText").isValid()
                || focusObject->property("selectionStart").isValid()
                || focusObject->property("selectionEnd").isValid());

        if (isTextInputFocused) {
            qDebug().noquote()
                << QStringLiteral("[NAV][Filter] bypass key=%1 text='%2' because text input is focused (%3)")
                       .arg(keyEvent->key())
                       .arg(keyText)
                       .arg(QString::fromLatin1(focusObject->metaObject()->className()));
            return QObject::eventFilter(watched, event);
        }

        bool horizontal = true;
        int step = 0;
        bool handled = false;

        switch (keyEvent->key()) {
        case Qt::Key_Tab:
            horizontal = true;
            step = 1;
            handled = true;
            break;
        case Qt::Key_Backtab:
            horizontal = true;
            step = -1;
            handled = true;
            break;
        case Qt::Key_A:
            horizontal = true;
            step = -1;
            handled = true;
            break;
        case Qt::Key_D:
            horizontal = true;
            step = 1;
            handled = true;
            break;
        case Qt::Key_W:
            horizontal = false;
            step = -1;
            handled = true;
            break;
        case Qt::Key_S:
            horizontal = false;
            step = 1;
            handled = true;
            break;
        default:
            break;
        }

        if (!handled) {
            if (keyText == QStringLiteral("a") || keyText == QStringLiteral("ф")) {
                horizontal = true;
                step = -1;
                handled = true;
            } else if (keyText == QStringLiteral("d") || keyText == QStringLiteral("в")) {
                horizontal = true;
                step = 1;
                handled = true;
            } else if (keyText == QStringLiteral("w") || keyText == QStringLiteral("ц")) {
                horizontal = false;
                step = -1;
                handled = true;
            } else if (keyText == QStringLiteral("s") || keyText == QStringLiteral("ы")) {
                horizontal = false;
                step = 1;
                handled = true;
            }
        }

        if (!handled) {
            return QObject::eventFilter(watched, event);
        }

        const char *methodName = horizontal ? "routeHorizontal" : "routeVertical";
        const bool invoked = QMetaObject::invokeMethod(
            m_rootObject,
            methodName,
            Qt::DirectConnection,
            Q_ARG(QVariant, QVariant(step))
        );

        qDebug().noquote()
            << QStringLiteral("[NAV][Filter] event=%1 watched=%2 key=%3 text='%4' modifiers=%5 -> %6(%7), invoked=%8")
                   .arg(event->type() == QEvent::ShortcutOverride ? QStringLiteral("ShortcutOverride") : QStringLiteral("KeyPress"))
                   .arg(watched ? QString::fromLatin1(watched->metaObject()->className()) : QStringLiteral("null"))
                   .arg(keyEvent->key())
                   .arg(keyText)
                   .arg(static_cast<int>(modifiers))
                   .arg(QString::fromLatin1(methodName))
                   .arg(step)
                   .arg(invoked ? QStringLiteral("true") : QStringLiteral("false"));

        if (invoked) {
            keyEvent->accept();
            return true;
        }

        return QObject::eventFilter(watched, event);
    }

private:
    QPointer<QObject> m_rootObject;
};

#ifdef Q_OS_WIN
class NavigationNativeKeyFilter final : public QAbstractNativeEventFilter
{
public:
    explicit NavigationNativeKeyFilter(QObject *rootObject)
        : m_rootObject(rootObject)
    {
        qDebug() << "[NAV][NativeFilter] created, rootObject=" << m_rootObject.data();
    }

    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override
    {
        Q_UNUSED(result);

        if (!m_rootObject) {
            return false;
        }
        if (eventType != "windows_generic_MSG" && eventType != "windows_dispatcher_MSG") {
            return false;
        }

        auto *msg = static_cast<MSG *>(message);
        if (!msg || (msg->message != WM_KEYDOWN && msg->message != WM_SYSKEYDOWN)) {
            return false;
        }

        bool horizontal = true;
        int step = 0;
        bool handled = false;

        switch (msg->wParam) {
        case VK_TAB:
            horizontal = true;
            step = (GetKeyState(VK_SHIFT) & 0x8000) ? -1 : 1;
            handled = true;
            break;
        case 'A':
            horizontal = true;
            step = -1;
            handled = true;
            break;
        case 'D':
            horizontal = true;
            step = 1;
            handled = true;
            break;
        case 'W':
            horizontal = false;
            step = -1;
            handled = true;
            break;
        case 'S':
            horizontal = false;
            step = 1;
            handled = true;
            break;
        default:
            break;
        }

        if (!handled) {
            return false;
        }

        QObject *focusObject = QGuiApplication::focusObject();
        const bool isTextInputFocused = focusObject
            && focusObject->property("cursorPosition").isValid()
            && (focusObject->property("selectedText").isValid()
                || focusObject->property("selectionStart").isValid()
                || focusObject->property("selectionEnd").isValid());
        if (isTextInputFocused) {
            qDebug().noquote()
                << QStringLiteral("[NAV][NativeFilter] bypass VK=%1 due text input focus (%2)")
                       .arg(static_cast<int>(msg->wParam))
                       .arg(QString::fromLatin1(focusObject->metaObject()->className()));
            return false;
        }

        const char *methodName = horizontal ? "routeHorizontal" : "routeVertical";
        const bool invoked = QMetaObject::invokeMethod(
            m_rootObject,
            methodName,
            Qt::DirectConnection,
            Q_ARG(QVariant, QVariant(step))
        );

        qDebug().noquote()
            << QStringLiteral("[NAV][NativeFilter] msg=%1 VK=%2 -> %3(%4), invoked=%5")
                   .arg(static_cast<qulonglong>(msg->message))
                   .arg(static_cast<int>(msg->wParam))
                   .arg(QString::fromLatin1(methodName))
                   .arg(step)
                   .arg(invoked ? QStringLiteral("true") : QStringLiteral("false"));

        return invoked;
    }

private:
    QPointer<QObject> m_rootObject;
};
#endif

QMutex g_logMutex;

QString startupLogPath()
{
    const QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(baseDir);
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }
    return dir.filePath(QStringLiteral("startup.log"));
}

void appendStartupLog(const QString &line)
{
    QMutexLocker locker(&g_logMutex);

    QFile file(startupLogPath());
    if (!file.open(QIODevice::Append | QIODevice::Text)) {
        return;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << line << Qt::endl;
}

void qtMessageHandler(QtMsgType type, const QMessageLogContext &, const QString &message)
{
    QString typeLabel = QStringLiteral("DEBUG");
    switch (type) {
    case QtDebugMsg:
        typeLabel = QStringLiteral("DEBUG");
        break;
    case QtInfoMsg:
        typeLabel = QStringLiteral("INFO");
        break;
    case QtWarningMsg:
        typeLabel = QStringLiteral("WARN");
        break;
    case QtCriticalMsg:
        typeLabel = QStringLiteral("CRIT");
        break;
    case QtFatalMsg:
        typeLabel = QStringLiteral("FATAL");
        break;
    }

    appendStartupLog(QStringLiteral("%1 [%2] %3")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")))
        .arg(typeLabel)
        .arg(message));

    if (type == QtFatalMsg) {
        abort();
    }
}

#ifdef Q_OS_WIN
void applyWindowsCaptionStyle(QQuickWindow *window)
{
    if (!window) {
        return;
    }

    const auto hwnd = reinterpret_cast<HWND>(window->winId());
    if (!hwnd) {
        return;
    }

    const BOOL useDarkCaption = TRUE;
    DwmSetWindowAttribute(
        hwnd,
        DWMWA_USE_IMMERSIVE_DARK_MODE,
        &useDarkCaption,
        sizeof(useDarkCaption)
    );

    const COLORREF captionColor = RGB(11, 16, 24);
    const COLORREF captionTextColor = RGB(237, 244, 255);
    const COLORREF borderColor = RGB(43, 69, 101);

    DwmSetWindowAttribute(hwnd, DWMWA_CAPTION_COLOR, &captionColor, sizeof(captionColor));
    DwmSetWindowAttribute(hwnd, DWMWA_TEXT_COLOR, &captionTextColor, sizeof(captionTextColor));
    DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &borderColor, sizeof(borderColor));
}
#endif

} // namespace

int main(int argc, char *argv[])
{
    qInstallMessageHandler(qtMessageHandler);
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("LabTester"));
    QCoreApplication::setApplicationName(QStringLiteral("LabTester"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.2v"));

    ensurePostgresRuntimePath();

    appendStartupLog(QStringLiteral("===== STARTUP ====="));
    appendStartupLog(QStringLiteral("cwd=%1").arg(QDir::currentPath()));
    appendStartupLog(QStringLiteral("appData=%1")
        .arg(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)));
    appendStartupLog(QStringLiteral("sqlDrivers=%1").arg(QSqlDatabase::drivers().join(", ")));

    QIcon appIcon(QStringLiteral(":/resources/icons/labtester_icon.ico"));
    if (appIcon.isNull()) {
        appIcon = QIcon(QStringLiteral(":/resources/icons/labtester_icon.svg"));
    }
    app.setWindowIcon(appIcon);

    labtester::database::DatabaseManager databaseManager;
    if (!databaseManager.initialize()) {
        qWarning() << "Failed to initialize SQLite database. Continuing with in-memory data.";
    }

    labtester::database::SubmissionRepository submissionRepository(databaseManager);
    labtester::database::ResultRepository resultRepository(databaseManager);

    labtester::services::SubmissionService submissionService(submissionRepository);
    labtester::services::SubmissionImportService submissionImportService(submissionService);
    labtester::services::WorkspaceService workspaceService;
    labtester::services::ReportService reportService;
    labtester::services::CloudSyncService cloudSyncService(databaseManager);
    labtester::services::ExecutionSettingsService executionSettingsService(databaseManager);
    labtester::runners::CppGTestRunner cppGTestRunner;
    labtester::runners::RemoteWorkerRunner remoteWorkerRunner(executionSettingsService);

    labtester::services::TestExecutionService testExecutionService(
        submissionRepository,
        resultRepository,
        cppGTestRunner,
        remoteWorkerRunner,
        executionSettingsService,
        workspaceService,
        reportService
    );

    labtester::app::ApplicationController appController(
        submissionService,
        submissionImportService,
        testExecutionService,
        cloudSyncService,
        executionSettingsService
    );
    QObject::connect(
        &app,
        &QCoreApplication::aboutToQuit,
        &appController,
        &labtester::app::ApplicationController::handleApplicationAboutToQuit
    );
    appController.initialize();

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("appController"), &appController);

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::warnings,
        &app,
        [](const QList<QQmlError> &warnings) {
            for (const auto &warning : warnings) {
                appendStartupLog(QStringLiteral("QML warning: %1").arg(warning.toString()));
            }
        }
    );

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() {
            appendStartupLog(QStringLiteral("QML objectCreationFailed"));
            QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection
    );

    engine.loadFromModule(QStringLiteral("LabTester"), QStringLiteral("Main"));
    if (engine.rootObjects().isEmpty()) {
        appendStartupLog(QStringLiteral("Root object list is empty after loadFromModule"));
        return -1;
    }

    QObject *rootObject = engine.rootObjects().constFirst();
    const bool probeHorizontal = QMetaObject::invokeMethod(
        rootObject,
        "routeHorizontal",
        Qt::DirectConnection,
        Q_ARG(QVariant, QVariant(0))
    );
    const bool probeVertical = QMetaObject::invokeMethod(
        rootObject,
        "routeVertical",
        Qt::DirectConnection,
        Q_ARG(QVariant, QVariant(0))
    );
    qDebug() << "[NAV][Probe] routeHorizontal(0)=" << probeHorizontal
             << "routeVertical(0)=" << probeVertical;

    auto *navigationKeyFilter = new NavigationKeyFilter(rootObject, &app);
    app.installEventFilter(navigationKeyFilter);
    rootObject->installEventFilter(navigationKeyFilter);
    qDebug() << "[NAV][Filter] installed on app and rootObject";

#ifdef Q_OS_WIN
    auto navigationNativeKeyFilter = std::make_unique<NavigationNativeKeyFilter>(rootObject);
    app.installNativeEventFilter(navigationNativeKeyFilter.get());
    qDebug() << "[NAV][NativeFilter] installed on app";
#endif

    if (auto *window = qobject_cast<QQuickWindow *>(rootObject)) {
        window->installEventFilter(navigationKeyFilter);
        qDebug() << "[NAV][Filter] installed on QQuickWindow";
        window->setIcon(appIcon);
#ifdef Q_OS_WIN
        QTimer::singleShot(0, window, [window]() {
            applyWindowsCaptionStyle(window);
        });
#endif
    }

    appendStartupLog(QStringLiteral("QML loaded successfully"));
    return app.exec();
}

