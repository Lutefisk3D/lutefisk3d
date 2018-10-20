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
#include "Node.h"

#include "Component.h"
#include "ObjectAnimation.h"
#include "ReplicationState.h"
#include "Scene.h"
#include "SceneResolver.h"
#include "SceneEvents.h"
#include "SmoothedTransform.h"
#include "UnknownComponent.h"
#include "Lutefisk3D/Core/Context.h"
#include "Lutefisk3D/Core/Profiler.h"
#include "Lutefisk3D/IO/Log.h"
#include "Lutefisk3D/IO/MemoryBuffer.h"
#include "Lutefisk3D/IO/VectorBuffer.h"
#include "Lutefisk3D/Resource/XMLFile.h"
#include "Lutefisk3D/Resource/JSONFile.h"
#ifdef LUTEFISK3D_PHYSICS
#include "Lutefisk3D/2D/PhysicsEvents2D.h"
#endif
namespace Urho3D
{
/// Internal implementation structure for less performance-critical Node variables.
struct LUTEFISK3D_EXPORT NodePrivate
{
    /// Nodes this node depends on for network updates.
    std::vector<Node*> dependencyNodes_;
    /// Attribute buffer for network updates.
    mutable VectorBuffer attrBuffer_;
    /// Node listeners.
    std::vector<WeakPtr<Component> > listeners_;
    /// Network owner connection.
    Connection* owner_;
    /// Name.
    QString name_;
    /// Tag strings.
    QStringList tags_;
    /// Name hash.
    StringHash nameHash_;
    void notifyListeners(Node *n) {
        // Notify listener components first, then mark child nodes
        size_t count_to_notify=listeners_.size();
        for (size_t current=0; current<count_to_notify;)
        {
            WeakPtr<Component> &c(listeners_[current]);
            if (c != nullptr)
            {
                c->OnMarkedDirty(n);
                ++current;
            }
            // If listener has expired, erase from list (swap with the last element to avoid O(n^2) behavior)
            else
            {
                c = std::move(listeners_[--count_to_notify]);
            }
        }
        if(count_to_notify!=listeners_.size())
            listeners_.erase(listeners_.begin()+(listeners_.size()-count_to_notify),listeners_.end());
    }
    void notifyListenersEnabled(Node *holder) {
        // Notify listener components of the state change
        size_t count_to_notify=listeners_.size();
        for (size_t current=0; current<count_to_notify;)
        {
            WeakPtr<Component> &c(listeners_[current]);
            if (c != nullptr)
            {
                c->OnNodeSetEnabled(holder);
                ++current;
            }
            // If listener has expired, erase from list (swap and pop since we don't care about order)
            else
            {
                c = std::move(listeners_[--count_to_notify]);
            }
        }
        if(count_to_notify!=listeners_.size())
            listeners_.erase(listeners_.begin()+(listeners_.size()-count_to_notify),listeners_.end());
    }
    void addListener(Component *component)
    {
        // Check for not adding twice
        for (auto & elem : listeners_)
        {
            if (elem == component)
                return;
        }
        listeners_.push_back(WeakPtr<Component>(component));
    }
    void removeListener(Component *component)
    {
        for (std::vector<WeakPtr<Component> >::iterator i = listeners_.begin(); i != listeners_.end(); ++i)
        {
            if (*i == component)
            {
                listeners_.erase(i);
                return;
            }
        }
    }
};
Node::Node(Context* context) :
    Animatable(context),
    worldTransform_(Matrix3x4::IDENTITY),
    dirty_(false),
    enabled_(true),
    enabledPrev_(true),
    networkUpdate_(false),
    parent_(nullptr),
    scene_(nullptr),
    id_(0),
    position_(Vector3::ZERO),
    rotation_(Quaternion::IDENTITY),
    scale_(Vector3::ONE),
    worldRotation_(Quaternion::IDENTITY),
    impl_(new NodePrivate)
{
    impl_->owner_ = 0;
}

Node::~Node()
{
    RemoveAllChildren();
    RemoveAllComponents();

    // Remove from the scene
    if (scene_ != nullptr)
        scene_->NodeRemoved(this);
#ifdef LUTEFISK3D_PHYSICS
    delete physics2dSignals_;
#endif
}

void Node::RegisterObject(Context* context)
{
    context->RegisterFactory<Node>();

    URHO3D_ACCESSOR_ATTRIBUTE("Is Enabled", IsEnabled, SetEnabled, bool, true, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Name", GetName, SetName, QString, QString(), AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Tags", GetTags, SetTags, QStringList, Variant::emptyStringVector, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Position", GetPosition, SetPosition, Vector3, Vector3::ZERO, AM_FILE);
    URHO3D_ACCESSOR_ATTRIBUTE("Rotation", GetRotation, SetRotation, Quaternion, Quaternion::IDENTITY, AM_FILE);
    URHO3D_ACCESSOR_ATTRIBUTE("Scale", GetScale, SetScale, Vector3, Vector3::ONE, AM_DEFAULT);
    URHO3D_ATTRIBUTE("Variables", VariantMap, vars_, Variant::emptyVariantMap, AM_FILE); // Network replication of vars uses custom data
    URHO3D_ACCESSOR_ATTRIBUTE("Network Position", GetNetPositionAttr, SetNetPositionAttr, Vector3, Vector3::ZERO, AM_NET | AM_LATESTDATA | AM_NOEDIT);
    URHO3D_ACCESSOR_ATTRIBUTE("Network Rotation", GetNetRotationAttr, SetNetRotationAttr, std::vector<unsigned char>, Variant::emptyBuffer, AM_NET | AM_LATESTDATA | AM_NOEDIT);
    URHO3D_ACCESSOR_ATTRIBUTE("Network Parent Node", GetNetParentAttr, SetNetParentAttr, std::vector<unsigned char>, Variant::emptyBuffer, AM_NET | AM_NOEDIT);
}

bool Node::Load(Deserializer& source)
{
    SceneResolver resolver;

    // Read own ID. Will not be applied, only stored for resolving possible references
    unsigned nodeID = source.ReadUInt();
    resolver.AddNode(nodeID, this);

    // Read attributes, components and child nodes
    bool success = Load(source, resolver);
    if (success)
    {
        resolver.Resolve();
        ApplyAttributes();
    }

    return success;
}

bool Node::Save(Serializer& dest) const
{
    // Write node ID
    if (!dest.WriteUInt(id_))
        return false;

    // Write attributes
    if (!Animatable::Save(dest))
        return false;

    // Write components
    dest.WriteVLE(GetNumPersistentComponents());
    for (unsigned i = 0; i < components_.size(); ++i)
    {
        Component* component = components_[i];
        if (component->IsTemporary())
            continue;

        // Create a separate buffer to be able to skip failing components during deserialization
        VectorBuffer compBuffer;
        if (!component->Save(compBuffer))
            return false;
        dest.WriteVLE(compBuffer.GetSize());
        dest.Write(compBuffer.GetData(), compBuffer.GetSize());
    }

    // Write child nodes
    dest.WriteVLE(GetNumPersistentChildren());
    for (unsigned i = 0; i < children_.size(); ++i)
    {
        Node* node = children_[i];
        if (node->IsTemporary())
            continue;

        if (!node->Save(dest))
            return false;
    }

    return true;
}

bool Node::LoadXML(const XMLElement& source)
{
    SceneResolver resolver;

    // Read own ID. Will not be applied, only stored for resolving possible references
    unsigned nodeID = source.GetUInt("id");
    resolver.AddNode(nodeID, this);

    // Read attributes, components and child nodes
    bool success = LoadXML(source, resolver);
    if (success)
    {
        resolver.Resolve();
        ApplyAttributes();
    }

    return success;
}

bool Node::LoadJSON(const JSONValue& source)
{
    SceneResolver resolver;

    // Read own ID. Will not be applied, only stored for resolving possible references
    unsigned nodeID = source.Get("id").GetUInt();
    resolver.AddNode(nodeID, this);

    // Read attributes, components and child nodes
    bool success = LoadJSON(source, resolver);
    if (success)
    {
        resolver.Resolve();
        ApplyAttributes();
    }

    return success;
}
bool Node::SaveXML(XMLElement& dest) const
{
    // Write node ID
    if (!dest.SetUInt("id", id_))
        return false;

    // Write attributes
    if (!Animatable::SaveXML(dest))
        return false;

    // Write components
    for (unsigned i = 0; i < components_.size(); ++i)
    {
        Component* component = components_[i];
        if (component->IsTemporary())
            continue;

        XMLElement compElem = dest.CreateChild("component");
        if (!component->SaveXML(compElem))
            return false;
    }

    // Write child nodes
    for (unsigned i = 0; i < children_.size(); ++i)
    {
        Node* node = children_[i];
        if (node->IsTemporary())
            continue;

        XMLElement childElem = dest.CreateChild("node");
        if (!node->SaveXML(childElem))
            return false;
    }
    return true;
}

bool Node::SaveJSON(JSONValue& dest) const
{
    // Write node ID
    dest.Set("id", id_);

    // Write attributes
    if (!Animatable::SaveJSON(dest))
        return false;

    // Write components
    JSONArray componentsArray;
    componentsArray.reserve(components_.size());
    for (unsigned i = 0; i < components_.size(); ++i)
    {
        Component* component = components_[i];
        if (component->IsTemporary())
            continue;

        JSONValue compVal;
        if (!component->SaveJSON(compVal))
            return false;
        componentsArray.push_back(compVal);
    }
    dest.Set("components", componentsArray);

    // Write child nodes
    JSONArray childrenArray;
    childrenArray.reserve(children_.size());
    for (unsigned i = 0; i < children_.size(); ++i)
    {
        Node* node = children_[i];
        if (node->IsTemporary())
            continue;

        JSONValue childVal;
        if (!node->SaveJSON(childVal))
            return false;
        childrenArray.push_back(childVal);
    }
    dest.Set("children", childrenArray);

    return true;
}

void Node::ApplyAttributes()
{
    for (unsigned i = 0; i < components_.size(); ++i)
        components_[i]->ApplyAttributes();

    for (unsigned i = 0; i < children_.size(); ++i)
        children_[i]->ApplyAttributes();
}

void Node::MarkNetworkUpdate()
{
    if (!networkUpdate_ && (scene_ != nullptr) && id_ < FIRST_LOCAL_ID)
    {
        scene_->MarkNetworkUpdate(this);
        networkUpdate_ = true;
    }
}

void Node::AddReplicationState(NodeReplicationState* state)
{
    if (networkState_ == nullptr)
        AllocateNetworkState();

    networkState_->replicationStates_.push_back(state);
}

bool Node::SaveXML(Serializer& dest, const QString& indentation) const
{
    SharedPtr<XMLFile> xml(new XMLFile(context_));
    XMLElement rootElem = xml->CreateRoot("node");
    if (!SaveXML(rootElem))
        return false;

    return xml->Save(dest, indentation);
}

bool Node::SaveJSON(Serializer& dest, const QString& indentation) const
{
    SharedPtr<JSONFile> json(new JSONFile(context_));
    JSONValue& rootElem = json->GetRoot();

    if (!SaveJSON(rootElem))
        return false;

    return json->Save(dest, indentation);
}

void Node::SetName(const QString& name)
{
    if (name != impl_->name_)
    {
        impl_->name_ = name;
        impl_->nameHash_ = name;

        MarkNetworkUpdate();

        // Send change event
        if (scene_ != nullptr)
        {
            scene_->nodeNameChagned(scene_, this);
        }
    }
}

void Node::SetTags(const QStringList& tags)
{
    RemoveAllTags();
    AddTags(tags);
    // MarkNetworkUpdate() already called in RemoveAllTags() / AddTags()
}

void Node::AddTag(const QString & tag)
{
    // Check if tag empty or already added
    if (tag.isEmpty() || HasTag(tag))
        return;

    // Add tag
    impl_->tags_.push_back(tag);

    // Cache
    scene_->NodeTagAdded(this, tag);

    // Send event
    scene_->nodeTagAdded(scene_,this,tag);

    // Sync
    MarkNetworkUpdate();
}

void Node::AddTags(const QString & tags, char separator)
{
    QStringList tagVector = tags.split(separator);
    AddTags(tagVector);
}

void Node::AddTags(const QStringList& tags)
{
    // This is OK, as MarkNetworkUpdate() early-outs when called multiple times
    for (unsigned i = 0; i < tags.size(); ++i)
        AddTag(tags[i]);
}

bool Node::RemoveTag(const QString & tag)
{
    bool removed = impl_->tags_.removeAll(tag) != 0;

    // Nothing to do
    if (!removed)
        return false;

    // Scene cache update
    if (scene_ != nullptr)
    {
        scene_->NodeTagRemoved(this, tag);
        // Send event
        scene_->nodeTagRemoved(scene_,this,tag);
    }

    // Sync
    MarkNetworkUpdate();
    return true;
}

void Node::RemoveAllTags()
{
    // Clear old scene cache
    if (scene_ != nullptr)
    {
        for (unsigned i = 0; i < impl_->tags_.size(); ++i)
        {
            scene_->NodeTagRemoved(this, impl_->tags_[i]);

            // Send event
            scene_->nodeTagRemoved(scene_,this,impl_->tags_[i]);
        }
    }

    impl_->tags_.clear();

    // Sync
    MarkNetworkUpdate();
}
void Node::SetPosition(const Vector3& position)
{
    position_ = position;
    MarkDirty();

    MarkNetworkUpdate();
}

void Node::SetRotation(const Quaternion& rotation)
{
    rotation_ = rotation;
    MarkDirty();

    MarkNetworkUpdate();
}

void Node::SetDirection(const Vector3& direction)
{
    SetRotation(Quaternion(Vector3::FORWARD, direction));
}

void Node::SetScale(float scale)
{
    SetScale(Vector3(scale, scale, scale));
}

void Node::SetScale(const Vector3& scale)
{
    scale_ = scale;
    // Prevent exact zero scale e.g. from momentary edits as this may cause division by zero
    // when decomposing the world transform matrix
    if (scale_.x_ == 0.0f)
        scale_.x_ = M_EPSILON;
    if (scale_.y_ == 0.0f)
        scale_.y_ = M_EPSILON;
    if (scale_.z_ == 0.0f)
        scale_.z_ = M_EPSILON;

    MarkDirty();
    MarkNetworkUpdate();
}

void Node::SetTransform(const Vector3& position, const Quaternion& rotation)
{
    position_ = position;
    rotation_ = rotation;
    MarkDirty();

    MarkNetworkUpdate();
}

void Node::SetTransform(const Vector3& position, const Quaternion& rotation, float scale)
{
    SetTransform(position, rotation, Vector3(scale, scale, scale));
}

void Node::SetTransform(const Vector3& position, const Quaternion& rotation, const Vector3& scale)
{
    position_ = position;
    rotation_ = rotation;
    scale_ = scale;
    MarkDirty();

    MarkNetworkUpdate();
}

void Node::SetTransform(const Matrix3x4& matrix)
{
    SetTransform(matrix.Translation(), matrix.Rotation(), matrix.Scale());
}
void Node::SetWorldPosition(const Vector3& position)
{
    SetPosition((parent_ == scene_ || (parent_ == nullptr)) ? position : parent_->GetWorldTransform().Inverse() * position);
}

void Node::SetWorldRotation(const Quaternion& rotation)
{
    SetRotation((parent_ == scene_ || (parent_ == nullptr)) ? rotation : parent_->GetWorldRotation().Inverse() * rotation);
}

void Node::SetWorldDirection(const Vector3& direction)
{
    Vector3 localDirection = (parent_ == scene_ || (parent_ == nullptr)) ? direction : parent_->GetWorldRotation().Inverse() * direction;
    SetRotation(Quaternion(Vector3::FORWARD, localDirection));
}

void Node::SetWorldScale(float scale)
{
    SetWorldScale(Vector3(scale, scale, scale));
}

void Node::SetWorldScale(const Vector3& scale)
{
    SetScale((parent_ == scene_ || (parent_ == nullptr)) ? scale : scale / parent_->GetWorldScale());
}

void Node::SetWorldTransform(const Vector3& position, const Quaternion& rotation)
{
    SetWorldPosition(position);
    SetWorldRotation(rotation);
}

void Node::SetWorldTransform(const Vector3& position, const Quaternion& rotation, float scale)
{
    SetWorldPosition(position);
    SetWorldRotation(rotation);
    SetWorldScale(scale);
}

void Node::SetWorldTransform(const Vector3& position, const Quaternion& rotation, const Vector3& scale)
{
    SetWorldPosition(position);
    SetWorldRotation(rotation);
    SetWorldScale(scale);
}

void Node::Translate(const Vector3& delta, TransformSpace space)
{
    switch (space)
    {
        case TS_LOCAL:
            // Note: local space translation disregards local scale for scale-independent movement speed
            position_ += rotation_ * delta;
            break;

        case TS_PARENT:
            position_ += delta;
            break;

        case TS_WORLD:
            position_ += (parent_ == scene_ || (parent_ == nullptr))
                             ? delta
                             : parent_->GetWorldTransform().Inverse() * Vector4(delta, 0.0f);
            break;
    }

    MarkDirty();

    MarkNetworkUpdate();
}

void Node::Rotate(const Quaternion& delta, TransformSpace space)
{
    switch (space)
    {
        case TS_LOCAL:
            rotation_ = (rotation_ * delta).Normalized();
            break;

        case TS_PARENT:
            rotation_ = (delta * rotation_).Normalized();
            break;

        case TS_WORLD:
            if (parent_ == scene_ || (parent_ == nullptr))
                rotation_ = (delta * rotation_).Normalized();
            else
            {
                Quaternion worldRotation = GetWorldRotation();
                rotation_ = rotation_ * worldRotation.Inverse() * delta * worldRotation;
            }
            break;
    }

    MarkDirty();

    MarkNetworkUpdate();
}

void Node::RotateAround(const Vector3& point, const Quaternion& delta, TransformSpace space)
{
    Vector3 parentSpacePoint;
    Quaternion oldRotation = rotation_;

    switch (space)
    {
        case TS_LOCAL:
            parentSpacePoint = GetTransform() * point;
            rotation_ = (rotation_ * delta).Normalized();
            break;

        case TS_PARENT:
            parentSpacePoint = point;
            rotation_ = (delta * rotation_).Normalized();
            break;

        case TS_WORLD:
            if (parent_ == scene_ || (parent_ == nullptr))
            {
                parentSpacePoint = point;
                rotation_ = (delta * rotation_).Normalized();
            }
            else
            {
                parentSpacePoint = parent_->GetWorldTransform().Inverse() * point;
                Quaternion worldRotation = GetWorldRotation();
                rotation_ = rotation_ * worldRotation.Inverse() * delta * worldRotation;
            }
            break;
    }

    Vector3 oldRelativePos = oldRotation.Inverse() * (position_ - parentSpacePoint);
    position_ = rotation_ * oldRelativePos + parentSpacePoint;

    MarkDirty();

    MarkNetworkUpdate();
}

void Node::Yaw(float angle, TransformSpace space)
{
    Rotate(Quaternion(angle, Vector3::UP), space);
}

void Node::Pitch(float angle, TransformSpace space)
{
    Rotate(Quaternion(angle, Vector3::RIGHT), space);
}

void Node::Roll(float angle, TransformSpace space)
{
    Rotate(Quaternion(angle, Vector3::FORWARD), space);
}

bool Node::LookAt(const Vector3& target, const Vector3& up, TransformSpace space)
{
    Vector3 worldSpaceTarget;

    switch (space)
    {
        case TS_LOCAL:
            worldSpaceTarget = GetWorldTransform() * target;
            break;

        case TS_PARENT:
            worldSpaceTarget = (parent_ == scene_ || (parent_ == nullptr)) ? target : parent_->GetWorldTransform() * target;
            break;

        case TS_WORLD:
            worldSpaceTarget = target;
            break;
    }

    Vector3 lookDir = worldSpaceTarget - GetWorldPosition();
    // Check if target is very close, in that case can not reliably calculate lookat direction
    if (lookDir.Equals(Vector3::ZERO))
        return false;
    Quaternion newRotation;
    // Do nothing if setting look rotation failed
    if (!newRotation.FromLookRotation(lookDir, up))
        return false;

    SetWorldRotation(newRotation);
    return true;
}

void Node::Scale(float scale)
{
    Scale(Vector3(scale, scale, scale));
}

void Node::Scale(const Vector3& scale)
{
    scale_ *= scale;
    MarkDirty();

    MarkNetworkUpdate();
}

void Node::SetEnabled(bool enable)
{
    SetEnabled(enable, false, true);
}

void Node::SetDeepEnabled(bool enable)
{
    SetEnabled(enable, true, false);
}

void Node::ResetDeepEnabled()
{
    SetEnabled(enabledPrev_, false, false);

    for (const SharedPtr<Node> &i : children_)
        i->ResetDeepEnabled();
}

void Node::SetEnabledRecursive(bool enable)
{
    SetEnabled(enable, true, true);
}

void Node::SetOwner(Connection* owner)
{
    impl_->owner_ = owner;
}

void Node::MarkDirty()
{
    Node *cur = this;
    for (;;)
    {
        // Precondition:
        // a) whenever a node is marked dirty, all its children are marked dirty as well.
        // b) whenever a node is cleared from being dirty, all its parents must have been
        //    cleared as well.
        // Therefore if we are recursing here to mark this node dirty, and it already was,
        // then all children of this node must also be already dirty, and we don't need to
        // reflag them again.
        if (cur->dirty_)
            return;
        cur->dirty_ = true;

        cur->impl_->notifyListeners(cur);

        // Tail call optimization: Don't recurse to mark the first child dirty, but
        // instead process it in the context of the current function. If there are more
        // than one child, then recurse to the excess children.
        std::vector< SharedPtr<Node> >::iterator i = cur->children_.begin();
        if (i != cur->children_.end())
        {
            Node *next = *i;
            for (++i; i != cur->children_.end(); ++i)
                (*i)->MarkDirty();
            cur = next;
        }
        else
            return;
    }
}

Node* Node::CreateChild(const QString& name, CreateMode mode, unsigned id, bool temporary)
{
    Node* newNode = CreateChild(id, mode, temporary);
    newNode->SetName(name);
    return newNode;
}

Node* Node::CreateTemporaryChild(const QString& name, CreateMode mode, unsigned id)
{
    return CreateChild(name, mode, id, true);
}

void Node::AddChild(Node* node, unsigned index)
{
    // Check for illegal or redundant parent assignment
    if ((node == nullptr) || node == this || node->parent_ == this)
        return;
    // Check for possible cyclic parent assignment
    if (IsChildOf(node))
        return;
    auto location = (index != M_MAX_UNSIGNED) ? children_.begin()+index : children_.end();
    // Keep a shared ptr to the node while transfering
    SharedPtr<Node> nodeShared(node);
    Node* oldParent = node->parent_;
    if (oldParent != nullptr)
    {
        // If old parent is in different scene, perform the full removal
        if (oldParent->GetScene() != scene_)
            oldParent->RemoveChild(node);
        else
        {
            if (scene_ != nullptr)
            {
                // Otherwise do not remove from the scene during reparenting, just send the necessary change event
                scene_->nodeRemoved(scene_,oldParent,node);
            }
            auto it = std::find(oldParent->children_.begin(),oldParent->children_.end(),nodeShared);
            if(it!=oldParent->children_.end())
                oldParent->children_.erase(it);
        }
    }

    // Add to the child vector, then add to the scene if not added yet
    children_.emplace(location, nodeShared);
    if ((scene_ != nullptr) && node->GetScene() != scene_)
        scene_->NodeAdded(node);

    node->parent_ = this;
    node->MarkDirty();
    node->MarkNetworkUpdate();
    // If the child node has components, also mark network update on them to ensure they have a valid NetworkState
    for (std::vector<SharedPtr<Component> >::iterator i = node->components_.begin(); i != node->components_.end(); ++i)
        (*i)->MarkNetworkUpdate();

    // Send change event
    if (scene_ != nullptr)
    {
        scene_->nodeAdded(scene_,this,node);
    }
    g_sceneSignals.nodeAdded(scene_,this,node);
}

void Node::RemoveChild(Node* node)
{
    if (node == nullptr)
        return;

    for (auto i = children_.begin(); i != children_.end(); ++i)
    {
        if (*i == node)
        {
            RemoveChild(i);
            return;
        }
    }
}

void Node::RemoveAllChildren()
{
    RemoveChildren(true, true, true);
}

void Node::RemoveChildren(bool removeReplicated, bool removeLocal, bool recursive)
{
    unsigned numRemoved = 0;

    for (unsigned i = children_.size() - 1; i < children_.size(); --i)
    {
        bool remove = false;
        Node* childNode = children_[i];

        if (recursive)
            childNode->RemoveChildren(removeReplicated, removeLocal, true);
        if (childNode->GetID() < FIRST_LOCAL_ID && removeReplicated)
            remove = true;
        else if (childNode->GetID() >= FIRST_LOCAL_ID && removeLocal)
            remove = true;

        if (remove)
        {
            RemoveChild(children_.begin() + i);
            ++numRemoved;
        }
    }

    // Mark node dirty in all replication states
    if (numRemoved != 0u)
        MarkReplicationDirty();
}

Component* Node::CreateComponent(StringHash type, CreateMode mode, unsigned id)
{
    // Do not attempt to create replicated components to local nodes, as that may lead to component ID overwrite
    // as replicated components are synced over
    if (id_ >= FIRST_LOCAL_ID && mode == REPLICATED)
        mode = LOCAL;
    // Check that creation succeeds and that the object in fact is a component
    SharedPtr<Component> newComponent = DynamicCast<Component>(context_->CreateObject(type));
    if (newComponent == nullptr)
    {
        URHO3D_LOGERROR("Could not create unknown component type " + type.ToString());
        return nullptr;
    }

    AddComponent(newComponent, id, mode);
    return newComponent;
}

Component* Node::GetOrCreateComponent(StringHash type, CreateMode mode, unsigned id)
{
    Component* oldComponent = GetComponent(type);
    if (oldComponent != nullptr)
        return oldComponent;

    return CreateComponent(type, mode, id);
}

Component* Node::CloneComponent(Component* component, unsigned id)
{
    if (component == nullptr)
    {
        URHO3D_LOGERROR("Null source component given for CloneComponent");
        return nullptr;
    }

    return CloneComponent(component, component->GetID() < FIRST_LOCAL_ID ? REPLICATED : LOCAL, id);
}

Component* Node::CloneComponent(Component* component, CreateMode mode, unsigned id)
{
    if (component == nullptr)
    {
        URHO3D_LOGERROR("Null source component given for CloneComponent");
        return nullptr;
    }

    Component* cloneComponent = SafeCreateComponent(&component->GetTypeName(), component->GetType(), mode, 0);
    if (cloneComponent == nullptr)
    {
        URHO3D_LOGERROR("Could not clone component " + component->GetTypeName());
        return nullptr;
    }

    const std::vector<AttributeInfo>* compAttributes = component->GetAttributes();
    const std::vector<AttributeInfo>* cloneAttributes = cloneComponent->GetAttributes();

    if (compAttributes != nullptr)
    {
        for (unsigned i = 0; i < compAttributes->size() && i < cloneAttributes->size(); ++i)
        {
            const AttributeInfo& attr = compAttributes->at(i);
            const AttributeInfo& cloneAttr = cloneAttributes->at(i);
            if ((attr.mode_ & AM_FILE) != 0u)
            {
                Variant value;
                component->OnGetAttribute(attr, value);
                // Note: when eg. a ScriptInstance component is cloned, its script object attributes are unique and therefore we
                // can not simply refer to the source component's AttributeInfo
                cloneComponent->OnSetAttribute(cloneAttr, value);
            }
        }
        cloneComponent->ApplyAttributes();
    }
    scene_->componentCloned(scene_,component,cloneComponent);
    return cloneComponent;
}

void Node::RemoveComponent(Component* component)
{
    for (std::vector<SharedPtr<Component> >::iterator i = components_.begin(); i != components_.end(); ++i)
    {
        if (*i == component)
        {
            RemoveComponent(i);

            // Mark node dirty in all replication states
            MarkReplicationDirty();
            return;
        }
    }
}

void Node::RemoveComponent(StringHash type)
{
    for (std::vector<SharedPtr<Component> >::iterator i = components_.begin(); i != components_.end(); ++i)
    {
        if ((*i)->GetType() == type)
        {
            RemoveComponent(i);

            // Mark node dirty in all replication states
            MarkReplicationDirty();
            return;
        }
    }
}

void Node::RemoveComponents(bool removeReplicated, bool removeLocal)
{
    unsigned numRemoved = 0;

    for (unsigned i = components_.size() - 1; i < components_.size(); --i)
    {
        bool remove = false;
        Component* component = components_[i];

        if (component->GetID() < FIRST_LOCAL_ID && removeReplicated)
            remove = true;
        else if (component->GetID() >= FIRST_LOCAL_ID && removeLocal)
            remove = true;

        if (remove)
        {
            RemoveComponent(components_.begin() + i);
            ++numRemoved;
        }
    }

    // Mark node dirty in all replication states
    if (numRemoved != 0u)
        MarkReplicationDirty();
}

void Node::RemoveComponents(StringHash type)
{
    unsigned numRemoved = 0;

    for (unsigned i = components_.size() - 1; i < components_.size(); --i)
    {
        if (components_[i]->GetType() == type)
        {
            RemoveComponent(components_.begin() + i);
            ++numRemoved;
        }
    }

    // Mark node dirty in all replication states
    if (numRemoved != 0u)
        MarkReplicationDirty();
}

void Node::RemoveAllComponents()
{
    RemoveComponents(true, true);
}
void Node::ReorderComponent(Component* component, unsigned index)
{
    if (nullptr==component || component->GetNode() != this)
        return;

    for (std::vector<SharedPtr<Component> >::iterator i = components_.begin(); i != components_.end(); ++i)
    {
        if (*i == component)
        {
            // Need shared ptr to insert. Also, prevent destruction when removing first
            SharedPtr<Component> componentShared(component);
            components_.erase(i);
            components_.insert(components_.begin()+index, componentShared);
            return;
        }
    }
}
Node* Node::Clone(CreateMode mode)
{
    // The scene itself can not be cloned
    if (this == scene_ || (parent_ == nullptr))
    {
        URHO3D_LOGERROR("Can not clone node without a parent");
        return nullptr;
    }

    URHO3D_PROFILE(CloneNode);

    SceneResolver resolver;
    Node* clone = CloneRecursive(parent_, resolver, mode);
    resolver.Resolve();
    clone->ApplyAttributes();
    return clone;
}

void Node::Remove()
{
    if (parent_ != nullptr)
        parent_->RemoveChild(this);
}

void Node::SetParent(Node* parent)
{
    if (parent != nullptr)
    {
        Matrix3x4 oldWorldTransform = GetWorldTransform();

        parent->AddChild(this);

        if (parent != scene_)
        {
            Matrix3x4 newTransform = parent->GetWorldTransform().Inverse() * oldWorldTransform;
            SetTransform(newTransform.Translation(), newTransform.Rotation(), newTransform.Scale());
        }
        else
        {
            // The root node is assumed to have identity transform, so can disregard it
            SetTransform(oldWorldTransform.Translation(), oldWorldTransform.Rotation(), oldWorldTransform.Scale());
        }
    }
}

void Node::SetVar(StringHash key, const Variant& value)
{
    vars_[key] = value;
    MarkNetworkUpdate();
}

void Node::AddListener(Component* component)
{
    if (component == nullptr)
        return;

    impl_->addListener(component);
    if (dirty_)
        component->OnMarkedDirty(this);
}

void Node::RemoveListener(Component* component)
{
    impl_->removeListener(component);
}

const QString &Node::GetName() const
{
    return impl_->name_;
}
Vector3 Node::GetSignedWorldScale() const
{
    if (dirty_)
        UpdateWorldTransform();
    return worldTransform_.SignedScale(worldRotation_.RotationMatrix());
}
/// Convert a local space position to world space.
Vector3 Node::LocalToWorld(const Vector3& position) const
{
    return GetWorldTransform() * position;
}
/// Convert a local space position or rotation to world space.
Vector3 Node::LocalToWorld(const Vector4& vector) const
{
    return GetWorldTransform() * vector;
}
/// Convert a local space position or rotation to world space (for Urho2D).
Vector2 Node::LocalToWorld2D(const Vector2& vector) const
{
    Vector3 result = LocalToWorld(Vector3(vector));
    return Vector2(result.x_, result.y_);
}
/// Convert a world space position to local space.
Vector3 Node::WorldToLocal(const Vector3& position) const
{
    return GetWorldTransform().Inverse() * position;
}
/// Convert a world space position or rotation to local space.
Vector3 Node::WorldToLocal(const Vector4& vector) const
{
    return GetWorldTransform().Inverse() * vector;
}
/// Convert a world space position or rotation to local space (for Urho2D).
Vector2 Node::WorldToLocal2D(const Vector2& vector) const
{
    Vector3 result = WorldToLocal(Vector3(vector));
    return Vector2(result.x_, result.y_);
}
/// Return number of child scene nodes.
unsigned Node::GetNumChildren(bool recursive) const
{
    if (!recursive)
        return children_.size();


    unsigned allChildren = children_.size();
    for (const auto & elem : children_)
        allChildren += elem->GetNumChildren(true);

    return allChildren;

}
//! Return child scene nodes, optionally recursive.
void Node::GetChildren(std::vector<Node*>& dest, bool recursive) const
{
    dest.clear();

    if (!recursive)
    {
        for (const auto & elem : children_)
            dest.push_back(elem);
    }
    else
        GetChildrenRecursive(dest);
}
//! Return child scene nodes, optionally recursive.
std::vector<Node*> Node::GetChildren(bool recursive) const
{
    std::vector<Node*> dest;
    GetChildren(dest, recursive);
    return dest;
}
//! Return child scene nodes with a specific component type.
void Node::GetChildrenWithComponent(std::vector<Node*>& dest, StringHash type, bool recursive) const
{
    dest.clear();

    if (!recursive)
    {
        for (const auto & elem : children_)
        {
            if (elem->HasComponent(type))
                dest.push_back(elem);
        }
    }
    else
        GetChildrenWithComponentRecursive(dest, type);
}
//! Return child scene nodes with a specific component.
std::vector<Node*> Node::GetChildrenWithComponent(StringHash type, bool recursive) const
{
    std::vector<Node*> dest;
    GetChildrenWithComponent(dest, type, recursive);
    return dest;
}
//! Return child scene nodes with a specific tag.
void Node::GetChildrenWithTag(std::vector<Node*>& dest, const QString & tag, bool recursive /*= true*/) const
{
    dest.clear();

    if (!recursive)
    {
        for (std::vector<SharedPtr<Node> >::const_iterator i = children_.begin(); i != children_.end(); ++i)
        {
            if ((*i)->HasTag(tag))
                dest.push_back(*i);
        }
    }
    else
        GetChildrenWithTagRecursive(dest, tag);
}
//! Return child scene nodes with a specific tag.
std::vector<Node*> Node::GetChildrenWithTag(const QString& tag, bool recursive) const
{
    std::vector<Node*> dest;
    GetChildrenWithTag(dest, tag, recursive);
    return dest;
}
//! Return child scene node by index.
Node* Node::GetChild(unsigned index) const
{
    return index < children_.size() ? children_[index].Get() : nullptr;
}
//! Return child scene node by name.
Node* Node::GetChild(const QStringRef& name, bool recursive) const
{
    return GetChild(StringHash(name), recursive);
}
//! Return child scene node by name.
Node* Node::GetChild(const char* name, bool recursive) const
{
    return GetChild(StringHash(name), recursive);
}
//! Return child scene node by name hash.
Node* Node::GetChild(StringHash nameHash, bool recursive) const
{
    for (const auto & elem : children_)
    {
        if (elem->GetNameHash() == nameHash)
            return elem;

        if (recursive)
        {
            Node* node = elem->GetChild(nameHash, true);
            if (node != nullptr)
                return node;
        }
    }

    return nullptr;
}
/// Return number of non-local components.
unsigned Node::GetNumNetworkComponents() const
{
    unsigned num = 0;
    for (const auto & elem : components_)
    {
        if (elem->GetID() < FIRST_LOCAL_ID)
            ++num;
    }

    return num;
}
/// Return all components of type. Optionally recursive.
void Node::GetComponents(std::vector<Component*>& dest, StringHash type, bool recursive) const
{
    dest.clear();

    if (!recursive)
    {
        for (const auto & elem : components_)
        {
            if (elem->GetType() == type)
                dest.push_back(elem);
        }
    }
    else
        GetComponentsRecursive(dest, type);
}
/// Return whether has a specific component.
bool Node::HasComponent(StringHash type) const
{
    for (const auto & elem : components_)
    {
        if (elem->GetType() == type)
            return true;
    }
    return false;
}

bool Node::IsReplicated() const
{
    return Scene::IsReplicatedID(id_);
}
bool Node::HasTag(const QString & tag) const
{
    return impl_->tags_.contains(tag);
}

bool Node::IsChildOf(Node* node) const
{
    Node* parent = parent_;
    while (parent)
    {
        if (parent == node)
            return true;
        parent = parent->parent_;
    }
    return false;
}

Connection *Node::GetOwner() const
{
    return impl_->owner_;
}
/// Return a user variable.
const Variant& Node::GetVar(StringHash key) const
{
    auto i = vars_.find(key);
    return i != vars_.end() ? MAP_VALUE(i) : Variant::EMPTY;
}
/// Return component by type. If there are several, returns the first.
Component* Node::GetComponent(StringHash type, bool recursive) const
{
    for (const auto & elem : components_)
    {
        if ((elem)->GetType() == type)
            return elem;
    }
    if (recursive)
    {
        for (const auto & elem : children_)
        {
            Component* component = elem->GetComponent(type, true);
            if (component != nullptr)
                return component;
        }
    }

    return nullptr;
}
/// Return component in parent node. If there are several, returns the first. May optional traverse up to the root node.
Component* Node::GetParentComponent(StringHash type, bool fullTraversal) const
{
    Node* current = GetParent();
    while (current != nullptr)
    {
        Component* soughtComponent = current->GetComponent(type);
        if (soughtComponent != nullptr)
            return soughtComponent;

        if (fullTraversal)
            current = current->GetParent();
        else
            break;
    }
    return nullptr;
}

void Node::SetID(unsigned id)
{
    id_ = id;
}

void Node::SetScene(Scene* scene)
{
    scene_ = scene;
}

void Node::ResetScene()
{
    SetID(0);
    SetScene(nullptr);
    SetOwner(nullptr);
}

void Node::SetNetPositionAttr(const Vector3& value)
{
    SmoothedTransform* transform = GetComponent<SmoothedTransform>();
    if (transform != nullptr)
        transform->SetTargetPosition(value);
    else
        SetPosition(value);
}

void Node::SetNetRotationAttr(const std::vector<unsigned char>& value)
{
    MemoryBuffer buf(value);
    SmoothedTransform* transform = GetComponent<SmoothedTransform>();
    if (transform != nullptr)
        transform->SetTargetRotation(buf.ReadPackedQuaternion());
    else
        SetRotation(buf.ReadPackedQuaternion());
}

void Node::SetNetParentAttr(const std::vector<unsigned char>& value)
{
    Scene* scene = GetScene();
    if (scene == nullptr)
        return;

    MemoryBuffer buf(value);
    // If nothing in the buffer, parent is the root node
    if (buf.IsEof())
    {
        scene->AddChild(this);
        return;
    }

    unsigned baseNodeID = buf.ReadNetID();
    Node* baseNode = scene->GetNode(baseNodeID);
    if (baseNode == nullptr)
    {
        URHO3D_LOGWARNING("Failed to find parent node " + QString::number(baseNodeID));
        return;
    }

    // If buffer contains just an ID, the parent is replicated and we are done
    if (buf.IsEof())
        baseNode->AddChild(this);
    else
    {
        // Else the parent is local and we must find it recursively by name hash
        StringHash nameHash = buf.ReadStringHash();
        Node* parentNode = baseNode->GetChild(nameHash, true);
        if (parentNode == nullptr)
            URHO3D_LOGWARNING("Failed to find parent node with name hash " + nameHash.ToString());
        else
            parentNode->AddChild(this);
    }
}

const Vector3& Node::GetNetPositionAttr() const
{
    return position_;
}

const std::vector<unsigned char>& Node::GetNetRotationAttr() const
{
    impl_->attrBuffer_.clear();
    impl_->attrBuffer_.WritePackedQuaternion(rotation_);
    return impl_->attrBuffer_.GetBuffer();
}

const std::vector<unsigned char>& Node::GetNetParentAttr() const
{
    impl_->attrBuffer_.clear();
    Scene* scene = GetScene();
    if ((scene != nullptr) && (parent_ != nullptr) && parent_ != scene)
    {
        // If parent is replicated, can write the ID directly
        unsigned parentID = parent_->GetID();
        if (parentID < FIRST_LOCAL_ID)
            impl_->attrBuffer_.WriteNetID(parentID);
        else
        {
            // Parent is local: traverse hierarchy to find a non-local base node
            // This iteration always stops due to the scene (root) being non-local
            Node* current = parent_;
            while (current->GetID() >= FIRST_LOCAL_ID)
                current = current->GetParent();

            // Then write the base node ID and the parent's name hash
            impl_->attrBuffer_.WriteNetID(current->GetID());
            impl_->attrBuffer_.WriteStringHash(parent_->GetNameHash());
        }
    }

    return impl_->attrBuffer_.GetBuffer();
}

bool Node::Load(Deserializer& source, SceneResolver& resolver, bool readChildren, bool rewriteIDs, CreateMode mode)
{
    // Remove all children and components first in case this is not a fresh load
    RemoveAllChildren();
    RemoveAllComponents();

    // ID has been read at the parent level
    if (!Animatable::Load(source))
        return false;

    unsigned numComponents = source.ReadVLE();
    for (unsigned i = 0; i < numComponents; ++i)
    {
        VectorBuffer compBuffer(source, source.ReadVLE());
        StringHash compType = compBuffer.ReadStringHash();
        unsigned compID = compBuffer.ReadUInt();

        Component* newComponent = SafeCreateComponent(nullptr, compType,
                                                      (mode == REPLICATED && compID < FIRST_LOCAL_ID) ? REPLICATED : LOCAL, rewriteIDs ? 0 : compID);
        if (newComponent != nullptr)
        {
            resolver.AddComponent(compID, newComponent);
            // Do not abort if component fails to load, as the component buffer is nested and we can skip to the next
            newComponent->Load(compBuffer);
        }
    }

    if (!readChildren)
        return true;

    unsigned numChildren = source.ReadVLE();
    for (unsigned i = 0; i < numChildren; ++i)
    {
        unsigned nodeID = source.ReadUInt();
        Node* newNode = CreateChild(rewriteIDs ? 0 : nodeID, (mode == REPLICATED && nodeID < FIRST_LOCAL_ID) ? REPLICATED :
                                                                                                               LOCAL);
        resolver.AddNode(nodeID, newNode);
        if (!newNode->Load(source, resolver, readChildren, rewriteIDs, mode))
            return false;
    }

    return true;
}

bool Node::LoadXML(const XMLElement& source, SceneResolver& resolver, bool readChildren, bool rewriteIDs, CreateMode mode)
{
    // Remove all children and components first in case this is not a fresh load
    RemoveAllChildren();
    RemoveAllComponents();

    if (!Animatable::LoadXML(source))
        return false;

    XMLElement compElem = source.GetChild("component");
    while (compElem)
    {
        QString typeName = compElem.GetAttribute("type");
        unsigned compID = compElem.GetUInt("id");
        Component* newComponent = SafeCreateComponent(&typeName, StringHash(typeName),
                                                      (mode == REPLICATED && compID < FIRST_LOCAL_ID) ? REPLICATED : LOCAL, rewriteIDs ? 0 : compID);
        if (newComponent != nullptr)
        {
            resolver.AddComponent(compID, newComponent);
            if (!newComponent->LoadXML(compElem))
                return false;
        }

        compElem = compElem.GetNext("component");
    }

    if (!readChildren)
        return true;

    XMLElement childElem = source.GetChild("node");
    while (childElem)
    {
        unsigned nodeID = childElem.GetUInt("id");
        Node* newNode = CreateChild(rewriteIDs ? 0 : nodeID, (mode == REPLICATED && nodeID < FIRST_LOCAL_ID) ? REPLICATED :
                                                                                                               LOCAL);
        resolver.AddNode(nodeID, newNode);
        if (!newNode->LoadXML(childElem, resolver, readChildren, rewriteIDs, mode))
            return false;

        childElem = childElem.GetNext("node");
    }

    return true;
}

bool Node::LoadJSON(const JSONValue& source, SceneResolver& resolver, bool readChildren, bool rewriteIDs, CreateMode mode)
{
    // Remove all children and components first in case this is not a fresh load
    RemoveAllChildren();
    RemoveAllComponents();

    if (!Animatable::LoadJSON(source))
        return false;

    const JSONArray& componentsArray = source.Get("components").GetArray();

    for (unsigned i = 0; i < componentsArray.size(); i++)
    {
        const JSONValue& compVal = componentsArray.at(i);
        QString typeName = compVal.Get("type").GetString();
        unsigned compID = compVal.Get("id").GetUInt();
        Component* newComponent = SafeCreateComponent(&typeName, StringHash(typeName),
                                                      (mode == REPLICATED && compID < FIRST_LOCAL_ID) ? REPLICATED : LOCAL, rewriteIDs ? 0 : compID);
        if (newComponent != nullptr)
        {
            resolver.AddComponent(compID, newComponent);
            if (!newComponent->LoadJSON(compVal))
                return false;
        }
    }

    if (!readChildren)
        return true;

    const JSONArray& childrenArray = source.Get("children").GetArray();
    for (unsigned i = 0; i < childrenArray.size(); i++)
    {
        const JSONValue& childVal = childrenArray.at(i);

        unsigned nodeID = childVal.Get("id").GetUInt();
        Node* newNode = CreateChild(rewriteIDs ? 0 : nodeID, (mode == REPLICATED && nodeID < FIRST_LOCAL_ID) ? REPLICATED :
                                                                                                               LOCAL);
        resolver.AddNode(nodeID, newNode);
        if (!newNode->LoadJSON(childVal, resolver, readChildren, rewriteIDs, mode))
            return false;
    }

    return true;
}

const std::vector<Node *> &Node::GetDependencyNodes() const
{
    return impl_->dependencyNodes_;
}

void Node::PrepareNetworkUpdate()
{
    // Update dependency nodes list first
    impl_->dependencyNodes_.clear();

    // Add the parent node, but if it is local, traverse to the first non-local node
    if ((parent_ != nullptr) && parent_ != scene_)
    {
        Node* current = parent_;
        while (current->id_ >= FIRST_LOCAL_ID)
            current = current->parent_;
        if ((current != nullptr) && current != scene_)
            impl_->dependencyNodes_.push_back(current);
    }

    // Let the components add their dependencies
    for (std::vector<SharedPtr<Component> >::const_iterator i = components_.begin(); i != components_.end(); ++i)
    {
        Component* component = *i;
        if (component->GetID() < FIRST_LOCAL_ID)
            component->GetDependencyNodes(impl_->dependencyNodes_);
    }

    // Then check for node attribute changes
    if (networkState_ == nullptr)
        AllocateNetworkState();

    const std::vector<AttributeInfo>* attributes = networkState_->attributes_;
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

            // Mark the attribute dirty in all replication states that are tracking this node
            for (ReplicationState* elem : networkState_->replicationStates_)
            {
                NodeReplicationState* nodeState = static_cast<NodeReplicationState*>(elem);
                nodeState->dirtyAttributes_.Set(i);

                // Add node to the dirty set if not added yet
                if (!nodeState->markedDirty_)
                {
                    nodeState->markedDirty_ = true;
                    nodeState->sceneState_->dirtyNodes_.insert(id_);
                }
            }
        }
    }

