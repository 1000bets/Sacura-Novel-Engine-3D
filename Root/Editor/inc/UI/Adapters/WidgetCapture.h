#pragma once

class QString;
class QWidget;

namespace EditorUi
{
class WidgetCapture
{
public:
    // Captures Qt-owned content only, regardless of desktop occlusion.
    // A native GPU swapchain requires a separate renderer readback.
    static bool Save(QWidget& Widget, const QString& OutputFile);
};
}
