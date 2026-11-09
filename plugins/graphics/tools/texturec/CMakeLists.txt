file(GLOB TEXTUREC_SOURCES CONFIGURE_DEPENDS "*.cpp" "*.h")
add_executable(texturec ${TEXTUREC_SOURCES})
target_link_libraries(texturec PRIVATE bimg_decode bimg_encode bimg)
frost_graphics_tool(texturec)
