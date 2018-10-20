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

#include "Lutefisk3D/Container/ArrayPtr.h"
#include "Lutefisk3D/Resource/Resource.h"
#include "Lutefisk3D/Math/Rect.h"
#include "Lutefisk3D/Math/Color.h"
struct SDL_Surface;
extern "C" {
struct GLFWimage;
}
namespace Urho3D
{

static const int COLOR_LUT_SIZE = 16;

/// Supported compressed image formats.
enum CompressedFormat : unsigned
{
    CF_NONE = 0,
    CF_RGBA,
    CF_DXT1,
    CF_DXT3,
    CF_DXT5,
    CF_ETC1,
    CF_PVRTC_RGB_2BPP,
    CF_PVRTC_RGBA_2BPP,
    CF_PVRTC_RGB_4BPP,
    CF_PVRTC_RGBA_4BPP,
};

enum class ImageSet : uint8_t {
    SINGLE,
    ARRAY,   //!< Texture array status if DDS.
    CUBEMAP, //!< Cubemap status if DDS.
};
/// Compressed image mip level.
struct CompressedLevel
{
    /// Decompress to RGBA. The destination buffer required is width * height * 4 bytes. Return true if successful.
    bool Decompress(uint8_t* dest);
    uint8_t *        data_      = nullptr; //!< Compressed image data.
    CompressedFormat format_    = CF_NONE; //!< Compression format.
    int              width_     = 0;       //!< Width.
    int              height_    = 0;       //!< Height.
    int              depth_     = 0;       //!< Depth.
    unsigned         blockSize_ = 0;       //!< Block size in bytes.
    unsigned         dataSize_  = 0;       //!< Total data size in bytes.
    unsigned         rowSize_   = 0;       //!< Row size in bytes.
    unsigned         rows_      = 0;       //!< Number of rows.
};

/// %Image resource.
class LUTEFISK3D_EXPORT Image : public Resource
{
    URHO3D_OBJECT(Image, Resource)

public:
    Image(Context* context);
    virtual ~Image();

    static void RegisterObject(Context* context);

    virtual bool BeginLoad(Deserializer& source) override;
    virtual bool Save(Serializer& dest) const override;

    bool SetSize(int width, int height, unsigned components);
    bool SetSize(int width, int height, int depth, unsigned components);
    void SetData(const uint8_t* pixelData);
    void SetPixel(int x, int y, const Color& color);
    void SetPixel(int x, int y, int z, const Color& color);
    void SetPixelInt(int x, int y, unsigned uintColor);
    void SetPixelInt(int x, int y, int z, unsigned uintColor);
    bool LoadColorLUT(Deserializer& source);
    bool FlipHorizontal();
    bool FlipVertical();
    bool Resize(int width, int height);
    void Clear(const Color& color);
    void ClearInt(unsigned uintColor);
    bool SaveBMP(const QString& fileName) const;
    bool SavePNG(const QString& fileName) const;
    bool SaveJPG(const QString& fileName, int quality) const;
    /// Whether this texture is detected as a cubemap, only relevant for DDS.
    bool IsCubemap() const { return imageset_==ImageSet::CUBEMAP; }
    /// Whether this texture has been detected as a volume, only relevant for DDS.
    bool IsArray() const { return imageset_==ImageSet::ARRAY; }
    /// Whether this texture is in sRGB, only relevant for DDS.
    bool IsSRGB() const { return sRGB_; }


    Color GetPixel(int x, int y) const;
    Color GetPixel(int x, int y, int z) const;
    unsigned GetPixelInt(int x, int y) const;
    unsigned GetPixelInt(int x, int y, int z) const;
    Color GetPixelBilinear(float x, float y) const;
    Color GetPixelTrilinear(float x, float y, float z) const;
    /// Return width.
    int GetWidth() const { return width_; }
    /// Return height.
    int GetHeight() const { return height_; }
    /// Return depth.
    int GetDepth() const { return depth_; }
    /// Return number of color components.
    unsigned GetComponents() const { return components_; }
    /// Return pixel data.
    uint8_t* GetData() const { return data_.get(); }
    /// Return whether is compressed.
    bool IsCompressed() const { return compressedFormat_ != CF_NONE; }
    /// Return compressed format.
    CompressedFormat GetCompressedFormat() const { return compressedFormat_; }
    /// Return number of compressed mip levels. Returns 0 if the image is has not been loaded from a source file containing multiple mip levels.
    unsigned GetNumCompressedLevels() const { return numCompressedLevels_; }
    SharedPtr<Image> GetNextLevel() const;
    /// Return the next sibling image of an array or cubemap.
    SharedPtr<Image> GetNextSibling() const { return nextSibling_;  }
    SharedPtr<Image> ConvertToRGBA() const;
    CompressedLevel GetCompressedLevel(unsigned index) const;
    Image* GetSubimage(const IntRect& rect) const;

    SDL_Surface *GetSDLSurface(const IntRect &rect = IntRect::ZERO) const;
    std::unique_ptr<GLFWimage> GetGLFWImage(const IntRect& rect = IntRect::ZERO) const;
    void PrecalculateLevels();
    /// Whether this texture has an alpha channel
    bool HasAlphaChannel() const;
    /// Copy contents of the image into the defined rect, scaling if necessary. This image should already be large enough to include the rect. Compressed and 3D images are not supported.
    bool SetSubimage(const Image* image, const IntRect& rect);
    /// Clean up the mip levels.
    void CleanupLevels();
    void GetLevels(std::vector<Image *> &levels);
    void GetLevels(std::vector<const Image *> &levels) const;

private:
    bool saveImageCommon(const QString &fileName, const char *format, int quality = -1) const;
    static uint8_t *GetImageData(Deserializer &source, int &width, int &height, unsigned &components);
    static void FreeImageData(uint8_t *pixelData);

    int                        width_               = 0;       ///< Width.
    int                        height_              = 0;       ///< Height.
    int                        depth_               = 0;       ///< Depth.
    unsigned                   components_          = 0;       ///< Number of color components.
    unsigned                   numCompressedLevels_ = 0;       ///< Number of compressed mip levels.
    CompressedFormat           compressedFormat_    = CF_NONE; ///< Compressed format.
    std::unique_ptr<uint8_t[]> data_;                ///< Pixel data.
    SharedPtr<Image>           nextLevel_;           ///< Precalculated mip level image.
    SharedPtr<Image>           nextSibling_;         ///< Next texture array or cube map image.
    ImageSet                   imageset_=ImageSet::SINGLE;
    bool                       sRGB_    = false;               ///< Data is sRGB.
};
}
