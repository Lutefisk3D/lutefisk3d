//
// Copyright (c) 2017 the Urho3D project.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#include "Lutefisk3D/Input/InputEvents.h"
#include "Lutefisk3D/Input/Input.h"
#include "Lutefisk3D/Core/CoreEvents.h"
#include "Lutefisk3D/Core/Context.h"
#include "Lutefisk3D/Core/Profiler.h"
#include "Lutefisk3D/Core/Utils.h"
#include "Lutefisk3D/IO/File.h"
#include "Lutefisk3D/Engine/EngineEvents.h"
#include "Lutefisk3D/Graphics/GraphicsEvents.h"
#include "Lutefisk3D/Graphics/Graphics.h"
#include "Lutefisk3D/Resource/ResourceCache.h"
#include "SystemUI.h"
#include "Console.h"
#include <GLFW/glfw3.h>
#include <ImGuizmo/ImGuizmo.h>
#include <ImGui/imgui_internal.h>
#include <ImGui/imgui_stl.h>
#include <ImGui/imgui_freetype.h>
#include <IO/Log.h>


using namespace std::placeholders;
namespace Urho3D
{

//TODO: fix this ImGui staticlib mess
bool  (*force_reference)(const char*, std::string* , ImGuiInputTextFlags, ImGuiInputTextCallback, void*) =&ImGui::InputText;
static Vector3 systemUiScale{Vector3::ONE};
static Vector3 systemUiScalePixelPerfect{Vector3::ONE};
void(*forced_reference)(bool* p_open) = ImGui::ShowDemoWindow; // demos will need this
void OnKeyDown(int key, int scancode, unsigned buttons, int quals, bool rep)
{
    auto& io = ImGui::GetIO();
    auto down = true;
    if (key < KEY_MAX_VALUE)
        io.KeysDown[key] = down;
    QualifierFlags qual_flags(quals);

    if (qual_flags.Test(QUAL_CTRL))
        io.KeyCtrl = down;
    else if (qual_flags.Test(QUAL_SHIFT))
        io.KeyShift = down;
    else if (qual_flags.Test(QUAL_ALT))
        io.KeyAlt = down;
    else if (qual_flags.Test(QUAL_SUPER))
        io.KeySuper = down;
}
void OnKeyUp(int key, int scancode, unsigned buttons, int quals)
{
    auto& io = ImGui::GetIO();
    auto down = false;
    if (key < KEY_MAX_VALUE)
        io.KeysDown[key] = down;
    QualifierFlags qual_flags(quals);

    if (qual_flags.Test(QUAL_CTRL))
        io.KeyCtrl = down;
    else if (qual_flags.Test(QUAL_SHIFT))
        io.KeyShift = down;
    else if (qual_flags.Test(QUAL_ALT))
        io.KeyAlt = down;
    else if (qual_flags.Test(QUAL_SUPER))
        io.KeySuper = down;
}
SystemUI::SystemUI(Urho3D::Context* context)
    : Object(context)
    , vertexBuffer_(context)
    , indexBuffer_(context)
{
    imContext_ = ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.KeyMap[ImGuiKey_Tab] = KEY_TAB;
    io.KeyMap[ImGuiKey_LeftArrow] = KEY_LEFT;
    io.KeyMap[ImGuiKey_RightArrow] = KEY_RIGHT;
    io.KeyMap[ImGuiKey_UpArrow] = KEY_UP;
    io.KeyMap[ImGuiKey_DownArrow] = KEY_DOWN;
    io.KeyMap[ImGuiKey_Home] = KEY_HOME;
    io.KeyMap[ImGuiKey_End] = KEY_END;
    io.KeyMap[ImGuiKey_Delete] = KEY_DELETE;
    io.KeyMap[ImGuiKey_Backspace] = KEY_BACKSPACE;
    io.KeyMap[ImGuiKey_Enter] = KEY_ENTER;
    io.KeyMap[ImGuiKey_Escape] = KEY_ESCAPE;
    io.KeyMap[ImGuiKey_A] = KEY_A;
    io.KeyMap[ImGuiKey_C] = KEY_C;
    io.KeyMap[ImGuiKey_V] = KEY_V;
    io.KeyMap[ImGuiKey_X] = KEY_X;
    io.KeyMap[ImGuiKey_Y] = KEY_Y;
    io.KeyMap[ImGuiKey_Z] = KEY_Z;
    io.KeyMap[ImGuiKey_PageUp] = KEY_PAGE_UP;
    io.KeyMap[ImGuiKey_PageDown] = KEY_DOWN;

    io.SetClipboardTextFn = [](void* userData, const char* text) {
        GLFWwindow *win = ((SystemUI *)userData)->GetContext()->m_Graphics->GetWindow();
        glfwSetClipboardString(win,text);
    };
    io.GetClipboardTextFn = [](void* userData) -> const char* {
        GLFWwindow *win = ((SystemUI *)userData)->GetContext()->m_Graphics->GetWindow();
        return glfwGetClipboardString(win); };

    io.UserData = this;

    SetScale(Vector3::ZERO, false);
    g_engineSignals.applicationStarted.Connect(this,&SystemUI::OnAppStarted);
    g_inputSignals.keyDown.Connect(OnKeyDown);
    g_inputSignals.keyUp.Connect(OnKeyUp);
    g_inputSignals.mouseButtonDown.Connect([](MouseButton b, unsigned, int) {
            auto& io = ImGui::GetIO();
            uint32_t buttons = (uint32_t)b;
            uint32_t shift=0;
            while(buttons && shift<5)
            {
                if(buttons&1)
                    io.MouseDown[shift] = true;
                buttons>>=1;
                shift++;
            }
    });
    g_inputSignals.mouseButtonUp.Connect([](MouseButton b, unsigned, int) {
            auto& io = ImGui::GetIO();
            uint32_t buttons = (uint32_t)b;
            uint32_t shift=0;
            while(buttons && shift<5)
            {
                if(buttons&1)
                    io.MouseDown[shift] = false;
                buttons>>=1;
                shift++;
            }
    });
    g_inputSignals.mouseWheel.Connect([](int w, unsigned, int) {
            auto& io = ImGui::GetIO();
            io.MouseWheel = w;
    });
    g_inputSignals.mouseMove.Connect(this,&SystemUI::OnMouseMove);
    // Subscribe to events
    g_inputSignals.textInput.Connect([](const QString &text) {
        ImGui::GetIO().AddInputCharactersUTF8(text.toUtf8().data());
    });
    g_graphicsSignals.newScreenMode.Connect(this,&SystemUI::UpdateProjectionMatrix);
    g_inputSignals.inputEnd.Connect(this,&SystemUI::OnInputEnd);
    g_graphicsSignals.endRendering.Connect(this,&SystemUI::OnRenderEnd);
}
SystemUI::~SystemUI()
{
    ImGui::EndFrame();
    ImGui::Shutdown(imContext_);
    ImGui::DestroyContext(imContext_);
}
void SystemUI::OnRenderEnd()
{
    URHO3D_PROFILE(SystemUiRender);
    ImGui::Render();
    OnRenderDrawLists(ImGui::GetDrawData());
}
void SystemUI::OnInputEnd()
{
    float timeStep = context_->m_TimeSystem->GetTimeStep();
    ImGui::GetIO().DeltaTime = timeStep > 0.0f ? timeStep : 1.0f / 60.0f;
    ImGui::NewFrame();
    ImGuizmo::BeginFrame();
}
void SystemUI::OnAppStarted()
{
    ImGuiIO& io = ImGui::GetIO();
    if (io.Fonts->Fonts.empty())
    {
        io.Fonts->AddFontDefault();
        ReallocateFontTexture();
    }

    UpdateProjectionMatrix(context_->m_Graphics->GetWidth(),context_->m_Graphics->GetHeight());
    // Initializes ImGui. ImGui::Render() can not be called unless imgui is initialized. This call avoids initialization
    // check on every frame in E_ENDRENDERING.
    ImGui::NewFrame();
    ImGui::EndFrame();
    g_engineSignals.applicationStarted.Disconnect(this,&SystemUI::OnAppStarted);

}
void SystemUI::UpdateProjectionMatrix(int width,int height,bool,bool,bool,bool highdpi,int,int)
{
    // Update screen size
    auto graphics = context_->m_Graphics.get();
    ImGui::GetIO().DisplaySize = ImVec2(width, height);

    // Update projection matrix
    IntVector2 viewSize = graphics->GetViewport().Size();
    Vector2 invScreenSize(1.0f / viewSize.x_, 1.0f / viewSize.y_);
    Vector2 scale(2.0f * invScreenSize.x_, -2.0f * invScreenSize.y_);
    Vector2 offset(-1.0f, 1.0f);

    projection_ = Matrix4(Matrix4::IDENTITY);
    projection_.m00_ = scale.x_ * uiZoom_;
    projection_.m03_ = offset.x_;
    projection_.m11_ = scale.y_ * uiZoom_;
    projection_.m13_ = offset.y_;
    projection_.m22_ = 1.0f;
    projection_.m23_ = 0.0f;
    projection_.m33_ = 1.0f;
}
void SystemUI::OnMouseMove(int x, int y, int dx, int dy, unsigned, int)
{
    auto& io = ImGui::GetIO();
    io.MousePos.x = x / uiZoom_;
    io.MousePos.y = y / uiZoom_;

}

void SystemUI::OnRenderDrawLists(ImDrawData* data)
{
    auto graphics = context_->m_Graphics.get();
    // Engine does not render when window is closed or device is lost
    assert(graphics && graphics->IsInitialized() && !graphics->IsDeviceLost());

    for (int n = 0; n < data->CmdListsCount; n++)
    {
        const ImDrawList* cmdList = data->CmdLists[n];
        unsigned int idxBufferOffset = 0;

        // Resize vertex and index buffers on the fly. Once buffer becomes too small for data that is to be rendered
        // we reallocate buffer to be twice as big as we need now. This is done in order to minimize memory reallocation
        // in rendering loop.
        if (cmdList->VtxBuffer.Size > vertexBuffer_.GetVertexCount())
        {
            std::vector<VertexElement> elems = {VertexElement(TYPE_VECTOR2, SEM_POSITION),
                                              VertexElement(TYPE_VECTOR2, SEM_TEXCOORD),
                                              VertexElement(TYPE_UBYTE4_NORM, SEM_COLOR)
            };
            vertexBuffer_.SetSize((unsigned int)(cmdList->VtxBuffer.Size * 2), elems, true);
        }
        if (cmdList->IdxBuffer.Size > indexBuffer_.GetIndexCount())
            indexBuffer_.SetSize((unsigned int)(cmdList->IdxBuffer.Size * 2), false, true);

        vertexBuffer_.SetDataRange(cmdList->VtxBuffer.Data, 0, (unsigned int)cmdList->VtxBuffer.Size, true);
        indexBuffer_.SetDataRange(cmdList->IdxBuffer.Data, 0, (unsigned int)cmdList->IdxBuffer.Size, true);

        graphics->ClearParameterSources();
        graphics->SetColorWrite(true);
        graphics->SetCullMode(CULL_NONE);
        graphics->SetDepthTest(CMP_ALWAYS);
        graphics->SetDepthWrite(false);
        graphics->SetFillMode(FILL_SOLID);
        graphics->SetStencilTest(false);
        graphics->SetVertexBuffer(&vertexBuffer_);
        graphics->SetIndexBuffer(&indexBuffer_);

        for (const ImDrawCmd* cmd = cmdList->CmdBuffer.begin(); cmd != cmdList->CmdBuffer.end(); cmd++)
        {
            if (cmd->UserCallback)
                cmd->UserCallback(cmdList, cmd);
            else
            {
                ShaderVariation* ps;
                ShaderVariation* vs;

                auto* texture = static_cast<Texture2D*>(cmd->TextureId);
                if (!texture)
                {
                    ps = graphics->GetShader(PS, "Basic", "VERTEXCOLOR");
                    vs = graphics->GetShader(VS, "Basic", "VERTEXCOLOR");
                }
                else
                {
                    // If texture contains only an alpha channel, use alpha shader (for fonts)
                    vs = graphics->GetShader(VS, "Basic", "DIFFMAP VERTEXCOLOR");
                    if (texture->GetFormat() == Graphics::GetAlphaFormat())
                        ps = graphics->GetShader(PS, "Basic", "ALPHAMAP VERTEXCOLOR");
                    else
                        ps = graphics->GetShader(PS, "Basic", "DIFFMAP VERTEXCOLOR");
                }

                graphics->SetShaders(vs, ps);
                if (graphics->NeedParameterUpdate(SP_OBJECT, this))
                    graphics->SetShaderParameter(VSP_MODEL, Matrix3x4::IDENTITY);
                if (graphics->NeedParameterUpdate(SP_CAMERA, this))
                    graphics->SetShaderParameter(VSP_VIEWPROJ, projection_);
                if (graphics->NeedParameterUpdate(SP_MATERIAL, this))
                    graphics->SetShaderParameter(PSP_MATDIFFCOLOR, Color(1.0f, 1.0f, 1.0f, 1.0f));

                float elapsedTime = context_->m_TimeSystem->GetElapsedTime();
                graphics->SetShaderParameter(VSP_ELAPSEDTIME, elapsedTime);
                graphics->SetShaderParameter(PSP_ELAPSEDTIME, elapsedTime);

                IntRect scissor = IntRect(int(cmd->ClipRect.x * uiZoom_), int(cmd->ClipRect.y * uiZoom_),
                                          int(cmd->ClipRect.z * uiZoom_), int(cmd->ClipRect.w * uiZoom_));

                graphics->SetBlendMode(BLEND_ALPHA);
                graphics->SetScissorTest(true, scissor);
                graphics->SetTexture(0, texture);
                graphics->Draw(TRIANGLE_LIST, idxBufferOffset, cmd->ElemCount, 0, 0,
                                vertexBuffer_.GetVertexCount());
                idxBufferOffset += cmd->ElemCount;
            }
        }
    }
    graphics->SetScissorTest(false);
}

ImFont* SystemUI::AddFont(const QString& fontPath, const std::vector<unsigned short>& ranges, float size, bool merge)
{
    if (!ranges.empty() && ranges.back() != 0)
    {
        URHO3D_LOGWARNING("SystemUI: List of font ranges must be terminated with a zero.");
        return nullptr;
    }

    auto io = ImGui::GetIO();

    fontSizes_.push_back(size);

    if (size == 0.0f)
    {
        if (io.Fonts->Fonts.empty())
            size = SYSTEMUI_DEFAULT_FONT_SIZE * fontScale_;
        else
            size = io.Fonts->Fonts.back()->FontSize;
    }
    else
        size *= fontScale_;
    auto fontFile = context_->m_ResourceCache->GetFile(fontPath);
    if (!fontFile)
        return nullptr;

    std::vector<uint8_t> data;
    data.resize(fontFile->GetSize());
    auto bytesLen = fontFile->Read(&data.front(), data.size());
    ImFontConfig cfg;
    cfg.MergeMode = merge;
    cfg.FontDataOwnedByAtlas = false;
    cfg.PixelSnapH = true;
    if (auto newFont = ImGui::GetIO().Fonts->AddFontFromMemoryTTF(&data.front(), bytesLen, size, &cfg,
        ranges.empty() ? nullptr : &ranges.front()))
    {
        ReallocateFontTexture();
        return newFont;
    }
    return nullptr;
}

void SystemUI::ReallocateFontTexture()
{
    auto io = ImGui::GetIO();
    // Create font texture.
    unsigned char* pixels;
    int width, height;

    ImGuiFreeType::BuildFontAtlas(io.Fonts, ImGuiFreeType::ForceAutoHint);
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    if (fontTexture_.Null())
    {
        fontTexture_ = new Texture2D(context_);
        fontTexture_->SetNumLevels(1);
        fontTexture_->SetFilterMode(FILTER_BILINEAR);
    }

    if (fontTexture_->GetWidth() != width || fontTexture_->GetHeight() != height)
        fontTexture_->SetSize(width, height, Graphics::GetRGBAFormat());

    fontTexture_->SetData(0, 0, 0, width, height, pixels);

    // Store our identifier
    io.Fonts->TexID = (void*)fontTexture_.Get();
    io.Fonts->ClearTexData();
}

void SystemUI::SetZoom(float zoom)
{
    if (uiZoom_ == zoom)
        return;
    uiZoom_ = zoom;
    UpdateProjectionMatrix(context_->m_Graphics->GetWidth(),context_->m_Graphics->GetHeight());
}

void SystemUI::SetScale(Vector3 scale, bool pixelPerfect)
{
    auto& io = ui::GetIO();
    auto& style = ui::GetStyle();

    if (scale == Vector3::ZERO)
        scale = context_->m_Graphics->GetDisplayDPI() / 96.f;

    if (scale == Vector3::ZERO)
    {
        URHO3D_LOGWARNING("SystemUI failed to set font scaling, DPI unknown.");
        return;
    }

    systemUiScalePixelPerfect = {
        static_cast<float>(ClosestPowerOfTwo(static_cast<unsigned>(scale.x_))),
        static_cast<float>(ClosestPowerOfTwo(static_cast<unsigned>(scale.y_))),
        static_cast<float>(ClosestPowerOfTwo(static_cast<unsigned>(scale.z_)))
    };

    if (pixelPerfect)
        scale = systemUiScalePixelPerfect;

    systemUiScale = scale;

    io.DisplayFramebufferScale = {scale.x_, scale.y_};
    fontScale_ = scale.z_;

    float prevSize = SYSTEMUI_DEFAULT_FONT_SIZE;
    for (auto i = 0; i < io.Fonts->Fonts.size(); i++)
    {
        float sizePixels = fontSizes_[i];
        if (sizePixels == 0)
            sizePixels = prevSize;
        io.Fonts->ConfigData[i].SizePixels = sizePixels * fontScale_;
    }

    if (!io.Fonts->Fonts.empty())
        ReallocateFontTexture();
}

void SystemUI::ApplyStyleDefault(bool darkStyle, float alpha)
{
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScrollbarSize = 10.f;
    if (darkStyle)
        ui::StyleColorsDark(&style);
    else
        ui::StyleColorsLight(&style);
    style.Alpha = 1.0f;
    style.FrameRounding = 3.0f;
    style.ScaleAllSizes(GetFontScale());
}

bool SystemUI::IsAnyItemActive() const
{
    return ui::IsAnyItemActive();
}

bool SystemUI::IsAnyItemHovered() const
{
    return ui::IsAnyItemHovered() || ui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow);
}

