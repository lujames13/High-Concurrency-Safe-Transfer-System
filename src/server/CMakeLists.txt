add_executable(server
    main.c
    # worker.c # Uncomment when implemented
)

target_link_libraries(server PRIVATE common)
