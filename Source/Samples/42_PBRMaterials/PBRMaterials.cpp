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
#include "PBRMaterials.h"

#include <Lutefisk3D/Core/CoreEvents.h>
#include <Lutefisk3D/Engine/Engine.h>
#include <Lutefisk3D/Graphics/Camera.h>
#include <Lutefisk3D/Graphics/Graphics.h>
#include <Lutefisk3D/Graphics/RenderPath.h>
#include <Lutefisk3D/Graphics/StaticModel.h>
#include <Lutefisk3D/Graphics/Zone.h>
#include <Lutefisk3D/IO/File.h>
#include <Lutefisk3D/Input/Input.h>
#include <Lutefisk3D/Resource/ResourceCache.h>
#include <Lutefisk3D/Scene/Scene.h>
#include <Lutefisk3D/UI/Button.h>
#include <Lutefisk3D/UI/Font.h>
#include <Lutefisk3D/UI/Slider.h>
#include <Lutefisk3D/UI/UI.h>
#include <Lutefisk3D/UI/UIEvents.h>
#include <Lutefisk3D/UI/Text.h>
#ifdef LUTEFISK3D_ANGELSCRIPT
#include <Lutefisk3D/AngelScript/Script.h>
#endif


URHO3D_DEFINE_APPLICATION_MAIN(PBRMaterials)

PBRMaterials::PBRMaterials(Context* context) :
    Sample("PBRMaterials",context),
    dynamicMaterial_(0),
    roughnessLabel_(0),
    metallicLabel_(0),
    ambientLabel_(0)
{
}

void PBRMaterials::Start()
{
    // Execute base class startup
    Sample::Start();

    // Create the scene content
    CreateScene();

    // Create the UI content
    CreateUI();
    CreateInstructions();

    // Setup the viewport for displaying the scene
    SetupViewport();

    // Subscribe to global events for camera movement
    SubscribeToEvents();
}

void PBRMaterials::CreateInstructions()
{
    ResourceCache* cache = m_context->m_ResourceCache.get();
    UI* ui = m_context->m_UISystem.get();

    // Construct new Text object, set string to display and font to use
    Text* instructionText = ui->GetRoot()->CreateChild<Text>();
    instructionText->SetText("Use sliders to change Roughness and Metallic\n"
        "Hold RMB and use WASD keys and mouse to move");
    instructionText->SetFont(cache->GetResource<Font>("Fonts/Anonymous Pro.ttf"), 15);
    instructionText->SetTextAlignment(HA_CENTER);

    // Position the text relative to the screen center
    instructionText->SetHorizontalAlignment(HA_CENTER);
    instructionText->SetVerticalAlignment(VA_CENTER);
    instructionText->SetPosition(0, ui->GetRoot()->GetHeight() / 4);
}

void PBRMaterials::CreateScene()
{
    ResourceCache* cache = m_context->m_ResourceCache.get();

#ifdef LUTEFISK3D_ANGELSCRIPT
    // The scene uses an AngelScript component for animation. Instantiate the subsystem if possible
    m_context->RegisterSubsystem(new Script(m_context));
#endif

    scene_ = new Scene(m_context);

    // Load scene content prepared in the editor (XML format). GetFile() returns an open file from the resource system
    // which scene.LoadXML() will read
    SharedPtr<File> file = cache->GetFile("Scenes/PBRExample.xml");
    scene_->LoadXML(*file);

    Node* sphereWithDynamicMatNode = scene_->GetChild("SphereWithDynamicMat");
    StaticModel* staticModel = sphereWithDynamicMatNode->GetComponent<StaticModel>();
    dynamicMaterial_ = staticModel->GetMaterial(0);

    Node* zoneNode = scene_->GetChild("Zone");
    zone_ = zoneNode->GetComponent<Zone>();

    // Create the camera (not included in the scene file)
    cameraNode_ = scene_->CreateChild("Camera");
    cameraNode_->CreateComponent<Camera>();

    cameraNode_->SetPosition(sphereWithDynamicMatNode->GetPosition() + Vector3(2.0f, 2.0f, 2.0f));
    cameraNode_->LookAt(sphereWithDynamicMatNode->GetPosition());
    yaw_ = cameraNode_->GetRotation().YawAngle();
    pitch_ = cameraNode_->GetRotation().PitchAngle();
}