    // Finally check for user var changes
    for (VariantMap::const_iterator i=vars_.begin(),fin=vars_.end(); i!=fin; ++i)
    {
        auto j = networkState_->previousVars_.find(MAP_KEY(i));
        if (j != networkState_->previousVars_.end() && *j == *i)
            continue;
        networkState_->previousVars_[MAP_KEY(i)] = MAP_VALUE(i);

        // Mark the var dirty in all replication states that are tracking this node
        for (auto & elem : networkState_->replicationStates_)
        {
            NodeReplicationState* nodeState = static_cast<NodeReplicationState*>(elem);
            nodeState->dirtyVars_.insert(MAP_KEY(i));

            if (!nodeState->markedDirty_)
            {
                nodeState->markedDirty_ = true;
                nodeState->sceneState_->dirtyNodes_.insert(id_);
            }
        }
    }

    networkUpdate_ = false;
}

void Node::CleanupConnection(Connection* connection)
{
    if (impl_->owner_ == connection)
        impl_->owner_ = nullptr;

    if (networkState_ == nullptr)
        return;
    for (auto i = networkState_->replicationStates_.begin(),fin=networkState_->replicationStates_.end(); i!=fin ; )
    {
        if ((*i)->connection_ == connection)
            i = networkState_->replicationStates_.erase(i);
        else
            ++i;
    }
}

