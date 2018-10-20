//
// Copyright (c) 2018 Rokas Kupstys
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

#include <Lutefisk3D/Scene/Scene.h>
#include <Lutefisk3D/Graphics/Octree.h>
#include <Lutefisk3D/Graphics/Texture2D.h>
#include <Lutefisk3D/Graphics/Graphics.h>
#include <Lutefisk3D/Graphics/Camera.h>
#include <Lutefisk3D/Graphics/DebugRenderer.h>
#include <Lutefisk3D/Graphics/Renderer.h>
#include <Lutefisk3D/Graphics/RenderSurface.h>
#include <Lutefisk3D/Graphics/RenderPath.h>
#include <Toolbox/Scene/DebugCameraController.h>

#include "SceneView.h"

namespace Urho3D
{

SceneView::SceneView(Context* context, const IntRect& rect)
    : rect_(rect)
{
    scene_ = SharedPtr<Scene>(new Scene(context));
    scene_->CreateComponent<Octree>();
    viewport_ = SharedPtr<Viewport>(new Viewport(context, scene_, nullptr));
    viewport_->SetRect(IntRect(IntVector2::ZERO, rect_.Size()));
    CreateObjects();
    texture_ = SharedPtr<Texture2D>(new Texture2D(context));
    // Make sure viewport is not using default renderpath. That would cause issues when renderpath is shared with other
    // viewports (like in resource inspector).
    viewport_->SetRenderPath(viewport_->GetRenderPath()->Clone());
    SetSize(rect);
}

void SceneView::SetSize(const IntRect& rect)
{
    if (rect_ == rect)
        return;

    rect_ = rect;
    viewport_->SetRect(IntRect(IntVector2::ZERO, rect.Size()));
    texture_->SetSize(rect.Width(), rect.Height(), Graphics::GetRGBFormat(), TEXTURE_RENDERTARGET);
    texture_->GetRenderSurface()->SetViewport(0, viewport_);
    texture_->GetRenderSurface()->SetUpdateMode(SURFACE_UPDATEALWAYS);
}

void SceneView::CreateObjects()
{
    camera_ = WeakPtr<Node>(scene_->GetChild("EditorCamera", true));
    if (camera_.Expired())
    {
        camera_ = scene_->CreateChild("EditorCamera", LOCAL, FIRST_INTERNAL_ID, true);
        camera_->CreateComponent<Camera>()->setFarClipDistance(160000);
        camera_->AddTag("__EDITOR_OBJECT__");
        camera_->SetTemporary(true);
    }
    auto* debug = scene_->GetComponent<DebugRenderer>();
    if (debug == nullptr)
    {
        debug = scene_->CreateComponent<DebugRenderer>(LOCAL, FIRST_INTERNAL_ID);
        debug->SetTemporary(true);
    }
    debug->SetView(GetCamera());
    debug->SetTemporary(true);
    viewport_->SetCamera(GetCamera());
}

Camera* SceneView::GetCamera() const
{
    if (camera_.Null())
        return nullptr;

    return camera_->GetComponent<Camera>();
}

}
