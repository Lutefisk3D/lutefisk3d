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



#include <Lutefisk3D/Graphics/Camera.h>
#include <Lutefisk3D/2D/CollisionBox2D.h>
#include <Lutefisk3D/2D/CollisionEdge2D.h>
#include <Lutefisk3D/2D/ConstraintRevolute2D.h>
#include <Lutefisk3D/2D/ConstraintRope2D.h>
#include <Lutefisk3D/Core/CoreEvents.h>
#include <Lutefisk3D/Graphics/DebugRenderer.h>
#include <Lutefisk3D/Engine/Engine.h>
#include <Lutefisk3D/UI/Font.h>
#include <Lutefisk3D/Graphics/Graphics.h>
#include <Lutefisk3D/Input/Input.h>
#include <Lutefisk3D/Graphics/Octree.h>
#include <Lutefisk3D/2D/PhysicsWorld2D.h>
#include <Lutefisk3D/Graphics/Renderer.h>
#include <Lutefisk3D/2D/RigidBody2D.h>
#include <Lutefisk3D/Scene/Scene.h>
#include <Lutefisk3D/Scene/SceneEvents.h>
#include <Lutefisk3D/UI/Text.h>

#include "Urho2DPhysicsRope.h"



URHO3D_DEFINE_APPLICATION_MAIN(Urho2DPhysicsRope)

static const unsigned NUM_OBJECTS = 10;

Urho2DPhysicsRope::Urho2DPhysicsRope(Context* context) :
    Sample("Urho2DPhysicsRope",context)
{
}

void Urho2DPhysicsRope::Start()
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

void Urho2DPhysicsRope::CreateScene()
{
    scene_ = new Scene(m_context);
    scene_->CreateComponent<Octree>();
    scene_->CreateComponent<DebugRenderer>();
    // Create camera node
    cameraNode_ = scene_->CreateChild("Camera");
    // Set camera's position
    cameraNode_->SetPosition(Vector3(0.0f, 5.0f, -10.0f));

    Camera* camera = cameraNode_->CreateComponent<Camera>();
    camera->SetOrthographic(true);

    Graphics* graphics = m_context->m_Graphics.get();
    camera->SetOrthoSize((float)graphics->GetHeight() * 0.05f);
    camera->SetZoom(1.5f * Min((float)graphics->GetWidth() / 1280.0f, (float)graphics->GetHeight() / 800.0f)); // Set zoom according to user's resolution to ensure full visibility (initial zoom (1.5) is set for full visibility at 1280x800 resolution)

    // Create 2D physics world component
    PhysicsWorld2D* physicsWorld = scene_->CreateComponent<PhysicsWorld2D>();
    physicsWorld->SetDrawJoint(true);

    // Create ground
    Node* groundNode = scene_->CreateChild("Ground");
    // Create 2D rigid body for gound
    RigidBody2D* groundBody = groundNode->CreateComponent<RigidBody2D>();
    // Create edge collider for ground
    CollisionEdge2D* groundShape = groundNode->CreateComponent<CollisionEdge2D>();
    groundShape->SetVertices(Vector2(-40.0f, 0.0f), Vector2(40.0f, 0.0f));

    const float y = 15.0f;
    RigidBody2D* prevBody = groundBody;

    for (unsigned i = 0; i < NUM_OBJECTS; ++i)
    {
        Node* node  = scene_->CreateChild("RigidBody");

        // Create rigid body
        RigidBody2D* body = node->CreateComponent<RigidBody2D>();
        body->SetBodyType(BT_DYNAMIC);

        // Create box
        CollisionBox2D* box = node->CreateComponent<CollisionBox2D>();
        // Set friction
        box->SetFriction(0.2f);
        // Set mask bits.
        box->SetMaskBits(0xFFFF & ~0x0002);

        if (i == NUM_OBJECTS - 1)
        {
            node->SetPosition(Vector3(1.0f * i, y, 0.0f));
            body->SetAngularDamping(0.4f);
            box->SetSize(3.0f, 3.0f);
            box->SetDensity(100.0f);
            box->SetCategoryBits(0x0002);
        }
        else
        {
            node->SetPosition(Vector3(0.5f + 1.0f * i, y, 0.0f));
            box->SetSize(1.0f, 0.25f);
            box->SetDensity(20.0f);
            box->SetCategoryBits(0x0001);
        }

        ConstraintRevolute2D* joint = node->CreateComponent<ConstraintRevolute2D>();
        joint->SetOtherBody(prevBody);
        joint->SetAnchor(Vector2(float(i), y));
        joint->SetCollideConnected(false);

        prevBody = body;
    }

    ConstraintRope2D* constraintRope = groundNode->CreateComponent<ConstraintRope2D>();
    constraintRope->SetOtherBody(prevBody);
    constraintRope->SetOwnerBodyAnchor(Vector2(0.0f, y));
    constraintRope->SetMaxLength(NUM_OBJECTS - 1.0f + 0.01f);
}

void Urho2DPhysicsRope::CreateInstructions()
{
    ResourceCache* cache = m_context->m_ResourceCache.get();
    UI* ui = m_context->m_UISystem.get();

    // Construct new Text object, set string to display and font to use
    Text* instructionText = ui->GetRoot()->CreateChild<Text>();
    instructionText->SetText("Use WASD keys and mouse/touch to move, Use PageUp PageDown to zoom.");
    instructionText->SetFont(cache->GetResource<Font>("Fonts/Anonymous Pro.ttf"), 15);

    // Position the text relative to the screen center
    instructionText->SetHorizontalAlignment(HA_CENTER);
    instructionText->SetVerticalAlignment(VA_CENTER);
    instructionText->SetPosition(0, ui->GetRoot()->GetHeight() / 4);
}

void Urho2DPhysicsRope::SetupViewport()
{
    Renderer* renderer = m_context->m_Renderer.get();

    // Set up a viewport to the Renderer subsystem so that the 3D scene can be seen
    SharedPtr<Viewport> viewport(new Viewport(m_context, scene_, cameraNode_->GetComponent<Camera>()));
    renderer->SetViewport(0, viewport);
}

void Urho2DPhysicsRope::MoveCamera(float timeStep)
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

void Urho2DPhysicsRope::SubscribeToEvents()
{
    // Subscribe HandleUpdate() function for processing update events
    g_coreSignals.update.Connect(this,&Urho2DPhysicsRope::HandleUpdate);

    // Unsubscribe the SceneUpdate event from base class to prevent camera pitch and yaw in 2D sample
    g_sceneSignals.sceneUpdate.Disconnect(this);
}

void Urho2DPhysicsRope::HandleUpdate(float timeStep)
{
    // Move the camera, scale movement with time step
    MoveCamera(timeStep);

    PhysicsWorld2D* physicsWorld = scene_->GetComponent<PhysicsWorld2D>();
    physicsWorld->DrawDebugGeometry();
}
