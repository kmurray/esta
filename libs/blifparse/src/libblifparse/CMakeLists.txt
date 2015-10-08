#
# Compiler flags come from parent
#

#
#
# Build files configuration
#
#

#Source files for the library
file(GLOB_RECURSE LIB_BLIF_PARSE_SOURCES *.cpp)
file(GLOB_RECURSE LIB_BLIF_PARSE_HEADERS *.hpp)

set(LEXER_SRC ${CMAKE_CURRENT_SOURCE_DIR}/blif_parse.l)
set(PARSER_SRC ${CMAKE_CURRENT_SOURCE_DIR}/blif_parse.y)

#Include directories
foreach(header ${LIB_BLIF_PARSE_HEADERS})
    get_filename_component(incl_dir ${header} DIRECTORY)
    list(APPEND LIB_BLIF_PARSE_INCLUDE_DIRS ${incl_dir})
endforeach()
#Remove duplicate include directories
list(REMOVE_DUPLICATES LIB_BLIF_PARSE_INCLUDE_DIRS)


#
# Configure intermediate files
#

#Directory to put the generated files (avoid clogging up main build directory)
set(GEN_FILES_DIR ${CMAKE_CURRENT_BINARY_DIR}/gen_files)
file(MAKE_DIRECTORY ${GEN_FILES_DIR})

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
    "-Wno-unused-function"  #yyunput not used
    "-Wno-missing-declarations"  #yyerror not defined
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

set(FLEX_GEN_SRC ${GEN_FILES_DIR}/blif_parse.lex.cpp)
set(FLEX_GEN_HDR ${GEN_FILES_DIR}/blif_parse.lex.hpp)
set(FLEX_GEN_OUTPUTS ${FLEX_GEN_SRC} ${FLEX_GEN_HDR})

set(BISON_GEN_SRC ${GEN_FILES_DIR}/blif_parse.par.cpp)
set(BISON_GEN_HDR ${GEN_FILES_DIR}/blif_parse.par.hpp)
set(BISON_GEN_OUTPUTS ${BISON_GEN_SRC} ${BISON_GEN_HDR})

#Generate the lexer
add_custom_command(OUTPUT ${FLEX_GEN_SRC} ${FLEX_GEN_HDR}
                   COMMAND ${FLEX_EXECUTABLE} --header-file=${FLEX_GEN_HDR} -o ${FLEX_GEN_SRC} ${LEXER_SRC}
                   DEPENDS ${LEXER_SRC} ${BISON_GEN_HDR})

#Generate the parser
add_custom_command(OUTPUT ${BISON_GEN_SRC} ${BISON_GEN_HDR}
                   COMMAND ${BISON_EXECUTABLE} --report=all -d -o ${BISON_GEN_SRC} ${PARSER_SRC}
                   DEPENDS ${PARSER_SRC})


#Apply suppression flags to the relevant files, must come after flex/bision
#targets for output vars to be defined
set_source_files_properties(${FLEX_GEN_OUTPUTS}
                            ${BISON_GEN_OUTPUTS}
                            PROPERTIES COMPILE_FLAGS ${FLEX_BISON_WARN_SUPPRESS_FLAGS})

#Indicate where to look for generated header files
include_directories(${GEN_FILES_DIR})

#Define the library
add_library(blifparse STATIC ${LIB_BLIF_PARSE_SOURCES} ${LIB_BLIF_PARSE_HEADERS} ${FLEX_GEN_OUTPUTS} ${BISON_GEN_OUTPUTS})

#Export library headers
target_include_directories(blifparse PUBLIC ${LIB_BLIF_PARSE_INCLUDE_DIRS})

