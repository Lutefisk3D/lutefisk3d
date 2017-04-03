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
#include "../Texture.h"

#include "../Graphics.h"
#include "../GraphicsImpl.h"
#include "../Material.h"
#include "../RenderSurface.h"

#include "../../Resource/ResourceCache.h"
#include "../../Resource/XMLFile.h"
#include "../../IO/FileSystem.h"
#include "../../IO/Log.h"
#include "../../Core/StringUtils.h"
#include "../../Core/Profiler.h"

using namespace gl;

namespace Urho3D
{

static GLenum glWrapModes[] =
{
    GL_REPEAT,
    GL_MIRRORED_REPEAT,
    GL_CLAMP_TO_EDGE,
    GL_CLAMP
};
static GLenum gl3WrapModes[] =
{
    GL_REPEAT,
    GL_MIRRORED_REPEAT,
    GL_CLAMP_TO_EDGE,
    GL_CLAMP_TO_BORDER
};

static GLenum GetWrapMode(TextureAddressMode mode)
{
    return gl3WrapModes[mode];
}

void Texture::SetSRGB(bool enable)
{
    if (graphics_)
        enable &= graphics_->GetSRGBSupport();

    if (enable != sRGB_)
    {
        sRGB_ = enable;
        // If texture had already been created, must recreate it to set the sRGB texture format
        if (object_)
            Create();

        // If texture in use in the framebuffer, mark it dirty
        if (graphics_ && graphics_->GetRenderTarget(0) && graphics_->GetRenderTarget(0)->GetParentTexture() == this)
            graphics_->MarkFBODirty();
    }
}

void Texture::UpdateParameters()
{
    if (!object_ || !graphics_)
        return;

    // If texture is multisampled, do not attempt to set parameters as it's illegal, just return
    if (target_ == GL_TEXTURE_2D_MULTISAMPLE)
    {
        parametersDirty_ = false;
        return;
    }
    // Wrapping
    glTexParameteri(target_, GL_TEXTURE_WRAP_S, (GLint)GetWrapMode(addressMode_[COORD_U]));
    glTexParameteri(target_, GL_TEXTURE_WRAP_T, (GLint)GetWrapMode(addressMode_[COORD_V]));
    glTexParameteri(target_, GL_TEXTURE_WRAP_R, (GLint)GetWrapMode(addressMode_[COORD_W]));

    TextureFilterMode filterMode = filterMode_;
    if (filterMode == FILTER_DEFAULT)
        filterMode = graphics_->GetDefaultTextureFilterMode();

    // Filtering
    switch (filterMode)
    {
    case FILTER_NEAREST:
        glTexParameteri(target_, GL_TEXTURE_MIN_FILTER, (GLint)GL_NEAREST);
        glTexParameteri(target_, GL_TEXTURE_MAG_FILTER, (GLint)GL_NEAREST);
        break;

    case FILTER_BILINEAR:
        if (levels_ < 2)
            glTexParameteri(target_, GL_TEXTURE_MIN_FILTER, (GLint)GL_LINEAR);
        else
            glTexParameteri(target_, GL_TEXTURE_MIN_FILTER, (GLint)GL_LINEAR_MIPMAP_NEAREST);
        glTexParameteri(target_, GL_TEXTURE_MAG_FILTER, (GLint)GL_LINEAR);
        break;

    case FILTER_ANISOTROPIC:
    case FILTER_TRILINEAR:
        if (levels_ < 2)
            glTexParameteri(target_, GL_TEXTURE_MIN_FILTER, (GLint)GL_LINEAR);
        else
            glTexParameteri(target_, GL_TEXTURE_MIN_FILTER, (GLint)GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(target_, GL_TEXTURE_MAG_FILTER, (GLint)GL_LINEAR);
        break;

    default:
        break;
    }

    // Anisotropy
    if (graphics_->GetAnisotropySupport())
    {
        unsigned maxAnisotropy = anisotropy_ ? anisotropy_ : graphics_->GetDefaultTextureAnisotropy();
        glTexParameterf(target_, GL_TEXTURE_MAX_ANISOTROPY_EXT,
            filterMode == FILTER_ANISOTROPIC ? (float)maxAnisotropy : 1.0f);
    }

    // Shadow compare
    if (shadowCompare_)
    {
        glTexParameteri(target_, GL_TEXTURE_COMPARE_MODE, (GLint)GL_COMPARE_R_TO_TEXTURE);
        glTexParameteri(target_, GL_TEXTURE_COMPARE_FUNC, (GLint)GL_LEQUAL);
    }
    else
        glTexParameteri(target_, GL_TEXTURE_COMPARE_MODE, (GLint)GL_NONE);

    glTexParameterfv(target_, GL_TEXTURE_BORDER_COLOR, borderColor_.Data());

    parametersDirty_ = false;
}

bool Texture::GetParametersDirty() const
{
    return parametersDirty_;
}

bool Texture::IsCompressed() const
{
    return format_ == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT || format_ == GL_COMPRESSED_RGBA_S3TC_DXT3_EXT ||
        format_ == GL_COMPRESSED_RGBA_S3TC_DXT5_EXT ;
}

unsigned Texture::GetRowDataSize(int width) const
{
    switch (format_)
    {
    case GL_ALPHA:
    case GL_LUMINANCE:
        return (unsigned)width;

    case GL_LUMINANCE_ALPHA:
        return (unsigned)(width * 2);

    case GL_RGB:
        return (unsigned)(width * 3);

    case GL_RGBA:
    case GL_DEPTH24_STENCIL8_EXT:
    case GL_RG16:
    case GL_RG16F:
    case GL_R32F:
        return (unsigned)(width * 4);

    case GL_R8:
        return (unsigned)width;

    case GL_RG8:
    case GL_R16F:
        return (unsigned)(width * 2);
    case GL_RGBA16:
    case GL_RGBA16F_ARB:
        return (unsigned)(width * 8);

    case GL_RGBA32F_ARB:
        return (unsigned)(width * 16);

    case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
        return (unsigned)(((width + 3) >> 2) * 8);

    case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
        return (unsigned)(((width + 3) >> 2) * 16);
    default:
        return 0;
    }
}
gl::GLenum Texture::GetExternalFormat(GLenum format)
{
    if (format == GL_DEPTH_COMPONENT16 || format == GL_DEPTH_COMPONENT24 || format == GL_DEPTH_COMPONENT32)
        return GL_DEPTH_COMPONENT;
    else if (format == GL_DEPTH24_STENCIL8_EXT)
        return GL_DEPTH_STENCIL_EXT;
    else if (format == GL_SLUMINANCE_EXT)
        return GL_LUMINANCE;
    else if (format == GL_SLUMINANCE_ALPHA_EXT)
        return GL_LUMINANCE_ALPHA;
    else if (format == GL_R8 || format == GL_R16F || format == GL_R32F)
        return GL_RED;
    else if (format == GL_RG8 || format == GL_RG16 || format == GL_RG16F || format == GL_RG32F)
        return GL_RG;
    else if (format == GL_RGBA16 || format == GL_RGBA16F_ARB || format == GL_RGBA32F_ARB || format == GL_SRGB_ALPHA_EXT)
        return GL_RGBA;
    else if (format == GL_SRGB_EXT)
        return GL_RGB;
    else
        return format;
}

gl::GLenum Texture::GetDataType(GLenum format)
{
    if (format == GL_DEPTH24_STENCIL8_EXT)
        return GL_UNSIGNED_INT_24_8_EXT;
    else if (format == GL_RG16 || format == GL_RGBA16)
        return GL_UNSIGNED_SHORT;
    else if (format == GL_RGBA32F_ARB || format == GL_RG32F || format == GL_R32F)
        return GL_FLOAT;
    else if (format == GL_RGBA16F_ARB || format == GL_RG16F || format == GL_R16F)
        return GL_HALF_FLOAT_ARB;
    else
        return GL_UNSIGNED_BYTE;
}

gl::GLenum Texture::GetSRGBFormat(gl::GLenum format)
{
    if (!graphics_ || !graphics_->GetSRGBSupport())
        return format;

    switch (format)
    {
    case GL_RGB:
        return GL_SRGB_EXT;
    case GL_RGBA:
        return GL_SRGB_ALPHA_EXT;
    case GL_LUMINANCE:
        return GL_SLUMINANCE_EXT;
    case GL_LUMINANCE_ALPHA:
        return GL_SLUMINANCE_ALPHA_EXT;
    case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
        return GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT;
    case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
        return GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT;
    case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
        return GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT;
    default:
        return format;
    }
}

}