void Node::MarkReplicationDirty()
{
    if (networkState_ == nullptr)
        return;
    for (auto & elem : networkState_->replicationStates_)
    {
        NodeReplicationState* nodeState = static_cast<NodeReplicationState*>(elem);
        if (!nodeState->markedDirty_)
        {
            nodeState->markedDirty_ = true;
            nodeState->sceneState_->dirtyNodes_.insert(id_);
        }
    }
}

Node* Node::CreateChild(unsigned id, CreateMode mode, bool temporary)
{
    SharedPtr<Node> newNode(new Node(context_));
    newNode->SetTemporary(temporary);

    // If zero ID specified, or the ID is already taken, let the scene assign
    if (scene_ != nullptr)
    {
        if ((id == 0u) || (scene_->GetNode(id) != nullptr))
            id = scene_->GetFreeNodeID(mode);
        newNode->SetID(id);
    }
    else
        newNode->SetID(id);

    AddChild(newNode);
    return newNode;
}

void Node::AddComponent(Component* component, unsigned id, CreateMode mode)
{
    if (component == nullptr)
        return;

    components_.push_back(SharedPtr<Component>(component));
    if (component->GetNode() != nullptr)
        URHO3D_LOGWARNING("Component " + component->GetTypeName() + " already belongs to a node!");

    component->SetNode(this);

    // If zero ID specified, or the ID is already taken, let the scene assign
    if (scene_ != nullptr)
    {
        if ((id == 0u) || (scene_->GetComponent(id) != nullptr))
            id = scene_->GetFreeComponentID(mode);
        component->SetID(id);
        scene_->ComponentAdded(component);
    }
    else
        component->SetID(id);

    component->OnMarkedDirty(this);

    // Check attributes of the new component on next network update, and mark node dirty in all replication states
    component->MarkNetworkUpdate();
    MarkNetworkUpdate();
    MarkReplicationDirty();

    // Send change event
    if (scene_ != nullptr)
    {
        scene_->componentAdded(scene_,this,component);
    }
    g_sceneSignals.componentAdded(scene_,this,component);
}

