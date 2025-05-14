# project setting
add_definitions(-std=gnu++11)

if(NOT PROJ_FOUND)
	message(FATAL_ERROR "needs proj library but not found, check PROJ_DIR variable")
endif()

# include path
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${POSTGIS_HOME}/liblwgeom)
include_directories(${PROJ_INCLUDE_DIR})

# static target
set(src_list ${CMAKE_CURRENT_SOURCE_DIR}/lwgeom_wagyu.cpp)
# add_library(wagyu STATIC ${src_list}) # some function needs be export, but have not
										# used by liblwgeom. so we build wagyu as object.
add_library(wagyu OBJECT ${src_list})
