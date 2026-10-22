# Frontend icons in Qt

The editor uses the original **lucide-react 0.468.0** geometry from the React
prototype, with its 24-unit viewBox, 1.65 stroke width and round caps/joins.
`frontend-icons.json` lists the 247 imported names, including React aliases.

Use `EditorUi::MakeFrontendIcon("Rotate3D")` for frontend names, or
`MakeStudioIcon` for existing native semantic names. Both use the same vector
icon engine. Disabled, hover and checked colors come from the editor palette;
the optional checked-color token distinguishes selected tools from toggles.
The icon is painted at the requested DPI, including fractional scaling.

To update the geometry after a deliberate frontend dependency update:

1. Install the frontend's locked npm dependencies and Python `fonttools`.
2. Run `python Scripts/ImportFrontendIcons.py` from the repository root.
3. Review the generated `src/UI/Styles/FrontendIconPaths.inc` and catalog.
4. Build the editor and run `Editor.UI.Foundation` and `Editor.Story.Graph.Shell`
   when the local native test targets are available.

Normal C++ builds use the checked-in geometry and require neither npm nor
fonttools nor a QtSvg deployment. License and attribution: `LUCIDE-LICENSE`.
