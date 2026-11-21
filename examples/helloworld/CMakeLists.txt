# Example
file(GLOB_RECURSE HELLOWORLD_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
)

add_executable(helloworld ${HELLOWORLD_SOURCES})

target_link_libraries(helloworld
    PRIVATE
        frost
        device   # Device plugin
        graphics # Graphics plugin
        scene    # Scene plugin
        devmenu  # Devmenu plugin
)

target_include_directories(helloworld PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

if(MSVC)
    target_compile_options(helloworld PRIVATE /EHs-c-)
    target_compile_definitions(helloworld PRIVATE _HAS_EXCEPTIONS=0)
endif()

# Copy runtime contents beside the executable, preserving any authored subdirectories.
add_custom_command(TARGET helloworld POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/runtime"
        "$<TARGET_FILE_DIR:helloworld>"
        VERBATIM
)

# Run the debugger from the executable directory.
set_target_properties(helloworld PROPERTIES
        FOLDER "frost/examples"
        VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:helloworld>"
)
