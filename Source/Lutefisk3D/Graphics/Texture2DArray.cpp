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
#include "Texture2DArray.h"

#include "Graphics.h"
#include "GraphicsEvents.h"
#include "GraphicsImpl.h"
#include "Renderer.h"
#include "RenderSurface.h"

#include "Lutefisk3D/Core/Context.h"
#include "Lutefisk3D/Core/Profiler.h"
#include "Lutefisk3D/IO/FileSystem.h"
#include "Lutefisk3D/IO/Log.h"
#include "Lutefisk3D/Resource/ResourceCache.h"
#include "Lutefisk3D/Resource/XMLFile.h"
#include "Lutefisk3D/Resource/Image.h"

#include <GL/glew.h>
namespace Urho3D
{

Texture2DArray::Texture2DArray(Context* context) :
    Texture(context),
    layers_(0)
{
    target_ = GL_TEXTURE_2D_ARRAY;
}

Texture2DArray::~Texture2DArray()
{
    Release();
}

void Texture2DArray::RegisterObject(Context* context)
{
    context->RegisterFactory<Texture2DArray>();
}

bool Texture2DArray::BeginLoad(Deserializer& source)
{
    ResourceCache* cache = context_->resourceCache();

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

    cache->ResetDependencies(this);

    QString texPath, texName, texExt;
    SplitPath(GetName(), texPath, texName, texExt);

    loadParameters_ = (new XMLFile(context_));
    if (!loadParameters_->Load(source))
    {
        loadParameters_.Reset();
        return false;
    }

    loadImages_.clear();

    XMLElement textureElem = loadParameters_->GetRoot();
    XMLElement layerElem = textureElem.GetChild("layer");
    while (layerElem)
    {
        QString name = layerElem.GetAttribute("name");

        // If path is empty, add the XML file path
        if (GetPath(name).isEmpty())
            name = texPath + name;

        loadImages_.push_back(cache->GetTempResource<Image>(name));
        cache->StoreResourceDependency(this, name);

        layerElem = layerElem.GetNext("layer");
    }

    // Precalculate mip levels if async loading
    if (GetAsyncLoadState() == ASYNC_LOADING)
    {
        for (unsigned i = 0; i < loadImages_.size(); ++i)
        {
            if (loadImages_[i])
                loadImages_[i]->PrecalculateLevels();
        }
    }

    return true;
}

bool Texture2DArray::EndLoad()
{
    // In headless mode, do not actually load the texture, just return success
    if (!graphics_ || graphics_->IsDeviceLost())
        return true;

    // If over the texture budget, see if materials can be freed to allow textures to be freed
    CheckTextureBudget(GetTypeStatic());

    SetParameters(loadParameters_);
    SetLayers(loadImages_.size());

    for (unsigned i = 0; i < loadImages_.size(); ++i)
        SetData(i, loadImages_[i]);

    loadImages_.clear();
    loadParameters_.Reset();

    return true;
}

void Texture2DArray::SetLayers(unsigned layers)
{
    Release();

    layers_ = layers;
}

bool Texture2DArray::SetSize(unsigned layers, int width, int height, uint32_t format, TextureUsage usage)
{
    if (width <= 0 || height <= 0)
    {
        URHO3D_LOGERROR("Zero or negative texture array size");
        return false;
    }
    if (usage == TEXTURE_DEPTHSTENCIL)
    {
        URHO3D_LOGERROR("Depth-stencil usage not supported for texture arrays");
        return false;
    }

    // Delete the old rendersurface if any
    renderSurface_.Reset();

    usage_ = usage;

    if (usage == TEXTURE_RENDERTARGET)
    {
        renderSurface_ = new RenderSurface(this);

        // Nearest filtering by default
        filterMode_ = FILTER_NEAREST;
    }

    if (usage == TEXTURE_RENDERTARGET)
        g_graphicsSignals.renderSurfaceUpdate.Connect(this,&Texture2DArray::HandleRenderSurfaceUpdate);
    else
        g_graphicsSignals.renderSurfaceUpdate.Disconnect(this,&Texture2DArray::HandleRenderSurfaceUpdate);

    width_ = width;
    height_ = height;
    format_ = format;
    depth_ = 1;
    if (layers)
        layers_ = layers;

    layerMemoryUse_.resize(layers_);
    for (unsigned i = 0; i < layers_; ++i)
        layerMemoryUse_[i] = 0;

    return Create();
}

void Texture2DArray::HandleRenderSurfaceUpdate()
{
    if (renderSurface_ && (renderSurface_->GetUpdateMode() == SURFACE_UPDATEALWAYS || renderSurface_->IsUpdateQueued()))
    {
        if (context_->m_Renderer)
            context_->m_Renderer->QueueRenderSurface(renderSurface_);
        renderSurface_->ResetUpdateQueued();
    }
}

}