int ToImGui(MouseButton button)
{
    switch (button)
    {
    case MOUSEB_LEFT:
        return 0;
    case MOUSEB_MIDDLE:
        return 2;
    case MOUSEB_RIGHT:
        return 1;
    case MOUSEB_X1:
        return 3;
    case MOUSEB_X2:
        return 4;
    default:
        return -1;
    }
}

}

bool ImGui::IsMouseDown(Urho3D::MouseButton button)
{
    return ImGui::IsMouseDown(Urho3D::ToImGui(button));
}

bool ImGui::IsMouseDoubleClicked(Urho3D::MouseButton button)
{
    return ImGui::IsMouseDoubleClicked(Urho3D::ToImGui(button));
}

bool ImGui::IsMouseDragging(Urho3D::MouseButton button, float lock_threshold)
{
    return ImGui::IsMouseDragging(Urho3D::ToImGui(button), lock_threshold);
}

bool ImGui::IsMouseReleased(Urho3D::MouseButton button)
{
    return ImGui::IsMouseReleased(Urho3D::ToImGui(button));
}

bool ImGui::IsMouseClicked(Urho3D::MouseButton button, bool repeat)
{
    return ImGui::IsMouseClicked(Urho3D::ToImGui(button), repeat);
}

bool ImGui::IsItemClicked(Urho3D::MouseButton button)
{
    return ImGui::IsItemClicked(Urho3D::ToImGui(button));
}

