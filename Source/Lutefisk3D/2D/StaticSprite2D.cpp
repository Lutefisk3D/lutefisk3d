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
#include "StaticSprite2D.h"

#include "Sprite2D.h"
#include "Renderer2D.h"

#include "Lutefisk3D/Core/Context.h"
#include "Lutefisk3D/Graphics/Material.h"
#include "Lutefisk3D/Graphics/Texture2D.h"
#include "Lutefisk3D/Resource/ResourceCache.h"
#include "Lutefisk3D/Scene/Scene.h"

namespace Urho3D
{

extern const char* URHO2D_CATEGORY;
extern const char* blendModeNames[];

StaticSprite2D::StaticSprite2D(Context* context) :
    Drawable2D(context),
    blendMode_(BLEND_ALPHA),
    flipX_(false),
    flipY_(false),
    color_(Color::WHITE),
    useHotSpot_(false),
    useDrawRect_(false),
    useTextureRect_(false),
    hotSpot_(0.5f, 0.5f),
    drawRect_(Rect::ZERO),
    textureRect_(Rect::ZERO)
{
    //sourceBatch_.owner_ = this;
}

StaticSprite2D::~StaticSprite2D()
{
}

void StaticSprite2D::RegisterObject(Context* context)
{
    //context->RegisterFactory<StaticSprite2D>(URHO2D_CATEGORY);

    URHO3D_ACCESSOR_ATTRIBUTE("Is Enabled", IsEnabled, SetEnabled, bool, true, AM_DEFAULT);
    URHO3D_COPY_BASE_ATTRIBUTES(Drawable2D);
    URHO3D_MIXED_ACCESSOR_ATTRIBUTE("Sprite", GetSpriteAttr, SetSpriteAttr, ResourceRef, ResourceRef(Sprite2D::GetTypeStatic()), AM_DEFAULT);
    URHO3D_ENUM_ACCESSOR_ATTRIBUTE("Blend Mode", GetBlendMode, SetBlendMode, BlendMode, blendModeNames, BLEND_ALPHA, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Flip X", GetFlipX, SetFlipX, bool, false, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Flip Y", GetFlipY, SetFlipY, bool, false, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Color", GetColor, SetColor, Color, Color::WHITE, AM_DEFAULT);
    URHO3D_MIXED_ACCESSOR_ATTRIBUTE("Custom material", GetCustomMaterialAttr, SetCustomMaterialAttr, ResourceRef, ResourceRef(Material::GetTypeStatic()), AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Hot Spot", GetHotSpot, SetHotSpot, Vector2, Vector2(0.5f, 0.5f), AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Use Hot Spot", GetUseHotSpot, SetUseHotSpot, bool, false, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Draw Rectangle", GetDrawRect, SetDrawRect, Rect, Rect::ZERO, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Use Draw Rectangle", GetUseDrawRect, SetUseDrawRect, bool, false, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Texture Rectangle", GetTextureRect, SetTextureRect, Rect, Rect::ZERO, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Use Texture Rectangle", GetUseTextureRect, SetUseTextureRect, bool, false, AM_DEFAULT);
}

void StaticSprite2D::SetSprite(Sprite2D* sprite)
{
    if (sprite == sprite_)
        return;

    sprite_ = sprite;
    UpdateMaterial();

    sourceBatchesDirty_ = true;
    MarkNetworkUpdate();
    UpdateDrawRect();
}

void StaticSprite2D::SetDrawRect(const Rect& rect)
{
    drawRect_ = rect;

    if(useDrawRect_)
        sourceBatchesDirty_ = true;
}

void StaticSprite2D::SetTextureRect(const Rect& rect)
{
    textureRect_ = rect;

    if(useTextureRect_)
    {
        sourceBatchesDirty_ = true;
    }
}

void StaticSprite2D::SetBlendMode(BlendMode blendMode)
{
    if (blendMode == blendMode_)
        return;

    blendMode_ = blendMode;

    UpdateMaterial();
    MarkNetworkUpdate();
}

void StaticSprite2D::SetFlip(bool flipX, bool flipY)
{
    if (flipX == flipX_ && flipY == flipY_)
        return;

    flipX_ = flipX;
    flipY_ = flipY;
    sourceBatchesDirty_ = true;

    MarkNetworkUpdate();
}

void StaticSprite2D::SetFlipX(bool flipX)
{
    SetFlip(flipX, flipY_);
}

void StaticSprite2D::SetFlipY(bool flipY)
{
    SetFlip(flipX_, flipY);
}

void StaticSprite2D::SetColor(const Color& color)
{
    if (color == color_)
        return;

    color_ = color;
    sourceBatchesDirty_ = true;
    MarkNetworkUpdate();
}

void StaticSprite2D::SetAlpha(float alpha)
{
    if (alpha == color_.a_)
        return;

    color_.a_ = alpha;
    sourceBatchesDirty_ = true;
    MarkNetworkUpdate();
}

void StaticSprite2D::SetUseHotSpot(bool useHotSpot)
{
    if (useHotSpot == useHotSpot_)
        return;

    useHotSpot_ = useHotSpot;
    sourceBatchesDirty_ = true;
    MarkNetworkUpdate();
    UpdateDrawRect();
}

void StaticSprite2D::SetUseDrawRect(bool useDrawRect)
{
    if (useDrawRect == useDrawRect_)
        return;

    useDrawRect_ = useDrawRect;
    sourceBatchesDirty_ = true;
    MarkNetworkUpdate();
    UpdateDrawRect();
}

void StaticSprite2D::SetUseTextureRect(bool useTextureRect)
{
    if (useTextureRect == useTextureRect_)
        return;

    useTextureRect_ = useTextureRect;
    sourceBatchesDirty_ = true;
    MarkNetworkUpdate();
}

void StaticSprite2D::SetHotSpot(const Vector2& hotspot)
{
    if (hotspot == hotSpot_)
        return;

    hotSpot_ = hotspot;

    if (useHotSpot_)
    {
        sourceBatchesDirty_ = true;
        MarkNetworkUpdate();
    }
    UpdateDrawRect();
}

void StaticSprite2D::SetCustomMaterial(Material* customMaterial)
{
    if (customMaterial == customMaterial_)
        return;

    customMaterial_ = customMaterial;
    sourceBatchesDirty_ = true;

    UpdateMaterial();
    MarkNetworkUpdate();
}

Sprite2D* StaticSprite2D::GetSprite() const
{
    return sprite_;
}

Material* StaticSprite2D::GetCustomMaterial() const
{
    return customMaterial_;
}
void StaticSprite2D::SetSpriteAttr(const ResourceRef& value)
{
    Sprite2D* sprite = Sprite2D::LoadFromResourceRef(this, value);
    if (sprite)
        SetSprite(sprite);
}

ResourceRef StaticSprite2D::GetSpriteAttr() const
{
    return Sprite2D::SaveToResourceRef(sprite_);
}

void StaticSprite2D::SetCustomMaterialAttr(const ResourceRef& value)
{
    ResourceCache* cache = context_->m_ResourceCache.get();
    SetCustomMaterial(cache->GetResource<Material>(value.name_));
}

ResourceRef StaticSprite2D::GetCustomMaterialAttr() const
{
    return GetResourceRef(customMaterial_, Material::GetTypeStatic());
}

/// Handle scene being assigned.
void StaticSprite2D::OnSceneSet(Scene* scene)
{
    Drawable2D::OnSceneSet(scene);

    UpdateMaterial();
}
/// Recalculate the world-space bounding box.
void StaticSprite2D::OnWorldBoundingBoxUpdate()
{
    boundingBox_.Clear();
    worldBoundingBox_.Clear();
    if (sourceBatchesDirty_)
        UpdateSourceBatches();
    std::vector<Vertex2D>& vertices = sourceBatch_.front().vertices_;
    for (const Vertex2D &vert : vertices)
        worldBoundingBox_.Merge(vert.position_);

    boundingBox_ = worldBoundingBox_.Transformed(node_->GetWorldTransform().Inverse());
}
/// Handle draw order changed.
void StaticSprite2D::OnDrawOrderChanged()
{
    sourceBatch_.front().drawOrder_ = GetDrawOrder();
}
/// Update source batches.
void StaticSprite2D::UpdateSourceBatches()
{
    if (!sourceBatchesDirty_)
        return;

    std::vector<Vertex2D>& vertices = sourceBatch_.front().vertices_;
    vertices.clear();

    if (!sprite_)
        return;

    if (!useTextureRect_)
    {
        if (!sprite_->GetTextureRectangle(textureRect_, flipX_, flipY_))
            return;
    }

    /*
    V1---------V2
    |         / |
    |       /   |
    |     /     |
    |   /       |
    | /         |
    V0---------V3
    */
    Vertex2D vertex0;
    Vertex2D vertex1;
    Vertex2D vertex2;
    Vertex2D vertex3;

    // Convert to world space

    const Matrix3x4& worldTransform = node_->GetWorldTransform();
    vertex0.position_ = worldTransform * Vector3(drawRect_.min_.x_, drawRect_.min_.y_, 0.0f);
    vertex1.position_ = worldTransform * Vector3(drawRect_.min_.x_, drawRect_.max_.y_, 0.0f);
    vertex2.position_ = worldTransform * Vector3(drawRect_.max_.x_, drawRect_.max_.y_, 0.0f);
    vertex3.position_ = worldTransform * Vector3(drawRect_.max_.x_, drawRect_.min_.y_, 0.0f);

    vertex0.uv_ = textureRect_.min_;
    vertex1.uv_ = Vector2(textureRect_.min_.x_, textureRect_.max_.y_);
    vertex2.uv_ = textureRect_.max_;
    vertex3.uv_ = Vector2(textureRect_.max_.x_, textureRect_.min_.y_);

    vertex0.color_ = vertex1.color_ = vertex2.color_  = vertex3.color_ = color_.ToUInt();

    vertices.push_back(vertex0);
    vertices.push_back(vertex1);
    vertices.push_back(vertex2);
    vertices.push_back(vertex3);

    sourceBatchesDirty_ = false;
}

/// Update material.
void StaticSprite2D::UpdateMaterial()
{

    if (customMaterial_)
        sourceBatch_.front().material_ = customMaterial_;
    else
    {
        if (sprite_ && renderer_)
            sourceBatch_.front().material_ = renderer_->GetMaterial(sprite_->GetTexture(), blendMode_);
        else
            sourceBatch_.front().material_ = 0;
    }
}
/// Update drawRect.
void StaticSprite2D::UpdateDrawRect()
{
    if (!useDrawRect_)
    {
        if (useHotSpot_)
        {
            if (sprite_ && !sprite_->GetDrawRectangle(drawRect_, hotSpot_, flipX_, flipY_))
                return;
        }
        else
        {
            if (sprite_ && !sprite_->GetDrawRectangle(drawRect_, flipX_, flipY_))
                return;
        }
    }
}
namespace {
struct Sprite2DFactory : public ObjectFactory
{
    StaticSprite2D_Manager *m_manager;
public:
    Sprite2DFactory(Context *ctx,StaticSprite2D_Manager *manager) : ObjectFactory(ctx,StaticSprite2D::GetTypeInfoStatic()),
        m_manager(manager)
    {
    }
    SharedPtr<Object> CreateObject() override
    {
        assert(m_manager);
        return m_manager->allocateData();
    }
};
}
StaticSprite2D_Manager::StaticSprite2D_Manager(Context *ctx) : m_context(ctx)
{

}

void StaticSprite2D_Manager::initialize()
{
    //context->RegisterFactory<StaticSprite2D>(URHO2D_CATEGORY);
    //TODO: initialize memory allocation scheme here.
    m_context->RegisterFactory(new Sprite2DFactory(m_context,this),URHO2D_CATEGORY);
}

SharedPtr<StaticSprite2D> StaticSprite2D_Manager::allocateData()
{
    return SharedPtr<StaticSprite2D>(new StaticSprite2D(m_context));
}

void initializeSprite2D_Manager(Context *ctx)
{
    static StaticSprite2D_Manager sprmngr(ctx);
    if(sprmngr.m_context!=nullptr)
        assert(ctx==sprmngr.m_context);
    sprmngr.initialize();
    StaticSprite2D::RegisterObject(ctx);
}

}