unsigned Node::GetNumPersistentChildren() const
{
    unsigned ret = 0;

    for (const auto & elem : children_)
    {
        if (!elem->IsTemporary())
            ++ret;
    }

    return ret;
}

unsigned Node::GetNumPersistentComponents() const
{
    unsigned ret = 0;

    for (const auto & elem : components_)
    {
        if (!elem->IsTemporary())
            ++ret;
    }

    return ret;
}

void Node::SetTransformSilent(const Vector3& position, const Quaternion& rotation, const Vector3& scale)
{
    position_ = position;
    rotation_ = rotation;
    scale_ = scale;
}

void Node::OnAttributeAnimationAdded()
{
    if (attributeAnimationInfos_.size() == 1)
        GetScene()->attributeAnimationUpdate.Connect(this,&Node::HandleAttributeAnimationUpdate);
}

void Node::OnAttributeAnimationRemoved()
{
    if (attributeAnimationInfos_.empty())
        GetScene()->attributeAnimationUpdate.Disconnect(this,&Node::HandleAttributeAnimationUpdate);
}

Animatable* Node::FindAttributeAnimationTarget(const QString& name, QString& outName)
{
    QStringList names = name.split('/');
    // Only attribute name
    if (names.size() == 1)
    {
        outName = name;
        return this;
    }


    // Name must in following format: "#0/#1/@component#0/attribute"
    Node* node = this;
    unsigned i = 0;
    for (; i < names.size() - 1; ++i)
    {
        if (!names[i].startsWith('#'))
            break;
        QStringRef nameref(names[i].midRef(1, names[i].length() - 1));
        if(nameref[0].isDigit())
        {
            unsigned index = nameref.toUInt();
            node = node->GetChild(index);
        }
        else
        {
            node = node->GetChild(nameref);
        }
        if (node == nullptr)
        {
            URHO3D_LOGERROR("Could not find node by name " + name);
            return nullptr;
        }
    }

    if (i == names.size() - 1)
    {
        outName = names.back();
        return node;
    }

    if (i != names.size() - 2 || !names[i].startsWith('@') )
    {
        URHO3D_LOGERROR("Invalid name " + name);
        return nullptr;
    }

    QString componentName = names[i].mid(1, names[i].length() - 1);
    QStringList componentNames = componentName.split('#');
    if (componentNames.size() == 1)
    {
        Component* component = node->GetComponent(StringHash(componentNames.front()));
        if (component == nullptr)
        {
            URHO3D_LOGERROR("Could not find component by name " + name);
            return nullptr;
        }

        outName = names.back();
        return component;
    }


    unsigned index = componentNames[1].toUInt();
    std::vector<Component*> components;
    node->GetComponents(components, StringHash(componentNames.front()));
    if (index >= components.size())
    {
        URHO3D_LOGERROR("Could not find component by name " + name);
        return  nullptr;
    }

    outName = names.back();
    return components[index];


}

