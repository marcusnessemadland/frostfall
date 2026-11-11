file(GLOB TEXTUREC_SOURCES CONFIGURE_DEPENDS "*.cpp" "*.h")
add_executable(texturec ${TEXTUREC_SOURCES})
target_link_libraries(texturec PRIVATE bimg_decode bimg_encode bimg)
set_target_properties(texturec PROPERTIES
	FOLDER "frost/plugins/graphics/tools"
	OUTPUT_NAME "${FROST_TOOLS_PREFIX}texturec"
)
add_executable(frost::texturec ALIAS texturec)
