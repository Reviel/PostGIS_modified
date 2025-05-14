# project setting
if("${CMAKE_C_COMPILER_ID}" STREQUAL "MSVC")
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
	add_definitions(-DWIN32)
	add_definitions(-D_USE_MATH_DEFINES)
else()
	set(CMAKE_C_FLAGS "-std=gnu99")
endif()

# include path
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)
include_directories(${POSTGIS_HOME}/liblwgeom)
include_directories(${GEOS_INCLUDE_DIR})
include_directories(${PROJ_INCLUDE_DIR})
include_directories(${GDAL_INCLUDE_DIR})

# needs lib path
link_directories(${GEOS_C_LIBRARY_DIR})
link_directories(${PROJ_LIBRARY_DIR})
link_directories(${GDAL_LIBRARY_DIR})
link_directories(${JSON_C_LIBRARY_DIR})
link_directories(${SFCGAL_LIBRARY_DIR})

# needs lib name
get_filename_component(geos_c_lib ${GEOS_C_LIBRARY} NAME)
get_filename_component(gdal_lib   ${GDAL_LIBRARY}   NAME)

# shared target
file(GLOB_RECURSE src_list "${CMAKE_CURRENT_SOURCE_DIR}/*.c")
add_library(librtcore SHARED ${src_list})
	target_link_libraries(librtcore liblwgeom_static)
	target_link_libraries(librtcore ${geos_c_lib})
	target_link_libraries(librtcore ${gdal_lib})
	if("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
		set_target_properties(librtcore PROPERTIES OUTPUT_NAME rtcore)
	elseif("${CMAKE_C_COMPILER_ID}" STREQUAL "MSVC")
		set_target_properties(librtcore PROPERTIES LINK_FLAGS "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/librtcore.def")
	endif()

# install path
install(TARGETS librtcore LIBRARY DESTINATION ./lib)
