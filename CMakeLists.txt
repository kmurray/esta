cmake_minimum_required(VERSION 2.8.12)

project("Extended_Timing_Analysis")


message(STATUS "CMAKE_CONFIGURATION_TYPES: ${CMAKE_CONFIGURATION_TYPES}")
#
#
# Determine compiler configuration
#
#

#Set the default build type if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING
        "Choose the type of build: None, Debug, Release, RelWithDebInfo, MinSizeRel"
        FORCE)
endif()
message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")

#We want assertions to stay on in release.
#Remove the DNDEBUG macro that cmake defines by default for release types.
set(NDEBUG_REGEX "[/-]D *NDEBUG")
foreach(flags_to_scrub CMAKE_CXX_FLAGS_RELEASE CMAKE_CXX_FLAGS_RELWITHDEBINFO MAKE_CXX_FLAGS_MINSIZEREL)
    string(REGEX REPLACE ${NDEBUG_REGEX} "" "${flags_to_scrub}" "${${flags_to_scrub}}")
endforeach()

#Compiler flag configuration checks
include(CheckCXXCompilerFlag)

#
# We require c++11 support
#
CHECK_CXX_COMPILER_FLAG("-std=c++14" COMPILER_SUPPORTS_CXX14_FLAG)
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11_FLAG)
CHECK_CXX_COMPILER_FLAG("-std=c++0x" COMPILER_SUPPORTS_CXX0X_FLAG)
#if(COMPILER_SUPPORTS_CXX14_FLAG)
    #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
