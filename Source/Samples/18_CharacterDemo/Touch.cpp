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
#include "Touch.h"

#include "Character.h"

#include <Lutefisk3D/Input/Controls.h>
#include <Lutefisk3D/Core/Context.h>
#include <Lutefisk3D/Graphics/Graphics.h>
#include <Lutefisk3D/Input/Input.h>
#include <Lutefisk3D/Graphics/Renderer.h>

const float GYROSCOPE_THRESHOLD = 0.1f;

Touch::Touch(Context* context, float touchSensitivity) :
    Object(context),
    touchSensitivity_(touchSensitivity),
    cameraDistance_(CAMERA_INITIAL_DIST),
    zoom_(false),
    useGyroscope_(false)
{
}

Touch::~Touch()
{
}

void Touch::UpdateTouches(Controls& controls) // Called from HandleUpdate
{
    zoom_ = false; // reset bool

    Input* input = context_->m_InputSystem.get();

    // Zoom in/out
    // Gyroscope (emulated by SDL through a virtual joystick)
    if (useGyroscope_ && input->GetNumJoysticks() > 0)  // numJoysticks = 1 on iOS & Android
    {
        JoystickState* joystick = input->GetJoystickByIndex(0);
        if (joystick->GetNumAxes() >= 2)
        {
            if (joystick->GetAxisPosition(0) < -GYROSCOPE_THRESHOLD)
                controls.Set(CTRL_LEFT, true);
            if (joystick->GetAxisPosition(0) > GYROSCOPE_THRESHOLD)
                controls.Set(CTRL_RIGHT, true);
            if (joystick->GetAxisPosition(1) < -GYROSCOPE_THRESHOLD)
                controls.Set(CTRL_FORWARD, true);
            if (joystick->GetAxisPosition(1) > GYROSCOPE_THRESHOLD)
                controls.Set(CTRL_BACK, true);
        }
    }
}
