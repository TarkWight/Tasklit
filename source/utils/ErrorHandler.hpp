#ifndef ERRORHANDLER_HPP
#define ERRORHANDLER_HPP

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>
#include <QtHttpServer/QHttpServerRequest>
#include <QtHttpServer/QHttpServerResponse>
#include <functional>

#include "Logger.hpp"

inline QHttpServerResponse
makeApiError(QHttpServerResponse::StatusCode status, const QString &message,
             const QString &type = QStringLiteral("error"),
             QJsonObject details = {}, const QString &requestId = {}) {
    QJsonObject obj{
                    {"ok", false},
                    {"type", type},
                    {"message", message},
                    {"status", static_cast<int>(status)},
                    {"requestId", requestId.isEmpty()
                                      ? QUuid::createUuid().toString(QUuid::WithoutBraces)
                                      : requestId},
                    {"ts", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}};
    if (!details.isEmpty())
        obj.insert("details", details);

    return QHttpServerResponse(obj, status);
}

inline QHttpServerResponse makeApiOk(const QString &message = QString(),
                                     QJsonObject data = {},
                                     const QString &requestId = {},
                                     QHttpServerResponse::StatusCode status = QHttpServerResponse::StatusCode::Ok) {
    QJsonObject obj{
        {"ok", true},
        {"message", message},
        {"requestId", requestId.isEmpty()
                          ? QUuid::createUuid().toString(QUuid::WithoutBraces)
                          : requestId},
        {"ts", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}
    };
    if (!data.isEmpty())
        obj.insert("data", data);
    return QHttpServerResponse(obj, status);
}

template <typename Fn> auto wrapSafe(const char *routeName, Fn fn) {
    return [routeName, fn](auto &&...args) -> QHttpServerResponse {
        const QString requestId =
            QUuid::createUuid().toString(QUuid::WithoutBraces);
        const auto started = QDateTime::currentMSecsSinceEpoch();
        try {
            auto resp = fn(std::forward<decltype(args)>(args)...,
                           requestId); // см. сигнатуру ниже
            const auto elapsed = QDateTime::currentMSecsSinceEpoch() - started;
            qInfo(appHttp) << "[DONE]" << routeName
                           << "| requestId=" << requestId << "| ms=" << elapsed;
            return resp;
        } catch (const std::exception &e) {
            const auto elapsed = QDateTime::currentMSecsSinceEpoch() - started;
            qCritical(appHttp)
                << "[EXC]" << routeName << "| requestId=" << requestId
                << "| ms=" << elapsed << "| what=" << e.what();
            return makeApiError(
                QHttpServerResponse::StatusCode::InternalServerError,
                QStringLiteral("Internal error"),
                QStringLiteral("internal_error"),
                QJsonObject{{"what", e.what()}}, requestId);
        } catch (...) {
            const auto elapsed = QDateTime::currentMSecsSinceEpoch() - started;
            qCritical(appHttp)
                << "[EXC]" << routeName << "| requestId=" << requestId
                << "| ms=" << elapsed << "| unknown exception";
            return makeApiError(
                QHttpServerResponse::StatusCode::InternalServerError,
                QStringLiteral("Internal error"),
                QStringLiteral("internal_error"),
                QJsonObject{{"what", "unknown"}}, requestId);
        }
    };
}

inline void sendNotFound(QHttpServerResponder &responder,
                         const QHttpServerRequest &request) {
    const QString methodString = toString(request.method());
    const QString urlString = request.url().toString();
    qWarning(appHttp) << "404 no route for" << methodString << urlString;

    QHttpServerResponse response = makeApiError(
        QHttpServerResponse::StatusCode::NotFound,
        QStringLiteral("Route not found"), QStringLiteral("not_found"),
        QJsonObject{{"method", methodString},
                    {"path", urlString},
                    {"hint", "Check path, HTTP method and trailing slash"}});
    responder.sendResponse(response);
}

// ─────────────────────────────────────────────────────────────────────────────
// Safe wrapper overloads с фиксированными сигнатурами
// ─────────────────────────────────────────────────────────────────────────────

inline auto wrapSafe(const char *routeName,
                     std::function<QHttpServerResponse(const QString &requestId)> fn) {
    return [routeName, fn]() -> QHttpServerResponse {
        const QString requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        const qint64 started = QDateTime::currentMSecsSinceEpoch();
        try {
            auto resp = fn(requestId);
            qInfo(appHttp) << "[DONE]" << routeName
                           << "| requestId=" << requestId
                           << "| ms=" << (QDateTime::currentMSecsSinceEpoch() - started);
            return resp;
        } catch (const std::exception &e) {
            qCritical(appHttp) << "[EXC]" << routeName
                               << "| requestId=" << requestId
                               << "| what=" << e.what();
            return makeApiError(QHttpServerResponse::StatusCode::InternalServerError,
                                "Internal error", "internal_error",
                                QJsonObject{{"what", e.what()}}, requestId);
        } catch (...) {
            qCritical(appHttp) << "[EXC]" << routeName
                               << "| requestId=" << requestId
                               << "| unknown exception";
            return makeApiError(QHttpServerResponse::StatusCode::InternalServerError,
                                "Internal error", "internal_error",
                                QJsonObject{{"what", "unknown"}}, requestId);
        }
    };
}

