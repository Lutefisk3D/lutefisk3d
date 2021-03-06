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

#include "Lutefisk3D/Math/Matrix3x4.h"
#include "Lutefisk3D/Scene/Animatable.h"
#include <QtCore/QStringList>
namespace Urho3D
{

class Component;
class Connection;
class Node;
class Scene;
class SceneResolver;

struct NodeReplicationState;
struct NodePrivate;
/// Component and child node creation mode for networking.
enum CreateMode
{
    REPLICATED = 0,
    LOCAL = 1
};

/// Transform space for translations and rotations.
enum TransformSpace
{
    TS_LOCAL = 0,
    TS_PARENT,
    TS_WORLD
};


/// %Scene node that may contain components and child nodes.
class LUTEFISK3D_EXPORT Node : public Animatable
{
    URHO3D_OBJECT(Node,Animatable)

    friend class Connection;

public:
    Node(Context* context);
    /// Destruct. Any child nodes are detached.
    virtual ~Node();
    /// Register object factory.
    static void RegisterObject(Context* context);

    /// Load from binary data. Return true if successful.
    bool Load(Deserializer& source) override;
    /// Load from XML data. Return true if successful.
    bool LoadXML(const XMLElement& source) override;
    /// Load from JSON data. Return true if successful.
    bool LoadJSON(const JSONValue& source) override;
    /// Save as binary data. Return true if successful.
    bool Save(Serializer& dest) const override;
    /// Save as XML data. Return true if successful.
    bool SaveXML(XMLElement& dest) const override;
    /// Save as JSON data. Return true if successful.
    bool SaveJSON(JSONValue& dest) const override;
    /// Apply attribute changes that can not be applied immediately recursively to child nodes and components.
    void ApplyAttributes() override;
    /// Return whether should save default-valued attributes into XML. Always save node transforms for readability, even if identity.
    bool SaveDefaultAttributes() const override { return true; }
    /// Mark for attribute check on the next network update.
    void MarkNetworkUpdate() override;
    /// Add a replication state that is tracking this node.
    virtual void AddReplicationState(NodeReplicationState* state);

