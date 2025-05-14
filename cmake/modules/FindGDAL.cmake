# Find GDAL
# ~~~~~~~~~~~~
# Copyright (c) 2024, dameng <yangzhenglong at dameng.com>
# Copyright (c) 2007, Martin Dobias <wonder.sk at gmail.com>
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#
# CMake module to search for gdal library by the given variable GDAL_DIR
#
# If it's found it sets GDAL_FOUND to TRUE
# and following variables are set:
#    GDAL_INCLUDE_DIR
#    GDAL_LIBRARY
#    GDAL_LIBRARY_DIR
#    GDAL_VERSION

# These variable were setted by find_package()
#    GDAL_FIND_REQUIRED
#    GDAL_FIND_QUIETLY


# find_path and find_library normally search standard locations
# before the specified paths. To search non-standard paths first,
# FIND_* is invoked first with specified paths and NO_DEFAULT_PATH
# and then again with no specified paths to search the default
# locations. When an earlier FIND_* succeeds, subsequent FIND_*s
# searching for the same item do nothing.

# not support APPLE currently

if(NOT GDAL_DIR)
  message(FATAL_ERROR "needs set GDAL_DIR first")
endif()

find_path(GDAL_INCLUDE_DIR NAMES gdal.h PATHS
  "${GDAL_DIR}/include"
  NO_DEFAULT_PATH
)

find_library(GDAL_LIBRARY NAMES gdal gdald PATHS
  "${GDAL_DIR}/lib"
  "${GDAL_DIR}/lib64"
  NO_DEFAULT_PATH
)

get_filename_component(GDAL_LIBRARY_DIR ${GDAL_LIBRARY} DIRECTORY)

if(GDAL_INCLUDE_DIR AND GDAL_LIBRARY)
  set(GDAL_FOUND TRUE)
endif()

# Extract version information from the header file
if(GDAL_INCLUDE_DIR)
    file(STRINGS ${GDAL_INCLUDE_DIR}/gdal_version.h _ver_line
         REGEX "^#  define GDAL_RELEASE_NAME  *\"[0-9]+\\.[0-9]+\\.[0-9]+\""
         LIMIT_COUNT 1)
    string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+"
           GDAL_VERSION "${_ver_line}")
    unset(_ver_line)
endif()

if(GDAL_FOUND)
  if(NOT GDAL_FIND_QUIETLY)
    message(STATUS "Found gdal library: ${GDAL_LIBRARY}")
    message(STATUS "Found gdal headers: ${GDAL_INCLUDE_DIR}")
    message(STATUS "gdal version: ${GDAL_VERSION}")
  endif()
else()

if(GDAL_FIND_REQUIRED) # this variable will be set when 'REQUIRED' in find_package() function
  message(FATAL_ERROR "Could not find gdal")
endif()

endif()