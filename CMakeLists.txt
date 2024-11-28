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
)
source_group("" FILES ${no_group_source_files})

set(Source_Files__common
    "src/common/buffer.c"
    "src/common/buffer.h"
    "src/common/common.h"
    "src/common/os.c"
    "src/common/os.h"
)
source_group("Source Files\\common" FILES ${Source_Files__common})

set(Source_Files__compiler
    "src/compiler/ast.c"
    "src/compiler/ast.h"
    "src/compiler/chunk.c"
    "src/compiler/chunk.h"
    "src/compiler/compiler.c"
    "src/compiler/compiler.h"
    "src/compiler/lexer.c"
    "src/compiler/lexer.h"
    "src/compiler/parser.c"
    "src/compiler/parser.h"
    "src/compiler/resolver.c"
    "src/compiler/resolver.h"
    "src/compiler/symbol.c"
    "src/compiler/symbol.h"
    "src/compiler/token.c"
    "src/compiler/token.h"
)
source_group("Source Files\\compiler" FILES ${Source_Files__compiler})

set(Source_Files__inc
    "src/inc/ini.c"
    "src/inc/ini.h"
    "src/inc/pcg.c"
    "src/inc/pcg.h"
    "src/inc/regex.c"
    "src/inc/regex.h"
    "src/inc/utf8.c"
    "src/inc/utf8.h"
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
    "src/std/net.c"
    "src/std/net.h"
    "src/std/util.c"
    "src/std/util.h"
)
source_group("Source Files\\std" FILES ${Source_Files__std})

set(Source_Files__vm
    "src/vm/assert.c"
    "src/vm/assert.h"
    "src/vm/compiler_v1.c"
    "src/vm/compiler_v1.h"
    "src/vm/date.c"
    "src/vm/date.h"
    "src/vm/debug.c"
    "src/vm/debug.h"
    "src/vm/dict.c"
    "src/vm/dict.h"
    "src/vm/exception.c"
    "src/vm/exception.h"
    "src/vm/file.c"
    "src/vm/file.h"
    "src/vm/generator.c"
    "src/vm/generator.h"
    "src/vm/hash.c"
    "src/vm/hash.h"
    "src/vm/http.c"
    "src/vm/http.h"
    "src/vm/id.c"
    "src/vm/id.h"
    "src/vm/interceptor.c"
    "src/vm/interceptor.h"
    "src/vm/klass.c"
    "src/vm/klass.h"
    "src/vm/loop.c"
    "src/vm/loop.h"
    "src/vm/memory.c"
    "src/vm/memory.h"
    "src/vm/namespace.c"
    "src/vm/namespace.h"
    "src/vm/native.c"
    "src/vm/native.h"
    "src/vm/network.c"
    "src/vm/network.h"
    "src/vm/object.c"
    "src/vm/object.h"
    "src/vm/promise.c"
    "src/vm/promise.h"
    "src/vm/scanner.c"
    "src/vm/scanner.h"
    "src/vm/shape.c"
    "src/vm/shape.h"
    "src/vm/string.c"
    "src/vm/string.h"
    "src/vm/table.c"
    "src/vm/table.h"
    "src/vm/value.c"
    "src/vm/value.h"
    "src/vm/variable.c"
    "src/vm/variable.h"
    "src/vm/vm.c"
    "src/vm/vm.h"
)
source_group("Source Files\\vm" FILES ${Source_Files__vm})

set(ALL_FILES
    ${no_group_source_files}
    ${Source_Files__common}
    ${Source_Files__compiler}
    ${Source_Files__inc}
    ${Source_Files__std}
    ${Source_Files__vm}
)

################################################################################
# Target
################################################################################
add_executable(${PROJECT_NAME} ${ALL_FILES})

if (MSVC)
    find_package(CURL CONFIG REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE CURL::libcurl)
    target_link_libraries(${PROJECT_NAME} PRIVATE uv)
else ()
    find_package(CURL REQUIRED)
    target_link_libraries(${PROJECT_NAME} CURL::libcurl)
    target_link_libraries(${PROJECT_NAME} uv)
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