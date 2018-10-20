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
#include "Viewport.h"

#include "Camera.h"
#include "Graphics.h"
#include "Lutefisk3D/IO/Log.h"
#include "Renderer.h"
#include "RenderPath.h"
#include "Lutefisk3D/Resource/ResourceCache.h"
#include "Lutefisk3D/Scene/Scene.h"
#include "View.h"
#include "Lutefisk3D/Resource/XMLFile.h"
#include "Lutefisk3D/Core/Context.h"



namespace Urho3D
{

Viewport::Viewport(Context* context) :
    Object(context),
    rect_(IntRect::ZERO),
    drawDebug_(true)
{
    SetRenderPath((RenderPath*)nullptr);
}

Viewport::Viewport(Context* context, Scene* scene, Camera* camera, RenderPath* renderPath) :
    Object(context),
    scene_(scene),
    camera_(camera),
    rect_(IntRect::ZERO),
    drawDebug_(true)
{
    SetRenderPath(renderPath);
}

Viewport::Viewport(Context* context, Scene* scene, Camera* camera, const IntRect& rect, RenderPath* renderPath) :
    Object(context),
    scene_(scene),
    camera_(camera),
    rect_(rect),
    drawDebug_(true)
{
    SetRenderPath(renderPath);
}

Viewport::~Viewport() = default;


    void Viewport::SetScene(Scene* scene)
{
    scene_ = scene;
}

void Viewport::SetCamera(Camera* camera)
{
    camera_ = camera;
}

void Viewport::SetCullCamera(Camera* camera)
{
    cullCamera_ = camera;
}

void Viewport::SetRect(const IntRect& rect)
{
    rect_ = rect;
}

void Viewport::SetDrawDebug(bool enable)
{
    drawDebug_ = enable;
}

void Viewport::SetRenderPath(RenderPath* renderPath)
{
    if (renderPath)
        renderPath_ = renderPath;
    else
    {
        Renderer* renderer = context_->m_Renderer.get();
        if (renderer)
            renderPath_ = renderer->GetDefaultRenderPath();
    }
}

bool Viewport::SetRenderPath(XMLFile* file)
{
    SharedPtr<RenderPath> newRenderPath(new RenderPath());
    if (newRenderPath->Load(file))
    {
        renderPath_ = newRenderPath;
        return true;
    }
    return false;
}

Scene* Viewport::GetScene() const
{
    return scene_;
}

Camera* Viewport::GetCamera() const
{
    return camera_;
}

Camera* Viewport::GetCullCamera() const
{
    return cullCamera_;
}

View* Viewport::GetView() const
{
    return view_;
}

RenderPath* Viewport::GetRenderPath() const
{
    return renderPath_;
}

Ray Viewport::GetScreenRay(int x, int y) const
{
    if (!camera_)
        return Ray();

    float screenX;
    float screenY;

    if (rect_ == IntRect::ZERO)
    {
        Graphics* graphics = context_->m_Graphics.get();
        screenX = (float)x / (float)graphics->GetWidth();
        screenY = (float)y / (float)graphics->GetHeight();
    }
    else
    {
        screenX = float(x - rect_.left_) / (float)rect_.Width();
        screenY = float(y - rect_.top_) / (float)rect_.Height();
    }

    return camera_->GetScreenRay(screenX, screenY);
}

IntVector2 Viewport::WorldToScreenPoint(const Vector3& worldPos) const
{
    if (!camera_)
        return IntVector2::ZERO;

    Vector2 screenPoint = Urho3D::WorldToScreenPoint(*camera_,worldPos);

    int x;
    int y;
    if (rect_ == IntRect::ZERO)
    {
        /// \todo This is incorrect if the viewport is used on a texture rendertarget instead of the backbuffer, as it may have different dimensions.
        Graphics* graphics = context_->m_Graphics.get();
        x = (int)(screenPoint.x_ * graphics->GetWidth());
        y = (int)(screenPoint.y_ * graphics->GetHeight());
    }
    else
    {
        x = (int)(rect_.left_ + screenPoint.x_ * rect_.Width());
        y = (int)(rect_.top_ + screenPoint.y_ * rect_.Height());
    }

    return IntVector2(x, y);
}

Vector3 Viewport::ScreenToWorldPoint(int x, int y, float depth) const
{
    if (!camera_)
        return Vector3::ZERO;

    float screenX;
    float screenY;

    if (rect_ == IntRect::ZERO)
    {
        /// \todo This is incorrect if the viewport is used on a texture rendertarget instead of the backbuffer, as it may have different dimensions.
        Graphics* graphics = context_->m_Graphics.get();
        screenX = (float)x / (float)graphics->GetWidth();
        screenY = (float)y / (float)graphics->GetHeight();
    }
    else
    {
        screenX = float(x - rect_.left_) / (float)rect_.Width();
        screenY = float(y - rect_.top_) / (float)rect_.Height();
    }

    return Urho3D::ScreenToWorldPoint(*camera_,Vector3(screenX, screenY, depth));
}

void Viewport::AllocateView()
{
    view_ = new View(context_);
}

}
