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

#include "RibbonTrail.h"

#include "Lutefisk3D/Core/Context.h"
#include "Lutefisk3D/Graphics/VertexBuffer.h"
#include "Lutefisk3D/Graphics/IndexBuffer.h"
#include "Lutefisk3D/Graphics/Camera.h"
#include "Lutefisk3D/Graphics/Material.h"
#include "Lutefisk3D/Graphics/OctreeQuery.h"
#include "Lutefisk3D/Graphics/Geometry.h"
#include "Lutefisk3D/Scene/Scene.h"
#include "Lutefisk3D/Scene/SceneEvents.h"
#include "Lutefisk3D/Resource/ResourceCache.h"
#include "Lutefisk3D/IO/Log.h"

namespace Urho3D
{

extern const char* GEOMETRY_CATEGORY;

namespace {
const unsigned MAX_TAIL_COLUMN = 16;
const char* trailTypeNames[] =
{
    "Face Camera",
    "Bone",
    nullptr
};
inline bool CompareTails(TrailPoint* lhs, TrailPoint* rhs)
{
    return lhs->sortDistance_ > rhs->sortDistance_;
}
}

/*!
    \class RibbonTrail
        \brief Drawable component that creates a tail.

    \fn RibbonTrail::GetVertexDistance
        \brief  Get distance between points.
    \fn RibbonTrail::GetWidth
        \brief  Get width of the trail.
    \fn RibbonTrail::GetStartColor
        \brief  Get vertex blended color for start of trail.
    \fn RibbonTrail::GetEndColor
        \brief  Get vertex blended color for end of trail.
    \fn RibbonTrail::GetStartScale
        \brief  Get vertex blended scale for start of trail.
    \fn RibbonTrail::GetEndScale
        \brief  Get vertex blended scale for end of trail.
    \fn RibbonTrail::IsSorted
        \brief  Return whether tails are sorted.
    \fn RibbonTrail::GetLifetime
        \brief  Return tail time to live.
    \fn RibbonTrail::GetAnimationLodBias
        \brief  Return animation LOD bias.
    \fn RibbonTrail::GetTrailType
        \brief  Return how the trail behave.
    \fn RibbonTrail::GetTailColumn
        \brief  Get number of column for tails.
    \fn RibbonTrail::IsEmitting
        \brief  Return whether is currently emitting.
    \fn RibbonTrail::GetUpdateInvisible
        \brief  Return whether to update when trail emitter are not visible.

*/
/*!
    \var RibbonTrail::points_
        \brief Tails.
    \var RibbonTrail::sortedPoints_
        \brief this vector is used by UpdateVertexBuffer
    \var RibbonTrail::false
        \brief Tails sorted flag.
    \var RibbonTrail::animationLodBias_
        \brief Animation LOD bias.
    \var RibbonTrail::animationLodTimer_
        \brief Animation LOD timer.
    \var RibbonTrail::trailType_
        \brief Trail type.

    \var RibbonTrail::geometry_
        \brief trail geometry.
    \var RibbonTrail::vertexBuffer_
        \brief Vertex buffer.
    \var RibbonTrail::indexBuffer_
        \brief Index buffer.
    \var RibbonTrail::transforms_
        \brief Transform matrices for position and orientation.
    \var RibbonTrail::bufferSizeDirty_
        \brief Buffers need resize flag.
    \var RibbonTrail::bufferDirty_
        \brief Vertex buffer needs rewrite flag.
    \var RibbonTrail::previousPosition_
        \brief Previous position of tail
    \var RibbonTrail::vertexDistance_
        \brief Distance between points. Basically is tail length.
    \var RibbonTrail::width_
        \brief Width of trail.
    \var RibbonTrail::numPoints_
        \brief Number of points.
    \var RibbonTrail::startColor_
        \brief Color for start of trails.
    \var RibbonTrail::endColor_
        \brief Color for end of trails.
    \var RibbonTrail::startScale_
        \brief Scale for start of trails.
    \var RibbonTrail::endScale_
        \brief End for start of trails.
    \var RibbonTrail::lastTimeStep_
        \brief Last scene timestep.
    \var RibbonTrail::tailColumn_
        \brief Number of columns for every tails.
    \var RibbonTrail::lastUpdateFrameNumber_;
        \brief Rendering framenumber on which was last updated.
    \var RibbonTrail::needUpdate_;
        \brief Need update flag.
    \var RibbonTrail::previousOffset_;
        \brief Previous offset to camera for determining whether sorting is necessary.
    \var RibbonTrail::sortedPoints_;
        \brief Trail pointers for sorting.
    \var RibbonTrail::forceUpdate_;
        \brief Force update flag (ignore animation LOD momentarily.)
    \var RibbonTrail::emitting_
        \brief Currently emitting flag.
    \var RibbonTrail::updateInvisible_
        \brief Update when invisible flag.
    \var RibbonTrail::endTail_
        \brief End of trail point for smoother tail disappearance.
    \var RibbonTrail::startEndTailTime_
        \brief The time the tail become end of trail.

*/

RibbonTrail::RibbonTrail(Context* context) :
    Drawable(context, DRAWABLE_GEOMETRY),
    geometry_(new Geometry(context_)),
    vertexBuffer_(new VertexBuffer(context_)),
    indexBuffer_(new IndexBuffer(context_)),
    emitting_(true)
{
    geometry_->SetVertexBuffer(0, vertexBuffer_);
    geometry_->SetIndexBuffer(indexBuffer_);

    transforms_ = Matrix3x4::IDENTITY;

    batches_.resize(1);
    batches_[0].geometry_ = geometry_;
    batches_[0].geometryType_ = GEOM_TRAIL_FACE_CAMERA;
    batches_[0].worldTransform_ = &transforms_;
    batches_[0].numWorldTransforms_ = 1;
}

RibbonTrail::~RibbonTrail()
{
}
/// Register object factory and instance attributes.
void RibbonTrail::RegisterObject(Context* context)
{
    context->RegisterFactory<RibbonTrail>(GEOMETRY_CATEGORY);

    URHO3D_ACCESSOR_ATTRIBUTE("Is Enabled", IsEnabled, SetEnabled, bool, true, AM_DEFAULT);
    URHO3D_COPY_BASE_ATTRIBUTES(Drawable);
    URHO3D_MIXED_ACCESSOR_ATTRIBUTE("Material", GetMaterialAttr, SetMaterialAttr, ResourceRef, {Material::GetTypeStatic()}, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Emitting", IsEmitting, SetEmitting, bool, true, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Update Invisible", GetUpdateInvisible, SetUpdateInvisible, bool, false, AM_DEFAULT);
    URHO3D_ENUM_ACCESSOR_ATTRIBUTE("Trail Type", GetTrailType, SetTrailType, TrailType, trailTypeNames, TT_FACE_CAMERA, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Tail Lifetime", GetLifetime, SetLifetime, float, 1.0f, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Tail Column", GetTailColumn, SetTailColumn, unsigned, 0, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Vertex Distance", GetVertexDistance, SetVertexDistance, float, 0.1f, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Width", GetWidth, SetWidth, float, 0.2f, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Start Scale", GetStartScale, SetStartScale, float, 1.0f, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("End Scale", GetEndScale, SetEndScale, float, 1.0f, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Start Color", GetStartColor, SetStartColor, Color, Color::WHITE, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("End Color", GetEndColor, SetEndColor, Color, Color(1.0f, 1.0f, 1.0f, 0.0f), AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Animation LOD Bias", GetAnimationLodBias, SetAnimationLodBias, float, 1.0f, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Sort By Distance", IsSorted, SetSorted, bool, false, AM_DEFAULT);
}
/// Process octree raycast. May be called from a worker thread.
void RibbonTrail::ProcessRayQuery(const RayOctreeQuery& query, std::vector<RayQueryResult>& results)
{
    // If no trail-level testing, use the Drawable test
    if (query.level_ < RAY_TRIANGLE)
    {
        Drawable::ProcessRayQuery(query, results);
        return;
    }

    // Check ray hit distance to AABB before proceeding with trail-level tests
    if (query.ray_.HitDistance(GetWorldBoundingBox()) >= query.maxDistance_)
        return;

    // Approximate the tails as spheres for raycasting
    for (unsigned i = 0; i < points_.size() - 1; ++i)
    {
        Vector3 center = (points_[i].position_ + points_[i+1].position_) * 0.5f;
        Vector3 scale = width_ * Vector3::ONE;
        // Tail should be represented in cylinder shape, but we don't have this yet on Urho,
        // so this implementation will use bounding box instead (hopefully only temporarily)
        float distance = query.ray_.HitDistance(BoundingBox(center - scale, center + scale));
        if (distance < query.maxDistance_)
        {
            // If the code reaches here then we have a hit
            RayQueryResult result;
            result.position_ = query.ray_.origin_ + distance * query.ray_.direction_;
            result.normal_ = -query.ray_.direction_;
            result.distance_ = distance;
            result.drawable_ = this;
            result.node_ = node_;
            result.subObject_ = i;
            results.emplace_back(result);
        }
    }
}
/// Handle enabled/disabled state change.
void RibbonTrail::OnSetEnabled()
{
    Drawable::OnSetEnabled();

    previousPosition_ = node_->GetWorldPosition();

    Scene* scene = GetScene();
    if (scene)
    {
        if (IsEnabledEffective())
            scene->scenePostUpdate.Connect(this,&RibbonTrail::HandleScenePostUpdate);
        else
            scene->scenePostUpdate.Disconnect(this,&RibbonTrail::HandleScenePostUpdate);
    }
}
/**
 * \brief Handle scene post-update event.
 * \param ts - time step of the last update
 */
void RibbonTrail::HandleScenePostUpdate(Scene*,float ts)
{
    lastTimeStep_ = ts;

    // Update if frame has changed
    if (updateInvisible_ || viewFrameNumber_ != lastUpdateFrameNumber_)
    {
        // Reset if ribbon trail is too small and too much difference in frame
        if (points_.size() < 3 && viewFrameNumber_ - lastUpdateFrameNumber_ > 1)
        {
            previousPosition_ = node_->GetWorldPosition();
            points_.clear();
        }

        lastUpdateFrameNumber_ = viewFrameNumber_;
        needUpdate_ = true;
        MarkForUpdate();
    }
}
/// Update before octree reinsertion. Is called from a main thread.
void RibbonTrail::Update(const FrameInfo &frame)
{
    Drawable::Update(frame);

    if (!needUpdate_)
        return;

    UpdateTail();
    OnMarkedDirty(node_);
    needUpdate_ = false;
}
/**
 * \brief Update/Rebuild tail mesh only if position changed (called by UpdateBatches())
 * \sa UpdateBatches
 */
void RibbonTrail::UpdateTail()
{
    Vector3 worldPosition = node_->GetWorldPosition();
    float path = (previousPosition_ - worldPosition).Length();

    // Update tails lifetime
    int expiredIndex = -1;
    if (points_.size() > 0)
    {
        // No need to update last point
        for (unsigned i = 0; i < points_.size() - 1; ++i)
        {
            points_[i].lifetime_ += lastTimeStep_;

            // Get point index with expired lifetime
            if (points_[i].lifetime_ > lifetime_)
                expiredIndex = i;
        }
    }

    // Delete expired points
    if (expiredIndex != -1)
    {
        points_.erase(points_.begin(), points_.begin() + (unsigned)(expiredIndex + 1));

        // Update endTail pointer
        if (points_.size() > 1)
        {
            endTail_.position_ = points_[0].position_;
            startEndTailTime_ = points_[0].lifetime_;
        }
    }

    // Update previous world position if trail is still zero
    if (points_.size() == 0)
    {
        previousPosition_ = worldPosition;
    }
    // Delete lonely point
    else if (points_.size() == 1)
    {
        points_.pop_front();
        previousPosition_ = worldPosition;
    }
    // Update end of trail position using endTail linear interpolation
    else if (points_.size() > 1 && points_[0].lifetime_ < lifetime_)
    {
        float step = SmoothStep(startEndTailTime_, lifetime_, points_[0].lifetime_);
        points_[0].position_ = Lerp(endTail_.position_, points_[1].position_, step);
        bufferDirty_ = true;
    }

    // Add starting points
    if (points_.empty() && path > M_LARGE_EPSILON && emitting_)
    {
        Vector3 forwardMotion = (previousPosition_ - worldPosition).Normalized();

        TrailPoint startPoint;
        startPoint.position_ = previousPosition_;
        startPoint.lifetime_ = 0.0f;
        startPoint.forward_ = forwardMotion;

        TrailPoint nextPoint;
        nextPoint.position_ = worldPosition;
        nextPoint.lifetime_ = 0.0f;
        nextPoint.forward_ = forwardMotion;

        if (node_->GetParent() != nullptr)
        {
            startPoint.parentPos_ = node_->GetParent()->GetWorldPosition();
            nextPoint.parentPos_ = startPoint.parentPos_;   // was: node_->GetParent()->GetWorldPosition();
        }

        points_.push_back(startPoint);
        points_.push_back(nextPoint);

        // Update endTail
        endTail_.position_ = startPoint.position_;
        startEndTailTime_ = 0.0f;
    }

    // Add more points
    if (points_.size() > 1 && emitting_)
    {
        Vector3 forwardMotion = (previousPosition_ - worldPosition).Normalized();

        // Add more points if path exceeded tail length
        if (path > vertexDistance_)
        {
            TrailPoint newPoint;
            newPoint.position_ = worldPosition;
            newPoint.lifetime_ = 0.0f;
            newPoint.forward_ = forwardMotion;
            if (node_->GetParent() != nullptr)
                newPoint.parentPos_ = node_->GetParent()->GetWorldPosition();

            points_.push_back(newPoint);

            previousPosition_ = worldPosition;
        }
        else
        {
            // Update recent tail
            points_.back().position_ = worldPosition;
            if (forwardMotion != Vector3::ZERO)
                points_.back().forward_ = forwardMotion;
        }
    }

    // Update buffer size if size of points different with tail number
    if (points_.size() != numPoints_)
        bufferSizeDirty_ = true;
}
/// Set vertex blended scale for end of trail.
void RibbonTrail::SetEndScale(float endScale)
{
    endScale_ = endScale;
    Commit();
}
/// Set vertex blended scale for start of trail.
void RibbonTrail::SetStartScale(float startScale)
{
    startScale_ = startScale;
    Commit();
}
/// Set whether trail should be emitting.
void RibbonTrail::SetEmitting(bool emitting)
{
    if (emitting == emitting_)
        return;

    emitting_ = emitting;

    // Reset already available points
    if (emitting && points_.size() > 0)
    {
        points_.clear();
        bufferSizeDirty_ = true;
    }

    Drawable::OnMarkedDirty(node_);
    MarkNetworkUpdate();
}
/// Set number of column for every tails. Can be useful for fixing distortion at high angle.
void RibbonTrail::SetTailColumn(unsigned tailColumn)
{
    if (tailColumn > MAX_TAIL_COLUMN)
    {
        URHO3D_LOGWARNING("Max ribbon trail tail column is " + QString::number(MAX_TAIL_COLUMN));
        tailColumn_ = MAX_TAIL_COLUMN;
    }
    else if (tailColumn < 1)
    {
        tailColumn_ = 1;
    }
    else
        tailColumn_ = tailColumn;

    Drawable::OnMarkedDirty(node_);
    bufferSizeDirty_ = true;
    MarkNetworkUpdate();
}
/// Calculate distance and prepare batches for rendering. May be called from worker thread(s), possibly re-entrantly.
void RibbonTrail::UpdateBatches(const FrameInfo& frame)
{
    // Update information for renderer about this drawable
    distance_ = frame.camera_->GetDistance(GetWorldBoundingBox().Center());
    batches_[0].distance_ = distance_;

    // Calculate scaled distance for animation LOD
    float scale = GetWorldBoundingBox().size().DotProduct(DOT_SCALE);
    // If there are no trail, the size becomes zero, and LOD'ed updates no longer happen. Disable LOD in that case
    if (scale > M_EPSILON)
        lodDistance_ = frame.camera_->GetLodDistance(distance_, scale, lodBias_);
    else
        lodDistance_ = 0.0f;

    Vector3 worldPos = node_->GetWorldPosition();
    Vector3 offset = (worldPos - frame.camera_->GetNode()->GetWorldPosition());
    if (sorted_ && offset != previousOffset_)
    {
        bufferDirty_ = true;
        previousOffset_ = offset;
    }
}
/// Prepare geometry for rendering. Called from a worker thread if possible (no GPU update.)
void RibbonTrail::UpdateGeometry(const FrameInfo& frame)
{
    if (bufferSizeDirty_ || indexBuffer_->IsDataLost())
        UpdateBufferSize();

    if (bufferDirty_ || vertexBuffer_->IsDataLost())
        UpdateVertexBuffer(frame);
}
/// Return whether a geometry update is necessary, and if it can happen in a worker thread.
UpdateGeometryType RibbonTrail::GetUpdateGeometryType()
{
    if (bufferDirty_ || bufferSizeDirty_ || vertexBuffer_->IsDataLost() || indexBuffer_->IsDataLost())
        return UPDATE_MAIN_THREAD;
    else
        return UPDATE_NONE;
}
/// Set material.
void RibbonTrail::SetMaterial(Material* material)
{
    batches_[0].material_ = material;
    MarkNetworkUpdate();
}
/// Handle node being assigned.
void RibbonTrail::OnSceneSet(Scene* scene)
{
    Drawable::OnSceneSet(scene);

    if (scene && IsEnabledEffective())
        scene->scenePostUpdate.Connect(this,&RibbonTrail::HandleScenePostUpdate);
    else if (!scene) {
        if(GetScene())
            GetScene()->scenePostUpdate.Disconnect(this);
    }
}
/// Recalculate the world-space bounding box.
void RibbonTrail::OnWorldBoundingBoxUpdate()
{
    BoundingBox worldBox;

    for (unsigned i = 0; i < points_.size(); ++i)
    {
        Vector3 &p = points_[i].position_;
        Vector3 scale = width_ * Vector3::ONE;
        worldBox.Merge(BoundingBox(p - scale, p + scale));
    }

    worldBoundingBox_ = worldBox;
}
/**
 * \brief Resize RibbonTrail vertex and index buffers.
 */
void RibbonTrail::UpdateBufferSize()
{
    numPoints_ = points_.size();

    unsigned indexPerSegment = 6 + (tailColumn_ - 1) * 6;
    unsigned vertexPerSegment = 4 + (tailColumn_ - 1) * 2;

    unsigned mask = 0;

    if (trailType_ == TT_FACE_CAMERA)
    {
        batches_[0].geometryType_ = GEOM_TRAIL_FACE_CAMERA;
        mask =  MASK_POSITION | MASK_COLOR | MASK_TEXCOORD1 | MASK_TANGENT;
    }
    else if (trailType_ == TT_BONE)
    {
        batches_[0].geometryType_ = GEOM_TRAIL_BONE;
        mask =  MASK_POSITION | MASK_NORMAL | MASK_COLOR | MASK_TEXCOORD1 | MASK_TANGENT;
    }

    bufferSizeDirty_ = false;
    bufferDirty_ = true;
    forceUpdate_ = true;

    if (numPoints_ < 2)
    {
        indexBuffer_->SetSize(0, false);
        vertexBuffer_->SetSize(0, mask, true);
        return;
    }
    else
    {
        indexBuffer_->SetSize(((numPoints_ - 1) * indexPerSegment), false);
        vertexBuffer_->SetSize(numPoints_ * vertexPerSegment, mask, true);
    }

    // Indices do not change for a given tail generator capacity
    unsigned short* dest = (unsigned short*)indexBuffer_->Lock(0, ((numPoints_ - 1) * indexPerSegment), true);
    if (!dest)
        return;

    unsigned vertexIndex = 0;
    unsigned stripsLen = numPoints_ - 1;

    while (stripsLen--)
    {
        dest[0] = (unsigned short)vertexIndex;
        dest[1] = (unsigned short)(vertexIndex + 2);
        dest[2] = (unsigned short)(vertexIndex + 1);

        dest[3] = (unsigned short)(vertexIndex + 1);
        dest[4] = (unsigned short)(vertexIndex + 2);
        dest[5] = (unsigned short)(vertexIndex + 3);

        dest += 6;
        vertexIndex += 2;

        for (unsigned i = 0; i < (tailColumn_ - 1); ++i)
        {
            dest[0] = (unsigned short)vertexIndex;
            dest[1] = (unsigned short)(vertexIndex + 2);
            dest[2] = (unsigned short)(vertexIndex + 1);

            dest[3] = (unsigned short)(vertexIndex + 1);
            dest[4] = (unsigned short)(vertexIndex + 2);
            dest[5] = (unsigned short)(vertexIndex + 3);

            dest += 6;
            vertexIndex += 2;
        }

       vertexIndex += 2;

    }

    indexBuffer_->Unlock();
    indexBuffer_->ClearDataLost();
}
/**
 * @brief Rewrite RibbonTrail vertex buffer.
 * @param frame - source frame information
 */
void RibbonTrail::UpdateVertexBuffer(const FrameInfo& frame)
{
    // If using animation LOD, accumulate time and see if it is time to update
    if (animationLodBias_ > 0.0f && lodDistance_ > 0.0f)
    {
        animationLodTimer_ += animationLodBias_ * frame.timeStep_ * ANIMATION_LOD_BASESCALE;
        if (animationLodTimer_ >= lodDistance_)
            animationLodTimer_ = fmodf(animationLodTimer_, lodDistance_);
        else
        {
            // No LOD if immediate update forced
            if (!forceUpdate_)
                return;
        }
    }

    // if tail path is short and nothing to draw, exit
    if (numPoints_ < 2)
    {
        batches_[0].geometry_->SetDrawRange(TRIANGLE_LIST, 0, 0, false);
        return;
    }

    unsigned indexPerSegment = 6 + (tailColumn_ - 1) * 6;
    unsigned vertexPerSegment = 4 + (tailColumn_ - 1) * 2;

    // Fill sorted points vector
    sortedPoints_.reserve(numPoints_);
    sortedPoints_.clear();
    for (unsigned i = 0; i < numPoints_; ++i)
    {
        TrailPoint& point(points_[i]);
        sortedPoints_.emplace_back(&point);
        if (sorted_)
            point.sortDistance_ = frame.camera_->GetDistanceSquared(point.position_);
    }

    // Sort points
    if (sorted_)
        std::stable_sort(sortedPoints_.begin(), sortedPoints_.end(), CompareTails);

    // Update individual trail elapsed length
    float trailLength = 0.0f;
    for(unsigned i = 0; i < numPoints_; ++i)
    {
        float length = i == 0 ? 0.0f : (points_[i].position_ - points_[i-1].position_).Length();
        trailLength += length;
        points_[i].elapsedLength_ = trailLength;
        if (i < numPoints_ - 1)
            points_[i].next_ = &points_[i+1];
    }

    batches_[0].geometry_->SetDrawRange(TRIANGLE_LIST, 0, (numPoints_ - 1) * indexPerSegment, false);
    bufferDirty_ = false;
    forceUpdate_ = false;

    float* dest = (float*)vertexBuffer_->Lock(0, (numPoints_ - 1) * vertexPerSegment, true);
    if (!dest)
        return;

    // Generate trail mesh
    if (trailType_ == TT_FACE_CAMERA)
    {
        for (unsigned i = 0; i < numPoints_; ++i)
        {
            TrailPoint& point = *sortedPoints_[i];

            if (sortedPoints_[i] == &points_.back())
                continue;

            // This point
            float factor = SmoothStep(0.0f, trailLength, point.elapsedLength_);
            unsigned c = endColor_.Lerp(startColor_, factor).ToUInt();
            float width = Lerp(width_ * endScale_, width_ * startScale_, factor);

            // Next point
            float nextFactor = SmoothStep(0.0f, trailLength, point.next_->elapsedLength_);
            unsigned nextC = endColor_.Lerp(startColor_, nextFactor).ToUInt();
            float nextWidth = Lerp(width_ * endScale_, width_ * startScale_, nextFactor);

            // First row
            dest[0] = point.position_.x_;
            dest[1] = point.position_.y_;
            dest[2] = point.position_.z_;
            ((unsigned&)dest[3]) = c;
            dest[4] = factor;
            dest[5] = 0.0f;
            dest[6] = point.forward_.x_;
            dest[7] = point.forward_.y_;
            dest[8] = point.forward_.z_;
            dest[9] = width;

            dest[10] = point.next_->position_.x_;
            dest[11] = point.next_->position_.y_;
            dest[12] = point.next_->position_.z_;
            ((unsigned&)dest[13]) = nextC;
            dest[14] = nextFactor;
            dest[15] = 0.0f;
            dest[16] = point.next_->forward_.x_;
            dest[17] = point.next_->forward_.y_;
            dest[18] = point.next_->forward_.z_;
            dest[19] = nextWidth;

            dest += 20;

            // Middle rows
            for (unsigned j = 0; j < (tailColumn_ - 1); ++j)
            {
                float elapsed = 1.0f / tailColumn_ * (j + 1);
                float midWidth = width - elapsed * 2.0f * width;
                float nextMidWidth = nextWidth - elapsed * 2.0f * nextWidth;

                dest[0] = point.position_.x_;
                dest[1] = point.position_.y_;
                dest[2] = point.position_.z_;
                ((unsigned&)dest[3]) = c;
                dest[4] = factor;
                dest[5] = elapsed;
                dest[6] = point.forward_.x_;
                dest[7] = point.forward_.y_;
                dest[8] = point.forward_.z_;
                dest[9] = midWidth;

                dest[10] = point.next_->position_.x_;
                dest[11] = point.next_->position_.y_;
                dest[12] = point.next_->position_.z_;
                ((unsigned&)dest[13]) = nextC;
                dest[14] = nextFactor;
                dest[15] = elapsed;
                dest[16] = point.next_->forward_.x_;
                dest[17] = point.next_->forward_.y_;
                dest[18] = point.next_->forward_.z_;
                dest[19] = nextMidWidth;

                dest += 20;
            }

            // Last row
            dest[0] = point.position_.x_;
            dest[1] = point.position_.y_;
            dest[2] = point.position_.z_;
            ((unsigned&)dest[3]) = c;
            dest[4] = factor;
            dest[5] = 1.0f;
            dest[6] = point.forward_.x_;
            dest[7] = point.forward_.y_;
            dest[8] = point.forward_.z_;
            dest[9] = -width;

            dest[10] = point.next_->position_.x_;
            dest[11] = point.next_->position_.y_;
            dest[12] = point.next_->position_.z_;
            ((unsigned&)dest[13]) = nextC;
            dest[14] = nextFactor;
            dest[15] = 1.0f;
            dest[16] = point.next_->forward_.x_;
            dest[17] = point.next_->forward_.y_;
            dest[18] = point.next_->forward_.z_;
            dest[19] = -nextWidth;

            dest += 20;
        }
    }
    else if (trailType_ == TT_BONE)
    {
        for (unsigned i = 0; i < numPoints_; ++i)
        {
            TrailPoint& point = *sortedPoints_[i];

            if (sortedPoints_[i] == &points_.back())
                continue;

            // This point
            float factor = SmoothStep(0.0f, trailLength, point.elapsedLength_);
            unsigned c = endColor_.Lerp(startColor_, factor).ToUInt();

            float rightScale = Lerp(endScale_, startScale_, factor);
            float shift = (rightScale - 1.0f) / 2.0f;
            float leftScale = 0.0f - shift;

            // Next point
            float nextFactor = SmoothStep(0.0f, trailLength, point.next_->elapsedLength_);
            unsigned nextC = endColor_.Lerp(startColor_, nextFactor).ToUInt();

            float nextRightScale = Lerp(endScale_, startScale_, nextFactor);
            float nextShift = (nextRightScale - 1.0f) / 2.0f;
            float nextLeftScale = 0.0f - nextShift;

            // First row
            dest[0] = point.position_.x_;
            dest[1] = point.position_.y_;
            dest[2] = point.position_.z_;
            dest[3] = point.forward_.x_;
            dest[4] = point.forward_.y_;
            dest[5] = point.forward_.z_;
            ((unsigned&)dest[6]) = c;
            dest[7] = factor;
            dest[8] = 0.0f;
            dest[9] = point.parentPos_.x_;
            dest[10] = point.parentPos_.y_;
            dest[11] = point.parentPos_.z_;
            dest[12] = leftScale;

            dest[13] = point.next_->position_.x_;
            dest[14] = point.next_->position_.y_;
            dest[15] = point.next_->position_.z_;
            dest[16] = point.next_->forward_.x_;
            dest[17] = point.next_->forward_.y_;
            dest[18] = point.next_->forward_.z_;
            ((unsigned&)dest[19]) = nextC;
            dest[20] = nextFactor;
            dest[21] = 0.0f;
            dest[22] = point.next_->parentPos_.x_;
            dest[23] = point.next_->parentPos_.y_;
            dest[24] = point.next_->parentPos_.z_;
            dest[25] = nextLeftScale;

            dest += 26;

            // Middle row
            for (unsigned j = 0; j < (tailColumn_ - 1); ++j)
            {
                float elapsed = 1.0f / tailColumn_ * (j + 1);

                dest[0] = point.position_.x_;
                dest[1] = point.position_.y_;
                dest[2] = point.position_.z_;
                dest[3] = point.forward_.x_;
                dest[4] = point.forward_.y_;
                dest[5] = point.forward_.z_;
                ((unsigned&)dest[6]) = c;
                dest[7] = factor;
                dest[8] = elapsed;
                dest[9] = point.parentPos_.x_;
                dest[10] = point.parentPos_.y_;
                dest[11] = point.parentPos_.z_;
                dest[12] = Lerp(leftScale, rightScale, elapsed);

                dest[13] = point.next_->position_.x_;
                dest[14] = point.next_->position_.y_;
                dest[15] = point.next_->position_.z_;
                dest[16] = point.next_->forward_.x_;
                dest[17] = point.next_->forward_.y_;
                dest[18] = point.next_->forward_.z_;
                ((unsigned&)dest[19]) = nextC;
                dest[20] = nextFactor;
                dest[21] = elapsed;
                dest[22] = point.next_->parentPos_.x_;
                dest[23] = point.next_->parentPos_.y_;
                dest[24] = point.next_->parentPos_.z_;
                dest[25] = Lerp(nextLeftScale, nextRightScale, elapsed);

                dest += 26;
            }

            // Last row
            dest[0] = point.position_.x_;
            dest[1] = point.position_.y_;
            dest[2] = point.position_.z_;
            dest[3] = point.forward_.x_;
            dest[4] = point.forward_.y_;
            dest[5] = point.forward_.z_;
            ((unsigned&)dest[6]) = c;
            dest[7] = factor;
            dest[8] = 1.0f;
            dest[9] = point.parentPos_.x_;
            dest[10] = point.parentPos_.y_;
            dest[11] = point.parentPos_.z_;
            dest[12] = rightScale;

            dest[13] = point.next_->position_.x_;
            dest[14] = point.next_->position_.y_;
            dest[15] = point.next_->position_.z_;
            dest[16] = point.next_->forward_.x_;
            dest[17] = point.next_->forward_.y_;
            dest[18] = point.next_->forward_.z_;
            ((unsigned&)dest[19]) = nextC;
            dest[20] = nextFactor;
            dest[21] = 1.0f;
            dest[22] = point.next_->parentPos_.x_;
            dest[23] = point.next_->parentPos_.y_;
            dest[24] = point.next_->parentPos_.z_;
            dest[25] = nextRightScale;

            dest += 26;
        }
    }

    vertexBuffer_->Unlock();
    vertexBuffer_->ClearDataLost();
}
/// Set tail time to live.
void RibbonTrail::SetLifetime(float time)
{
    lifetime_ = time;
    Commit();
}
/// Set distance between points.
void RibbonTrail::SetVertexDistance(float length)
{
    vertexDistance_ = length;
    Commit();
}
/// Set vertex blended color for end of trail.
void RibbonTrail::SetEndColor(const Color& color)
{
    endColor_ = color;
    Commit();
}
/// Set vertex blended color for start of trail.
void RibbonTrail::SetStartColor(const Color& color)
{
    startColor_ = color;
    Commit();
}
/// Set whether tails are sorted by distance. Default false.
void RibbonTrail::SetSorted(bool enable)
{
    sorted_ = enable;
    Commit();
}
/// Set how the trail behave.
void RibbonTrail::SetTrailType(TrailType type)
{
    if (trailType_ == type)
        return;

    if (type == TT_BONE && (node_->GetParent() == nullptr || node_->GetParent() == node_->GetScene()))
    {
        URHO3D_LOGWARNING("No parent node found, revert back to Face Camera type");
        return;
    }

    trailType_ = type;
    Drawable::OnMarkedDirty(node_);
    bufferSizeDirty_ = true;
    MarkNetworkUpdate();
}
/// Set material attribute.
void RibbonTrail::SetMaterialAttr(const ResourceRef& value)
{
    ResourceCache* cache =context_->m_ResourceCache.get();
    SetMaterial(cache->GetResource<Material>(value.name_));
    Commit();
}
/// Set width of the tail. Only works for face camera trail type.
void RibbonTrail::SetWidth(float width)
{
    width_ = width;
    Commit();
}
/// Set animation LOD bias.
void RibbonTrail::SetAnimationLodBias(float bias)
{
    animationLodBias_ = Max(bias, 0.0f);
    MarkNetworkUpdate();
}
/// Set whether to update when trail emiiter are not visible.
void RibbonTrail::SetUpdateInvisible(bool enable)
{
    updateInvisible_ = enable;
    MarkNetworkUpdate();
}
/**
 * \brief Mark for bounding box and vertex buffer update. Call after modifying the trails.
 */
void RibbonTrail::Commit()
{
    MarkPositionsDirty();
    MarkNetworkUpdate();
}
/// Mark vertex buffer to need an update.
void RibbonTrail::MarkPositionsDirty()
{
    Drawable::OnMarkedDirty(node_);
    bufferDirty_ = true;
}
/// Return material.
Material* RibbonTrail::GetMaterial() const
{
    return batches_[0].material_;
}
/// Return material attribute.
ResourceRef RibbonTrail::GetMaterialAttr() const
{
    return GetResourceRef(batches_[0].material_, Material::GetTypeStatic());
}

}
