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


#include "View3D.h"
#include "UI.h"
#include "UIEvents.h"
#include "Lutefisk3D/Core/Context.h"
#include "Lutefisk3D/Graphics/Camera.h"
#include "Lutefisk3D/Graphics/Graphics.h"
#include "Lutefisk3D/Graphics/GraphicsEvents.h"
#include "Lutefisk3D/Graphics/Octree.h"
#include "Lutefisk3D/Graphics/RenderSurface.h"
#include "Lutefisk3D/Graphics/Texture2D.h"
#include "Lutefisk3D/Graphics/Zone.h"
#include "Lutefisk3D/Graphics/Viewport.h"
#include "Lutefisk3D/Scene/Scene.h"


namespace Urho3D
{

extern const char* UI_CATEGORY;

View3D::View3D(Context* context) :
    Window(context),
    ownScene_(true),
    rttFormat_(Graphics::GetRGBFormat()),
    autoUpdate_(true)
{
    renderTexture_ = new Texture2D(context_);
    depthTexture_ = new Texture2D(context_);
    viewport_ = new Viewport(context_);
    g_graphicsSignals.renderSurfaceUpdate.Connect(this,&View3D::HandleRenderSurfaceUpdate);
}

View3D::~View3D()
{
    ResetScene();
}

void View3D::RegisterObject(Context* context)
{
    context->RegisterFactory<View3D>(UI_CATEGORY);

    URHO3D_COPY_BASE_ATTRIBUTES(Window);
    // The texture format is API specific, so do not register it as a serializable attribute
    URHO3D_ACCESSOR_ATTRIBUTE("Auto Update", GetAutoUpdate, SetAutoUpdate, bool, true, AM_FILE);
    URHO3D_UPDATE_ATTRIBUTE_DEFAULT_VALUE("Clip Children", true);
    URHO3D_UPDATE_ATTRIBUTE_DEFAULT_VALUE("Is Enabled", true);
}

void View3D::OnResize(const IntVector2& newSize, const IntVector2& delta)
{
    int width = newSize.x_;
    int height = newSize.y_;

    if (width > 0 && height > 0)
    {
        renderTexture_->SetSize(width, height, rttFormat_, TEXTURE_RENDERTARGET);
        depthTexture_->SetSize(width, height, Graphics::GetDepthStencilFormat(), TEXTURE_DEPTHSTENCIL);
        RenderSurface* surface = renderTexture_->GetRenderSurface();
        surface->SetViewport(0, viewport_);
        surface->SetUpdateMode(SURFACE_MANUALUPDATE);
        surface->SetLinkedDepthStencil(depthTexture_->GetRenderSurface());

        SetTexture(renderTexture_);
        SetImageRect(IntRect(0, 0, width, height));

        if (!autoUpdate_)
            surface->QueueUpdate();
    }
}

void View3D::SetView(Scene* scene, Camera* camera, bool ownScene)
{
    ResetScene();

    scene_ = scene;
    cameraNode_ = camera ? camera->GetNode() : nullptr;
    ownScene_ = ownScene;

    viewport_->SetScene(scene_);
    viewport_->SetCamera(camera);
    QueueUpdate();
}

void View3D::SetFormat(gl::GLenum format)
{
    if (format != rttFormat_)
    {
        rttFormat_ = format;
        OnResize(GetSize(), IntVector2::ZERO);
    }
}

void View3D::SetAutoUpdate(bool enable)
{
    autoUpdate_ = enable;
}

void View3D::QueueUpdate()
{
    RenderSurface* surface = renderTexture_->GetRenderSurface();
    if (surface)
        surface->QueueUpdate();
}

Scene* View3D::GetScene() const
{
    return scene_;
}

Node* View3D::GetCameraNode() const
{
    return cameraNode_;
}

Texture2D* View3D::GetRenderTexture() const
{
    return renderTexture_;
}

Texture2D* View3D::GetDepthTexture() const
{
    return depthTexture_;
}

Viewport* View3D::GetViewport() const
{
    return viewport_;
}

void View3D::ResetScene()
{
    if (!scene_)
        return;

    if (!ownScene_)
    {
        RefCount* refCount = scene_->RefCountPtr();
        ++refCount->refs_;
        scene_ = 0;
        --refCount->refs_;
    }
    else
        scene_ = 0;
}

void View3D::HandleRenderSurfaceUpdate()
{
    if (autoUpdate_ && IsVisibleEffective())
        QueueUpdate();
}

}
