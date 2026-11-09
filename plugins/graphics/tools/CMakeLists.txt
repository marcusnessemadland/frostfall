# Match the host directory names used by bgfx's tools.mk and our shader makefile.
if(CMAKE_HOST_WIN32)
	set(GRAPHICS_TOOLS_OS windows)
elseif(CMAKE_HOST_APPLE)
	set(GRAPHICS_TOOLS_OS darwin)
else()
	set(GRAPHICS_TOOLS_OS linux)
endif()

set(FROST_GRAPHICS_TOOLS_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin/${GRAPHICS_TOOLS_OS}"
	CACHE PATH "Output directory for graphics command-line tools")

function(frost_graphics_tool name)
	set_target_properties(${name} PROPERTIES
		FOLDER "frost/plugins/graphics/tools"
		OUTPUT_NAME "${FROST_TOOLS_PREFIX}${name}"
		# A generator expression prevents multi-config generators adding a config directory.
		RUNTIME_OUTPUT_DIRECTORY "${FROST_GRAPHICS_TOOLS_OUTPUT_DIRECTORY}/$<0:>"
	)
	add_executable(frost::${name} ALIAS ${name})
endfunction()

if(FROST_BUILD_TOOLS_SHADERC)
	add_subdirectory(shaderc)
endif()
if(FROST_BUILD_TOOLS_TEXTUREC)
	add_subdirectory(texturec)
endif()
if(FROST_BUILD_TOOLS_GEOMETRYC)
	add_subdirectory(geometryc)
endif()