    /// Save to an XML file. Return true if successful.
    bool SaveXML(Serializer& dest, const QString& indentation = "\t") const;
    /// Save to a JSON file. Return true if successful.
    bool SaveJSON(Serializer& dest, const QString& indentation = "\t") const;
    /// Set name of the scene node. Names are not required to be unique.
    void SetName(const QString& name);
    /// Set tags. Old tags are overwritten.
    void SetTags(const QStringList& tags);
    /// Add a tag.
    void AddTag(const QString &tag);
    /// Add tags with the specified separator, by default ;
    void AddTags(const QString &tags, char separator = ';');
    /// Add tags.
    void AddTags(const QStringList& tags);
    /// Remove tag. Return true if existed.
    bool RemoveTag(const QString &tag);
    /// Remove all tags.
    void RemoveAllTags();
    /// Set position in parent space. If the scene node is on the root level (is child of the scene itself), this is same as world space.
    void SetPosition(const Vector3& position);
    /// Set position in parent space (for Urho2D).
    void SetPosition2D(const Vector2& position) { SetPosition(Vector3(position)); }
    /// Set position in parent space (for Urho2D).
    void SetPosition2D(float x, float y) { SetPosition(Vector3(x, y, 0.0f)); }
    /// Set rotation in parent space.
    void SetRotation(const Quaternion& rotation);
    /// Set rotation in parent space (for Urho2D).
    void SetRotation2D(float rotation) { SetRotation(Quaternion(rotation)); }
    /// Set forward direction in parent space. Positive Z axis equals identity rotation.
    void SetDirection(const Vector3& direction);
    /// Set uniform scale in parent space.
    void SetScale(float scale);
    /// Set scale in parent space.
    void SetScale(const Vector3& scale);
    /// Set scale in parent space (for Urho2D).
    void SetScale2D(const Vector2& scale) { SetScale(Vector3(scale, 1.0f)); }
    /// Set scale in parent space (for Urho2D).
    void SetScale2D(float x, float y) { SetScale(Vector3(x, y, 1.0f)); }
    /// Set both position and rotation in parent space as an atomic operation. This is faster than setting position and rotation separately.
    void SetTransform(const Vector3& position, const Quaternion& rotation);
    /// Set both position, rotation and uniform scale in parent space as an atomic operation.
    void SetTransform(const Vector3& position, const Quaternion& rotation, float scale);
    /// Set both position, rotation and scale in parent space as an atomic operation.
    void SetTransform(const Vector3& position, const Quaternion& rotation, const Vector3& scale);
    /// Set node transformation in parent space as an atomic operation.
    void SetTransform(const Matrix3x4& matrix);
    /// Set both position and rotation in parent space as an atomic operation (for Urho2D).
    void SetTransform2D(const Vector2& position, float rotation) { SetTransform(Vector3(position), Quaternion(rotation)); }
    /// Set both position, rotation and uniform scale in parent space as an atomic operation (for Urho2D).
    void SetTransform2D(const Vector2& position, float rotation, float scale) { SetTransform(Vector3(position), Quaternion(rotation), scale); }
    /// Set both position, rotation and scale in parent space as an atomic operation (for Urho2D).
    void SetTransform2D(const Vector2& position, float rotation, const Vector2& scale)  { SetTransform(Vector3(position), Quaternion(rotation), Vector3(scale, 1.0f)); }
    /// Set position in world space.
    void SetWorldPosition(const Vector3& position);
    /// Set position in world space (for Urho2D).
    void SetWorldPosition2D(const Vector2& position) { SetWorldPosition(Vector3(position)); }
    /// Set position in world space (for Urho2D).
    void SetWorldPosition2D(float x, float y) { SetWorldPosition(Vector3(x, y, 0.0f)); }
    /// Set rotation in world space.
    void SetWorldRotation(const Quaternion& rotation);
    /// Set rotation in world space (for Urho2D).
    void SetWorldRotation2D(float rotation) { SetWorldRotation(Quaternion(rotation)); }
    /// Set forward direction in world space.
    void SetWorldDirection(const Vector3& direction);
    /// Set uniform scale in world space.
    void SetWorldScale(float scale);
    /// Set scale in world space.
    void SetWorldScale(const Vector3& scale);
    /// Set scale in world space (for Urho2D).
    void SetWorldScale2D(const Vector2& scale) { SetWorldScale(Vector3(scale, 1.0f)); }
    /// Set scale in world space (for Urho2D).
    void SetWorldScale2D(float x, float y) { SetWorldScale(Vector3(x, y, 1.0f)); }
    /// Set both position and rotation in world space as an atomic operation.
    void SetWorldTransform(const Vector3& position, const Quaternion& rotation);
    /// Set both position, rotation and uniform scale in world space as an atomic operation.
    void SetWorldTransform(const Vector3& position, const Quaternion& rotation, float scale);
    /// Set both position, rotation and scale in world space as an atomic opration.
    void SetWorldTransform(const Vector3& position, const Quaternion& rotation, const Vector3& scale);
    /// Set both position and rotation in world space as an atomic operation (for Urho2D).
    void SetWorldTransform2D(const Vector2& position, float rotation) { SetWorldTransform(Vector3(position), Quaternion(rotation)); }
    /// Set both position, rotation and uniform scale in world space as an atomic operation (for Urho2D).
    void SetWorldTransform2D(const Vector2& position, float rotation, float scale) { SetWorldTransform(Vector3(position), Quaternion(rotation), scale); }
    /// Set both position, rotation and scale in world space as an atomic opration (for Urho2D).
    void SetWorldTransform2D(const Vector2& position, float rotation, const Vector2& scale) { SetWorldTransform(Vector3(position), Quaternion(rotation), Vector3(scale, 1.0f)); }
    /// Move the scene node in the chosen transform space.
    void Translate(const Vector3& delta, TransformSpace space = TS_LOCAL);
    /// Move the scene node in the chosen transform space (for Urho2D).
    void Translate2D(const Vector2& delta, TransformSpace space = TS_LOCAL) { Translate(Vector3(delta), space); }
    /// Rotate the scene node in the chosen transform space.
    void Rotate(const Quaternion& delta, TransformSpace space = TS_LOCAL);
    /// Rotate the scene node in the chosen transform space (for Urho2D).
    void Rotate2D(float delta, TransformSpace space = TS_LOCAL) { Rotate(Quaternion(delta), space); }
    /// Rotate around a point in the chosen transform space.
    void RotateAround(const Vector3& point, const Quaternion& delta, TransformSpace space = TS_LOCAL);
    /// Rotate around a point in the chosen transform space (for Urho2D).
    void RotateAround2D(const Vector2& point, float delta, TransformSpace space = TS_LOCAL) { RotateAround(Vector3(point), Quaternion(delta), space); }
    /// Rotate around the X axis.
    void Pitch(float angle, TransformSpace space = TS_LOCAL);
    /// Rotate around the Y axis.
    void Yaw(float angle, TransformSpace space = TS_LOCAL);
    /// Rotate around the Z axis.
    void Roll(float angle, TransformSpace space = TS_LOCAL);
    /// Look at a target position in the chosen transform space. Note that the up vector is always specified in world space. Return true if successful, or false if resulted in an illegal rotation, in which case the current rotation remains.
    bool LookAt(const Vector3& target, const Vector3& up = Vector3::UP, TransformSpace space = TS_WORLD);
    /// Modify scale in parent space uniformly.
    void Scale(float scale);
    /// Modify scale in parent space.
    void Scale(const Vector3& scale);
    /// Modify scale in parent space (for Urho2D).
    void Scale2D(const Vector2& scale) { Scale(Vector3(scale, 1.0f)); }
    /// Set enabled/disabled state without recursion. Components in a disabled node become effectively disabled regardless of their own enable/disable state.
    void SetEnabled(bool enable);
    /// Set enabled state on self and child nodes. Nodes' own enabled state is remembered (IsEnabledSelf) and can be restored.
    void SetDeepEnabled(bool enable);
    /// Reset enabled state to the node's remembered state prior to calling SetDeepEnabled.
    void ResetDeepEnabled();
    /// Set enabled state on self and child nodes. Unlike SetDeepEnabled this does not remember the nodes' own enabled state, but overwrites it.
    void SetEnabledRecursive(bool enable);
    /// Set owner connection for networking.
    void SetOwner(Connection* owner);
    /// Mark node and child nodes to need world transform recalculation. Notify listener components.
    void MarkDirty();
    /// Create a child scene node (with specified ID if provided).
    Node* CreateChild(const QString& name = QString(), CreateMode mode = REPLICATED, unsigned id = 0, bool temporary = false);
    /// Create a temporary child scene node (with specified ID if provided).
    Node* CreateTemporaryChild(const QString& name = QString(), CreateMode mode = REPLICATED, unsigned id = 0);
    /// Add a child scene node at a specific index. If index is not explicitly specified or is greater than current children size, append the new child at the end.
    void AddChild(Node* node, unsigned index = M_MAX_UNSIGNED);
    /// Remove a child scene node.
    void RemoveChild(Node* node);
    /// Remove all child scene nodes.
    void RemoveAllChildren();
    /// Remove child scene nodes that match criteria.
    void RemoveChildren(bool removeReplicated, bool removeLocal, bool recursive);
    /// Create a component to this node (with specified ID if provided).
    Component* CreateComponent(StringHash type, CreateMode mode = REPLICATED, unsigned id = 0);
    /// Create a component to this node if it does not exist already.
    Component* GetOrCreateComponent(StringHash type, CreateMode mode = REPLICATED, unsigned id = 0);
    /// Clone a component from another node using its create mode. Return the clone if successful or null on failure.
    Component* CloneComponent(Component* component, unsigned id = 0);
    /// Clone a component from another node and specify the create mode. Return the clone if successful or null on failure.
    Component* CloneComponent(Component* component, CreateMode mode, unsigned id = 0);
    /// Remove a component from this node.
    void RemoveComponent(Component* component);
    /// Remove the first component of specific type from this node.
    void RemoveComponent(StringHash type);
    /// Remove components that match criteria.
    void RemoveComponents(bool removeReplicated, bool removeLocal);
    /// Remove all components of specific type.
    void RemoveComponents(StringHash type);
    /// Remove all components from this node.
    void RemoveAllComponents();
    /// Adjust index order of an existing component in this node.
    void ReorderComponent(Component* component, unsigned index);
    /// Clone scene node, components and child nodes. Return the clone.
    Node* Clone(CreateMode mode = REPLICATED);
    /// Remove from the parent node. If no other shared pointer references exist, causes immediate deletion.
    void Remove();
    /// Assign to a new parent scene node. Retains the world transform.
    void SetParent(Node* parent);
    /// Set a user variable.
    void SetVar(StringHash key, const Variant& value);
    /// Add listener component that is notified of node being dirtied. Can either be in the same node or another.
    void AddListener(Component* component);
    /// Remove listener component.
    void RemoveListener(Component* component);
    /// Template version of creating a component.
    template <class T> T* CreateComponent(CreateMode mode = REPLICATED, unsigned id = 0);
    /// Template version of getting or creating a component.
    template <class T> T* GetOrCreateComponent(CreateMode mode = REPLICATED, unsigned id = 0);
    /// Template version of removing a component.
    template <class T> void RemoveComponent();
    /// Template version of removing all components of specific type.
    template <class T> void RemoveComponents();

