cmake_minimum_required(VERSION 3.16.0 FATAL_ERROR)

set(CMAKE_SYSTEM_VERSION 10.0 CACHE STRING "" FORCE)

project(CLox C)

################################################################################
# Set target arch type if empty. Visual studio solution generator provides it.
################################################################################
if(NOT CMAKE_VS_PLATFORM_NAME)
    set(CMAKE_VS_PLATFORM_NAME "x64")
endif()
message("${CMAKE_VS_PLATFORM_NAME} architecture in use")

if(NOT ("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64"
     OR "${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x86"))
    message(FATAL_ERROR "${CMAKE_VS_PLATFORM_NAME} arch is not supported!")
endif()

################################################################################
# Global configuration types
################################################################################
set(CMAKE_CONFIGURATION_TYPES
    "Debug"
    "Release"
    CACHE STRING "" FORCE
)

################################################################################
# Global compiler options
################################################################################
if(MSVC)
    # remove default flags provided with CMake for MSVC
    set(CMAKE_C_FLAGS "")
    set(CMAKE_C_FLAGS_DEBUG "")
    set(CMAKE_C_FLAGS_RELEASE "")
endif()

################################################################################
# Global linker options
################################################################################
if(MSVC)
    # remove default flags provided with CMake for MSVC
    set(CMAKE_EXE_LINKER_FLAGS "")
    set(CMAKE_MODULE_LINKER_FLAGS "")
    set(CMAKE_SHARED_LINKER_FLAGS "")
    set(CMAKE_STATIC_LINKER_FLAGS "")
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS}")
    set(CMAKE_MODULE_LINKER_FLAGS_DEBUG "${CMAKE_MODULE_LINKER_FLAGS}")
    set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS}")
    set(CMAKE_STATIC_LINKER_FLAGS_DEBUG "${CMAKE_STATIC_LINKER_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS}")
    set(CMAKE_MODULE_LINKER_FLAGS_RELEASE "${CMAKE_MODULE_LINKER_FLAGS}")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS}")
    set(CMAKE_STATIC_LINKER_FLAGS_RELEASE "${CMAKE_STATIC_LINKER_FLAGS}")
endif()

################################################################################
# Nuget packages function stub.
################################################################################
function(use_package TARGET PACKAGE VERSION)
    message(WARNING "No implementation of use_package. Create yours. "
                    "Package \"${PACKAGE}\" with version \"${VERSION}\" "
                    "for target \"${TARGET}\" is ignored!")
endfunction()

################################################################################
# Common utils
################################################################################
include(CMake/Utils.cmake)

################################################################################
# Additional Global Settings(add specific info there)
################################################################################
include(CMake/GlobalSettingsInclude.cmake OPTIONAL)

################################################################################
# Use solution folders feature
################################################################################
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

################################################################################
# Sub-projects
################################################################################

set(PROJECT_NAME CLox)

################################################################################
# Source groups
################################################################################
set(no_group_source_files
    "clox.ini"
    "CMakeLists.txt"
    "main.c"
    "README.md"
)
source_group("" FILES ${no_group_source_files})

set(Source_Files__inc
    "src/inc/ini.c"
    "src/inc/ini.h"
    "src/inc/pcg.c"
    "src/inc/pcg.h"
    "src/inc/regex.c"
    "src/inc/regex.h"
    "src/inc/uuid4.c"
    "src/inc/uuid4.h"
    "src/inc/yuarel.c"
    "src/inc/yuarel.h"    
)
source_group("Source Files\\inc" FILES ${Source_Files__inc})

set(Source_Files__std
    "src/std/collection.c"
    "src/std/collection.h"
    "src/std/io.c"
    "src/std/io.h"
    "src/std/lang.c"
    "src/std/lang.h"
    "src/std/network.c"
    "src/std/network.h"
    "src/std/util.c"
    "src/std/util.h"
)
source_group("Source Files\\std" FILES ${Source_Files__std})

set(Source_Files__vm
    "src/vm/assert.c"
    "src/vm/assert.h"
    "src/vm/chunk.c"
    "src/vm/chunk.h"
    "src/vm/common.h"
    "src/vm/compiler.c"
    "src/vm/compiler.h"
    "src/vm/debug.c"
    "src/vm/debug.h"
    "src/vm/dict.c"
    "src/vm/dict.h"
    "src/vm/hash.c"
    "src/vm/hash.h"
    "src/vm/memory.c"
    "src/vm/memory.h"
    "src/vm/native.c"
    "src/vm/native.h"
    "src/vm/object.c"
    "src/vm/object.h"
    "src/vm/os.c"
    "src/vm/os.h"
    "src/vm/parser.c"
    "src/vm/parser.h"
    "src/vm/scanner.c"
    "src/vm/scanner.h"
    "src/vm/string.c"
    "src/vm/string.h"
    "src/vm/table.c"
    "src/vm/table.h"
    "src/vm/value.c"
    "src/vm/value.h"
    "src/vm/vm.c"
    "src/vm/vm.h"
)
source_group("Source Files\\vm" FILES ${Source_Files__vm})

