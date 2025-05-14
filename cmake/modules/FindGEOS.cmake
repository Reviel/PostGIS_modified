# Find GEOS
# ~~~~~~~~~~~~
# Copyright (c) 2024, dameng <yangzhenglong at dameng.com>
# Copyright (c) 2007, Martin Dobias <wonder.sk at gmail.com>
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#
# CMake module to search for geos library by the given variable GEOS_DIR
#
# If it's found it sets GEOS_FOUND to TRUE
# and following variables are set:
#    GEOS_INCLUDE_DIR
#    GEOS_LIBRARY
#    GEOS_C_LIBRARY
#    GEOS_LIBRARY_DIR
#    GEOS_C_LIBRARY_DIR
#    GEOS_VERSION

# These variable were setted by find_package()
#    GEOS_FIND_REQUIRED
#    GEOS_FIND_QUIETLY


# find_path and find_library normally search standard locations
# before the specified paths. To search non-standard paths first,
# FIND_* is invoked first with specified paths and NO_DEFAULT_PATH
# and then again with no specified paths to search the default
# locations. When an earlier FIND_* succeeds, subsequent FIND_*s
# searching for the same item do nothing.

# not support APPLE currently

if(NOT GEOS_DIR)
  message(FATAL_ERROR "needs set GEOS_DIR first")
endif()

find_path(GEOS_INCLUDE_DIR NAMES geos_c.h PATHS
  "${GEOS_DIR}/include"
  NO_DEFAULT_PATH
)

find_library(GEOS_C_LIBRARY NAMES libgeos_c geos_c PATHS
  "${GEOS_DIR}/lib"
  "${GEOS_DIR}/lib64"
  "${GEOS_DIR}/bin" # for windows
  NO_DEFAULT_PATH
)

find_library(GEOS_LIBRARY NAMES libgeos geos PATHS
  "${GEOS_DIR}/lib"
  "${GEOS_DIR}/lib64"
  "${GEOS_DIR}/bin" # for windows
  NO_DEFAULT_PATH
)

get_filename_component(GEOS_LIBRARY_DIR ${GEOS_LIBRARY} DIRECTORY)

get_filename_component(GEOS_C_LIBRARY_DIR ${GEOS_C_LIBRARY} DIRECTORY)

if(GEOS_INCLUDE_DIR AND (GEOS_C_LIBRARY OR GEOS_LIBRARY))
  set(GEOS_FOUND TRUE)
endif()

# Extract version information from the header file
if(GEOS_INCLUDE_DIR)
    file(STRINGS ${GEOS_INCLUDE_DIR}/geos_c.h _ver_line
	 REGEX "^#define GEOS_VERSION  *\"[0-9]+\\.[0-9]+\\.[0-9]+\""
         LIMIT_COUNT 1)
    string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+"
           GEOS_VERSION "${_ver_line}")
    unset(_ver_line)
endif()

if(GEOS_FOUND)
  if(NOT GEOS_FIND_QUIETLY)
    message(STATUS "Found GEOS library: ${GEOS_LIBRARY} and ${GEOS_C_LIBRARY}")
    message(STATUS "Found GEOS headers: ${GEOS_INCLUDE_DIR}")
    message(STATUS "GEOS version: ${GEOS_VERSION}")
  endif()
else()

if(GEOS_FIND_REQUIRED) # this variable will be set when 'REQUIRED' in find_package() function
  message(FATAL_ERROR "Could not find GEOS")
endif()

endif()