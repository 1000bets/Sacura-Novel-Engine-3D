#pragma once
#include <array>
#include <string_view>

enum class RenderVisualization
{
    Lit,
    BaseColor,
    WorldNormal,
    Roughness,
    Metallic,
    AmbientOcclusion,
    Emissive,
    LinearDepth,
    SceneColorHdr,
    ShadowAtlas,
    TransparencyAccumulation,
    TransparencyRevealage,
    Opacity
};

struct RenderVisualizationDescriptor
{
    RenderVisualization Mode;
    const char* Identifier;
    const char* Label;
    const char* Description;
};

inline constexpr std::array<RenderVisualizationDescriptor, 13> RenderVisualizations{{
    {RenderVisualization::Lit, "lit", "Освещение", "Итоговый кадр с освещением и постобработкой"},
    {RenderVisualization::BaseColor, "base_color", "Цвет материала", "Цвет поверхности без освещения; диагностический проход forward-шейдера"},
    {RenderVisualization::WorldNormal, "world_normal", "Нормали", "Мировые нормали после normal map: RGB соответствует XYZ от −1 до 1"},
    {RenderVisualization::Roughness, "roughness", "Шероховатость", "Чёрный — 0, белый — 1; диагностический проход"},
    {RenderVisualization::Metallic, "metallic", "Металличность", "Чёрный — диэлектрик, белый — металл"},
    {RenderVisualization::AmbientOcclusion, "ambient_occlusion", "Окклюзия материала", "AO материала: чёрный — закрыто, белый — открыто; это не SSAO"},
    {RenderVisualization::Emissive, "emissive", "Свечение", "Emission материала без освещения и bloom; значения выше 1 ограничены дисплеем"},
    {RenderVisualization::LinearDepth, "linear_depth", "Глубина", "Реальный буфер глубины в линейных метрах: белый — выбранная дальность или фон"},
    {RenderVisualization::SceneColorHdr, "scene_color_hdr", "Цвет сцены HDR", "Цвет до постобработки; HDR сжат формулой C / (1 + C)"},
    {RenderVisualization::ShadowAtlas, "shadow_atlas", "Атлас теней", "Реальный атлас 4×4; нелинейная глубина, белый — пусто"},
    {RenderVisualization::TransparencyAccumulation, "transparency_accumulation", "Накопление прозрачности", "Взвешенный цвет OIT до композиции, сжатый C / (1 + C); чёрный — пусто"},
    {RenderVisualization::TransparencyRevealage, "transparency_revealage", "Пропускание прозрачности", "OIT revealage: белый — нет прозрачных поверхностей, чёрный — непрозрачно"},
    {RenderVisualization::Opacity, "opacity", "Непрозрачность материала", "Opacity материала: чёрный — 0, белый — 1; masked сохраняет вырезанные пиксели"}
}};

inline const RenderVisualizationDescriptor* FindRenderVisualization(std::string_view Identifier)
{
    for (const auto& Descriptor : RenderVisualizations)
    {
        if (Identifier == Descriptor.Identifier)
        {
            return &Descriptor;
        }
    }
    return nullptr;
}
