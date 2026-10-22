#pragma once
#include "UI/Presentation/StoryGraphModel.h"
#include <QStringList>
#include <QMargins>
namespace EditorUi
{
struct StoryEventStyle
{
    static constexpr qreal PhaseHeader = 40, BatchOrder = 28, EventHeight = 78, EventGap = 5;
    static constexpr int FieldGap = 10, LabelGap = 8, CoordinateGap = 5, HeadingGap = 8, GroupControlGap = 4;
    static constexpr int DescriptionHeight = 90, IconSize = 16;
    static QMargins HeadingMargins()
    {
        return {16, 0, 16, 0};
    }
    static QMargins FieldsMargins()
    {
        return {16, 14, 16, 14};
    }
    static QMargins ContextMargins()
    {
        return {16, 10, 16, 10};
    }
    static QMargins GroupsMargins()
    {
        return {16, 10, 16, 14};
    }
    static QMargins GroupHeadingMargins()
    {
        return {7, 6, 7, 6};
    }
    static QStringList Phases();
    static QStringList PhaseHeaders();
    static QStringList Joins();
    static QStringList ActionTypes();
    static QString Summary(const StoryGraphAction& Action);
    static QString Summary(const StoryGraphEvent& Event);
};
} // namespace EditorUi
