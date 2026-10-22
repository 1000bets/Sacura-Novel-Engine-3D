#include "UI/Styles/StoryEventStyle.h"
#include <QObject>
namespace EditorUi
{
QStringList StoryEventStyle::Phases()
{
    return {QObject::tr("До реплики"), QObject::tr("Во время реплики"), QObject::tr("После реплики")};
}
QStringList StoryEventStyle::PhaseHeaders()
{
    return {QObject::tr("ДО ТЕКСТА"), QObject::tr("ВМЕСТЕ С ТЕКСТОМ"), QObject::tr("ПОСЛЕ ОТВЕТА ИГРОКА")};
}
QStringList StoryEventStyle::Joins()
{
    return {QObject::tr("Не задерживает поток"), QObject::tr("Ждём запуска"),
            QObject::tr("Ждём готовности · фон продолжится"), QObject::tr("Ждём завершения")};
}
QStringList StoryEventStyle::ActionTypes()
{
    return {QObject::tr("Ожидание"), QObject::tr("Переместить объект"), QObject::tr("Смена камеры"),
            QObject::tr("Движение по маршруту")};
}
QString StoryEventStyle::Summary(const StoryGraphAction& Action)
{
    return ActionTypes().value(Action.Kind) + QStringLiteral(" · %1 с").arg(Action.Duration, 0, 'g', 4);
}
QString StoryEventStyle::Summary(const StoryGraphEvent& Event)
{
    QStringList Values;
    int Count = 0;
    for (const auto& Group : Event.Groups)
    {
        for (const auto& Action : Group.Actions)
        {
            if (Count++ < 2)
            {
                Values.push_back(Summary(Action));
            }
        }
    }
    if (Count > 2)
    {
        Values.push_back(QObject::tr("ещё %1").arg(Count - 2));
    }
    return Values.join(QStringLiteral(" · "));
}
} // namespace EditorUi
