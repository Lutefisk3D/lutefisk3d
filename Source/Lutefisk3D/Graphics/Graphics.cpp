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
#include "Graphics.h"

#include "AnimatedModel.h"
#include "Animation.h"
#include "AnimationController.h"
#include "Camera.h"
#include "CustomGeometry.h"
#include "DebugRenderer.h"
#include "DecalSet.h"
#include "GraphicsImpl.h"
#include "Material.h"
#include "Octree.h"
#include "Light.h"
#include "ParticleEffect.h"
#include "ParticleEmitter.h"
#include "RibbonTrail.h"
#include "Shader.h"
#include "ShaderPrecache.h"
#include "Skybox.h"
#include "StaticModelGroup.h"
#include "Technique.h"
#include "Terrain.h"
#include "TerrainPatch.h"
#include "Texture2D.h"
#include "Texture2DArray.h"
#include "Texture3D.h"
#include "TextureCube.h"
#include "Zone.h"

#include "Lutefisk3D/Core/StringUtils.h"
#include "Lutefisk3D/Core/Profiler.h"
#include "Lutefisk3D/Core/Context.h"
#include "Lutefisk3D/IO/FileSystem.h"
#include "Lutefisk3D/IO/Log.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>

namespace Urho3D
{
//////////////////////////////////////////////////////////
/// Template instantiation
//////////////////////////////////////////////////////////
template class WeakPtr<Graphics>;

void Graphics::SetExternalWindow(void* window)
{
    if (!window_)
        externalWindow_ = window;
    else
        URHO3D_LOGERROR("Window already opened, can not set external window");
}

void Graphics::SetWindowTitle(const QString& windowTitle)
{
    windowTitle_ = windowTitle;
    if (window_)
        SDL_SetWindowTitle(window_, qPrintable(windowTitle_));
}

void Graphics::SetWindowIcon(Image* windowIcon)
{
    windowIcon_ = windowIcon;
    if (window_)
        CreateWindowIcon();
}

void Graphics::SetWindowPosition(const IntVector2& position)
{
    if (window_)
        SDL_SetWindowPosition(window_, position.x_, position.y_);
    else
        position_ = position; // Sets as initial position for OpenWindow()
}

void Graphics::SetWindowPosition(int x, int y)
{
    SetWindowPosition(IntVector2(x, y));
}

void Graphics::SetOrientations(const QString& orientations)
{
    orientations_ = orientations.trimmed();
    SDL_SetHint(SDL_HINT_ORIENTATIONS, qPrintable(orientations_));
}

bool Graphics::ToggleFullscreen()
{
    return SetMode(width_, height_, !fullscreen_, borderless_, resizable_, highDPI_, vsync_, tripleBuffer_, multiSample_, monitor_, refreshRate_);
}

void Graphics::SetShaderParameter(StringHash param, const Variant& value)
{
    switch (value.GetType())
    {
    case VAR_BOOL:
        SetShaderParameter(param, value.GetBool());
        break;

    case VAR_INT:
        SetShaderParameter(param, value.GetInt());
        break;

    case VAR_FLOAT:
    case VAR_DOUBLE:
        SetShaderParameter(param, value.GetFloat());
        break;

    case VAR_VECTOR2:
        SetShaderParameter(param, value.GetVector2());
        break;

    case VAR_VECTOR3:
        SetShaderParameter(param, value.GetVector3());
        break;

    case VAR_VECTOR4:
        SetShaderParameter(param, value.GetVector4());
        break;

    case VAR_COLOR:
        SetShaderParameter(param, value.GetColor());
        break;

    case VAR_MATRIX3:
        SetShaderParameter(param, value.GetMatrix3());
        break;

    case VAR_MATRIX3X4:
        SetShaderParameter(param, value.GetMatrix3x4());
        break;

    case VAR_MATRIX4:
        SetShaderParameter(param, value.GetMatrix4());
        break;

    case VAR_BUFFER:
        {
            const std::vector<uint8_t>& buffer = value.GetBuffer();
            if (buffer.size() >= sizeof(float))
                SetShaderParameter(param, reinterpret_cast<const float*>(buffer.data()), buffer.size() / sizeof(float));
        }
        break;

    default:
        // Unsupported parameter type, do nothing
        break;
    }
}

IntVector2 Graphics::GetWindowPosition() const
{
    if (window_)
        return position_;
    return IntVector2::ZERO;
}

std::vector<IntVector3> Graphics::GetResolutions(int monitor) const
{
    std::vector<IntVector3> ret;
    unsigned numModes = (unsigned)SDL_GetNumDisplayModes(monitor);

    for (unsigned i = 0; i < numModes; ++i)
    {
        SDL_DisplayMode mode;
        SDL_GetDisplayMode(monitor, i, &mode);
        int width = mode.w;
        int height  = mode.h;
        int rate = mode.refresh_rate;

        // Store mode if unique
        bool unique = true;
        for (unsigned j = 0; j < ret.size(); ++j)
        {
            if (ret[j].x_ == width && ret[j].y_ == height && ret[j].z_ == rate)
            {
                unique = false;
                break;
            }
        }

        if (unique)
            ret.emplace_back(width, height, rate);
    }

    return ret;
}
IntVector2 Graphics::GetDesktopResolution(int monitor) const
{
    SDL_DisplayMode mode;
    SDL_GetDesktopDisplayMode(monitor, &mode);
    return IntVector2(mode.w, mode.h);
}

int Graphics::GetMonitorCount() const
{
    return SDL_GetNumVideoDisplays();
}

void Graphics::Maximize()
{
    if (!window_)
        return;

    SDL_MaximizeWindow(window_);
}

void Graphics::Minimize()
{
    if (!window_)
        return;

    SDL_MinimizeWindow(window_);
}

void Graphics::BeginDumpShaders(const QString& fileName)
{
    shaderPrecache_ = new ShaderPrecache(m_context, fileName);
}

void Graphics::EndDumpShaders()
{
    shaderPrecache_.Reset();
}

void Graphics::PrecacheShaders(Deserializer& source)
{
    URHO3D_PROFILE_CTX(m_context,PrecacheShaders);

    ShaderPrecache::LoadShaders(this, source);
}

void Graphics::SetShaderCacheDir(const QString& path)
{
    QString trimmedPath = path.trimmed();
    if (!trimmedPath.isEmpty())
        shaderCacheDir_ = AddTrailingSlash(trimmedPath);
}

void Graphics::AddGPUObject(GPUObject* object)
{
    MutexLock lock(gpuObjectMutex_);

    gpuObjects_.push_back(object);
}

void Graphics::RemoveGPUObject(GPUObject* object)
{
    MutexLock lock(gpuObjectMutex_);
    if(gpuObjects_.empty())
    {
        // this might happen if Graphics subsystem is shutting down.
        return;
    }
    auto iter = std::find(gpuObjects_.begin(),gpuObjects_.end(),object);
    if(iter==gpuObjects_.end()) {
        URHO3D_LOGDEBUG("Graphics::RemoveGPUObject called multiple times on same object");
    }

    gpuObjects_.erase(iter);
}

void* Graphics::ReserveScratchBuffer(unsigned size)
{
    if (!size)
        return nullptr;

    if (size > maxScratchBufferRequest_)
        maxScratchBufferRequest_ = size;

    // First check for a free buffer that is large enough
    for (ScratchBuffer & elem : scratchBuffers_)
    {
        if (!elem.reserved_ && elem.size_ >= size)
        {
            elem.reserved_ = true;
            return elem.data_.get();
        }
    }

    // Then check if a free buffer can be resized
    for (ScratchBuffer & elem : scratchBuffers_)
    {
        if (!elem.reserved_)
        {
            elem.data_.reset(new uint8_t[size]);
            elem.size_ = size;
            elem.reserved_ = true;
            URHO3D_LOGDEBUG("Resized scratch buffer to size " + QString::number(size));
            return elem.data_.get();
        }
    }

    // Finally allocate a new buffer
    ScratchBuffer newBuffer{std::unique_ptr<uint8_t[]>(new uint8_t[size]), size, true};
    scratchBuffers_.emplace_back(std::move(newBuffer));
    URHO3D_LOGDEBUG("Allocated scratch buffer with size " + QString::number(size));
    return scratchBuffers_.back().data_.get();
}

void Graphics::FreeScratchBuffer(void* buffer)
{
    if (!buffer)
        return;

    for (ScratchBuffer & elem : scratchBuffers_)
    {
        if (elem.reserved_ && elem.data_.get() == buffer)
        {
            elem.reserved_ = false;
            return;
        }
    }

    URHO3D_LOGWARNING("Reserved scratch buffer " + ToStringHex((unsigned)(size_t)buffer) + " not found");
}

void Graphics::CleanupScratchBuffers()
{
    for (ScratchBuffer & elem : scratchBuffers_)
    {
        if (!elem.reserved_ && elem.size_ > maxScratchBufferRequest_ * 2 && elem.size_ >= 1024 * 1024)
        {
            elem.data_.reset(maxScratchBufferRequest_ > 0 ? new uint8_t[maxScratchBufferRequest_] : nullptr);
            elem.size_ = maxScratchBufferRequest_;
            URHO3D_LOGDEBUG("Resized scratch buffer to size " + QString::number(maxScratchBufferRequest_));
        }
    }

    maxScratchBufferRequest_ = 0;
}



void Graphics::CreateWindowIcon()
{
    if (windowIcon_)
    {
        SDL_Surface* surface = windowIcon_->GetSDLSurface();
        if (surface)
        {
            SDL_SetWindowIcon(window_, surface);
            SDL_FreeSurface(surface);
        }
    }
}

void RegisterGraphicsLibrary(Context* context)
{
    Animation::RegisterObject(context);
    Material::RegisterObject(context);
    Model::RegisterObject(context);
    Shader::RegisterObject(context);
    Technique::RegisterObject(context);
    Texture2D::RegisterObject(context);
    Texture2DArray::RegisterObject(context);
    Texture3D::RegisterObject(context);
    TextureCube::RegisterObject(context);
    Camera::RegisterObject(context);
    Drawable::RegisterObject(context);
    Light::RegisterObject(context);
    StaticModel::RegisterObject(context);
    StaticModelGroup::RegisterObject(context);
    Skybox::RegisterObject(context);
    AnimatedModel::RegisterObject(context);
    AnimationController::RegisterObject(context);
    BillboardSet::RegisterObject(context);
    ParticleEffect::RegisterObject(context);
    ParticleEmitter::RegisterObject(context);
    RibbonTrail::RegisterObject(context);
    CustomGeometry::RegisterObject(context);
    DecalSet::RegisterObject(context);
    Terrain::RegisterObject(context);
    TerrainPatch::RegisterObject(context);
    DebugRenderer::RegisterObject(context);
    Octree::RegisterObject(context);
    Zone::RegisterObject(context);
}

}
