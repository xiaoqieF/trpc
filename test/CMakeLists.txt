add_executable(server server.cpp)
target_include_directories(server PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(server pthread)

add_executable(client client.cpp)
target_include_directories(client PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(client pthread)