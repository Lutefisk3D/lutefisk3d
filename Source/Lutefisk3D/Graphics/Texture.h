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

#include "Lutefisk3D/Graphics/GPUObject.h"
#include "Lutefisk3D/Graphics/GraphicsDefs.h"
#include "Lutefisk3D/Math/Color.h"
#include "Lutefisk3D/Resource/Resource.h"

namespace Urho3D
{

static const int MAX_TEXTURE_QUALITY_LEVELS = 3;

class XMLElement;
class XMLFile;

/// Base class for texture resources.
class LUTEFISK3D_EXPORT Texture : public ResourceWithMetadata, public GPUObject, public jl::SignalObserver
{
public:
    Texture(Context* context);
    virtual ~Texture();

    void SetNumLevels(unsigned levels);
    /// Set filtering mode.
    void SetFilterMode(TextureFilterMode filter);
    /// Set addressing mode by texture coordinate.
    void SetAddressMode(TextureCoordinate coord, TextureAddressMode address);
    /// Set texture max. anisotropy level. No effect if not using anisotropic filtering. Value 0 (default) uses the default setting from Renderer.
    void SetAnisotropy(unsigned level);
    /// Set shadow compare mode. Not used on Direct3D9.
    void SetShadowCompare(bool enable);
    /// Set border color for border addressing mode.
    void SetBorderColor(const Color& color);
    /// Set sRGB sampling and writing mode.
    void SetSRGB(bool enable);
    /// Set backup texture to use when rendering to this texture.
    void SetBackupTexture(Texture* texture);
    /// Set mip levels to skip on a quality setting when loading. Ensures higher quality levels do not skip more.
    void SetMipsToSkip(int quality, int toSkip);

    /// Return API-specific texture format.
    uint32_t GetFormat() const { return format_; }
    /// Return whether the texture format is compressed.
    bool IsCompressed() const;
    /// Return number of mip levels.
    unsigned GetLevels() const { return levels_; }
    /// Return width.
    int GetWidth() const { return width_; }
    /// Return height.
    int GetHeight() const { return height_; }
    /// Return depth.
    int GetDepth() const { return depth_; }
    /// Return filtering mode.
    TextureFilterMode GetFilterMode() const { return filterMode_; }
    /// Return addressing mode by texture coordinate.
    TextureAddressMode GetAddressMode(TextureCoordinate coord) const { return addressMode_[coord]; }
    /// Return texture max. anisotropy level. Value 0 means to use the default value from Renderer.
    unsigned GetAnisotropy() const { return anisotropy_; }

    /// Return whether shadow compare is enabled. Not used on Direct3D9.
    bool GetShadowCompare() const { return shadowCompare_; }
     /// Return border color.
    const Color& GetBorderColor() const { return borderColor_; }
    /// Return whether is using sRGB sampling and writing.
    bool GetSRGB() const { return sRGB_; }
    /// Return texture multisampling level (1 = no multisampling).
    int GetMultiSample() const { return multiSample_; }

    /// Return texture multisampling autoresolve mode. When true, the texture is resolved before being sampled on SetTexture(). When false, the texture will not be resolved and must be read as individual samples in the shader.
    bool GetAutoResolve() const { return autoResolve_; }

    /// Return whether multisampled texture needs resolve.
    bool IsResolveDirty() const { return resolveDirty_; }
    /// Return whether rendertarget mipmap levels need regenration.
    bool GetLevelsDirty() const { return levelsDirty_; }
    /// Return backup texture.
    Texture* GetBackupTexture() const { return backupTexture_; }
    /// Return mip levels to skip on a quality setting when loading.
    int GetMipsToSkip(int quality) const;
    /// Return mip level width, or 0 if level does not exist.
    int GetLevelWidth(unsigned level) const;
    /// Return mip level width, or 0 if level does not exist.
    int GetLevelHeight(unsigned level) const;
    /// Return mip level depth, or 0 if level does not exist.
    int GetLevelDepth(unsigned level) const;
    /// Return texture usage type.
    TextureUsage GetUsage() const { return usage_; }
    /// Return data size in bytes for a rectangular region.
    unsigned GetDataSize(int width, int height) const;
    /// Return data size in bytes for a volume region.
    unsigned GetDataSize(int width, int height, int depth) const;
    /// Return data size in bytes for a pixel or block row.
    unsigned GetRowDataSize(int width) const;
    /// Return number of image components required to receive pixel data from GetData(), or 0 for compressed images.
    unsigned GetComponents() const;
    /// Return whether the parameters are dirty.
    bool GetParametersDirty() const;

