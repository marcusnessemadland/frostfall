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
frost_graphics_tool(shaderc)

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
	set(SHADERC_RUNTIME_FILES "${BGFX_DIR}/tools/bin/linux/libdxcompiler.so" "${BGFX_DIR}/tools/bin/linux/libdxil.so")
	add_custom_command(TARGET shaderc POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:shaderc>/../windows"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			"${BGFX_DIR}/tools/bin/windows/d3d4linux.exe" "${BGFX_DIR}/tools/bin/windows/d3dcompiler_47.dll"
			"$<TARGET_FILE_DIR:shaderc>/../windows"
		VERBATIM
	)
endif()
if(SHADERC_RUNTIME_FILES)
	add_custom_command(TARGET shaderc POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SHADERC_RUNTIME_FILES} "$<TARGET_FILE_DIR:shaderc>"
		VERBATIM
	)
endif()

find_program(FROST_SHADER_MAKE NAMES gmake make)
if(FROST_SHADER_MAKE)
	add_custom_target(graphics-shaders
		COMMAND "${FROST_SHADER_MAKE}" "SHADERC=$<TARGET_FILE:shaderc>" all
		WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../shaders"
		DEPENDS shaderc
		COMMENT "Regenerating embedded graphics shaders"
		VERBATIM
	)
	set_target_properties(graphics-shaders PROPERTIES FOLDER "frost/plugins/graphics/tools")
endif()
