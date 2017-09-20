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

#include "Lutefisk3D/Graphics/Drawable.h"

namespace Urho3D
{

class LUTEFISK3D_EXPORT Drawable2D;
class LUTEFISK3D_EXPORT Renderer2D;
class LUTEFISK3D_EXPORT Texture2D;
class LUTEFISK3D_EXPORT VertexBuffer;

/// 2D vertex.
struct Vertex2D
{
    Vector3  position_; ///< Position.
    unsigned color_;    ///< Color.
    Vector2  uv_;       ///< UV.
};

/// 2D source batch.
struct SourceBatch2D
{
    WeakPtr<Drawable2D>   owner_;            ///< Owner.
    mutable float         distance_  = 0.0f; ///< Distance to camera.
    int                   drawOrder_ = 0;    ///< Draw order.
    SharedPtr<Material>   material_;         ///< Material.
    std::vector<Vertex2D> vertices_;         ///< Vertices.
};
/// Pixel size (equal 0.01f).
extern LUTEFISK3D_EXPORT const float PIXEL_SIZE;

/// Base class for 2D visible components.
class LUTEFISK3D_EXPORT Drawable2D : public Drawable
{
    URHO3D_OBJECT(Drawable2D,Drawable)

public:
    Drawable2D(Context* context);
    ~Drawable2D();
    /// Register object factory. Drawable must be registered first.
    static void RegisterObject(Context* context);


    void OnSetEnabled() override;
    void SetLayer(int layer);
    void SetOrderInLayer(int orderInLayer);
    /// Return layer.
    int GetLayer() const { return layer_; }
    /// Return order in layer.
    int GetOrderInLayer() const { return orderInLayer_; }
    const std::vector<SourceBatch2D> &GetSourceBatches();

protected:

    void OnSceneSet(Scene* scene) override;
    void OnMarkedDirty(Node* node) override;
    /// Handle draw order changed.
    virtual void OnDrawOrderChanged() = 0;
    /// Update source batches.
    virtual void UpdateSourceBatches() = 0;
    /// Return draw order by layer and order in layer.
    int GetDrawOrder() const { return (layer_ << 20) + (orderInLayer_ << 10); }

    int                  layer_;              ///< Layer.
    int                  orderInLayer_;       ///< Order in layer.
    std::vector<SourceBatch2D> sourceBatch_;        ///< Source batches.
    bool                 sourceBatchesDirty_; ///< Source batches dirty flag.
    WeakPtr<Renderer2D>  renderer_;           ///< Renderer2D.
};

}
