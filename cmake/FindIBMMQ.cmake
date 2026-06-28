# FindIBMMQ.cmake
# Locate IBM MQ client libraries and headers.
#
# Sets:
#   IBMMQ_FOUND        - TRUE if IBM MQ was found
#   IBMMQ_INCLUDE_DIR  - Include directory for MQ headers
#   IBMMQ_LIBRARIES    - Libraries to link against
#
# Hints:
#   MQ_HOME            - Root of IBM MQ installation
#   MQ_INCLUDE_PATH    - Direct path to headers
#   MQ_LIB_PATH        - Direct path to libraries

# Default search paths per platform
if(WIN32)
    set(_MQ_DEFAULT_PATHS
        "C:/Program Files/IBM/MQ"
        "C:/Program Files (x86)/IBM/MQ"
        "C:/IBM/MQ"
    )
    set(_MQ_LIB_NAMES mqm mqic32)
    set(_MQ_LIB_SUFFIXES lib lib64 bin bin64 tools/lib tools/lib64)
    set(_MQ_INC_SUFFIXES inc include tools/c/include)
else()
    set(_MQ_DEFAULT_PATHS
        /opt/mqm
        /usr/local/mqm
        /usr/mqm
    )
    set(_MQ_LIB_NAMES mqm mqm_r mqic mqic_r)
    set(_MQ_LIB_SUFFIXES lib lib64)
    set(_MQ_INC_SUFFIXES inc include)
endif()

# Build search hints
set(_MQ_SEARCH_HINTS "")
if(DEFINED ENV{MQ_HOME})
    list(APPEND _MQ_SEARCH_HINTS $ENV{MQ_HOME})
endif()
if(MQ_HOME)
    list(APPEND _MQ_SEARCH_HINTS ${MQ_HOME})
endif()

# Find include directory
if(MQ_INCLUDE_PATH)
    set(IBMMQ_INCLUDE_DIR ${MQ_INCLUDE_PATH})
else()
    find_path(IBMMQ_INCLUDE_DIR
        NAMES cmqc.h
        HINTS ${_MQ_SEARCH_HINTS}
        PATHS ${_MQ_DEFAULT_PATHS}
        PATH_SUFFIXES ${_MQ_INC_SUFFIXES}
    )
endif()

# Find library
if(MQ_LIB_PATH)
    find_library(IBMMQ_LIBRARY
        NAMES ${_MQ_LIB_NAMES}
        PATHS ${MQ_LIB_PATH}
        NO_DEFAULT_PATH
    )
else()
    find_library(IBMMQ_LIBRARY
        NAMES ${_MQ_LIB_NAMES}
        HINTS ${_MQ_SEARCH_HINTS}
        PATHS ${_MQ_DEFAULT_PATHS}
        PATH_SUFFIXES ${_MQ_LIB_SUFFIXES}
    )
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(IBMMQ
    REQUIRED_VARS IBMMQ_INCLUDE_DIR
    FAIL_MESSAGE "IBM MQ client not found. Set MQ_HOME or use -DIBMMQ_EXPORTER_USE_STUB_MQ=ON"
)

if(IBMMQ_FOUND)
    if(IBMMQ_LIBRARY)
        set(IBMMQ_LIBRARIES ${IBMMQ_LIBRARY})
    else()
        set(IBMMQ_LIBRARIES "")
    endif()
    message(STATUS "Found IBM MQ: ${IBMMQ_INCLUDE_DIR}")
    if(IBMMQ_LIBRARY)
        message(STATUS "  Library: ${IBMMQ_LIBRARY}")
    endif()
endif()

mark_as_advanced(IBMMQ_INCLUDE_DIR IBMMQ_LIBRARY)
