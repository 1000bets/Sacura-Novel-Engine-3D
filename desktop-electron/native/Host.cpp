// Experimental Windows host. Owns only the child HWND and app loop, not a renderer.
#include "Core/EnginePaths.h"
#include "Core/Threading/ThreadContext.h"
#include "Engine.h"
#include "Project/ProjectSession.h"
#include "World/Scene.h"
#include "World/GameObject.h"
#include "World/Components/MeshRendererComponent.h"
#include <QApplication>
#include <QElapsedTimer>
#include <QTimer>
#include <QImage>
#include <nlohmann/json.hpp>
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <filesystem>
#include "FramePipe.h"
#include "ViewportController.h"
#include "NativeBridge.h"
#include "ControlPipe.h"
#include "ProjectCli.h"

using Json = nlohmann::json;
namespace {
void Emit(const Json& Value)
{
    // One pipe write: Engine/Diligent logs originate on other threads. Multiple
    // ostream insertions could interleave a log in the middle of a JSON record.
    const std::string Packet = "\n@sacura " + Value.dump() + "\n";
    DWORD Written = 0;
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), Packet.data(), static_cast<DWORD>(Packet.size()), &Written, nullptr);
}
LRESULT CALLBACK WindowProc(HWND Window, UINT Message, WPARAM W, LPARAM L)
{
    switch (Message) {
    case WM_ERASEBKGND: return 1;
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    }
    return DefWindowProcW(Window, Message, W, L);
}
// Diagnostic screenshot only. This is never used to deliver viewport frames.
bool Capture(HWND Parent, const std::string& Path)
{
    RECT Rect{}; GetClientRect(Parent, &Rect);
    const int Width = Rect.right, Height = Rect.bottom;
    POINT Origin{}; ClientToScreen(Parent, &Origin);
    HDC Source = GetDC(nullptr), Destination = CreateCompatibleDC(Source);
    BITMAPINFO Info{}; Info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    Info.bmiHeader.biWidth = Width; Info.bmiHeader.biHeight = -Height;
    Info.bmiHeader.biPlanes = 1; Info.bmiHeader.biBitCount = 32; Info.bmiHeader.biCompression = BI_RGB;
    void* Pixels = nullptr;
    HBITMAP Bitmap = CreateDIBSection(Source, &Info, DIB_RGB_COLORS, &Pixels, nullptr, 0);
    if (!Bitmap) { DeleteDC(Destination); ReleaseDC(nullptr, Source); return false; }
    const auto Old = SelectObject(Destination, Bitmap);
    const bool Copied = BitBlt(Destination, 0, 0, Width, Height, Source, Origin.x, Origin.y, SRCCOPY | CAPTUREBLT) != 0;
    const bool Saved = Copied && QImage(static_cast<uchar*>(Pixels), Width, Height, QImage::Format_RGB32)
        .save(QString::fromUtf8(Path.c_str()));
    SelectObject(Destination, Old); DeleteObject(Bitmap); DeleteDC(Destination); ReleaseDC(nullptr, Source);
    return Saved;
}
}

