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

#include "Lutefisk3D/Physics/PhysicsEvents.h"
#include "Lutefisk3D/Scene/Component.h"
#include "Lutefisk3D/Math/BoundingBox.h"
#include "Lutefisk3D/Math/Sphere.h"
#include "Lutefisk3D/Math/Vector3.h"
#include "Lutefisk3D/IO/VectorBuffer.h"
#include "Lutefisk3D/Container/HashMap.h"

#include <QtCore/QSet>

class btCollisionConfiguration;
class btCollisionShape;
class btBroadphaseInterface;
class btConstraintSolver;
class btDiscreteDynamicsWorld;
class btDispatcher;
class btDynamicsWorld;
class btPersistentManifold;

namespace Urho3D
{

class CollisionShape;
class Deserializer;
class Constraint;
class Model;
class Node;
class Ray;
class RigidBody;
class Scene;
class Serializer;
class XMLElement;

struct CollisionGeometryData;

/// Physics raycast hit.
struct LUTEFISK3D_EXPORT PhysicsRaycastResult
{
    /// Test for inequality, added to prevent GCC from complaining.
    bool operator !=(const PhysicsRaycastResult& rhs) const
    {
        return position_ != rhs.position_ || normal_ != rhs.normal_ || distance_ != rhs.distance_ || body_ != rhs.body_;
    }

    /// Hit worldspace position.
    Vector3 position_;
    /// Hit worldspace normal.
    Vector3 normal_;
    /// Hit distance from ray origin.
    float distance_;
    /// Hit fraction.
    float hitFraction_;
    /// Rigid body that was hit.
    RigidBody* body_ = nullptr;
};

/// Delayed world transform assignment for parented rigidbodies.
struct DelayedWorldTransform
{
    /// Rigid body.
    RigidBody* rigidBody_;
    /// Parent rigid body.
    RigidBody* parentRigidBody_;
    /// New world position.
    Vector3 worldPosition_;
    /// New world rotation.
    Quaternion worldRotation_;
};

/// Manifold pointers stored during collision processing.
struct ManifoldPair
{
    /// Manifold without the body pointers flipped.
    btPersistentManifold* manifold_=nullptr;
    /// Manifold with the body pointers flipped.
    btPersistentManifold* flippedManifold_=nullptr;
};

/// Custom overrides of physics internals. To use overrides, must be set before the physics component is created.
struct PhysicsWorldConfig
{
    /// Override for the collision configuration (default btDefaultCollisionConfiguration).
    btCollisionConfiguration* collisionConfig_=nullptr;
};

static const float DEFAULT_MAX_NETWORK_ANGULAR_VELOCITY = 100.0f;

/// Physics simulation world component. Should be added only to the root scene node.
class LUTEFISK3D_EXPORT PhysicsWorld : public Component, public PhysicsSignals
{
    URHO3D_OBJECT(PhysicsWorld,Component)
    friend struct PhysicsWorldPrivate;

public:
    /// Construct.
    PhysicsWorld(Context* scontext);
    /// Destruct.
    ~PhysicsWorld() override;
    /// Register object factory.
    static void RegisterObject(Context* context);

    /// Visualize the component as debug geometry.
    virtual void DrawDebugGeometry(DebugRenderer* debug, bool depthTest) override;

