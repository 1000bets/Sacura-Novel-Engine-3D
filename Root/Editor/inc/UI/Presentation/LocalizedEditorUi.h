#pragma once

#include "Localization/EditorLocalization.h"
#include <QAction>
#include <QMenu>
#include <QLabel>
#include <QAbstractButton>
#include <QGroupBox>
#include <type_traits>
#include <QVariant>

inline void BindEditorProperty(QObject* Widget, const char* Property, const QString& Source)
{
    EditorLocalization::Get().Bind(Widget, [Widget, Property, Source]()
    {
        Widget->setProperty(Property, EditorLocalization::Get().Caption(Source));
    });
}

template<typename WidgetType>
WidgetType* BindEditorCaption(WidgetType* Widget, const QString& Source)
{
    EditorLocalization::Get().Bind(Widget, [Widget, Source]()
    {
        const QString Text = EditorLocalization::Get().Caption(Source);
        if constexpr (std::is_base_of_v<QMenu, WidgetType> || std::is_base_of_v<QGroupBox, WidgetType>)
        {
            Widget->setTitle(Text);
        }
        else
        {
            Widget->setText(Text);
        }
    });
    return Widget;
}
