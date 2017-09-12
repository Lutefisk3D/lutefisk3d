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
#include "RigidBody.h"

#include "CollisionShape.h"
#include "Constraint.h"
#include "Lutefisk3D/Core/Context.h"
#include "Lutefisk3D/IO/Log.h"
#include "Lutefisk3D/IO/MemoryBuffer.h"
#include "Lutefisk3D/IO/File.h"
#include "PhysicsUtils.h"
#include "PhysicsWorld.h"
#include "Lutefisk3D/Core/Profiler.h"
#include "Lutefisk3D/Resource/BackgroundLoader.h"
#include "Lutefisk3D/Resource/ResourceCache.h"
#include "Lutefisk3D/Scene/Scene.h"
#include "Lutefisk3D/Scene/SceneEvents.h"
#include "Lutefisk3D/Scene/SmoothedTransform.h"

#include <Bullet/BulletDynamics/Dynamics/btDiscreteDynamicsWorld.h>
#include <Bullet/BulletDynamics/Dynamics/btRigidBody.h>
#include <Bullet/BulletCollision/CollisionShapes/btCompoundShape.h>
#include <Bullet/LinearMath/btMotionState.h>

namespace Urho3D
{
struct RigidBodyPrivate : public btMotionState
{
    RigidBodyPrivate(RigidBody *o) : owner(o),
        compoundShape_(new btCompoundShape()),
        shiftedCompoundShape_(new btCompoundShape())
    {}
    /// Return initial world transform to Bullet.
    virtual void getWorldTransform(btTransform &worldTrans) const override;
    /// Update world transform from Bullet.
    virtual void setWorldTransform(const btTransform &worldTrans) override;

