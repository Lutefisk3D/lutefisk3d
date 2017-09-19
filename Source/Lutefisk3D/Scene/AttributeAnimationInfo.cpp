#include "AttributeAnimationInfo.h"

#include "Animatable.h"

using namespace Urho3D;
template class Urho3D::HashSet<const AttributeInfo*>;
template class Urho3D::HashMap<QString, SharedPtr<AttributeAnimationInfo> >;

AttributeAnimationInfo::AttributeAnimationInfo(Animatable* target, const AttributeInfo& attributeInfo, ValueAnimation* attributeAnimation, WrapMode wrapMode, float speed) :
    ValueAnimationInfo(target, attributeAnimation, wrapMode, speed),
    attributeInfo_(attributeInfo)
{
}

/// Apply new animation value to the target object. Called by Update().
void AttributeAnimationInfo::ApplyValue(const Variant& newValue)
{
    Animatable* animatable = static_cast<Animatable*>(target_.Get());
    if (animatable != nullptr)
    {
        animatable->OnSetAttribute(attributeInfo_, newValue);
        animatable->ApplyAttributes();
    }
}