void Node::SetEnabled(bool enable, bool recursive, bool storeSelf)
{
    // The enabled state of the whole scene can not be changed. SetUpdateEnabled() is used instead to start/stop updates.
    if (GetType() == Scene::GetTypeStatic())
    {
        URHO3D_LOGERROR("Can not change enabled state of the Scene");
        return;
    }

    if (storeSelf)
        enabledPrev_ = enable;

    if (enable != enabled_)
    {
        enabled_ = enable;
        MarkNetworkUpdate();

        impl_->notifyListenersEnabled(this);

        // Send change event
        if (scene_ != nullptr)
        {
            scene_->nodeEnabledChanged(scene_,this);
        }

        for (auto & elem : components_)
        {
            elem->OnSetEnabled();

            // Send change event for the component
            if (scene_ != nullptr)
            {
                scene_->componentEnabledChanged(scene_,this,elem);
            }
        }
    }

    if (recursive)
    {
        for (auto & elem : children_)
            elem->SetEnabled(enable, recursive, storeSelf);
    }
}

Component* Node::SafeCreateComponent(const QStringRef& typeName, StringHash type, CreateMode mode, unsigned id)
{
    // Do not attempt to create replicated components to local nodes, as that may lead to component ID overwrite
    // as replicated components are synced over
    if (id_ >= FIRST_LOCAL_ID && mode == REPLICATED)
        mode = LOCAL;
    // First check if factory for type exists
    if (!context_->GetTypeName(type).isEmpty())
        return CreateComponent(type, mode, id);


    URHO3D_LOGWARNING("Component type " + type.ToString() + " not known, creating UnknownComponent as placeholder");
    // Else create as UnknownComponent
    SharedPtr<UnknownComponent> newComponent(new UnknownComponent(context_));
    if (typeName.isEmpty() || typeName.startsWith("Unknown", Qt::CaseInsensitive))
        newComponent->SetType(type);
    else
        newComponent->SetTypeName(typeName);

    AddComponent(newComponent, id, mode);
    return newComponent;

}

