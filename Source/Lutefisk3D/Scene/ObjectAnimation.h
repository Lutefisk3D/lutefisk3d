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

#include "../Scene/AnimationDefs.h"
#include "../Resource/Resource.h"

namespace Urho3D
{

class ValueAnimation;
class ValueAnimationInfo;
class XMLElement;
class JSONValue;

/// Object animation class, an object animation include one or more attribute animations and theirs wrap mode and speed for an Animatable object.
class ObjectAnimation : public Resource
{
    URHO3D_OBJECT(ObjectAnimation,Resource );

public:
    /// Construct.
    ObjectAnimation(Context* context);
    /// Destruct.
    virtual ~ObjectAnimation() = default;
    /// Register object factory.
    static void RegisterObject(Context* context);

    /// Load resource from stream. May be called from a worker thread. Return true if successful.
    virtual bool BeginLoad(Deserializer& source) override;
    /// Save resource. Return true if successful.
    virtual bool Save(Serializer& dest) const override;
    /// Load from XML data. Return true if successful.
    bool LoadXML(const XMLElement& source);
    /// Save as XML data. Return true if successful.
    bool SaveXML(XMLElement& dest) const;
    /// Load from JSON data. Return true if successful.
    bool LoadJSON(const JSONValue& source);
    /// Save as JSON data. Return true if successful.
    bool SaveJSON(JSONValue& dest) const;

    /// Add attribute animation, attribute name can in following format: "attribute" or "#0/#1/attribute" or ""#0/#1/@component#1/attribute.
    void AddAttributeAnimation(const QString& name, ValueAnimation* attributeAnimation, WrapMode wrapMode = WM_LOOP, float speed = 1.0f);
    /// Remove attribute animation, attribute name can in following format: "attribute" or "#0/#1/attribute" or ""#0/#1/@component#1/attribute.
    void RemoveAttributeAnimation(const QString& name);
    /// Remove attribute animation.
    void RemoveAttributeAnimation(ValueAnimation* attributeAnimation);

    /// Return attribute animation by name.
    ValueAnimation* GetAttributeAnimation(const QString& name) const;
    /// Return attribute animation wrap mode by name.
    WrapMode GetAttributeAnimationWrapMode(const QString& name) const;
    /// Return attribute animation speed by name.
    float GetAttributeAnimationSpeed(const QString& name) const;
    /// Return all attribute animations infos.
    const HashMap<QString, SharedPtr<ValueAnimationInfo> >& GetAttributeAnimationInfos() const { return attributeAnimationInfos_; }
    /// Return attribute animation info by name.
    ValueAnimationInfo* GetAttributeAnimationInfo(const QString& name) const;

private:
    /// Send attribute animation added event.
    void SendAttributeAnimationAddedEvent(const QString& name);
    /// Send attribute animation remove event.
    void SendAttributeAnimationRemovedEvent(const QString& name);

    /// Name to attribute animation info mapping.
    HashMap<QString, SharedPtr<ValueAnimationInfo> > attributeAnimationInfos_;
};

}
