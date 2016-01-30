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

#include "../Container/Ptr.h"
#include "../Math/Quaternion.h"
#include "../Math/Vector3.h"
#include "../Resource/Resource.h"

namespace Urho3D
{

/// Skeletal animation keyframe.
struct AnimationKeyFrame
{
    /// Construct.
    AnimationKeyFrame() :
        time_(0.0f),
        scale_(Vector3::ONE)
    {
    }
    /// Keyframe time.
    float time_;
    /// Bone position.
    Vector3 position_;
    /// Bone rotation.
    Quaternion rotation_;
    /// Bone scale.
    Vector3 scale_;
};

/// Skeletal animation track, stores keyframes of a single bone.
struct AnimationTrack
{
    /// Construct.
    AnimationTrack() :
        channelMask_(0)
    {
    }

    /// Assign keyframe at index.
    void SetKeyFrame(unsigned index, const AnimationKeyFrame& command);
    /// Add a keyframe at the end.
    void AddKeyFrame(const AnimationKeyFrame& keyFrame);
    /// Insert a keyframe at index.
    void InsertKeyFrame(unsigned index, const AnimationKeyFrame& keyFrame);
    /// Remove a keyframe at index.
    void RemoveKeyFrame(unsigned index);
    /// Remove all keyframes.
    void RemoveAllKeyFrames();
    
    /// Return keyframe at index, or null if not found.
    AnimationKeyFrame* GetKeyFrame(unsigned index);
    /// Return number of keyframes.
    unsigned GetNumKeyFrames() const { return keyFrames_.size(); }
    /// Return keyframe index based on time and previous index.
    void GetKeyFrameIndex(float time, unsigned& index) const;

    /// Bone or scene node name.
    QString name_;
    /// Name hash.
    StringHash nameHash_;
    /// Bitmask of included data (position, rotation, scale.)
    unsigned char channelMask_;
    /// Keyframes.
    std::vector<AnimationKeyFrame> keyFrames_;
};

/// %Animation trigger point.
struct AnimationTriggerPoint
{
    /// Construct.
    AnimationTriggerPoint() :
        time_(0.0f)
    {
    }

    /// Trigger time.
    float time_;
    /// Trigger data.
    Variant data_;
};

static const unsigned char CHANNEL_POSITION = 0x1;
static const unsigned char CHANNEL_ROTATION = 0x2;
static const unsigned char CHANNEL_SCALE = 0x4;

/// Skeletal animation resource.
class Animation : public Resource
{
    URHO3D_OBJECT(Animation,Resource);

public:
    /// Construct.
    Animation(Context* context);
    /// Destruct.
    virtual ~Animation();
    /// Register object factory.
    static void RegisterObject(Context* context);

    /// Load resource from stream. May be called from a worker thread. Return true if successful.
    virtual bool BeginLoad(Deserializer& source) override;
    /// Save resource. Return true if successful.
    virtual bool Save(Serializer& dest) const override;

    /// Set animation name.
    void SetAnimationName(const QString& name);
    /// Set animation length.
    void SetLength(float length);
    /// Create and return a track by name. If track by same name already exists, returns the existing.
    AnimationTrack* CreateTrack(const QString& name);
    /// Remove a track by name. Return true if was found and removed successfully. This is unsafe if the animation is currently used in playback.
    bool RemoveTrack(const QString& name);
    /// Remove all tracks. This is unsafe if the animation is currently used in playback.
    void RemoveAllTracks();
    /// Set all animation tracks.
    void SetTracks(const HashMap<StringHash, AnimationTrack>& tracks);
    /// Set a trigger point at index.
    void SetTrigger(unsigned index, const AnimationTriggerPoint& trigger);
    /// Add a trigger point.
    void AddTrigger(const AnimationTriggerPoint& trigger);
    /// Add a trigger point.
    void AddTrigger(float time, bool timeIsNormalized, const Variant& data);
    /// Remove a trigger point by index.
    void RemoveTrigger(unsigned index);
    /// Remove all trigger points.
    void RemoveAllTriggers();
    /// Resize trigger point vector.
    void SetNumTriggers(unsigned num);

    /// Return animation name.
    const QString& GetAnimationName() const { return animationName_; }
    /// Return animation name hash.
    StringHash GetAnimationNameHash() const { return animationNameHash_; }
    /// Return animation length.
    float GetLength() const { return length_; }
    /// Return all animation tracks.
    const HashMap<StringHash, AnimationTrack>& GetTracks() const { return tracks_; }
    /// Return number of animation tracks.
    unsigned GetNumTracks() const { return tracks_.size(); }
    /// Return animation track by name.
    AnimationTrack* GetTrack(const QString& name);
    /// Return animation track by name hash.
    AnimationTrack* GetTrack(StringHash nameHash);
    /// Return animation trigger points.
    const std::vector<AnimationTriggerPoint>& GetTriggers() const { return triggers_; }
    /// Return number of animation trigger points.
    unsigned GetNumTriggers() const {return triggers_.size(); }
    /// Return a trigger point by index.
    AnimationTriggerPoint* GetTrigger(unsigned index);

private:
    /// Animation name.
    QString animationName_;
    /// Animation name hash.
    StringHash animationNameHash_;
    /// Animation length.
    float length_;
    /// Animation tracks.
    HashMap<StringHash, AnimationTrack> tracks_;
    /// Animation trigger points.
    std::vector<AnimationTriggerPoint> triggers_;
};

}
