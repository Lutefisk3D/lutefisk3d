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

#include "Lutefisk3D/Container/Ptr.h"
#include "Lutefisk3D/Math/BoundingBox.h"
#include "Lutefisk3D/Math/StringHash.h"
#include "Lutefisk3D/Math/Quaternion.h"
#include "Lutefisk3D/Math/Matrix3x4.h"
#include <vector>
#include <QtCore/QString>
namespace Urho3D
{
class LUTEFISK3D_EXPORT Deserializer;
class LUTEFISK3D_EXPORT ResourceCache;
class LUTEFISK3D_EXPORT Serializer;
class LUTEFISK3D_EXPORT Node;

enum eBoneCollision : unsigned {
    BONECOLLISION_NONE = 0,
    BONECOLLISION_SPHERE = 1,
    BONECOLLISION_BOX = 2,
};

/// %Bone in a skeleton.
struct LUTEFISK3D_EXPORT Bone
{
    QString       name_;                                   //!< Bone name.
    StringHash    nameHash_;                               //!< Bone name hash.
    unsigned      parentIndex_     = 0;                    //!< Parent bone index.
    Vector3       initialPosition_ = Vector3::ZERO;        //!< Reset position.
    Quaternion    initialRotation_ = Quaternion::IDENTITY; //!< Reset rotation.
    Vector3       initialScale_    = Vector3::ONE;         //!< Reset scale.
    Matrix3x4     offsetMatrix_;                           //!< Offset matrix.
    BoundingBox   boundingBox_;                            //!< Local-space bounding box.
    WeakPtr<Node> node_;                                   //!< Scene node.
    float         radius_        = 0.0f;                   //!< Radius.
    bool          animated_      = true;                   //!< Animation enable flag.
    unsigned char collisionMask_ = 0;                      //!< Supported collision types.
};

/// Hierarchical collection of bones.
class LUTEFISK3D_EXPORT Skeleton
{
public:
    bool Load(Deserializer& source);
    bool Save(Serializer& dest) const;
    void Define(const Skeleton& src);
    void SetRootBoneIndex(unsigned index);
    void ClearBones();
    void Reset();
    /// Return all bones.
    const std::vector<Bone>& GetBones() const { return bones_; }
    /// Return modifiable bones.
    std::vector<Bone>& GetModifiableBones() { return bones_; }
    /// Return number of bones.
    unsigned GetNumBones() const { return bones_.size(); }

    Bone* GetRootBone();
    Bone* GetBone(unsigned index);
    Bone* GetBone(StringHash boneNameHash);
    void ResetSilent();

private:
    std::vector<Bone> bones_;                          //!< Bones.
    unsigned          rootBoneIndex_ = M_MAX_UNSIGNED; //!< Root bone index.
};
}
