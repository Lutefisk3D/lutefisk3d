//
// Copyright (c) 2008-2016 the Urho3D project.
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

#include "SceneAndUILoad.h"

#include <Lutefisk3D/UI/Button.h>
#include <Lutefisk3D/Graphics/Camera.h>
#include <Lutefisk3D/Core/CoreEvents.h>
#include <Lutefisk3D/IO/File.h>
#include <Lutefisk3D/UI/Cursor.h>
#include <Lutefisk3D/Engine/Engine.h>
#include <Lutefisk3D/Graphics/Graphics.h>
#include <Lutefisk3D/Input/Input.h>
#include <Lutefisk3D/Resource/ResourceCache.h>
#include <Lutefisk3D/Scene/Scene.h>
#include <Lutefisk3D/UI/UI.h>
#include <Lutefisk3D/UI/UIEvents.h>
#include <Lutefisk3D/Resource/XMLFile.h>
#include <Lutefisk3D/Graphics/Zone.h>

URHO3D_DEFINE_APPLICATION_MAIN(SceneAndUILoad)

SceneAndUILoad::SceneAndUILoad(Context* context) :
    Sample("SceneAndUILoad",context)
{
}

void SceneAndUILoad::Start()
{
    // Execute base class startup
    Sample::Start();

    // Create the scene content
    CreateScene();

    // Create the UI content
    CreateUI();

    // Setup the viewport for displaying the scene
    SetupViewport();

    // Subscribe to global events for camera movement
    SubscribeToEvents();
}

void SceneAndUILoad::CreateScene()
{
    ResourceCache* cache = m_context->m_ResourceCache.get();

    scene_ = new Scene(m_context);

    // Load scene content prepared in the editor (XML format). GetFile() returns an open file from the resource system
    // which scene.LoadXML() will read
    std::unique_ptr<File> file(cache->GetFile("Scenes/SceneLoadExample.xml"));
    scene_->LoadXML(*file);

    // Create the camera (not included in the scene file)
    cameraNode_ = scene_->CreateChild("Camera");
    cameraNode_->CreateComponent<Camera>();

    // Set an initial position for the camera scene node above the plane
    cameraNode_->SetPosition(Vector3(0.0f, 2.0f, -10.0f));
}

void SceneAndUILoad::CreateUI()
{
    ResourceCache* cache = m_context->m_ResourceCache.get();
    UI* ui = m_context->m_UISystem.get();

    // Set up global UI style into the root UI element
    XMLFile* style = cache->GetResource<XMLFile>("UI/DefaultStyle.xml");
    ui->GetRoot()->SetDefaultStyle(style);

    // Create a Cursor UI element because we want to be able to hide and show it at will. When hidden, the mouse cursor will
    // control the camera, and when visible, it will interact with the UI
    SharedPtr<Cursor> cursor(new Cursor(m_context));
    cursor->SetStyleAuto();
    ui->SetCursor(cursor);
    // Set starting position of the cursor at the rendering window center
    Graphics* graphics = m_context->m_Graphics.get();
    cursor->SetPosition(graphics->GetWidth() / 2, graphics->GetHeight() / 2);

    // Load UI content prepared in the editor and add to the UI hierarchy
    SharedPtr<UIElement> layoutRoot = ui->LoadLayout(cache->GetResource<XMLFile>("UI/UILoadExample.xml"));
    ui->GetRoot()->AddChild(layoutRoot);

    // Subscribe to button actions (toggle scene lights when pressed then released)
    Button* button = static_cast<Button*>(layoutRoot->GetChild("ToggleLight1", true));
    if (button)
        button->released.Connect(this,&SceneAndUILoad::ToggleLight1);
    button = static_cast<Button*>(layoutRoot->GetChild("ToggleLight2", true));
    if (button)
        button->released.Connect(this,&SceneAndUILoad::ToggleLight2);
}

void SceneAndUILoad::SetupViewport()
{
    Renderer* renderer = m_context->m_Renderer.get();

    // Set up a viewport to the Renderer subsystem so that the 3D scene can be seen
    SharedPtr<Viewport> viewport(new Viewport(m_context, scene_, cameraNode_->GetComponent<Camera>()));
    renderer->SetViewport(0, viewport);
}

void SceneAndUILoad::SubscribeToEvents()
{
    // Subscribe HandleUpdate() function for camera motion
    g_coreSignals.update.Connect(this,&SceneAndUILoad::HandleUpdate);
}

void SceneAndUILoad::MoveCamera(float timeStep)
{
    // Right mouse button controls mouse cursor visibility: hide when pressed
    UI* ui = m_context->m_UISystem.get();
    Input* input = m_context->m_InputSystem.get();
    ui->GetCursor()->SetVisible(!input->GetMouseButtonDown(MouseButton::RIGHT));

    // Do not move if the UI has a focused element
    if (ui->GetFocusElement())
        return;

    // Movement speed as world units per second
    const float MOVE_SPEED = 20.0f;
    // Mouse sensitivity as degrees per pixel
    const float MOUSE_SENSITIVITY = 0.1f;

    // Use this frame's mouse motion to adjust camera node yaw and pitch. Clamp the pitch between -90 and 90 degrees
    // Only move the camera when the cursor is hidden
    if (!ui->GetCursor()->IsVisible())
    {
        IntVector2 mouseMove = input->GetMouseMove();
        yaw_ += MOUSE_SENSITIVITY * mouseMove.x_;
        pitch_ += MOUSE_SENSITIVITY * mouseMove.y_;
        pitch_ = Clamp(pitch_, -90.0f, 90.0f);

        // Construct new orientation for the camera scene node from yaw and pitch. Roll is fixed to zero
        cameraNode_->SetRotation(Quaternion(pitch_, yaw_, 0.0f));
    }

    // Read WASD keys and move the camera scene node to the corresponding direction if they are pressed
    if (input->GetKeyDown(KEY_W))
        cameraNode_->Translate(Vector3::FORWARD * MOVE_SPEED * timeStep);
    if (input->GetKeyDown(KEY_S))
        cameraNode_->Translate(Vector3::BACK * MOVE_SPEED * timeStep);
    if (input->GetKeyDown(KEY_A))
        cameraNode_->Translate(Vector3::LEFT * MOVE_SPEED * timeStep);
    if (input->GetKeyDown(KEY_D))
        cameraNode_->Translate(Vector3::RIGHT * MOVE_SPEED * timeStep);
}

void SceneAndUILoad::HandleUpdate(float timeStep)
{
    // Move the camera, scale movement with time step
    MoveCamera(timeStep);
}

void SceneAndUILoad::ToggleLight1(UIElement *)
{
    Node* lightNode = scene_->GetChild("Light1", true);
    if (lightNode)
        lightNode->SetEnabled(!lightNode->IsEnabled());
}

void SceneAndUILoad::ToggleLight2(UIElement *)
{
    Node* lightNode = scene_->GetChild("Light2", true);
    if (lightNode)
        lightNode->SetEnabled(!lightNode->IsEnabled());
}
