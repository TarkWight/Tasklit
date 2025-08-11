#include <QtCore/QFile>
#include <QtCore/QTextStream>
#include <QtCore/QDateTime>
#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>
#include <QtCore/QDebug>
#include <QtHttpServer/QHttpServerRequest>

#include "Logger.hpp"

Q_LOGGING_CATEGORY(appCore, "tasklit.core")
Q_LOGGING_CATEGORY(appHttp, "tasklit.http")
Q_LOGGING_CATEGORY(appSql,  "tasklit.sql")

static QFile* g_logFile = nullptr;
static QMutex g_logMutex;

static void messageHandler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg)
{
    QString line = qFormatLogMessage(type, ctx, msg) + '\n';

    fprintf(stderr, "%s", line.toLocal8Bit().constData());

    if (g_logFile && g_logFile->isOpen()) {
        QMutexLocker lock(&g_logMutex);
        QTextStream ts(g_logFile);
        ts << line;
        ts.flush();
    }
}

void initLogging(const QString& filePath) {
    qSetMessagePattern("%{time yyyy-MM-dd hh:mm:ss.zzz} [%{type}] %{category} "
                       "(%{if-debug}%{function}:%{line}%{endif}): %{message}");

    if (!filePath.isEmpty()) {

        g_logFile = new QFile(filePath);
        if (!g_logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            delete g_logFile;
            g_logFile = nullptr;
            qWarning(appCore) << "Failed to open log file:" << filePath;
        }
    }

    qInstallMessageHandler(messageHandler);

    qInfo(appCore) << "Logging initialized"
                   << (g_logFile ? QString("-> %1").arg(filePath) : "(stderr only)");
}

const char* toString(QHttpServerRequest::Method m) {
    using M = QHttpServerRequest::Method;
    switch (m) {
    case M::Get: return "GET";
    case M::Post: return "POST";
    case M::Put: return "PUT";
    case M::Delete: return "DELETE";
    case M::Patch: return "PATCH";
    case M::Head: return "HEAD";
    case M::Options: return "OPTIONS";
    case M::Trace: return "TRACE";
    case M::Connect: return "CONNECT";
    default: return "UNKNOWN";
    }
}
