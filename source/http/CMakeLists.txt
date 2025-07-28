add_library(http
    IRouter.hpp
    TaskRouter.hpp
    TaskRouter.cpp
)

target_link_libraries(http
    PUBLIC
        Qt6::Core
        Qt6::HttpServer
        model
        service
)

target_include_directories(http
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)
