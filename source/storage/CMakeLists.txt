add_library(storage
    IStorage.hpp
    SQLiteStorage.hpp
    SQLiteStorage.cpp
)

target_link_libraries(storage
    PUBLIC
        Qt6::Core
        Qt6::Sql
        model
        utils
)

target_include_directories(storage
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)
