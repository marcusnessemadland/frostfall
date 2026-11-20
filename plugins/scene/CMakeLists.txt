cmake_dependent_option(FROST_BUILD_TOOLS_SCENEC "Build scene compiler." ON "FROST_BUILD_TOOLS;NOT CMAKE_CROSSCOMPILING" OFF)

cmake_dependent_option(FROST_BUILD_TOOLS_UNREALC "Build Unreal glTF scene converter." ON "FROST_BUILD_TOOLS;NOT CMAKE_CROSSCOMPILING" OFF)

add_library(scene src/scene.cpp include/scene.h)
target_link_libraries(scene PUBLIC frost graphics)
target_include_directories(scene PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
if(MSVC)
	target_compile_options(scene PRIVATE /EHs-c-)
	target_compile_definitions(scene PRIVATE _HAS_EXCEPTIONS=0)
endif()
set_target_properties(scene PROPERTIES FOLDER "frost/plugins/scene")
add_subdirectory(tools)
