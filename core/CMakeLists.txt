add_library(RLModKitCore SHARED
    dllmain.cpp
    Logger.cpp
    EacGuard.cpp
    Services.cpp
    PluginManager.cpp
    Console.cpp
    Renderer.cpp
)

set_target_properties(RLModKitCore PROPERTIES
    OUTPUT_NAME "RLModKitCore"      # produces RLModKitCore.dll
    PREFIX ""                        # no "lib" prefix
)

target_link_libraries(RLModKitCore PRIVATE
    rlmk_sdk
    ${RLMK_GUI_LIBS}
    d3d11
    dxgi
    psapi
)

# Ensure the runtime plugins directory exists in the output tree.
add_custom_command(TARGET RLModKitCore POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${RLMK_PLUGIN_DIR}"
)
