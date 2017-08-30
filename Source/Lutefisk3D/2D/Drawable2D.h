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
#include "Lutefisk3D/Graphics/GraphicsDefs.h"

namespace Urho3D
{

class Drawable2D;
class Renderer2D;
class Texture2D;
class VertexBuffer;

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
    friend class SourceBatch2D_Manager;
private:
    SourceBatch2D() = default;
public:
    WeakPtr<Drawable2D>   owner_;            ///< Owner.
    mutable float         distance_  = 0.0f; ///< Distance to camera.
    int                   drawOrder_ = 0;    ///< Draw order.
    SharedPtr<Material>   material_;         ///< Material.
    std::vector<Vertex2D> vertices_;         ///< Vertices.
};
using SourceBatch2D_Handle = DataHandle<SourceBatch2D,20,20>;
class SourceBatch2D_Manager {

public:
    SourceBatch2D_Handle create();
    void release(SourceBatch2D_Handle h);
    SourceBatch2D &get(SourceBatch2D_Handle h) const;
};
extern SourceBatch2D_Manager source2DBatchManger;
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


    virtual void OnSetEnabled() override;
    void SetLayer(int layer);
    void SetOrderInLayer(int orderInLayer);
    /// Return layer.
    int GetLayer() const { return layer_; }
    /// Return order in layer.
    int GetOrderInLayer() const { return orderInLayer_; }
    const std::vector<SourceBatch2D> &GetSourceBatches();

protected:

    virtual void OnSceneSet(Scene* scene) override;
    virtual void OnMarkedDirty(Node* node) override;
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
