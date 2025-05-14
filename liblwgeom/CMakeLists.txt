# project setting
if("${CMAKE_C_COMPILER_ID}" STREQUAL "MSVC")
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
	add_definitions(-DYY_NO_UNISTD_H)
	add_definitions(-DWIN32)
	add_definitions(-D_USE_MATH_DEFINES)
else()
	add_definitions(-std=gnu99)
endif()

# include path
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${POSTGIS_HOME}/deps)
include_directories(${GEOS_INCLUDE_DIR})
include_directories(${PROJ_INCLUDE_DIR})
include_directories(${JSON_C_INCLUDE_DIR})
include_directories(${JSON_C_INCLUDE_DIR}/json-c)
include_directories(${PROTOBUF_C_INCLUDE_DIR})
include_directories(${SFCGAL_INCLUDE_DIR})

# needs lib path
link_directories(${GEOS_C_LIBRARY_DIR})
link_directories(${PROJ_LIBRARY_DIR})
link_directories(${JSON_C_LIBRARY_DIR})
link_directories(${PROTOBUF_C_LIBRARY_DIR})
link_directories(${SFCGAL_LIBRARY_DIR})

# needs lib name
get_filename_component(geos_c_lib     ${GEOS_C_LIBRARY}     NAME)
get_filename_component(proj_lib       ${PROJ_LIBRARY}       NAME)
get_filename_component(json_c_lib     ${JSON_C_LIBRARY}     NAME)
get_filename_component(protobuf_c_lib ${PROTOBUF_C_LIBRARY} NAME)
get_filename_component(SFCGAL_lib     ${SFCGAL_LIBRARY}     NAME)

# target
file(GLOB_RECURSE src_list "${CMAKE_CURRENT_SOURCE_DIR}/*.c")
# shared target
add_library(liblwgeom SHARED ${src_list} $<TARGET_OBJECTS:wagyu>)
	# target_link_libraries(liblwgeom wagyu)
	target_link_libraries(liblwgeom ryu)
	target_link_libraries(liblwgeom ${json_c_lib})
	target_link_libraries(liblwgeom ${proj_lib})
	target_link_libraries(liblwgeom ${geos_c_lib})
	target_link_libraries(liblwgeom ${SFCGAL_lib})
	target_link_libraries(liblwgeom ${protobuf_c_lib})
	if("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
		set_target_properties(liblwgeom PROPERTIES OUTPUT_NAME lwgeom)
	elseif("${CMAKE_C_COMPILER_ID}" STREQUAL "MSVC")
		set_target_properties(liblwgeom PROPERTIES LINK_FLAGS "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/liblwgeom.def")
	endif()

# static target
add_library(liblwgeom_static STATIC ${src_list} $<TARGET_OBJECTS:wagyu>)
	# target_link_libraries(liblwgeom_static wagyu)
	target_link_libraries(liblwgeom_static ryu)
	target_link_libraries(liblwgeom_static ${json_c_lib})
	target_link_libraries(liblwgeom_static ${proj_lib})
	target_link_libraries(liblwgeom_static ${geos_c_lib})
	target_link_libraries(liblwgeom_static ${SFCGAL_lib})
	set_target_properties(liblwgeom_static PROPERTIES OUTPUT_NAME lwgeom_static)

# install path
install(TARGETS liblwgeom LIBRARY DESTINATION ./lib)
