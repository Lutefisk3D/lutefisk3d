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

#pragma once


#include "../Graphics/Viewport.h"
#include "../Scene/Component.h"


namespace Urho3D
{

static const Rect fullScreenViewport{0, 0, 1, 1};

URHO3D_EVENT(E_CAMERAVIEWPORTRESIZED, CameraViewportResized)
{
    URHO3D_PARAM(P_CAMERA, Camera);                             // Camera pointer
    URHO3D_PARAM(P_VIEWPORT, Viewport);                         // Viewport pointer
    URHO3D_PARAM(P_SIZENORM, SizeNorm);                         // Rect
    URHO3D_PARAM(P_SIZE, Size);                                 // IntRect
}

class LUTEFISK3D_EXPORT CameraViewport
    : public Component
{
    URHO3D_OBJECT(CameraViewport, Component)
public:
    /// Construct.
    explicit CameraViewport(Context* context);
    /// Returns normalized viewport rect.
    Rect GetNormalizedRect() const { return rect_; }
    /// Sets normalized viewport rect.
    void SetNormalizedRect(const Rect& rect);
    /// Set renderpath from resource.
    void SetRenderPath(const ResourceRef& renderPathFile);
    /// Returns last renderpath that was set to this component. If viewport is modified directly - modification will not be reflected by return value of this method.
    const ResourceRef& GetLastRenderPath() const { return renderPath_; }
    /// Returns a viewport used for rendering.
    Viewport* GetViewport() { return viewport_.Get(); }
    /// Builds new renderpath using specified attributes and sets it to the viewport. Returns new renderpath.
    RenderPath* RebuildRenderPath();

    /// Handle scene node being assigned at creation.
    void OnNodeSet(Node* node) override;
    /// Handle scene being assigned. This may happen several times during the component's lifetime.
    /// Scene-wide subsystems and events are subscribed to here.
    void OnSceneSet(Scene* scene) override;

    /// Returns custom list of attributes that are different per instance.
    const std::vector<AttributeInfo>* GetAttributes() const override;

    /// Register object with the engine.
    static void RegisterObject(Context* context);

protected:
    IntVector2 GetScreenSize() const;
    /// Method mimicking Context attribute registration, required for using engine attribute macros for registering
    /// custom per-object attributes.
    template<typename T> AttributeInfo& RegisterAttribute(const AttributeInfo& attr);
    ///
    void RebuildAttributes();

    /// Normalized viewport rectangle.
    Rect rect_;
    /// Viewport used for rendering.
    SharedPtr<Viewport> viewport_;
    /// Current selected renderpath.
    ResourceRef renderPath_;

    /// Flag that triggers rebuilding of attributes.
    bool attributesDirty_ = true;
    /// List of attributes available at the moment.
    std::vector<AttributeInfo> attributes_;
    /// Maping of effect tag to effect file.
    HashMap<QString, QString> effects_;
private:
    void OtherComponentWasAdded(Scene *, Node *n, Component *component);
    void OtherComponentWasRemoved(Scene *, Node *n, Component *component);
};

}