    /// Step the simulation forward.
    void Update(float timeStep);
    /// Refresh collisions only without updating dynamics.
    void UpdateCollisions();
    /// Set simulation substeps per second.
    void SetFps(int fps);
    /// Set gravity.
    void SetGravity(const Vector3& gravity);
    /// Set maximum number of physics substeps per frame. 0 (default) is unlimited. Positive values cap the amount. Use a negative value to enable an adaptive timestep. This may cause inconsistent physics behavior.
    void SetMaxSubSteps(int num);
    /// Set number of constraint solver iterations.
    void SetNumIterations(int num);
    /// Enable or disable automatic physics simulation during scene update. Enabled by default.
    void SetUpdateEnabled(bool enable);
    /// Set whether to interpolate between simulation steps.
    void SetInterpolation(bool enable);
    /// Set whether to use Bullet's internal edge utility for trimesh collisions. Disabled by default.
    void SetInternalEdge(bool enable);
    /// Set split impulse collision mode. This is more accurate, but slower. Disabled by default.
    void SetSplitImpulse(bool enable);
    /// Set maximum angular velocity for network replication.
    void SetMaxNetworkAngularVelocity(float velocity);
    /// Perform a physics world raycast and return all hits.
    void Raycast(std::vector<PhysicsRaycastResult>& result, const Ray& ray, float maxDistance, unsigned collisionMask = M_MAX_UNSIGNED);
    /// Perform a physics world raycast and return the closest hit.
    void RaycastSingle(PhysicsRaycastResult& result, const Ray& ray, float maxDistance, unsigned collisionMask = M_MAX_UNSIGNED);
    /// Perform a physics world segmented raycast and return the closest hit. Useful for big scenes with many bodies.
    void RaycastSingleSegmented(PhysicsRaycastResult& result, const Ray& ray, float maxDistance, float segmentDistance, unsigned collisionMask = M_MAX_UNSIGNED);
    /// Perform a physics world swept sphere test and return the closest hit.
    void SphereCast(PhysicsRaycastResult& result, const Ray& ray, float radius, float maxDistance, unsigned collisionMask = M_MAX_UNSIGNED);
    /// Perform a physics world swept convex test using a user-supplied collision shape and return the first hit.
    void ConvexCast(PhysicsRaycastResult& result, CollisionShape* shape, const Vector3& startPos, const Quaternion& startRot, const Vector3& endPos, const Quaternion& endRot, unsigned collisionMask = M_MAX_UNSIGNED);
    /// Perform a physics world swept convex test using a user-supplied Bullet collision shape and return the first hit.
    void ConvexCast(PhysicsRaycastResult& result, btCollisionShape* shape, const Vector3& startPos, const Quaternion& startRot, const Vector3& endPos, const Quaternion& endRot, unsigned collisionMask = M_MAX_UNSIGNED);
    /// Invalidate cached collision geometry for a model.
    void RemoveCachedGeometry(Model* model);
    /// Return rigid bodies by a sphere query.
    void GetRigidBodies(std::unordered_set<RigidBody*>& result, const Sphere& sphere, unsigned collisionMask = M_MAX_UNSIGNED);
    /// Return rigid bodies by a box query.
    void GetRigidBodies(std::unordered_set<RigidBody *> &result, const BoundingBox& box, unsigned collisionMask = M_MAX_UNSIGNED);
    /// Return rigid bodies by contact test with the specified body. It needs to be active to return all contacts reliably.
    void GetRigidBodies(std::unordered_set<RigidBody*>& result, const RigidBody* body);
    /// Return rigid bodies that have been in collision with the specified body on the last simulation step. Only returns collisions that were sent as events (depends on collision event mode) and excludes e.g. static-static collisions.
    void GetCollidingBodies(std::unordered_set<RigidBody*>& result, const RigidBody* body);

    /// Return gravity.
    Vector3 GetGravity() const;
    /// Return maximum number of physics substeps per frame.
    int GetMaxSubSteps() const { return maxSubSteps_; }
    /// Return number of constraint solver iterations.
    int GetNumIterations() const;
    /// Return whether physics world will automatically simulate during scene update.
    bool IsUpdateEnabled() const { return updateEnabled_; }
    /// Return whether interpolation between simulation steps is enabled.
    bool GetInterpolation() const { return interpolation_; }
    /// Return whether Bullet's internal edge utility for trimesh collisions is enabled.
    bool GetInternalEdge() const { return internalEdge_; }
    /// Return whether split impulse collision mode is enabled.
    bool GetSplitImpulse() const;
    /// Return simulation steps per second.
    int GetFps() const { return fps_; }
    /// Return maximum angular velocity for network replication.
    float GetMaxNetworkAngularVelocity() const { return maxNetworkAngularVelocity_; }

    /// Add a rigid body to keep track of. Called by RigidBody.
    void AddRigidBody(RigidBody* body);
    /// Remove a rigid body. Called by RigidBody.
    void RemoveRigidBody(RigidBody* body);
    /// Add a collision shape to keep track of. Called by CollisionShape.
    void AddCollisionShape(CollisionShape* shape);
    /// Remove a collision shape. Called by CollisionShape.
    void RemoveCollisionShape(CollisionShape* shape);
    /// Add a constraint to keep track of. Called by Constraint.
    void AddConstraint(Constraint* joint);
    /// Remove a constraint. Called by Constraint.
    void RemoveConstraint(Constraint* joint);
    /// Add a delayed world transform assignment. Called by RigidBody.
    void AddDelayedWorldTransform(const DelayedWorldTransform& transform);
    /// Add debug geometry to the debug renderer.
    void DrawDebugGeometry(bool depthTest);
    /// Set debug renderer to use. Called both by PhysicsWorld itself and physics components.
    void SetDebugRenderer(DebugRenderer* debug);
    /// Set debug geometry depth test mode. Called both by PhysicsWorld itself and physics components.
    void SetDebugDepthTest(bool enable);

