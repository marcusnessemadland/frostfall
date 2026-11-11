file(GLOB MESHC_SOURCES CONFIGURE_DEPENDS "*.cpp" "*.h")
file(GLOB MESHC_MESHOPTIMIZER_SOURCES CONFIGURE_DEPENDS "${BGFX_DIR}/3rdparty/meshoptimizer/src/*.cpp")
add_executable(meshc ${MESHC_SOURCES} ${MESHC_MESHOPTIMIZER_SOURCES})
target_link_libraries(meshc PRIVATE frost bx bgfx-vertexlayout)
target_include_directories(meshc PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../include" "${BGFX_DIR}/src" "${BGFX_DIR}/3rdparty")
target_compile_definitions(meshc PRIVATE _CRT_SECURE_NO_WARNINGS)
set_target_properties(meshc PROPERTIES
	FOLDER "frost/plugins/graphics/tools"
	OUTPUT_NAME "${FROST_TOOLS_PREFIX}meshc"
)
add_executable(frost::meshc ALIAS meshc)
