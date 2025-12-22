add_library(common STATIC
    protocol.c
    shm_wrapper.c
    mq_wrapper.c
    bank_logic.c
)

target_include_directories(common PUBLIC 
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(common PRIVATE pthread rt)