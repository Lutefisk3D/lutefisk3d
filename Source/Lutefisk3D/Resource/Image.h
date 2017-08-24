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

struct SDL_Surface;

namespace Urho3D
{

static const int COLOR_LUT_SIZE = 16;

/// Supported compressed image formats.
enum CompressedFormat
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

/// Compressed image mip level.
struct CompressedLevel
{
    /// Construct empty.
    CompressedLevel() :
        data_(nullptr),
        format_(CF_NONE),
        width_(0),
        height_(0),
        depth_(0),
        blockSize_(0),
        dataSize_(0),
        rowSize_(0),
        rows_(0)
    {
    }

    /// Decompress to RGBA. The destination buffer required is width * height * 4 bytes. Return true if successful.
    bool Decompress(unsigned char* dest);

    /// Compressed image data.
    unsigned char* data_;
    /// Compression format.
    CompressedFormat format_;
    /// Width.
    int width_;
    /// Height.
    int height_;
    /// Depth.
    int depth_;
    /// Block size in bytes.
    unsigned blockSize_;
    /// Total data size in bytes.
    unsigned dataSize_;
    /// Row size in bytes.
    unsigned rowSize_;
    /// Number of rows.
    unsigned rows_;
};

/// %Image resource.
class URHO3D_API Image : public Resource
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
    void SetData(const unsigned char* pixelData);
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
    bool IsCubemap() const { return cubemap_; }
    /// Whether this texture has been detected as a volume, only relevant for DDS.
    bool IsArray() const { return array_; }
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
    unsigned char* GetData() const { return data_; }
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
    void PrecalculateLevels();
    void CleanupLevels();
    void GetLevels(std::vector<Image *> &levels);
    void GetLevels(std::vector<const Image *> &levels) const;

private:
    bool saveImageCommon(const QString &fileName, const char *format, int quality = -1) const;
    static unsigned char *GetImageData(Deserializer &source, int &width, int &height, unsigned &components);
    static void FreeImageData(unsigned char *pixelData);

    int                           width_;               ///< Width.
    int                           height_;              ///< Height.
    int                           depth_;               ///< Depth.
    unsigned                      components_;          ///< Number of color components.
    unsigned                      numCompressedLevels_; ///< Number of compressed mip levels.
    bool                          cubemap_;             ///< Cubemap status if DDS.
    bool                          array_;               ///< Texture array status if DDS.
    bool                          sRGB_;                ///< Data is sRGB.
    CompressedFormat              compressedFormat_;    ///< Compressed format.
    SharedArrayPtr<unsigned char> data_;                ///< Pixel data.
    SharedPtr<Image>              nextLevel_;           ///< Precalculated mip level image.
    SharedPtr<Image>              nextSibling_;         ///< Next texture array or cube map image.
};
}
