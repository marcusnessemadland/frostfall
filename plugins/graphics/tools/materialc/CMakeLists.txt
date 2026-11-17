file(GLOB MATERIALC_SOURCES CONFIGURE_DEPENDS "*.cpp" "*.h")
add_executable(materialc ${MATERIALC_SOURCES})
target_link_libraries(materialc PRIVATE frost)
target_include_directories(materialc PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../include")
set_target_properties(materialc PROPERTIES
	FOLDER "frost/plugins/graphics/tools"
	OUTPUT_NAME "${FROST_TOOLS_PREFIX}materialc"
)
add_executable(frost::materialc ALIAS materialc)
