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

#include "Animatable.h"

#include "ObjectAnimation.h"
#include "AttributeAnimationInfo.h"
#include "Lutefisk3D/Core/Context.h"
#include "Lutefisk3D/IO/Log.h"
#include "Lutefisk3D/Resource/ResourceCache.h"
#include "Lutefisk3D/Scene/SceneEvents.h"
#include "Lutefisk3D/Scene/ValueAnimation.h"
#include "Lutefisk3D/Resource/XMLElement.h"
#include "Lutefisk3D/Resource/JSONValue.h"

namespace Urho3D
{

extern const char* wrapModeNames[];

Animatable::Animatable(Context* context) :
    Serializable(context),
    animationEnabled_(true)
{
}

Animatable::~Animatable() {
}

void Animatable::RegisterObject(Context* context)
{
    URHO3D_MIXED_ACCESSOR_ATTRIBUTE("Object Animation", GetObjectAnimationAttr, SetObjectAnimationAttr, ResourceRef, ResourceRef{ObjectAnimation::GetTypeStatic()}, AM_DEFAULT);
}

/// Load from XML data. When setInstanceDefault is set to true, after setting the attribute value, store the value as instance's default value. Return true if successful.
bool Animatable::LoadXML(const XMLElement& source)
{
    if (!Serializable::LoadXML(source))
        return false;

    SetObjectAnimation(nullptr);
    attributeAnimationInfos_.clear();

    XMLElement elem = source.GetChild("objectanimation");
    if (elem)
    {
        SharedPtr<ObjectAnimation> objectAnimation(new ObjectAnimation(context_));
        if (!objectAnimation->LoadXML(elem))
            return false;

        SetObjectAnimation(objectAnimation);
    }

    elem = source.GetChild("attributeanimation");
    while (elem)
    {
        QString name = elem.GetAttribute("name");
        SharedPtr<ValueAnimation> attributeAnimation(new ValueAnimation(context_));
        if (!attributeAnimation->LoadXML(elem))
            return false;

        QString wrapModeString = source.GetAttribute("wrapmode");
        WrapMode wrapMode = WM_LOOP;
        for (int i = 0; i <= WM_CLAMP; ++i)
        {
            if (wrapModeString == wrapModeNames[i])
            {
                wrapMode = (WrapMode)i;
                break;
            }
        }

        float speed = elem.GetFloat("speed");
        SetAttributeAnimation(name, attributeAnimation, wrapMode, speed);

        elem = elem.GetNext("attributeanimation");
    }

    return true;
}
/// Load from JSON data. When setInstanceDefault is set to true, after setting the attribute value, store the value as instance's default value. Return true if successful.
bool Animatable::LoadJSON(const JSONValue& source)
{
    if (!Serializable::LoadJSON(source))
        return false;

    SetObjectAnimation(nullptr);
    attributeAnimationInfos_.clear();

    JSONValue value = source.Get("objectanimation");
    if (!value.IsNull())
    {
        SharedPtr<ObjectAnimation> objectAnimation(new ObjectAnimation(context_));
        if (!objectAnimation->LoadJSON(value))
            return false;

        SetObjectAnimation(objectAnimation);
    }

    JSONValue attributeAnimationValue = source.Get("attributeanimation");

    if (attributeAnimationValue.IsNull())
        return true;

    if (!attributeAnimationValue.IsObject())
    {
        URHO3D_LOGWARNING("'attributeanimation' value is present in JSON data, but is not a JSON object; skipping it");
        return true;
    }

    const JSONObject& attributeAnimationObject = attributeAnimationValue.GetObject();
    for (JSONObject::const_iterator it = attributeAnimationObject.begin(); it != attributeAnimationObject.end(); it++)
    {
        QString name = MAP_KEY(it);
        JSONValue value = MAP_VALUE(it);
        SharedPtr<ValueAnimation> attributeAnimation(new ValueAnimation(context_));
        if (!attributeAnimation->LoadJSON(MAP_VALUE(it)))
            return false;

        QString wrapModeString = source.Get("wrapmode").GetString();
        WrapMode wrapMode = WM_LOOP;
        for (int i = 0; i < WM_NUM_WRAP_MODES; ++i)
        {
            if (wrapModeString == wrapModeNames[i])
            {
                wrapMode = (WrapMode)i;
                break;
            }
        }

        float speed = value.Get("speed").GetFloat();
        SetAttributeAnimation(name, attributeAnimation, wrapMode, speed);

        it++; //BUG: very likely - needs verification
    }

    return true;
}
/// Save as XML data. Return true if successful.
bool Animatable::SaveXML(XMLElement& dest) const
{
    if (!Serializable::SaveXML(dest))
        return false;

    // Object animation without name
    if ((objectAnimation_ != nullptr) && objectAnimation_->GetName().isEmpty())
    {
        XMLElement elem = dest.CreateChild("objectanimation");
        if (!objectAnimation_->SaveXML(elem))
            return false;
    }

    for (auto &map_entry: attributeAnimationInfos_)
    {
        const SharedPtr<AttributeAnimationInfo> & _i(ELEMENT_VALUE(map_entry));
        ValueAnimation* attributeAnimation = _i->GetAnimation();
        if (attributeAnimation->GetOwner() != nullptr)
            continue;

        const AttributeInfo& attr = _i->GetAttributeInfo();
        XMLElement elem = dest.CreateChild("attributeanimation");
        elem.SetAttribute("name", attr.name_);
        if (!attributeAnimation->SaveXML(elem))
            return false;

        elem.SetAttribute("wrapmode", wrapModeNames[_i->GetWrapMode()]);
        elem.SetFloat("speed", _i->GetSpeed());
    }

    return true;
}
/// Save as JSON data. Return true if successful.
bool Animatable::SaveJSON(JSONValue& dest) const
{
    if (!Serializable::SaveJSON(dest))
        return false;

    // Object animation without name
    if ((objectAnimation_ != nullptr) && objectAnimation_->GetName().isEmpty())
    {
        JSONValue objectAnimationValue;
        if (!objectAnimation_->SaveJSON(objectAnimationValue))
            return false;
        dest.Set("objectanimation", objectAnimationValue);
    }

    JSONValue attributeAnimationValue;

    for (auto &i : attributeAnimationInfos_)
    {
        ValueAnimation* attributeAnimation = ELEMENT_VALUE(i)->GetAnimation();
        if (attributeAnimation->GetOwner() != nullptr)
            continue;

        const AttributeInfo& attr = ELEMENT_VALUE(i)->GetAttributeInfo();
        JSONValue attributeValue;
        attributeValue.Set("name", attr.name_);
        if (!attributeAnimation->SaveJSON(attributeValue))
            return false;

        attributeValue.Set("wrapmode", wrapModeNames[ELEMENT_VALUE(i)->GetWrapMode()]);
        attributeValue.Set("speed", ELEMENT_VALUE(i)->GetSpeed());

        attributeAnimationValue.Set(attr.name_, attributeValue);
    }

    return true;
}
/// Set automatic update of animation, default true.
void Animatable::SetAnimationEnabled(bool enable)
{
    if (objectAnimation_ != nullptr)
    {
        // In object animation there may be targets in hierarchy. Set same enable/disable state in all
        HashSet<Animatable*> targets;
        const HashMap<QString, SharedPtr<ValueAnimationInfo> >& infos = objectAnimation_->GetAttributeAnimationInfos();
        for (auto i = infos.begin(); i != infos.end(); ++i)
        {
            QString outName;
            Animatable* target = FindAttributeAnimationTarget(MAP_KEY(i), outName);
            if ((target != nullptr) && target != this)
                targets.insert(target);
        }

        for (auto i = targets.begin(); i != targets.end(); ++i)
            (*i)->animationEnabled_ = enable;
    }

    animationEnabled_ = enable;
}
/// Set time position of all attribute animations or an object animation manually. Automatic update should be disabled in this case.
void Animatable::SetAnimationTime(float time)
{
    if (objectAnimation_ != nullptr)
    {
        // In object animation there may be targets in hierarchy. Set same time in all
        const HashMap<QString, SharedPtr<ValueAnimationInfo> >& infos = objectAnimation_->GetAttributeAnimationInfos();
        for (auto i = infos.begin(); i != infos.end(); ++i)
        {
            QString outName;
            Animatable* target = FindAttributeAnimationTarget(MAP_KEY(i), outName);
            if (target != nullptr)
                target->SetAttributeAnimationTime(outName, time);
        }
    }
    else
    {
        for (auto i = attributeAnimationInfos_.begin(); i != attributeAnimationInfos_.end(); ++i)
            MAP_VALUE(i)->SetTime(time);
    }
}
/// Set object animation.
void Animatable::SetObjectAnimation(ObjectAnimation* objectAnimation)
{
    if (objectAnimation == objectAnimation_)
        return;

    if (objectAnimation_ != nullptr)
    {
        OnObjectAnimationRemoved(objectAnimation_);
        objectAnimation_->attributeAnimationAdded.Disconnect(this,&Animatable::HandleAttributeAnimationAdded);
        objectAnimation_->attributeAnimationRemoved.Disconnect(this,&Animatable::HandleAttributeAnimationAdded);
    }

    objectAnimation_ = objectAnimation;

    if (objectAnimation_ != nullptr)
    {
        OnObjectAnimationAdded(objectAnimation_);
        objectAnimation_->attributeAnimationAdded.Connect(this,&Animatable::HandleAttributeAnimationAdded);
        objectAnimation_->attributeAnimationRemoved.Connect(this,&Animatable::HandleAttributeAnimationAdded);
    }
}

void Animatable::SetAttributeAnimation(const QString& name, ValueAnimation* attributeAnimation, WrapMode wrapMode, float speed)
{
    AttributeAnimationInfo* info = GetAttributeAnimationInfo(name);

    if (attributeAnimation != nullptr)
    {
        if ((info != nullptr) && attributeAnimation == info->GetAnimation())
        {
            info->SetWrapMode(wrapMode);
            info->SetSpeed(speed);
            return;
        }

        // Get attribute info
        const AttributeInfo* attributeInfo = nullptr;
        if (info != nullptr)
            attributeInfo = &info->GetAttributeInfo();
        else
        {
            const std::vector<AttributeInfo>* attributes = GetAttributes();
            if (attributes == nullptr)
            {
                URHO3D_LOGERROR(GetTypeName() + " has no attributes");
                return;
            }

            for (const AttributeInfo & attribute : *attributes)
            {
                if (name == attribute.name_)
                {
                    attributeInfo = &attribute;
                    break;
                }
            }
        }

        if (attributeInfo == nullptr)
        {
            URHO3D_LOGERROR("Invalid name: " + name);
            return;
        }

        // Check value type is same with attribute type
        if (attributeAnimation->GetValueType() != attributeInfo->type_)
        {
            URHO3D_LOGERROR("Invalid value type");
            return;
        }

        // Add network attribute to set
        if ((attributeInfo->mode_ & AM_NET) != 0u)
            animatedNetworkAttributes_.insert(attributeInfo);

        attributeAnimationInfos_[name] = new AttributeAnimationInfo(this, *attributeInfo, attributeAnimation, wrapMode, speed);

        if (info == nullptr)
            OnAttributeAnimationAdded();
    }
    else
    {
        if (info == nullptr)
            return;

        // Remove network attribute from set
        if ((info->GetAttributeInfo().mode_ & AM_NET) != 0u)
            animatedNetworkAttributes_.remove(&info->GetAttributeInfo());

        attributeAnimationInfos_.erase(name);
        OnAttributeAnimationRemoved();
    }
}

void Animatable::SetAttributeAnimationWrapMode(const QString& name, WrapMode wrapMode)
{
    AttributeAnimationInfo* info = GetAttributeAnimationInfo(name);
    if (info != nullptr)
        info->SetWrapMode(wrapMode);
}

void Animatable::SetAttributeAnimationSpeed(const QString& name, float speed)
{
    AttributeAnimationInfo* info = GetAttributeAnimationInfo(name);
    if (info != nullptr)
        info->SetSpeed(speed);
}

void Animatable::SetAttributeAnimationTime(const QString& name, float time)
{
    AttributeAnimationInfo* info = GetAttributeAnimationInfo(name);
    if (info != nullptr)
        info->SetTime(time);
}

void Animatable::RemoveObjectAnimation()
{
    SetObjectAnimation(nullptr);
}

void Animatable::RemoveAttributeAnimation(const QString& name)
{
    SetAttributeAnimation(name, nullptr);
}
ObjectAnimation* Animatable::GetObjectAnimation() const
{
    return objectAnimation_;
}

ValueAnimation* Animatable::GetAttributeAnimation(const QString& name) const
{
    const AttributeAnimationInfo* info = GetAttributeAnimationInfo(name);
    return info != nullptr ? info->GetAnimation() : nullptr;
}

WrapMode Animatable::GetAttributeAnimationWrapMode(const QString& name) const
{
    const AttributeAnimationInfo* info = GetAttributeAnimationInfo(name);
    return info != nullptr ? info->GetWrapMode() : WM_LOOP;
}

float Animatable::GetAttributeAnimationSpeed(const QString& name) const
{
    const AttributeAnimationInfo* info = GetAttributeAnimationInfo(name);
    return info != nullptr ? info->GetSpeed() : 1.0f;
}

float Animatable::GetAttributeAnimationTime(const QString& name) const
{
    const AttributeAnimationInfo* info = GetAttributeAnimationInfo(name);
    return info != nullptr ? info->GetTime() : 0.0f;
}
void Animatable::SetObjectAnimationAttr(const ResourceRef& value)
{
    if (!value.name_.isEmpty())
    {
        SetObjectAnimation(context_->m_ResourceCache->GetResource<ObjectAnimation>(value.name_));
    }
}

ResourceRef Animatable::GetObjectAnimationAttr() const
{
    return GetResourceRef(objectAnimation_, ObjectAnimation::GetTypeStatic());
}

Animatable* Animatable::FindAttributeAnimationTarget(const QString& name, QString& outName)
{
    // Base implementation only handles self
    outName = name;
    return this;
}

void Animatable::SetObjectAttributeAnimation(const QString& name, ValueAnimation* attributeAnimation, WrapMode wrapMode, float speed)
{
    QString outName;
    Animatable* target = FindAttributeAnimationTarget(name, outName);
    if (target != nullptr)
        target->SetAttributeAnimation(outName, attributeAnimation, wrapMode, speed);
}

void Animatable::OnObjectAnimationAdded(ObjectAnimation* objectAnimation)
{
    if (objectAnimation == nullptr)
        return;

    // Set all attribute animations from the object animation
    const auto & attributeAnimationInfos = objectAnimation->GetAttributeAnimationInfos();
    for (auto iter=attributeAnimationInfos.begin(),fin=attributeAnimationInfos.end(); iter!=fin; ++iter)
    {
        const QString& name = MAP_KEY(iter);
        ValueAnimationInfo* info = MAP_VALUE(iter);
        SetObjectAttributeAnimation(name, info->GetAnimation(), info->GetWrapMode(), info->GetSpeed());
    }
}

void Animatable::OnObjectAnimationRemoved(ObjectAnimation* objectAnimation)
{
    if (objectAnimation == nullptr)
        return;

    // Just remove all attribute animations listed by the object animation
    const HashMap<QString, SharedPtr<ValueAnimationInfo> >& infos = objectAnimation->GetAttributeAnimationInfos();
    for (auto i = infos.begin(); i != infos.end(); ++i)
        SetObjectAttributeAnimation(MAP_KEY(i), 0, WM_LOOP, 1.0f);
}

void Animatable::UpdateAttributeAnimations(float timeStep)
{
    if (!animationEnabled_)
        return;
    // Keep weak pointer to self to check for destruction caused by event handling
    WeakPtr<Animatable> self(this);

    QStringList finishedNames;
    for (auto &elem: attributeAnimationInfos_)
    {
        SharedPtr<AttributeAnimationInfo> & i(ELEMENT_VALUE(elem));
        bool finished = i->Update(timeStep);
        // If self deleted as a result of an event sent during animation playback, nothing more to do
        if (self.Expired())
            return;
        if(finished)
            finishedNames.push_back(i->GetAttributeInfo().name_);
    }

    for(QString &name : finishedNames)
        SetAttributeAnimation(name, nullptr);
}

bool Animatable::IsAnimatedNetworkAttribute(const AttributeInfo& attrInfo) const
{
    return animatedNetworkAttributes_.find(&attrInfo) != animatedNetworkAttributes_.end();
}

AttributeAnimationInfo* Animatable::GetAttributeAnimationInfo(const QString& name) const
{
    const auto i = attributeAnimationInfos_.find(name);
    if (i != attributeAnimationInfos_.end())
        return MAP_VALUE(i);

    return nullptr;
}

void Animatable::HandleAttributeAnimationAdded(Object *anm,const QString &name)
{
    if (objectAnimation_ == nullptr)
        return;
    assert(anm==objectAnimation_);
    ValueAnimationInfo* info = objectAnimation_->GetAttributeAnimationInfo(name);
    if (info == nullptr)
        return;

    SetObjectAttributeAnimation(name, info->GetAnimation(), info->GetWrapMode(), info->GetSpeed());
}

void Animatable::HandleAttributeAnimationRemoved(Object *anm,const QString &name)
{
    if (objectAnimation_ == nullptr)
        return;
    assert(anm==objectAnimation_);

    SetObjectAttributeAnimation(name, nullptr, WM_LOOP, 1.0f);
}

}
