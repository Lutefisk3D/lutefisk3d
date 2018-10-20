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

#include <Lutefisk3D/Core/CoreEvents.h>
#include <Lutefisk3D/Core/ProcessUtils.h>
#include <Lutefisk3D/Input/Input.h>
#include <Lutefisk3D/Input/InputConstants.h>
#include <Lutefisk3D/Graphics/Graphics.h>
#include <Lutefisk3D/Graphics/Octree.h>
#include <Lutefisk3D/Graphics/Zone.h>
#include <Lutefisk3D/Graphics/Camera.h>
#include <Lutefisk3D/Graphics/Renderer.h>
#include <Lutefisk3D/Resource/ResourceCache.h>
#include <Lutefisk3D/Scene/Scene.h>
#include <Lutefisk3D/SystemUI/SystemUI.h>
#include <Lutefisk3D/SystemUI/Console.h>

#include "HelloSystemUI.h"


// Expands to this example's entry-point
URHO3D_DEFINE_APPLICATION_MAIN(HelloSystemUi)

HelloSystemUi::HelloSystemUi(Context* context) :
    Sample("HelloSystemUi",context)
{
}

void HelloSystemUi::Start()
{
    // Execute base class startup
    Sample::Start();

    // Create scene providing a colored background.
    CreateScene();

    // Finally subscribe to the update event. Note that by subscribing events at this point we have already missed some events
    // like the ScreenMode event sent by the Graphics subsystem when opening the application window. To catch those as well we
    // could subscribe in the constructor instead.
    SubscribeToEvents();

    // Set the mouse mode to use in the sample
    Sample::InitMouseMode(MM_FREE);
}

void HelloSystemUi::SubscribeToEvents()
{
    g_inputSignals.keyDown.Connect(this,&HelloSystemUi::HandleKeyDown);
    g_coreSignals.update.Connect(this,&HelloSystemUi::RenderUi);
}

void HelloSystemUi::RenderUi(float ts)
{
    ui::SetNextWindowSize(ImVec2(200, 300), ImGuiCond_FirstUseEver);
    ui::SetNextWindowPos(ImVec2(200, 300), ImGuiCond_FirstUseEver);
    if (ui::Begin("Sample SystemUI", 0, ImGuiWindowFlags_NoSavedSettings))
    {
        if (messageBox_.NotNull())
        {
            if (ui::Button("Close message box"))
                messageBox_ = nullptr;
        }
        else
        {
            if (ui::Button("Show message box"))
            {
                messageBox_ = new SystemMessageBox(context_, "Hello from SystemUI", "Sample Message Box");
                messageBox_->messageAck.ConnectL([&](bool) { messageBox_ = nullptr; });
            }
        }

        if (ui::Button("Toggle console"))
            GetSubsystem<Console>()->Toggle();

        if (ui::Button("Toggle demo window"))
            demoOpen_ ^= true;
    }
    ui::End();
    if (demoOpen_)
        ui::ShowDemoWindow(&demoOpen_);
}

void HelloSystemUi::HandleKeyDown(int key, int, unsigned, int, bool)
{
    if (key == KEY_GRAVE_ACCENT)
        GetSubsystem<Console>()->Toggle();
}

void HelloSystemUi::CreateScene()
{
    scene_ = new Scene(context_);

    // Create the Octree component to the scene so that drawable objects can be rendered. Use default volume
    // (-1000, -1000, -1000) to (1000, 1000, 1000)
    scene_->CreateComponent<Octree>();

    // Create a Zone component into a child scene node. The Zone controls ambient lighting and fog settings. Like the Octree,
    // it also defines its volume with a bounding box, but can be rotated (so it does not need to be aligned to the world X, Y
    // and Z axes.) Drawable objects "pick up" the zone they belong to and use it when rendering; several zones can exist
    Node* zoneNode = scene_->CreateChild(QStringLiteral("Zone"));
    Zone* zone = zoneNode->CreateComponent<Zone>();
    // Set same volume as the Octree, set a close bluish fog and some ambient light
    zone->SetBoundingBox(BoundingBox(-1000.0f, 1000.0f));
    zone->SetAmbientColor(Color(0.15f, 0.15f, 0.15f));
    zone->SetFogColor(Color(0.5f, 0.5f, 0.7f));
    zone->SetFogStart(100.0f);
    zone->SetFogEnd(300.0f);

    cameraNode_ = scene_->CreateChild(QStringLiteral("Camera"));
    auto camera = cameraNode_->CreateComponent<Camera>();
    GetContext()->m_Renderer->SetViewport(0, new Viewport(context_, scene_, camera));
}
