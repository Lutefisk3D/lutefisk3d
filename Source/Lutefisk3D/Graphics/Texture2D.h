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
#include "Lutefisk3D/Graphics/Texture.h"

namespace Urho3D
{

class Image;
class XMLFile;
class LUTEFISK3D_EXPORT RenderSurface;
/// 2D texture resource.
class LUTEFISK3D_EXPORT Texture2D : public Texture
{
    URHO3D_OBJECT(Texture2D,Texture)

public:
    Texture2D(Context* context);
    virtual ~Texture2D();
    /// Register object factory.
    static void RegisterObject(Context* context);
    bool BeginLoad(Deserializer& source) override;
    bool EndLoad() override;
    void OnDeviceLost() override;
    void OnDeviceReset() override;
    /// Release the texture.
    virtual void Release() override;
    bool SetSize(int width, int height, gl::GLenum format, TextureUsage usage = TEXTURE_STATIC, int multiSample = 1, bool autoResolve = true);
    bool SetData(unsigned level, int x, int y, int width, int height, const void* data);
    bool SetData(Image *image, bool useAlpha = false);
    bool GetData(unsigned level, void* dest) const;
    /// Get image data from zero mip level. Only RGB and RGBA textures are supported.
    SharedPtr<Image> GetImage() const;
    /// Return render surface.
    RenderSurface* GetRenderSurface() const { return renderSurface_; }

protected:
    /// Create the GPU texture.
    virtual bool Create() override;
private:
    /// Handle render surface update event.
    void HandleRenderSurfaceUpdate();

    /// Render surface.
    SharedPtr<RenderSurface> renderSurface_;
    /// Image file acquired during BeginLoad.
    SharedPtr<Image> loadImage_;
    /// Parameter file acquired during BeginLoad.
    SharedPtr<XMLFile> loadParameters_;
};

}