void Node::UpdateWorldTransform() const
{
    Matrix3x4 transform = GetTransform();

    // Assume the root node (scene) has identity transform
    if (parent_ == scene_ || (parent_ == nullptr))
    {
        worldTransform_ = transform;
        worldRotation_ = rotation_;
    }
    else
    {
        worldTransform_ = parent_->GetWorldTransform() * transform;
        worldRotation_ = parent_->GetWorldRotation() * rotation_;
    }

    dirty_ = false;
}

void Node::RemoveChild(std::vector<SharedPtr<Node> >::iterator i)
{
    // Keep a shared pointer to the child about to be removed, to make sure the erase from container completes first. Otherwise
    // it would be possible that other child nodes get removed as part of the node's components' cleanup, causing a re-entrant
    // erase and a crash
    SharedPtr<Node> child(*i);
    // Send change event. Do not send when this node is already being destroyed
    if (Refs() > 0 && (scene_ != nullptr))
    {
        scene_->nodeRemoved(scene_,this,child);
    }

    child->parent_ = nullptr;
    child->MarkDirty();
    child->MarkNetworkUpdate();
    if (scene_ != nullptr)
        scene_->NodeRemoved(child);

    children_.erase(i);
}

void Node::GetChildrenRecursive(std::vector<Node*>& dest) const
{
    for (const auto & elem : children_)
    {
        Node* node = elem;
        dest.push_back(node);
        if (!node->children_.empty())
            node->GetChildrenRecursive(dest);
    }
}

