#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <QtHttpServer/QHttpServer>
#include <QtCore/QLoggingCategory>
#include <QtCore/QString>

Q_DECLARE_LOGGING_CATEGORY(appCore)
Q_DECLARE_LOGGING_CATEGORY(appHttp)
Q_DECLARE_LOGGING_CATEGORY(appSql)

void initLogging(const QString& filePath = QString());

const char* toString(QHttpServerRequest::Method m);

#endif // LOGGER_HPP
