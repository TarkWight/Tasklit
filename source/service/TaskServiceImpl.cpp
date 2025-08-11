#include "TaskServiceImpl.hpp"

TaskServiceImpl::TaskServiceImpl(std::shared_ptr<IStorage> storage)
    : m_storage(std::move(storage)) {}

std::vector<Task> TaskServiceImpl::getAllTasks() const {
    return m_storage->getAllTasks();
}

std::optional<Task> TaskServiceImpl::getTaskById(qint64 id) const {
    return m_storage->getTaskById(id);
}

qint64 TaskServiceImpl::addTask(const Task &task) {
    return m_storage->addTask(task);
}

bool TaskServiceImpl::updateTask(qint64 id, const Task &task) {
    return m_storage->updateTask(id, task);
}

bool TaskServiceImpl::deleteTask(qint64 id) {
    return m_storage->deleteTask(id);
}

bool TaskServiceImpl::deleteAll() {
    return m_storage->deleteAll();
}

std::vector<Tag> TaskServiceImpl::getAllTags() const {
    return m_storage->getAllTags();
}

qint64 TaskServiceImpl::addTag(const Tag &tag) {
    return m_storage->addTag(tag);
}