    /// Return ID.
    unsigned GetID() const { return id_; }
    /// Return whether the node is replicated or local to a scene.
    bool IsReplicated() const;
    /// Return name.
    const QString& GetName() const;
    /// Return name hash.
    StringHash GetNameHash() const;
    /// Return all tags.
    const QStringList& GetTags() const;

    /// Return whether has a specific tag.
    bool HasTag(const QString &tag) const;
    /// Return parent scene node.
    Node* GetParent() const { return parent_; }
    /// Return scene.
    Scene* GetScene() const { return scene_; }
    /// Return whether is a direct or indirect child of specified node.
    bool IsChildOf(Node* node) const;
    /// Return whether is enabled. Disables nodes effectively disable all their components.
    bool IsEnabled() const { return enabled_; }
    /// Returns the node's last own enabled state. May be different than the value returned by IsEnabled when SetDeepEnabled has been used.
    bool IsEnabledSelf() const { return enabledPrev_; }
    /// Return owner connection in networking.
    Connection* GetOwner() const;
    /// Return position in parent space.
    const Vector3& GetPosition() const { return position_; }
    /// Return position in parent space (for Urho2D).
    Vector2 GetPosition2D() const { return Vector2(position_.x_, position_.y_); }
    /// Return rotation in parent space.
    const Quaternion& GetRotation() const { return rotation_; }
    /// Return rotation in parent space (for Urho2D).
    float GetRotation2D() const { return rotation_.RollAngle(); }
    /// Return forward direction in parent space. Positive Z axis equals identity rotation.
    Vector3 GetDirection() const { return rotation_ * Vector3::FORWARD; }
    /// Return up direction in parent space. Positive Y axis equals identity rotation.
    Vector3 GetUp() const { return rotation_ * Vector3::UP; }
    /// Return right direction in parent space. Positive X axis equals identity rotation.
    Vector3 GetRight() const { return rotation_ * Vector3::RIGHT; }

