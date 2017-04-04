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

#include "ObjectAnimation.h"

#include "Lutefisk3D/Core/Context.h"
#include "SceneEvents.h"
#include "ValueAnimation.h"
#include "ValueAnimationInfo.h"
#include "Lutefisk3D/Resource/XMLFile.h"
#include "Lutefisk3D/Resource/JSONFile.h"

namespace Urho3D
{

const char* wrapModeNames[] =
{
    "Loop",
    "Once",
    "Clamp",
    nullptr
};

ObjectAnimation::ObjectAnimation(Context* context) :
    Resource(context)
{
}

void ObjectAnimation::RegisterObject(Context* context)
{
    context->RegisterFactory<ObjectAnimation>();
}

bool ObjectAnimation::BeginLoad(Deserializer& source)
{
    XMLFile xmlFile(context_);
    if (!xmlFile.Load(source))
        return false;

    return LoadXML(xmlFile.GetRoot());
}

bool ObjectAnimation::Save(Serializer& dest) const
{
    XMLFile xmlFile(context_);

    XMLElement rootElem = xmlFile.CreateRoot("objectanimation");
    if (!SaveXML(rootElem))
        return false;

    return xmlFile.Save(dest);
}

bool ObjectAnimation::LoadXML(const XMLElement& source)
{
    attributeAnimationInfos_.clear();

    XMLElement animElem;
    animElem = source.GetChild("attributeanimation");
    while (animElem)
    {
        QString name = animElem.GetAttribute("name");

        SharedPtr<ValueAnimation> animation(new ValueAnimation(context_));
        if (!animation->LoadXML(animElem))
            return false;

        QString wrapModeString = animElem.GetAttribute("wrapmode");
        WrapMode wrapMode = WM_LOOP;
        for (int i = 0; i <= WM_CLAMP; ++i)
        {
            if (wrapModeString == wrapModeNames[i])
            {
                wrapMode = (WrapMode)i;
                break;
            }
        }

        float speed = animElem.GetFloat("speed");
        AddAttributeAnimation(name, animation, wrapMode, speed);

        animElem = animElem.GetNext("attributeanimation");
    }

    return true;
}

bool ObjectAnimation::SaveXML(XMLElement& dest) const
{

    for (auto elem=attributeAnimationInfos_.begin(),fin=attributeAnimationInfos_.end(); elem != fin; ++elem)
    {
        XMLElement animElem = dest.CreateChild("attributeanimation");
        animElem.SetAttribute("name", MAP_KEY(elem));

        const ValueAnimationInfo* info = MAP_VALUE(elem);
        if (!info->GetAnimation()->SaveXML(animElem))
            return false;

        animElem.SetAttribute("wrapmode", wrapModeNames[info->GetWrapMode()]);
        animElem.SetFloat("speed", info->GetSpeed());
    }

    return true;
}

bool ObjectAnimation::LoadJSON(const JSONValue& source)
{
    attributeAnimationInfos_.clear();

    JSONValue attributeAnimationsValue = source.Get("attributeanimations");
    if (attributeAnimationsValue.IsNull())
        return true;
    if (!attributeAnimationsValue.IsObject())
        return true;

    const JSONObject& attributeAnimationsObject = attributeAnimationsValue.GetObject();

    for (const auto & ob : attributeAnimationsObject)
    {
        QString name = ELEMENT_KEY(ob);
        JSONValue value = ELEMENT_VALUE(ob);
        SharedPtr<ValueAnimation> animation(new ValueAnimation(context_));
        if (!animation->LoadJSON(value))
            return false;

        QString wrapModeString = value.Get("wrapmode").GetString();
        WrapMode wrapMode = WM_LOOP;
        for (int i = 0; i <= WM_CLAMP; ++i)
        {
            if (wrapModeString == wrapModeNames[i])
            {
                wrapMode = (WrapMode)i;
                break;
            }
        }

        float speed = value.Get("speed").GetFloat();
        AddAttributeAnimation(name, animation, wrapMode, speed);
    }

    return true;
}

bool ObjectAnimation::SaveJSON(JSONValue& dest) const
{
    JSONValue attributeAnimationsValue;

    for (auto &elem : attributeAnimationInfos_)
    {
        JSONValue animValue;
        animValue.Set("name", ELEMENT_KEY(elem));

        const ValueAnimationInfo* info = ELEMENT_VALUE(elem);
        if (!info->GetAnimation()->SaveJSON(animValue))
            return false;

        animValue.Set("wrapmode", wrapModeNames[info->GetWrapMode()]);
        animValue.Set("speed", (float) info->GetSpeed());

        attributeAnimationsValue.Set(ELEMENT_KEY(elem), animValue);
    }

    dest.Set("attributeanimations", attributeAnimationsValue);
    return true;
}

void ObjectAnimation::AddAttributeAnimation(const QString& name, ValueAnimation* attributeAnimation, WrapMode wrapMode, float speed)
{
    if (attributeAnimation == nullptr)
        return;

    attributeAnimation->SetOwner(this);
    attributeAnimationInfos_[name] = new ValueAnimationInfo(attributeAnimation, wrapMode, speed);

    SendAttributeAnimationAddedEvent(name);
}

void ObjectAnimation::RemoveAttributeAnimation(const QString& name)
{
    HashMap<QString, SharedPtr<ValueAnimationInfo> >::iterator i = attributeAnimationInfos_.find(name);
    if (i != attributeAnimationInfos_.end())
    {
        SendAttributeAnimationRemovedEvent(name);
        MAP_VALUE(i)->GetAnimation()->SetOwner(nullptr);
        attributeAnimationInfos_.erase(i);
    }
}

void ObjectAnimation::RemoveAttributeAnimation(ValueAnimation* attributeAnimation)
{
    if (attributeAnimation == nullptr)
        return;

    for (auto i = attributeAnimationInfos_.begin(); i != attributeAnimationInfos_.end(); ++i)
    {
        if (MAP_VALUE(i)->GetAnimation() == attributeAnimation)
        {
            SendAttributeAnimationRemovedEvent(MAP_KEY(i));
            attributeAnimation->SetOwner(nullptr);
            attributeAnimationInfos_.erase(i);
            return;
        }
    }
}

ValueAnimation* ObjectAnimation::GetAttributeAnimation(const QString& name) const
{
    ValueAnimationInfo* info = GetAttributeAnimationInfo(name);
    return info != nullptr ? info->GetAnimation() : nullptr;
}

WrapMode ObjectAnimation::GetAttributeAnimationWrapMode(const QString& name) const
{
    ValueAnimationInfo* info = GetAttributeAnimationInfo(name);
    return info != nullptr ? info->GetWrapMode() : WM_LOOP;
}

float ObjectAnimation::GetAttributeAnimationSpeed(const QString& name) const
{
    ValueAnimationInfo* info = GetAttributeAnimationInfo(name);
    return info != nullptr ? info->GetSpeed() : 1.0f;
}

ValueAnimationInfo* ObjectAnimation::GetAttributeAnimationInfo(const QString& name) const
{
    auto i = attributeAnimationInfos_.find(name);
    if (i != attributeAnimationInfos_.end())
        return MAP_VALUE(i);
    return nullptr;
}

void ObjectAnimation::SendAttributeAnimationAddedEvent(const QString& name)
{
    using namespace AttributeAnimationAdded;
    VariantMap& eventData = GetEventDataMap();
    eventData[P_OBJECTANIMATION] = this;
    eventData[P_ATTRIBUTEANIMATIONNAME] = name;
    SendEvent(E_ATTRIBUTEANIMATIONADDED, eventData);
}

void ObjectAnimation::SendAttributeAnimationRemovedEvent(const QString& name)
{
    using namespace AttributeAnimationRemoved;
    VariantMap& eventData = GetEventDataMap();
    eventData[P_OBJECTANIMATION] = this;
    eventData[P_ATTRIBUTEANIMATIONNAME] = name;
    SendEvent(E_ATTRIBUTEANIMATIONREMOVED, eventData);
}

}
