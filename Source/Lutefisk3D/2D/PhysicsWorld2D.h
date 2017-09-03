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

#pragma once

#include "Lutefisk3D/Scene/Component.h"
#include "Lutefisk3D/IO/VectorBuffer.h"
#include "Lutefisk3D/2D/PhysicsEvents2D.h"

#include <unordered_set>

class b2World;
namespace Urho3D
{

class Camera;
class CollisionShape2D;
class RigidBody2D;

/// 2D Physics raycast hit.
struct LUTEFISK3D_EXPORT PhysicsRaycastResult2D
{
    /// Test for inequality, added to prevent GCC from complaining.
    bool operator !=(const PhysicsRaycastResult2D& rhs) const
    {
        return position_ != rhs.position_ || normal_ != rhs.normal_ || distance_ != rhs.distance_ || body_ != rhs.body_;
    }

    /// Hit worldspace position.
    Vector2 position_;
    /// Hit worldspace normal.
    Vector2 normal_;
    /// Hit distance from ray origin.
    float distance_;
    /// Rigid body that was hit.
    RigidBody2D* body_ = nullptr;
};

/// Delayed world transform assignment for parented 2D rigidbodies.
struct DelayedWorldTransform2D
{
    /// Rigid body.
    RigidBody2D* rigidBody_;
    /// Parent rigid body.
    RigidBody2D* parentRigidBody_;
    /// New world position.
    Vector3 worldPosition_;
    /// New world rotation.
    Quaternion worldRotation_;
};

/// 2D physics simulation world component. Should be added only to the root scene node.
class LUTEFISK3D_EXPORT PhysicsWorld2D : public Component, public Physics2DWorldSignals, public PhysicsSignals
{
    URHO3D_OBJECT(PhysicsWorld2D,Component)

public:
    /// Construct.
    PhysicsWorld2D(Context* scontext);
    /// Destruct.
    virtual ~PhysicsWorld2D();
    /// Register object factory.
    static void RegisterObject(Context* context);

    /// Visualize the component as debug geometry.
    void DrawDebugGeometry(DebugRenderer* debug, bool depthTest) override;

    /// Step the simulation forward.
    void Update(float timeStep);
    /// Add debug geometry to the debug renderer.
    void DrawDebugGeometry();
    /// Enable or disable automatic physics simulation during scene update. Enabled by default.
    void SetUpdateEnabled(bool enable);
    /// Set draw shape.
    void SetDrawShape(bool drawShape);
    /// Set draw joint.
    void SetDrawJoint(bool drawJoint);
    /// Set draw aabb.
    void SetDrawAabb(bool drawAabb);
    /// Set draw pair.
    void SetDrawPair(bool drawPair);
    /// Set draw center of mass.
    void SetDrawCenterOfMass(bool drawCenterOfMass);
    /// Set allow sleeping.
    void SetAllowSleeping(bool enable);
    /// Set warm starting.
    void SetWarmStarting(bool enable);
    /// Set continuous physics.
    void SetContinuousPhysics(bool enable);
    /// Set sub stepping.
    void SetSubStepping(bool enable);
    /// Set gravity.
    void SetGravity(const Vector2& gravity);
    /// Set auto clear forces.
    void SetAutoClearForces(bool enable);
    /// Set velocity iterations.
    void SetVelocityIterations(int velocityIterations);
    /// Set position iterations.
    void SetPositionIterations(int positionIterations);
    /// Add rigid body.
    void AddRigidBody(RigidBody2D* rigidBody);
    /// Remove rigid body.
    void RemoveRigidBody(RigidBody2D* rigidBody);
    /// Add a delayed world transform assignment. Called by RigidBody2D.
    void AddDelayedWorldTransform(const DelayedWorldTransform2D& transform);

    /// Perform a physics world raycast and return all hits.
    void Raycast(std::vector<PhysicsRaycastResult2D>& results, const Vector2& startPoint, const Vector2& endPoint, unsigned collisionMask = M_MAX_UNSIGNED);
    /// Perform a physics world raycast and return the closest hit.
    void RaycastSingle(PhysicsRaycastResult2D& result, const Vector2& startPoint, const Vector2& endPoint, unsigned collisionMask = M_MAX_UNSIGNED);
    /// Return rigid body at point.
    RigidBody2D* GetRigidBody(const Vector2& point, unsigned collisionMask = M_MAX_UNSIGNED);
    /// Return rigid body at screen point.
    RigidBody2D* GetRigidBody(int screenX, int screenY, unsigned collisionMask = M_MAX_UNSIGNED);
    /// Return rigid bodies by a box query.
    void GetRigidBodies(std::vector<RigidBody2D*>& result, const Rect& aabb, unsigned collisionMask = M_MAX_UNSIGNED);

    /// Return whether physics world will automatically simulate during scene update.
    bool IsUpdateEnabled() const { return updateEnabled_; }

    /// Return draw shape.
    bool GetDrawShape() const;
    /// Return draw joint.
    bool GetDrawJoint() const;
    /// Return draw aabb.
    bool GetDrawAabb() const;
    /// Return draw pair.
    bool GetDrawPair() const;
    /// Return draw center of mass.
    bool GetDrawCenterOfMass() const;
    /// Return allow sleeping.
    bool GetAllowSleeping() const;
    /// Return warm starting.
    bool GetWarmStarting() const;
    /// Return continuous physics.
    bool GetContinuousPhysics() const;
    /// Return sub stepping.
    bool GetSubStepping() const;
    /// Return auto clear forces.
    bool GetAutoClearForces() const;
    /// Return gravity.
    const Vector2& GetGravity() const { return gravity_; }
    /// Return velocity iterations.
    int GetVelocityIterations() const { return velocityIterations_; }
    /// Return position iterations.
    int GetPositionIterations() const { return positionIterations_; }

    /// Return the Box2D physics world.
    b2World* GetWorld() { return world_.get(); }
    /// Set node dirtying to be disregarded.
    void SetApplyingTransforms(bool enable) { applyingTransforms_ = enable; }
    /// Return whether node dirtying should be disregarded.
    bool IsApplyingTransforms() const { return applyingTransforms_; }

protected:
    /// Handle scene being assigned.
    void OnSceneSet(Scene *scene) override;

    /// Handle the scene subsystem update event, step simulation here.
    void HandleSceneSubsystemUpdate(Scene *, float ts);

    /// Box2D physics world.
    std::unique_ptr<b2World> world_;
    /// Gravity.
    Vector2 gravity_;
    /// Velocity iterations.
    int velocityIterations_;
    /// Position iterations.
    int positionIterations_;

    /// Extra weak pointer to scene to allow for cleanup in case the world is destroyed before other components.
    WeakPtr<Scene> scene_;

    /// Automatic simulation update enabled flag.
    bool updateEnabled_;
    /// Applying transforms.
    bool applyingTransforms_;
    /// Rigid bodies.
    std::unordered_set< WeakPtr<RigidBody2D> > rigidBodies_;
    /// Delayed (parented) world transform assignments.
    HashMap<RigidBody2D*, DelayedWorldTransform2D> delayedWorldTransforms_;
private:
    // Data hiding the Box2D specific things.
    void *privateData_ = nullptr;
};

}
