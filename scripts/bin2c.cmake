file(GLOB_RECURSE BIN2C_SOURCES CONFIGURE_DEPENDS
	"${FROST_DIR}/tools/bin2c/*.cpp"
	"${FROST_DIR}/tools/bin2c/*.h"
)

add_executable(bin2c ${BIN2C_SOURCES})

target_link_libraries(bin2c PRIVATE frost)

# Put in a "frost" folder in Visual Studio
set_target_properties(
	bin2c PROPERTIES FOLDER "frost/tools" #
					 OUTPUT_NAME "${FROST_TOOLS_PREFIX}bin2c" #
)

add_executable(frost::bin2c ALIAS bin2c)

if(IOS)
	set_target_properties(bin2c PROPERTIES MACOSX_BUNDLE ON MACOSX_BUNDLE_GUI_IDENTIFIER bin2c)
endif()

if(FROST_INSTALL)
	install(TARGETS bin2c EXPORT "${TARGETS_EXPORT_NAME}" DESTINATION "${CMAKE_INSTALL_BINDIR}")
endif()
