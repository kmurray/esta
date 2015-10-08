#
# Compiler flags come from parent
#

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

message(STATUS "CUDD INSTALL DIR: ${install_dir}")

#Export include dirs to we can compile against CUDD
include_directories(${install_dir}/src/project_cudd/include/ ${install_dir}/src/project_cudd/obj/)

#
#
# Build files configuration
#
#

#Demo executable
#set(EXEC_NAME tatum_sta)

#Source files for the demo executable (not generated)
file(GLOB_RECURSE ETA_SOURCES *.cpp)
file(GLOB_RECURSE ETA_HEADERS *.hpp)

#Define Executable
add_executable(eta
               ${ETA_SOURCES}
               ${ETA_HEADERS})

#Exectuable Includes
target_include_directories(eta PRIVATE
                           ${ETA_INCLUDE_DIRS})

#Executable links to the library
target_link_libraries(eta blifparse tatum cudd cudd_mtr cudd_st cudd_epd cudd_util cudd_obj)