set(Test_Files__lox
    "test/lox/anonymous.lox"
    "test/lox/array.lox"
    "test/lox/benchmark.lox"
    "test/lox/boundmethod.lox"
    "test/lox/closure.lox"
    "test/lox/date.lox"
    "test/lox/dictionary.lox"
    "test/lox/enumerate.lox"
    "test/lox/fibonacci.lox"
    "test/lox/file.lox"
    "test/lox/fraction.lox"
    "test/lox/http.lox"
    "test/lox/initializer.lox"
    "test/lox/invoke.lox"
    "test/lox/io.lox"
    "test/lox/klass.lox"
    "test/lox/lambda.lox"
    "test/lox/lang.lox"
    "test/lox/linkedlist.lox"
    "test/lox/loop.lox"
    "test/lox/method.lox"
    "test/lox/namespace.lox"
    "test/lox/native.lox"
    "test/lox/network.lox"
    "test/lox/object.lox"
    "test/lox/queue.lox"
    "test/lox/rand.lox"
    "test/lox/range.lox"
    "test/lox/receiver.lox"
    "test/lox/reentrancy.lox"
    "test/lox/regex.lox"
    "test/lox/require.lox"
    "test/lox/set.lox"
    "test/lox/socket.lox"
    "test/lox/stack.lox"
    "test/lox/string.lox"
    "test/lox/sum.lox"
    "test/lox/super.lox"
    "test/lox/switch.lox"
    "test/lox/trait.lox"
    "test/lox/url.lox"
    "test/lox/uuid.lox"
    "test/lox/variable.lox"
)
source_group("Test Files\\lox" FILES ${Test_Files__lox})

set(Test_Files__others
    "test/others/file_input.txt"
    "test/others/file_output.txt"
)
source_group("Test Files\\others" FILES ${Test_Files__others})

set(ALL_FILES
    ${no_group_source_files}
    ${Source_Files__inc}
    ${Source_Files__std}
    ${Source_Files__vm}
    ${Test_Files__lox}
    ${Test_Files__others}
)

################################################################################
# Target
################################################################################
add_executable(${PROJECT_NAME} ${ALL_FILES})

if (MSVC)
    find_package(CURL CONFIG REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE CURL::libcurl)
else ()
    find_package(CURL REQUIRED)
    target_link_libraries(${PROJECT_NAME} CURL::libcurl)   
endif()

use_props(${PROJECT_NAME} "${CMAKE_CONFIGURATION_TYPES}" "${DEFAULT_CXX_PROPS}")
set(ROOT_NAMESPACE CLox)

set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_GLOBAL_KEYWORD "Win32Proj"
)

if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
    set_target_properties(${PROJECT_NAME} PROPERTIES
        INTERPROCEDURAL_OPTIMIZATION_RELEASE "TRUE"
    )
elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x86")
    set_target_properties(${PROJECT_NAME} PROPERTIES
        INTERPROCEDURAL_OPTIMIZATION_RELEASE "TRUE"
    )
endif()

################################################################################
# Compile definitions
################################################################################
if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        "$<$<CONFIG:Debug>:"
            "_DEBUG"
        ">"
        "$<$<CONFIG:Release>:"
            "NDEBUG"
        ">"
        "_CONSOLE;"
        "UNICODE;"
        "_UNICODE"
    )
elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x86")
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        "$<$<CONFIG:Debug>:"
            "_DEBUG"
        ">"
        "$<$<CONFIG:Release>:"
            "NDEBUG"
        ">"
        "WIN32;"
        "_CONSOLE;"
        "UNICODE;"
        "_UNICODE"
    )
endif()

################################################################################
# Compile and link options
################################################################################
if(MSVC)
    if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
        target_compile_options(${PROJECT_NAME} PRIVATE
            $<$<CONFIG:Debug>:
                /wd26451
            >
            $<$<CONFIG:Release>:
                /Oi;
                /Gy
            >
            /permissive-;
            /sdl;
            /W3;
            ${DEFAULT_CXX_DEBUG_INFORMATION_FORMAT};
            ${DEFAULT_CXX_EXCEPTION_HANDLING}
        )
    elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x86")
        target_compile_options(${PROJECT_NAME} PRIVATE
            $<$<CONFIG:Release>:
                /Oi;
                /Gy
            >
            /permissive-;
            /sdl;
            /W3;
            ${DEFAULT_CXX_DEBUG_INFORMATION_FORMAT};
            ${DEFAULT_CXX_EXCEPTION_HANDLING}
        )
    endif()
    if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
        target_link_options(${PROJECT_NAME} PRIVATE
            $<$<CONFIG:Debug>:
                /INCREMENTAL
            >
            $<$<CONFIG:Release>:
                /OPT:REF;
                /OPT:ICF;
                /INCREMENTAL:NO
            >
            /DEBUG;
            /SUBSYSTEM:CONSOLE
        )
    elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x86")
        target_link_options(${PROJECT_NAME} PRIVATE
            $<$<CONFIG:Debug>:
                /INCREMENTAL
            >
            $<$<CONFIG:Release>:
                /OPT:REF;
                /OPT:ICF;
                /INCREMENTAL:NO
            >
            /DEBUG;
            /SUBSYSTEM:CONSOLE
        )
    endif()
else ()
    target_link_libraries(${PROJECT_NAME} m)
endif()

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_LIST_DIR}/clox.ini $<TARGET_FILE_DIR:${PROJECT_NAME}>
)