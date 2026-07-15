# Example minigame plugin. Copy this folder to start your own plugin.
add_library(SpeedClock SHARED SpeedClockPlugin.cpp)

set_target_properties(SpeedClock PROPERTIES
    OUTPUT_NAME "SpeedClock"
    PREFIX ""                    # produces SpeedClock.dll (no "lib" prefix)
    # Drop the built plugin straight into the core's runtime plugin folder so
    # it is auto-loaded on the next injection.
    LIBRARY_OUTPUT_DIRECTORY "${RLMK_PLUGIN_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${RLMK_PLUGIN_DIR}"
)

# Plugins only need the header-only SDK.
target_link_libraries(SpeedClock PRIVATE rlmk_sdk)

# Uncomment to let this plugin draw an ImGui settings tab:
# target_link_libraries(SpeedClock PRIVATE imgui_backend)
# target_compile_definitions(SpeedClock PRIVATE RLMK_PLUGIN_USE_IMGUI)
