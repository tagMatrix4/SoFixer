cmake_minimum_required(VERSION 3.3)
project(SoFixer)

# =========================================================
# SoFixer options
# =========================================================
set(SO_64 OFF CACHE BOOL "build SoFixer for 64bit target")

if(SO_64)
    message("building SoFixer for 64bit target")
    add_definitions("-D__SO64__")
    set(TARGET_LIB_NAME SoFixerLib64)
    set(TARGET_EXE_NAME SoFixer64)
else()
    message("building SoFixer for 32bit target")
    add_definitions("-D__SO32__")
    set(TARGET_LIB_NAME SoFixerLib32)
    set(TARGET_EXE_NAME SoFixer32)
endif()

if(MSVC)
    # Replace the default /MD or /MDd with /MT or /MTd for static runtime library linking
    # For Release builds, use /MT
    string(REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
    # For Debug builds, one would typically use /MTd
    # string(REPLACE "/MDd" "/MTd" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}") 
    # The CMAKE_CXX_FLAGS is also modified to ensure it doesn't conflict if set globally.
    # However, directly setting CMAKE_CXX_FLAGS_RELEASE/DEBUG is more targeted.

    # Ensure c++14 is still set, as the replace might affect other flags if not careful
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++14") 
    # The /MT or /MTd flags will be part of CMAKE_CXX_FLAGS_RELEASE or CMAKE_CXX_FLAGS_DEBUG respectively

    add_definitions("-D_CRT_SECURE_NO_WARNINGS")
    add_definitions("-DNOMINMAX")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
endif()

if(MINGW)
    message("build with mingw for static executable")
    # These flags help in creating a more portable executable with MinGW
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -static-libgcc -static-libstdc++")
    # We can also add to CMAKE_CXX_FLAGS if we want these applied during compilation too, 
    # but for executable portability, linker flags are key.
    # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static -static-libgcc -static-libstdc++")
endif()

set(LIB_SRC ElfReader.cpp
        ElfRebuilder.cpp
        ObElfReader.cpp)

# Add the static library
add_library(${TARGET_LIB_NAME} STATIC ${LIB_SRC})

# Add the executable that links against the static library
add_executable(${TARGET_EXE_NAME} main.cpp)
target_link_libraries(${TARGET_EXE_NAME} PRIVATE ${TARGET_LIB_NAME})

# If using MSVC and targeting static runtime, ensure the library also compiles with it.
if(MSVC)
    set_target_properties(${TARGET_LIB_NAME} PROPERTIES 
        COMPILE_FLAGS_RELEASE "/MT" 
        # COMPILE_FLAGS_DEBUG "/MTd"
    )
    set_target_properties(${TARGET_EXE_NAME} PROPERTIES 
        COMPILE_FLAGS_RELEASE "/MT" 
        # COMPILE_FLAGS_DEBUG "/MTd"
    )
endif()
