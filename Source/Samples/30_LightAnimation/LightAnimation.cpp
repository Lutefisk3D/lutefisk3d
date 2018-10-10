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
#include "LightAnimation.h"

#include <Lutefisk3D/Scene/Animatable.h>
#include <Lutefisk3D/Graphics/Camera.h>
#include <Lutefisk3D/Core/CoreEvents.h>
#include <Lutefisk3D/Engine/Engine.h>
#include <Lutefisk3D/UI/Font.h>
#include <Lutefisk3D/Graphics/Graphics.h>
#include <Lutefisk3D/Input/Input.h>
#include <Lutefisk3D/Graphics/Light.h>
#include <Lutefisk3D/Graphics/Material.h>
#include <Lutefisk3D/Graphics/Model.h>
#include <Lutefisk3D/Graphics/Octree.h>
#include <Lutefisk3D/Scene/ObjectAnimation.h>
#include <Lutefisk3D/Graphics/Renderer.h>
#include <Lutefisk3D/Resource/ResourceCache.h>
#include <Lutefisk3D/Scene/Scene.h>
#include <Lutefisk3D/Graphics/StaticModel.h>
#include <Lutefisk3D/UI/Text.h>
#include <Lutefisk3D/UI/UI.h>
#include <Lutefisk3D/Scene/ValueAnimation.h>


URHO3D_DEFINE_APPLICATION_MAIN(LightAnimation)

LightAnimation::LightAnimation(Context* context) :
    Sample("LightAnimation",context)
{
}

void LightAnimation::Start()
{
    // Execute base class startup
    Sample::Start();

    // Create the UI content
    CreateInstructions();

    // Create the scene content
    CreateScene();

    // Setup the viewport for displaying the scene
    SetupViewport();

    // Hook up to the frame update events
    SubscribeToEvents();
}

void LightAnimation::CreateScene()
{
    ResourceCache* cache = GetContext()->m_ResourceCache.get();

    scene_ = new Scene(GetContext());

    // Create the Octree component to the scene. This is required before adding any drawable components, or else nothing will
    // show up. The default octree volume will be from (-1000, -1000, -1000) to (1000, 1000, 1000) in world coordinates; it
    // is also legal to place objects outside the volume but their visibility can then not be checked in a hierarchically
    // optimizing manner
    scene_->CreateComponent<Octree>();

    // Create a child scene node (at world origin) and a StaticModel component into it. Set the StaticModel to show a simple
    // plane mesh with a "stone" material. Note that naming the scene nodes is optional. Scale the scene node larger
    // (100 x 100 world units)
    Node* planeNode = scene_->CreateChild("Plane");
    planeNode->SetScale(Vector3(100.0f, 1.0f, 100.0f));
    StaticModel* planeObject = planeNode->CreateComponent<StaticModel>();
    planeObject->SetModel(cache->GetResource<Model>("Models/Plane.mdl"));
    planeObject->SetMaterial(cache->GetResource<Material>("Materials/StoneTiled.xml"));

    // Create a point light to the world so that we can see something.
    Node* lightNode = scene_->CreateChild("PointLight");
    Light* light = lightNode->CreateComponent<Light>();
    light->SetLightType(LIGHT_POINT);
    light->SetRange(10.0f);

    // Create light animation
    SharedPtr<ObjectAnimation> lightAnimation(new ObjectAnimation(GetContext()));

    // Create light position animation
    SharedPtr<ValueAnimation> positionAnimation(new ValueAnimation(GetContext()));
    // Use spline interpolation method
    positionAnimation->SetInterpolationMethod(IM_SPLINE);
    // Set spline tension
    positionAnimation->SetSplineTension(0.7f);
    positionAnimation->SetKeyFrame(0.0f, Vector3(-30.0f, 5.0f, -30.0f));
    positionAnimation->SetKeyFrame(1.0f, Vector3( 30.0f, 5.0f, -30.0f));
    positionAnimation->SetKeyFrame(2.0f, Vector3( 30.0f, 5.0f,  30.0f));
    positionAnimation->SetKeyFrame(3.0f, Vector3(-30.0f, 5.0f,  30.0f));
    positionAnimation->SetKeyFrame(4.0f, Vector3(-30.0f, 5.0f, -30.0f));
    // Set position animation
    lightAnimation->AddAttributeAnimation("Position", positionAnimation);

    // Create text animation
    SharedPtr<ValueAnimation> textAnimation(new ValueAnimation(GetContext()));
    textAnimation->SetKeyFrame(0.0f, "WHITE");
    textAnimation->SetKeyFrame(1.0f, "RED");
    textAnimation->SetKeyFrame(2.0f, "YELLOW");
    textAnimation->SetKeyFrame(3.0f, "GREEN");
    textAnimation->SetKeyFrame(4.0f, "WHITE");
    GetContext()->m_UISystem.get()->GetRoot()->GetChild(QString("animatingText"))->SetAttributeAnimation("Text", textAnimation);

    // Create light color animation
    SharedPtr<ValueAnimation> colorAnimation(new ValueAnimation(GetContext()));
    colorAnimation->SetKeyFrame(0.0f, Color::WHITE);
    colorAnimation->SetKeyFrame(1.0f, Color::RED);
    colorAnimation->SetKeyFrame(2.0f, Color::YELLOW);
    colorAnimation->SetKeyFrame(3.0f, Color::GREEN);
    colorAnimation->SetKeyFrame(4.0f, Color::WHITE);
    // Set Light component's color animation
    lightAnimation->AddAttributeAnimation("@Light/Color", colorAnimation);

    // Apply light animation to light node
    lightNode->SetObjectAnimation(lightAnimation);

    // Create more StaticModel objects to the scene, randomly positioned, rotated and scaled. For rotation, we construct a
    // quaternion from Euler angles where the Y angle (rotation about the Y axis) is randomized. The mushroom model contains
    // LOD levels, so the StaticModel component will automatically select the LOD level according to the view distance (you'll
    // see the model get simpler as it moves further away). Finally, rendering a large number of the same object with the
    // same material allows instancing to be used, if the GPU supports it. This reduces the amount of CPU work in rendering the
    // scene.
    const unsigned NUM_OBJECTS = 200;
    for (unsigned i = 0; i < NUM_OBJECTS; ++i)
    {
        Node* mushroomNode = scene_->CreateChild("Mushroom");
        mushroomNode->SetPosition(Vector3(Random(90.0f) - 45.0f, 0.0f, Random(90.0f) - 45.0f));
        mushroomNode->SetRotation(Quaternion(0.0f, Random(360.0f), 0.0f));
        mushroomNode->SetScale(0.5f + Random(2.0f));
        StaticModel* mushroomObject = mushroomNode->CreateComponent<StaticModel>();
        mushroomObject->SetModel(cache->GetResource<Model>("Models/Mushroom.mdl"));
        mushroomObject->SetMaterial(cache->GetResource<Material>("Materials/Mushroom.xml"));
    }

    // Create a scene node for the camera, which we will move around
    // The camera will use default settings (1000 far clip distance, 45 degrees FOV, set aspect ratio automatically)
    cameraNode_ = scene_->CreateChild("Camera");
    cameraNode_->CreateComponent<Camera>();

    // Set an initial position for the camera scene node above the plane
    cameraNode_->SetPosition(Vector3(0.0f, 5.0f, 0.0f));
}

