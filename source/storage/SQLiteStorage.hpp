#ifndef TASKLIT_STORAGE_SQLITESTORAGE_HPP
#define TASKLIT_STORAGE_SQLITESTORAGE_HPP

#include "IStorage.hpp"
#include <QtSql/QSqlDatabase>

class SQLiteStorage : public IStorage {
public:
    explicit SQLiteStorage(const QString &dbPath);

    std::vector<Task> getAllTasks() const override;
    std::optional<Task> getTaskById(qint64 id) const override;
    qint64 addTask(const Task &task) override;
    bool updateTask(qint64 id, const Task &task) override;
    bool deleteTask(qint64 id) override;
    bool deleteAll() override;

    std::vector<Tag> getAllTags() const override;
    qint64 addTag(const Tag& tag) override;

private:
    QSqlDatabase m_db;
};

#endif // TASKLIT_STORAGE_SQLITESTORAGE_HPP
