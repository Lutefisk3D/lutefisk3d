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

#include "Lutefisk3D/Scene/Serializable.h"
#include "Lutefisk3D/Container/HashMap.h"
#include "jlsignal/Signal.h"

namespace Urho3D
{
class JSONValue;
class Animatable;
class ValueAnimation;
class AttributeAnimationInfo;
class ObjectAnimation;
enum WrapMode : int;

/// Base class for animatable object, an animatable object can be set animation on it's attributes, or can be set an object animation to it.
class LUTEFISK3D_EXPORT Animatable : public Serializable, public jl::SignalObserver
{
    URHO3D_OBJECT(Animatable,Serializable)

public:
    /// Construct.
    Animatable(Context* context);
    /// Destruct.
    virtual ~Animatable();
    /// Register object factory.
    static void RegisterObject(Context* context);

    /// Load from XML data. When setInstanceDefault is set to true, after setting the attribute value, store the value as instance's default value. Return true if successful.
    virtual bool LoadXML(const XMLElement& source, bool setInstanceDefault = false) override;
    /// Save as XML data. Return true if successful.
    virtual bool SaveXML(XMLElement& dest) const override;
    /// Load from JSON data. When setInstanceDefault is set to true, after setting the attribute value, store the value as instance's default value. Return true if successful.
    virtual bool LoadJSON(const JSONValue& source, bool setInstanceDefault = false) override;
    /// Save as JSON data. Return true if successful.
    virtual bool SaveJSON(JSONValue& dest) const override;

    /// Set automatic update of animation, default true.
    void SetAnimationEnabled(bool enable);
    /// Set time position of all attribute animations or an object animation manually. Automatic update should be disabled in this case.
    void SetAnimationTime(float time);
    /// Set object animation.
    void SetObjectAnimation(ObjectAnimation* objectAnimation);
    /// Set attribute animation, default wrapMode is WM_LOOP
    void SetAttributeAnimation(const QString& name, ValueAnimation* attributeAnimation, WrapMode wrapMode = WrapMode(0), float speed = 1.0f);
    /// Set attribute animation wrap mode.
    void SetAttributeAnimationWrapMode(const QString& name, WrapMode wrapMode);
    /// Set attribute animation speed.
    void SetAttributeAnimationSpeed(const QString& name, float speed);
    /// Set attribute animation time position manually. Automatic update should be disabled in this case.
    void SetAttributeAnimationTime(const QString& name, float time);
    /// Remove object animation. Same as calling SetObjectAnimation with a null pointer.
    void RemoveObjectAnimation();
    /// Remove attribute animation. Same as calling SetAttributeAnimation with a null pointer.
    void RemoveAttributeAnimation(const QString& name);

    /// Return animation enabled.
    bool GetAnimationEnabled() const { return animationEnabled_; }
    /// Return object animation.
    ObjectAnimation* GetObjectAnimation() const;
    /// Return attribute animation.
    ValueAnimation* GetAttributeAnimation(const QString& name) const;
    /// Return attribute animation wrap mode.
    WrapMode GetAttributeAnimationWrapMode(const QString& name) const;
    /// Return attribute animation speed.
    float GetAttributeAnimationSpeed(const QString& name) const;
    /// Return attribute animation time position.
    float GetAttributeAnimationTime(const QString& name) const;

    /// Set object animation attribute.
    void SetObjectAnimationAttr(const ResourceRef& value);
    /// Return object animation attribute.
    ResourceRef GetObjectAnimationAttr() const;

protected:
    /// Handle attribute animation added.
    virtual void OnAttributeAnimationAdded() = 0;
    /// Handle attribute animation removed.
    virtual void OnAttributeAnimationRemoved() = 0;
    /// Find target of an attribute animation from object hierarchy by name.
    virtual Animatable* FindAttributeAnimationTarget(const QString& name, QString& outName);
    /// Set object attribute animation internal.
    void SetObjectAttributeAnimation(const QString& name, ValueAnimation* attributeAnimation, WrapMode wrapMode, float speed);
    /// Handle object animation added.
    void OnObjectAnimationAdded(ObjectAnimation* objectAnimation);
    /// Handle object animation removed.
    void OnObjectAnimationRemoved(ObjectAnimation* objectAnimation);
    /// Update attribute animations.
    void UpdateAttributeAnimations(float timeStep);
    /// Is animated network attribute.
    bool IsAnimatedNetworkAttribute(const AttributeInfo& attrInfo) const;
    /// Return attribute animation info.
    AttributeAnimationInfo* GetAttributeAnimationInfo(const QString& name) const;
    /// Handle attribute animation added.
    void HandleAttributeAnimationAdded(Object *anm, const QString &name);
    /// Handle attribute animation removed.
    void HandleAttributeAnimationRemoved(Object *anm, const QString &name);

    /// Animation enabled.
    bool animationEnabled_;
    /// Animation.
    SharedPtr<ObjectAnimation> objectAnimation_;
    /// Animated network attribute set.
    HashSet<const AttributeInfo*> animatedNetworkAttributes_;
    /// Attribute animation infos.
    HashMap<QString, SharedPtr<AttributeAnimationInfo> > attributeAnimationInfos_;
};

}
