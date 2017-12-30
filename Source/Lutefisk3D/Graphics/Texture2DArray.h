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

#pragma once

#include "Lutefisk3D/Container/Ptr.h"
#include "Lutefisk3D/Graphics/Texture.h"

namespace Urho3D
{

class LUTEFISK3D_EXPORT Deserializer;
class LUTEFISK3D_EXPORT Image;
class LUTEFISK3D_EXPORT RenderSurface;
extern template class SharedPtr<XMLFile>;

/// 2D texture array resource.
class LUTEFISK3D_EXPORT Texture2DArray : public Texture
{
    URHO3D_OBJECT(Texture2DArray, Texture)

public:
    /// Construct.
    Texture2DArray(Context* context);
    /// Destruct.
    virtual ~Texture2DArray();
    /// Register object factory.
    static void RegisterObject(Context* context);

    /// Load resource from stream. May be called from a worker thread. Return true if successful.
    virtual bool BeginLoad(Deserializer& source) override;
    /// Finish resource loading. Always called from the main thread. Return true if successful.
    virtual bool EndLoad() override;
    /// Mark the GPU resource destroyed on context destruction.
    virtual void OnDeviceLost() override;
    /// Recreate the GPU resource and restore data if applicable.
    virtual void OnDeviceReset() override;
    /// Release the texture.
    virtual void Release() override;

    /// Set the number of layers in the texture. To be used before SetData.
    void SetLayers(unsigned layers);
    /// Set layers, size, format and usage. Set layers to zero to leave them unchanged. Return true if successful.
    bool SetSize(unsigned layers, int width, int height, uint32_t format, TextureUsage usage = TEXTURE_STATIC);
    /// Set data either partially or fully on a layer's mip level. Return true if successful.
    bool SetData(unsigned layer, unsigned level, int x, int y, int width, int height, const void* data);
    /// Set data of one layer from a stream. Return true if successful.
    bool SetData(unsigned layer, Deserializer& source);
    /// Set data of one layer from an image. Return true if successful. Optionally make a single channel image alpha-only.
    bool SetData(unsigned layer, Image *image, bool useAlpha = false);

    /// Return number of layers in the texture.
    unsigned GetLayers() const { return layers_; }
    /// Get data from a mip level. The destination buffer must be big enough. Return true if successful.
    bool GetData(unsigned layer, unsigned level, void* dest) const;
    /// Return render surface.
    RenderSurface* GetRenderSurface() const { return renderSurface_; }

protected:
    /// Create the GPU texture.
    virtual bool Create() override;

private:
    /// Handle render surface update event.
    void HandleRenderSurfaceUpdate();

    /// Texture array layers number.
    unsigned layers_;
    /// Render surface.
    SharedPtr<RenderSurface> renderSurface_;
    /// Memory use per layer.
    std::vector<unsigned> layerMemoryUse_;
    /// Layer image files acquired during BeginLoad.
    std::vector<SharedPtr<Image> > loadImages_;
    /// Parameter file acquired during BeginLoad.
    SharedPtr<XMLFile> loadParameters_;
};

}
