#ifndef TASKLIT_SERVICE_TASKSERVICEIMPL_HPP
#define TASKLIT_SERVICE_TASKSERVICEIMPL_HPP

#include "ITaskService.hpp"
#include "IStorage.hpp"
#include <memory>

class TaskServiceImpl : public ITaskService {
public:
    explicit TaskServiceImpl(std::shared_ptr<IStorage> storage);

    std::vector<Task> getAllTasks() const override;
    std::optional<Task> getTaskById(qint64 id) const override;
    qint64 addTask(const Task &task) override;
    bool updateTask(qint64 id, const Task &task) override;
    bool deleteTask(qint64 id) override;
    bool deleteAll() override;

private:
    std::shared_ptr<IStorage> m_storage;
};

#endif // TASKLIT_SERVICE_TASKSERVICEIMPL_HPP
