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

#include "Lutefisk3D/Container/RefCounted.h"
#include "Lutefisk3D/Container/Ptr.h"
#include <memory>

namespace Urho3D
{
class Viewport;
class Texture;
enum RenderSurfaceUpdateMode : uint8_t;
enum TextureUsage : uint8_t;
struct RenderSurfacePrivate;
/// %Color or depth-stencil surface that can be rendered into.
class LUTEFISK3D_EXPORT RenderSurface : public RefCounted
{
    friend class Texture2D;
    friend class Texture2DArray;
    friend class TextureCube;

public:
    /// Construct with parent texture.
    RenderSurface(Texture* parentTexture);
    /// Destruct.
    ~RenderSurface();

    /// Set number of viewports.
    void SetNumViewports(unsigned num);
    /// Set viewport.
    void SetViewport(unsigned index, Viewport* viewport);
    /// Set viewport update mode. Default is to update when visible.
    void SetUpdateMode(RenderSurfaceUpdateMode mode);
    /// Set linked color rendertarget.
    void SetLinkedRenderTarget(RenderSurface* renderTarget);
    /// Set linked depth-stencil surface.
    void SetLinkedDepthStencil(RenderSurface* depthStencil);
    /// Queue manual update of the viewport(s).
    void QueueUpdate();
    /// Release surface.
    void Release();
    /// Mark the GPU resource destroyed on graphics context destruction. Only used on OpenGL.
    void OnDeviceLost();
    /// Create renderbuffer that cannot be sampled as a texture. Only used on OpenGL.
    bool CreateRenderBuffer(unsigned width, unsigned height, uint32_t format, int multiSample);

    /// Return width.
    int GetWidth() const;
    /// Return height.
    int GetHeight() const;
    /// Return usage.
    TextureUsage GetUsage() const;
    /// Return multisampling level.
    int GetMultiSample() const;

    /// Return multisampling autoresolve mode.
    bool GetAutoResolve() const;
    /// Return number of viewports.
    unsigned GetNumViewports() const;
    /// Return viewport by index.
    Viewport* GetViewport(unsigned index) const;
    /// Return viewport update mode.
    RenderSurfaceUpdateMode GetUpdateMode() const { return updateMode_; }
    /// Return linked color rendertarget.
    RenderSurface* GetLinkedRenderTarget() const;
    /// Return linked depth-stencil surface.
    RenderSurface* GetLinkedDepthStencil() const;

    /// Return whether manual update queued. Called internally.
    bool IsUpdateQueued() const { return updateQueued_; }
    void ResetUpdateQueued();

    /// Return parent texture.
    Texture* GetParentTexture() const { return parentTexture_; }

    /// Return surface's OpenGL target.
    uint32_t GetTarget() const { return target_; }

    /// Return OpenGL renderbuffer if created.
    unsigned GetRenderBuffer() const { return renderBuffer_; }

    /// Return whether multisampled rendertarget needs resolve.
    bool IsResolveDirty() const { return resolveDirty_; }

    /// Set or clear the need resolve flag. Called internally by Graphics.
    void SetResolveDirty(bool enable) { resolveDirty_ = enable; }
private:
    std::unique_ptr<RenderSurfacePrivate> d;
    /// Parent texture.
    Texture* parentTexture_;
    /// OpenGL renderbuffer name.
    unsigned renderBuffer_=0;
    /// OpenGL target.
    uint32_t target_;
    /// Update mode for viewports.
    RenderSurfaceUpdateMode updateMode_;
    /// Update queued flag.
    bool updateQueued_=false;
    /// Multisampled resolve dirty flag.
    bool resolveDirty_;
};

}