    /// Set additional parameters from an XML file.
    void SetParameters(XMLFile* xml);
    /// Set additional parameters from an XML element.
    void SetParameters(const XMLElement& element);
    /// Mark parameters dirty. Called by Graphics.
    void SetParametersDirty();
    /// Update dirty parameters to the texture object. Called by Graphics when assigning the texture.
    void UpdateParameters();

    /// Return texture's target. Only used on OpenGL.
    uint32_t GetTarget() const { return target_; }

    /// Convert format to sRGB. Not used on Direct3D9.
    uint32_t GetSRGBFormat(uint32_t format);
    /// Set or clear the need resolve flag. Called internally by Graphics.
    void SetResolveDirty(bool enable) { resolveDirty_ = enable; }

    /// Set the mipmap levels dirty flag. Called internally by Graphics.
    void SetLevelsDirty();
    /// Regenerate mipmap levels for a rendertarget after rendering and before sampling. Called internally by Graphics. No-op on Direct3D9. On OpenGL the texture must have been bound to work properly.
    void RegenerateLevels();
    /// Check maximum allowed mip levels for a specific texture size.
    static unsigned CheckMaxLevels(int width, int height, unsigned requestedLevels);
    /// Check maximum allowed mip levels for a specific 3D texture size.
    static unsigned CheckMaxLevels(int width, int height, int depth, unsigned requestedLevels);
    /// Return the shader resource view format corresponding to a texture format. Handles conversion of typeless depth texture formats. Only used on Direct3D11.
    static unsigned GetSRVFormat(unsigned format);
    /// Return the depth-stencil view format corresponding to a texture format. Handles conversion of typeless depth texture formats. Only used on Direct3D11.
    static unsigned GetDSVFormat(unsigned format);
    /// Return the non-internal texture format corresponding to an OpenGL internal format.
    static uint32_t GetExternalFormat(uint32_t format);
    /// Return the data type corresponding to an OpenGL internal format.
    static uint32_t GetDataType(uint32_t format);

protected:
    /// Check whether texture memory budget has been exceeded. Free unused materials in that case to release the texture references.
    void CheckTextureBudget(StringHash type);
    /// Create the GPU texture. Implemented in subclasses.
    virtual bool Create() { return true; }

    uint32_t         target_;                       //!< OpenGL target.
    uint32_t         format_;                       //!< Texture format.
    TextureUsage       usage_;                        //!< Texture usage type.
    unsigned           levels_;                       //!< Current mip levels.
    unsigned           requestedLevels_;              //!< Requested mip levels.
    int                width_;                        //!< Texture width.
    int                height_;                       //!< Texture height.
    int                depth_;                        //!< Texture depth.
    bool               shadowCompare_;                //!< Shadow compare mode.
    TextureFilterMode  filterMode_;                   //!< Filtering mode.
    TextureAddressMode addressMode_[MAX_COORDS];      //!< Addressing mode.
    unsigned           anisotropy_;                   //!< Texture anisotropy level.
    unsigned mipsToSkip_[MAX_TEXTURE_QUALITY_LEVELS]; //!< Mip levels to skip when loading per texture quality setting.
    Color    borderColor_;                            //!< Border color.
    int      multiSample_;                            //!< Multisampling level.
    bool     sRGB_;                                   //!< sRGB sampling and writing mode flag.
    bool     parametersDirty_;                        //!< Parameters dirty flag.
    bool     autoResolve_;                            //!< Multisampling autoresolve flag.
    bool     resolveDirty_;                           //!< Multisampling resolve needed -flag.
    bool     levelsDirty_;                            //!< Mipmap levels regeneration needed -flag.
    SharedPtr<Texture> backupTexture_;                //!< Backup texture.
};
}