void Node::GetChildrenWithComponentRecursive(std::vector<Node*>& dest, StringHash type) const
{
    for (const auto & elem : children_)
    {
        Node* node = elem;
        if (node->HasComponent(type))
            dest.push_back(node);
        if (!node->children_.empty())
            node->GetChildrenWithComponentRecursive(dest, type);
    }
}

void Node::GetComponentsRecursive(std::vector<Component*>& dest, StringHash type) const
{
    for (const auto & elem : components_)
    {
        if (elem->GetType() == type)
            dest.push_back(elem);
    }
    for (const auto & elem : children_)
        elem->GetComponentsRecursive(dest, type);
}

void Node::GetChildrenWithTagRecursive(std::vector<Urho3D::Node *> &dest, const QString & tag) const
{
    for (std::vector<SharedPtr<Node> >::const_iterator i = children_.begin(); i != children_.end(); ++i)
    {
        Node* node = *i;
        if (node->HasTag(tag))
            dest.push_back(node);
        if (!node->children_.empty())
            node->GetChildrenWithTagRecursive(dest, tag);
    }
}
Node* Node::CloneRecursive(Node* parent, SceneResolver& resolver, CreateMode mode)
{
    // Create clone node
    Node* cloneNode = parent->CreateChild(0, (mode == REPLICATED && id_ < FIRST_LOCAL_ID) ? REPLICATED : LOCAL);
    resolver.AddNode(id_, cloneNode);

    // Copy attributes
    const std::vector<AttributeInfo>* attributes = GetAttributes();
    for (unsigned j = 0; j < attributes->size(); ++j)
    {
        const AttributeInfo& attr = attributes->at(j);
        // Do not copy network-only attributes, as they may have unintended side effects
        if ((attr.mode_ & AM_FILE) != 0u)
        {
            Variant value;
            OnGetAttribute(attr, value);
            cloneNode->OnSetAttribute(attr, value);
        }
    }

    // Clone components
    for (std::vector<SharedPtr<Component> >::const_iterator i = components_.begin(); i != components_.end(); ++i)
    {
        Component* component = *i;
        if (component->IsTemporary())
            continue;

        Component* cloneComponent = cloneNode->CloneComponent(component,
                                                              (mode == REPLICATED && component->GetID() < FIRST_LOCAL_ID) ? REPLICATED : LOCAL, 0);
        if (cloneComponent != nullptr)
            resolver.AddComponent(component->GetID(), cloneComponent);
    }

    // Clone child nodes recursively
    for (std::vector<SharedPtr<Node> >::const_iterator i = children_.begin(); i != children_.end(); ++i)
    {
        Node* node = *i;
        if (node->IsTemporary())
            continue;

        node->CloneRecursive(cloneNode, resolver, mode);
    }
    scene_->nodeCloned(scene_,this,cloneNode);
    return cloneNode;
}

void Node::RemoveComponent(std::vector<SharedPtr<Component> >::iterator i)
{

    // Send node change event. Do not send when already being destroyed
    if (Refs() > 0 && (scene_ != nullptr))
    {
        scene_->componentRemoved(scene_,this,(*i).Get());
    }

    RemoveListener(*i);
    if (scene_ != nullptr)
        scene_->ComponentRemoved(*i);
    (*i)->SetNode(nullptr);
    components_.erase(i);

}

void Node::HandleAttributeAnimationUpdate(Scene*s,float ts)
{
    UpdateAttributeAnimations(ts);
}

StringHash Urho3D::Node::GetNameHash() const
{
    return impl_->nameHash_;
}

const QStringList &Node::GetTags() const
{
    return impl_->tags_;
}
}