void LightAnimation::CreateInstructions()
{
    ResourceCache* cache = GetContext()->m_ResourceCache.get();
    UI* ui = GetContext()->m_UISystem.get();

    // Construct new Text object, set string to display and font to use
    Text* instructionText = ui->GetRoot()->CreateChild<Text>();
    instructionText->SetText("Use WASD keys and mouse/touch to move");
    Font* font = cache->GetResource<Font>("Fonts/Anonymous Pro.ttf");
    instructionText->SetFont(font, 15);

    // Position the text relative to the screen center
    instructionText->SetHorizontalAlignment(HA_CENTER);
    instructionText->SetVerticalAlignment(VA_CENTER);
    instructionText->SetPosition(0, ui->GetRoot()->GetHeight() / 4);

    // Animating text
    Text* text = ui->GetRoot()->CreateChild<Text>("animatingText");
    text->SetFont(font, 15);
    text->SetHorizontalAlignment(HA_CENTER);
    text->SetVerticalAlignment(VA_CENTER);
    text->SetPosition(0, ui->GetRoot()->GetHeight() / 4 + 20);
}

void LightAnimation::SetupViewport()
{
    Renderer* renderer = GetContext()->m_Renderer.get();

    // Set up a viewport to the Renderer subsystem so that the 3D scene can be seen. We need to define the scene and the camera
    // at minimum. Additionally we could configure the viewport screen size and the rendering path (eg. forward / deferred) to
    // use, but now we just use full screen and default render path configured in the engine command line options
    SharedPtr<Viewport> viewport(new Viewport(GetContext(), scene_, cameraNode_->GetComponent<Camera>()));
    renderer->SetViewport(0, viewport);
}

void LightAnimation::MoveCamera(float timeStep)
{
    // Do not move if the UI has a focused element (the console)
    if (GetContext()->m_UISystem.get()->GetFocusElement())
        return;

    Input* input = GetContext()->m_InputSystem.get();

    // Movement speed as world units per second
    const float MOVE_SPEED = 20.0f;
    // Mouse sensitivity as degrees per pixel
    const float MOUSE_SENSITIVITY = 0.1f;

    // Use this frame's mouse motion to adjust camera node yaw and pitch. Clamp the pitch between -90 and 90 degrees
    IntVector2 mouseMove = input->GetMouseMove();
    yaw_ += MOUSE_SENSITIVITY * mouseMove.x_;
    pitch_ += MOUSE_SENSITIVITY * mouseMove.y_;
    pitch_ = Clamp(pitch_, -90.0f, 90.0f);

    // Construct new orientation for the camera scene node from yaw and pitch. Roll is fixed to zero
    cameraNode_->SetRotation(Quaternion(pitch_, yaw_, 0.0f));

    // Read WASD keys and move the camera scene node to the corresponding direction if they are pressed
    // Use the Translate() function (default local space) to move relative to the node's orientation.
    if (input->GetKeyDown(KEY_W))
        cameraNode_->Translate(Vector3::FORWARD * MOVE_SPEED * timeStep);
    if (input->GetKeyDown(KEY_S))
        cameraNode_->Translate(Vector3::BACK * MOVE_SPEED * timeStep);
    if (input->GetKeyDown(KEY_A))
        cameraNode_->Translate(Vector3::LEFT * MOVE_SPEED * timeStep);
    if (input->GetKeyDown(KEY_D))
        cameraNode_->Translate(Vector3::RIGHT * MOVE_SPEED * timeStep);
}

void LightAnimation::SubscribeToEvents()
{
    // Subscribe HandleUpdate() function for processing update events
    g_coreSignals.update.Connect(this,&LightAnimation::HandleUpdate);
}

void LightAnimation::HandleUpdate(float timeStep)
{
    // Move the camera, scale movement with time step
    MoveCamera(timeStep);
}