if(COMPILER_SUPPORTS_CXX11_FLAG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
elseif(COMPILER_SUPPORTS_CXX0X_FLAG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
else()
    message(WARNING "Could not determine C++11 support for compiler ${CMAKE_CXX_COMPILER}. C++11 support is required.")
endif()

#
# Debug/Opt flags
#
#Support for optimization that don't interfere with debugging?
CHECK_CXX_COMPILER_FLAG("-Og" COMPILER_SUPPORTS_-Og)
if(COMPILER_SUPPORTS_-Og)
    #Currently disable, Og still causes some variables to be optimized away
    #set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Og")
endif()

#Support for including non-inlined versions of inlined functions (useful for interactive debugging)
CHECK_CXX_COMPILER_FLAG("-fkeep-inline-functions" COMPILER_SUPPORTS_-fkeep-inline-functions)
if(COMPILER_SUPPORTS_-fkeep-inline-functions)
    #set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fkeep-inline-functions")
    #set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -fkeep-inline-functions")
endif()

#Support for gprof
#CHECK_CXX_COMPILER_FLAG("-pg" COMPILER_SUPPORTS_-pg)
#if(COMPILER_SUPPORTS_-fkeep-inline-functions)
    ##set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fkeep-inline-functions")
    #set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -pg")
#endif()

#
# Warning flags
#
set(WARN_FLAGS "") #The actual warning flags to be applied
set(WARN_FLAGS_TO_CHECK  #The flags to check if the compiler supports
    #GCC-like
    "-Wall"                     #Most warnings, typically good
    "-Wextra"                   #Extra warning, usually good
    "-pedantic"                 #Ensure ISO compliance (i.e. no non-standard extensions)
    "-Wcast-qual"               #Warn if cast removes qualifier (e.g. const char* -> char*)
    "-Wcast-align"              #Warn if a cast causes memory alignment changes
    "-Wshadow"                  #Warn if local variable shadows another variable
    "-Wformat=2"                #Sanity checks for printf-like formatting
    "-Wlogical-op"              #Checks for logical op when bit-wise expected
    "-Wmissing-declarations"    #Warn if a global function is defined with no delcaration in a header file
    "-Wmissing-include-dirs"    #Warn if a user include directory is missing
    "-Wredundant-decls"         #Warn if there are overlapping declarations
    "-Wswitch-default"          #Warn if a switch has no default
    "-Wundef"                   #Warn if #if() preprocessor refers to an undefined directive
    "-Wunused-variable"         #Warn about variables that are not used
    "-Wno-unused-parameter"     #Don't worry about unused function parameters
    #GCC-like optional
    #"-Wsign-conversion"         #Warn if a conversion may change the sign
    #"-Wpadded"                  #Will warn if additional padding is introduced to a struct/class. Turn on if optimizing class memory layouts
    )

#Check and see if the compiler supports the various warning flags,
#and add valid flags
foreach(flag ${WARN_FLAGS_TO_CHECK})
    CHECK_CXX_COMPILER_FLAG(${flag} COMPILER_SUPPORTS_${flag})
    if(COMPILER_SUPPORTS_${flag})
        #Flag supported, so enable it
        set(WARN_FLAGS "${WARN_FLAGS} ${flag}")
    endif()
endforeach()

#Apply the warning flags to all build types
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WARN_FLAGS}")

message(STATUS "CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")

#
#
# External dependancy on BOOST
#
#
#set(TATUM_USE_SYSTEM_BOOST YES)
#find_package(Boost)
#if(Boost_FOUND)
    #message(STATUS "FOUND BOOST ${Boost_VERSION} at ${Boost_INCLUDE_DIRS}")
    #include_directories(SYSTEM ${Boost_INCLUDE_DIRS})
#else()
    #message(FATAL_ERROR "Could not find BOOST!")
#endif()

#
#
# External dependancy on CUDD
#
#

# We download a specific version of CUDD and build it using
# its conventional make-based build system.
include(ExternalProject)
ExternalProject_Add(project_cudd
   DOWNLOAD_DIR ${CMAKE_CURRENT_BINARY_DIR}
   URL "ftp://vlsi.colorado.edu/pub/cudd-2.5.1.tar.gz"
   URL_MD5 "e2a514c2d309feab6b697195b7615b8b"
   CONFIGURE_COMMAND ""
   BUILD_COMMAND make objlib #the object-oriented interface to CUDD
   BUILD_IN_SOURCE 1
   LOG_BUILD 0
   INSTALL_COMMAND ""
   TEST_COMMAND ""
   )
ExternalProject_Get_Property(project_cudd install_dir)

#Main CUDD library
add_library(cudd STATIC IMPORTED GLOBAL)
set_property(TARGET cudd PROPERTY IMPORTED_LOCATION ${install_dir}/src/project_cudd/cudd/libcudd.a)
add_dependencies(cudd project_cudd)

#mtr library
add_library(cudd_mtr STATIC IMPORTED GLOBAL)
set_property(TARGET cudd_mtr PROPERTY IMPORTED_LOCATION ${install_dir}/src/project_cudd/mtr/libmtr.a)
add_dependencies(cudd_mtr project_cudd)

#epd library
add_library(cudd_epd STATIC IMPORTED GLOBAL)
set_property(TARGET cudd_epd PROPERTY IMPORTED_LOCATION ${install_dir}/src/project_cudd/epd/libepd.a)
add_dependencies(cudd_epd project_cudd)

#st library
add_library(cudd_st STATIC IMPORTED GLOBAL)
set_property(TARGET cudd_st PROPERTY IMPORTED_LOCATION ${install_dir}/src/project_cudd/st/libst.a)
add_dependencies(cudd_st project_cudd)

#util library
add_library(cudd_util STATIC IMPORTED GLOBAL)
set_property(TARGET cudd_util PROPERTY IMPORTED_LOCATION ${install_dir}/src/project_cudd/util/libutil.a)
add_dependencies(cudd_util project_cudd)

#Object Oriented CUDD interface
add_library(cudd_obj STATIC IMPORTED GLOBAL)
set_property(TARGET cudd_obj PROPERTY IMPORTED_LOCATION ${install_dir}/src/project_cudd/obj/libobj.a)
add_dependencies(cudd_obj project_cudd)

#Collect up all the cudd libraries
set(CUDD_LIBS
    #NOTE: the link order of the various CUDD sub-libraries is very important!
    #      incorrect ordering will case linker errors!
    cudd_obj cudd cudd_mtr cudd_st cudd_util cudd_epd )

message(STATUS "CUDD INSTALL DIR: ${install_dir}")

#Export include dirs to we can compile against CUDD
include_directories(${install_dir}/src/project_cudd/include/ ${install_dir}/src/project_cudd/obj/ ${install_dir}/src/project_cudd/cudd/)

enable_testing()

add_subdirectory(src)
add_subdirectory(libs)
add_subdirectory(tests)

