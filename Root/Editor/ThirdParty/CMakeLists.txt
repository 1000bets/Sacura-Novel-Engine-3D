# ---------------------------------------------------------------
# Editor-owned third-party dependencies.
# Do not re-add Engine deps (SDL3, Diligent, ImGui, asset runtime) here.
# ---------------------------------------------------------------

add_subdirectory(ufbx)

add_library(SakuraEditorThirdParty INTERFACE)
add_library(Sakura::EditorThirdParty ALIAS SakuraEditorThirdParty)

target_link_libraries(SakuraEditorThirdParty INTERFACE
    Sakura::Ufbx
)
