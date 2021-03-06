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
file(GLOB_RECURSE TATUM_STA_SOURCES *.cpp)
file(GLOB_RECURSE TATUM_STA_HEADERS *.hpp)

set(LEXER_SRC ${CMAKE_CURRENT_SOURCE_DIR}/parsers/vpr_timing_graph.l)
set(PARSER_SRC ${CMAKE_CURRENT_SOURCE_DIR}/parsers/vpr_timing_graph.y)

#Include directories
foreach(header ${TATUM_STA_HEADERS})
    get_filename_component(incl_dir ${header} DIRECTORY)
    list(APPEND TATUM_STA_INCLUDE_DIRS ${incl_dir})
endforeach()
#Remove duplicate include directories
list(REMOVE_DUPLICATES TATUM_STA_INCLUDE_DIRS)


#
# Configure intermediate files
#

#Directory to put the generated files (avoid clogging up main build directory)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/gen_files)

#The demo executable uses a flex/bision generated
#parser to read VPR timing graph files
#
#We use cmake to find the relevant executables
find_package(BISON REQUIRED)
find_package(FLEX REQUIRED)

#The flex/bison code is not warning clean so we need to suppress some warnings
set(FLEX_BISON_WARN_SUPPRESS_FLAGS "")
set(FLEX_BISON_WARN_SUPPRESS_FLAGS_TO_CHECK
    "-Wno-redundant-decls"  #Flex/bison generate code with redundant declarations
    "-Wno-switch-default"   #Flex/bison generate switch statments w/o default cases
    )
foreach(flag ${FLEX_BISON_WARN_SUPPRESS_FLAGS_TO_CHECK})
    CHECK_CXX_COMPILER_FLAG(${flag} COMPILER_SUPPORTS_${flag})
    if(COMPILER_SUPPORTS_${flag})
        #Flag supported, so enable it
        set(FLEX_BISON_WARN_SUPPRESS_FLAGS "${FLEX_BISON_WARN_SUPPRESS_FLAGS} ${flag}")
    endif()
endforeach(flag)

#
#
# Define the actual build targets
#
#

#Generate the lexer and parser
flex_target(vpr_timing_graph_lexer ${LEXER_SRC}
            ${CMAKE_CURRENT_BINARY_DIR}/gen_files/vpr_timing_graph.lex.cpp)
bison_target(vpr_timing_graph_parser ${PARSER_SRC}
            ${CMAKE_CURRENT_BINARY_DIR}/gen_files/vpr_timing_graph.parse.cpp)
            #VERBOSE ${CMAKE_CURRENT_BINARY_DIR}/gen_files/vpr_timing_graph.parse.log)

#Capture lexer/parser dependance
add_flex_bison_dependency(vpr_timing_graph_lexer vpr_timing_graph_parser)
#Apply suppression flags to the relevant files, must come after flex/bision
#targets for output vars to be defined
set_source_files_properties(${FLEX_vpr_timing_graph_lexer_OUTPUTS}
                            ${BISON_vpr_timing_graph_parser_OUTPUTS}
                            PROPERTIES COMPILE_FLAGS ${FLEX_BISON_WARN_SUPPRESS_FLAGS})

#Define Executable
add_executable(tatum_sta
               ${TATUM_STA_SOURCES}
               ${TATUM_STA_HEADERS}
               ${FLEX_vpr_timing_graph_lexer_OUTPUTS}
               ${BISON_vpr_timing_graph_parser_OUTPUTS})

#Exectuable Includes
target_include_directories(tatum_sta PRIVATE
                           ${TATUM_STA_INCLUDE_DIRS})

#Executable links to the library
target_link_libraries(tatum_sta tatum)

#set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
                            #PROPERTIES COMPILE_FLAGS "-fno-inline")
