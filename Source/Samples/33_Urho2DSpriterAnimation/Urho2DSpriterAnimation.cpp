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



#include <Lutefisk3D/Urho2D/AnimatedSprite2D.h>
#include <Lutefisk3D/Urho2D/AnimationSet2D.h>
#include <Lutefisk3D/Graphics/Camera.h>
#include <Lutefisk3D/Core/CoreEvents.h>
#include <Lutefisk3D/Urho2D/Drawable2D.h>
#include <Lutefisk3D/Engine/Engine.h>
#include <Lutefisk3D/UI/Font.h>
#include <Lutefisk3D/Graphics/Graphics.h>
#include <Lutefisk3D/Input/Input.h>
#include <Lutefisk3D/Graphics/Octree.h>
#include <Lutefisk3D/Graphics/Renderer.h>
#include <Lutefisk3D/Resource/ResourceCache.h>
#include <Lutefisk3D/Scene/Scene.h>
#include <Lutefisk3D/UI/Text.h>
#include <Lutefisk3D/Graphics/Zone.h>

#include "Urho2DSpriterAnimation.h"



URHO3D_DEFINE_APPLICATION_MAIN(Urho2DSpriterAnimation)

Urho2DSpriterAnimation::Urho2DSpriterAnimation(Context* context) :
    Sample(context),
    spriterAnimationIndex_(0)
{
}

void Urho2DSpriterAnimation::Start()
{
    // Execute base class startup
    Sample::Start();

    // Create the scene content
    CreateScene();

    // Create the UI content
    CreateInstructions();

    // Setup the viewport for displaying the scene
    SetupViewport();

    // Hook up to the frame update events
    SubscribeToEvents();
}

void Urho2DSpriterAnimation::CreateScene()
{
    scene_ = new Scene(m_context);
    scene_->CreateComponent<Octree>();

    // Create camera node
    cameraNode_ = scene_->CreateChild("Camera");
    // Set camera's position
    cameraNode_->SetPosition(Vector3(0.0f, 0.0f, -10.0f));

    Camera* camera = cameraNode_->CreateComponent<Camera>();
    camera->SetOrthographic(true);

    Graphics* graphics = m_context->m_Graphics.get();
    camera->SetOrthoSize((float)graphics->GetHeight() * PIXEL_SIZE);
    camera->SetZoom(1.5f * Min((float)graphics->GetWidth() / 1280.0f, (float)graphics->GetHeight() / 800.0f)); // Set zoom according to user's resolution to ensure full visibility (initial zoom (1.5) is set for full visibility at 1280x800 resolution)

    ResourceCache* cache = m_context->m_ResourceCache.get();  
    AnimationSet2D* spriterAnimationSet = cache->GetResource<AnimationSet2D>("Urho2D/imp/imp.scml");
    if (!spriterAnimationSet)
        return;

    spriterNode_ = scene_->CreateChild("SpriterAnimation");
    AnimatedSprite2D* spriterAnimatedSprite = spriterNode_->CreateComponent<AnimatedSprite2D>();
    spriterAnimatedSprite->SetAnimationSet(spriterAnimationSet);
    spriterAnimatedSprite->SetAnimation(spriterAnimationSet->GetAnimation(spriterAnimationIndex_));
}

void Urho2DSpriterAnimation::CreateInstructions()
{
    ResourceCache* cache = m_context->m_ResourceCache.get();
    UI* ui = m_context->m_UISystem.get();

    // Construct new Text object, set string to display and font to use
    Text* instructionText = ui->GetRoot()->CreateChild<Text>();
    instructionText->SetText("Mouse click to play next animation, \nUse WASD keys to move, use PageUp PageDown keys to zoom.");
    instructionText->SetFont(cache->GetResource<Font>("Fonts/Anonymous Pro.ttf"), 15);
    instructionText->SetTextAlignment(HA_CENTER); // Center rows in relation to each other

    // Position the text relative to the screen center
    instructionText->SetHorizontalAlignment(HA_CENTER);
    instructionText->SetVerticalAlignment(VA_CENTER);
    instructionText->SetPosition(0, ui->GetRoot()->GetHeight() / 4);
}

void Urho2DSpriterAnimation::SetupViewport()
{
    Renderer* renderer = m_context->m_Renderer.get();

    // Set up a viewport to the Renderer subsystem so that the 3D scene can be seen
    SharedPtr<Viewport> viewport(new Viewport(m_context, scene_, cameraNode_->GetComponent<Camera>()));
    renderer->SetViewport(0, viewport);
}

void Urho2DSpriterAnimation::MoveCamera(float timeStep)
{
    // Do not move if the UI has a focused element (the console)
    if (m_context->m_UISystem.get()->GetFocusElement())
        return;

    Input* input = m_context->m_InputSystem.get();

    // Movement speed as world units per second
    const float MOVE_SPEED = 4.0f;

    // Read WASD keys and move the camera scene node to the corresponding direction if they are pressed
    if (input->GetKeyDown(KEY_W))
        cameraNode_->Translate(Vector3::UP * MOVE_SPEED * timeStep);
    if (input->GetKeyDown(KEY_S))
        cameraNode_->Translate(Vector3::DOWN * MOVE_SPEED * timeStep);
    if (input->GetKeyDown(KEY_A))
        cameraNode_->Translate(Vector3::LEFT * MOVE_SPEED * timeStep);
    if (input->GetKeyDown(KEY_D))
        cameraNode_->Translate(Vector3::RIGHT * MOVE_SPEED * timeStep);

    if (input->GetKeyDown(KEY_PAGEUP))
    {
        Camera* camera = cameraNode_->GetComponent<Camera>();
        camera->SetZoom(camera->GetZoom() * 1.01f);
    }

    if (input->GetKeyDown(KEY_PAGEDOWN))
    {
        Camera* camera = cameraNode_->GetComponent<Camera>();
        camera->SetZoom(camera->GetZoom() * 0.99f);
    }
}

void Urho2DSpriterAnimation::SubscribeToEvents()
{
    // Subscribe HandleUpdate() function for processing update events
    g_coreSignals.update.Connect(this,&Urho2DSpriterAnimation::HandleUpdate);
    SubscribeToEvent(E_MOUSEBUTTONDOWN, URHO3D_HANDLER(Urho2DSpriterAnimation, HandleMouseButtonDown));


    // Unsubscribe the SceneUpdate event from base class to prevent camera pitch and yaw in 2D sample
    UnsubscribeFromEvent(E_SCENEUPDATE);
}

void Urho2DSpriterAnimation::HandleUpdate(float timeStep)
{
    using namespace Update;

    // Take the frame time step, which is stored as a float
    float timeStep = eventData[P_TIMESTEP].GetFloat();

    // Move the camera, scale movement with time step
    MoveCamera(timeStep);
}

void Urho2DSpriterAnimation::HandleMouseButtonDown(StringHash eventType, VariantMap& eventData)
{
    AnimatedSprite2D* spriterAnimatedSprite = spriterNode_->GetComponent<AnimatedSprite2D>();
    AnimationSet2D* spriterAnimationSet = spriterAnimatedSprite->GetAnimationSet();
    spriterAnimationIndex_ = (spriterAnimationIndex_ + 1) % spriterAnimationSet->GetNumAnimations();
    spriterAnimatedSprite->SetAnimation(spriterAnimationSet->GetAnimation(spriterAnimationIndex_), LM_FORCE_LOOPED);
}
