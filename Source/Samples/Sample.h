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

#pragma once

#include <Lutefisk3D/Engine/Application.h>
#include <Lutefisk3D/Input/Input.h>
#include <Lutefisk3D/Core/Context.h>
#include <QtCore/QString>

namespace Urho3D
{

class Node;
class Scene;
class Sprite;

}

// All Urho3D classes reside in namespace Urho3D

const float TOUCH_SENSITIVITY = 2.0f;

/// Sample class, as framework for all samples.
///    - Initialization of the Urho3D engine (in Application class)
///    - Modify engine parameters for windowed mode and to show the class name as title
///    - Create Urho3D logo at screen
///    - Set custom window title and icon
///    - Create Console and Debug HUD, and use F1 and F2 key to toggle them
///    - Toggle rendering options from the keys 1-8
///    - Take screenshot with key 9
///    - Handle Esc key down to hide Console or exit application
///    - Init touch input on mobile platform using screen joysticks (patched for each individual sample)
class Sample : public Urho3D::Application
{
public:
    /// Construct.
    Sample(const QString& sampleName,Urho3D::Context* context);

    /// Setup before engine initialization. Modifies the engine parameters.
    void Setup() override;
    /// Setup after engine initialization. Creates the logo, console & debug HUD.
    void Start() override;
    /// Cleanup after the main loop. Called by Application.
    void Stop() override;

protected:
    /// Initialize mouse mode on non-web platform.
    void InitMouseMode(Urho3D::MouseMode mode);
    /// Control logo visibility.
    void SetLogoVisible(bool enable);

    /// Logo sprite.
    Urho3D::SharedPtr<Urho3D::Sprite> logoSprite_;
    /// Scene.
    Urho3D::SharedPtr<Urho3D::Scene> scene_;
    /// Camera scene node.
    Urho3D::SharedPtr<Urho3D::Node> cameraNode_;
    /// Camera yaw angle.
    float yaw_=0;
    /// Camera pitch angle.
    float pitch_=0;
    /// Mouse mode option to use in the sample.
    Urho3D::MouseMode useMouseMode_;
private:
    /// Create logo.
    void CreateLogo();
    /// Set custom window Title & Icon
    void SetWindowTitleAndIcon();
    /// Create console and debug HUD.
    void CreateConsoleAndDebugHud();
    /// Handle key down event to process key controls common to all samples.
    virtual void HandleKeyDown(int key,int scancode,unsigned buttons,int qualifiers, bool repeat);
    /// Handle key up event to process key controls common to all samples.
    void HandleKeyUp(int key,int scancode,unsigned buttons,int qualifiers);
    /// Handle scene update event to control camera's pitch and yaw for all samples.
    void HandleSceneUpdate(Urho3D::Scene *scene,float timeStep);
};

#include "Sample.inl"
