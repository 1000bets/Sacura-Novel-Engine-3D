#include "UI/MaterialWidget.h"
#include "Engine.h"
#include "Rendering/RHI/Renderer.h"
#include <QPainter>
#include <QResizeEvent>
#include <QShowEvent>
#include <QHideEvent>
#include <QtMath>

MaterialWidget::MaterialWidget(Engine& InEngine, QWidget* Parent)
    : QWidget(Parent), BoundEngine(InEngine)
{
    BoundEngine.RegisterMaterialWidgetBinding(Binding);
    setAttribute(Qt::WA_TranslucentBackground);
    setMinimumSize(1, 1);
    Clock.start();
    Timer.setInterval(16);
    connect(&Timer, &QTimer::timeout, this, &MaterialWidget::TickMaterial);
}

MaterialWidget::~MaterialWidget()
{
    Timer.stop();
    InvalidateRequest();
}

void MaterialWidget::InvalidateRequest()
{
    ++Generation;
    if (PendingImage)
    {
        PendingImage->bCancelled.store(true);
        PendingImage.reset();
    }
    bDirty = true;
}

void MaterialWidget::SetMaterial(const AssetKey& Material)
{
    MaterialAsset = Material;
    DynamicMaterial.reset();
    Binding->Asset = Material;
    Binding->Dynamic.reset();
    InvalidateRequest();
}

void MaterialWidget::SetDynamicMaterial(std::shared_ptr<DynamicMaterialInstance> Material)
{
    DynamicMaterial = std::move(Material);
    Binding->Dynamic = DynamicMaterial;
    InvalidateRequest();
}

void MaterialWidget::SetColor(const DirectX::SimpleMath::Vector4& Value)
{
    Color = Value;
    InvalidateRequest();
}

void MaterialWidget::RefreshMaterial()
{
    InvalidateRequest();
}

void MaterialWidget::SetPreviewShape(int32_t Shape)
{
    bPreview = true;
    Binding->bPreview = true;
    PreviewShape = Shape;
    InvalidateRequest();
}

void MaterialWidget::TickMaterial()
{
    if (!isVisible() || window()->isMinimized() || !BoundEngine.IsInitialized())
    {
        return;
    }
    auto& Thread = BoundEngine.GetRenderThread();
    if (!Thread.IsReady())
    {
        return;
    }
    Thread.Enqueue([&Thread]() { Thread.GetRenderer()->PollMaterialImages(); });
    if (PendingImage)
    {
        const auto Completed = PendingImage;
        const auto State = PendingImage->State.load();
        if (State == MaterialImageState::Ready && PendingImage->Generation == Generation)
        {
            std::lock_guard<std::mutex> Lock(Completed->Mutex);
            Image = QImage(PendingImage->Pixels.data(), static_cast<int>(PendingImage->Width),
                static_cast<int>(PendingImage->Height), static_cast<int>(PendingImage->Width * 4), QImage::Format_RGBA8888_Premultiplied).copy();
            Image.setDevicePixelRatio(devicePixelRatioF());
            Diagnostic.clear();
            update();
            PendingImage.reset();
        }
        else if (State == MaterialImageState::Failed || State == MaterialImageState::Cancelled)
        {
            std::lock_guard<std::mutex> Lock(Completed->Mutex);
            Diagnostic = QString::fromStdString(PendingImage->Diagnostic);
            PendingImage.reset();
            update();
        }
        else
        {
            return;
        }
    }
    const auto Snapshot = BoundEngine.GetSceneAssetResolver().ResolveMaterial(MaterialAsset, DynamicMaterial);
    const auto MaterialDiagnostic = BoundEngine.GetSceneAssetResolver().GetMaterialDiagnostic(MaterialAsset);
    if (!MaterialDiagnostic.empty())
    {
        Diagnostic = QString::fromStdString(MaterialDiagnostic);
        update();
    }
    if (!Snapshot)
    {
        return;
    }
    if (!bPreview && Snapshot->Material->Definition.Domain != MaterialDomain::UserInterface)
    {
        Diagnostic = tr("Material domain must be UserInterface");
        update();
        return;
    }
    uint64_t CurrentDynamicRevision = 0;
    if (DynamicMaterial)
    {
        CurrentDynamicRevision = DynamicMaterial->GetRevision();
    }
    bDirty = bDirty || MaterialRevision != Snapshot->Revision || DynamicRevision != CurrentDynamicRevision;
    BlendMode = Snapshot->Material->Definition.BlendMode;
    const bool bAnimated = Snapshot->Material->Definition.bAnimated;
    if ((!bDirty && !bAnimated) || Clock.elapsed() - LastSubmission < 33)
    {
        return;
    }
    auto Request = std::make_shared<MaterialImageRequest>();
    Request->Snapshot = Snapshot;
    Request->Generation = Generation;
    Request->Width = static_cast<uint32_t>(qBound(1, qCeil(width() * devicePixelRatioF()), 8192));
    Request->Height = static_cast<uint32_t>(qBound(1, qCeil(height() * devicePixelRatioF()), 8192));
    Request->Time = static_cast<float>(Clock.elapsed()) / 1000.f;
    if (!bPreview && (BoundEngine.IsGameRunning() || BoundEngine.GetPlaySession().IsSimulating()))
    {
        Request->Time = BoundEngine.GetMaterialTime();
    }
    Request->bPreview = bPreview;
    Request->PreviewShape = PreviewShape;
    Request->Color = Color;
    const auto Result = Thread.Enqueue([&Thread, Request]() { Thread.GetRenderer()->QueueMaterialImage(Request); });
    if (Result == RenderCommandEnqueueResult::Accepted)
    {
        PendingImage = Request;
        bDirty = false;
        MaterialRevision = Snapshot->Revision;
        DynamicRevision = CurrentDynamicRevision;
        LastSubmission = Clock.elapsed();
    }
}

void MaterialWidget::paintEvent(QPaintEvent*)
{
    QPainter Painter(this);
    if (!Diagnostic.isEmpty() && Image.isNull())
    {
        Painter.fillRect(rect(), QColor(160, 20, 120, 200));
        Painter.setPen(Qt::white);
        Painter.drawText(rect().adjusted(8, 8, -8, -8), Qt::TextWordWrap, Diagnostic);
        return;
    }
    if (!Image.isNull())
    {
        if (BlendMode == MaterialBlendMode::Additive)
        {
            Painter.setCompositionMode(QPainter::CompositionMode_Plus);
        }
        Painter.drawImage(rect(), Image);
    }
}

void MaterialWidget::resizeEvent(QResizeEvent* Event)
{
    QWidget::resizeEvent(Event);
    InvalidateRequest();
}

void MaterialWidget::showEvent(QShowEvent* Event)
{
    QWidget::showEvent(Event);
    Binding->bVisible = true;
    InvalidateRequest();
    Timer.start();
}

void MaterialWidget::hideEvent(QHideEvent* Event)
{
    Timer.stop();
    Binding->bVisible = false;
    InvalidateRequest();
    QWidget::hideEvent(Event);
}

bool MaterialWidget::event(QEvent* Event)
{
    if (Event->type() == QEvent::DevicePixelRatioChange)
    {
        InvalidateRequest();
    }
    return QWidget::event(Event);
}
