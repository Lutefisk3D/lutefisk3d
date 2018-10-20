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
#include "Texture.h"

#include "Graphics.h"
#include "GraphicsImpl.h"
#include "Material.h"
#include "Lutefisk3D/Core/StringUtils.h"
#include "Lutefisk3D/Core/Context.h"
#include "Lutefisk3D/IO/FileSystem.h"
#include "Lutefisk3D/IO/Log.h"
#include "Lutefisk3D/Resource/ResourceCache.h"
#include "Lutefisk3D/Resource/XMLFile.h"

#include <GL/glew.h>

namespace std {

template<> struct hash<Urho3D::TextureUnit>
{
    size_t operator()(Urho3D::TextureUnit v) const {
        return std::hash<unsigned>()(v);
    }
};

}
namespace Urho3D
{
static const char* addressModeNames[] =
{
    "wrap",
    "mirror",
    "clamp",
    "border",
    nullptr
};

static const char* filterModeNames[] =
{
    "nearest",
    "bilinear",
    "trilinear",
    "anisotropic",
    "nearestanisotropic",
    "default",
    nullptr
};

Texture::Texture(Context* context) :
    ResourceWithMetadata(context),
    GPUObject(context->m_Graphics.get()),
    target_(GL_NONE),
    format_(GL_NONE),
    usage_(TEXTURE_STATIC),
    levels_(0),
    requestedLevels_(0),
    width_(0),
    height_(0),
    depth_(0),
    shadowCompare_(false),
    filterMode_(FILTER_DEFAULT),
    anisotropy_(0),
    multiSample_(1),
    sRGB_(false),
    parametersDirty_(true),
    autoResolve_(false),
    resolveDirty_(false),
    levelsDirty_(false)
{
    for (int i = 0; i < MAX_COORDS; ++i)
        addressMode_[i] = ADDRESS_WRAP;
    for (int i = 0; i < MAX_TEXTURE_QUALITY_LEVELS; ++i)
        mipsToSkip_[i] = (unsigned)(MAX_TEXTURE_QUALITY_LEVELS - 1 - i);
}

Texture::~Texture()
{
}
/**
 * \brief Set number of requested mip levels. Needs to be called before setting size.
 *
 * The default value (0) allocates as many mip levels as necessary to reach 1x1 size. Set value 1 to disable mipmapping.
 * \note that rendertargets need to regenerate mips dynamically after rendering, which may cost performance.
 * Screen buffers and shadow maps allocated by Renderer will have mipmaps disabled.
 */
void Texture::SetNumLevels(unsigned levels)
{
    if (usage_ > TEXTURE_RENDERTARGET)
        requestedLevels_ = 1;
    else
        requestedLevels_ = levels;
}

void Texture::SetFilterMode(TextureFilterMode mode)
{
    filterMode_ = mode;
    parametersDirty_ = true;
}

void Texture::SetAddressMode(TextureCoordinate coord, TextureAddressMode mode)
{
    addressMode_[coord] = mode;
    parametersDirty_ = true;
}

void Texture::SetAnisotropy(unsigned level)
{
    anisotropy_ = level;
    parametersDirty_ = true;
}

void Texture::SetShadowCompare(bool enable)
{
    shadowCompare_ = enable;
    parametersDirty_ = true;
}

void Texture::SetBorderColor(const Color& color)
{
    borderColor_ = color;
    parametersDirty_ = true;
}

void Texture::SetBackupTexture(Texture* texture)
{
    backupTexture_ = texture;
}

void Texture::SetMipsToSkip(int quality, int toSkip)
{
    if (quality >= QUALITY_LOW && quality < MAX_TEXTURE_QUALITY_LEVELS)
    {
        mipsToSkip_[quality] = (unsigned)toSkip;

        // Make sure a higher quality level does not actually skip more mips
        for (int i = 1; i < MAX_TEXTURE_QUALITY_LEVELS; ++i)
        {
            if (mipsToSkip_[i] > mipsToSkip_[i - 1])
                mipsToSkip_[i] = mipsToSkip_[i - 1];
        }
    }
}

int Texture::GetMipsToSkip(int quality) const
{
    return (quality >= QUALITY_LOW && quality < MAX_TEXTURE_QUALITY_LEVELS) ? mipsToSkip_[quality] : 0;
}

int Texture::GetLevelWidth(unsigned level) const
{
    if (level > levels_)
        return 0;
    return Max(width_ >> level, 1);
}

int Texture::GetLevelHeight(unsigned level) const
{
    if (level > levels_)
        return 0;
    return Max(height_ >> level, 1);
}

int Texture::GetLevelDepth(unsigned level) const
{
    if (level > levels_)
        return 0;
    return Max(depth_ >> level, 1);
}

unsigned Texture::GetDataSize(int width, int height) const
{
    if (IsCompressed())
        return GetRowDataSize(width) * ((height + 3) >> 2);
    else
        return GetRowDataSize(width) * height;
}

unsigned Texture::GetDataSize(int width, int height, int depth) const
{
    return depth * GetDataSize(width, height);
}

unsigned Texture::GetComponents() const
{
    if (!width_ || IsCompressed())
        return 0;
    else
        return GetRowDataSize(width_) / width_;
}

void Texture::SetParameters(XMLFile* file)
{
    if (!file)
        return;

    XMLElement rootElem = file->GetRoot();
    SetParameters(rootElem);
}

void Texture::SetParameters(const XMLElement& element)
{
    LoadMetadataFromXML(element);
    for (XMLElement paramElem = element.GetChild(); paramElem; paramElem = paramElem.GetNext())
    {
        QString name = paramElem.GetName();

        if (name == "address")
        {
            QString coord = paramElem.GetAttributeLower("coord");
            if (!coord.isEmpty())
            {
                TextureCoordinate coordIndex = (TextureCoordinate)(coord[0].toLatin1() - 'u');
                QString mode = paramElem.GetAttributeLower("mode");
                SetAddressMode(coordIndex, (TextureAddressMode)GetStringListIndex(mode, addressModeNames, ADDRESS_WRAP));
            }
        }

        if (name == "border")
            SetBorderColor(paramElem.GetColor("color"));

        if (name == "filter")
        {
            QString mode = paramElem.GetAttributeLower("mode");
            SetFilterMode((TextureFilterMode)GetStringListIndex(mode, filterModeNames, FILTER_DEFAULT));
            if (paramElem.HasAttribute("anisotropy"))
                SetAnisotropy(paramElem.GetUInt("anisotropy"));
        }
        if (name == "mipmap")
            SetNumLevels(paramElem.GetBool("enable") ? 0 : 1);

        if (name == "quality")
        {
            if (paramElem.HasAttribute("low"))
                SetMipsToSkip(QUALITY_LOW, paramElem.GetInt("low"));
            if (paramElem.HasAttribute("med"))
                SetMipsToSkip(QUALITY_MEDIUM, paramElem.GetInt("med"));
            if (paramElem.HasAttribute("medium"))
                SetMipsToSkip(QUALITY_MEDIUM, paramElem.GetInt("medium"));
            if (paramElem.HasAttribute("high"))
                SetMipsToSkip(QUALITY_HIGH, paramElem.GetInt("high"));
        }

        if (name == "srgb")
            SetSRGB(paramElem.GetBool("enable"));
    }
}

void Texture::SetParametersDirty()
{
    parametersDirty_ = true;
}

void Texture::SetLevelsDirty()
{
    if (usage_ == TEXTURE_RENDERTARGET && levels_ > 1)
        levelsDirty_ = true;
}

unsigned Texture::CheckMaxLevels(int width, int height, unsigned requestedLevels)
{
    unsigned maxLevels = 1;
    while (width > 1 || height > 1)
    {
        ++maxLevels;
        width = width > 1 ? (width >> 1) : 1;
        height = height > 1 ? (height >> 1) : 1;
    }

    if (!requestedLevels || maxLevels < requestedLevels)
        return maxLevels;
    else
        return requestedLevels;
}

unsigned Texture::CheckMaxLevels(int width, int height, int depth, unsigned requestedLevels)
{
    unsigned maxLevels = 1;
    while (width > 1 || height > 1 || depth > 1)
    {
        ++maxLevels;
        width = width > 1 ? (width >> 1) : 1;
        height = height > 1 ? (height >> 1) : 1;
        depth = depth > 1 ? (depth >> 1) : 1;
    }

    if (!requestedLevels || maxLevels < requestedLevels)
        return maxLevels;
    else
        return requestedLevels;
}

void Texture::CheckTextureBudget(StringHash type)
{
    ResourceCache* cache = context_->resourceCache();
    uint64_t textureBudget = cache->GetMemoryBudget(type);
    uint64_t textureUse = cache->GetMemoryUse(type);
    if (!textureBudget)
        return;

    // If textures are over the budget, they likely can not be freed directly as materials still refer to them.
    // Therefore free unused materials first
    if (textureUse > textureBudget)
        cache->ReleaseResources(Material::GetTypeStatic());
}

}
