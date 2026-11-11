# 3rdparty sdl
add_subdirectory(3rdparty/sdl EXCLUDE_FROM_ALL)

set_target_properties(SDL_uclibc PROPERTIES FOLDER "frost/plugins/device/3rdparty")
set_target_properties(SDL3-shared PROPERTIES FOLDER "frost/plugins/device/3rdparty")

# Device plugin
file(GLOB_RECURSE DEVICE_SOURCES
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
)
add_library(device ${DEVICE_SOURCES})

target_link_libraries(device
	PUBLIC
		frost
	PRIVATE
		SDL3::SDL3
)

target_include_directories(device
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

if(MSVC)
	target_compile_options(device PRIVATE /EHs-c-)
	target_compile_definitions(device PRIVATE _HAS_EXCEPTIONS=0)
endif()

set_target_properties(device PROPERTIES FOLDER "frost/plugins/device")
