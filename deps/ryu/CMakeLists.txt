# project setting
add_definitions(-std=gnu99)

# include path
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)

# static target
set(src_list ${CMAKE_CURRENT_SOURCE_DIR}/d2s.c)
add_library(ryu STATIC ${src_list})
