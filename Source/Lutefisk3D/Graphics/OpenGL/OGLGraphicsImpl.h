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

#include "../../Math/Color.h"
#include "../../Container/HashMap.h"
#include "../../Core/Timer.h"
#include "../GraphicsDefs.h"

#include "glbinding/gl33ext/enum.h"
#include "glbinding/gl33ext/functions.h"


#include "SDL2/SDL.h"

namespace Urho3D
{
class RenderSurface;
class Context;

/// Cached state of a frame buffer object
struct FrameBufferObject
{
    FrameBufferObject() :
        fbo_(0),
        depthAttachment_(nullptr),
        readBuffers_(M_MAX_UNSIGNED),
        drawBuffers_(M_MAX_UNSIGNED)
    {
        for (auto & elem : colorAttachments_)
            elem = nullptr;
    }

    /// Frame buffer handle.
    unsigned fbo_;
    /// Bound color attachment textures.
    RenderSurface* colorAttachments_[MAX_RENDERTARGETS];
    /// Bound depth/stencil attachment.
    RenderSurface* depthAttachment_;
    /// Read buffer bits.
    unsigned readBuffers_;
    /// Draw buffer bits.
    unsigned drawBuffers_;
};

/// %Graphics subsystem implementation. Holds API-specific objects.
class GraphicsImpl
{
    friend class Graphics;

public:
    /// Construct.
    GraphicsImpl();
    /// Return the SDL window.
    SDL_Window* GetWindow() const { return window_; }
    /// Return the GL Context.
    const SDL_GLContext& GetGLContext() { return context_; }

private:
    /// SDL window.
    SDL_Window* window_;
    /// SDL OpenGL context.
    SDL_GLContext context_;
    /// IOS system framebuffer handle.
    unsigned systemFBO_;
    /// Active texture unit.
    unsigned activeTexture_;
    /// Vertex attributes in use.
    unsigned enabledAttributes_;
    /// Currently bound frame buffer object.
    unsigned boundFBO_;
    /// Currently bound vertex buffer object.
    unsigned boundVBO_;
    /// Currently bound uniform buffer object.
    unsigned boundUBO_;
    /// Current pixel format.
    int pixelFormat_;
    /// Map for FBO's per resolution and format.
    HashMap<unsigned long long, FrameBufferObject> frameBuffers_;
    /// Need FBO commit flag.
    bool fboDirty_;
    /// sRGB write mode flag.
    bool sRGBWrite_;
};

}
