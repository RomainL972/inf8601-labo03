add_executable(test-grid
    ../source/grid.c
    grid.c
)
target_include_directories(test-grid PRIVATE ../include)
add_test(NAME test-grid COMMAND test-grid WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(test-cart
    ../source/grid.c
    ../source/cart.c
    cart.c
)
target_include_directories(test-cart PRIVATE ../include)
add_test(NAME test-cart COMMAND test-cart WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
