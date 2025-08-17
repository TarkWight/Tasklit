#include "SQLiteStorage.hpp"

#include <QJsonArray>
#include <QVariant>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlRecord>

#include "Logger.hpp"

// ─────────────────────────────────────────────────────────────────────────────
// helpers
// ─────────────────────────────────────────────────────────────────────────────
namespace {

QString uuidToStr(const QUuid &id) { return id.toString(QUuid::WithoutBraces); }

QUuid strToUuid(const QString &s) { return QUuid::fromString(s); }

bool ensureSchema(QSqlDatabase db) {
    QSqlQuery query(db);

    qInfo(appSql) << "Ensuring DB schema...";

    // tasks: UUID как TEXT PK
    if (!query.exec("CREATE TABLE IF NOT EXISTS tasks ("
                    "  id TEXT PRIMARY KEY,"
                    "  title TEXT NOT NULL,"
                    "  description TEXT NOT NULL,"
                    "  isCompleted INTEGER NOT NULL DEFAULT 0"
                    ");")) {
        qCritical(appSql) << "schema tasks:" << query.lastError().text();
        return false;
    }

    // tags: UUID как TEXT PK, name уникально
    if (!query.exec("CREATE TABLE IF NOT EXISTS tags ("
                    "  id TEXT PRIMARY KEY,"
                    "  name TEXT NOT NULL UNIQUE"
                    ");")) {
        qCritical(appSql) << "schema tags:" << query.lastError().text();
        return false;
    }

    // связывающая таблица: UUID TEXT
    if (!query.exec(
            "CREATE TABLE IF NOT EXISTS task_tags ("
            "  task_id TEXT NOT NULL,"
            "  tag_id  TEXT NOT NULL,"
            "  PRIMARY KEY (task_id, tag_id),"
            "  FOREIGN KEY(task_id) REFERENCES tasks(id) ON DELETE CASCADE,"
            "  FOREIGN KEY(tag_id)  REFERENCES tags(id)  ON DELETE CASCADE"
            ");")) {
        qCritical(appSql) << "schema task_tags:" << query.lastError().text();
        return false;
    }

    qInfo(appSql) << "Schema OK";
    return true;
}

static QVector<QUuid> fetchTagIdsForTask(QSqlDatabase db, const QUuid &taskId) {
    QVector<QUuid> out;
    QSqlQuery query(db);

    query.prepare("SELECT tt.tag_id "
                  "FROM task_tags tt "
                  "WHERE tt.task_id = ?");
    query.addBindValue(uuidToStr(taskId));

    if (!query.exec()) {
        qWarning(appSql) << "fetchTagIdsForTask:" << query.lastError().text();
        return out;
    }

    while (query.next()) {
        const QUuid id = strToUuid(query.value(0).toString());
        if (!id.isNull()) {
            out.append(id);
        }
    }

    qInfo(appSql) << "Fetched" << out.size() << "tag ids for task"
                  << uuidToStr(taskId);
    return out;
}

static bool allTagsExist(QSqlDatabase db, const QVector<QUuid> &tagIds) {
    if (tagIds.isEmpty()) {
        return true;
    }

    QSqlQuery query(db);
    query.prepare("SELECT COUNT(*) FROM tags WHERE id = ?");
    for (const QUuid &id : tagIds) {
        query.addBindValue(uuidToStr(id));

        if (!query.exec()) {
            qWarning(appSql) << "allTagsExist:" << query.lastError().text();
            return false;
        }

        if (!query.next()) {
            return false;
        }

        const int cnt = query.value(0).toInt();
        if (cnt == 0) {
            qWarning(appSql) << "Tag not found id=" << uuidToStr(id);
            return false;
        }

        query.finish();
        query.clear();
    }

    return true;
}

static bool replaceTaskTags(QSqlDatabase db, const QUuid &taskId,
                            const QVector<QUuid> &tagIds) {
    QSqlQuery del(db), add(db);

    del.prepare("DELETE FROM task_tags WHERE task_id = ?");
    del.addBindValue(uuidToStr(taskId));

    if (!del.exec()) {
        qWarning(appSql) << "clear task_tags:" << del.lastError().text();
        return false;
    }

    if (tagIds.isEmpty()) {
        qInfo(appSql) << "Cleared tag links for task" << uuidToStr(taskId);
        return true;
    }

    add.prepare(
        "INSERT OR IGNORE INTO task_tags(task_id, tag_id) VALUES(?, ?)");
    for (const QUuid &tagId : tagIds) {
        add.addBindValue(uuidToStr(taskId));
        add.addBindValue(uuidToStr(tagId));

        if (!add.exec()) {
            qWarning(appSql) << "add tag link:" << add.lastError().text();
            return false;
        }

        add.finish();
        add.clear();
    }

    qInfo(appSql) << "Updated" << tagIds.size() << "tag links for task"
                  << uuidToStr(taskId);

    return true;
}

static Task rowToTask(const QSqlRecord &record) {
    Task task;
    task.id = strToUuid(record.value("id").toString());
    task.title = record.value("title").toString();
    task.description = record.value("description").toString();
    task.isCompleted = record.value("isCompleted").toInt() != 0;

    return task;
}

} // END NAMESPACE

