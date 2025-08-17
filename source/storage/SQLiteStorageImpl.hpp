#ifndef TASKLIT_STORAGE_SQLITESTORAGE_HPP
#define TASKLIT_STORAGE_SQLITESTORAGE_HPP

#include "IStorage.hpp"
#include <QtSql/QSqlDatabase>

class SQLiteStorage : public IStorage {
public:
    explicit SQLiteStorage(const QString &dbPath);

    std::vector<Task> getAllTasks() const override;
    std::optional<Task> getTaskById(const QUuid& id) const override;

    QUuid addTask(const Task &task) override;
    bool updateTask(const QUuid &id, const Task &task) override;
    bool deleteTask(const QUuid &id) override;
    bool deleteAll() override;

    std::vector<Tag> getAllTags() const override;
    QUuid addTag(const Tag& tag) override;

private:
    QSqlDatabase m_db;
};

#endif // TASKLIT_STORAGE_SQLITESTORAGE_HPP
