#include "Shell/EditorMainWindow.h"
#include "UI/Views/StudioActionMenu.h"
#include "UI/Styles/StudioIcons.h"
#include "Commands/EditorCommands.h"
#include "Core/Object/MemorySubsystem.h"
#include "Engine.h"
#include "World/Scene.h"
#include "World/GameObject.h"
#include "World/Serialization/SceneSerializer.h"
#include "World/Simulation/PlaySession.h"
#include "World/Components/ArrowComponent.h"
#include "World/Components/CameraComponent.h"
#include "World/Components/LightComponent.h"
#include "World/Components/MeshRendererComponent.h"
#include <QTimer>
#include <algorithm>
#include <cmath>

namespace
{
Vector3 PlacementPoint(const RenderViewCamera& Camera, QPoint Position, QSize Size)
{
    Vector3 Forward = Camera.Target - Camera.Position;
    if (Forward.LengthSquared() < .000001f) { return Camera.Target; }
    Forward.Normalize();
    Vector3 Right = Forward.Cross(Camera.Up);
    if (Right.LengthSquared() < .000001f) { return Camera.Target; }
    Right.Normalize();
    const Vector3 Up = Right.Cross(Forward);
    const float Height = static_cast<float>(std::max(1, Size.height()));
    const float Width = static_cast<float>(std::max(1, Size.width()));
    const float Field = std::tan(Camera.FieldOfViewDegrees * .01745329252f * .5f);
    Vector3 Ray = Forward + Right * ((2.f * Position.x() / Width - 1.f) * Field * Width / Height)
        + Up * ((1.f - 2.f * Position.y() / Height) * Field);
    Ray.Normalize();
    // Ground intersection; looking above the horizon uses the focus-depth plane.
    const float GroundDistance = std::abs(Ray.y) > .00001f ? -Camera.Position.y / Ray.y : -1.f;
    const float Distance = GroundDistance > 0.f && GroundDistance < Camera.FarPlane
        ? GroundDistance : std::max(2.f, (Camera.Target - Camera.Position).Length()) / std::max(.01f, Ray.Dot(Forward));
    return Camera.Position + Ray * Distance;
}
}

void EditorMainWindow::CloseSceneContextMenu()
{
    if (SceneContextMenu) { SceneContextMenu->close(); SceneContextMenu.clear(); }
}

void EditorMainWindow::FrameSelectedObject()
{
    auto* Selected = GetSelectedGameObject();
    if (!Selected || !PrimaryViewport || BoundEngine.GetPlaySession().IsSimulating()) { return; }
    auto Camera = PrimaryViewport->GetViewCamera();
    Vector3 Direction = Camera.Position - Camera.Target;
    if (Direction.LengthSquared() < .001f) { Direction = Vector3(1, 1, -1); }
    Direction.Normalize();
    const auto World = Selected->GetWorldTransform();
    Camera.Target = World.Position;
    Camera.Position = Camera.Target + Direction * std::max(2.f, World.Scale.Length() * 3.f);
    PrimaryViewport->SetViewCamera(Camera);
}

void EditorMainWindow::DuplicateSelectedObject()
{
    auto* Selected = GetSelectedGameObject();
    if (!Selected || BoundEngine.GetPlaySession().IsSimulating()) { return; }
    std::string Json;
    if (!SceneSerializer::SerializeSubtreeToJson(*Selected, Json).bOk) { return; }
    const ObjectHandle Parent = Selected->GetParent() ? Selected->GetParent()->GetObjectHandle() : ObjectHandle{};
    ObjectHandle Created;
    if (ExecuteCommand(MakePasteSubtreeCommand(GetEditScene(), std::move(Json), Parent, &Created)))
    { SelectObject(Created); }
}

