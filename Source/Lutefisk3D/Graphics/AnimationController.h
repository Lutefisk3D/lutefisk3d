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

#include "Lutefisk3D/IO/VectorBuffer.h"
#include "Lutefisk3D/Scene/Component.h"
#include "Lutefisk3D/Graphics/AnimationState.h"

namespace Urho3D
{

class AnimatedModel;
class Animation;
class AnimationState;
struct Bone;

/// Control data for an animation.
struct LUTEFISK3D_EXPORT AnimationControl
{
    /// Construct with defaults.
    AnimationControl() :
        speed_(1.0f),
        targetWeight_(0.0f),
        fadeTime_(0.0f),
        autoFadeTime_(0.0f),
        setTimeTtl_(0.0f),
        setWeightTtl_(0.0f),
        setTime_(0),
        setWeight_(0),
        setTimeRev_(0),
        setWeightRev_(0),
        removeOnCompletion_(true)
    {
    }

    /// Animation resource name.
    QString name_;
    /// Animation resource name hash.
    StringHash hash_;
    /// Animation speed.
    float speed_;
    /// Animation target weight.
    float targetWeight_;
    /// Animation weight fade time, 0 if no fade.
    float fadeTime_;
    /// Animation autofade on stop -time, 0 if disabled.
    float autoFadeTime_;
    /// Set time command time-to-live.
    float setTimeTtl_;
    /// Set weight command time-to-live.
    float setWeightTtl_;
    /// Set time command.
    unsigned short setTime_;
    /// Set weight command.
    unsigned char setWeight_;
    /// Set time command revision.
    unsigned char setTimeRev_;
    /// Set weight command revision.
    unsigned char setWeightRev_;
    /// Sets whether this should automatically be removed when it finishes playing.
    bool removeOnCompletion_;
};

/// %Component that drives an AnimatedModel's animations.
class LUTEFISK3D_EXPORT AnimationController : public Component
{
    URHO3D_OBJECT(AnimationController,Component);

public:
    /// Construct.
    AnimationController(Context* context);
    /// Destruct.
    virtual ~AnimationController() = default;

    /// Register object factory.
    static void RegisterObject(Context* context);

    /// Handle enabled/disabled state change.
    virtual void OnSetEnabled() override;

    /// Update the animations. Is called from HandleScenePostUpdate().
    virtual void Update(float timeStep);
    /// Play an animation and set full target weight. Name must be the full resource name. Return true on success.
    bool Play(const QString& name, unsigned char layer, bool looped, float fadeInTime = 0.0f);
    /// Play an animation, set full target weight and fade out all other animations on the same layer. Name must be the full resource name. Return true on success.
    bool PlayExclusive(const QString& name, unsigned char layer, bool looped, float fadeTime = 0.0f);
    /// Stop an animation. Zero fadetime is instant. Return true on success.
    bool Stop(const QString& name, float fadeOutTime = 0.0f);
    /// Stop all animations on a specific layer. Zero fadetime is instant.
    void StopLayer(unsigned char layer, float fadeOutTime = 0.0f);
    /// Stop all animations. Zero fadetime is instant.
    void StopAll(float fadeTime = 0.0f);
    /// Fade animation to target weight. Return true on success.
    bool Fade(const QString& name, float targetWeight, float fadeTime);
    /// Fade other animations on the same layer to target weight. Return true on success.
    bool FadeOthers(const QString& name, float targetWeight, float fadeTime);

    /// Set animation blending layer priority. Return true on success.
    bool SetLayer(const QString& name, unsigned char layer);
    /// Set animation start bone. Return true on success.
    bool SetStartBone(const QString& name, const QString& startBoneName);
    /// Set animation time position. Return true on success.
    bool SetTime(const QString& name, float time);
    /// Set animation weight. Return true on success.
    bool SetWeight(const QString& name, float weight);
    /// Set animation looping. Return true on success.
    bool SetLooped(const QString& name, bool enable);
    /// Set animation speed. Return true on success.
    bool SetSpeed(const QString& name, float speed);
    /// Set animation autofade at end (non-looped animations only.) Zero time disables. Return true on success.
    bool SetAutoFade(const QString& name, float fadeOutTime);
    /// Set whether an animation auto-removes on completion.
    bool SetRemoveOnCompletion(const QString& name, bool removeOnCompletion);
    /// Set animation blending mode. Return true on success.
    bool SetBlendMode(const QString& name, AnimationBlendMode mode);

