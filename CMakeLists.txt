cmake_minimum_required(VERSION 4.3)
project(csc_gles LANGUAGES C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(ANDROID)
    set(GLES_LIB GLESv3)
else()
    set(GLES_LIB GLESv2)
endif()

include_directories(include/ )

# csc_gles library
add_library(csc_gles SHARED)
file(GLOB csc_gles_srcs CONFIGURE_DEPENDS src/*.c src/*.h)
target_sources(csc_gles PRIVATE ${csc_gles_srcs})
target_link_libraries(csc_gles PRIVATE EGL)
target_link_libraries(csc_gles PRIVATE ${GLES_LIB})

# Demo for using OpenGL ES directly
add_executable(csc_gles_demo)
file(GLOB csc_gles_demo_srcs CONFIGURE_DEPENDS test/csc_gles_demo/*.c test/csc_gles_demo/*.h)
target_sources(csc_gles_demo PUBLIC ${csc_gles_demo_srcs})
target_link_libraries(csc_gles_demo PRIVATE EGL)
target_link_libraries(csc_gles_demo PRIVATE ${GLES_LIB})

# Test program for csc_gles library
add_executable(csc_gles_test_simple)
file(GLOB csc_gles_test_simple_srcs CONFIGURE_DEPENDS test/csc_gles_test_simple/*.c test/csc_gles_test_simple/*.h)
target_sources(csc_gles_test_simple PUBLIC ${csc_gles_test_simple_srcs})
target_link_libraries(csc_gles_test_simple PRIVATE csc_gles)

if(ANDROID)

# Test program for csc_gles library
add_executable(csc_gles_test_android)
file(GLOB csc_gles_test_android_srcs CONFIGURE_DEPENDS test/csc_gles_test_android/*.c test/csc_gles_test_android/*.h)
target_sources(csc_gles_test_android PUBLIC ${csc_gles_test_android_srcs})
target_link_libraries(csc_gles_test_android PRIVATE csc_gles)

endif()