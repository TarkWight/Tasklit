#ifndef TASKLIT_HTTP_TASKROUTER_HPP
#define TASKLIT_HTTP_TASKROUTER_HPP

#include "IRouter.hpp"
#include "ITaskService.hpp"
#include <memory>

class TaskRouter : public IRouter {
public:
    explicit TaskRouter(std::shared_ptr<ITaskService> service);

    void registerRoutes(QHttpServer &server) override;

private:
    std::shared_ptr<ITaskService> m_service;
};

#endif // TASKLIT_HTTP_TASKROUTER_HPP
