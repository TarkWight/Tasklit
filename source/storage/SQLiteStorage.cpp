#include "SQLiteStorage.hpp"

#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlRecord>
#include <QVariant>
#include <QJsonArray>

#include "Logger.hpp"

namespace {

bool ensureSchema(QSqlDatabase db) {
    QSqlQuery q(db);

    qInfo(appSql) << "Ensuring DB schema...";

    // tasks
    if (!q.exec(
            "CREATE TABLE IF NOT EXISTS tasks ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  title TEXT NOT NULL,"
            "  description TEXT NOT NULL,"
            "  completed INTEGER NOT NULL DEFAULT 0"
            ");"
            )) {
        qCritical(appSql) << "schema tasks:" << q.lastError().text();
        return false;
    }

    // tags
    if (!q.exec(
            "CREATE TABLE IF NOT EXISTS tags ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  name TEXT NOT NULL UNIQUE"
            ");"
            )) {
        qCritical(appSql) << "schema tags:" << q.lastError().text();
        return false;
    }

    // m2m
    if (!q.exec(
            "CREATE TABLE IF NOT EXISTS task_tags ("
            "  task_id INTEGER NOT NULL,"
            "  tag_id  INTEGER NOT NULL,"
            "  PRIMARY KEY (task_id, tag_id),"
            "  FOREIGN KEY(task_id) REFERENCES tasks(id) ON DELETE CASCADE,"
            "  FOREIGN KEY(tag_id)  REFERENCES tags(id)  ON DELETE CASCADE"
            ");"
            )) {
        qCritical(appSql) << "schema task_tags:" << q.lastError().text();
        return false;
    }

    qInfo(appSql) << "Schema OK";
    return true;
}

static QList<Tag> fetchTagsForTask(QSqlDatabase db, qint64 taskId) {
    QList<Tag> out;
    QSqlQuery q(db);

    q.prepare(
        "SELECT t.id, t.name "
        "FROM tags t "
        "JOIN task_tags tt ON tt.tag_id = t.id "
        "WHERE tt.task_id = ?"
        );

    q.addBindValue(taskId);

    if (!q.exec()) {
        qWarning(appSql) << "fetchTagsForTask:" << q.lastError().text();
        return out;
    }

    while (q.next()) {
        Tag t;
        t.id = q.value(0).toLongLong();
        t.name = q.value(1).toString();
        out.append(t);
    }

    qInfo(appSql) << "Fetched" << out.size() << "tags for task" << taskId;
    return out;
}

static QList<qint64> upsertTagsByName(QSqlDatabase db, const QList<Tag>& tags) {
    QList<qint64> ids;
    QSqlQuery sel(db), ins(db);

    sel.prepare("SELECT id FROM tags WHERE name = ?");
    ins.prepare("INSERT INTO tags(name) VALUES(?)");

    for (const auto& t : tags) {
        sel.bindValue(0, t.name);

        if (!sel.exec()) {
            qWarning(appSql) << "tag select:" << sel.lastError().text();
            continue;
        }

        if (sel.next()) {
            auto id = sel.value(0).toLongLong();
            ids.append(id);
            qInfo(appSql) << "Tag exists:" << t.name << "id=" << id;
            continue;
        }

        ins.bindValue(0, t.name);

        if (!ins.exec()) {
            qWarning(appSql) << "tag insert failed:" << ins.lastError().text();
            // ещё раз селект
            sel.bindValue(0, t.name);
            if (sel.exec() && sel.next()) {
                ids.append(sel.value(0).toLongLong());
            }
            continue;
        }

        auto newId = ins.lastInsertId().toLongLong();
        ids.append(newId);
        qInfo(appSql) << "Tag inserted:" << t.name << "id=" << newId;
    }

    return ids;
}

static bool replaceTaskTags(QSqlDatabase db, qint64 taskId, const QList<qint64>& tagIds) {
    QSqlQuery del(db), add(db);

    if (!del.exec(QStringLiteral("DELETE FROM task_tags WHERE task_id = %1").arg(taskId))) {
        qWarning(appSql) << "clear task_tags:" << del.lastError().text();
        return false;
    }

    add.prepare("INSERT OR IGNORE INTO task_tags(task_id, tag_id) VALUES(?, ?)");

    for (auto id : tagIds) {
        add.bindValue(0, taskId);
        add.bindValue(1, id);
        if (!add.exec()) {
            qWarning(appSql) << "add tag link:" << add.lastError().text();
            return false;
        }
    }

    qInfo(appSql) << "Updated" << tagIds.size() << "tag links for task" << taskId;
    return true;
}

static Task rowToTask(const QSqlRecord& r) {
    Task t;
    t.id = r.value("id").toLongLong();
    t.title = r.value("title").toString();
    t.description = r.value("description").toString();
    t.completed = r.value("completed").toInt() != 0;
    return t;
}

} // END NAMESPACE

SQLiteStorage::SQLiteStorage(const QString &dbPath) {
    m_db = QSqlDatabase::addDatabase("QSQLITE");
    m_db.setDatabaseName(dbPath);

    if (!m_db.open()) {
        qCritical(appSql) << "Failed to open database:" << m_db.lastError().text();
        return;
    }

    if (!ensureSchema(m_db)) {
        qCritical(appSql) << "Failed to init schema";
    }

    QSqlQuery pragma(m_db);
    pragma.exec("PRAGMA foreign_keys = ON;");
    qInfo(appSql) << "SQLiteStorage ready, path:" << dbPath;
}

