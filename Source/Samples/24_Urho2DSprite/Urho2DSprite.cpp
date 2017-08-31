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

#include "Urho2DSprite.h"

#include <Lutefisk3D/2D/AnimatedSprite2D.h>
#include <Lutefisk3D/2D/AnimationSet2D.h>
#include <Lutefisk3D/2D/Sprite2D.h>
#include <Lutefisk3D/2D/StaticSprite2D.h>
#include <Lutefisk3D/Core/CoreEvents.h>
#include <Lutefisk3D/Engine/Engine.h>
#include <Lutefisk3D/Engine/Application.h>
#include <Lutefisk3D/UI/Font.h>
#include <Lutefisk3D/UI/Text.h>
#include <Lutefisk3D/Input/Input.h>
#include <Lutefisk3D/Resource/ResourceCache.h>
#include <Lutefisk3D/Scene/Scene.h>
#include <Lutefisk3D/Graphics/Camera.h>
#include <Lutefisk3D/Graphics/Zone.h>
#include <Lutefisk3D/Graphics/Graphics.h>
#include <Lutefisk3D/Graphics/Octree.h>
#include <Lutefisk3D/Graphics/Renderer.h>




// Number of static sprites to draw
static const unsigned NUM_SPRITES = 3200;
static const StringHash VAR_MOVESPEED("MoveSpeed");
static const StringHash VAR_ROTATESPEED("RotateSpeed");

URHO3D_DEFINE_APPLICATION_MAIN(Urho2DSprite)

Urho2DSprite::Urho2DSprite(Context* context) :
    Sample("Urho2DSprite",context)
{
}

void Urho2DSprite::Start()
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

void Urho2DSprite::CreateScene()
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

    ResourceCache* cache = m_context->m_ResourceCache.get();
    // Get sprite
    Sprite2D* sprite = cache->GetResource<Sprite2D>("Urho2D/Aster.png");
    if (!sprite)
        return;

    float halfWidth  = graphics->GetWidth() * 0.5f * PIXEL_SIZE;
    float halfHeight = graphics->GetHeight() * 0.5f * PIXEL_SIZE;

    for (unsigned i = 0; i < NUM_SPRITES; ++i)
    {
        SharedPtr<Node> spriteNode(scene_->CreateChild("StaticSprite2D"));
        spriteNode->SetPosition(Vector3(Random(-halfWidth, halfWidth), Random(-halfHeight, halfHeight), 0.0f));

        StaticSprite2D* staticSprite = spriteNode->CreateComponent<StaticSprite2D>();
        // Set random color
        staticSprite->SetColor(Color(Random(1.0f), Random(1.0f), Random(1.0f), 1.0f));
        // Set blend mode
        staticSprite->SetBlendMode(BLEND_ALPHA);
        // Set sprite
        staticSprite->SetSprite(sprite);

        // Set move speed
        spriteNode->SetVar(VAR_MOVESPEED, Vector3(Random(-2.0f, 2.0f), Random(-2.0f, 2.0f), 0.0f));
        // Set rotate speed
        spriteNode->SetVar(VAR_ROTATESPEED, Random(-90.0f, 90.0f));

        // Add to sprite node vector
        spriteNodes_.push_back(spriteNode);
    }

    // Get animation set
    AnimationSet2D* animationSet = cache->GetResource<AnimationSet2D>("Urho2D/GoldIcon.scml");
    if (!animationSet)
        return;

    SharedPtr<Node> spriteNode(scene_->CreateChild("AnimatedSprite2D"));
    spriteNode->SetPosition(Vector3(0.0f, 0.0f, -1.0f));

    AnimatedSprite2D* animatedSprite = spriteNode->CreateComponent<AnimatedSprite2D>();
    // Set animation
    animatedSprite->SetAnimationSet(animationSet);
    animatedSprite->SetAnimation("idle");
}

void Urho2DSprite::CreateInstructions()
{
    ResourceCache* cache = m_context->m_ResourceCache.get();
    UI* ui = m_context->m_UISystem.get();

    // Construct new Text object, set string to display and font to use
    Text* instructionText = ui->GetRoot()->CreateChild<Text>();
    instructionText->SetText("Use WASD keys to move, use PageUp PageDown keys to zoom.");
    instructionText->SetFont(cache->GetResource<Font>("Fonts/Anonymous Pro.ttf"), 15);

    // Position the text relative to the screen center
    instructionText->SetHorizontalAlignment(HA_CENTER);
    instructionText->SetVerticalAlignment(VA_CENTER);
    instructionText->SetPosition(0, ui->GetRoot()->GetHeight() / 4);
}

void Urho2DSprite::SetupViewport()
{
    Renderer* renderer = m_context->m_Renderer.get();

    // Set up a viewport to the Renderer subsystem so that the 3D scene can be seen
    SharedPtr<Viewport> viewport(new Viewport(m_context, scene_, cameraNode_->GetComponent<Camera>()));
    renderer->SetViewport(0, viewport);
}

void Urho2DSprite::MoveCamera(float timeStep)
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

void Urho2DSprite::SubscribeToEvents()
{
    // Subscribe HandleUpdate() function for processing update events
    g_coreSignals.update.Connect(this,&Urho2DSprite::HandleUpdate);

    // Unsubscribe the SceneUpdate event from base class to prevent camera pitch and yaw in 2D sample
    g_sceneSignals.sceneUpdate.Disconnect(this);
}

void Urho2DSprite::HandleUpdate(float timeStep)
{
    // Move the camera, scale movement with time step
    MoveCamera(timeStep);

    Graphics* graphics = m_context->m_Graphics.get();
    float halfWidth = (float)graphics->GetWidth() * 0.5f * PIXEL_SIZE;
    float halfHeight = (float)graphics->GetHeight() * 0.5f * PIXEL_SIZE;

    for (unsigned i = 0; i < spriteNodes_.size(); ++i)
    {
        SharedPtr<Node> node = spriteNodes_[i];

        Vector3 position = node->GetPosition();
        Vector3 moveSpeed = node->GetVar(VAR_MOVESPEED).GetVector3();
        Vector3 newPosition = position + moveSpeed * timeStep;
        if (newPosition.x_ < -halfWidth || newPosition.x_ > halfWidth)
        {
            newPosition.x_ = position.x_;
            moveSpeed.x_ = -moveSpeed.x_;
            node->SetVar(VAR_MOVESPEED, moveSpeed);
        }
        if (newPosition.y_ < -halfHeight || newPosition.y_ > halfHeight)
        {
            newPosition.y_ = position.y_;
            moveSpeed.y_ = -moveSpeed.y_;
            node->SetVar(VAR_MOVESPEED, moveSpeed);
        }

        node->SetPosition(newPosition);

        float rotateSpeed = node->GetVar(VAR_ROTATESPEED).GetFloat();
        node->Roll(rotateSpeed * timeStep);
    }
}
