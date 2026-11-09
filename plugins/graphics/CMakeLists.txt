# 3rdparty bgfx
set(BGFX_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/bgfx/bgfx"
    CACHE PATH "Path to bgfx source"
    FORCE
)

set(BGFX_BUILD_TOOLS ON CACHE BOOL "Build bgfx tools" FORCE)
set(BGFX_BUILD_TOOLS_BIN2C OFF CACHE BOOL "Build bgfx bin2c" FORCE)
set(BGFX_BUILD_TOOLS_SHADER OFF CACHE BOOL "Build bgfx shader tools" FORCE)
set(BGFX_BUILD_TOOLS_GEOMETRY OFF CACHE BOOL "Build bgfx geometry tools" FORCE)
set(BGFX_BUILD_TOOLS_TEXTURE OFF CACHE BOOL "Build bgfx texture tools" FORCE)
set(BGFX_BUILD_EXAMPLES OFF CACHE BOOL "Build bgfx examples" FORCE)
set(BGFX_BUILD_TESTS OFF CACHE BOOL "Build bgfx tests" FORCE)
set(BGFX_INSTALL OFF CACHE BOOL "Install bgfx" FORCE)
set(BGFX_LIBRARY_TYPE STATIC CACHE STRING "bgfx library type" FORCE)

include(3rdparty/bgfx/cmake/bgfx/3rdparty/spirv-opt.cmake)
include(3rdparty/bgfx/cmake/bgfx/3rdparty/spirv-cross.cmake)
include(3rdparty/bgfx/cmake/bgfx/3rdparty/glslang.cmake)
include(3rdparty/bgfx/cmake/bgfx/3rdparty/webgpu.cmake)
include(3rdparty/bgfx/cmake/bgfx/3rdparty/tint.cmake)
set_target_properties(glslang PROPERTIES FOLDER "frost/plugins/graphics/3rdparty")
set_target_properties(spirv-cross PROPERTIES FOLDER "frost/plugins/graphics/3rdparty")
set_target_properties(spirv-opt PROPERTIES FOLDER "frost/plugins/graphics/3rdparty")
set_target_properties(tint PROPERTIES FOLDER "frost/plugins/graphics/3rdparty")

add_subdirectory(3rdparty/bgfx EXCLUDE_FROM_ALL)
set_target_properties(bx PROPERTIES FOLDER "frost/plugins/graphics/3rdparty")
set_target_properties(bimg PROPERTIES FOLDER "frost/plugins/graphics/3rdparty")
set_target_properties(bgfx PROPERTIES FOLDER "frost/plugins/graphics/3rdparty")
set_target_properties(bimg_decode PROPERTIES FOLDER "frost/plugins/graphics/3rdparty")
set_target_properties(bimg_encode PROPERTIES FOLDER "frost/plugins/graphics/3rdparty")

# Graphics plugin
file(GLOB GRAPHICS_SHADERS CONFIGURE_DEPENDS
	"${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.sc"
	"${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.sh"
)

file(GLOB_RECURSE GRAPHICS_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
)

add_library(graphics
    ${GRAPHICS_SOURCES}
	${GRAPHICS_SHADERS}
	${BGFX_DIR}/3rdparty/meshoptimizer/src/vertexcodec.cpp
	${BGFX_DIR}/3rdparty/meshoptimizer/src/indexcodec.cpp
)

target_link_libraries(graphics
	PUBLIC
		frost
		device # Depends on device plugin
	PRIVATE
		bgfx
		bx
)

target_include_directories(graphics
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/shaders
		${BGFX_DIR}/src
		${BGFX_DIR}/3rdparty
)

if(MSVC)
	target_compile_options(graphics PRIVATE /EHs-c-)
	target_compile_definitions(graphics PRIVATE _HAS_EXCEPTIONS=0)
endif()

set_target_properties(graphics PROPERTIES FOLDER "frost/plugins/graphics")

add_subdirectory(tools)