bool ImGui::SetDragDropVariant(const char* type, const Urho3D::Variant& variant, ImGuiCond cond)
{
    if (SetDragDropPayload(type, nullptr, 0, cond))
    {
        auto* systemUI = static_cast<Urho3D::SystemUI*>(GetIO().UserData);
        systemUI->GetContext()->SetGlobalVar(QString::asprintf("SystemUI_Drag&Drop_%s", type), variant);
        return true;
    }
    return false;
}

const Urho3D::Variant& ImGui::AcceptDragDropVariant(const char* type, ImGuiDragDropFlags flags)
{
    if (AcceptDragDropPayload(type, flags))
    {
        auto* systemUI = static_cast<Urho3D::SystemUI*>(GetIO().UserData);
        return systemUI->GetContext()->GetGlobalVar(QString::asprintf("SystemUI_Drag&Drop_%s", type));
    }
    return Urho3D::Variant::EMPTY;
}

float ImGui::dpx(float x)
{
    return x * Urho3D::systemUiScale.x_;
}

float ImGui::dpy(float y)
{
    return y * Urho3D::systemUiScale.y_;
}

float ImGui::dp(float z)
{
    return z * Urho3D::systemUiScale.z_;
}

float ImGui::pdpx(float x)
{
    return x * Urho3D::systemUiScalePixelPerfect.x_;
}