    /// Return scale in parent space.
    const Vector3& GetScale() const { return scale_; }
    /// Return scale in parent space (for Urho2D).
    Vector2 GetScale2D() const { return Vector2(scale_.x_, scale_.y_); }
    /// Return parent space transform matrix.
    Matrix3x4 GetTransform() const { return Matrix3x4(position_, rotation_, scale_); }

    /// Return position in world space.
    Vector3 GetWorldPosition() const
    {
        if (dirty_)
            UpdateWorldTransform();

        return worldTransform_.Translation();
    }

    /// Return position in world space (for Urho2D).
    Vector2 GetWorldPosition2D() const
    {
        Vector3 worldPosition = GetWorldPosition();
        return Vector2(worldPosition.x_, worldPosition.y_);
    }

    /// Return rotation in world space.
    Quaternion GetWorldRotation() const
    {
        if (dirty_)
            UpdateWorldTransform();

        return worldRotation_;
    }

    /// Return rotation in world space (for Urho2D).
    float GetWorldRotation2D() const
    {
        return GetWorldRotation().RollAngle();
    }

    /// Return direction in world space.
    Vector3 GetWorldDirection() const
    {
        if (dirty_)
            UpdateWorldTransform();

        return worldRotation_ * Vector3::FORWARD;
    }

    /// Return node's up vector in world space.
    Vector3 GetWorldUp() const
    {
        if (dirty_)
            UpdateWorldTransform();

        return worldRotation_ * Vector3::UP;
    }

    /// Return node's right vector in world space.
    Vector3 GetWorldRight() const
    {
        if (dirty_)
            UpdateWorldTransform();

        return worldRotation_ * Vector3::RIGHT;
    }

