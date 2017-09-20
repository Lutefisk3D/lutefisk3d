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

#include "Lutefisk3D/Graphics/GPUObject.h"
#include "Lutefisk3D/Graphics/GraphicsDefs.h"
#include "Lutefisk3D/Container/RefCounted.h"

namespace Urho3D
{
class Context;
/// Hardware constant buffer.
class LUTEFISK3D_EXPORT ConstantBuffer : public RefCounted, public GPUObject
{
public:
    ConstantBuffer(Context* context);
    virtual ~ConstantBuffer();

    /// Recreate the GPU resource and restore data if applicable.
    void OnDeviceReset() override;
    /// Release the buffer.
    void Release() override;

    /// Set size and create GPU-side buffer. Return true on success.
    bool SetSize(unsigned size);
    /// Set a generic parameter and mark buffer dirty.
    void SetParameter(unsigned offset, unsigned size, const void* data);
    /// Set a Vector3 array parameter and mark buffer dirty.
    void SetVector3ArrayParameter(unsigned offset, unsigned rows, const void* data);
    /// Apply to GPU.
    void Apply();

    /// Return size.
    unsigned GetSize() const { return size_; }
    /// Return whether has unapplied data.
    bool IsDirty() const { return dirty_; }

private:

    /// Shadow data.
    std::unique_ptr<uint8_t[]> shadowData_;
    /// Buffer byte size.
    unsigned size_;
    /// Dirty flag.
    bool dirty_;
};

}

