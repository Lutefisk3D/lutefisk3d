//
// Copyright (c) 2008-2017 the Urho3D project.
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

#include <Lutefisk3D/Engine/Application.h>
#include <Lutefisk3D/Graphics/Camera.h>
#include <Lutefisk3D/SystemUI/Console.h>
#include <Lutefisk3D/UI/Cursor.h>
#include <Lutefisk3D/SystemUI/DebugHud.h>
#include <Lutefisk3D/Engine/Engine.h>
#include <Lutefisk3D/Engine/EngineDefs.h>
#include <Lutefisk3D/IO/FileSystem.h>
#include <Lutefisk3D/Graphics/Graphics.h>
#include <Lutefisk3D/Input/Input.h>
#include <Lutefisk3D/Input/InputEvents.h>
#include <Lutefisk3D/Graphics/Renderer.h>
#include <Lutefisk3D/Resource/ResourceCache.h>
#include <Lutefisk3D/Scene/Scene.h>
#include <Lutefisk3D/Scene/SceneEvents.h>
#include <Lutefisk3D/UI/Sprite.h>
#include <Lutefisk3D/Graphics/Texture2D.h>
#include <Lutefisk3D/Core/Timer.h>
#include <Lutefisk3D/Core/Profiler.h>
#include <Lutefisk3D/UI/UI.h>
#include <Lutefisk3D/Resource/XMLFile.h>
#include <Lutefisk3D/IO/Log.h>
#include <QtCore/QString>

using namespace Urho3D;
Sample::Sample(const QString& sampleName,Urho3D::Context* context) :
    Application(sampleName,context),
    yaw_(0.0f),
    pitch_(0.0f),
    useMouseMode_(Urho3D::MM_ABSOLUTE)
{
}

void Sample::Setup()
{
#ifdef LUTEFISK3D_PROFILING
    if(GetContext()->m_ProfilerSystem)
    {
        GetContext()->m_ProfilerSystem->StartListen();
        GetContext()->m_ProfilerSystem->SetEnabled(true);
    }
#endif
    // Modify engine startup parameters
    engineParameters_[EP_WINDOW_TITLE] = m_appName;
    engineParameters_[EP_LOG_NAME]     = GetContext()->m_FileSystem->GetAppPreferencesDir("urho3d", "logs") + m_appName + ".log";
    engineParameters_[EP_FULL_SCREEN]  = false;
    engineParameters_[EP_HEADLESS]     = false;
    engineParameters_[EP_SOUND]        = false;

    // Construct a search path to find the resource prefix with two entries:
    // The first entry is an empty path which will be substituted with program/bin directory -- this entry is for binary when it is still in build tree
    // The second and third entries are possible relative paths from the installed program/bin directory to the asset directory -- these entries are for binary when it is in the Urho3D SDK installation location
    if (!hashContains(engineParameters_,EP_RESOURCE_PREFIX_PATHS))
        engineParameters_[EP_RESOURCE_PREFIX_PATHS] = ";../share/Resources;../share/Urho3D/Resources";
}

void Sample::Start()
{
    // Create logo
    CreateLogo();

    // Set custom window Title & Icon
    SetWindowTitleAndIcon();

    // Create console and debug HUD
    CreateConsoleAndDebugHud();

    // Subscribe key down event
    g_inputSignals.keyDown.Connect(this,&Sample::HandleKeyDown);
    // Subscribe key up event
    g_inputSignals.keyUp.Connect(this,&Sample::HandleKeyUp);
    // Subscribe scene update event
    g_sceneSignals.sceneUpdate.Connect(this,&Sample::HandleSceneUpdate);
}

void Sample::Stop()
{
    engine_->DumpResources(true);
}

void Sample::InitMouseMode(MouseMode mode)
{
    useMouseMode_ = mode;

    Input* input = GetContext()->m_InputSystem.get();

    assert(GetPlatform() != "Web");
    if (useMouseMode_ == MM_FREE)
        input->SetMouseVisible(true);

    Console* console = GetContext()->GetSubsystemT<Console>();
    if (useMouseMode_ != MM_ABSOLUTE)
    {
        input->SetMouseMode(useMouseMode_);
        if (console && console->IsVisible())
            input->SetMouseMode(MM_ABSOLUTE, true);
    }
}

void Sample::SetLogoVisible(bool enable)
{
    if (logoSprite_)
        logoSprite_->SetVisible(enable);
}

