add_executable(RLModKitInjector main.cpp)

set_target_properties(RLModKitInjector PROPERTIES
    OUTPUT_NAME "RLModKit"          # produces RLModKit.exe
)

target_link_libraries(RLModKitInjector PRIVATE psapi)

# Make sure the core DLL is built before the injector so a one-shot build
# leaves both artifacts side-by-side in bin/.
add_dependencies(RLModKitInjector RLModKitCore)
