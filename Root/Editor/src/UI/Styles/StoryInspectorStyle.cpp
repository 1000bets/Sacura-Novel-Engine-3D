#include "UI/Styles/StoryInspectorStyle.h"
#include "UI/Styles/ControlStyle.h"
#include <QSignalBlocker>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QFont>

namespace EditorUi
{
void StoryInspectorStyle::ApplyDialogueTypography(QTextEdit& Text)
{
    const QFont Font = ControlStyle::Font(13);
    // QTextDocument also needs the font: the Windows rich-text document can
    // otherwise retain its default serif face after stylesheet polishing.
    Text.setFont(Font);
    Text.document()->setDefaultFont(Font);
    Text.document()->setDocumentMargin(0);
    QObject::connect(&Text, &QTextEdit::textChanged, &Text, [&Text]()
    {
        const QSignalBlocker Blocker(Text);
        QTextCursor Cursor(Text.document());
        Cursor.select(QTextCursor::Document);
        QTextBlockFormat Format;
        Format.setLineHeight(165, QTextBlockFormat::ProportionalHeight);
        Cursor.mergeBlockFormat(Format);
    });
}
}
