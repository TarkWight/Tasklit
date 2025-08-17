#ifndef TASKLIT_SERVICE_TASKSERVICEIMPL_HPP
#define TASKLIT_SERVICE_TASKSERVICEIMPL_HPP

#include <QUuid>
#include <memory>
#include <optional>
#include <vector>

#include "IStorage.hpp"
#include "ITaskService.hpp"

class TaskServiceImpl : public ITaskService {
public:
    explicit TaskServiceImpl(std::shared_ptr<IStorage> storage);

    std::vector<Task> getAllTasks() const override;
    std::optional<Task> getTaskById(const QUuid &taskId) const override;

    QUuid addTask(const Task &task) override;
    bool updateTask(const QUuid &taskId, const Task &task) override;
    bool deleteTask(const QUuid &taskId) override;
    bool deleteAll() override;

    std::vector<Tag> getAllTags() const override;
    QUuid addTag(const Tag &tag) override;

private:
    std::shared_ptr<IStorage> m_storage;
};

#endif // TASKLIT_SERVICE_TASKSERVICEIMPL_HPP
