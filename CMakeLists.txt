###############################################################################
# yangzhegnlong 2023/12/08 postgis CMake file, Toplevel
#
# [Notes]
# If you want to set CMAKE_C_FLAGS or CMAKE_CXX_FLAGS, please set `-fPIC` also
# 
# [Usage]
# build Release (Default):
# mkdir build && mkdir tmp_install && cd build
# cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=../tmp_install ..
# cmake --build ./ --target install --config Release
# build Debug:
# mkdir build && mkdir tmp_install && cd build
# cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=../tmp_install ..
# cmake --build ./ --target install --config Debug
###############################################################################

# set cmake version requirement
cmake_minimum_required(VERSION 2.8.12)
project(postgis C CXX)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules)

# project requirement
find_package(JSON_C REQUIRED)
find_package(GEOS REQUIRED)
find_package(PROJ REQUIRED)
find_package(GDAL REQUIRED)
find_package(PROTOBUF_C REQUIRED)
find_package(SFCGAL REQUIRED)

# global path var
set(POSTGIS_HOME ${CMAKE_CURRENT_SOURCE_DIR})

# set default build type
if("${CMAKE_BUILD_TYPE}" STREQUAL "")
	set(CMAKE_BUILD_TYPE Release)
endif()

# set default install rpath
if("${CMAKE_INSTALL_RPATH}" STREQUAL "")
	set(CMAKE_INSTALL_RPATH "\$ORIGIN/")
endif()

# set default flags
if("${CMAKE_C_COMPILER_ID}" STREQUAL "MSVC")
set(CMAKE_C_FLAGS_DEBUG   			 "${CMAKE_C_FLAGS_DEBUG}   /DEBUG")
set(CMAKE_CXX_FLAGS_DEBUG 			 "${CMAKE_CXX_FLAGS_DEBUG} /DEBUG")
set(CMAKE_C_FLAGS_RELEASE   		 "${CMAKE_C_FLAGS_RELEASE}   /O2")
set(CMAKE_CXX_FLAGS_RELEASE 		 "${CMAKE_CXX_FLAGS_RELEASE} /O2")
set(CMAKE_C_FLAGS_RELWITHDEBUGINFO   "${CMAKE_C_FLAGS_RELWITHDEBUGINFO}   /DEBUG /O2")
set(CMAKE_CXX_FLAGS_RELWITHDEBUGINFO "${CMAKE_CXX_FLAGS_RELWITHDEBUGINFO} /DEBUG /O2")
else()
add_compile_options(-fPIC)
set(CMAKE_C_FLAGS_DEBUG   			 "${CMAKE_C_FLAGS_DEBUG}   -g")
set(CMAKE_CXX_FLAGS_DEBUG 			 "${CMAKE_CXX_FLAGS_DEBUG} -g")
set(CMAKE_C_FLAGS_RELEASE   		 "${CMAKE_C_FLAGS_RELEASE}   -O2")
set(CMAKE_CXX_FLAGS_RELEASE 		 "${CMAKE_CXX_FLAGS_RELEASE} -O2")
set(CMAKE_C_FLAGS_RELWITHDEBUGINFO   "${CMAKE_C_FLAGS_RELWITHDEBUGINFO}   -g -O2")
set(CMAKE_CXX_FLAGS_RELWITHDEBUGINFO "${CMAKE_CXX_FLAGS_RELWITHDEBUGINFO} -g -O2")
endif()

# sub project
add_subdirectory(${POSTGIS_HOME}/deps/wagyu)
add_subdirectory(${POSTGIS_HOME}/deps/ryu)
add_subdirectory(${POSTGIS_HOME}/liblwgeom)
add_subdirectory(${POSTGIS_HOME}/raster/rt_core)
