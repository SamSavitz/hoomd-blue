# $Id$
# $URL$
# Maintainer: joaander

#################################
## Optional use of zlib to compress binary output files (defaults to off on windows)
if (WIN32)
option(ENABLE_ZLIB "When set to ON, a gzip compression option for binary output files is available" OFF)
else (WIN32)
option(ENABLE_ZLIB "When set to ON, a gzip compression option for binary output files is available" ON)
endif (WIN32)
if (ENABLE_ZLIB)
    add_definitions(-DENABLE_ZLIB)
endif(ENABLE_ZLIB)

#################################
## Optional static build
## ENABLE_STATIC is an option to control whether HOOMD is built as a statically linked exe or as a python module.
if (WIN32)
OPTION(ENABLE_STATIC "Link as many libraries as possible statically, cannot be changed after the first run of CMake" ON)
else (WIN32)
OPTION(ENABLE_STATIC "Link as many libraries as possible statically, cannot be changed after the first run of CMake" OFF)
endif (WIN32)

mark_as_advanced(ENABLE_STATIC)
if (ENABLE_STATIC)
    add_definitions(-DENABLE_STATIC)
endif(ENABLE_STATIC)

#################################
## Optional build with double size exclusion list
## LARGE_EXCLUSION_LIST is an option to control whether HOOMD will have extra
## exclusion lists to handle more complex molecules. Has some speed impact and
## thus is not on by default.
OPTION(LARGE_EXCLUSION_LIST "Increase the number of allowed exclusions from 4 to 16 per atom. Only needed for branched molecules with angles and dihedrals." OFF)
if (LARGE_EXCLUSION_LIST)
    add_definitions(-DLARGE_EXCLUSION_LIST)
endif(LARGE_EXCLUSION_LIST)

#################################
## Optional single/double precision build
option(SINGLE_PRECISION "Use single precision math" ON)
if (SINGLE_PRECISION)
    add_definitions (-DSINGLE_PRECISION)
endif (SINGLE_PRECISION)

#####################3
## CUDA related options
if (NOT SINGLE_PRECISION)
    set(ENABLE_CUDA OFF CACHE BOOL "Enable the compilation of the CUDA GPU code" FORCE)
endif (NOT SINGLE_PRECISION)

find_package(CUDA QUIET)
if (CUDA_FOUND)
option(ENABLE_CUDA "Enable the compilation of the CUDA GPU code" on)
else (CUDA_FOUND)
option(ENABLE_CUDA "Enable the compilation of the CUDA GPU code" off)
endif (CUDA_FOUND)

# disable CUDA if the intel compiler is detected
if (CMAKE_CXX_COMPILER MATCHES "icpc")
    set(ENABLE_CUDA OFF CACHE BOOL "Forced OFF by the use of the intel c++ compiler" FORCE)
endif (CMAKE_CXX_COMPILER MATCHES "icpc")

if (ENABLE_CUDA)
    add_definitions (-DENABLE_CUDA)

    # CUDA ARCH settings
    set(CUDA_ARCH 11 CACHE STRING "Target architecture to compile CUDA code for. Valid options are 10, 11, 12, or 13 (currently). They correspond to compute 1.0, 1.1, 1.2, and 1.3 GPU hardware")
    # the arch is going to be passed on a command line: verify it so the user doesn't make any blunders
    set(_cuda_arch_ok FALSE)
    foreach(_valid_cuda_arch 10 11 12 13)
        if (CUDA_ARCH EQUAL ${_valid_cuda_arch})
            set(_cuda_arch_ok TRUE)
        endif (CUDA_ARCH EQUAL ${_valid_cuda_arch})
            endforeach(_valid_cuda_arch)
        if (NOT _cuda_arch_ok)
            message(FATAL_ERROR "Wrong CUDA_ARCH specified. Must be one of 10, 11, 12, or 13")
    endif (NOT _cuda_arch_ok)

    add_definitions(-DCUDA_ARCH=${CUDA_ARCH})
    list(APPEND CUDA_NVCC_FLAGS -arch "sm_${CUDA_ARCH}")
    
    # ULF bug workaround disable option
    option(DISABLE_ULF_WORKAROUND "Set to ON to enable higher performace at the cost of stability on pre C1060 GPUs" off)
    mark_as_advanced(DISABLE_ULF_WORKAROUND)
    if (DISABLE_ULF_WORKAROUND)
        add_definitions (-DDISABLE_ULF_WORKAROUND)
    endif (DISABLE_ULF_WORKAROUND)
endif (ENABLE_CUDA)

#################################
## Optionally enable documentation build
find_package(Doxygen)
if (DOXYGEN_FOUND)
    # get the doxygen version
    exec_program(${DOXYGEN_EXECUTABLE} ${HOOMD_SOURCE_DIR} ARGS --version OUTPUT_VARIABLE DOXYGEN_VERSION)

    if (${DOXYGEN_VERSION} VERSION_GREATER 1.5.5)
        OPTION(ENABLE_DOXYGEN "Enables building of documentation with doxygen" ON)
    else (${DOXYGEN_VERSION} VERSION_GREATER 1.5.5)
        message(STATUS "Doxygen version less than 1.5.5, defaulting ENABLE_DOXYGEN=OFF")
        OPTION(ENABLE_DOXYGEN "Enables building of documentation with doxygen" OFF)
    endif (${DOXYGEN_VERSION} VERSION_GREATER 1.5.5)
endif (DOXYGEN_FOUND)

################################
## thread safe compiling
if(WIN32)
    add_definitions(-D_MT)
elseif(UNIX)
    add_definitions(-D_REENTRANT)
endif(WIN32)

################################
## detect and optionally enable OpenMP
find_package(OpenMP)
if (OPENMP_FOUND)
    option(ENABLE_OPENMP "Enable openmp compliation to accelerate CPU code on multi-core machines" ON)
    if (ENABLE_OPENMP)
        add_definitions (-DENABLE_OPENMP)
        # the compiler options to enable openmp are set in HOOMDCFlagsSetup
    endif (ENABLE_OPENMP)
endif (OPENMP_FOUND)

