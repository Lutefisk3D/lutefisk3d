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

#include <Lutefisk3D/Scene/LogicComponent.h>

/// Custom logic component for rotating a scene node.
class Rotator : public Urho3D::LogicComponent
{
    URHO3D_OBJECT(Rotator,Urho3D::LogicComponent);

public:
    /// Construct.
    Rotator(Urho3D::Context* context);

    /// Set rotation speed about the Euler axes. Will be scaled with scene update time step.
    void SetRotationSpeed(const Urho3D::Vector3& speed);
    /// Handle scene update. Called by LogicComponent base class.
    virtual void Update(float timeStep) override;

    /// Return rotation speed.
    const Urho3D::Vector3& GetRotationSpeed() const { return rotationSpeed_; }

private:
    /// Rotation speed.
    Urho3D::Vector3 rotationSpeed_;
};