void EditorMainWindow::OnViewportContextMenu(QPoint Position)
{
    CloseSceneContextMenu();
    Scene* World = GetEditScene();
    if (!World || !PrimaryViewport || bPlayRequested || BoundEngine.GetPlaySession().IsSimulating()) { return; }
    bStoryEditingContext = false;
    const ObjectHandle SceneIdentity = World->GetObjectHandle();
    const ObjectHandle Selection = GetSelectedObjectHandle();
    const bool HasSelection = World->FindByHandle(Selection) != nullptr;
    const auto Camera = PrimaryViewport->GetViewCamera();
    const Vector3 Point = PlacementPoint(Camera, Position, PrimaryViewport->size());
    const QPointer<EditorMainWindow> Guard(this);
    auto Valid = [Guard, SceneIdentity]() {
        return Guard && Guard->GetEditScene() && Guard->GetEditScene()->GetObjectHandle() == SceneIdentity
            && !Guard->bPlayRequested && !Guard->BoundEngine.GetPlaySession().IsSimulating();
    };
    auto SelectedCommand = [Guard, Valid, Selection](void (EditorMainWindow::*Command)()) {
        return [Guard, Valid, Selection, Command]() {
            if (!Valid() || !Guard->GetEditScene()->FindByHandle(Selection)) { return; }
            Guard->bStoryEditingContext = false;
            Guard->SelectObject(Selection);
            (Guard.data()->*Command)();
        };
    };
    auto Create = [Guard, Valid, Camera, Point](const QString& Name, const QString& Kind) {
        return [Guard, Valid, Camera, Point, Name, Kind]() {
            if (!Valid()) { return; }
            // Prepare off-document, then insert the complete object with one undoable command.
            auto Draft = std::unique_ptr<Scene>(MemorySubsystem::Get()->NewObject<Scene>("SceneMenuDraft"));
            auto* Object = Draft->CreateGameObject(Name.toStdString());
            Transform Pose;
            Pose.Position = Point;
            if (Kind == "camera")
            {
                auto* Component = Object->AddComponent<CameraComponent>();
                Component->FieldOfViewDegrees = Camera.FieldOfViewDegrees;
                Component->NearPlane = Camera.NearPlane;
                Component->FarPlane = Camera.FarPlane;
                Component->AspectRatio = static_cast<float>(std::max(1, Guard->PrimaryViewport->width()))
                    / std::max(1, Guard->PrimaryViewport->height());
                Component->bPrimary = Guard->GetEditScene()->FindPrimaryCamera() == nullptr;
                Pose.Position = Camera.Position;
                const Matrix CameraWorld = Matrix::CreateLookAt(Camera.Position, Camera.Target, Camera.Up).Invert();
                Pose.Rotation = Quaternion::CreateFromRotationMatrix(CameraWorld);
            }
            else if (Kind == "light")
            { Object->AddComponent<LightComponent>()->Type = RenderLightType::Point; }
            else if (Kind == "point") { Object->AddComponent<ArrowComponent>(); }
            else if (Kind == "mesh") { Object->AddComponent<MeshRendererComponent>(); }
            Object->SetTransform(Pose);
            std::string Json;
            if (!SceneSerializer::SerializeSubtreeToJson(*Object, Json).bOk) { return; }
            ObjectHandle Created;
            if (Guard->ExecuteCommand(MakePasteSubtreeCommand(Guard->GetEditScene(), std::move(Json), {}, &Created)))
            { Guard->SelectObject(Created); }
        };
    };
    using EditorUi::StudioMenuItem;
    auto Entry = [](const char* Id, const QString& Label, const char* Icon, const QString& Group,
                    std::function<void()> Invoke = {}) {
        StudioMenuItem Item;
        Item.Identity = QString::fromLatin1(Id);
        Item.Label = Label;
        Item.Icon = EditorUi::MakeFrontendIcon(QString::fromLatin1(Icon), "--text-primary");
        Item.Group = Group;
        Item.Invoke = std::move(Invoke);
        return Item;
    };
    auto Unavailable = [&](const char* Id, const QString& Label, const char* Icon, const QString& Group, const QString& Why) {
        auto Item = Entry(Id, Label, Icon, Group);
        Item.bEnabled = false;
        Item.Description = Why;
        return Item;
    };
    const QString Add = tr("Добавить объект"), Select = tr("Выделение");
    QVector<StudioMenuItem> Items;
    Items.push_back(Entry("scene.character", tr("Персонаж"), "PersonStanding", Add,
        [this]() { OpenCharacterWorkspace(); }));
    Items.push_back(Unavailable("scene.interactive", tr("Интерактивный предмет"), "MousePointer2", Add,
        tr("Шаблон интерактивного предмета пока не поддерживается нативным редактором.")));
    auto Objects = Entry("scene.objects", tr("Объект"), "Box", Add);
    Objects.Children = {
        Entry("scene.empty", tr("Пустой объект"), "Box", {}, Create(tr("Объект"), "empty")),
        Entry("scene.mesh", tr("Статический меш"), "Box", {}, Create(tr("Меш"), "mesh"))};
    for (const auto& Shape : {std::make_pair("cube", tr("Куб")), std::make_pair("sphere", tr("Сфера")), std::make_pair("cylinder", tr("Цилиндр"))})
    {
        auto Item = Unavailable(Shape.first, Shape.second, "Box", {}, tr("Готовый примитив пока отсутствует. Добавьте статический меш и выберите импортированную модель в инспекторе."));
        Objects.Children.push_back(std::move(Item));
    }
    Items.push_back(std::move(Objects));
    Items.push_back(Entry("scene.light", tr("Источник освещения"), "Lightbulb", Add, Create(tr("Источник освещения"), "light")));
    auto PointItem = Entry("scene.point", tr("Точка постановки здесь"), "MapPin", Add, Create(tr("Точка постановки"), "point"));
    PointItem.Description = tr("Объект-маркер на плоскости земли под курсором; доступен как цель действий истории. Над горизонтом используется глубина фокуса камеры.");
    Items.push_back(std::move(PointItem));
    Items.push_back(Unavailable("scene.sound", tr("Звуковое событие"), "Volume2", Add,
        tr("Звуковые события пока не поддерживаются нативным исполнением.")));
    Items.push_back(Entry("scene.camera", tr("Камера из текущего вида"), "Video", Add, Create(tr("Камера"), "camera")));
    auto Frame = Entry("scene.frame", tr("Приблизить выделение"), "Focus", Select, SelectedCommand(&EditorMainWindow::FrameSelectedObject));
    Frame.Shortcut = "F"; Frame.bEnabled = HasSelection; Items.push_back(std::move(Frame));
    Items.push_back(Unavailable("scene.group", tr("Сгруппировать выделение"), "FolderPlus", Select,
        tr("Группировка требует нескольких объектов; сейчас редактор поддерживает одиночное выделение.")));
    auto Duplicate = Entry("scene.duplicate", tr("Дублировать выделение"), "Copy", Select, SelectedCommand(&EditorMainWindow::DuplicateSelectedObject));
    Duplicate.bEnabled = HasSelection; Items.push_back(std::move(Duplicate));
    auto Delete = Entry("scene.delete", tr("Удалить выделение"), "Trash2", Select, SelectedCommand(&EditorMainWindow::OnDeleteSelectedObject));
    Delete.Shortcut = "Del"; Delete.bEnabled = HasSelection; Items.push_back(std::move(Delete));
    auto Axes = Entry("scene.axes", tr("Оси трансформации"), "Axis3d", Select);
    Axes.bEnabled = HasSelection;
    Axes.Children = {
        Entry("scene.axes.world", tr("Мировые оси"), PrimaryViewport->HasWorldAxes() ? "Check" : "Globe", {}, [Guard, Valid]() { if (Valid()) { Guard->PrimaryViewport->SetWorldAxes(true); } }),
        Entry("scene.axes.local", tr("Оси объекта"), PrimaryViewport->HasWorldAxes() ? "Box" : "Check", {}, [Guard, Valid]() { if (Valid()) { Guard->PrimaryViewport->SetWorldAxes(false); } })};
    Items.push_back(std::move(Axes));
    Items.push_back(Entry("scene.grid", PrimaryViewport->IsGridVisible() ? tr("Скрыть сетку") : tr("Показать сетку"), "Grid2X2", tr("Вид"),
        [Guard, Valid]() { if (Valid()) { Guard->PrimaryViewport->SetGridVisible(!Guard->PrimaryViewport->IsGridVisible()); } }));
    SceneContextMenu = new EditorUi::StudioActionMenu(tr("Сцена"), std::move(Items), this);
    // Scene replacement and Play can happen independently of the popup's event loop.
    auto* Lifetime = new QTimer(SceneContextMenu);
    connect(Lifetime, &QTimer::timeout, SceneContextMenu, [Menu = SceneContextMenu, Valid]() { if (!Valid() && Menu) { Menu->close(); } });
    Lifetime->start(50);
    SceneContextMenu->Open(PrimaryViewport->mapToGlobal(Position));
}
