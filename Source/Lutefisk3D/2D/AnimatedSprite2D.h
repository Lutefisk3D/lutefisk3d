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

#pragma once

#include "Lutefisk3D/2D/StaticSprite2D.h"

namespace Urho3D {
class LUTEFISK3D_EXPORT AnimationSet2D;

/// Loop mode.
enum LoopMode2D
{
    LM_DEFAULT = 0,  ///< Default, use animation's value.
    LM_FORCE_LOOPED, ///< Force looped.
    LM_FORCE_CLAMPED ///< Force clamped.
};
namespace Spriter
{
    class SpriterInstance;
}

/// Animated sprite component, it uses to play animation created by Spine (http://www.esotericsoftware.com) and Spriter (http://www.brashmonkey.com/).
class LUTEFISK3D_EXPORT AnimatedSprite2D : public StaticSprite2D
{
    URHO3D_OBJECT(AnimatedSprite2D,StaticSprite2D)

public:
    AnimatedSprite2D(Context* context);
    virtual ~AnimatedSprite2D();
    /// Register object factory.
    static void RegisterObject(Context* context);
    /// Handle enabled/disabled state change.
    void OnSetEnabled() override;

    /// Set animation set.
    void SetAnimationSet(AnimationSet2D* animationSet);
    /// Set entity name (skin name for spine, entity name for spriter).
    void SetEntity(const QString& name);
    /// Set animation by name and loop mode.
    void SetAnimation(const QString& name, LoopMode2D loopMode = LM_DEFAULT);
    /// Set loop mode.
    void SetLoopMode(LoopMode2D loopMode);
    /// Set speed.
    void SetSpeed(float speed);
    /// Return animation.
    AnimationSet2D* GetAnimationSet() const;
    /// Return entity name.
    const QString& GetEntity() const { return entity_; }
    /// Return animation name.
    const QString& GetAnimation() const { return animationName_; }
    /// Return loop mode.
    LoopMode2D GetLoopMode() const { return loopMode_; }
    /// Return speed.
    float GetSpeed() const { return speed_; }
    /// Set animation set attribute.
    void SetAnimationSetAttr(const ResourceRef& value);
    /// Return animation set attribute.
    ResourceRef GetAnimationSetAttr() const;
    /// Set animation by name.
    void SetAnimationAttr(const QString& name);

protected:
    /// Handle scene being assigned.
    void OnSceneSet(Scene* scene) override;
    /// Handle update vertices.
    void UpdateSourceBatches() override;
    /// Handle scene post update.
    void HandleScenePostUpdate(Scene *, float ts);
    /// Update animation.
    void UpdateAnimation(float timeStep);
    /// Handle set spriter animation.
    void SetSpriterAnimation();
    /// Update spriter animation.
    void UpdateSpriterAnimation(float timeStep);
    /// Update vertices for spriter animation.
    void UpdateSourceBatchesSpriter();

    float                                     speed_ = 1.0f;          //!< Speed.
    QString                                   entity_;                //!< Entity name.
    SharedPtr<AnimationSet2D>                 animationSet_;          //!< Animation set.
    QString                                   animationName_;         //!< Animation name.
    LoopMode2D                                loopMode_ = LM_DEFAULT; //!< Loop mode.
    std::unique_ptr<Spriter::SpriterInstance> spriterInstance_;       //!< Spriter instance.
};
}
