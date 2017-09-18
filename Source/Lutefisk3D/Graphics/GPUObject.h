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

#include "Lutefisk3D/Container/Ptr.h"

namespace Urho3D
{

class Graphics;

/// API-specific GPU object representation.
union GPUObjectHandle
{
    /// Object name (OpenGL.)
    unsigned name_;
};
/// Base class for GPU resources.
class LUTEFISK3D_EXPORT GPUObject
{
public:
    /// Construct with graphics subsystem pointer.
    GPUObject(Graphics* graphics);
    /// Destruct. Remove from the Graphics.
    virtual ~GPUObject();

    /// Mark the GPU resource destroyed on graphics context destruction.
    virtual void OnDeviceLost();
    /// Recreate the GPU resource and restore data if applicable.
    virtual void OnDeviceReset() {}
    /// Unconditionally release the GPU resource.
    virtual void Release() {}

    /// Clear the data lost flag.
    void ClearDataLost();

    /// Return the graphics subsystem associated with this GPU object.
    Graphics* GetGraphics() const;
    /// Return the object's OpenGL handle.
    unsigned GetGPUObject() const { return object_; }
    /// Return whether data is lost due to context loss.
    bool IsDataLost() const { return dataLost_; }
    /// Return whether has pending data assigned while graphics context was lost.
    bool HasPendingData() const { return dataPending_; }

protected:
    /// Graphics subsystem.
    WeakPtr<Graphics> graphics_; // non-owning pointer
    /// Object handle.
    unsigned object_;
    /// Data lost flag.
    bool dataLost_;
    /// Data pending flag.
    bool dataPending_;
};

}
