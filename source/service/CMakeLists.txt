add_library(service
    ITaskService.hpp
    TaskServiceImpl.hpp
    TaskServiceImpl.cpp
)

target_link_libraries(service
    PUBLIC
        Qt6::Core
        model
        storage
)

target_include_directories(service
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)
