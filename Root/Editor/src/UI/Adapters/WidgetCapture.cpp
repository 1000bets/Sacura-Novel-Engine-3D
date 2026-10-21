#include "UI/Adapters/WidgetCapture.h"

#include <QPixmap>
#include <QWidget>

namespace EditorUi
{
bool WidgetCapture::Save(QWidget& Widget, const QString& OutputFile)
{
    Widget.ensurePolished();
    return !OutputFile.isEmpty() && Widget.grab().save(OutputFile, "PNG");
}
}
