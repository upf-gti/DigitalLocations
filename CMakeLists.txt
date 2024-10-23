
cmake_minimum_required(VERSION 3.13)

set(GTI_FABW_DEMO_DIR_ROOT        ${CMAKE_CURRENT_SOURCE_DIR})
set(GTI_FABW_DEMO_DIR_SOURCES     "${GTI_FABW_DEMO_DIR_ROOT}/src")
set(GTI_FABW_DEMO_DIR_LIBS        "${GTI_FABW_DEMO_DIR_ROOT}/libraries")

if (EMSCRIPTEN)
    project(index LANGUAGES C CXX)
else()
    project(digital_locations LANGUAGES C CXX)
endif()

add_definitions(-D_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING)

# Macro to map filters to folder structure for MSVC projects
macro(GroupSources curdir)
    if(MSVC)
		file(GLOB children RELATIVE ${PROJECT_SOURCE_DIR}/${curdir} ${PROJECT_SOURCE_DIR}/${curdir}/*)

        foreach(child ${children})
            if(IS_DIRECTORY ${PROJECT_SOURCE_DIR}/${curdir}/${child})
                GroupSources(${curdir}/${child})
            else()
                string(REPLACE "/" "\\" groupname ${curdir})
                source_group(${groupname} FILES ${PROJECT_SOURCE_DIR}/${curdir}/${child})
            endif()
        endforeach()
    endif()
endmacro()

GroupSources(src)

# Sources
macro(GTI_FABW_DEMO_FILES_APPEND)
    file(GLOB FILES_APPEND CONFIGURE_DEPENDS ${ARGV})
    list(APPEND GTI_FABW_DEMO_SOURCES ${FILES_APPEND})
endmacro()
macro(GTI_FABW_DEMO_SOURCES_APPEND)
    GTI_FABW_DEMO_FILES_APPEND(${ARGV0}/*.h)
    GTI_FABW_DEMO_FILES_APPEND(${ARGV0}/*.cpp)
endmacro()

GTI_FABW_DEMO_SOURCES_APPEND(${GTI_FABW_DEMO_DIR_SOURCES})
GTI_FABW_DEMO_SOURCES_APPEND(${GTI_FABW_DEMO_DIR_SOURCES}/engine)
GTI_FABW_DEMO_SOURCES_APPEND(${GTI_FABW_DEMO_DIR_SOURCES}/graphics)
GTI_FABW_DEMO_SOURCES_APPEND(${GTI_FABW_DEMO_DIR_SOURCES}/vpet)

add_executable(${PROJECT_NAME} ${GTI_FABW_DEMO_SOURCES})

target_include_directories(${PROJECT_NAME} PUBLIC ${GTI_FABW_DEMO_DIR_SOURCES})

set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_ENVIRONMENT "DAWN_DEBUG_BREAK_ON_ERROR=1")

set_property(DIRECTORY ${GTI_FABW_DEMO_DIR_ROOT} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 20)
set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${GTI_FABW_DEMO_DIR_ROOT}")

if (MSVC)
    add_compile_options(/Zc:__cplusplus)
endif()

if (EMSCRIPTEN)
    set(SHELL_FILE shell.html)

    include_directories(BEFORE
        "${CMAKE_BINARY_DIR}/_deps/emdawn-src/include/"
    )

    add_library(webgpu_layer
        "${CMAKE_BINARY_DIR}/_deps/emdawn-src/include/webgpu.cpp"
    )
    target_link_libraries(${PROJECT_NAME}
        webgpu_layer
    )

    add_compile_options(
        #"-fno-exceptions"
        "$<$<CONFIG:Release>:-flto>"
    )

    add_link_options(
        #"-fno-exceptions"
        "$<$<CONFIG:Release>:-flto>"
    )

    target_link_options(${PROJECT_NAME} PRIVATE

        # -sVERBOSE

        # We are using Dawn-generated bindings, not built-in ones
        -sUSE_WEBGPU=0
        # The JS libraries needed for bindings
        "--js-library=${CMAKE_BINARY_DIR}/_deps/emdawn-src/library_webgpu_enum_tables.js"
        "--js-library=${CMAKE_BINARY_DIR}/_deps/emdawn-src/library_webgpu_generated_struct_info.js"
        "--js-library=${CMAKE_BINARY_DIR}/_deps/emdawn-src/library_webgpu_generated_sig_info.js"
        "--js-library=${CMAKE_BINARY_DIR}/_deps/emdawn-src/library_webgpu.js"
        "--js-library=${CMAKE_BINARY_DIR}/_deps/emdawn-src/library_html5_webgpu.js"
        "--closure-args=--externs=${CMAKE_BINARY_DIR}/_deps/emdawn-src/webgpu-externs.js"

        -sUSE_GLFW=3
        -sALLOW_MEMORY_GROWTH
#        -sASYNCIFY
#        -sWASM=0
#        -sASSERTIONS
#        -fsanitize=address
#        -sMAXIMUM_MEMORY=2GB
        -Wdeprecated-literal-operator
        -sSTACK_SIZE=5MB
#        -sASYNCIFY_STACK_SIZE=10000
        -lembind # to bind functions from c++ to javascript
        -O3
#        -sWASM_BIGINT
        # only for dev
#        -sERROR_ON_WASM_CHANGES_AFTER_LINK
#        -O0
        # ------------

        --shell-file "${GTI_FABW_DEMO_DIR_ROOT}/${SHELL_FILE}"
        --preload-file "${GTI_FABW_DEMO_DIR_ROOT}/data@/data"
	)

	# Make sure to re-link when the shell file changes
    set_property(
        TARGET ${PROJECT_NAME}
        PROPERTY LINK_DEPENDS
        "${GTI_FABW_DEMO_DIR_ROOT}/${SHELL_FILE}"
    )

    set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".html")
endif()

# wgpuEngine
add_subdirectory(libraries/wgpuEngine)
target_link_libraries(${PROJECT_NAME} webgpuEngine)

# libzmq
set(BUILD_SHARED OFF)
set(ENABLE_CLANG OFF)
set(BUILD_TESTS OFF)
add_subdirectory(libraries/libzmq)
target_link_libraries(${PROJECT_NAME} libzmq-static)
set_property(TARGET libzmq-static PROPERTY FOLDER "External/libzmq")

# vulkan
include_directories(${Vulkan_INCLUDE_DIR})

# Enable multicore compile on VS solution
if(MSVC)
  add_definitions(/MP)
endif()

