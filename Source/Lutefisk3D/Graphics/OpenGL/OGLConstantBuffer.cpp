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
#include "../ConstantBuffer.h"

#include "../Graphics.h"
#include "../GraphicsImpl.h"
#include "../../IO/Log.h"

#include <GL/glew.h>
namespace Urho3D
{

void ConstantBuffer::Release()
{
    if (object_)
    {
        if (!graphics_)
            return;

        graphics_->SetUBO(0);

        glDeleteBuffers(1, &object_);
        object_ = 0;
    }

    shadowData_.reset();
    size_ = 0;
}

void ConstantBuffer::OnDeviceReset()
{
    if (size_)
        SetSize(size_); // Recreate
}

bool ConstantBuffer::SetSize(unsigned size)
{
    if (!size)
    {
        URHO3D_LOGERROR("Can not create zero-sized constant buffer");
        return false;
    }

    // Round up to next 16 bytes
    size += 15;
    size &= 0xfffffff0;

    size_ = size;
    dirty_ = false;
    shadowData_.reset(new unsigned char[size_]);
    memset(shadowData_.get(), 0, size_);

    if (graphics_)
    {
        if (!object_)
            glGenBuffers(1, &object_);
        graphics_->SetUBO(object_);
        glBufferData(GL_UNIFORM_BUFFER, size_, shadowData_.get(), GL_DYNAMIC_DRAW);
    }

    return true;
}

void ConstantBuffer::Apply()
{
    if (dirty_ && object_)
    {
        graphics_->SetUBO(object_);
        glBufferData(GL_UNIFORM_BUFFER, size_, shadowData_.get(), GL_DYNAMIC_DRAW);
        dirty_ = false;
    }
}

}
