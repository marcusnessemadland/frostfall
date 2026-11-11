set(BGFX_CMAKE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty/bgfx/cmake/bgfx")
foreach(dependency spirv-opt spirv-cross glslang webgpu tint)
	include("${BGFX_CMAKE_DIR}/3rdparty/${dependency}.cmake")
endforeach()
set_target_properties(glslang spirv-cross spirv-opt tint PROPERTIES FOLDER "frost/plugins/graphics/3rdparty")

file(GLOB SHADERC_SOURCES CONFIGURE_DEPENDS "*.cpp" "*.h")
add_executable(shaderc ${SHADERC_SOURCES})
target_link_libraries(shaderc PRIVATE bx bimg bgfx-vertexlayout bgfx-shader glslang spirv-opt spirv-cross webgpu tint)
target_include_directories(shaderc PRIVATE
	"${BGFX_DIR}/src"
	"${BGFX_DIR}/3rdparty"
	"${BGFX_DIR}/3rdparty/dawn/src"
)
set_target_properties(shaderc PROPERTIES
	FOLDER "frost/plugins/graphics/tools"
	OUTPUT_NAME "${FROST_TOOLS_PREFIX}shaderc"
)
add_executable(frost::shaderc ALIAS shaderc)

# The Direct3D compilers are loaded dynamically, relative to the executable.
if(WIN32)
	target_include_directories(shaderc PRIVATE
		"${BGFX_DIR}/3rdparty/directx-headers/include/directx"
		"${BGFX_DIR}/3rdparty/directx-headers/include"
	)
	set(SHADERC_RUNTIME_FILES
		"${BGFX_DIR}/tools/bin/windows/d3dcompiler_47.dll"
		"${BGFX_DIR}/tools/bin/windows/dxcompiler.dll"
		"${BGFX_DIR}/tools/bin/windows/dxil.dll"
	)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	target_include_directories(shaderc PRIVATE
		"${BGFX_DIR}/3rdparty/directx-headers/include/directx"
		"${BGFX_DIR}/3rdparty/directx-headers/include"
		"${BGFX_DIR}/3rdparty/directx-headers/include/wsl/stubs"
		"${BGFX_DIR}/3rdparty/d3d4linux/include"
	)
	set(SHADERC_RUNTIME_FILES
		"${BGFX_DIR}/tools/bin/linux/libdxcompiler.so"
		"${BGFX_DIR}/tools/bin/linux/libdxil.so"
		"${BGFX_DIR}/tools/bin/windows/d3d4linux.exe"
		"${BGFX_DIR}/tools/bin/windows/d3dcompiler_47.dll"
	)
endif()
if(SHADERC_RUNTIME_FILES)
	add_custom_command(TARGET shaderc POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SHADERC_RUNTIME_FILES} "$<TARGET_FILE_DIR:shaderc>"
		VERBATIM
	)
endif()
