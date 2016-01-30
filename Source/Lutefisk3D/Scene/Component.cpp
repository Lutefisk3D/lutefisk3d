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

#include "Component.h"
#include "../Container/HashMap.h"
#include "ReplicationState.h"
#include "Scene.h"
#include "SceneEvents.h"
#ifdef LUTEFISK3D_PHYSICS
#include "../Physics/PhysicsWorld.h"
#endif
#ifdef LUTEFISK3D_URHO2D
#include "../Urho2D/PhysicsWorld2D.h"
#endif
#include "../Resource/JSONValue.h"
namespace Urho3D
{

Component::Component(Context* context) :
    Animatable(context),
    node_(nullptr),
    id_(0),
    networkUpdate_(false),
    enabled_(true)
{
}

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

void Component::GetDependencyNodes(std::vector<Node*>& dest)
{
}

void Component::DrawDebugGeometry(DebugRenderer* debug, bool depthTest)
{
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
            using namespace ComponentEnabledChanged;

            VariantMap& eventData = GetEventDataMap();
            eventData[P_SCENE] = scene;
            eventData[P_NODE] = node_;
            eventData[P_COMPONENT] = this;

            scene->SendEvent(E_COMPONENTENABLEDCHANGED, eventData);
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

    if (networkState_->currentValues_.size() != numAttributes)
    {
        networkState_->currentValues_.resize(numAttributes);
        networkState_->previousValues_.resize(numAttributes);

        // Copy the default attribute values to the previous state as a starting point
        for (unsigned i = 0; i < numAttributes; ++i)
            networkState_->previousValues_[i] = attributes->at(i).defaultValue_;
    }

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
    if (networkState_ != nullptr)
    {
        for (unsigned i = networkState_->replicationStates_.size() - 1; i < networkState_->replicationStates_.size(); --i)
        {
            if (networkState_->replicationStates_[i]->connection_ == connection)
                networkState_->replicationStates_.erase(networkState_->replicationStates_.begin() + i);
        }
    }
}

void Component::OnAttributeAnimationAdded()
{
    if (attributeAnimationInfos_.size() == 1)
        SubscribeToEvent(GetScene(), E_ATTRIBUTEANIMATIONUPDATE, URHO3D_HANDLER(Component, HandleAttributeAnimationUpdate));
}

void Component::OnAttributeAnimationRemoved()
{
    if (attributeAnimationInfos_.isEmpty())
        UnsubscribeFromEvent(GetScene(), E_ATTRIBUTEANIMATIONUPDATE);
}

void Component::OnNodeSet(Node* node)
{
}

void Component::OnSceneSet(Scene* scene)
{
}

void Component::OnMarkedDirty(Node* node)
{
}

void Component::OnNodeSetEnabled(Node* node)
{
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

void Component::HandleAttributeAnimationUpdate(StringHash eventType, VariantMap& eventData)
{
    using namespace AttributeAnimationUpdate;

    UpdateAttributeAnimations(eventData[P_TIMESTEP].GetFloat());
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
}
