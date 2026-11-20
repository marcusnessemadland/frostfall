add_executable(scenec scenec.cpp)
target_link_libraries(scenec PRIVATE scene)
set_target_properties(scenec PROPERTIES
	FOLDER "frost/plugins/scene/tools"
	OUTPUT_NAME "${FROST_TOOLS_PREFIX}scenec"
)
add_executable(frost::scenec ALIAS scenec)
