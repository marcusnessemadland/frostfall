add_library(devmenu src/devmenu.cpp include/devmenu.h)

target_link_libraries(devmenu PUBLIC frost device graphics)
target_include_directories(devmenu PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

if(MSVC)
	target_compile_options(devmenu PRIVATE /EHs-c-)
	target_compile_definitions(devmenu PRIVATE _HAS_EXCEPTIONS=0)
endif()

set_target_properties(devmenu PROPERTIES FOLDER "frost/plugins/devmenu")