    RigidBody *owner = nullptr;
    /// Bullet rigid body.
    std::unique_ptr<btRigidBody> body_;
    /// Bullet compound collision shape.
    std::unique_ptr<btCompoundShape> compoundShape_;
    /// Compound collision shape with center of mass offset applied.
    std::unique_ptr<btCompoundShape> shiftedCompoundShape_;
    /// Last interpolated position from the simulation.
    mutable Vector3 lastPosition_  {0,0,0};
    /// Last interpolated rotation from the simulation.
    mutable Quaternion lastRotation_ = Quaternion::IDENTITY;
    /// Internal flag whether has simulated at least once.
    mutable bool hasSimulated_=false;

};
static const float DEFAULT_MASS = 0.0f;
static const float DEFAULT_FRICTION = 0.5f;
static const float DEFAULT_RESTITUTION = 0.0f;
static const float DEFAULT_ROLLING_FRICTION = 0.0f;
static const unsigned DEFAULT_COLLISION_LAYER = 0x1;
static const unsigned DEFAULT_COLLISION_MASK = M_MAX_UNSIGNED;

static const char* collisionEventModeNames[] =
{
    "Never",
    "When Active",
    "Always",
    nullptr
};

extern const char* PHYSICS_CATEGORY;

RigidBody::RigidBody(Context* context) :
    Component(context),
    gravityOverride_(Vector3::ZERO),
    centerOfMass_(Vector3::ZERO),
    mass_(DEFAULT_MASS),
    collisionLayer_(DEFAULT_COLLISION_LAYER),
    collisionMask_(DEFAULT_COLLISION_MASK),
    collisionEventMode_(COLLISION_ACTIVE),
    kinematic_(false),
    trigger_(false),
    useGravity_(true),
    readdBody_(false),
    inWorld_(false),
    enableMassUpdate_(true)
{
}

RigidBody::~RigidBody()
{
    ReleaseBody();

    if (physicsWorld_)
        physicsWorld_->RemoveRigidBody(this);
}

void RigidBody::RegisterObject(Context* context)
{
    context->RegisterFactory<RigidBody>(PHYSICS_CATEGORY);

    URHO3D_ACCESSOR_ATTRIBUTE("Is Enabled", IsEnabled, SetEnabled, bool, true, AM_DEFAULT);
    URHO3D_MIXED_ACCESSOR_ATTRIBUTE("Physics Rotation", GetRotation, SetRotation, Quaternion, Quaternion::IDENTITY, AM_FILE | AM_NOEDIT);
    URHO3D_MIXED_ACCESSOR_ATTRIBUTE("Physics Position", GetPosition, SetPosition, Vector3, Vector3::ZERO, AM_FILE | AM_NOEDIT);
    URHO3D_ATTRIBUTE("Mass", float, mass_, DEFAULT_MASS, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Friction", GetFriction, SetFriction, float, DEFAULT_FRICTION, AM_DEFAULT);
    URHO3D_MIXED_ACCESSOR_ATTRIBUTE("Anisotropic Friction", GetAnisotropicFriction, SetAnisotropicFriction, Vector3, Vector3::ONE, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Rolling Friction", GetRollingFriction, SetRollingFriction, float, DEFAULT_ROLLING_FRICTION, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Restitution", GetRestitution, SetRestitution, float, DEFAULT_RESTITUTION, AM_DEFAULT);
    URHO3D_MIXED_ACCESSOR_ATTRIBUTE("Linear Velocity", GetLinearVelocity, SetLinearVelocity, Vector3, Vector3::ZERO, AM_DEFAULT | AM_LATESTDATA);
    URHO3D_MIXED_ACCESSOR_ATTRIBUTE("Angular Velocity", GetAngularVelocity, SetAngularVelocity, Vector3, Vector3::ZERO, AM_FILE);
    URHO3D_MIXED_ACCESSOR_ATTRIBUTE("Linear Factor", GetLinearFactor, SetLinearFactor, Vector3, Vector3::ONE, AM_DEFAULT);
    URHO3D_MIXED_ACCESSOR_ATTRIBUTE("Angular Factor", GetAngularFactor, SetAngularFactor, Vector3, Vector3::ONE, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Linear Damping", GetLinearDamping, SetLinearDamping, float, 0.0f, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Angular Damping", GetAngularDamping, SetAngularDamping, float, 0.0f, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Linear Rest Threshold", GetLinearRestThreshold, SetLinearRestThreshold, float, 0.8f, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Angular Rest Threshold", GetAngularRestThreshold, SetAngularRestThreshold, float, 1.0f, AM_DEFAULT);
    URHO3D_ATTRIBUTE("Collision Layer", int, collisionLayer_, DEFAULT_COLLISION_LAYER, AM_DEFAULT);
    URHO3D_ATTRIBUTE("Collision Mask", int, collisionMask_, DEFAULT_COLLISION_MASK, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Contact Threshold", GetContactProcessingThreshold, SetContactProcessingThreshold, float, BT_LARGE_FLOAT, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("CCD Radius", GetCcdRadius, SetCcdRadius, float, 0.0f, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("CCD Motion Threshold", GetCcdMotionThreshold, SetCcdMotionThreshold, float, 0.0f, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Network Angular Velocity", GetNetAngularVelocityAttr, SetNetAngularVelocityAttr, std::vector<unsigned char>, Variant::emptyBuffer, AM_NET | AM_LATESTDATA | AM_NOEDIT);
    URHO3D_ENUM_ATTRIBUTE("Collision Event Mode", collisionEventMode_, collisionEventModeNames, COLLISION_ACTIVE, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Use Gravity", GetUseGravity, SetUseGravity, bool, true, AM_DEFAULT);
    URHO3D_ATTRIBUTE("Is Kinematic", bool, kinematic_, false, AM_DEFAULT);
    URHO3D_ATTRIBUTE("Is Trigger", bool, trigger_, false, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Gravity Override", GetGravityOverride, SetGravityOverride, Vector3, Vector3::ZERO, AM_DEFAULT);
}

void RigidBody::OnSetAttribute(const AttributeInfo& attr, const Variant& src)
{
    Serializable::OnSetAttribute(attr, src);

    // Change of any non-accessor attribute requires the rigid body to be re-added to the physics world
    if (!attr.accessor_)
        readdBody_ = true;
}

void RigidBody::ApplyAttributes()
{
    if (readdBody_)
        AddBodyToWorld();
}

void RigidBody::OnSetEnabled()
{
    bool enabled = IsEnabledEffective();

    if (enabled && !inWorld_)
        AddBodyToWorld();
    else if (!enabled && inWorld_)
        RemoveBodyFromWorld();
}

void RigidBodyPrivate::getWorldTransform(btTransform &worldTrans) const
{
    // We may be in a pathological state where a RigidBody exists without a scene node when this callback is fired,
    // so check to be sure
    if (owner->GetNode())
    {
        lastPosition_ = owner->GetNode()->GetWorldPosition();
        lastRotation_ = owner->GetNode()->GetWorldRotation();
        worldTrans.setOrigin(ToBtVector3(lastPosition_ + lastRotation_ * owner->GetCenterOfMass()));
        worldTrans.setRotation(ToBtQuaternion(lastRotation_));
    }
    hasSimulated_ = true;
}

void RigidBodyPrivate::setWorldTransform(const btTransform &worldTrans)
{
    Quaternion newWorldRotation = ToQuaternion(worldTrans.getRotation());
    Vector3 newWorldPosition = ToVector3(worldTrans.getOrigin()) - newWorldRotation * owner->GetCenterOfMass();
    RigidBody* parentRigidBody = nullptr;

    // It is possible that the RigidBody component has been kept alive via a shared pointer,
    // while its scene node has already been destroyed
    if (owner->GetNode())
    {
        // If the rigid body is parented to another rigid body, can not set the transform immediately.
        // In that case store it to PhysicsWorld for delayed assignment
        Node* parent = owner->GetNode()->GetParent();
        if (parent != owner->GetScene() && parent)
            parentRigidBody = parent->GetComponent<RigidBody>();

        if (!parentRigidBody)
            owner->ApplyWorldTransform(newWorldPosition, newWorldRotation);
        else
        {
            DelayedWorldTransform delayed;
            delayed.rigidBody_ = owner;
            delayed.parentRigidBody_ = parentRigidBody;
            delayed.worldPosition_ = newWorldPosition;
            delayed.worldRotation_ = newWorldRotation;
            owner->GetPhysicsWorld()->AddDelayedWorldTransform(delayed);
        }

        owner->MarkNetworkUpdate();
    }
    hasSimulated_ = true;
}

void RigidBody::DrawDebugGeometry(DebugRenderer* debug, bool depthTest)
{
    if (debug && physicsWorld_ && private_data->body_ && IsEnabledEffective())
    {
        physicsWorld_->SetDebugRenderer(debug);
        physicsWorld_->SetDebugDepthTest(depthTest);

        btDiscreteDynamicsWorld* world = physicsWorld_->GetWorld();
        world->debugDrawObject(private_data->body_->getWorldTransform(), private_data->shiftedCompoundShape_.get(),
                               IsActive() ? btVector3(1.0f, 1.0f, 1.0f) : btVector3(0.0f, 1.0f, 0.0f));

        physicsWorld_->SetDebugRenderer(nullptr);
    }
}

void RigidBody::SetMass(float mass)
{
    mass = Max(mass, 0.0f);

    if (mass != mass_)
    {
        mass_ = mass;
        AddBodyToWorld();
        MarkNetworkUpdate();
    }
}

void RigidBody::SetPosition(const Vector3& position)
{
    if (private_data->body_)
    {
        btTransform& worldTrans = private_data->body_->getWorldTransform();
        worldTrans.setOrigin(ToBtVector3(position + ToQuaternion(worldTrans.getRotation()) * centerOfMass_));

        // When forcing the physics position, set also interpolated position so that there is no jitter
        // When not inside the simulation loop, this may lead to erratic movement of parented rigidbodies
        // so skip in that case. Exception made before first simulation tick so that interpolation position
        // of e.g. instantiated prefabs will be correct from the start
        if (!private_data->hasSimulated_ || physicsWorld_->IsSimulating())
        {
            btTransform interpTrans = private_data->body_->getInterpolationWorldTransform();
            interpTrans.setOrigin(worldTrans.getOrigin());
            private_data->body_->setInterpolationWorldTransform(interpTrans);
        }

        Activate();
        MarkNetworkUpdate();
    }
}

void RigidBody::SetRotation(const Quaternion& rotation)
{
    if (private_data->body_)
    {
        Vector3 oldPosition = GetPosition();
        btTransform& worldTrans = private_data->body_->getWorldTransform();
        worldTrans.setRotation(ToBtQuaternion(rotation));
        if (!centerOfMass_.Equals(Vector3::ZERO))
            worldTrans.setOrigin(ToBtVector3(oldPosition + rotation * centerOfMass_));

        if (!private_data->hasSimulated_ || physicsWorld_->IsSimulating())
        {
            btTransform interpTrans = private_data->body_->getInterpolationWorldTransform();
            interpTrans.setRotation(worldTrans.getRotation());
            if (!centerOfMass_.Equals(Vector3::ZERO))
                interpTrans.setOrigin(worldTrans.getOrigin());
            private_data->body_->setInterpolationWorldTransform(interpTrans);
        }
        private_data->body_->updateInertiaTensor();

        Activate();
        MarkNetworkUpdate();
    }
}

void RigidBody::SetTransform(const Vector3& position, const Quaternion& rotation)
{
    if (private_data->body_)
    {
        btTransform& worldTrans = private_data->body_->getWorldTransform();
        worldTrans.setRotation(ToBtQuaternion(rotation));
        worldTrans.setOrigin(ToBtVector3(position + rotation * centerOfMass_));

        if (!private_data->hasSimulated_ || physicsWorld_->IsSimulating())
        {
            btTransform interpTrans = private_data->body_->getInterpolationWorldTransform();
            interpTrans.setOrigin(worldTrans.getOrigin());
            interpTrans.setRotation(worldTrans.getRotation());
            private_data->body_->setInterpolationWorldTransform(interpTrans);
        }
        private_data->body_->updateInertiaTensor();

        Activate();
        MarkNetworkUpdate();
    }
}

void RigidBody::SetLinearVelocity(const Vector3& velocity)
{
    if (private_data->body_)
    {
        private_data->body_->setLinearVelocity(ToBtVector3(velocity));
        if (velocity != Vector3::ZERO)
            Activate();
        MarkNetworkUpdate();
    }
}

void RigidBody::SetLinearFactor(const Vector3& factor)
{
    if (private_data->body_)
    {
        private_data->body_->setLinearFactor(ToBtVector3(factor));
        MarkNetworkUpdate();
    }
}

void RigidBody::SetLinearRestThreshold(float threshold)
{
    if (private_data->body_)
    {
        private_data->body_->setSleepingThresholds(threshold, private_data->body_->getAngularSleepingThreshold());
        MarkNetworkUpdate();
    }
}

void RigidBody::SetLinearDamping(float damping)
{
    if (private_data->body_)
    {
        private_data->body_->setDamping(damping, private_data->body_->getAngularDamping());
        MarkNetworkUpdate();
    }
}

void RigidBody::SetAngularVelocity(const Vector3& velocity)
{
    if (private_data->body_)
    {
        private_data->body_->setAngularVelocity(ToBtVector3(velocity));
        if (velocity != Vector3::ZERO)
            Activate();
        MarkNetworkUpdate();
    }
}

void RigidBody::SetAngularFactor(const Vector3& factor)
{
    if (private_data->body_)
    {
        private_data->body_->setAngularFactor(ToBtVector3(factor));
        MarkNetworkUpdate();
    }
}

void RigidBody::SetAngularRestThreshold(float threshold)
{
    if (private_data->body_)
    {
        private_data->body_->setSleepingThresholds(private_data->body_->getLinearSleepingThreshold(), threshold);
        MarkNetworkUpdate();
    }
}

void RigidBody::SetAngularDamping(float damping)
{
    if (private_data->body_)
    {
        private_data->body_->setDamping(private_data->body_->getLinearDamping(), damping);
        MarkNetworkUpdate();
    }
}

void RigidBody::SetFriction(float friction)
{
    if (private_data->body_)
    {
        private_data->body_->setFriction(friction);
        MarkNetworkUpdate();
    }
}

void RigidBody::SetAnisotropicFriction(const Vector3& friction)
{
    if (private_data->body_)
    {
        private_data->body_->setAnisotropicFriction(ToBtVector3(friction));
        MarkNetworkUpdate();
    }
}

void RigidBody::SetRollingFriction(float friction)
{
    if (private_data->body_)
    {
        private_data->body_->setRollingFriction(friction);
        MarkNetworkUpdate();
    }
}

void RigidBody::SetRestitution(float restitution)
{
    if (private_data->body_)
    {
        private_data->body_->setRestitution(restitution);
        MarkNetworkUpdate();
    }
}

void RigidBody::SetContactProcessingThreshold(float threshold)
{
    if (private_data->body_)
    {
        private_data->body_->setContactProcessingThreshold(threshold);
        MarkNetworkUpdate();
    }
}

void RigidBody::SetCcdRadius(float radius)
{
    radius = Max(radius, 0.0f);
    if (private_data->body_)
    {
        private_data->body_->setCcdSweptSphereRadius(radius);
        MarkNetworkUpdate();
    }
}

void RigidBody::SetCcdMotionThreshold(float threshold)
{
    threshold = Max(threshold, 0.0f);
    if (private_data->body_)
    {
        private_data->body_->setCcdMotionThreshold(threshold);
        MarkNetworkUpdate();
    }
}

void RigidBody::SetUseGravity(bool enable)
{
    if (enable != useGravity_)
    {
        useGravity_ = enable;
        UpdateGravity();
        MarkNetworkUpdate();
    }
}

void RigidBody::SetGravityOverride(const Vector3& gravity)
{
    if (gravity != gravityOverride_)
    {
        gravityOverride_ = gravity;
        UpdateGravity();
        MarkNetworkUpdate();
    }
}

void RigidBody::SetKinematic(bool enable)
{
    if (enable != kinematic_)
    {
        kinematic_ = enable;
        AddBodyToWorld();
        MarkNetworkUpdate();
    }
}

void RigidBody::SetTrigger(bool enable)
{
    if (enable != trigger_)
    {
        trigger_ = enable;
        AddBodyToWorld();
        MarkNetworkUpdate();
    }
}

void RigidBody::SetCollisionLayer(unsigned layer)
{
    if (layer != collisionLayer_)
    {
        collisionLayer_ = layer;
        AddBodyToWorld();
        MarkNetworkUpdate();
    }
}

void RigidBody::SetCollisionMask(unsigned mask)
{
    if (mask != collisionMask_)
    {
        collisionMask_ = mask;
        AddBodyToWorld();
        MarkNetworkUpdate();
    }
}

void RigidBody::SetCollisionLayerAndMask(unsigned layer, unsigned mask)
{
    if (layer != collisionLayer_ || mask != collisionMask_)
    {
        collisionLayer_ = layer;
        collisionMask_ = mask;
        AddBodyToWorld();
        MarkNetworkUpdate();
    }
}

void RigidBody::SetCollisionEventMode(CollisionEventMode mode)
{
    collisionEventMode_ = mode;
    MarkNetworkUpdate();
}

void RigidBody::ApplyForce(const Vector3& force)
{
    if (private_data->body_ && force != Vector3::ZERO)
    {
        Activate();
        private_data->body_->applyCentralForce(ToBtVector3(force));
    }
}

void RigidBody::ApplyForce(const Vector3& force, const Vector3& position)
{
    if (private_data->body_ && force != Vector3::ZERO)
    {
        Activate();
        private_data->body_->applyForce(ToBtVector3(force), ToBtVector3(position - centerOfMass_));
    }
}

void RigidBody::ApplyTorque(const Vector3& torque)
{
    if (private_data->body_ && torque != Vector3::ZERO)
    {
        Activate();
        private_data->body_->applyTorque(ToBtVector3(torque));
    }
}

void RigidBody::ApplyImpulse(const Vector3& impulse)
{
    if (private_data->body_ && impulse != Vector3::ZERO)
    {
        Activate();
        private_data->body_->applyCentralImpulse(ToBtVector3(impulse));
    }
}

void RigidBody::ApplyImpulse(const Vector3& impulse, const Vector3& position)
{
    if (private_data->body_ && impulse != Vector3::ZERO)
    {
        Activate();
        private_data->body_->applyImpulse(ToBtVector3(impulse), ToBtVector3(position - centerOfMass_));
    }
}

void RigidBody::ApplyTorqueImpulse(const Vector3& torque)
{
    if (private_data->body_ && torque != Vector3::ZERO)
    {
        Activate();
        private_data->body_->applyTorqueImpulse(ToBtVector3(torque));
    }
}

void RigidBody::ResetForces()
{
    if (private_data->body_)
        private_data->body_->clearForces();
}

void RigidBody::Activate()
{
    if (private_data->body_ && mass_ > 0.0f)
        private_data->body_->activate(true);
}

void RigidBody::ReAddBodyToWorld()
{
    if (private_data->body_ && inWorld_)
        AddBodyToWorld();
}

void RigidBody::DisableMassUpdate()
{
    enableMassUpdate_ = false;
}

void RigidBody::EnableMassUpdate()
{
    if (!enableMassUpdate_)
    {
        enableMassUpdate_ = true;
        UpdateMass();
    }
}

btRigidBody *RigidBody::GetBody() const {
    return private_data->body_.get();
}

btCompoundShape *RigidBody::GetCompoundShape() const
{
    return private_data->compoundShape_.get();
}
Vector3 RigidBody::GetPosition() const
{
    if (private_data->body_)
    {
        const btTransform& transform = private_data->body_->getWorldTransform();
        return ToVector3(transform.getOrigin()) - ToQuaternion(transform.getRotation()) * centerOfMass_;
    }
    else
        return Vector3::ZERO;
}

Quaternion RigidBody::GetRotation() const
{
    return private_data->body_ ? ToQuaternion(private_data->body_->getWorldTransform().getRotation()) : Quaternion::IDENTITY;
}

Vector3 RigidBody::GetLinearVelocity() const
{
    return private_data->body_ ? ToVector3(private_data->body_->getLinearVelocity()) : Vector3::ZERO;
}

Vector3 RigidBody::GetLinearFactor() const
{
    return private_data->body_ ? ToVector3(private_data->body_->getLinearFactor()) : Vector3::ZERO;
}

Vector3 RigidBody::GetVelocityAtPoint(const Vector3& position) const
{
    return private_data->body_ ? ToVector3(private_data->body_->getVelocityInLocalPoint(ToBtVector3(position - centerOfMass_))) : Vector3::ZERO;
}

float RigidBody::GetLinearRestThreshold() const
{
    return private_data->body_ ? private_data->body_->getLinearSleepingThreshold() : 0.0f;
}

float RigidBody::GetLinearDamping() const
{
    return private_data->body_ ? private_data->body_->getLinearDamping() : 0.0f;
}

Vector3 RigidBody::GetAngularVelocity() const
{
    return private_data->body_ ? ToVector3(private_data->body_->getAngularVelocity()) : Vector3::ZERO;
}

Vector3 RigidBody::GetAngularFactor() const
{
    return private_data->body_ ? ToVector3(private_data->body_->getAngularFactor()) : Vector3::ZERO;
}

float RigidBody::GetAngularRestThreshold() const
{
    return private_data->body_ ? private_data->body_->getAngularSleepingThreshold() : 0.0f;
}

float RigidBody::GetAngularDamping() const
{
    return private_data->body_ ? private_data->body_->getAngularDamping() : 0.0f;
}

float RigidBody::GetFriction() const
{
    return private_data->body_ ? private_data->body_->getFriction() : 0.0f;
}

Vector3 RigidBody::GetAnisotropicFriction() const
{
    return private_data->body_ ? ToVector3(private_data->body_->getAnisotropicFriction()) : Vector3::ZERO;
}

float RigidBody::GetRollingFriction() const
{
    return private_data->body_ ? private_data->body_->getRollingFriction() : 0.0f;
}

float RigidBody::GetRestitution() const
{
    return private_data->body_ ? private_data->body_->getRestitution() : 0.0f;
}

float RigidBody::GetContactProcessingThreshold() const
{
    return private_data->body_ ? private_data->body_->getContactProcessingThreshold() : 0.0f;
}

float RigidBody::GetCcdRadius() const
{
    return private_data->body_ ? private_data->body_->getCcdSweptSphereRadius() : 0.0f;
}

float RigidBody::GetCcdMotionThreshold() const
{
    return private_data->body_ ? private_data->body_->getCcdMotionThreshold() : 0.0f;
}

bool RigidBody::IsActive() const
{
    return private_data->body_ ? private_data->body_->isActive() : false;
}

void RigidBody::GetCollidingBodies(std::unordered_set<RigidBody*>& result) const
{
    if (physicsWorld_)
        physicsWorld_->GetCollidingBodies(result, this);
    else
        result.clear();
}

void RigidBody::ApplyWorldTransform(const Vector3& newWorldPosition, const Quaternion& newWorldRotation)
{
    // In case of holding an extra reference to the RigidBody, this could be called in a situation
    // where node is already null
    if (!node_ || !physicsWorld_)
        return;

    physicsWorld_->SetApplyingTransforms(true);

    // Apply transform to the SmoothedTransform component instead of node transform if available
    if (smoothedTransform_)
    {
        smoothedTransform_->SetTargetWorldPosition(newWorldPosition);
        smoothedTransform_->SetTargetWorldRotation(newWorldRotation);
        private_data->lastPosition_ = newWorldPosition;
        private_data->lastRotation_ = newWorldRotation;
    }
    else
    {
        node_->SetWorldPosition(newWorldPosition);
        node_->SetWorldRotation(newWorldRotation);
        private_data->lastPosition_ = node_->GetWorldPosition();
        private_data->lastRotation_ = node_->GetWorldRotation();
    }

    physicsWorld_->SetApplyingTransforms(false);
}

void RigidBody::UpdateMass()
{
    if (!private_data->body_ || !enableMassUpdate_)
        return;

    btTransform principal;
    principal.setRotation(btQuaternion::getIdentity());
    principal.setOrigin(btVector3(0.0f, 0.0f, 0.0f));

    // Calculate center of mass shift from all the collision shapes
    unsigned numShapes = private_data->compoundShape_->getNumChildShapes();
    if (numShapes)
    {
        std::vector<float> masses(numShapes);
        for (unsigned i = 0; i < numShapes; ++i)
        {
            // The actual mass does not matter, divide evenly between child shapes
            masses[i] = 1.0f;
        }

        btVector3 inertia(0.0f, 0.0f, 0.0f);
        private_data->compoundShape_->calculatePrincipalAxisTransform(&masses[0], principal, inertia);
    }

    // Add child shapes to shifted compound shape with adjusted offset
    while (private_data->shiftedCompoundShape_->getNumChildShapes())
        private_data->shiftedCompoundShape_->removeChildShapeByIndex(private_data->shiftedCompoundShape_->getNumChildShapes() - 1);
    for (unsigned i = 0; i < numShapes; ++i)
    {
        btTransform adjusted = private_data->compoundShape_->getChildTransform(i);
        adjusted.setOrigin(adjusted.getOrigin() - principal.getOrigin());
        private_data->shiftedCompoundShape_->addChildShape(adjusted, private_data->compoundShape_->getChildShape(i));
    }

    // If shifted compound shape has only one child with no offset/rotation, use the child shape
    // directly as the rigid body collision shape for better collision detection performance
    bool useCompound = !numShapes || numShapes > 1;
    if (!useCompound)
    {
        const btTransform& childTransform = private_data->shiftedCompoundShape_->getChildTransform(0);
        if (!ToVector3(childTransform.getOrigin()).Equals(Vector3::ZERO) ||
                !ToQuaternion(childTransform.getRotation()).Equals(Quaternion::IDENTITY))
            useCompound = true;
    }
    btCollisionShape* oldCollisionShape = private_data->body_->getCollisionShape();
    private_data->body_->setCollisionShape(useCompound ? private_data->shiftedCompoundShape_.get() : private_data->shiftedCompoundShape_->getChildShape(0));

    // If we have one shape and this is a triangle mesh, we use a custom material callback in order to adjust internal edges
    if (!useCompound && private_data->body_->getCollisionShape()->getShapeType() == SCALED_TRIANGLE_MESH_SHAPE_PROXYTYPE &&
            physicsWorld_->GetInternalEdge())
        private_data->body_->setCollisionFlags(private_data->body_->getCollisionFlags() | btCollisionObject::CF_CUSTOM_MATERIAL_CALLBACK);
    else
        private_data->body_->setCollisionFlags(private_data->body_->getCollisionFlags() & ~btCollisionObject::CF_CUSTOM_MATERIAL_CALLBACK);

    // Reapply rigid body position with new center of mass shift
    Vector3 oldPosition = GetPosition();
    centerOfMass_ = ToVector3(principal.getOrigin());
    SetPosition(oldPosition);

    // Calculate final inertia
    btVector3 localInertia(0.0f, 0.0f, 0.0f);
    if (mass_ > 0.0f)
        private_data->shiftedCompoundShape_->calculateLocalInertia(mass_, localInertia);
    private_data->body_->setMassProps(mass_, localInertia);
    private_data->body_->updateInertiaTensor();

    // Reapply constraint positions for new center of mass shift
    if (node_)
    {
        for (Constraint* elem : constraints_)
            elem->ApplyFrames();
    }
    // Readd body to world to reset Bullet collision cache if collision shape was changed (issue #2064)
    if (inWorld_ && private_data->body_->getCollisionShape() != oldCollisionShape && physicsWorld_)
    {
        btDiscreteDynamicsWorld* world = physicsWorld_->GetWorld();
        world->removeRigidBody(private_data->body_.get());
        world->addRigidBody(private_data->body_.get(), (short)collisionLayer_, (short)collisionMask_);
    }
}

void RigidBody::UpdateGravity()
{
    if (physicsWorld_ && private_data->body_)
    {
        btDiscreteDynamicsWorld* world = physicsWorld_->GetWorld();

        int flags = private_data->body_->getFlags();
        if (useGravity_ && gravityOverride_ == Vector3::ZERO)
            flags &= ~BT_DISABLE_WORLD_GRAVITY;
        else
            flags |= BT_DISABLE_WORLD_GRAVITY;
        private_data->body_->setFlags(flags);

        if (useGravity_)
        {
            // If override vector is zero, use world's gravity
            if (gravityOverride_ == Vector3::ZERO)
                private_data->body_->setGravity(world->getGravity());
            else
                private_data->body_->setGravity(ToBtVector3(gravityOverride_));
        }
        else
            private_data->body_->setGravity(btVector3(0.0f, 0.0f, 0.0f));
    }
}

void RigidBody::SetNetAngularVelocityAttr(const std::vector<unsigned char>& value)
{
    float maxVelocity = physicsWorld_ ? physicsWorld_->GetMaxNetworkAngularVelocity() : DEFAULT_MAX_NETWORK_ANGULAR_VELOCITY;
    MemoryBuffer buf(value);
    SetAngularVelocity(buf.ReadPackedVector3(maxVelocity));
}

const std::vector<unsigned char>& RigidBody::GetNetAngularVelocityAttr() const
{
    float maxVelocity = physicsWorld_ ? physicsWorld_->GetMaxNetworkAngularVelocity() : DEFAULT_MAX_NETWORK_ANGULAR_VELOCITY;
    attrBuffer_.clear();
    attrBuffer_.WritePackedVector3(GetAngularVelocity(), maxVelocity);
    return attrBuffer_.GetBuffer();
}

void RigidBody::AddConstraint(Constraint* constraint)
{
    constraints_.insert(constraint);
}

void RigidBody::RemoveConstraint(Constraint* constraint)
{
    constraints_.erase(constraint);
    // A constraint being removed should possibly cause the object to eg. start falling, so activate
    Activate();
}

void RigidBody::ReleaseBody()
{
    if (private_data->body_)
    {
        // Release all constraints which refer to this body
        // Make a copy for iteration
        std::unordered_set<Constraint*> constraints(constraints_);
        for (Constraint* constraint : constraints)
            constraint->ReleaseConstraint();

        RemoveBodyFromWorld();

        private_data->body_.reset();
    }
}

void RigidBody::OnMarkedDirty(Node* node)
{
    // If node transform changes, apply it back to the physics transform. However, do not do this when a SmoothedTransform
    // is in use, because in that case the node transform will be constantly updated into smoothed, possibly non-physical
    // states; rather follow the SmoothedTransform target transform directly
    // Also, for kinematic objects Bullet asks the position from us, so we do not need to apply ourselves
    // (exception: initial setting of transform)
    if ((!kinematic_ || !private_data->hasSimulated_) && (!physicsWorld_ || !physicsWorld_->IsApplyingTransforms()) && !smoothedTransform_)
    {
        // Physics operations are not safe from worker threads
        Scene* scene = GetScene();
        if (scene && scene->IsThreadedUpdate())
        {
            scene->DelayedMarkedDirty(this);
            return;
        }

        // Check if transform has changed from the last one set in ApplyWorldTransform()
        Vector3 newPosition = node_->GetWorldPosition();
        Quaternion newRotation = node_->GetWorldRotation();

        if (!newRotation.Equals(private_data->lastRotation_))
        {
            private_data->lastRotation_ = newRotation;
            SetRotation(newRotation);
        }
        if (!newPosition.Equals(private_data->lastPosition_))
        {
            private_data->lastPosition_ = newPosition;
            SetPosition(newPosition);
        }
    }
}

void RigidBody::OnNodeSet(Node* node)
{
    if (node)
        node->AddListener(this);
}

void RigidBody::OnSceneSet(Scene* scene)
{
    if (scene)
    {
        if (scene == node_)
            URHO3D_LOGWARNING(GetTypeName() + " should not be created to the root scene node");

        physicsWorld_ = scene->GetOrCreateComponent<PhysicsWorld>();
        physicsWorld_->AddRigidBody(this);

        AddBodyToWorld();
    }
    else
    {
        ReleaseBody();

        if (physicsWorld_)
            physicsWorld_->RemoveRigidBody(this);
    }
}

void RigidBody::AddBodyToWorld()
{
    if (!physicsWorld_)
        return;

    URHO3D_PROFILE(AddBodyToWorld);

    if (mass_ < 0.0f)
        mass_ = 0.0f;

    if (private_data->body_)
        RemoveBodyFromWorld();
    else
    {
        // Correct inertia will be calculated below
        btVector3 localInertia(0.0f, 0.0f, 0.0f);
        private_data->body_.reset(new btRigidBody(mass_, private_data, private_data->shiftedCompoundShape_.get(), localInertia));
        private_data->body_->setUserPointer(this);

        // Check for existence of the SmoothedTransform component, which should be created by now in network client mode.
        // If it exists, subscribe to its change events
        smoothedTransform_ = GetComponent<SmoothedTransform>();
        if (smoothedTransform_)
        {
            smoothedTransform_->targetPositionChanged.Connect(this,&RigidBody::HandleTargetPosition);
            smoothedTransform_->targetRotationChanged.Connect(this,&RigidBody::HandleTargetRotation);
        }

        // Check if CollisionShapes already exist in the node and add them to the compound shape.
        // Do not update mass yet, but do it once all shapes have been added
        std::vector<CollisionShape*> shapes;
        node_->GetComponents<CollisionShape>(shapes);
        for (CollisionShape * shape : shapes)
            shape->NotifyRigidBody(false);

        // Check if this node contains Constraint components that were waiting for the rigid body to be created, and signal them
        // to create themselves now
        std::vector<Constraint*> constraints;
        node_->GetComponents<Constraint>(constraints);
        for (Constraint * constraint : constraints)
            constraint->CreateConstraint();
    }

    UpdateMass();
    UpdateGravity();

    int flags = private_data->body_->getCollisionFlags();
    if (trigger_)
        flags |= btCollisionObject::CF_NO_CONTACT_RESPONSE;
    else
        flags &= ~btCollisionObject::CF_NO_CONTACT_RESPONSE;
    if (kinematic_)
        flags |= btCollisionObject::CF_KINEMATIC_OBJECT;
    else
        flags &= ~btCollisionObject::CF_KINEMATIC_OBJECT;
    private_data->body_->setCollisionFlags(flags);
    private_data->body_->forceActivationState(kinematic_ ? DISABLE_DEACTIVATION : ISLAND_SLEEPING);

    if (!IsEnabledEffective())
        return;

    btDiscreteDynamicsWorld* world = physicsWorld_->GetWorld();
    world->addRigidBody(private_data->body_.get(), collisionLayer_, collisionMask_);
    inWorld_ = true;
    readdBody_ = false;
    private_data->hasSimulated_ = false;

    if (mass_ > 0.0f)
        Activate();
    else
    {
        SetLinearVelocity(Vector3::ZERO);
        SetAngularVelocity(Vector3::ZERO);
    }
}

void RigidBody::RemoveBodyFromWorld()
{
    if (physicsWorld_ && private_data->body_ && inWorld_)
    {
        btDiscreteDynamicsWorld* world = physicsWorld_->GetWorld();
        world->removeRigidBody(private_data->body_.get());
        inWorld_ = false;
    }
}

void RigidBody::HandleTargetPosition()
{
    // Copy the smoothing target position to the rigid body
    if (!physicsWorld_ || !physicsWorld_->IsApplyingTransforms())
        SetPosition(static_cast<SmoothedTransform*>(GetEventSender())->GetTargetWorldPosition());
}

void RigidBody::HandleTargetRotation()
{
    // Copy the smoothing target rotation to the rigid body
    if (!physicsWorld_ || !physicsWorld_->IsApplyingTransforms())
        SetRotation(static_cast<SmoothedTransform*>(GetEventSender())->GetTargetWorldRotation());
}

}
