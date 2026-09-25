#include "Rendering/RenderView.h"
#include "Rendering/Visibility.h"

#include <cmath>
#include <iostream>

namespace
{
int Failures = 0;

void Expect(bool Condition, const char* Message)
{
    if (!Condition)
    {
        ++Failures;
        std::cout << "FAIL: " << Message << "\n";
        return;
    }
    std::cout << "PASS: " << Message << "\n";
}

bool NearlyEqual(float Left, float Right, float Epsilon = 0.01f)
{
    return std::fabs(Left - Right) <= Epsilon;
}
}

int main()
{
    RenderViewCamera Front{};
    Front.Position = Vector3(0.f, 0.f, -5.f);
    Front.Target = Vector3::Zero;
    Front.FieldOfViewDegrees = 60.f;
    Front.NearPlane = 0.1f;
    Front.FarPlane = 100.f;

    RenderViewCamera Side = Front;
    Side.Position = Vector3(5.f, 0.f, 0.f);
    Side.FieldOfViewDegrees = 35.f;
    Side.NearPlane = 0.5f;

    RenderCamera FrontCamera{};
    RenderCamera SideCamera{};
    Front.BuildRenderCamera(16.f / 9.f, FrontCamera);
    Side.BuildRenderCamera(16.f / 9.f, SideCamera);

    Expect(FrontCamera.bValid && SideCamera.bValid, "Both view cameras valid");
    Expect(FrontCamera.Position.z == -5.f, "Front camera position");
    Expect(SideCamera.Position.x == 5.f, "Side camera position");
    Expect(NearlyEqual(FrontCamera.FieldOfView, 60.f), "Front FOV");
    Expect(NearlyEqual(SideCamera.FieldOfView, 35.f), "Side FOV differs");
    Expect(NearlyEqual(SideCamera.NearPlane, 0.5f), "Side near plane differs");
    Expect(FrontCamera.ViewProjection != SideCamera.ViewProjection, "ViewProjection matrices differ");

    RenderViewId ViewA{1};
    RenderViewId ViewB{2};
    Expect(ViewA != ViewB, "View ids are independent");

    Expect(IntersectsFrustum(AxisAlignedBounds::FromCenterExtents(Vector3::Zero, Vector3(0.5f)), FrontCamera.ViewProjection),
        "Visible box survives frustum culling");
    Expect(!IntersectsFrustum(AxisAlignedBounds::FromCenterExtents(Vector3(100.f, 0.f, 0.f), Vector3(0.5f)), FrontCamera.ViewProjection),
        "Box outside side plane is culled");
    Expect(!IntersectsFrustum(AxisAlignedBounds::FromCenterExtents(Vector3(0.f, 0.f, -10.f), Vector3(0.5f)), FrontCamera.ViewProjection),
        "Box behind camera is culled");
    Expect(!IntersectsFrustum(AxisAlignedBounds::FromCenterExtents(Vector3(0.f, 0.f, 150.f), Vector3(0.5f)), FrontCamera.ViewProjection),
        "Box beyond far plane is culled");
    Expect(IntersectsFrustum(AxisAlignedBounds::FromCenterExtents(FrontCamera.Position, Vector3(2.f)), FrontCamera.ViewProjection),
        "Box crossing near plane is conservatively retained");
    Expect(IntersectsFrustum(AxisAlignedBounds::FromCenterExtents(Vector3::Zero, Vector3(200.f)), FrontCamera.ViewProjection),
        "Box containing camera frustum is retained");

    std::cout << "Failures: " << Failures << "\n";
    return Failures == 0 ? 0 : 1;
}