    /// Return the Bullet physics world.
    btDiscreteDynamicsWorld* GetWorld();
    /// Clean up the geometry cache.
    void CleanupGeometryCache();
    /// Return trimesh collision geometry cache.
    HashMap<std::pair<Model*, unsigned>, SharedPtr<CollisionGeometryData> >& GetTriMeshCache() { return triMeshCache_; }
    /// Return convex collision geometry cache.
    HashMap<std::pair<Model*, unsigned>, SharedPtr<CollisionGeometryData> >& GetConvexCache() { return convexCache_; }
    /// Set node dirtying to be disregarded.
    void SetApplyingTransforms(bool enable) { applyingTransforms_ = enable; }
    /// Return whether node dirtying should be disregarded.
    bool IsApplyingTransforms() const { return applyingTransforms_; }

    /// Return whether is currently inside the Bullet substep loop.
    bool IsSimulating() const { return simulating_; }
    /// Overrides of the internal configuration.
    static struct PhysicsWorldConfig config;
protected:
    /// Handle scene being assigned.
    virtual void OnSceneSet(Scene * scene) override;

private:
    /// Handle the scene subsystem update event, step simulation here.
    void HandleSceneSubsystemUpdate(Scene *, float ts);
    /// Trigger update before each physics simulation step.
    void PreStep(float timeStep);
    /// Trigger update after each physics simulation step.
    void PostStep(float timeStep);
    /// Send accumulated collision events.
    void SendCollisionEvents();

    /// Extra weak pointer to scene to allow for cleanup in case the world is destroyed before other components.
    WeakPtr<Scene> scene_;
    /// Rigid bodies in the world.
    std::unordered_set<RigidBody*> rigidBodies_;
    /// Collision shapes in the world.
    std::unordered_set<CollisionShape*> collisionShapes_;
    /// Constraints in the world.
    std::vector<Constraint*> constraints_;
    /// Collision pairs on this frame.
    HashMap<std::pair<WeakPtr<RigidBody>, WeakPtr<RigidBody> >, ManifoldPair> currentCollisions_;
    /// Collision pairs on the previous frame. Used to check if a collision is "new." Manifolds are not guaranteed to exist anymore.
    HashMap<std::pair<WeakPtr<RigidBody>, WeakPtr<RigidBody> >, ManifoldPair> previousCollisions_;
    /// Delayed (parented) world transform assignments.
    HashMap<RigidBody*, DelayedWorldTransform> delayedWorldTransforms_;
    /// Cache for trimesh geometry data by model and LOD level.
    HashMap<std::pair<Model*, unsigned>, SharedPtr<CollisionGeometryData> > triMeshCache_;
    /// Cache for convex geometry data by model and LOD level.
    HashMap<std::pair<Model*, unsigned>, SharedPtr<CollisionGeometryData> > convexCache_;
    /// Preallocated buffer for physics collision contact data.
    VectorBuffer contacts_;
    /// Simulation substeps per second.
    unsigned fps_;
    /// Maximum number of simulation substeps per frame. 0 (default) unlimited, or negative values for adaptive timestep.
    int maxSubSteps_;
    /// Time accumulator for non-interpolated mode.
    float timeAcc_;
    /// Maximum angular velocity for network replication.
    float maxNetworkAngularVelocity_;
    /// Automatic simulation update enabled flag.
    bool updateEnabled_;
    /// Interpolation flag.
    bool interpolation_;
    /// Use internal edge utility flag.
    bool internalEdge_;
    /// Applying transforms flag.
    bool applyingTransforms_;
    /// Simulating flag.
    bool simulating_;
    struct PhysicsWorldPrivate *private_data = nullptr;
};

/// Register Physics library objects.
void LUTEFISK3D_EXPORT RegisterPhysicsLibrary(Context* context);

}
