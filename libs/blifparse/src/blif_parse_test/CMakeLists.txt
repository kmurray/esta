#
# Compiler flags come from parent
#

#
#
# Build files configuration
#
#

#Demo executable
#set(EXEC_NAME tatum_sta)

#Source files for the demo executable (not generated)
file(GLOB_RECURSE BLIF_PARSE_TEST_SOURCES *.cpp)
file(GLOB_RECURSE BLIF_PARSE_TEST_HEADERS *.hpp)

#Define Executable
add_executable(blif_parse_test
               ${BLIF_PARSE_TEST_SOURCES}
               ${BLIF_PARSE_TEST_HEADERS})

#Exectuable Includes
target_include_directories(blif_parse_test PRIVATE
                           ${BLIF_PARSE_TEST_INCLUDE_DIRS})

#Executable links to the library
target_link_libraries(blif_parse_test blifparse)

