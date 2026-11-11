# Ensure the directory exists
if(NOT IS_DIRECTORY "${FROST_DIR}")
	message(SEND_ERROR "Could not load frost, directory does not exist. ${FROST_DIR}")
	return()
endif()

include(GNUInstallDirs)

# Grab the frost source files
file(GLOB_RECURSE FROST_SOURCES CONFIGURE_DEPENDS
	"${FROST_DIR}/include/*.h"
	"${FROST_DIR}/include/*.inl"
	"${FROST_DIR}/src/*.cpp"
	"${FROST_DIR}/scripts/*.natvis"
)

# Build individual source files only.
list(FILTER FROST_SOURCES EXCLUDE REGEX "/amalgamated\\.[^/]*$")

add_library(frost STATIC ${FROST_SOURCES})

if(MSVC)
	target_compile_options(frost PRIVATE /EHs-c-)
	target_compile_definitions(frost PRIVATE _HAS_EXCEPTIONS=0)
endif()

# Put in a "frost" folder in Visual Studio
set_target_properties(frost PROPERTIES FOLDER "frost")

# Build system specific configurations
if(MINGW)
	set(FROST_COMPAT_PLATFORM mingw)
elseif(WIN32)
	set(FROST_COMPAT_PLATFORM msvc)
elseif(APPLE) # APPLE is technically UNIX... ORDERING MATTERS!
	set(FROST_COMPAT_PLATFORM osx)
elseif(UNIX)
	set(FROST_COMPAT_PLATFORM linux)
endif()

# Add include directory of frost
target_include_directories(
	frost
	PUBLIC $<BUILD_INTERFACE:${FROST_DIR}/include> #
		   $<BUILD_INTERFACE:${FROST_DIR}/3rdparty> #
		   $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}> #
		   $<BUILD_INTERFACE:${FROST_DIR}/include/compat/${FROST_COMPAT_PLATFORM}> #
		   $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/frost/compat/${FROST_COMPAT_PLATFORM}> #
)

# All configurations
target_compile_definitions(frost PUBLIC "FROST_CONFIG_DEBUG=$<IF:$<CONFIG:Debug>,1,$<BOOL:${FROST_CONFIG_DEBUG}>>")
target_compile_definitions(frost PUBLIC "__STDC_LIMIT_MACROS")
target_compile_definitions(frost PUBLIC "__STDC_FORMAT_MACROS")
target_compile_definitions(frost PUBLIC "__STDC_CONSTANT_MACROS")

target_compile_features(frost PUBLIC cxx_std_20)
# (note: see frost\scripts\toolchain.lua for equivalent compiler flag)
target_compile_options(frost PUBLIC $<$<CXX_COMPILER_ID:MSVC>:/Zc:__cplusplus /Zc:preprocessor>)

# frost/include/frost/simd_t.h includes smmintrin.h unconditionally, so x86/x64 builds need SSE4.2.
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64|i[3-6]86|x86)$" OR
	CMAKE_CXX_COMPILER_ARCHITECTURE_ID MATCHES "^(x86_64|amd64|AMD64|i[3-6]86|x86)$")
	target_compile_options(frost PUBLIC $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-msse4.2>)
endif()

# Link against psapi on Windows
if(WIN32)
	target_link_libraries(frost PUBLIC psapi bcrypt)
endif()

# Additional dependencies on Unix
if(ANDROID)
	# For __android_log_write
	find_library(LOG_LIBRARY log)
	mark_as_advanced(LOG_LIBRARY)
	target_link_libraries(frost PUBLIC ${LOG_LIBRARY})
elseif(APPLE)
	find_library(FOUNDATION_LIBRARY Foundation)
	mark_as_advanced(FOUNDATION_LIBRARY)
	target_link_libraries(frost PUBLIC ${FOUNDATION_LIBRARY})
elseif(UNIX)
	# Threads
	find_package(Threads)
	target_link_libraries(frost ${CMAKE_THREAD_LIBS_INIT} dl)

	# Real time (for clock_gettime)
	target_link_libraries(frost rt)
endif()

if(FROST_INSTALL)
	install(
		TARGETS frost
		EXPORT "${TARGETS_EXPORT_NAME}"
		LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
		ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
		RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
	)
	# We will make sure tinystl and compat are not installed in /usr/include
	install(DIRECTORY "${FROST_DIR}/include/frost" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
	install(DIRECTORY "${FROST_DIR}/include/compat/${FROST_COMPAT_PLATFORM}"
			DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/frost/compat"
	)
	install(DIRECTORY "${FROST_DIR}/include/tinystl" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/frost")
endif()
