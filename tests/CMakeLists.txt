#The gtest code is not warning clean so we need to suppress some warnings
set(GTEST_WARN_SUPPRESS_FLAGS_TO_CHECK
    "-Wno-undef"
    "-Wno-missing-declarations"
    )
foreach(flag ${GTEST_WARN_SUPPRESS_FLAGS_TO_CHECK})
    CHECK_CXX_COMPILER_FLAG(${flag} COMPILER_SUPPORTS_${flag})
    if(COMPILER_SUPPORTS_${flag})
        #Flag supported, so enable it
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${flag}")
    endif()
endforeach()

#Gtest
add_subdirectory(gtest)

#Source files
file(GLOB_RECURSE TEST_SRCS *.cpp)

add_executable(run_unit_tests ${TEST_SRCS})

include_directories(${gtest_SOURCE_DIR} ${gtest_SOURCE_DIR}/include)

target_include_directories(run_unit_tests PRIVATE
                           ${ETA_INCLUDE_DIRS})

target_link_libraries(run_unit_tests 
                      libeta
                      ${CUDD_LIBS}
                      gtest
                      gtest_main
                      )

add_test(NAME "Unit_Tests" COMMAND run_unit_tests)