inline auto wrapSafe(const char *routeName,
                     std::function<QHttpServerResponse(const QHttpServerRequest &request,
                                                       const QString &requestId)> fn) {
    return [routeName, fn](const QHttpServerRequest &request) -> QHttpServerResponse {
        const QString requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        const qint64 started = QDateTime::currentMSecsSinceEpoch();
        try {
            auto resp = fn(request, requestId);
            qInfo(appHttp) << "[DONE]" << routeName
                           << "| requestId=" << requestId
                           << "| ms=" << (QDateTime::currentMSecsSinceEpoch() - started);
            return resp;
        } catch (const std::exception &e) {
            qCritical(appHttp) << "[EXC]" << routeName
                               << "| requestId=" << requestId
                               << "| what=" << e.what();
            return makeApiError(QHttpServerResponse::StatusCode::InternalServerError,
                                "Internal error", "internal_error",
                                QJsonObject{{"what", e.what()}}, requestId);
        } catch (...) {
            qCritical(appHttp) << "[EXC]" << routeName
                               << "| requestId=" << requestId
                               << "| unknown exception";
            return makeApiError(QHttpServerResponse::StatusCode::InternalServerError,
                                "Internal error", "internal_error",
                                QJsonObject{{"what", "unknown"}}, requestId);
        }
    };
}

inline auto wrapSafe(const char *routeName,
                     std::function<QHttpServerResponse(qint64 pathId,
                                                       const QHttpServerRequest &request,
                                                       const QString &requestId)> fn) {
    return [routeName, fn](qint64 pathId, const QHttpServerRequest &request) -> QHttpServerResponse {
        const QString requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        const qint64 started = QDateTime::currentMSecsSinceEpoch();
        try {
            auto resp = fn(pathId, request, requestId);
            qInfo(appHttp) << "[DONE]" << routeName
                           << "| requestId=" << requestId
                           << "| ms=" << (QDateTime::currentMSecsSinceEpoch() - started);
            return resp;
        } catch (const std::exception &e) {
            qCritical(appHttp) << "[EXC]" << routeName
                               << "| requestId=" << requestId
                               << "| what=" << e.what();
            return makeApiError(QHttpServerResponse::StatusCode::InternalServerError,
                                "Internal error", "internal_error",
                                QJsonObject{{"what", e.what()}}, requestId);
        } catch (...) {
            qCritical(appHttp) << "[EXC]" << routeName
                               << "| requestId=" << requestId
                               << "| unknown exception";
            return makeApiError(QHttpServerResponse::StatusCode::InternalServerError,
                                "Internal error", "internal_error",
                                QJsonObject{{"what", "unknown"}}, requestId);
        }
    };
}

inline auto wrapSafe(const char *routeName,
                     std::function<QHttpServerResponse(const QString &pathArg,
                                                       const QHttpServerRequest &request,
                                                       const QString &requestId)> fn) {
    return [routeName, fn](const QString &pathArg, const QHttpServerRequest &request) -> QHttpServerResponse {
        const QString requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        const qint64 started = QDateTime::currentMSecsSinceEpoch();
        try {
            auto resp = fn(pathArg, request, requestId);
            qInfo(appHttp) << "[DONE]" << routeName
                           << "| requestId=" << requestId
                           << "| ms=" << (QDateTime::currentMSecsSinceEpoch() - started);
            return resp;
        } catch (const std::exception &e) {
            qCritical(appHttp) << "[EXC]" << routeName
                               << "| requestId=" << requestId
                               << "| what=" << e.what();
            return makeApiError(QHttpServerResponse::StatusCode::InternalServerError,
                                "Internal error", "internal_error",
                                QJsonObject{{"what", e.what()}}, requestId);
        } catch (...) {
            qCritical(appHttp) << "[EXC]" << routeName
                               << "| requestId=" << requestId
                               << "| unknown exception";
            return makeApiError(QHttpServerResponse::StatusCode::InternalServerError,
                                "Internal error", "internal_error",
                                QJsonObject{{"what", "unknown"}}, requestId);
        }
    };
}
#endif // ERRORHANDLER_HPP