    /// Return scale in world space.
    Vector3 GetWorldScale() const
    {
        if (dirty_)
            UpdateWorldTransform();

        return worldTransform_.Scale();
    }

    /// Return signed scale in world space. Utilized for Urho2D physics.
    Vector3 GetSignedWorldScale() const;

    /// Return scale in world space (for Urho2D).
    Vector2 GetWorldScale2D() const
    {
        Vector3 worldScale = GetWorldScale();
        return Vector2(worldScale.x_, worldScale.y_);
    }

    /// Return world space transform matrix.
    const Matrix3x4& GetWorldTransform() const
    {
        if (dirty_)
            UpdateWorldTransform();

        return worldTransform_;
    }

    Vector3 LocalToWorld(const Vector3& position) const;
    Vector3 LocalToWorld(const Vector4& vector) const;
    Vector2 LocalToWorld2D(const Vector2& vector) const;
    Vector3 WorldToLocal(const Vector3& position) const;
    Vector3 WorldToLocal(const Vector4& vector) const;
    Vector2 WorldToLocal2D(const Vector2& vector) const;
    /// Return whether transform has changed and world transform needs recalculation.
    bool IsDirty() const { return dirty_; }
    unsigned GetNumChildren(bool recursive = false) const;
    /// Return immediate child scene nodes.
    const std::vector<SharedPtr<Node> >& GetChildren() const { return children_; }
    void GetChildren(std::vector<Node*>& dest, bool recursive = false) const;
    std::vector<Node*> GetChildren(bool recursive) const;
    void GetChildrenWithComponent(std::vector<Node*>& dest, StringHash type, bool recursive = false) const;
    std::vector<Node*> GetChildrenWithComponent(StringHash type, bool recursive = false) const;
    void GetChildrenWithTag(std::vector<Node *> &dest, const QString &tag, bool recursive = false) const;
    std::vector<Node*> GetChildrenWithTag(const QString& tag, bool recursive = false) const;
    Node* GetChild(unsigned index) const;
    Node* GetChild(const QStringRef& name, bool recursive = false) const;
    Node* GetChild(const char* name, bool recursive = false) const;
    Node* GetChild(StringHash nameHash, bool recursive = false) const;
    /// Return number of components.
    size_t GetNumComponents() const { return components_.size(); }
    unsigned GetNumNetworkComponents() const;
    /// Return all components.
    const std::vector<SharedPtr<Component> >& GetComponents() const { return components_; }
    void GetComponents(std::vector<Component*>& dest, StringHash type, bool recursive = false) const;
    Component* GetComponent(StringHash type, bool recursive = false) const;
    Component* GetParentComponent(StringHash type, bool fullTraversal = false) const;
    bool HasComponent(StringHash type) const;
    const Variant& GetVar(StringHash key) const;
    /// Return all user variables.
    const VariantMap& GetVars() const { return vars_; }
    /// Return first component derived from class.
    template <class T> T* GetDerivedComponent(bool recursive = false) const;
    /// Return first component derived from class in the parent node, or if fully traversing then the first node up the tree with one.
    template <class T> T* GetParentDerivedComponent(bool fullTraversal = false) const;
    /// Return components derived from class.
    template <class T> void GetDerivedComponents(std::vector<T*>& dest, bool recursive = false, bool clearVector = true) const;
    /// Template version of returning child nodes with a specific component.
    template <class T> void GetChildrenWithComponent(std::vector<Node*>& dest, bool recursive = false) const;
    /// Template version of returning a component by type.
    template <class T> T* GetComponent(bool recursive = false) const;
    /// Template version of returning a parent's component by type.
    template <class T> T* GetParentComponent(bool fullTraversal = false) const;
    /// Template version of returning all components of type.
    template <class T> void GetComponents(std::vector<T*>& dest, bool recursive = false) const;
    /// Template version of checking whether has a specific component.
    template <class T> bool HasComponent() const;

