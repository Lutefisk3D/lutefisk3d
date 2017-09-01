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

enum TrailType
{
    TT_FACE_CAMERA = 0,
    TT_BONE
};

class IndexBuffer;
class VertexBuffer;

/// Trail is consisting of series of tails. Two connected points make a tail.
struct LUTEFISK3D_EXPORT TrailPoint
{
    Vector3     position_;      //!< Position.
    Vector3     forward_;       //!< Forward vector.
    Vector3     parentPos_;     //!< Parent position. Trail bone type uses this.
    float       elapsedLength_; //!< Elapsed length inside the trail.
    TrailPoint *next_;          //!< Next point to make a tail.
    float       lifetime_;      //!< Tail time to live.
    float       sortDistance_;  //!< Distance for sorting.
};

class LUTEFISK3D_EXPORT RibbonTrail : public Drawable
{
    URHO3D_OBJECT(RibbonTrail, Drawable)

public:
    RibbonTrail(Context* context);
    virtual ~RibbonTrail();

    static void RegisterObject(Context* context);
    void ProcessRayQuery(const RayOctreeQuery &query, std::vector<RayQueryResult> &results) override;
    void OnSetEnabled() override;
    void Update(const FrameInfo &frame) override;
    void UpdateBatches(const FrameInfo &frame) override;
    void UpdateGeometry(const FrameInfo &frame) override;
    UpdateGeometryType GetUpdateGeometryType() override;

    void SetMaterial(Material *material);
    void SetMaterialAttr(const ResourceRef &value);
    void SetVertexDistance(float length);
    void SetWidth(float width);
    void SetStartColor(const Color &color);
    void SetEndColor(const Color &color);
    void SetStartScale(float startScale);
    void SetEndScale(float endScale);
    void SetTrailType(TrailType type);
    void SetSorted(bool enable);
    void SetLifetime(float time);
    void SetEmitting(bool emitting);
    void SetUpdateInvisible(bool enable);
    void SetTailColumn(unsigned tailColumn);
    void SetAnimationLodBias(float bias);
    void        Commit();
    Material *  GetMaterial() const;
    ResourceRef GetMaterialAttr() const;

    float        GetVertexDistance() const  { return vertexDistance_; }
    float        GetWidth() const           { return width_; }
    const Color &GetStartColor() const      { return startColor_; }
    const Color &GetEndColor() const        { return endColor_; }
    float        GetStartScale() const      { return startScale_; }
    float        GetEndScale() const        { return endScale_; }
    bool         IsSorted() const           { return sorted_; }
    float        GetLifetime() const        { return lifetime_; }
    float        GetAnimationLodBias() const { return animationLodBias_; }
    TrailType    GetTrailType() const       { return trailType_; }
    unsigned     GetTailColumn() const      { return tailColumn_; }
    bool         IsEmitting() const         { return emitting_; }
    bool         GetUpdateInvisible() const { return updateInvisible_; }

protected:
    void OnSceneSet(Scene *scene) override;
    void OnWorldBoundingBoxUpdate() override;
    void MarkPositionsDirty();

    std::deque<TrailPoint>    points_;
    std::vector<TrailPoint *> sortedPoints_;
    bool                      sorted_            = false;
    float                     animationLodBias_  = 1.0f;
    float                     animationLodTimer_ = 0.0f;
    TrailType                 trailType_         = TT_FACE_CAMERA;

private:
    void HandleScenePostUpdate(Scene *, float ts);
    void UpdateBufferSize();
    void UpdateVertexBuffer(const FrameInfo& frame);
    void UpdateTail();
    SharedPtr<Geometry>     geometry_;
    SharedPtr<VertexBuffer> vertexBuffer_;
    SharedPtr<IndexBuffer>  indexBuffer_;
    Matrix3x4               transforms_;
    TrailPoint              endTail_;
    Color                   startColor_            = Color(1.0f, 1.0f, 1.0f, 1.0f);
    Color                   endColor_              = Color(1.0f, 1.0f, 1.0f, 0.0f);
    Vector3                 previousPosition_      = Vector3::ZERO;
    Vector3                 previousOffset_        = Vector3::ZERO;
    float                   startEndTailTime_      = 0.0f;
    float                   vertexDistance_        = 0.1f;
    float                   width_                 = 0.2f;
    float                   startScale_            = 1.0f;
    float                   endScale_              = 1.0f;
    float                   lastTimeStep_          = 0.0f;
    float                   lifetime_              = 1.0f;
    unsigned                numPoints_             = 0;
    unsigned                tailColumn_            = 1;
    unsigned                lastUpdateFrameNumber_ = M_MAX_UNSIGNED;
    bool                    bufferSizeDirty_       = false;
    bool                    bufferDirty_           = true;
    bool                    needUpdate_            = false;
    bool                    forceUpdate_           = false;
    bool                    emitting_              = true;
    bool                    updateInvisible_       = false;
};
}
