//
// Copyright (c) 2008-2015 the Urho3D project.
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

#include "../Scene/Animatable.h"
#include "../Core/Context.h"
#include "../IO/Log.h"
#include "../Scene/ObjectAnimation.h"
#include "../Resource/ResourceCache.h"
#include "../Scene/SceneEvents.h"
#include "../Scene/ValueAnimation.h"
#include "../Resource/XMLElement.h"

namespace Urho3D
{

extern const char* wrapModeNames[];

AttributeAnimationInfo::AttributeAnimationInfo(Animatable* target, const AttributeInfo& attributeInfo, ValueAnimation* attributeAnimation, WrapMode wrapMode, float speed) :
    ValueAnimationInfo(target, attributeAnimation, wrapMode, speed),
    attributeInfo_(attributeInfo)
{
}

AttributeAnimationInfo::AttributeAnimationInfo(const AttributeAnimationInfo& other) :
    ValueAnimationInfo(other),
    attributeInfo_(other.attributeInfo_)
{
}

AttributeAnimationInfo::~AttributeAnimationInfo()
{
}

void AttributeAnimationInfo::ApplyValue(const Variant& newValue)
{
    Animatable* animatable = static_cast<Animatable*>(target_.Get());
    if (animatable)
    {
        animatable->OnSetAttribute(attributeInfo_, newValue);
        animatable->ApplyAttributes();
    }
}

Animatable::Animatable(Context* context) :
    Serializable(context),
    animationEnabled_(true)
{
}

Animatable::~Animatable()
{
}

void Animatable::RegisterObject(Context* context)
{
    MIXED_ACCESSOR_ATTRIBUTE("Object Animation", GetObjectAnimationAttr, SetObjectAnimationAttr, ResourceRef, ResourceRef(ObjectAnimation::GetTypeStatic()), AM_DEFAULT);
}

bool Animatable::LoadXML(const XMLElement& source, bool setInstanceDefault)
{
    if (!Serializable::LoadXML(source, setInstanceDefault))
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

bool Animatable::SaveXML(XMLElement& dest) const
{
    if (!Serializable::SaveXML(dest))
        return false;

    // Object animation without name
    if (objectAnimation_ && objectAnimation_->GetName().isEmpty())
    {
        XMLElement elem = dest.CreateChild("objectanimation");
        if (!objectAnimation_->SaveXML(elem))
            return false;
    }

    for (auto &map_entry: attributeAnimationInfos_)
    {
        const SharedPtr<AttributeAnimationInfo> & _i(ELEMENT_VALUE(map_entry));
        ValueAnimation* attributeAnimation = _i->GetAnimation();
        if (attributeAnimation->GetOwner())
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

void Animatable::SetObjectAnimation(ObjectAnimation* objectAnimation)
{
    if (objectAnimation == objectAnimation_)
        return;

    if (objectAnimation_)
    {
        OnObjectAnimationRemoved(objectAnimation_);
        UnsubscribeFromEvent(objectAnimation_, E_ATTRIBUTEANIMATIONADDED);
        UnsubscribeFromEvent(objectAnimation_, E_ATTRIBUTEANIMATIONREMOVED);
    }

    objectAnimation_ = objectAnimation;

    if (objectAnimation_)
    {
        OnObjectAnimationAdded(objectAnimation_);
        SubscribeToEvent(objectAnimation_, E_ATTRIBUTEANIMATIONADDED, HANDLER(Animatable, HandleAttributeAnimationAdded));
        SubscribeToEvent(objectAnimation_, E_ATTRIBUTEANIMATIONREMOVED, HANDLER(Animatable, HandleAttributeAnimationRemoved));
    }
}

void Animatable::SetAttributeAnimation(const QString& name, ValueAnimation* attributeAnimation, WrapMode wrapMode, float speed)
{
    AttributeAnimationInfo* info = GetAttributeAnimationInfo(name);

    if (attributeAnimation)
    {
        if (info && attributeAnimation == info->GetAnimation())
        {
            info->SetWrapMode(wrapMode);
            info->SetSpeed(speed);
            return;
        }

        // Get attribute info
        const AttributeInfo* attributeInfo = nullptr;
        if (info)
            attributeInfo = &info->GetAttributeInfo();
        else
        {
            const std::vector<AttributeInfo>* attributes = GetAttributes();
            if (!attributes)
            {
                LOGERROR(GetTypeName() + " has no attributes");
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

        if (!attributeInfo)
        {
            LOGERROR("Invalid name: " + name);
            return;
        }

        // Check value type is same with attribute type
        if (attributeAnimation->GetValueType() != attributeInfo->type_)
        {
            LOGERROR("Invalid value type");
            return;
        }

        // Add network attribute to set
        if (attributeInfo->mode_ & AM_NET)
            animatedNetworkAttributes_.insert(attributeInfo);

        attributeAnimationInfos_[name] = new AttributeAnimationInfo(this, *attributeInfo, attributeAnimation, wrapMode, speed);

        if (!info)
            OnAttributeAnimationAdded();
    }
    else
    {
        if (!info)
            return;

        // Remove network attribute from set
        if (info->GetAttributeInfo().mode_ & AM_NET)
            animatedNetworkAttributes_.remove(&info->GetAttributeInfo());

        attributeAnimationInfos_.remove(name);
        OnAttributeAnimationRemoved();
    }
}

void Animatable::SetAttributeAnimationWrapMode(const QString& name, WrapMode wrapMode)
{
    AttributeAnimationInfo* info = GetAttributeAnimationInfo(name);
    if (info)
        info->SetWrapMode(wrapMode);
}

void Animatable::SetAttributeAnimationSpeed(const QString& name, float speed)
{
    AttributeAnimationInfo* info = GetAttributeAnimationInfo(name);
    if (info)
        info->SetSpeed(speed);
}

ObjectAnimation* Animatable::GetObjectAnimation() const
{
    return objectAnimation_;
}

ValueAnimation* Animatable::GetAttributeAnimation(const QString& name) const
{
    const AttributeAnimationInfo* info = GetAttributeAnimationInfo(name);
    return info ? info->GetAnimation() : nullptr;
}

WrapMode Animatable::GetAttributeAnimationWrapMode(const QString& name) const
{
    const AttributeAnimationInfo* info = GetAttributeAnimationInfo(name);
    return info ? info->GetWrapMode() : WM_LOOP;
}

float Animatable::GetAttributeAnimationSpeed(const QString& name) const
{
    const AttributeAnimationInfo* info = GetAttributeAnimationInfo(name);
    return info ? info->GetSpeed() : 1.0f;
}

void Animatable::SetObjectAnimationAttr(const ResourceRef& value)
{
    if (!value.name_.isEmpty())
    {
        ResourceCache* cache = GetSubsystem<ResourceCache>();
        SetObjectAnimation(cache->GetResource<ObjectAnimation>(value.name_));
    }
}

ResourceRef Animatable::GetObjectAnimationAttr() const
{
    return GetResourceRef(objectAnimation_, ObjectAnimation::GetTypeStatic());
}

void Animatable::SetObjectAttributeAnimation(const QString& name, ValueAnimation* attributeAnimation, WrapMode wrapMode, float speed)
{
    SetAttributeAnimation(name, attributeAnimation, wrapMode, speed);
}

void Animatable::OnObjectAnimationAdded(ObjectAnimation* objectAnimation)
{
    if (!objectAnimation)
        return;

    // Set all attribute animations from the object animation
    const auto & attributeAnimationInfos = objectAnimation->GetAttributeAnimationInfos();
    for (auto info=attributeAnimationInfos.begin(),fin=attributeAnimationInfos.end(); info!=fin; ++info)
    {
        const QString& name = MAP_KEY(info);
        SetObjectAttributeAnimation(name,
                                    MAP_VALUE(info)->GetAnimation(), MAP_VALUE(info)->GetWrapMode(),
                                    MAP_VALUE(info)->GetSpeed());
    }
}

void Animatable::OnObjectAnimationRemoved(ObjectAnimation* objectAnimation)
{
    if (!objectAnimation)
        return;

    // Just remove all attribute animations from the object animation
    QStringList names;
    for (auto elem=attributeAnimationInfos_.begin(),fin=attributeAnimationInfos_.end(); elem!=fin; ++elem)
    {
        if (MAP_VALUE(elem)->GetAnimation()->GetOwner() == objectAnimation)
            names.push_back(MAP_KEY(elem));
    }

    for (unsigned i = 0; i < names.size(); ++i)
        SetObjectAttributeAnimation(names[i], nullptr, WM_LOOP, 1.0f);
}

void Animatable::UpdateAttributeAnimations(float timeStep)
{
    if (!animationEnabled_)
        return;

    QStringList finishedNames;
    for (auto &elem: attributeAnimationInfos_)
    {
        SharedPtr<AttributeAnimationInfo> & i(ELEMENT_VALUE(elem));
        if (i->Update(timeStep))
            finishedNames.push_back(i->GetAttributeInfo().name_);
    }

    for (unsigned i = 0; i < finishedNames.size(); ++i)
        SetAttributeAnimation(finishedNames[i], nullptr);
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

void Animatable::HandleAttributeAnimationAdded(StringHash eventType, VariantMap& eventData)
{
    if (!objectAnimation_)
        return;

    using namespace AttributeAnimationAdded;
    const QString& name =eventData[P_ATTRIBUTEANIMATIONNAME].GetString();

    ValueAnimationInfo* info = objectAnimation_->GetAttributeAnimationInfo(name);
    if (!info)
        return;

    SetObjectAttributeAnimation(name, info->GetAnimation(), info->GetWrapMode(), info->GetSpeed());
}

void Animatable::HandleAttributeAnimationRemoved(StringHash eventType, VariantMap& eventData)
{
    if (!objectAnimation_)
        return;

    using namespace AttributeAnimationRemoved;
    const QString& name = eventData[P_ATTRIBUTEANIMATIONNAME].GetString();

    SetObjectAttributeAnimation(name, nullptr, WM_LOOP, 1.0f);
}

}