// ─────────────────────────────────────────────────────────────────────────────
// ctor
// ─────────────────────────────────────────────────────────────────────────────
SQLiteStorage::SQLiteStorage(const QString &dbPath) {
    m_db = QSqlDatabase::addDatabase("QSQLITE");
    m_db.setDatabaseName(dbPath);

    if (!m_db.open()) {
        qCritical(appSql) << "Failed to open database:"
                          << m_db.lastError().text();
        return;
    }

    if (!ensureSchema(m_db)) {
        qCritical(appSql) << "Failed to init schema";
    }

    QSqlQuery pragma(m_db);
    pragma.exec("PRAGMA foreign_keys = ON;");
    qInfo(appSql) << "SQLiteStorage ready, path:" << dbPath;
}

// ─────────────────────────────────────────────────────────────────────────────
// tasks
// ─────────────────────────────────────────────────────────────────────────────
std::vector<Task> SQLiteStorage::getAllTasks() const {
    qInfo(appSql) << "Query: getAllTasks()";
    std::vector<Task> out;

    QSqlQuery query(m_db);
    if (!query.exec(
            "SELECT id, title, description, isCompleted FROM tasks ORDER "
            "BY rowid ASC")) {
        qWarning(appSql) << "getAllTasks:" << query.lastError().text();
        return out;
    }

    while (query.next()) {
        Task task = rowToTask(query.record());
        task.tags = fetchTagIdsForTask(m_db, task.id);
        task.tagsExpanded.reset();
        out.push_back(std::move(task));
    }

    qInfo(appSql) << "→" << out.size() << "tasks fetched";
    return out;
}

std::optional<Task> SQLiteStorage::getTaskById(const QUuid &id) const {
    qInfo(appSql) << "Query: getTaskById id=" << uuidToStr(id);

    QSqlQuery query(m_db);
    query.prepare(
        "SELECT id, title, description, isCompleted FROM tasks WHERE id = ?");
    query.addBindValue(uuidToStr(id));

    if (!query.exec()) {
        qWarning(appSql) << "getTaskById:" << query.lastError().text();
        return std::nullopt;
    }

    if (!query.next()) {
        qInfo(appSql) << "Task not found id=" << uuidToStr(id);
        return std::nullopt;
    }

    Task task = rowToTask(query.record());
    task.tags = fetchTagIdsForTask(m_db, task.id);
    task.tagsExpanded.reset();

    return task;
}

QUuid SQLiteStorage::addTask(const Task &task) {
    const QUuid newId = task.id.isNull() ? QUuid::createUuid() : task.id;
    qInfo(appSql) << "Insert task id=" << uuidToStr(newId)
                  << "title=" << task.title << "tags=" << task.tags.size();

    if (!m_db.transaction()) {
        qWarning(appSql) << "tx begin:" << m_db.lastError().text();
    }

    if (!allTagsExist(m_db, task.tags)) {
        qWarning(appSql) << "Insert aborted: some tag ids do not exist";
        m_db.rollback();
        return QUuid{};
    }

    QSqlQuery query(m_db);
    query.prepare("INSERT INTO tasks(id, title, description, isCompleted) "
                  "VALUES(?, ?, ?, ?)");
    query.addBindValue(uuidToStr(newId));
    query.addBindValue(task.title);
    query.addBindValue(task.description);
    query.addBindValue(task.isCompleted ? 1 : 0);

    if (!query.exec()) {
        qCritical(appSql) << "addTask:" << query.lastError().text();
        m_db.rollback();
        return QUuid{};
    }

    if (!replaceTaskTags(m_db, newId, task.tags)) {
        m_db.rollback();
        return QUuid{};
    }

    if (!m_db.commit()) {
        qCritical(appSql) << "tx commit:" << m_db.lastError().text();
        return QUuid{};
    }

    qInfo(appSql) << "Task inserted id=" << uuidToStr(newId);
    return newId;
}

