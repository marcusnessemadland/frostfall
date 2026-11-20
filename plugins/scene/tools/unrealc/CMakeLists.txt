add_executable(unrealc unrealc.cpp)
target_link_libraries(unrealc PRIVATE frost)
target_include_directories(unrealc PRIVATE
	"${FROST_DIR}/plugins/scene/include"
	"${FROST_DIR}/plugins/graphics/include"
	"${BGFX_DIR}/3rdparty"
)
target_compile_definitions(unrealc PRIVATE FROST_TOOLS_PREFIX="${FROST_TOOLS_PREFIX}")
set_target_properties(unrealc PROPERTIES
	FOLDER "frost/plugins/scene/tools"
	OUTPUT_NAME "${FROST_TOOLS_PREFIX}unrealc"
)
foreach(compiler meshc texturec materialc scenec)
	if(TARGET ${compiler})
		add_dependencies(unrealc ${compiler})
	endif()
endforeach()
add_executable(frost::unrealc ALIAS unrealc)