float ImGui::pdpy(float y)
{
    return y * Urho3D::systemUiScalePixelPerfect.y_;
}

float ImGui::pdp(float z)
{
    return z * Urho3D::systemUiScalePixelPerfect.z_;
}

float ImGui::litterals::operator "" _dpx(long double x)
{
    return x * Urho3D::systemUiScale.x_;
}

float ImGui::litterals::operator "" _dpx(unsigned long long x)
{
    return x * Urho3D::systemUiScale.x_;
}

float ImGui::litterals::operator "" _dpy(long double y)
{
    return y * Urho3D::systemUiScale.y_;
}

float ImGui::litterals::operator "" _dpy(unsigned long long y)
{
    return y * Urho3D::systemUiScale.y_;
}

float ImGui::litterals::operator "" _dp(long double z)
{
    return z * Urho3D::systemUiScale.z_;
}

float ImGui::litterals::operator "" _dp(unsigned long long z)
{
    return z * Urho3D::systemUiScale.z_;
}

float ImGui::litterals::operator "" _pdpx(long double x)
{
    return x * Urho3D::systemUiScalePixelPerfect.x_;
}

float ImGui::litterals::operator "" _pdpx(unsigned long long x)
{
    return x * Urho3D::systemUiScalePixelPerfect.x_;
}

float ImGui::litterals::operator "" _pdpy(long double y)
{
    return y * Urho3D::systemUiScalePixelPerfect.y_;
}

float ImGui::litterals::operator "" _pdpy(unsigned long long y)
{
    return y * Urho3D::systemUiScalePixelPerfect.y_;
}

float ImGui::litterals::operator "" _pdp(long double z)
{
    return z * Urho3D::systemUiScalePixelPerfect.z_;
}

float ImGui::litterals::operator "" _pdp(unsigned long long z)
{
    return z * Urho3D::systemUiScalePixelPerfect.z_;
}
