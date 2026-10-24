#include "Assets/AssetPreview.h"
#include "Assets/AssetRegistry.h"
#include "Assets/Loaders/ModelLoader.h"
#include "Assets/Loaders/TextureLoader.h"
#include "Assets/Resources/StaticMeshResource.h"
#include "Assets/Resources/TextureResource.h"
#include "Characters/CharacterDefinition.h"
#include "Materials/MaterialDocumentIO.h"

#include <algorithm>
#include <cmath>
#include <limits>

class AssetPreviewGenerator
{
public:
    static void SetPixel(ImageBuffer& Image, int Column, int Row, const DirectX::SimpleMath::Vector3& Color)
    {
        const size_t Offset = (static_cast<size_t>(Row) * Image.Width + Column) * 4;
        Image.Pixels[Offset] = static_cast<uint8_t>(std::clamp(Color.x, 0.f, 1.f) * 255.f);
        Image.Pixels[Offset + 1] = static_cast<uint8_t>(std::clamp(Color.y, 0.f, 1.f) * 255.f);
        Image.Pixels[Offset + 2] = static_cast<uint8_t>(std::clamp(Color.z, 0.f, 1.f) * 255.f);
        Image.Pixels[Offset + 3] = 255;
    }

    static DirectX::SimpleMath::Vector3 GetPixel(const ImageBuffer& Image, int Column, int Row)
    {
        const size_t Offset = (static_cast<size_t>(Row) * Image.Width + Column) * 4;
        return {Image.Pixels[Offset] / 255.f, Image.Pixels[Offset + 1] / 255.f, Image.Pixels[Offset + 2] / 255.f};
    }
    static ImageBuffer Background()
    {
        ImageBuffer Image{128, 128, std::vector<uint8_t>(128 * 128 * 4)};
        for (int Row = 0; Row < 128; ++Row)
        {
            for (int Column = 0; Column < 128; ++Column)
            {
                SetPixel(Image, Column, Row, {30.f / 255.f, 31.f / 255.f, 39.f / 255.f});
            }
        }
        return Image;
    }

    static ImageBuffer Texture(const AssetLoadContext& Context)
    {
        AssetDiagnostic Diagnostic;
        TextureLoader Loader;
        const auto Resource = std::static_pointer_cast<const TextureResource>(Loader.Load(Context, Diagnostic));
        if (!Resource || Resource->Pixels.empty() || Resource->ChannelCount != 4)
        {
            return {};
        }
        return ImageBuffer::FromRgba(Resource->Pixels.data(), Resource->Width, Resource->Height,
            Resource->RowStride).ResizeToFit(512, 512);
    }

    static DirectX::SimpleMath::Vector3 Sample(const ImageBuffer& TextureImage, float Horizontal, float Vertical, const DirectX::SimpleMath::Vector3& Tint)
    {
        if (TextureImage.Pixels.empty())
        {
            return Tint;
        }
        const int Column = std::clamp(static_cast<int>((Horizontal - std::floor(Horizontal)) * TextureImage.Width), 0, TextureImage.Width - 1);
        const int Row = std::clamp(static_cast<int>((Vertical - std::floor(Vertical)) * TextureImage.Height), 0, TextureImage.Height - 1);
        const DirectX::SimpleMath::Vector3 Pixel = GetPixel(TextureImage, Column, Row);
        return DirectX::SimpleMath::Vector3(Pixel.x * Tint.x, Pixel.y * Tint.y, Pixel.z * Tint.z);
    }

    static DirectX::SimpleMath::Vector3 Shade(const DirectX::SimpleMath::Vector3& Color, float Light)
    {
        return DirectX::SimpleMath::Vector3(std::clamp(static_cast<float>(Color.x) * Light, 0.0f, 1.0f),
            std::clamp(static_cast<float>(Color.y) * Light, 0.0f, 1.0f), std::clamp(static_cast<float>(Color.z) * Light, 0.0f, 1.0f));
    }

    static ImageBuffer Mesh(const StaticMeshResource& Resource)
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
        ImageBuffer Image = Background();
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
            const DirectX::SimpleMath::Vector3 Color = Shade(DirectX::SimpleMath::Vector3(180.f / 255.f, 188.f / 255.f, 205.f / 255.f), 0.35f + 0.65f * std::abs(Normal.Dot({0.3f, 0.8f, 0.5f})));
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
                        SetPixel(Image, Column, Row, Color);
                    }
                }
            }
        }
        return Image;
    }

    static ImageBuffer Generate(const AssetRegistry& Registry, const AssetKey& Key, int Depth = 0)
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
            DirectX::SimpleMath::Vector3 Tint(190.f / 255.f, 190.f / 255.f, 190.f / 255.f);
            const auto BaseColor = Material.Values.find("BaseColor");
            if (BaseColor != Material.Values.end())
            {
                const auto& Numbers = BaseColor->second.Numbers;
                Tint = DirectX::SimpleMath::Vector3(std::clamp(Numbers.x, 0.0f, 1.0f), std::clamp(Numbers.y, 0.0f, 1.0f), std::clamp(Numbers.z, 0.0f, 1.0f));
            }
            ImageBuffer TextureImage;
            const auto BaseTexture = Material.Values.find("BaseColorTexture");
            if (BaseTexture != Material.Values.end() && BaseTexture->second.Texture.IsValid())
            {
                TextureImage = Generate(Registry, BaseTexture->second.Texture, Depth + 1);
            }
            ImageBuffer Image = Background();
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
                        const DirectX::SimpleMath::Vector3 Color = Sample(TextureImage, 0.5f + std::atan2(Horizontal, Forward) / 6.2831853f,
                            0.5f - std::asin(Vertical) / 3.1415927f, Tint);
                        SetPixel(Image, Column, Row, Shade(Color, 0.25f + 0.75f * std::max(0.0f, -0.4f * Horizontal + 0.6f * Vertical + 0.69f * Forward)));
                    }
                }
            }
            return Image;
        }
        return {};
    }
};

ImageBuffer GenerateAssetPreview(const AssetRegistry& Registry, const AssetKey& Key)
{
    return AssetPreviewGenerator::Generate(Registry, Key);
}
