# ----------------------------------------------------------------------------
#  Third-party dependencies fetched at configure time.
#
#  - Dear ImGui : immediate-mode GUI used for the F1 console/overlay.
#  - MinHook    : lightweight x86/x64 inline hooking library used to hook the
#                 DirectX Present() call so we can draw our overlay.
#  - kiero      : tiny helper that locates the D3D/DXGI vtable to hook, so we
#                 don't have to hand-scan for the Present pointer.
#
#  All three are permissively licensed and widely used by the RL/modding scene.
# ----------------------------------------------------------------------------
include(FetchContent)

set(FETCHCONTENT_QUIET OFF)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.5
)

FetchContent_Declare(
    minhook
    GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
    GIT_TAG        v1.3.3
)

FetchContent_Declare(
    kiero
    GIT_REPOSITORY https://github.com/Rebzzel/kiero.git
    GIT_TAG        1.2.12
)

FetchContent_MakeAvailable(minhook)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
endif()
FetchContent_GetProperties(kiero)
if(NOT kiero_POPULATED)
    FetchContent_Populate(kiero)
endif()

# ImGui and kiero ship as loose sources, so build them into a small static lib
# alongside the DirectX11 + Win32 backends we actually use.
add_library(imgui_backend STATIC
    "${imgui_SOURCE_DIR}/imgui.cpp"
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp"
    "${kiero_SOURCE_DIR}/kiero.cpp"
    "${kiero_SOURCE_DIR}/minhook/src/buffer.c"   # kiero bundles a hook backend; we override to MinHook below
)

target_include_directories(imgui_backend PUBLIC
    "${imgui_SOURCE_DIR}"
    "${imgui_SOURCE_DIR}/backends"
    "${kiero_SOURCE_DIR}"
)

# Tell kiero to use MinHook as its hooking backend and target D3D11.
target_compile_definitions(imgui_backend PUBLIC
    KIERO_INCLUDE_D3D11
    KIERO_USE_MINHOOK=1
)

target_link_libraries(imgui_backend PUBLIC minhook d3d11 dxgi)

# Expose a convenience variable so targets can depend on the whole GUI stack.
set(RLMK_GUI_LIBS imgui_backend CACHE INTERNAL "GUI dependency libs")
