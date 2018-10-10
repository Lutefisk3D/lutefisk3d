//
// Copyright (c) 2008-2017 the Urho3D project.
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

#include "RenderSurface.h"

#include "Camera.h"
#include "Graphics.h"
#include "GraphicsImpl.h"
#include "Renderer.h"
#include "Texture.h"
#include <GL/glew.h>

namespace Urho3D
{
struct RenderSurfacePrivate
{
    /// Viewports.
    std::vector<SharedPtr<Viewport> > viewports_;
    /// Linked color buffer.
    WeakPtr<RenderSurface> linkedRenderTarget_;
    /// Linked depth buffer.
    WeakPtr<RenderSurface> linkedDepthStencil_;

};
RenderSurface::RenderSurface(Texture* parentTexture) :
    d(new RenderSurfacePrivate),
    parentTexture_(parentTexture),
    target_(GL_TEXTURE_2D),
    updateMode_(SURFACE_UPDATEVISIBLE)
{
}
RenderSurface::~RenderSurface()
{
    // only release if parent texture hasn't expired, in that case 
    // parent texture was deleted and will have called release on render surface
    if (!parentTexture_.Expired())
    {
        Release();
    }
}

bool RenderSurface::CreateRenderBuffer(unsigned width, unsigned height, uint32_t format, int multiSample)
{
    Graphics* graphics = parentTexture_->GetGraphics();
    if (!graphics)
        return false;

    Release();

    glGenRenderbuffers(1, &renderBuffer_);
    glBindRenderbuffer(GL_RENDERBUFFER, renderBuffer_);
    if (multiSample > 1)
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, multiSample, format, width, height);
    else
        glRenderbufferStorage(GL_RENDERBUFFER, format, width, height);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    return true;
}
void RenderSurface::OnDeviceLost()
{
    Graphics* graphics = parentTexture_->GetGraphics();
    if (!graphics)
        return;

    for (unsigned i = 0; i < MAX_RENDERTARGETS; ++i)
    {
        if (graphics->GetRenderTarget(i) == this)
            graphics->ResetRenderTarget(i);
    }

    if (graphics->GetDepthStencil() == this)
        graphics->ResetDepthStencil();

    // Clean up also from non-active FBOs
    graphics->CleanupRenderSurface(this);

    renderBuffer_ = 0;
}

void RenderSurface::Release()
{
    Graphics* graphics = parentTexture_->GetGraphics();
    if (!graphics)
        return;

    if (!graphics->IsDeviceLost())
    {
        for (unsigned i = 0; i < MAX_RENDERTARGETS; ++i)
        {
            if (graphics->GetRenderTarget(i) == this)
                graphics->ResetRenderTarget(i);
        }

        if (graphics->GetDepthStencil() == this)
            graphics->ResetDepthStencil();

        // Clean up also from non-active FBOs
        graphics->CleanupRenderSurface(this);

        if (renderBuffer_)
            glDeleteRenderbuffers(1, &renderBuffer_);
    }

    renderBuffer_ = 0;
}
void RenderSurface::SetNumViewports(unsigned num)
{
    d->viewports_.resize(num);
}

void RenderSurface::SetViewport(unsigned index, Viewport* viewport)
{
    if (index >= d->viewports_.size())
        d->viewports_.resize(index + 1);

    d->viewports_[index] = viewport;
}

void RenderSurface::SetUpdateMode(RenderSurfaceUpdateMode mode)
{
    updateMode_ = mode;
}

void RenderSurface::SetLinkedRenderTarget(RenderSurface* renderTarget)
{
    if (renderTarget != this)
        d->linkedRenderTarget_ = renderTarget;
}

void RenderSurface::SetLinkedDepthStencil(RenderSurface* depthStencil)
{
    if (depthStencil != this)
        d->linkedDepthStencil_ = depthStencil;
}

void RenderSurface::QueueUpdate()
{
    updateQueued_ = true;
}

/// Reset update queued flag. Called internally.
void RenderSurface::ResetUpdateQueued()
{
    updateQueued_ = false;
}

int RenderSurface::GetWidth() const
{
    return parentTexture_->GetWidth();
}

int RenderSurface::GetHeight() const
{
    return parentTexture_->GetHeight();
}

TextureUsage RenderSurface::GetUsage() const
{
    return parentTexture_->GetUsage();
}

int RenderSurface::GetMultiSample() const
{
    return parentTexture_->GetMultiSample();
}

bool RenderSurface::GetAutoResolve() const
{
    return parentTexture_->GetAutoResolve();
}

unsigned RenderSurface::GetNumViewports() const
{
    return d->viewports_.size();
}

Viewport* RenderSurface::GetViewport(unsigned index) const
{
    return index < d->viewports_.size() ? d->viewports_[index] : nullptr;
}

RenderSurface *RenderSurface::GetLinkedRenderTarget() const
{
    return d->linkedRenderTarget_;
}

RenderSurface *RenderSurface::GetLinkedDepthStencil() const
{
    return d->linkedDepthStencil_;
}
}
