#pragma once
#include "Materials/MaterialImageRequest.h"
#include "Materials/DynamicMaterialInstance.h"
#include "Materials/MaterialWidgetBinding.h"
#include <QWidget>
#include <QImage>
#include <QTimer>
#include <QElapsedTimer>

class Engine;

class MaterialWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MaterialWidget(Engine& InEngine, QWidget* Parent = nullptr);
    ~MaterialWidget() override;
    void SetMaterial(const AssetKey& Material);
    void SetDynamicMaterial(std::shared_ptr<DynamicMaterialInstance> Material);
    void SetColor(const DirectX::SimpleMath::Vector4& Color);
    void RefreshMaterial();
    void SetPreviewShape(int32_t Shape);
    const QImage& GetImage() const { return Image; }
    QString GetDiagnostic() const { return Diagnostic; }
    uint64_t GetGeneration() const { return Generation; }

protected:
    void paintEvent(QPaintEvent* Event) override;
    void resizeEvent(QResizeEvent* Event) override;
    void showEvent(QShowEvent* Event) override;
    void hideEvent(QHideEvent* Event) override;
    bool event(QEvent* Event) override;

private:
    void TickMaterial();
    void InvalidateRequest();
    Engine& BoundEngine;
    AssetKey MaterialAsset;
    std::shared_ptr<MaterialWidgetBinding> Binding = std::make_shared<MaterialWidgetBinding>();
    std::shared_ptr<DynamicMaterialInstance> DynamicMaterial;
    DirectX::SimpleMath::Vector4 Color = DirectX::SimpleMath::Vector4::One;
    std::shared_ptr<MaterialImageRequest> PendingImage;
    QImage Image;
    QString Diagnostic;
    QTimer Timer;
    QElapsedTimer Clock;
    uint64_t Generation = 1;
    uint64_t MaterialRevision = 0;
    uint64_t DynamicRevision = 0;
    qint64 LastSubmission = -1000;
    bool bDirty = true;
    MaterialBlendMode BlendMode = MaterialBlendMode::Translucent;
    bool bPreview = false;
    int32_t PreviewShape = 0;
};
