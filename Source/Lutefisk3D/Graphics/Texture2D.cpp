//
// Copyright (c) 2008-2017 the Urho3D project.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rightsR
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

#include "Texture2D.h"

#include "Graphics.h"
#include "GraphicsEvents.h"
#include "GraphicsImpl.h"
#include "Renderer.h"
#include "RenderSurface.h"

#include <glbinding/gl33core/functions.h>
#include "Lutefisk3D/Core/Context.h"
#include "Lutefisk3D/Core/Profiler.h"
#include "Lutefisk3D/IO/FileSystem.h"
#include "Lutefisk3D/IO/Log.h"
#include "Lutefisk3D/Resource/Image.h"
#include "Lutefisk3D/Resource/ResourceCache.h"
#include "Lutefisk3D/Resource/XMLFile.h"

using namespace gl;
namespace Urho3D
{
template class SharedPtr<Texture2D>;

Texture2D::Texture2D(Context* context) :
    Texture(context)
{
    target_ = gl::GL_TEXTURE_2D;
}

Texture2D::~Texture2D()
{
    Release();
}

void Texture2D::RegisterObject(Context* context)
{
    context->RegisterFactory<Texture2D>();
}

/// Load resource from stream. May be called from a worker thread. Return true if successful.
bool Texture2D::BeginLoad(Deserializer& source)
{
    // In headless mode, do not actually load the texture, just return success
    if (!graphics_)
        return true;

    // If device is lost, retry later
    if (graphics_->IsDeviceLost())
    {
        URHO3D_LOGWARNING("Texture load while device is lost");
        dataPending_ = true;
        return true;
    }

    // Load the image data for EndLoad()
    loadImage_ = new Image(context_);
    if (!loadImage_->Load(source))
    {
        loadImage_.Reset();
        return false;
    }

    // Precalculate mip levels if async loading
    if (GetAsyncLoadState() == ASYNC_LOADING)
        loadImage_->PrecalculateLevels();

    // Load the optional parameters file
    QString xmlName = ReplaceExtension(GetName(), ".xml");
    loadParameters_ = context_->m_ResourceCache->GetTempResource<XMLFile>(xmlName, false);

    return true;
}

/// Finish resource loading. Always called from the main thread. Return true if successful.
bool Texture2D::EndLoad()
{
    // In headless mode, do not actually load the texture, just return success
    if (!graphics_ || graphics_->IsDeviceLost())
        return true;

    // If over the texture budget, see if materials can be freed to allow textures to be freed
    CheckTextureBudget(GetTypeStatic());

    SetParameters(loadParameters_);
    bool success = SetData(loadImage_);

    loadImage_.Reset();
    loadParameters_.Reset();

    return success;
}
/// Mark the GPU resource destroyed on context destruction.
void Texture2D::OnDeviceLost()
{
    GPUObject::OnDeviceLost();

    if (renderSurface_)
        renderSurface_->OnDeviceLost();
}
/// Recreate the GPU resource and restore data if applicable.
void Texture2D::OnDeviceReset()
{
    if (!object_ || dataPending_)
    {
        // If has a resource file, reload through the resource cache. Otherwise just recreate.
        ResourceCache* cache = context_->m_ResourceCache.get();
        if (cache->Exists(GetName()))
            dataLost_ = !cache->ReloadResource(this);

        if (!object_)
        {
            Create();
            dataLost_ = true;
        }
    }

    dataPending_ = false;
}

void Texture2D::Release()
{
    if (object_)
    {
        if (!graphics_)
            return;

        if (!graphics_->IsDeviceLost())
        {
            for (unsigned i = 0; i < MAX_TEXTURE_UNITS; ++i)
            {
                if (graphics_->GetTexture(i) == this)
                    graphics_->SetTexture(i, nullptr);
            }

            glDeleteTextures(1, &object_);
        }

        if (renderSurface_)
            renderSurface_->Release();

        object_ = 0;
    }
    else
    {
        if (renderSurface_)
            renderSurface_->Release();
    }
    resolveDirty_ = false;
    levelsDirty_ = false;
}
/// Set data either partially or fully on a mip level. Return true if successful.
bool Texture2D::SetData(unsigned level, int x, int y, int width, int height, const void* data)
{
    URHO3D_PROFILE_CTX(context_,SetTextureData);

    if (!object_ || !graphics_)
    {
        URHO3D_LOGERROR("No texture created, can not set data");
        return false;
    }

    if (!data)
    {
        URHO3D_LOGERROR("Null source for setting data");
        return false;
    }

    if (level >= levels_)
    {
        URHO3D_LOGERROR("Illegal mip level for setting data");
        return false;
    }

    if (graphics_->IsDeviceLost())
    {
        URHO3D_LOGWARNING("Texture data assignment while device is lost");
        dataPending_ = true;
        return true;
    }

    if (IsCompressed())
    {
        x &= ~3;
        y &= ~3;
    }

    int levelWidth = GetLevelWidth(level);
    int levelHeight = GetLevelHeight(level);
    if (x < 0 || x + width > levelWidth || y < 0 || y + height > levelHeight || width <= 0 || height <= 0)
    {
        URHO3D_LOGERROR("Illegal dimensions for setting data");
        return false;
    }

    graphics_->SetTextureForUpdate(this);

    bool wholeLevel = x == 0 && y == 0 && width == levelWidth && height == levelHeight;
    gl::GLenum format = GetSRGB() ? GetSRGBFormat(format_) : format_;

    if (!IsCompressed())
    {
        if (wholeLevel)
            glTexImage2D(target_, level, format, width, height, 0, GetExternalFormat(format_), GetDataType(format_), data);
        else
            glTexSubImage2D(target_, level, x, y, width, height, GetExternalFormat(format_), GetDataType(format_), data);
    }
    else
    {
        if (wholeLevel)
            glCompressedTexImage2D(target_, level, format, width, height, 0, GetDataSize(width, height), data);
        else
            glCompressedTexSubImage2D(target_, level, x, y, width, height, format, GetDataSize(width, height), data);
    }

    graphics_->SetTexture(0, nullptr);
    return true;
}
/// Set data from an image. Return true if successful. Optionally make a single channel image alpha-only.
bool Texture2D::SetData(Urho3D::Image *image, bool useAlpha)
{
    if (!image)
    {
        URHO3D_LOGERROR("Null image, can not set data");
        return false;
    }
    // Use a shared ptr for managing the temporary mip images created during this function
    SharedPtr<Image> mipImage;
    unsigned memoryUse = sizeof(Texture2D);

    int quality = QUALITY_HIGH;
    Renderer* renderer = context_->m_Renderer.get();
    if (renderer)
        quality = renderer->GetTextureQuality();

    if (!image->IsCompressed())
    {
        // Convert unsuitable formats to RGBA
        unsigned components = image->GetComponents();
        if (((components == 1 && !useAlpha) || components == 2))
        {
            mipImage = image->ConvertToRGBA();
            image = mipImage;
            if (!image)
                return false;
            components = image->GetComponents();
        }
        unsigned char* levelData = image->GetData();
        int levelWidth = image->GetWidth();
        int levelHeight = image->GetHeight();
        GLenum format = GL_NONE;

        // Discard unnecessary mip levels
        for (unsigned i = 0; i < mipsToSkip_[quality]; ++i)
        {
            mipImage = image->GetNextLevel();
            image = mipImage;
            levelData = image->GetData();
            levelWidth = image->GetWidth();
            levelHeight = image->GetHeight();
        }

        switch (components)
        {
            case 1:
                format = useAlpha ? Graphics::GetAlphaFormat() : Graphics::GetLuminanceFormat();
                break;

            case 2:
                format = Graphics::GetLuminanceAlphaFormat();
                break;

            case 3:
                format = Graphics::GetRGBFormat();
                break;

            case 4:
                format = Graphics::GetRGBAFormat();
                break;

            default:
                assert(false);  // Should not reach here
                break;
        }

        // If image was previously compressed, reset number of requested levels to avoid error if level count is too high for new size
        if (IsCompressed() && requestedLevels_ > 1)
            requestedLevels_ = 0;
        SetSize(levelWidth, levelHeight, format);
        if (!object_)
            return false;

        for (unsigned i = 0; i < levels_; ++i)
        {
            SetData(i, 0, 0, levelWidth, levelHeight, levelData);
            memoryUse += levelWidth * levelHeight * components;

            if (i < levels_ - 1)
            {
                mipImage = image->GetNextLevel();
                image = mipImage;
                levelData = image->GetData();
                levelWidth = image->GetWidth();
                levelHeight = image->GetHeight();
            }
        }
    }
    else
    {
        int width = image->GetWidth();
        int height = image->GetHeight();
        unsigned levels = image->GetNumCompressedLevels();
        GLenum format = graphics_->GetFormat(image->GetCompressedFormat());
        bool needDecompress = false;

        if (GL_NONE==format)
        {
            format = Graphics::GetRGBAFormat();
            needDecompress = true;
        }

        unsigned mipsToSkip = mipsToSkip_[quality];
        if (mipsToSkip >= levels)
            mipsToSkip = levels - 1;
        while (mipsToSkip && (width / (1 << mipsToSkip) < 4 || height / (1 << mipsToSkip) < 4))
            --mipsToSkip;
        width /= (1 << mipsToSkip);
        height /= (1 << mipsToSkip);

        SetNumLevels(Max((levels - mipsToSkip), 1U));
        SetSize(width, height, format);

        for (unsigned i = 0; i < levels_ && i < levels - mipsToSkip; ++i)
        {
            CompressedLevel level = image->GetCompressedLevel(i + mipsToSkip);
            if (!needDecompress)
            {
                SetData(i, 0, 0, level.width_, level.height_, level.data_);
                memoryUse += level.rows_ * level.rowSize_;
            }
            else
            {
                unsigned char* rgbaData = new unsigned char[level.width_ * level.height_ * 4];
                level.Decompress(rgbaData);
                SetData(i, 0, 0, level.width_, level.height_, rgbaData);
                memoryUse += level.width_ * level.height_ * 4;
                delete[] rgbaData;
            }
        }
    }

    SetMemoryUse(memoryUse);
    return true;
}
/// Get data from a mip level. The destination buffer must be big enough. Return true if successful.
bool Texture2D::GetData(unsigned level, void* dest) const
{
    if (!object_ || !graphics_)
    {
        URHO3D_LOGERROR("No texture created, can not get data");
        return false;
    }

    if (!dest)
    {
        URHO3D_LOGERROR("Null destination for getting data");
        return false;
    }

    if (level >= levels_)
    {
        URHO3D_LOGERROR("Illegal mip level for getting data");
        return false;
    }

    if (graphics_->IsDeviceLost())
    {
        URHO3D_LOGWARNING("Getting texture data while device is lost");
        return false;
    }
    if (multiSample_ > 1 && !autoResolve_)
    {
        URHO3D_LOGERROR("Can not get data from multisampled texture without autoresolve");
        return false;
    }

    if (resolveDirty_)
        graphics_->ResolveToTexture(const_cast<Texture2D*>(this));

    graphics_->SetTextureForUpdate(const_cast<Texture2D*>(this));

    if (!IsCompressed())
        glGetTexImage(target_, level, GetExternalFormat(format_), GetDataType(format_), dest);
    else
        glGetCompressedTexImage(target_, level, dest);

    graphics_->SetTexture(0, nullptr);
    return true;
}

bool Texture2D::Create()
{
    Release();

    if (!graphics_ || !width_ || !height_)
        return false;

    if (graphics_->IsDeviceLost())
    {
        URHO3D_LOGWARNING("Texture creation while device is lost");
        return true;
    }

    GLenum format = GetSRGB() ? GetSRGBFormat(format_) : format_;
    GLenum externalFormat = GetExternalFormat(format_);
    GLenum dataType = GetDataType(format_);

    // Create a renderbuffer instead of a texture if depth texture is not properly supported, or if this will be a packed
    // depth stencil texture
    if (format == Graphics::GetDepthStencilFormat())
    {
        if (!renderSurface_)
            return false;
        renderSurface_->CreateRenderBuffer(width_, height_, format, multiSample_);
        return true;
    }
    else
    {
        if (multiSample_ > 1)
        {
            if (autoResolve_)
            {
                // Multisample with autoresolve: create a renderbuffer for rendering, but also a texture
                renderSurface_->CreateRenderBuffer(width_, height_, format, multiSample_);
            }
            else
            {
                // Multisample without autoresolve: create a texture only
                target_ = GL_TEXTURE_2D_MULTISAMPLE;
                if (renderSurface_)
                    renderSurface_->target_ = GL_TEXTURE_2D_MULTISAMPLE;
            }
        }
    }

    glGenTextures(1, &object_);

    // Ensure that our texture is bound to OpenGL texture unit 0
    graphics_->SetTextureForUpdate(this);

    // If not compressed, create the initial level 0 texture with null data
    bool success = true;

    if (!IsCompressed())
    {
        glGetError();
        if (multiSample_ > 1 && !autoResolve_)
            glTexImage2DMultisample(target_, multiSample_, format, width_, height_, GL_TRUE);
        else
            glTexImage2D(target_, 0, format, width_, height_, 0, externalFormat, dataType, nullptr);
        if (glGetError()!=GL_NO_ERROR)
        {
            URHO3D_LOGERROR("Failed to create texture");
            success = false;
        }
    }

    // Set mipmapping
    if (usage_ == TEXTURE_DEPTHSTENCIL)
        requestedLevels_ = 1;
    else if (usage_ == TEXTURE_RENDERTARGET)
    {
        if (requestedLevels_ != 1)
        {
            // Generate levels for the first time now
            RegenerateLevels();
            // Determine max. levels automatically
            requestedLevels_ = 0;
        }
    }
    levels_ = CheckMaxLevels(width_, height_, requestedLevels_);

    glTexParameteri(target_, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(target_, GL_TEXTURE_MAX_LEVEL, levels_ - 1);

    // Set initial parameters, then unbind the texture
    UpdateParameters();
    graphics_->SetTexture(0, nullptr);

    return success;
}

/** Set size, format, usage and multisampling parameters for rendertargets. Zero size will follow application window size. Return true if successful.
    Autoresolve true means the multisampled texture will be automatically resolved to 1-sample after being rendered to and before being sampled as a texture.
    Autoresolve false means the multisampled texture will be read as individual samples in the shader and is not supported on Direct3D9.
*/
bool Texture2D::SetSize(int width, int height, gl::GLenum format, TextureUsage usage, int multiSample, bool autoResolve)
{
    if (width <= 0 || height <= 0)
    {
        URHO3D_LOGERROR("Zero or negative texture dimensions");
        return false;
    }

    multiSample = Clamp(multiSample, 1, 16);
    if (multiSample == 1)
        autoResolve = false;
    else if (multiSample > 1 && usage < TEXTURE_RENDERTARGET)
    {
        URHO3D_LOGERROR("Multisampling is only supported for rendertarget or depth-stencil textures");
        return false;
    }

    // Disable mipmaps if multisample & custom resolve
    if (multiSample > 1 && autoResolve == false)
        requestedLevels_ = 1;
    // Delete the old rendersurface if any
    renderSurface_.Reset();

    usage_ = usage;

    if (usage >= TEXTURE_RENDERTARGET)
    {
        renderSurface_ = new RenderSurface(this);

        // Clamp mode addressing by default and nearest filtering
        addressMode_[COORD_U] = ADDRESS_CLAMP;
        addressMode_[COORD_V] = ADDRESS_CLAMP;
        filterMode_ = FILTER_NEAREST;
    }

    if (usage == TEXTURE_RENDERTARGET)
        g_graphicsSignals.renderSurfaceUpdate.Connect(this,&Texture2D::HandleRenderSurfaceUpdate);
    else
        g_graphicsSignals.renderSurfaceUpdate.Disconnect(this,&Texture2D::HandleRenderSurfaceUpdate);

    width_ = width;
    height_ = height;
    format_ = format;
    depth_ = 1;
    multiSample_ = multiSample;
    autoResolve_ = autoResolve;

    return Create();
}

SharedPtr<Image> Texture2D::GetImage() const
{
    if (format_ != Graphics::GetRGBAFormat() && format_ != Graphics::GetRGBFormat())
    {
        URHO3D_LOGERROR("Unsupported texture format, can not convert to Image");
        return SharedPtr<Image>();
    }

    Image* rawImage = new Image(context_);
    if (format_ == Graphics::GetRGBAFormat())
        rawImage->SetSize(width_, height_, 4);
    else if (format_ == Graphics::GetRGBFormat())
        rawImage->SetSize(width_, height_, 3);
    else
        assert(0);

    GetData(0, rawImage->GetData());
    return SharedPtr<Image>(rawImage);
}

void Texture2D::HandleRenderSurfaceUpdate()
{
    if (renderSurface_ && (renderSurface_->GetUpdateMode() == SURFACE_UPDATEALWAYS || renderSurface_->IsUpdateQueued()))
    {
        if (context_->m_Renderer)
            context_->m_Renderer->QueueRenderSurface(renderSurface_);
        renderSurface_->ResetUpdateQueued();
    }
}

}