void PBRMaterials::CreateUI()
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

    roughnessLabel_ = ui->GetRoot()->CreateChild<Text>();
    roughnessLabel_->SetFont(cache->GetResource<Font>("Fonts/Anonymous Pro.ttf"), 15);
    roughnessLabel_->SetPosition(370, 50);
    roughnessLabel_->SetTextEffect(TE_SHADOW);

    metallicLabel_ = ui->GetRoot()->CreateChild<Text>();
    metallicLabel_->SetFont(cache->GetResource<Font>("Fonts/Anonymous Pro.ttf"), 15);
    metallicLabel_->SetPosition(370, 100);
    metallicLabel_->SetTextEffect(TE_SHADOW);

    ambientLabel_ = ui->GetRoot()->CreateChild<Text>();
    ambientLabel_->SetFont(cache->GetResource<Font>("Fonts/Anonymous Pro.ttf"), 15);
    ambientLabel_->SetPosition(370, 150);
    ambientLabel_->SetTextEffect(TE_SHADOW);

    Slider* roughnessSlider = ui->GetRoot()->CreateChild<Slider>();
    roughnessSlider->SetStyleAuto();
    roughnessSlider->SetPosition(50, 50);
    roughnessSlider->SetSize(300, 20);
    roughnessSlider->SetRange(1.0f); // 0 - 1 range
    roughnessSlider->sliderChanged.Connect(this,&PBRMaterials::HandleRoughnessSliderChanged);
    roughnessSlider->SetValue(0.5f);

    Slider* metallicSlider = ui->GetRoot()->CreateChild<Slider>();
    metallicSlider->SetStyleAuto();
    metallicSlider->SetPosition(50, 100);
    metallicSlider->SetSize(300, 20);
    metallicSlider->SetRange(1.0f); // 0 - 1 range
    metallicSlider->sliderChanged.Connect(this,&PBRMaterials::HandleMetallicSliderChanged);
    metallicSlider->SetValue(0.5f);

    Slider* ambientSlider = ui->GetRoot()->CreateChild<Slider>();
    ambientSlider->SetStyleAuto();
    ambientSlider->SetPosition(50, 150);
    ambientSlider->SetSize(300, 20);
    ambientSlider->SetRange(10.0f); // 0 - 10 range
    ambientSlider->sliderChanged.Connect(this,&PBRMaterials::HandleAmbientSliderChanged);
    ambientSlider->SetValue(zone_->GetAmbientColor().a_);
}

void PBRMaterials::HandleRoughnessSliderChanged(UIElement *,float newValue)
{
    dynamicMaterial_->SetShaderParameter("Roughness", newValue);
    roughnessLabel_->SetText("Roughness: " + QString::number(newValue));
}

void PBRMaterials::HandleMetallicSliderChanged(UIElement *, float newValue)
{
    dynamicMaterial_->SetShaderParameter("Metallic", newValue);
    metallicLabel_->SetText("Metallic: " + QString::number(newValue));
}

void PBRMaterials::HandleAmbientSliderChanged(UIElement *, float newValue)
{
    Color col = Color(0.0, 0.0, 0.0, newValue);
    zone_->SetAmbientColor(col);
    ambientLabel_->SetText("Ambient HDR Scale: " + QString::number(zone_->GetAmbientColor().a_));
}

void PBRMaterials::SetupViewport()
{
    ResourceCache* cache = m_context->m_ResourceCache.get();
    Renderer* renderer = m_context->m_Renderer.get();

    renderer->SetHDRRendering(true);

    // Set up a viewport to the Renderer subsystem so that the 3D scene can be seen
    SharedPtr<Viewport> viewport(new Viewport(m_context, scene_, cameraNode_->GetComponent<Camera>()));
    renderer->SetViewport(0, viewport);

    // Add post-processing effects appropriate with the example scene
    SharedPtr<RenderPath> effectRenderPath = viewport->GetRenderPath()->Clone();
    effectRenderPath->Append(cache->GetResource<XMLFile>("PostProcess/FXAA2.xml"));
    effectRenderPath->Append(cache->GetResource<XMLFile>("PostProcess/GammaCorrection.xml"));

    viewport->SetRenderPath(effectRenderPath);
}

void PBRMaterials::SubscribeToEvents()
{
    // Subscribe HandleUpdate() function for camera motion
    g_coreSignals.update.Connect(this,&PBRMaterials::HandleUpdate);
}

void PBRMaterials::MoveCamera(float timeStep)
{
    // Right mouse button controls mouse cursor visibility: hide when pressed
    UI* ui = m_context->m_UISystem.get();
    Input* input = m_context->m_InputSystem.get();
    ui->GetCursor()->SetVisible(!input->GetMouseButtonDown(MOUSEB_RIGHT));

    // Do not move if the UI has a focused element
    if (ui->GetFocusElement())
        return;

    // Movement speed as world units per second
    const float MOVE_SPEED = 10.0f;
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

void PBRMaterials::HandleUpdate(float timeStep)
{
    // Move the camera, scale movement with time step
    MoveCamera(timeStep);
}