int main(int Argc, char** Argv)
{
    const auto PythonHome = std::filesystem::absolute(Argv[0]).parent_path() / "Python";
    if (std::filesystem::exists(PythonHome))
        qputenv("PYTHONHOME", QString::fromStdWString(PythonHome.wstring()).toUtf8());
    // Match Chromium per-monitor DPI before QApplication or any HWND creation.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    QApplication App(Argc, Argv);
    if(qEnvironmentVariableIsSet("SACURA_NATIVE_DIAGNOSTIC")) std::cout<<std::unitbuf;
    App.setQuitOnLastWindowClosed(false);
    const int ProjectResult=RunProjectCli(App.arguments());
    if(ProjectResult>=0) return ProjectResult;
    const bool Headless = Argc == 4 && std::string(Argv[1]) == "--bridge-headless";
    if (!Headless && Argc != 3 && Argc != 5 && Argc != 6) { std::cerr << "Usage: SakuraElectronHost <parent HWND decimal> <project.project> [frame-pipe consumer-pid]\n"; return 1; }
    std::unique_ptr<FramePipe> Frames;
    if (!Headless && Argc >= 5) Frames = std::make_unique<FramePipe>(Argv[3], static_cast<uint32_t>(std::stoul(Argv[4])));
    HWND Parent = Headless ? nullptr : reinterpret_cast<HWND>(static_cast<uintptr_t>(std::stoull(Argv[1])));
    std::unique_ptr<ControlPipe> Control;
    if(Headless || Argc==6) Control=std::make_unique<ControlPipe>(Headless ? Argv[3] : Argv[5]);
    HWND Child = nullptr;
    if (!Headless) {
    if (!IsWindow(Parent)) return 2;
    // No SetParent: create as a child from the outset, under the parent's DPI context.
    SetThreadDpiAwarenessContext(GetWindowDpiAwarenessContext(Parent));
    WNDCLASSW Class{}; Class.lpfnWndProc = WindowProc; Class.hInstance = GetModuleHandleW(nullptr);
    Class.lpszClassName = L"SacuraElectronViewport"; Class.hCursor = LoadCursor(nullptr, IDC_ARROW);
    if (!RegisterClassW(&Class)) return 3;
    SetWindowLongPtrW(Parent, GWL_STYLE, GetWindowLongPtrW(Parent, GWL_STYLE) | WS_CLIPCHILDREN);
    Child = CreateWindowExW(0, Class.lpszClassName, L"Sacura Diligent viewport",
        WS_CHILD | WS_CLIPSIBLINGS, 0, 0, 64, 64, Parent, nullptr, Class.hInstance, nullptr);
    if (!Child) { Emit({{"error", "CreateWindowEx failed"}, {"win32", GetLastError()}}); return 4; }
    }
    SetCurrentThreadRole(ThreadRole::Game);
    EnginePaths::InitializeFromExecutable(std::filesystem::absolute(Argv[0]));
    Engine Runtime;
    Runtime.SetGraphicsBackend(GraphicsBackend::D3D12);
    Runtime.InitializeHeadless({});
    ProjectSession Session; Session.BindEngine(&Runtime);
    if (!Runtime.IsInitialized() || !Session.OpenProject(std::filesystem::path(App.arguments()[2].toStdWString())) || !Runtime.GetEditScene() || Session.GetHealth()!=ProjectSessionHealth::Ready) {
        Emit({{"error", Runtime.GetInitializationError() + Session.GetLastError()}});
        Runtime.Shutdown(); DestroyWindow(Child); return 5;
    }
    if (!Headless && !Runtime.StartPresenting({Child, 64, 64})) {
        Emit({{"error", "StartPresenting failed"}}); Session.CloseProject(); Runtime.Shutdown(); DestroyWindow(Child); return 6;
    }
    if (Frames) Runtime.GetRenderThread().SetSharedFrameStream(Frames->Stream);
    auto Controller = std::make_unique<ViewportController>(Runtime, Session);
    std::unique_ptr<NativeBridge> Bridge;
    if(Control) {
        Bridge=std::make_unique<NativeBridge>(Runtime, Controller->GetWorkspace(), Session, std::filesystem::absolute(Argv[0]).parent_path()/"BridgeContract.json", [&]{Controller->ResetInput();});
        Control->Send(Bridge->Poll(true));
    }
    // Projects are loaded as authored; only the launcher creates an isolated fixture.
    ObjectHandle Animated;
    int MeshCount = 0;
    for (auto* Object : Runtime.GetEditScene()->GetAllObjects()) {
        if (auto* Mesh = Object->GetComponent<MeshRendererComponent>()) {
            if (!Animated.IsValid()) Animated = Object->GetObjectHandle();
            ++MeshCount;
        }
    }
    Emit({{"type", "ready"}, {"pid", GetCurrentProcessId()}, {"child", reinterpret_cast<uintptr_t>(Child)},
        {"parent", reinterpret_cast<uintptr_t>(GetParent(Child))}, {"isChild", bool(GetWindowLongPtrW(Child, GWL_STYLE) & WS_CHILD)},
        {"dpi", GetDpiForWindow(Child)}, {"sceneObjects", Runtime.GetEditScene()->GetObjectCount()}, {"meshes", MeshCount}});
    std::string Pending;
    HANDLE Input = GetStdHandle(STD_INPUT_HANDLE);
    QElapsedTimer Clock; Clock.start();
    QTimer Timer;
    int Width = 64, Height = 64, Ticks = 0;
    float Time = 0.f;
    bool Animate = false, Visible = false;
    QObject::connect(&Timer, &QTimer::timeout, &App, [&] {
        DWORD Available = 0;
        if (Frames && !Frames->Pump()) { App.quit(); return; }
        if (Frames && !Frames->Stream->GetError().empty()) { Emit({{"error", Frames->Stream->GetError()}}); App.exit(9); return; }
        if ((!Headless && !IsWindow(Parent)) || !PeekNamedPipe(Input, nullptr, 0, nullptr, &Available, nullptr)) { App.quit(); return; }
        if (Available) {
            char Buffer[4096]; DWORD Read = 0;
            if (!ReadFile(Input, Buffer, std::min<DWORD>(Available, sizeof Buffer), &Read, nullptr)) { App.quit(); return; }
            Pending.append(Buffer, Read);
            if (Pending.size() > 65536) { App.exit(7); return; }
        }
        if(Control) {
            try {
                std::vector<Json> Requests;
                if(!Control->Read(Requests)){App.quit();return;}
                for(const auto& Request:Requests) {
                    Animate=false;
                    auto Reply=Bridge->Execute(Request);
                    auto Event=Bridge->Poll();Reply["context"]=Bridge->Context();
                    if(!Control->Send(Reply)){App.quit();return;}
                    if(!Event.is_null())Control->Send(Event);
                }
            }catch(const std::exception& E){Emit({{"error",E.what()}});App.exit(10);return;}
        }
        size_t End;
        while ((End = Pending.find('\n')) != std::string::npos) {
            const std::string Line = Pending.substr(0, End); Pending.erase(0, End + 1);
            try {
                const auto Command = Json::parse(Line);
                const std::string Type = Command.at("type");
                if (Type == "shutdown") { App.quit(); return; }
                if (Type == "suspend") { if (Frames) Frames->Stream->Enabled = false; Visible=false; Controller->ResetInput(); Controller->Resize(0,0,1); Runtime.ResizePresentation(0, 0); }
                if (Type == "animate") {
                    Animate = Command.at("value").get<bool>();
                    if(Animate&&!Runtime.GetEditScene()->FindByHandle(Animated))
                        for(auto* Object:Runtime.GetEditScene()->GetAllObjects())
                            if(Object->GetComponent<MeshRendererComponent>()){Animated=Object->GetObjectHandle();break;}
                }
                if (Type == "input") {
                    Animate = false;
                    if (Frames) Frames->LastInputMicroseconds = Command.value("timestampUs", uint64_t{0});
                    Controller->Input(Command.at("event")); Controller->Tick(0.001f);
                }
                if (Type == "tools") Controller->SetTool(Command);
                if (Type == "capture") Emit({{"type", "capture"}, {"ok", Capture(Parent, Command.at("path"))}});
                if (Type == "bounds") {
                    const int X = Command.at("x"), Y = Command.at("y");
                    Width = std::clamp(Command.at("width").get<int>(), 0, 8192);
                    Height = std::clamp(Command.at("height").get<int>(), 0, 8192);
                    Visible = Command.value("visible", true) && Width > 0 && Height > 0;
                    Controller->Resize(Visible ? Width : 0, Visible ? Height : 0, Command.value("scale", 1.f));
                    if (!Visible) Controller->ResetInput();
                    if (!Visible) ShowWindow(Child, SW_HIDE);
                    else SetWindowPos(Child, HWND_TOP, X, Y, Width, Height, SWP_NOACTIVATE | (Frames ? 0 : SWP_SHOWWINDOW));
                    Runtime.ResizePresentation(Visible ? Width : 0, Visible ? Height : 0, Command.value("interactive", false));
                    RECT R{}; GetWindowRect(Child, &R); MapWindowPoints(nullptr, Parent, reinterpret_cast<POINT*>(&R), 2);
                    Emit({{"type", "bounds"}, {"seq", Command.value("seq", 0)}, {"x", R.left}, {"y", R.top},
                        {"width", R.right - R.left}, {"height", R.bottom - R.top}, {"visible", Visible}, {"dpi", GetDpiForWindow(Child)}});
                }
            } catch (const std::exception& Error) { Emit({{"error", Error.what()}}); App.exit(8); return; }
        }
        const float Dt = std::min(0.1f, Clock.restart() / 1000.f);
        if (Animate) Time += Dt;
        if (Animate) if (auto* Object = Runtime.GetEditScene()->FindByHandle(Animated)) { auto T = Object->GetTransform(); T.Rotation = Quaternion::CreateFromYawPitchRoll(Time * 0.6f, 0.2f, 0.f); Object->SetTransform(T); }
        Controller->Tick(Dt);
        Runtime.Tick(Dt);
        if(Bridge && Ticks % 15 == 0) {
            try {if(Ticks%120==0)Bridge->ScanAssets();auto Event=Bridge->Poll();if(!Event.is_null()&&!Control->Send(Event)){App.quit();return;}}
            catch(const std::exception& E){Emit({{"error",E.what()}});App.exit(10);return;}
        }
        if (++Ticks % 30 == 0) {
            const auto S = Runtime.GetRenderStatistics(RenderSurfaceId{1});
            int ResolvedMeshes = 0;
            for (const auto* Object : Runtime.GetEditScene()->GetAllObjects())
                if (const auto* Mesh = Object->GetComponent<MeshRendererComponent>())
                    if (Mesh->Mesh.IsValid() && !Mesh->bMissingAsset && !Mesh->bPendingAsset) ++ResolvedMeshes;
            Emit({{"type", "stats"}, {"frame", S.FrameIndex}, {"width", S.SurfaceWidth}, {"height", S.SurfaceHeight},
                {"drawCalls", S.DrawCalls}, {"triangles", S.Triangles}, {"visibleObjects", S.VisibleObjects},
                {"residentMeshes", S.ResidentMeshes}, {"resolvedMeshes", ResolvedMeshes}, {"visible", Visible}, {"animate", Animate}});
            Emit({{"type","timing"},{"cpuMs",S.CpuMilliseconds},{"gpuAvailable",S.bGpuTimingsAvailable},
                {"gpuMs",S.ShadowMilliseconds+S.OpaqueMilliseconds+S.TransparencyMilliseconds+S.PostprocessMilliseconds},
                {"residentBytes",S.ResidentBytes},{"dropped",Frames ? Frames->Stream->Dropped.load() : 0},
                {"sharedTextures",Frames ? Frames->Stream->TextureCount.load() : 0},
                {"sharedTextureBytes",Frames ? Frames->Stream->TextureBytes.load() : 0}});
            Emit({{"type", "viewport"}, {"data", Controller->Status()}});
        }
    });
    Timer.start(16);
    const int Result = App.exec();
    Timer.stop();
    Runtime.GetPlaySession().StopPlay();
    Bridge.reset();
    Controller.reset();
    // Release/join GPU work before destroying its native surface.
    Runtime.StopPresenting(); Session.CloseProject(); Runtime.Shutdown();
    DestroyWindow(Child);
    Emit({{"type", "stopped"}});
    return Result;
}
