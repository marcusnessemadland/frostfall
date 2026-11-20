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

# Keep the runtime asset directory separate from build outputs.
add_custom_target(helloworld_assets
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/runtime"
        "$<TARGET_FILE_DIR:helloworld>/assets/courtyard"
        VERBATIM
)
add_dependencies(helloworld helloworld_assets)
set_target_properties(helloworld_assets PROPERTIES FOLDER "frost/examples")

# Resolve the example's assets directory relative to the executable.
set_target_properties(helloworld PROPERTIES
        FOLDER "frost/examples"
        VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:helloworld>"
)
