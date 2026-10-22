#include "UI/Adapters/StoryMenuBuilder.h"
#include "UI/Styles/StudioIcons.h"
#include <QObject>

namespace EditorUi
{
namespace
{
StudioMenuItem Entry(const char* Identity, const QString& Label, const char* Icon,
                     const QString& Group = {}, std::function<void()> Invoke = {})
{
    StudioMenuItem Item;
    Item.Identity = QString::fromLatin1(Identity);
    Item.Label = Label;
    Item.Icon = MakeFrontendIcon(QString::fromLatin1(Icon), "--text-primary");
    Item.Group = Group;
    Item.Invoke = std::move(Invoke);
    return Item;
}
StudioMenuItem Unavailable(const char* Identity, const QString& Label, const char* Icon, const QString& Group = {})
{
    auto Item = Entry(Identity, Label, Icon, Group);
    Item.bEnabled = false;
    Item.Description = QObject::tr("Этот тип ноды пока не поддерживается нативным редактором истории.");
    return Item;
}
}

QVector<StudioMenuItem> BuildStoryNodeMenu(std::function<void(int)> AddNode,
                                         std::function<void()> AddEvent, bool CanAddEvent)
{
    QVector<StudioMenuItem> Items;
    auto Category = [&](const char* Identity, const QString& Label, const char* Icon,
                        QVector<StudioMenuItem> Children)
    {
        auto Item = Entry(Identity, Label, Icon);
        Item.Children = std::move(Children);
        Item.Description = QObject::tr("Доступно в веб-прототипе; поддержка в нативной истории ещё не перенесена.");
        Items.push_back(std::move(Item));
    };
    Category("scripting", QObject::tr("Скриптинг"), "Code2",
             {Unavailable("script-function", QObject::tr("Создать JavaScript-функцию…"), "FilePlus2")});
    Category("project-variables", QObject::tr("Переменные проекта"), "Database",
             {Unavailable("project-variable", QObject::tr("Переменные проекта"), "Database")});
    QVector<StudioMenuItem> Mathematics;
    const QStringList Operations = {QObject::tr("Сложение"), QObject::tr("Вычитание"), QObject::tr("Умножение"),
                                   QObject::tr("Деление"), QObject::tr("Остаток"), QObject::tr("Степень"),
                                   QObject::tr("Минимум"), QObject::tr("Максимум"), QObject::tr("Округление")};
    for (int Index = 0; Index < Operations.size(); ++Index)
    {
        auto Item = Unavailable("math", Operations[Index], "Calculator");
        Item.Identity += QString::number(Index);
        Mathematics.push_back(std::move(Item));
    }
    Category("mathematics", QObject::tr("Математические операции"), "Calculator", Mathematics);
    Category("conversions", QObject::tr("Конвертации"), "RefreshCw",
             {Unavailable("to-number", QObject::tr("В число"), "RefreshCw"),
              Unavailable("to-boolean", QObject::tr("В Да / нет"), "RefreshCw"),
              Unavailable("to-string", QObject::tr("В текст"), "RefreshCw")});
    Category("comparisons", QObject::tr("Сравнения"), "Binary",
             {Unavailable("eq", QObject::tr("= · Равно"), "Binary"),
              Unavailable("ne", QObject::tr("≠ · Не равно"), "Binary"),
              Unavailable("gte", QObject::tr("≥ · Больше или равно"), "Binary"),
              Unavailable("lte", QObject::tr("≤ · Меньше или равно"), "Binary"),
              Unavailable("gt", QObject::tr("> · Больше"), "Binary"),
              Unavailable("lt", QObject::tr("< · Меньше"), "Binary")});
    const auto Values = QObject::tr("Значения и переменные");
    Items += {Unavailable("variable", QObject::tr("Переменная · получить"), "Database", Values),
              Unavailable("set-variable", QObject::tr("Переменная · задать"), "Database", Values),
              Unavailable("literal", QObject::tr("Значение"), "Binary", Values),
              Unavailable("and", QObject::tr("И · оба условия"), "Binary", Values),
              Unavailable("or", QObject::tr("ИЛИ · любое условие"), "Binary", Values),
              Unavailable("not", QObject::tr("НЕ · инверсия условия"), "Binary", Values)};
    const auto Nodes = QObject::tr("Добавить ноду");
    auto Node = [&](const char* Identity, const QString& Label, const char* Icon, int Kind)
    { return Entry(Identity, Label, Icon, Nodes, [AddNode, Kind]() { if (AddNode) { AddNode(Kind); } }); };
    Items += {Node("dialogue", QObject::tr("Реплика"), "MessageSquare", 0),
              Unavailable("gameplay", QObject::tr("Игровая сцена"), "Gamepad2", Nodes),
              Node("choice", QObject::tr("Выбор игрока"), "GitFork", 1),
              Unavailable("branch", QObject::tr("If · Если"), "GitFork", Nodes),
              Unavailable("gate", QObject::tr("Ждать взаимодействие"), "MousePointerClick", Nodes),
              Unavailable("merge", QObject::tr("Схождение веток"), "Merge", Nodes),
              Node("end", QObject::tr("Концовка"), "Flag", 3),
              Node("action", QObject::tr("Действие"), "Zap", 2)};
    auto Event = Entry("add-event", QObject::tr("Добавить событие к выбранной\nреплике…"), "Layers",
                       QObject::tr("События"), std::move(AddEvent));
    Event.bEnabled = CanAddEvent;
    Event.Description = CanAddEvent ? QObject::tr("Выбрать или создать событие в начале реплики.")
                                    : QObject::tr("Сначала выберите одну реплику в потоке истории.");
    Items.push_back(std::move(Event));
    return Items;
}

QVector<StudioMenuItem> BuildStoryEventMenu(const QVector<StoryGraphEvent>& Events,
                                          std::function<void(QString)> AddEvent,
                                          std::function<void()> ImportEvent)
{
    QVector<StudioMenuItem> Items;
    Items.push_back(Entry("new-event", QObject::tr("Новое событие"), "Plus", {},
                          [AddEvent]() { if (AddEvent) { AddEvent({}); } }));
    if (ImportEvent)
    {
        Items.push_back(Entry("import-event", QObject::tr("Импорт события…"), "FolderOpen", {}, std::move(ImportEvent)));
    }
    for (const auto& Event : Events)
    {
        auto Item = Entry("event", Event.Name, "Layers", QObject::tr("События проекта"),
                           [AddEvent, Identity = Event.Identity]() { if (AddEvent) { AddEvent(Identity); } });
        Item.Identity = QStringLiteral("event:") + Event.Identity;
        Item.SearchText = Event.Description;
        Item.Description = Event.Description;
        Items.push_back(std::move(Item));
    }
    return Items;
}
}
