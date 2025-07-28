#include <QtSql/QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QDebug>

#include "SQLiteStorage.hpp"

SQLiteStorage::SQLiteStorage(const QString &dbPath) {
    m_db = QSqlDatabase::addDatabase("QSQLITE");
    m_db.setDatabaseName(dbPath);
    if (!m_db.open()) {
        qWarning() << "Failed to open database:" << m_db.lastError().text();
    }
}

std::vector<Task> SQLiteStorage::getAllTasks() const {
    return {};
}

std::optional<Task> SQLiteStorage::getTaskById(qint64 id) const {
    return std::nullopt;
}

qint64 SQLiteStorage::addTask(const Task &task) {
    return -1;
}

bool SQLiteStorage::updateTask(qint64 id, const Task &task) {
    return false;
}

bool SQLiteStorage::deleteTask(qint64 id) {
    return false;
}

bool SQLiteStorage::deleteAll() {
    return false;
}