void Sample::CreateLogo()
{
    // Get logo texture
    ResourceCache* cache = GetContext()->m_ResourceCache.get();
    Texture2D* logoTexture = cache->GetResource<Texture2D>("Textures/FishBoneLogo.png");
    if (!logoTexture)
        return;

    // Create logo sprite and add to the UI layout
    UI* ui = GetContext()->m_UISystem.get();
    logoSprite_ = ui->GetRoot()->CreateChild<Sprite>();

    // Set logo sprite texture
    logoSprite_->SetTexture(logoTexture);

    int textureWidth = logoTexture->GetWidth();
    int textureHeight = logoTexture->GetHeight();

    // Set logo sprite scale
    logoSprite_->SetScale(256.0f / textureWidth);

    // Set logo sprite size
    logoSprite_->SetSize(textureWidth, textureHeight);

    // Set logo sprite hot spot
    logoSprite_->SetHotSpot(textureWidth, textureHeight);

    // Set logo sprite alignment
    logoSprite_->SetAlignment(HA_RIGHT, VA_BOTTOM);

    // Make logo not fully opaque to show the scene underneath
    logoSprite_->SetOpacity(0.9f);

    // Set a low priority for the logo so that other UI elements can be drawn on top
    logoSprite_->SetPriority(-100);
}

void Sample::SetWindowTitleAndIcon()
{
    ResourceCache* cache = GetContext()->m_ResourceCache.get();
    Graphics* graphics = GetContext()->m_Graphics.get();
    Image* icon = cache->GetResource<Image>("Textures/UrhoIcon.png");
    graphics->SetWindowIcon(icon);
    graphics->SetWindowTitle("Urho3D Sample");
}

void Sample::CreateConsoleAndDebugHud()
{

    // Create console
    (void)engine_->CreateConsole();

    // Create debug HUD.
    (void)engine_->CreateDebugHud();
}


void Sample::HandleKeyUp(int key,int scancode,unsigned buttons,int qualifiers)
{
    // Close console (if open) or exit when ESC is pressed
    if (key == KEY_ESCAPE)
    {
#if LUTEFISK3D_SYSTEMUI
        Console* console = GetContext()->GetSubsystemT<Console>();
        if (console->IsVisible())
            console->SetVisible(false);
        else
#endif
        {
            if (GetPlatform() == "Web")
            {
                GetContext()->m_InputSystem->SetMouseVisible(true);
                if (useMouseMode_ != MM_ABSOLUTE)
                    GetContext()->m_InputSystem->SetMouseMode(MM_FREE);
            }
            else
                engine_->Exit();
        }
    }
}

void Sample::HandleKeyDown(int key,int scancode,unsigned buttons,int qualifiers, bool repeat)
{
    // Toggle console with F1
    if (key == KEY_F1)
        GetContext()->GetSubsystemT<Console>()->Toggle();

    // Toggle debug HUD with F2
    else if (key == KEY_F2)
        GetContext()->GetSubsystemT<DebugHud>()->ToggleAll();

    // Common rendering quality controls, only when UI has no focused element
    else if (!GetContext()->m_UISystem->GetFocusElement())
    {
        Renderer* renderer = GetContext()->m_Renderer.get();

        // Preferences / Pause
        if (key == '1') // Texture quality
        {
            int quality = renderer->GetTextureQuality();
            ++quality;
            if (quality > QUALITY_HIGH)
                quality = QUALITY_LOW;
            renderer->SetTextureQuality(eQuality(quality));
        }
        else if (key == '2') // Material quality
        {
            int quality = renderer->GetMaterialQuality();
            ++quality;
            if (quality > QUALITY_HIGH)
                quality = QUALITY_LOW;
            renderer->SetMaterialQuality(eQuality(quality));
        }

        // Specular lighting
        else if (key == '3')
            renderer->SetSpecularLighting(!renderer->GetSpecularLighting());

        // Shadow rendering
        else if (key == '4')
            renderer->SetDrawShadows(!renderer->GetDrawShadows());

        // Shadow map resolution
        else if (key == '5')
        {
            int shadowMapSize = renderer->GetShadowMapSize();
            shadowMapSize *= 2;
            if (shadowMapSize > 2048)
                shadowMapSize = 512;
            renderer->SetShadowMapSize(shadowMapSize);
        }

        // Shadow depth and filtering quality
        else if (key == '6')
        {
            ShadowQuality quality = renderer->GetShadowQuality();
            quality = (ShadowQuality)(quality + 1);
            if (quality > SHADOWQUALITY_BLUR_VSM)
                quality = SHADOWQUALITY_SIMPLE_16BIT;
            renderer->SetShadowQuality(quality);
        }

        // Occlusion culling
        else if (key == '7')
        {
            bool occlusion = renderer->GetMaxOccluderTriangles() > 0;
            occlusion = !occlusion;
            renderer->SetMaxOccluderTriangles(occlusion ? 5000 : 0);
        }

        // Instancing
        else if (key == '8')
            renderer->SetDynamicInstancing(!renderer->GetDynamicInstancing());

        // Take screenshot
        else if (key == '9')
        {
            Graphics* graphics = GetContext()->m_Graphics.get();
            Image screenshot(GetContext());
            graphics->TakeScreenShot(screenshot);
            // Here we save in the Data folder with date and time appended
            screenshot.SavePNG(GetContext()->m_FileSystem->GetProgramDir() + "Data/Screenshot_" +
                               Time::GetTimeStamp().replace(':', '_').replace('.', '_').replace(' ', '_') + ".png");
        }
    }
}

void Sample::HandleSceneUpdate(Urho3D::Scene *scene,float timeStep)
{
}

