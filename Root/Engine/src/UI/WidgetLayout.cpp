#include "UI/WidgetLayout.h"
#include <QFile>
#include <QImageReader>
#include <QBuffer>
#include <QDir>
#include <QJsonDocument>
#include <QSet>
#include <QUuid>
#include <algorithm>
#include <cmath>
#include <functional>

namespace WidgetLayout
{
namespace
{
QJsonArray Pair(double X, double Y)
{
    return {X, Y};
}
QPointF Point(const QJsonObject &O, const char *Key)
{
    const auto A = O[Key].toArray();
    return {A[0].toDouble(), A[1].toDouble()};
}
QRectF Local(const QJsonObject &L, QSizeF P, double Scale)
{
    const auto Min = Point(L, "anchorMin"), Max = Point(L, "anchorMax"), Pivot = Point(L, "pivot");
    const double W = std::max(1., P.width() * (Max.x() - Min.x()) + L["width"].toDouble() * Scale);
    const double H = std::max(1., P.height() * (Max.y() - Min.y()) + L["height"].toDouble() * Scale);
    return {P.width() * (Min.x() + (Max.x() - Min.x()) * Pivot.x()) + L["x"].toDouble() * Scale - W * Pivot.x(),
            P.height() * (Min.y() + (Max.y() - Min.y()) * Pivot.y()) + L["y"].toDouble() * Scale - H * Pivot.y(), W, H};
}
} // namespace
QString NewId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}
QStringList Kinds()
{
    return {"dialogue", "mainMenu", "pauseMenu", "hud"};
}
QString KindLabel(const QString &K)
{
    return QMap<QString, QString>{
        {"dialogue", "Диалог"}, {"mainMenu", "Главное меню"}, {"pauseMenu", "Меню паузы"}, {"hud", "Игровой HUD"}}
        .value(K, K);
}
QStringList Types()
{
    return {"panel", "text", "button", "image", "choices"};
}
QString TypeLabel(const QString &T)
{
    return QMap<QString, QString>{{"panel", "Панель"},
                                  {"text", "Текст"},
                                  {"button", "Кнопка"},
                                  {"image", "Изображение"},
                                  {"choices", "Ответы игрока"}}
        .value(T, T);
}
QJsonObject Element(const QString &Type)
{
    return {{"id", NewId()},
            {"name", TypeLabel(Type)},
            {"type", Type},
            {"parentId", ""},
            {"role", "none"},
            {"text", Type == "button" ? "Кнопка"
                     : Type == "text" ? "Новый текст"
                                      : ""},
            {"action", "none"},
            {"visible", true},
            {"layout", QJsonObject{{"anchorMin", Pair(.5, .5)},
                                   {"anchorMax", Pair(.5, .5)},
                                   {"pivot", Pair(.5, .5)},
                                   {"x", 0},
                                   {"y", 0},
                                   {"width", 360},
                                   {"height", 80}}},
            {"style", QJsonObject{{"background", Type == "panel"    ? "#211c29"
                                                 : Type == "button" ? "#704e7c"
                                                                    : "transparent"},
                                  {"color", "#fff4f8"},
                                  {"fontSize", 24},
                                  {"fontFamily", "Segoe UI"},
                                  {"fontWeight", 400},
                                  {"borderColor", "#ad849d"},
                                  {"borderWidth", 0},
                                  {"borderRadius", 12},
                                  {"padding", 12},
                                  {"opacity", 1},
                                  {"textAlign", "left"},
                                  {"gap", 8}}}};
}
QJsonObject Create(const QString &Kind, const QString &Name)
{
    QJsonArray Elements;
    auto Add = [&](QString Type, QString Label, QString Parent, QString Role, double Ax, double Ay, double Bx,
                   double By, double Px, double Py, double X, double Y, double W, double H, QString Text = {},
                   QString Action = {})
    {
        auto E = Element(Type);
        E["name"] = Label;
        E["parentId"] = Parent;
        E["role"] = Role;
        E["text"] = Text;
        E["action"] = Action.isEmpty() ? "none" : Action;
        E["layout"] = QJsonObject{{"anchorMin", Pair(Ax, Ay)},
                                  {"anchorMax", Pair(Bx, By)},
                                  {"pivot", Pair(Px, Py)},
                                  {"x", X},
                                  {"y", Y},
                                  {"width", W},
                                  {"height", H}};
        auto S = E["style"].toObject();
        S["padding"] = 0;
        if (Type == "panel")
        {
            S["borderWidth"] = 1;
            S["borderRadius"] = 16;
            S["opacity"] = .96;
        }
        if (Type == "button" || Type == "choices")
            S["textAlign"] = "center";
        if (Role == "speaker")
        {
            S["color"] = "#e2b8d1";
            S["fontSize"] = 23;
            S["fontWeight"] = 600;
        }
        if (Role == "status")
            S["fontSize"] = 15;
        if (Type == "choices")
        {
            S["fontSize"] = 22;
            S["padding"] = 4;
            S["borderRadius"] = 10;
        }
        if (Type == "button")
            S["borderRadius"] = 10;
        E["style"] = S;
        Elements.append(E);
        return E["id"].toString();
    };
    if (Kind == "dialogue")
    {
        auto Root = Add("panel", "Диалоговое окно", {}, "none", 0, 1, 1, 1, .5, 1, 0, -34, -100, 205);
        Add("text", "Имя персонажа", Root, "speaker", 0, 0, 1, 0, 0, 0, 28, 18, -56, 32);
        Add("text", "Текст реплики", Root, "dialogueText", 0, 0, 1, 1, 0, 0, 28, 60, -88, -105);
        Add("text", "Подсказка", Root, "status", 0, 1, 1, 1, 0, 1, 28, -16, -90, 25);
        Add("button", "Продолжить", Root, "none", 1, 1, 1, 1, 1, 1, -20, -14, 44, 44, "›", "advance");
        Add("choices", "Ответы игрока", {}, "choices", .5, 1, .5, 1, .5, 1, 0, -255, 620, 190);
    }
    else if (Kind != "hud")
    {
        auto Root = Add("panel", "Панель меню", {}, "none", .5, .5, .5, .5, .5, .5, 0, 0, 510, 440);
        Add("text", "Заголовок", Root, Kind == "mainMenu" ? "title" : "none", 0, 0, 1, 0, 0, 0, 30, 35, -60, 90,
            KindLabel(Kind));
        Add("button", Kind == "mainMenu" ? "Начать игру" : "Продолжить", Root, "none", .5, 0, .5, 0, .5, .5, 0, 160,
            380, 60, Kind == "mainMenu" ? "Начать игру" : "Продолжить", Kind == "mainMenu" ? "start" : "resume");
    }
    return {{"id", NewId()},
            {"name", Name.isEmpty() ? KindLabel(Kind) : Name},
            {"kind", Kind},
            {"referenceSize", Pair(1280, 720)},
            {"elements", Elements}};
}
QJsonObject Defaults()
{
    QJsonArray Widgets;
    QJsonObject Ui;
    for (const auto &Kind : Kinds())
    {
        auto W = Create(Kind);
        Ui[Kind] = W["id"];
        Widgets.append(W);
    }
    return {{"format", "sakura.widgets"}, {"version", 1}, {"widgets", Widgets}, {"ui", Ui}};
}
QJsonObject Resolve(const QJsonObject &Library, const QString &Kind)
{
    for (const auto &V : Library["widgets"].toArray())
        if (V.toObject()["id"] == Library["ui"].toObject()[Kind])
            return V.toObject();
    return {};
}
QMap<QString, QRectF> Rects(const QJsonObject &W, QSizeF Size)
{
    auto Ref = W["referenceSize"].toArray();
    if (Size.isEmpty())
        Size = {Ref[0].toDouble(1280), Ref[1].toDouble(720)};
    const double Scale = std::min(Size.width() / Ref[0].toDouble(1280), Size.height() / Ref[1].toDouble(720));
    QMap<QString, QJsonObject> Elements;
    for (auto E : W["elements"].toArray())
        Elements[E.toObject()["id"].toString()] = E.toObject();
    QMap<QString, QRectF> Result;
    QSet<QString> Visiting;
    std::function<QRectF(QString)> ResolveRect = [&](QString Id) -> QRectF
    {
        if (Result.contains(Id))
            return Result[Id];
        if (Visiting.contains(Id))
            return {};
        Visiting.insert(Id);
        auto E = Elements[Id];
        auto Parent = E["parentId"].toString();
        auto P = Elements.contains(Parent) ? ResolveRect(Parent) : QRectF(QPointF(), Size);
        auto R = Local(E["layout"].toObject(), P.size(), Scale);
        R.translate(P.topLeft());
        Visiting.remove(Id);
        Result[Id] = R;
        return R;
    };
    for (auto It = Elements.begin(); It != Elements.end(); ++It)
        ResolveRect(It.key());
    return Result;
}
QJsonObject Reanchor(const QJsonObject &Layout, QSizeF P, QPointF Min, QPointF Max, QPointF Pivot)
{
    const auto R = Local(Layout, P, 1);
    auto L = Layout;
    L["anchorMin"] = Pair(Min.x(), Min.y());
    L["anchorMax"] = Pair(Max.x(), Max.y());
    L["pivot"] = Pair(Pivot.x(), Pivot.y());
    L["width"] = R.width() - P.width() * (Max.x() - Min.x());
    L["height"] = R.height() - P.height() * (Max.y() - Min.y());
    L["x"] = R.x() + R.width() * Pivot.x() - P.width() * (Min.x() + (Max.x() - Min.x()) * Pivot.x());
    L["y"] = R.y() + R.height() * Pivot.y() - P.height() * (Min.y() + (Max.y() - Min.y()) * Pivot.y());
    return L;
}
QJsonObject IndependentCopy(const QJsonObject &Widget)
{
    auto W = Widget;
    W["id"] = NewId();
    QMap<QString, QString> Ids;
    for (auto E : W["elements"].toArray())
        Ids[E.toObject()["id"].toString()] = NewId();
    QJsonArray Elements;
    for (auto V : W["elements"].toArray())
    {
        auto E = V.toObject();
        E["id"] = Ids[E["id"].toString()];
        E["parentId"] = Ids.value(E["parentId"].toString());
        Elements.append(E);
    }
    W["elements"] = Elements;
    return W;
}
bool Validate(const QJsonObject &Library, QString &Error)
{
    auto Fail = [&](const QString &E)
    {
        Error = E;
        return false;
    };
    if (Library["format"] != "sakura.widgets" || Library["version"] != 1 || !Library["widgets"].isArray())
        return Fail("Неподдерживаемый формат библиотеки UI.");
    QSet<QString> Ids, Names;
    for (auto V : Library["widgets"].toArray())
    {
        const auto W = V.toObject();
        const QStringList WidgetFields = {"id", "name", "kind", "referenceSize", "elements", "css", "sources"};
        for (auto It = W.begin(); It != W.end(); ++It)
            if (!WidgetFields.contains(It.key())) return Fail("Неподдерживаемое свойство виджета: " + It.key());
        const auto Id = W["id"].toString();
        const auto Name = W["name"].toString().trimmed().toCaseFolded();
        if (Id.isEmpty() || Ids.contains(Id) || Name.isEmpty() || Names.contains(Name) ||
            !Kinds().contains(W["kind"].toString()))
            return Fail("Проверьте название и тип виджета: названия должны быть уникальны.");
        Ids.insert(Id);
        Names.insert(Name);
        auto Ref = W["referenceSize"].toArray();
        if (Ref.size() != 2 || Ref[0].toDouble() < 100 || Ref[1].toDouble() < 100 || Ref[0].toDouble() > 7680 ||
            Ref[1].toDouble() > 7680 || !W["elements"].isArray() || W["elements"].toArray().size() > 512)
            return Fail("Размер холста должен быть от 100 до 7680; не более 512 элементов.");
        QMap<QString, QJsonObject> Elements;
        for (auto Value : W["elements"].toArray())
        {
            auto E = Value.toObject();
            auto Key = E["id"].toString();
            auto L = E["layout"].toObject();
            const QStringList Supported = {"id",     "name",    "type",   "parentId", "role", "text",
                                           "action", "visible", "layout", "style",    "src"};
            for (auto It = E.begin(); It != E.end(); ++It)
                if (!Supported.contains(It.key()))
                    return Fail("Пакет содержит неподдерживаемую привязку или свойство элемента: " + It.key());
            if (Key.isEmpty() || Elements.contains(Key) || !Types().contains(E["type"].toString()))
                return Fail("Некорректный элемент UI.");
            for (auto Field : {"x", "y", "width", "height"})
                if (!L[Field].isDouble() || !std::isfinite(L[Field].toDouble()) ||
                    std::abs(L[Field].toDouble()) > 100000)
                    return Fail("Некорректное положение элемента.");
            for (auto Field : {"anchorMin", "anchorMax", "pivot"})
            {
                auto A = L[Field].toArray();
                if (A.size() != 2)
                    return Fail("Некорректные анкеры.");
                for (auto N : A)
                    if (!N.isDouble() || N.toDouble() < 0 || N.toDouble() > 1)
                        return Fail("Анкеры должны быть от 0 до 1.");
            }
            for (int Axis = 0; Axis < 2; ++Axis)
                if (L["anchorMin"].toArray()[Axis].toDouble() > L["anchorMax"].toArray()[Axis].toDouble())
                    return Fail("Минимальный анкер больше максимального.");
            if (!QStringList{"none", "speaker", "dialogueText", "choices", "status", "title"}.contains(
                    E["role"].toString()))
                return Fail("Эта привязка пока не поддерживается.");
            if (!QStringList{"none", "advance", "start", "resume"}.contains(E["action"].toString()))
                return Fail("Это действие пока не поддерживается нативным проигрывателем.");
            if (E.contains("binding") || E.contains("eventId") || E.contains("command"))
                return Fail("Игровые привязки этого виджета пока не поддерживаются.");
            auto Src = E["src"].toString();
            if (!Src.isEmpty() &&
                (!Src.startsWith("data:image/png;base64,") && !Src.startsWith("data:image/jpeg;base64,")))
                return Fail("Изображение должно быть встроенным PNG/JPEG.");
            if (Src.size() > 3 * 1024 * 1024)
                return Fail("Изображение превышает 2 МБ.");
            if (!Src.isEmpty())
            {
                auto Bytes = QByteArray::fromBase64(Src.mid(Src.indexOf(',') + 1).toLatin1(),
                                                    QByteArray::AbortOnBase64DecodingErrors);
                QBuffer Buffer(&Bytes);
                Buffer.open(QIODevice::ReadOnly);
                QImageReader Reader(&Buffer);
                auto Size = Reader.size();
                if (Bytes.size() > 2 * 1024 * 1024 || Size.isEmpty() || Size.width() > 8192 || Size.height() > 8192 ||
                    (Reader.format() != "png" && Reader.format() != "jpeg"))
                    return Fail("Повреждённое или слишком большое изображение.");
            }
            auto Style = E["style"].toObject();
            for (auto Field : {"fontSize", "fontWeight", "borderWidth", "borderRadius", "padding", "opacity", "gap"})
                if (Style.contains(Field) && (!Style[Field].isDouble() || !std::isfinite(Style[Field].toDouble()) ||
                                              Style[Field].toDouble() < 0 || Style[Field].toDouble() > 1000))
                    return Fail("Некорректное оформление элемента.");
            Elements[Key] = E;
        }
        for (auto E : Elements)
        {
            QSet<QString> Seen{E["id"].toString()};
            auto Parent = E["parentId"].toString();
            while (!Parent.isEmpty())
            {
                if (!Elements.contains(Parent) || Seen.contains(Parent) || Elements[Parent]["type"] != "panel")
                    return Fail("Некорректная иерархия элементов.");
                Seen.insert(Parent);
                Parent = Elements[Parent]["parentId"].toString();
            }
        }
        if (!W["css"].toString().trimmed().isEmpty())
            return Fail("CSS веб-виджетов пока не поддерживается. Используйте параметры оформления.");
    }
    for (const auto &Kind : Kinds())
    {
        auto W = Resolve(Library, Kind);
        if (W.isEmpty() || W["kind"] != Kind)
            return Fail("Выберите стандартный виджет для каждого типа интерфейса.");
    }
    Error.clear();
    return true;
}
bool Read(const QString &Path, QJsonObject &Library, QString &Error)
{
    QFile File(Path);
    if (!File.open(QIODevice::ReadOnly))
    {
        Error = File.errorString();
        return false;
    }
    if (File.size() > 32 * 1024 * 1024)
    {
        Error = "Библиотека UI превышает 32 МБ.";
        return false;
    }
    QJsonParseError Parse;
    auto Candidate = QJsonDocument::fromJson(File.readAll(), &Parse).object();
    if (Parse.error != QJsonParseError::NoError)
    {
        Error = Parse.errorString();
        return false;
    }
    if (!Validate(Candidate, Error))
        return false;
    Library = Candidate;
    return true;
}
QString ProjectFile(const QString &Root)
{
    return QDir(Root).filePath("UI/Project.widgets");
}
} // namespace WidgetLayout
