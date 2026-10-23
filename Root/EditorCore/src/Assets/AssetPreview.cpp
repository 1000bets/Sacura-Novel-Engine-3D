#include "Assets/AssetPreview.h"
#include "Assets/AssetRegistry.h"
#include "Assets/Loaders/ModelLoader.h"
#include "Assets/Loaders/TextureLoader.h"
#include "Assets/Resources/StaticMeshResource.h"
#include "Assets/Resources/TextureResource.h"
#include "Characters/CharacterDefinition.h"
#include "Materials/MaterialDocumentIO.h"
#include <QColor>
#include <algorithm>
#include <cmath>
#include <limits>

class AssetPreviewGenerator
{
public:
    static QImage Background()
    {
        QImage Image(128, 128, QImage::Format_ARGB32);
        Image.fill(QColor(30, 31, 39));
        return Image;
    }

    static QImage Texture(const AssetLoadContext& Context)
    {
        AssetDiagnostic Diagnostic;
        TextureLoader Loader;
        const auto Resource = std::static_pointer_cast<const TextureResource>(Loader.Load(Context, Diagnostic));
        if (!Resource || Resource->Pixels.empty() || Resource->ChannelCount != 4)
        {
            return {};
        }
        return QImage(Resource->Pixels.data(), Resource->Width, Resource->Height,
            Resource->RowStride, QImage::Format_RGBA8888).scaled(512, 512, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    static QColor Sample(const QImage& TextureImage, float Horizontal, float Vertical, const QColor& Tint)
    {
        if (TextureImage.isNull())
        {
            return Tint;
        }
        const int Column = std::clamp(static_cast<int>((Horizontal - std::floor(Horizontal)) * TextureImage.width()), 0, TextureImage.width() - 1);
        const int Row = std::clamp(static_cast<int>((Vertical - std::floor(Vertical)) * TextureImage.height()), 0, TextureImage.height() - 1);
        const QColor Pixel = TextureImage.pixelColor(Column, Row);
        return QColor::fromRgbF(Pixel.redF() * Tint.redF(), Pixel.greenF() * Tint.greenF(), Pixel.blueF() * Tint.blueF());
    }

    static QColor Shade(const QColor& Color, float Light)
    {
        return QColor::fromRgbF(std::clamp(static_cast<float>(Color.redF()) * Light, 0.0f, 1.0f),
            std::clamp(static_cast<float>(Color.greenF()) * Light, 0.0f, 1.0f), std::clamp(static_cast<float>(Color.blueF()) * Light, 0.0f, 1.0f));
    }

    static QImage Mesh(const StaticMeshResource& Resource)
    {
        if (Resource.Vertices.empty() || Resource.Indices.empty())
        {
            return {};
        }
        std::vector<DirectX::SimpleMath::Vector3> Projected;
        Projected.reserve(Resource.Vertices.size());
        float MinimumHorizontal = std::numeric_limits<float>::max();
        float MaximumHorizontal = std::numeric_limits<float>::lowest();
        float MinimumVertical = MinimumHorizontal;
        float MaximumVertical = MaximumHorizontal;
        for (const auto& Vertex : Resource.Vertices)
        {
            const auto& Position = Vertex.Position;
            DirectX::SimpleMath::Vector3 View(Position.x * 0.8f - Position.z * 0.6f,
                Position.y * 0.9f - (Position.x * 0.6f + Position.z * 0.8f) * 0.43589f,
                Position.y * 0.43589f + (Position.x * 0.6f + Position.z * 0.8f) * 0.9f);
            Projected.push_back(View);
            MinimumHorizontal = std::min(MinimumHorizontal, View.x);
            MaximumHorizontal = std::max(MaximumHorizontal, View.x);
            MinimumVertical = std::min(MinimumVertical, View.y);
            MaximumVertical = std::max(MaximumVertical, View.y);
        }
        const float Scale = 106.0f / std::max({MaximumHorizontal - MinimumHorizontal, MaximumVertical - MinimumVertical, 0.0001f});
        for (auto& Position : Projected)
        {
            Position.x = 64.0f + (Position.x - (MinimumHorizontal + MaximumHorizontal) * 0.5f) * Scale;
            Position.y = 64.0f - (Position.y - (MinimumVertical + MaximumVertical) * 0.5f) * Scale;
        }
        QImage Image = Background();
        std::vector<float> Depth(128 * 128, std::numeric_limits<float>::lowest());
        for (size_t Triangle = 0; Triangle + 2 < Resource.Indices.size(); Triangle += 3)
        {
            const auto FirstIndex = Resource.Indices[Triangle];
            const auto SecondIndex = Resource.Indices[Triangle + 1];
            const auto ThirdIndex = Resource.Indices[Triangle + 2];
            if (FirstIndex >= Projected.size() || SecondIndex >= Projected.size() || ThirdIndex >= Projected.size())
            {
                continue;
            }
            const auto& First = Projected[FirstIndex];
            const auto& Second = Projected[SecondIndex];
            const auto& Third = Projected[ThirdIndex];
            const float Area = (Second.y - Third.y) * (First.x - Third.x) + (Third.x - Second.x) * (First.y - Third.y);
            if (std::abs(Area) < 0.0001f)
            {
                continue;
            }
            auto Normal = (Resource.Vertices[SecondIndex].Position - Resource.Vertices[FirstIndex].Position).Cross(
                Resource.Vertices[ThirdIndex].Position - Resource.Vertices[FirstIndex].Position);
            Normal.Normalize();
            const QColor Color = Shade(QColor(180, 188, 205), 0.35f + 0.65f * std::abs(Normal.Dot({0.3f, 0.8f, 0.5f})));
            for (int Row = std::max(0, static_cast<int>(std::floor(std::min({First.y, Second.y, Third.y}))));
                Row <= std::min(127, static_cast<int>(std::ceil(std::max({First.y, Second.y, Third.y})))); ++Row)
            {
                for (int Column = std::max(0, static_cast<int>(std::floor(std::min({First.x, Second.x, Third.x}))));
                    Column <= std::min(127, static_cast<int>(std::ceil(std::max({First.x, Second.x, Third.x})))); ++Column)
                {
                    const float FirstWeight = ((Second.y - Third.y) * (Column + 0.5f - Third.x) + (Third.x - Second.x) * (Row + 0.5f - Third.y)) / Area;
                    const float SecondWeight = ((Third.y - First.y) * (Column + 0.5f - Third.x) + (First.x - Third.x) * (Row + 0.5f - Third.y)) / Area;
                    const float ThirdWeight = 1.0f - FirstWeight - SecondWeight;
                    const float Distance = FirstWeight * First.z + SecondWeight * Second.z + ThirdWeight * Third.z;
                    if (FirstWeight >= 0 && SecondWeight >= 0 && ThirdWeight >= 0 && Distance > Depth[Row * 128 + Column])
                    {
                        Depth[Row * 128 + Column] = Distance;
                        Image.setPixelColor(Column, Row, Color);
                    }
                }
            }
        }
        return Image;
    }

    static QImage Generate(const AssetRegistry& Registry, const AssetKey& Key, int Depth = 0)
    {
        if (Depth > 4)
        {
            return {};
        }
        AssetLoadContext Context;
        const SubAssetRecord* SubAsset = nullptr;
        if (!Registry.TryResolveKey(Key, Context.Entry, &SubAsset))
        {
            return {};
        }
        Context.Registry = &Registry;
        Context.Key = Key;
        BindLoadContextSubAsset(Context, SubAsset);
        AssetType Type = Context.Entry.Metadata.Type;
        if (SubAsset)
        {
            Type = SubAsset->Type;
        }
        if (Type == TextureAssetType)
        {
            return Texture(Context);
        }
        if (Type == CharacterAssetType)
        {
            CharacterDefinition Definition;
            std::string Error;
            if (CharacterDefinition::Read(Context.Entry.AbsolutePath, Definition, Error))
            {
                return Generate(Registry, Definition.Model, Depth + 1);
            }
        }
        if (Type == StaticMeshAssetType || Type == ModelAssetType || Type == SkeletalMeshAssetType)
        {
            ModelLoader Loader;
            AssetDiagnostic Diagnostic;
            if (Type == StaticMeshAssetType)
            {
                const auto Resource = std::static_pointer_cast<const StaticMeshResource>(Loader.Load(Context, Diagnostic));
                if (Resource)
                {
                    return Mesh(*Resource);
                }
            }
            else
            {
                const auto Document = Loader.LoadOrGetSharedDocument(Context.Entry.AbsolutePath, Context.Entry.Metadata.SourceFingerprint, Diagnostic);
                if (Document && !Document->Meshes.empty())
                {
                    int Index = 0;
                    if (SubAsset && SubAsset->Selector.Index >= 0)
                    {
                        Index = SubAsset->Selector.Index;
                    }
                    if (Index < static_cast<int>(Document->Meshes.size()) && Document->Meshes[Index].Mesh)
                    {
                        return Mesh(*Document->Meshes[Index].Mesh);
                    }
                }
            }
        }
        if (Type == MaterialAssetType || Type == MaterialInstanceAssetType)
        {
            ResolvedMaterial Material;
            if (MaterialDocumentIO::Resolve(Registry, Key, Material).HasError())
            {
                return {};
            }
            QColor Tint(190, 190, 190);
            const auto BaseColor = Material.Values.find("BaseColor");
            if (BaseColor != Material.Values.end())
            {
                const auto& Numbers = BaseColor->second.Numbers;
                Tint = QColor::fromRgbF(std::clamp(Numbers.x, 0.0f, 1.0f), std::clamp(Numbers.y, 0.0f, 1.0f), std::clamp(Numbers.z, 0.0f, 1.0f));
            }
            QImage TextureImage;
            const auto BaseTexture = Material.Values.find("BaseColorTexture");
            if (BaseTexture != Material.Values.end() && BaseTexture->second.Texture.IsValid())
            {
                TextureImage = Generate(Registry, BaseTexture->second.Texture, Depth + 1);
            }
            QImage Image = Background();
            for (int Row = 10; Row < 118; ++Row)
            {
                for (int Column = 10; Column < 118; ++Column)
                {
                    const float Horizontal = (Column - 63.5f) / 53.0f;
                    const float Vertical = (63.5f - Row) / 53.0f;
                    const float RadiusSquared = Horizontal * Horizontal + Vertical * Vertical;
                    if (RadiusSquared <= 1)
                    {
                        const float Forward = std::sqrt(1.0f - RadiusSquared);
                        const QColor Color = Sample(TextureImage, 0.5f + std::atan2(Horizontal, Forward) / 6.2831853f,
                            0.5f - std::asin(Vertical) / 3.1415927f, Tint);
                        Image.setPixelColor(Column, Row, Shade(Color, 0.25f + 0.75f * std::max(0.0f, -0.4f * Horizontal + 0.6f * Vertical + 0.69f * Forward)));
                    }
                }
            }
            return Image;
        }
        return {};
    }
};

QImage GenerateAssetPreview(const AssetRegistry& Registry, const AssetKey& Key)
{
    return AssetPreviewGenerator::Generate(Registry, Key);
}