bool SQLiteStorage::updateTask(const QUuid &id, const Task &task) {
    qInfo(appSql) << "Update task id=" << uuidToStr(id);

    if (!m_db.transaction()) {
        qWarning(appSql) << "tx begin:" << m_db.lastError().text();
    }

    if (!allTagsExist(m_db, task.tags)) {
        qWarning(appSql) << "Update aborted: some tag ids do not exist";
        m_db.rollback();
        return false;
    }

    QSqlQuery query(m_db);
    query.prepare(
        "UPDATE tasks SET title = ?, description = ?, isCompleted = ? "
        "WHERE id = ?");
    query.addBindValue(task.title);
    query.addBindValue(task.description);
    query.addBindValue(task.isCompleted ? 1 : 0);
    query.addBindValue(uuidToStr(id));

    if (!query.exec()) {
        qCritical(appSql) << "updateTask:" << query.lastError().text();
        m_db.rollback();
        return false;
    }

    if (query.numRowsAffected() == 0) {
        qInfo(appSql) << "No rows updated for id=" << uuidToStr(id);
        m_db.rollback();
        return false;
    }

    if (!replaceTaskTags(m_db, id, task.tags)) {
        m_db.rollback();
        return false;
    }

    if (!m_db.commit()) {
        qCritical(appSql) << "tx commit:" << m_db.lastError().text();
        return false;
    }

    qInfo(appSql) << "Task updated id=" << uuidToStr(id);
    return true;
}

bool SQLiteStorage::deleteTask(const QUuid &id) {
    qInfo(appSql) << "Delete task id=" << uuidToStr(id);

    QSqlQuery query(m_db);
    query.prepare("DELETE FROM tasks WHERE id = ?");
    query.addBindValue(uuidToStr(id));

    if (!query.exec()) {
        qWarning(appSql) << "deleteTask:" << query.lastError().text();
        return false;
    }

    const bool ok = query.numRowsAffected() > 0;
    qInfo(appSql) << (ok ? "Deleted" : "Not found") << "id=" << uuidToStr(id);
    return ok;
}

bool SQLiteStorage::deleteAll() {
    qInfo(appSql) << "Delete ALL tasks/tags";
    if (!m_db.transaction()) {
        qWarning(appSql) << "tx begin:" << m_db.lastError().text();
    }

    QSqlQuery query(m_db);

    if (!query.exec("DELETE FROM task_tags")) {
        qWarning(appSql) << "clear task_tags:" << query.lastError().text();
        m_db.rollback();
        return false;
    }

    if (!query.exec("DELETE FROM tasks")) {
        qWarning(appSql) << "clear tasks:" << query.lastError().text();
        m_db.rollback();
        return false;
    }

    if (!query.exec("DELETE FROM tags")) {
        qWarning(appSql) << "clear tags:" << query.lastError().text();
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

// ─────────────────────────────────────────────────────────────────────────────
// tags
// ─────────────────────────────────────────────────────────────────────────────
std::vector<Tag> SQLiteStorage::getAllTags() const {
    std::vector<Tag> out;
    QSqlQuery query(m_db);

    if (!query.exec("SELECT id, name FROM tags ORDER BY name ASC")) {
        qWarning(appSql) << "getAllTags:" << query.lastError().text();
        return out;
    }

    while (query.next()) {
        Tag tag;
        tag.id = strToUuid(query.value(0).toString());
        tag.name = query.value(1).toString();
        out.push_back(std::move(tag));
    }

    qInfo(appSql) << "→" << out.size() << "tags fetched";
    return out;
}

QUuid SQLiteStorage::addTag(const Tag &tag) {
    const QUuid newId = tag.id.isNull() ? QUuid::createUuid() : tag.id;
    qInfo(appSql) << "Insert tag id=" << uuidToStr(newId)
                  << "name=" << tag.name;

    QSqlQuery ins(m_db), sel(m_db);
    ins.prepare("INSERT INTO tags(id, name) VALUES(?, ?)");
    ins.addBindValue(uuidToStr(newId));
    ins.addBindValue(tag.name);

    if (!ins.exec()) {
        qWarning(appSql) << "addTag insert failed:" << ins.lastError().text()
        << "trying SELECT existing by name";
        sel.prepare("SELECT id FROM tags WHERE name = ?");
        sel.addBindValue(tag.name);

        if (sel.exec() && sel.next()) {
            const QUuid existingId = strToUuid(sel.value(0).toString());
            if (!existingId.isNull()) {
                qInfo(appSql) << "Tag exists id=" << uuidToStr(existingId);
                return existingId;
            }
        }

        return QUuid{};
    }

    qInfo(appSql) << "Tag inserted id=" << uuidToStr(newId);
    return newId;
}