    /// Return whether an animation is active. Note that non-looping animations that are being clamped at the end also return true.
    bool IsPlaying(const QString& name) const;
    /// Return whether any animation is active on a specific layer.
    bool IsPlaying(unsigned char layer) const;
    /// Return whether an animation is fading in.
    bool IsFadingIn(const QString& name) const;
    /// Return whether an animation is fading out.
    bool IsFadingOut(const QString& name) const;
    /// Return whether an animation is at its end. Will return false if the animation is not active at all.
    bool IsAtEnd(const QString& name) const;
    /// Return animation blending layer.
    unsigned char GetLayer(const QString& name) const;
    /// Return animation start bone, or null if no such animation.
    Bone* GetStartBone(const QString& name) const;
    /// Return animation start bone name, or empty string if no such animation.
    const QString& GetStartBoneName(const QString& name) const;
    /// Return animation time position.
    float GetTime(const QString& name) const;
    /// Return animation weight.
    float GetWeight(const QString& name) const;
    /// Return animation looping.
    bool IsLooped(const QString& name) const;
    /// Return animation blending mode.
    AnimationBlendMode GetBlendMode(const QString& name) const;
    /// Return animation length.
    float GetLength(const QString& name) const;
    /// Return animation speed.
    float GetSpeed(const QString& name) const;
    /// Return animation fade target weight.
    float GetFadeTarget(const QString& name) const;
    /// Return animation fade time.
    float GetFadeTime(const QString& name) const;
    /// Return animation autofade time.
    float GetAutoFade(const QString& name) const;
    /// Return whether animation auto-removes on completion, or false if no such animation.
    bool GetRemoveOnCompletion(const QString& name) const;
    /// Find an animation state by animation name.
    AnimationState* GetAnimationState(const QString& name) const;
    /// Find an animation state by animation name hash.
    AnimationState* GetAnimationState(StringHash nameHash) const;
    /// Return the animation control structures for inspection.
    const std::vector<AnimationControl>& GetAnimations() const { return animations_; }

    /// Set animation control structures attribute.
    void SetAnimationsAttr(const VariantVector& value);
    /// Set animations attribute for network replication.
    void SetNetAnimationsAttr(const std::vector<unsigned char>& value);
    /// Set node animation states attribute.
    void SetNodeAnimationStatesAttr(const VariantVector& value);
    /// Return animation control structures attribute.
    VariantVector GetAnimationsAttr() const;
    /// Return animations attribute for network replication.
    const std::vector<unsigned char>& GetNetAnimationsAttr() const;
    /// Return node animation states attribute.
    VariantVector GetNodeAnimationStatesAttr() const;

protected:
    /// Handle scene being assigned.
    virtual void OnSceneSet(Scene* scene) override;

private:
    /// Add an animation state either to AnimatedModel or as a node animation.
    AnimationState* AddAnimationState(Animation* animation);
    /// Remove an animation state.
    void RemoveAnimationState(AnimationState* state);
    /// Find the internal index and animation state of an animation.
    void FindAnimation(const QString& name, unsigned& index, AnimationState*& state) const;
    /// Handle scene post-update event.
    void HandleScenePostUpdate(Scene *, float ts);

    /// Animation control structures.
    std::vector<AnimationControl> animations_;
    /// Node hierarchy mode animation states.
    std::vector<SharedPtr<AnimationState> > nodeAnimationStates_;
    /// Attribute buffer for network replication.
    mutable VectorBuffer attrBuffer_;
};

}
