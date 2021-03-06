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

#include "Lutefisk3D/Core/Attribute.h"
#include "Lutefisk3D/Container/Ptr.h"
#include "Lutefisk3D/Math/StringHash.h"
#include "Lutefisk3D/Container/HashMap.h"


#include <QtCore/QSet>
#include <cstring>

namespace Urho3D
{

static const unsigned MAX_NETWORK_ATTRIBUTES = 64;

class Component;
class Connection;
class Node;
class Scene;

struct ReplicationState;
struct ComponentReplicationState;
struct NodeReplicationState;
struct SceneReplicationState;
/// Dirty attribute bits structure for network replication.
struct LUTEFISK3D_EXPORT DirtyBits
{
    /// Construct empty.
    DirtyBits() :
        count_(0)
    {
        memset(data_, 0, MAX_NETWORK_ATTRIBUTES / 8);
    }

    /// Copy-construct.
    DirtyBits(const DirtyBits& bits) :
        count_(bits.count_)
    {
        memcpy(data_, bits.data_, MAX_NETWORK_ATTRIBUTES / 8);
    }

    /// Set a bit.
    void Set(unsigned index)
    {
        if (index < MAX_NETWORK_ATTRIBUTES)
        {
            unsigned byteIndex = index >> 3;
            unsigned bit = 1U << (index & 7);
            if ((data_[byteIndex] & bit) == 0)
            {
                data_[byteIndex] |= bit;
                ++count_;
            }
        }
    }

    /// Clear a bit.
    void Clear(unsigned index)
    {
        if (index < MAX_NETWORK_ATTRIBUTES)
        {
            unsigned byteIndex = index >> 3;
            unsigned bit = 1U << (index & 7);
            if ((data_[byteIndex] & bit) != 0)
            {
                data_[byteIndex] &= ~bit;
                --count_;
            }
        }
    }

    /// Clear all bits.
    void ClearAll()
    {
        memset(data_, 0, MAX_NETWORK_ATTRIBUTES / 8);
        count_ = 0;
    }

    /// Return if bit is set.
    bool IsSet(unsigned index) const
    {
        if (index < MAX_NETWORK_ATTRIBUTES)
        {
            unsigned byteIndex = index >> 3;
            unsigned bit = 1U << (index & 7);
            return (data_[byteIndex] & bit) != 0;
        }
        else
            return false;
    }

    /// Return number of set bits.
    unsigned Count() const { return count_; }

    /// Bit data.
    unsigned char data_[MAX_NETWORK_ATTRIBUTES / 8];
    /// Number of set bits.
    unsigned char count_;
};

/// Per-object attribute state for network replication, allocated on demand.
struct LUTEFISK3D_EXPORT NetworkState
{
    /// Cached network attribute infos.
    const std::vector<AttributeInfo>* attributes_;
    /// Current network attribute values.
    std::vector<Variant> currentValues_;
    /// Previous network attribute values.
    std::vector<Variant> previousValues_;
    /// Replication states that are tracking this object.
    std::vector<ReplicationState*> replicationStates_;
    /// Previous user variables.
    VariantMap previousVars_;
    /// Bitmask for intercepting network messages. Used on the client only.
    uint64_t interceptMask_=0;
};

/// Base class for per-user network replication states.
struct LUTEFISK3D_EXPORT ReplicationState
{
    /// Parent network connection.
    Connection* connection_;
};

/// Per-user component network replication state.
struct LUTEFISK3D_EXPORT ComponentReplicationState : public ReplicationState
{
    /// Parent node replication state.
    NodeReplicationState* nodeState_;
    /// Link to the actual component.
    WeakPtr<Component> component_;
    /// Dirty attribute bits.
    DirtyBits dirtyAttributes_;
};

/// Per-user node network replication state.
struct LUTEFISK3D_EXPORT NodeReplicationState : public ReplicationState
{
    /// Parent scene replication state.
    SceneReplicationState* sceneState_;
    /// Link to the actual node.
    WeakPtr<Node> node_;
    /// Dirty attribute bits.
    DirtyBits dirtyAttributes_;
    /// Dirty user vars.
    QSet<StringHash> dirtyVars_;
    /// Components by ID.
    HashMap<unsigned, ComponentReplicationState> componentStates_;
    /// Interest management priority accumulator.
    float priorityAcc_ = 0.0f;
    /// Whether exists in the SceneState's dirty set.
    bool markedDirty_ = false;
};

/// Per-user scene network replication state.
struct LUTEFISK3D_EXPORT SceneReplicationState : public ReplicationState
{
    /// Nodes by ID.
    HashMap<unsigned, NodeReplicationState> nodeStates_;
    /// Dirty node IDs.
    QSet<unsigned> dirtyNodes_;

    void Clear()
    {
        nodeStates_.clear();
        dirtyNodes_.clear();
    }
};

}
