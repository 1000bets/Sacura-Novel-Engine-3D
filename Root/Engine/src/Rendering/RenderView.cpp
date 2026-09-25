#include "Rendering/RenderView.h"

void RenderViewCamera::BuildRenderCamera(float AspectRatio, RenderCamera& OutCamera) const
{
    const float SafeAspect = AspectRatio > 0.001f ? AspectRatio : 1.f;
    OutCamera.Position = Position;
    OutCamera.NearPlane = NearPlane;
    OutCamera.FarPlane = FarPlane;
    OutCamera.FieldOfView = FieldOfViewDegrees;
    OutCamera.AspectRatio = SafeAspect;
    OutCamera.View = Matrix::CreateLookAt(Position, Target, Up);
    OutCamera.Projection = Matrix::CreatePerspectiveFieldOfView(
        FieldOfViewDegrees * (3.1415926535f / 180.f),
        SafeAspect,
        NearPlane,
        FarPlane);
    OutCamera.ViewProjection = OutCamera.View * OutCamera.Projection;
    OutCamera.bValid = true;
}
