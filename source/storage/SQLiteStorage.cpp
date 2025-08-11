#include "SQLiteStorage.hpp"

#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlRecord>
#include <QVariant>
#include <QJsonArray>
#include <QDebug>

namespace {

bool ensureSchema(QSqlDatabase db) {
    QSqlQuery q(db);

    // tasks
    if (!q.exec(
            "CREATE TABLE IF NOT EXISTS tasks ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  title TEXT NOT NULL,"
            "  description TEXT NOT NULL,"
            "  completed INTEGER NOT NULL DEFAULT 0"
            ");"
            )) {
        qWarning() << "schema tasks:" << q.lastError();
        return false;
    }

    // tags
    if (!q.exec(
            "CREATE TABLE IF NOT EXISTS tags ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  name TEXT NOT NULL UNIQUE"
            ");"
            )) {
        qWarning() << "schema tags:" << q.lastError();
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
        qWarning() << "schema task_tags:" << q.lastError();
        return false;
    }

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
        qWarning() << "fetchTagsForTask:" << q.lastError();
        return out;
    }

    while (q.next()) {
        Tag t;
        t.id = q.value(0).toLongLong();
        t.name = q.value(1).toString();
        out.append(t);
    }

    return out;
}

static QList<qint64> upsertTagsByName(QSqlDatabase db, const QList<Tag>& tags) {
    QList<qint64> ids;
    QSqlQuery sel(db), ins(db);

    sel.prepare("SELECT id FROM tags WHERE name = ?");
    ins.prepare("INSERT INTO tags(name) VALUES(?)");

    for (const auto& t : tags) {
        // по имени
        sel.bindValue(0, t.name);

        if (!sel.exec()) {
            qWarning() << "tag select:" << sel.lastError();
            continue;
        }

        if (sel.next()) {
            ids.append(sel.value(0).toLongLong());
            continue;
        }

        ins.bindValue(0, t.name);

        if (!ins.exec()) {
            qWarning() << "tag insert:" << ins.lastError();
            sel.bindValue(0, t.name);

            if (sel.exec() && sel.next()) {
                ids.append(sel.value(0).toLongLong());
            }

            continue;
        }

        ids.append(ins.lastInsertId().toLongLong());
    }

    return ids;
}

static bool replaceTaskTags(QSqlDatabase db, qint64 taskId, const QList<qint64>& tagIds) {
    QSqlQuery del(db), add(db);

    if (!del.exec(QStringLiteral("DELETE FROM task_tags WHERE task_id = %1").arg(taskId))) {
        qWarning() << "clear task_tags:" << del.lastError();
        return false;
    }

    add.prepare("INSERT OR IGNORE INTO task_tags(task_id, tag_id) VALUES(?, ?)");

    for (auto id : tagIds) {
        add.bindValue(0, taskId);
        add.bindValue(1, id);
        if (!add.exec()) {
            qWarning() << "add tag link:" << add.lastError();

            return false;
        }
    }

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
        qWarning() << "Failed to open database:" << m_db.lastError().text();
        return;
    }

    if (!ensureSchema(m_db)) {
        qWarning() << "Failed to init schema";
    }

    QSqlQuery pragma(m_db);
    pragma.exec("PRAGMA foreign_keys = ON;");
}

std::vector<Task> SQLiteStorage::getAllTasks() const {
    std::vector<Task> out;
    QSqlQuery q(m_db);
    if (!q.exec("SELECT id, title, description, completed FROM tasks ORDER BY id ASC")) {
        qWarning() << "getAllTasks:" << q.lastError();
        return out;
    }

    while (q.next()) {
        Task t = rowToTask(q.record());
        t.tags = fetchTagsForTask(m_db, t.id);
        out.push_back(std::move(t));
    }

    return out;
}

std::optional<Task> SQLiteStorage::getTaskById(qint64 id) const {
    QSqlQuery q(m_db);
    q.prepare("SELECT id, title, description, completed FROM tasks WHERE id = ?");
    q.addBindValue(id);

    if (!q.exec()) {
        qWarning() << "getTaskById:" << q.lastError();
        return std::nullopt;
    }

    if (!q.next()) {
        return std::nullopt;
    }

    Task t = rowToTask(q.record());
    t.tags = fetchTagsForTask(m_db, t.id);

    return t;
}

qint64 SQLiteStorage::addTask(const Task &task) {
    QSqlQuery q(m_db);

    if (!m_db.transaction()) {
        qWarning() << "tx begin:" << m_db.lastError();
    }

    q.prepare("INSERT INTO tasks(title, description, completed) VALUES(?, ?, ?)");
    q.addBindValue(task.title);
    q.addBindValue(task.description);
    q.addBindValue(task.completed ? 1 : 0);

    if (!q.exec()) {
        qWarning() << "addTask:" << q.lastError();
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
        qWarning() << "tx commit:" << m_db.lastError();
        return -1;
    }

    return taskId;
}

bool SQLiteStorage::updateTask(qint64 id, const Task &task) {
    if (!m_db.transaction()) {
        qWarning() << "tx begin:" << m_db.lastError();
    }

    QSqlQuery q(m_db);
    q.prepare("UPDATE tasks SET title = ?, description = ?, completed = ? WHERE id = ?");
    q.addBindValue(task.title);
    q.addBindValue(task.description);
    q.addBindValue(task.completed ? 1 : 0);
    q.addBindValue(id);

    if (!q.exec()) {
        qWarning() << "updateTask:" << q.lastError();
        m_db.rollback();
        return false;
    }

    if (q.numRowsAffected() == 0) {
        m_db.rollback();
        return false;
    }

    auto tagIds = upsertTagsByName(m_db, task.tags);
    if (!replaceTaskTags(m_db, id, tagIds)) {
        m_db.rollback();
        return false;
    }

    if (!m_db.commit()) {
        qWarning() << "tx commit:" << m_db.lastError();
        return false;
    }

    return true;
}

bool SQLiteStorage::deleteTask(qint64 id) {
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM tasks WHERE id = ?");
    q.addBindValue(id);

    if (!q.exec()) {
        qWarning() << "deleteTask:" << q.lastError();
        return false;
    }

    return q.numRowsAffected() > 0;
}

bool SQLiteStorage::deleteAll() {

    if (!m_db.transaction()) {
        qWarning() << "tx begin:" << m_db.lastError();
    }

    QSqlQuery q(m_db);

    if (!q.exec("DELETE FROM task_tags")) {
        qWarning() << "clear task_tags:" << q.lastError();
        m_db.rollback();

        return false;
    }

    if (!q.exec("DELETE FROM tasks")) {
        qWarning() << "clear tasks:" << q.lastError();
        m_db.rollback();

        return false;
    }

    if (!q.exec("DELETE FROM tags")) {
        qWarning() << "clear tags:" << q.lastError();
        m_db.rollback();
        return false;
    }

    if (!m_db.commit()) {
        qWarning() << "tx commit:" << m_db.lastError();
        return false;
    }

    return true;
}
