#pragma once
#include <QMargins>
class QTextEdit;

namespace EditorUi
{
// Layout/typography values from storyInspector.css; QSS owns widget colors/fonts.
struct StoryInspectorStyle
{
    static constexpr int DialogueHeight = 120;
    static constexpr int FieldGap = 10;
    static constexpr int LabelMinimum = 76;
    static constexpr qreal LabelFraction = .8 / 2.2;
    static QMargins HeadingMargins() { return {16, 12, 16, 12}; }
    static QMargins PrimaryMargins() { return {16, 14, 16, 14}; }
    static QMargins SectionMargins() { return {16, 8, 16, 14}; }
    static void ApplyDialogueTypography(QTextEdit& Text);
};
}