    /// Set ID. Called by Scene.
    void SetID(unsigned id);
    /// Set scene. Called by Scene.
    void SetScene(Scene* scene);
    /// Reset scene, ID and owner. Called by Scene.
    void ResetScene();
    /// Set network position attribute.
    void SetNetPositionAttr(const Vector3& value);
    /// Set network rotation attribute.
    void SetNetRotationAttr(const std::vector<uint8_t>& value);
    /// Set network parent attribute.
    void SetNetParentAttr(const std::vector<uint8_t>& value);
    /// Return network position attribute.
    const Vector3& GetNetPositionAttr() const;
    /// Return network rotation attribute.
    const std::vector<uint8_t>& GetNetRotationAttr() const;
    /// Return network parent attribute.
    const std::vector<uint8_t>& GetNetParentAttr() const;
    /// Load components and optionally load child nodes.
    bool Load(Deserializer& source, SceneResolver& resolver, bool loadChildren = true, bool rewriteIDs = false, CreateMode mode = REPLICATED);
    /// Load components from XML data and optionally load child nodes.
    bool LoadXML(const XMLElement& source, SceneResolver& resolver, bool loadChildren = true, bool rewriteIDs = false, CreateMode mode = REPLICATED);
    /// Load components from XML data and optionally load child nodes.
    bool LoadJSON(const JSONValue& source, SceneResolver& resolver, bool loadChildren = true, bool rewriteIDs = false, CreateMode mode = REPLICATED);
    /// Return the depended on nodes to order network updates.
    const std::vector<Node*>& GetDependencyNodes() const;
    /// Prepare network update by comparing attributes and marking replication states dirty as necessary.
    void PrepareNetworkUpdate();
    /// Clean up all references to a network connection that is about to be removed.
    void CleanupConnection(Connection* connection);
    /// Mark node dirty in scene replication states.
    void MarkReplicationDirty();
    /// Create a child node with specific ID.
    Node* CreateChild(unsigned id, CreateMode mode, bool temporary = false);
    /// Add a pre-created component. Using this function from application code is discouraged, as component operation without an owner node may not be well-defined in all cases. Prefer CreateComponent() instead.
    void AddComponent(Component* component, unsigned id, CreateMode mode);
    /// Calculate number of non-temporary child nodes.
    unsigned GetNumPersistentChildren() const;
    /// Calculate number of non-temporary components.
    unsigned GetNumPersistentComponents() const;
    /// Set position in parent space silently without marking the node & child nodes dirty. Used by animation code.
    void SetPositionSilent(const Vector3& position) { position_ = position; }
    /// Set position in parent space silently without marking the node & child nodes dirty. Used by animation code.
    void SetRotationSilent(const Quaternion& rotation) { rotation_ = rotation; }
    /// Set scale in parent space silently without marking the node & child nodes dirty. Used by animation code.
    void SetScaleSilent(const Vector3& scale) { scale_ = scale; }
    /// Set local transform silently without marking the node & child nodes dirty. Used by animation code.
    void SetTransformSilent(const Vector3& position, const Quaternion& rotation, const Vector3& scale);

    // Signals are part of the public interface.

    /// if node is part of physics simulation, those will be used
    struct Physics2DNodeSignals *physics2dSignals_ = nullptr;

protected:
    /// Handle attribute animation added.
    void OnAttributeAnimationAdded() override;
    /// Handle attribute animation removed.
    void OnAttributeAnimationRemoved() override;
    /// Find target of an attribute animation from object hierarchy by name.
    Animatable* FindAttributeAnimationTarget(const QString& name, QString& outName) override;


private:
    /// Set enabled/disabled state with optional recursion. Optionally affect the remembered enable state.
    void SetEnabled(bool enable, bool recursive, bool storeSelf);
    /// Create component, allowing UnknownComponent if actual type is not supported. Leave typeName empty if not known.
    Component* SafeCreateComponent(const QStringRef &typeName, StringHash type, CreateMode mode, unsigned id);
    /// Recalculate the world transform.
    void UpdateWorldTransform() const;
    /// Remove child node by iterator.
    void RemoveChild(std::vector<SharedPtr<Node> >::iterator i);
    /// Return child nodes recursively.
    void GetChildrenRecursive(std::vector<Node*>& dest) const;
    /// Return child nodes with a specific component recursively.
    void GetChildrenWithComponentRecursive(std::vector<Node*>& dest, StringHash type) const;
    /// Return child nodes with a specific tag recursively.
    void GetChildrenWithTagRecursive(std::vector<Node*>& dest, const QString &tag) const;
    /// Return specific components recursively.
    void GetComponentsRecursive(std::vector<Component*>& dest, StringHash type) const;
    /// Clone node recursively.
    Node* CloneRecursive(Node* parent, SceneResolver& resolver, CreateMode mode);
    /// Remove a component from this node with the specified iterator.
    void RemoveComponent(std::vector<SharedPtr<Component> >::iterator i);
    /// Handle attribute animation update event.
    void HandleAttributeAnimationUpdate(Scene *s,float ts);