std::vector<Task> SQLiteStorage::getAllTasks() const {
    qInfo(appSql) << "Query: getAllTasks()";
    std::vector<Task> out;
    QSqlQuery q(m_db);
    if (!q.exec("SELECT id, title, description, completed FROM tasks ORDER BY id ASC")) {
        qWarning(appSql) << "getAllTasks:" << q.lastError().text();
        return out;
    }

    while (q.next()) {
        Task t = rowToTask(q.record());
        t.tags = fetchTagsForTask(m_db, t.id);
        out.push_back(std::move(t));
    }

    qInfo(appSql) << "→" << out.size() << "tasks fetched";
    return out;
}

std::optional<Task> SQLiteStorage::getTaskById(qint64 id) const {
    qInfo(appSql) << "Query: getTaskById id=" << id;
    QSqlQuery q(m_db);
    q.prepare("SELECT id, title, description, completed FROM tasks WHERE id = ?");
    q.addBindValue(id);

    if (!q.exec()) {
        qWarning(appSql) << "getTaskById:" << q.lastError().text();
        return std::nullopt;
    }

    if (!q.next()) {
        qInfo(appSql) << "Task not found id=" << id;
        return std::nullopt;
    }

    Task t = rowToTask(q.record());
    t.tags = fetchTagsForTask(m_db, t.id);

    return t;
}

qint64 SQLiteStorage::addTask(const Task &task) {
    qInfo(appSql) << "Insert task title=" << task.title << "tags=" << task.tags.size();
    QSqlQuery q(m_db);

    if (!m_db.transaction()) {
        qWarning(appSql) << "tx begin:" << m_db.lastError().text();
    }

    q.prepare("INSERT INTO tasks(title, description, completed) VALUES(?, ?, ?)");
    q.addBindValue(task.title);
    q.addBindValue(task.description);
    q.addBindValue(task.completed ? 1 : 0);

    if (!q.exec()) {
        qCritical(appSql) << "addTask:" << q.lastError().text();
        m_db.rollback();
        return -1;
    }

    qint64 taskId = q.lastInsertId().toLongLong();
    auto tagIds = upsertTagsByName(m_db, task.tags);

    if (!replaceTaskTags(m_db, taskId, tagIds)) {
        m_db.rollback();
        return -1;
    }

    if (!m_db.commit()) {
        qCritical(appSql) << "tx commit:" << m_db.lastError().text();
        return -1;
    }

    qInfo(appSql) << "Task inserted id=" << taskId;
    return taskId;
}

bool SQLiteStorage::updateTask(qint64 id, const Task &task) {
    qInfo(appSql) << "Update task id=" << id;
    if (!m_db.transaction()) {
        qWarning(appSql) << "tx begin:" << m_db.lastError().text();
    }

    QSqlQuery q(m_db);
    q.prepare("UPDATE tasks SET title = ?, description = ?, completed = ? WHERE id = ?");
    q.addBindValue(task.title);
    q.addBindValue(task.description);
    q.addBindValue(task.completed ? 1 : 0);
    q.addBindValue(id);

    if (!q.exec()) {
        qCritical(appSql) << "updateTask:" << q.lastError().text();
        m_db.rollback();
        return false;
    }

    if (q.numRowsAffected() == 0) {
        qInfo(appSql) << "No rows updated for id=" << id;
        m_db.rollback();
        return false;
    }

    auto tagIds = upsertTagsByName(m_db, task.tags);
    if (!replaceTaskTags(m_db, id, tagIds)) {
        m_db.rollback();
        return false;
    }

    if (!m_db.commit()) {
        qCritical(appSql) << "tx commit:" << m_db.lastError().text();
        return false;
    }

    qInfo(appSql) << "Task updated id=" << id;
    return true;
}

bool SQLiteStorage::deleteTask(qint64 id) {
    qInfo(appSql) << "Delete task id=" << id;
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM tasks WHERE id = ?");
    q.addBindValue(id);

    if (!q.exec()) {
        qWarning(appSql) << "deleteTask:" << q.lastError().text();
        return false;
    }

    bool ok = q.numRowsAffected() > 0;
    qInfo(appSql) << (ok ? "Deleted" : "Not found") << "id=" << id;
    return ok;
}

bool SQLiteStorage::deleteAll() {
    qInfo(appSql) << "Delete ALL tasks/tags";
    if (!m_db.transaction()) {
        qWarning(appSql) << "tx begin:" << m_db.lastError().text();
    }

    QSqlQuery q(m_db);

    if (!q.exec("DELETE FROM task_tags")) {
        qWarning(appSql) << "clear task_tags:" << q.lastError().text();
        m_db.rollback();
        return false;
    }

    if (!q.exec("DELETE FROM tasks")) {
        qWarning(appSql) << "clear tasks:" << q.lastError().text();
        m_db.rollback();
        return false;
    }

    if (!q.exec("DELETE FROM tags")) {
        qWarning(appSql) << "clear tags:" << q.lastError().text();
        m_db.rollback();
        return false;
    }

    if (!m_db.commit()) {
        qCritical(appSql) << "tx commit:" << m_db.lastError().text();
        return false;
    }

    qInfo(appSql) << "All cleared";
    return true;
}
