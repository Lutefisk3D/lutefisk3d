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

#include "Lutefisk3D/Scene/AnimationDefs.h"
#include "Lutefisk3D/Resource/Resource.h"
#include "Lutefisk3D/Scene/SceneEvents.h"
#include "Lutefisk3D/Scene/ValueAnimationInfo.h"
namespace Urho3D
{

class ValueAnimation;
//class ValueAnimationInfo;
class XMLElement;
class JSONValue;
/// Object animation class, an object animation includes one or more attribute animations and their wrap mode and speed for an Animatable object.
class LUTEFISK3D_EXPORT ObjectAnimation : public Resource, public ObjectAnimationSignals
{
    URHO3D_OBJECT(ObjectAnimation,Resource )

public:
    ObjectAnimation(Context* context);
    virtual ~ObjectAnimation() = default;

    static void RegisterObject(Context* context);

    bool BeginLoad(Deserializer& source) override;
    bool Save(Serializer& dest) const override;
    bool LoadXML(const XMLElement& source);
    bool SaveXML(XMLElement& dest) const;
    bool LoadJSON(const JSONValue& source);
    bool SaveJSON(JSONValue& dest) const;
    void AddAttributeAnimation(const QString& name, ValueAnimation* attributeAnimation, WrapMode wrapMode = WM_LOOP, float speed = 1.0f);
    void RemoveAttributeAnimation(const QString& name);
    void RemoveAttributeAnimation(ValueAnimation* attributeAnimation);
    ValueAnimation* GetAttributeAnimation(const QString& name) const;
    WrapMode GetAttributeAnimationWrapMode(const QString& name) const;
    float GetAttributeAnimationSpeed(const QString& name) const;
    /// Return all attribute animations infos.
    const HashMap<QString, SharedPtr<ValueAnimationInfo> >& GetAttributeAnimationInfos() const { return attributeAnimationInfos_; }
    ValueAnimationInfo* GetAttributeAnimationInfo(const QString& name) const;

private:
    /// Name to attribute animation info mapping.
    HashMap<QString, SharedPtr<ValueAnimationInfo> > attributeAnimationInfos_;
};

}
