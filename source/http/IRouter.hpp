#ifndef TASKLIT_HTTP_IROUTER_HPP
#define TASKLIT_HTTP_IROUTER_HPP

#include <QtHttpServer/QHttpServer>

class IRouter {
public:
    virtual ~IRouter() = default;

    virtual void registerRoutes(QHttpServer &server) = 0;
};

#endif // TASKLIT_HTTP_IROUTER_HPP
