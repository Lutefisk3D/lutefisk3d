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

#include "Component.h"
#include "Lutefisk3D/Container/HashMap.h"
#include "ReplicationState.h"
#include "Scene.h"
#include "SceneEvents.h"
#ifdef LUTEFISK3D_PHYSICS
#include "Lutefisk3D/Physics/PhysicsWorld.h"
#endif
#ifdef LUTEFISK3D_URHO2D
#include "Lutefisk3D/2D/PhysicsWorld2D.h"
#endif
#include "Lutefisk3D/Resource/JSONValue.h"
namespace Urho3D
{

const char* autoRemoveModeNames[] = {
    "Disabled",
    "Component",
    "Node",
    nullptr
};

bool Component::Save(Serializer& dest) const
{
    // Write type and ID
    if (!dest.WriteStringHash(GetType()))
        return false;
    if (!dest.WriteUInt(id_))
        return false;

    // Write attributes
    return Animatable::Save(dest);
}

bool Component::SaveXML(XMLElement& dest) const
{
    // Write type and ID
    if (!dest.SetString("type", GetTypeName()))
        return false;
    if (!dest.SetUInt("id", id_))
        return false;

    // Write attributes
    return Animatable::SaveXML(dest);
}

bool Component::SaveJSON(JSONValue& dest) const
{
    // Write type and ID
    dest.Set("type", GetTypeName());
    dest.Set("id", id_);

    // Write attributes
    return Animatable::SaveJSON(dest);
}

void Component::MarkNetworkUpdate()
{
    if (!networkUpdate_ && id_ < FIRST_LOCAL_ID)
    {
        Scene* scene = GetScene();
        if (scene != nullptr)
        {
            scene->MarkNetworkUpdate(this);
            networkUpdate_ = true;
        }
    }
}

void Component::SetEnabled(bool enable)
{
    if (enable != enabled_)
    {
        enabled_ = enable;
        OnSetEnabled();
        MarkNetworkUpdate();

        // Send change event for the component
        Scene* scene = GetScene();
        if (scene != nullptr)
        {
            scene->componentEnabledChanged.Emit(scene,node_,this);
        }
    }
}

void Component::Remove()
{
    if (node_ != nullptr)
        node_->RemoveComponent(this);
}

Scene* Component::GetScene() const
{
    return node_ != nullptr ? node_->GetScene() : nullptr;
}

void Component::AddReplicationState(ComponentReplicationState* state)
{
    if (networkState_ == nullptr)
        AllocateNetworkState();

    networkState_->replicationStates_.push_back(state);
}

void Component::PrepareNetworkUpdate()
{
    if (networkState_ == nullptr)
        AllocateNetworkState();

    const std::vector<AttributeInfo>* attributes = networkState_->attributes_;
    if (attributes == nullptr)
        return;

    unsigned numAttributes = attributes->size();


    // Check for attribute changes
    for (unsigned i = 0; i < numAttributes; ++i)
    {
        const AttributeInfo& attr = attributes->at(i);

        if (animationEnabled_ && IsAnimatedNetworkAttribute(attr))
            continue;

        OnGetAttribute(attr, networkState_->currentValues_[i]);

        if (networkState_->currentValues_[i] != networkState_->previousValues_[i])
        {
            networkState_->previousValues_[i] = networkState_->currentValues_[i];

            // Mark the attribute dirty in all replication states that are tracking this component
            for (auto & elem : networkState_->replicationStates_)
            {
                ComponentReplicationState* compState = static_cast<ComponentReplicationState*>(elem);
                compState->dirtyAttributes_.Set(i);

                // Add component's parent node to the dirty set if not added yet
                NodeReplicationState* nodeState = compState->nodeState_;
                if (!nodeState->markedDirty_)
                {
                    nodeState->markedDirty_ = true;
                    nodeState->sceneState_->dirtyNodes_.insert(node_->GetID());
                }
            }
        }
    }

    networkUpdate_ = false;
}

void Component::CleanupConnection(Connection* connection)
{
    if (networkState_ == nullptr)
        return;
    auto iter=networkState_->replicationStates_.begin();
    auto fin=networkState_->replicationStates_.end();
    for ( ; iter!=fin; )
    {
        if ((*iter)->connection_ == connection)
            iter = networkState_->replicationStates_.erase(iter);
        else
            ++iter;
    }
}

void Component::OnAttributeAnimationAdded()
{
    if (attributeAnimationInfos_.size() == 1)
        GetScene()->attributeAnimationUpdate.Connect(this,&Component::HandleAttributeAnimationUpdate);
}

void Component::OnAttributeAnimationRemoved()
{
    if (attributeAnimationInfos_.isEmpty())
        GetScene()->attributeAnimationUpdate.Disconnect(this,&Component::HandleAttributeAnimationUpdate);
}

void Component::SetID(unsigned id)
{
    id_ = id;
}

void Component::SetNode(Node* node)
{
    node_ = node;
    OnNodeSet(node_);
}

Component* Component::GetComponent(StringHash type) const
{
    return node_ != nullptr ? node_->GetComponent(type) : nullptr;
}

bool Component::IsEnabledEffective() const
{
    return enabled_ && (node_ != nullptr) && node_->IsEnabled();
}

void Component::GetComponents(std::vector<Component*>& dest, StringHash type) const
{
    if (node_ != nullptr)
        node_->GetComponents(dest, type);
    else
        dest.clear();
}

void Component::HandleAttributeAnimationUpdate(Scene *,float ts)
{
    UpdateAttributeAnimations(ts);
}
Component* Component::GetFixedUpdateSource()
{
    Component* ret = nullptr;
    Scene* scene = GetScene();

    if (scene != nullptr)
    {
#ifdef LUTEFISK3D_PHYSICS
        ret = scene->GetComponent<PhysicsWorld>();
#endif
#ifdef LUTEFISK3D_URHO2D
        if (!ret)
            ret = scene->GetComponent<PhysicsWorld2D>();
#endif
    }

    return ret;
}
PhysicsSignals* Component::GetFixedSignalSource()
{
    Scene* scene = GetScene();
    if (scene == nullptr)
        return nullptr;
#ifdef LUTEFISK3D_PHYSICS
    PhysicsWorld *retw = scene->GetComponent<PhysicsWorld>();
    if(retw)
        return static_cast<PhysicsSignals *>(retw);
#endif
#ifdef LUTEFISK3D_URHO2D
    PhysicsWorld2D *ret2d = scene->GetComponent<PhysicsWorld2D>();
    if(ret2d)
        return static_cast<PhysicsSignals *>(ret2d);
#endif

    return nullptr;
}
void Component::DoAutoRemove(AutoRemoveMode mode)
{
    switch (mode)
    {
    case REMOVE_COMPONENT:
        Remove();
        return;

    case REMOVE_NODE:
        if (node_)
            node_->Remove();
        return;

    default:
        return;
    }
}
}
