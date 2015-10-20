#
# Compiler flags come from parent
#

#Source files for the library
file(GLOB_RECURSE CPP_ARGPARSE_SOURCES *.cpp)
file(GLOB_RECURSE CPP_ARGPARSE_HEADERS *.h)

#Include directories
foreach(header ${CPP_ARGPARSE_HEADERS})
    get_filename_component(incl_dir ${header} DIRECTORY)
    list(APPEND CPP_ARGPARSE_INCLUDE_DIRS ${incl_dir})
endforeach()
#Remove duplicate include directories
list(REMOVE_DUPLICATES CPP_ARGPARSE_INCLUDE_DIRS)


#
#
# Define the actual build targets
#
#

#Define the library
add_library(cpp-argparse STATIC ${CPP_ARGPARSE_SOURCES} ${CPP_ARGPARSE_HEADERS})

#Export library headers
target_include_directories(cpp-argparse PUBLIC ${CPP_ARGPARSE_INCLUDE_DIRS})

