#ifndef ATTRIBUTEANIMATIONINFO_H
#define ATTRIBUTEANIMATIONINFO_H

#include "ValueAnimationInfo.h"

namespace Urho3D {
class Animatable;
struct AttributeInfo;
/// Attribute animation instance.
class AttributeAnimationInfo : public ValueAnimationInfo
{
public:
    AttributeAnimationInfo(Animatable* animatable, const AttributeInfo& attributeInfo, ValueAnimation* attributeAnimation, WrapMode wrapMode, float speed);
    AttributeAnimationInfo(const AttributeAnimationInfo& other) = default;
    ~AttributeAnimationInfo() override = default;

    /// Return attribute information.
    const AttributeInfo& GetAttributeInfo() const { return attributeInfo_; }

protected:
    void ApplyValue(const Variant& newValue) override;

private:
    /// Attribute information.
    const AttributeInfo& attributeInfo_;
};
}

#endif // ATTRIBUTEANIMATIONINFO_H