    /// World-space transform matrix.
    mutable Matrix3x4 worldTransform_;
    /// World transform needs update flag.
    mutable bool dirty_;
    /// Enabled flag.
    bool enabled_;
    /// Last SetEnabled flag before any SetDeepEnabled.
    bool enabledPrev_;
protected:
    /// Network update queued flag.
    bool networkUpdate_;
private:
    Node *                             parent_;        //!< Parent scene node.
    Scene *                            scene_;         //!< Scene (root node.)
    unsigned                           id_;            //!< Unique ID within the scene.
    Vector3                            position_;      //!< Position.
    Quaternion                         rotation_;      //!< Rotation.
    Vector3                            scale_;         //!< Scale.
    mutable Quaternion                 worldRotation_; //!< World-space rotation.
    std::vector<SharedPtr<Component>>  components_;    //!< Components.
    std::vector<SharedPtr<Node>>       children_;      //!< Child scene nodes.
    const std::unique_ptr<NodePrivate> impl_;          //!< Pointer to implementation.

protected:
    /// User variables.
    VariantMap vars_;
};

template <class T> T* Node::CreateComponent(CreateMode mode, unsigned id)
{
    return static_cast<T*>(CreateComponent(T::GetTypeStatic(), mode, id));
}
template <class T> T* Node::GetOrCreateComponent(CreateMode mode, unsigned id)
{
    return static_cast<T*>(GetOrCreateComponent(T::GetTypeStatic(), mode, id));
}
template <class T> void Node::RemoveComponent() { RemoveComponent(T::GetTypeStatic()); }
template <class T> void Node::RemoveComponents() { RemoveComponents(T::GetTypeStatic()); }

template <class T> void Node::GetChildrenWithComponent(std::vector<Node*>& dest, bool recursive) const
{
    GetChildrenWithComponent(dest, T::GetTypeStatic(), recursive);
}
template <class T> T* Node::GetComponent(bool recursive) const
{
    return static_cast<T*>(GetComponent(T::GetTypeStatic(), recursive));
}

template <class T> T* Node::GetParentComponent(bool fullTraversal) const
{
    return static_cast<T*>(GetParentComponent(T::GetTypeStatic(), fullTraversal));
}
template <class T> void Node::GetComponents(std::vector<T*>& dest, bool recursive) const
{
    GetComponents(reinterpret_cast<std::vector<Component*>&>(dest), T::GetTypeStatic(), recursive);
}
template <class T> bool Node::HasComponent() const
{
    return HasComponent(T::GetTypeStatic());
}

template <class T> T* Node::GetDerivedComponent(bool recursive) const
{
    for (const SharedPtr<Component> & elem : components_)
    {
        T* component = dynamic_cast<T*>(elem.Get());
        if (component)
            return component;
    }

    if (recursive)
    {
        for (const SharedPtr<Node> & elem : children_)
        {
            T* component = elem->GetDerivedComponent<T>(true);
            if (component)
                return component;
        }
    }
    return nullptr;
}

template <class T> T* Node::GetParentDerivedComponent(bool fullTraversal) const
{
    Node* current = GetParent();
    while (current)
    {
        T* soughtComponent = current->GetDerivedComponent<T>();
        if (soughtComponent)
            return soughtComponent;

        if (fullTraversal)
            current = current->GetParent();
        else
            break;
    }
    return nullptr;
}

template <class T> void Node::GetDerivedComponents(std::vector<T*>& dest, bool recursive, bool clearVector) const
    {
    if (clearVector)
        dest.clear();
    for (const SharedPtr<Component> & elem : components_)
    {
        T* component = dynamic_cast<T*>(elem.Get());
        if (component)
            dest.push_back(component);
    }
    if (recursive)
    {
        for (const SharedPtr<Node> & elem : children_)
            elem->GetDerivedComponents<T>(dest, true, false);
    }
}

}
