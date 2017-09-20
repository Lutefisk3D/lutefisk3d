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
#include <Lutefisk3D/Core/CoreEvents.h>
#include <Lutefisk3D/Engine/Engine.h>
#include <Lutefisk3D/UI/Font.h>
#include <Lutefisk3D/Graphics/Graphics.h>
#include <Lutefisk3D/Input/Input.h>
#include <Lutefisk3D/Input/InputEvents.h>
#include <Lutefisk3D/Graphics/Octree.h>
#include <Lutefisk3D/2D/ParticleEmitter2D.h>
#include <Lutefisk3D/2D/ParticleEffect2D.h>
#include <Lutefisk3D/Graphics/Renderer.h>
#include <Lutefisk3D/Resource/ResourceCache.h>
#include <Lutefisk3D/Scene/Scene.h>
#include <Lutefisk3D/UI/Text.h>
#include <Lutefisk3D/Graphics/Zone.h>

#include "Urho2DParticle.h"



URHO3D_DEFINE_APPLICATION_MAIN(Urho2DParticle)

Urho2DParticle::Urho2DParticle(Context* context) :
    Sample("Urho2DParticle",context)
{
}

void Urho2DParticle::Start()
{
    // Execute base class startup
    Sample::Start();

    // Set mouse visibile
    Input* input = m_context->m_InputSystem.get();
    input->SetMouseVisible(true);

    // Create the scene content
    CreateScene();

    // Create the UI content
    CreateInstructions();

    // Setup the viewport for displaying the scene
    SetupViewport();

    // Hook up to the frame update events
    SubscribeToEvents();
}

void Urho2DParticle::CreateScene()
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
    camera->SetZoom(1.2f * Min((float)graphics->GetWidth() / 1280.0f, (float)graphics->GetHeight() / 800.0f)); // Set zoom according to user's resolution to ensure full visibility (initial zoom (1.2) is set for full visibility at 1280x800 resolution)

    ResourceCache* cache = m_context->m_ResourceCache.get();
    ParticleEffect2D* particleEffect = cache->GetResource<ParticleEffect2D>("Urho2D/sun.pex");
    if (!particleEffect)
        return;

    particleNode_ = scene_->CreateChild("ParticleEmitter2D");
    ParticleEmitter2D* particleEmitter = particleNode_->CreateComponent<ParticleEmitter2D>();
    particleEmitter->SetEffect(particleEffect);

    ParticleEffect2D* greenSpiralEffect = cache->GetResource<ParticleEffect2D>("Urho2D/greenspiral.pex");
    if (!greenSpiralEffect)
        return;

    Node* greenSpiralNode = scene_->CreateChild("GreenSpiral");
    ParticleEmitter2D* greenSpiralEmitter = greenSpiralNode->CreateComponent<ParticleEmitter2D>();
    greenSpiralEmitter->SetEffect(greenSpiralEffect);
}

void Urho2DParticle::CreateInstructions()
{
    ResourceCache* cache = m_context->m_ResourceCache.get();
    UI* ui = m_context->m_UISystem.get();

    // Construct new Text object, set string to display and font to use
    Text* instructionText = ui->GetRoot()->CreateChild<Text>();
    instructionText->SetText("Use mouse/touch to move the particle.");
    instructionText->SetFont(cache->GetResource<Font>("Fonts/Anonymous Pro.ttf"), 15);

    // Position the text relative to the screen center
    instructionText->SetHorizontalAlignment(HA_CENTER);
    instructionText->SetVerticalAlignment(VA_CENTER);
    instructionText->SetPosition(0, ui->GetRoot()->GetHeight() / 4);
}

void Urho2DParticle::SetupViewport()
{
    Renderer* renderer = m_context->m_Renderer.get();

    // Set up a viewport to the Renderer subsystem so that the 3D scene can be seen
    SharedPtr<Viewport> viewport(new Viewport(m_context, scene_, cameraNode_->GetComponent<Camera>()));
    renderer->SetViewport(0, viewport);
}

void Urho2DParticle::SubscribeToEvents()
{
    g_inputSignals.mouseMove.Connect(this,&Urho2DParticle::HandleMouseMove);
    // Unsubscribe the SceneUpdate event from base class to prevent camera pitch and yaw in 2D sample
    g_sceneSignals.sceneUpdate.Disconnect(this);
}
void Urho2DParticle::HandleMouseMove(int x, int y, int, int, unsigned, int)
{
    if (particleNode_)
    {
        Graphics* graphics = m_context->m_Graphics.get();
        Camera* camera = cameraNode_->GetComponent<Camera>();
        particleNode_->SetPosition(camera->ScreenToWorldPoint(Vector3(float(x) / graphics->GetWidth(), float(y) / graphics->GetHeight(), 10.0f)));
    }
}
