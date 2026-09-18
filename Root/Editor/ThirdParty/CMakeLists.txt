# ---------------------------------------------------------------
# Editor-owned third-party dependencies.
# Do not re-add Engine deps (SDL3, Diligent, ImGui) here.
# ---------------------------------------------------------------

add_library(SakuraEditorThirdParty INTERFACE)
add_library(Sakura::EditorThirdParty ALIAS SakuraEditorThirdParty)
