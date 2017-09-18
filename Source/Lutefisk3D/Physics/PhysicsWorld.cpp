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

#include "PhysicsWorld.h"

#include "CollisionShape.h"
#include "Constraint.h"
#include "Lutefisk3D/Core/Context.h"
#include "Lutefisk3D/Graphics/DebugRenderer.h"
#include "Lutefisk3D/IO/Log.h"
#include "Lutefisk3D/Graphics/Model.h"
#include "Lutefisk3D/Core/Mutex.h"
#include "PhysicsEvents.h"
#include "PhysicsUtils.h"
#include "Lutefisk3D/Core/Profiler.h"
#include "Lutefisk3D/Math/Ray.h"
#include "RigidBody.h"
#include "Lutefisk3D/Scene/Scene.h"
#include "Lutefisk3D/Scene/SceneEvents.h"

#include <Bullet/BulletCollision/BroadphaseCollision/btDbvtBroadphase.h>
#include <Bullet/BulletCollision/BroadphaseCollision/btBroadphaseProxy.h>
#include <Bullet/BulletCollision/CollisionDispatch/btDefaultCollisionConfiguration.h>
#include <Bullet/BulletCollision/CollisionDispatch/btInternalEdgeUtility.h>
#include <Bullet/BulletCollision/CollisionShapes/btBoxShape.h>
#include <Bullet/BulletCollision/CollisionShapes/btSphereShape.h>
#include <Bullet/BulletDynamics/ConstraintSolver/btSequentialImpulseConstraintSolver.h>
#include <Bullet/BulletDynamics/Dynamics/btDiscreteDynamicsWorld.h>
#include <Bullet/LinearMath/btIDebugDraw.h>

#include <unordered_set>

extern ContactAddedCallback gContactAddedCallback;

namespace Urho3D
{

extern const char* SUBSYSTEM_CATEGORY;
const char* PHYSICS_CATEGORY = "Physics";

struct PhysicsWorldPrivate : public btIDebugDraw {
    PhysicsWorldPrivate(PhysicsWorld *o);
    ~PhysicsWorldPrivate();
    friend void InternalPreTickCallback(btDynamicsWorld *world, btScalar timeStep);
    friend void InternalTickCallback(btDynamicsWorld *world, btScalar timeStep);
    /// Check if an AABB is visible for debug drawing.
    virtual bool isVisible(const btVector3& aabbMin, const btVector3& aabbMax) override;
    /// Draw a physics debug line.
    virtual void drawLine(const btVector3& from, const btVector3& to, const btVector3& color) override;
    /// Log warning from the physics engine.
    virtual void reportErrorWarning(const char* warningString) override;
    /// Draw a physics debug contact point. Not implemented.
    virtual void drawContactPoint(const btVector3& pointOnB, const btVector3& normalOnB, btScalar distance, int lifeTime, const btVector3& color) override;
    /// Draw physics debug 3D text. Not implemented.
    virtual void draw3dText(const btVector3& location,const char* textString) override;
    /// Set debug draw flags.
    virtual void setDebugMode(int debugMode) override { debugMode_ = debugMode; }
    /// Return debug draw flags.
    virtual int getDebugMode() const override { return debugMode_; }
    void PreStep(float ts) { owner->PreStep(ts); }
    void PostStep(float ts) { owner->PostStep(ts); }

    PhysicsWorld *owner;
    /// Debug draw flags.
    int debugMode_ = btIDebugDraw::DBG_DrawWireframe | btIDebugDraw::DBG_DrawConstraints | btIDebugDraw::DBG_DrawConstraintLimits;
    /// Bullet collision configuration.
    btCollisionConfiguration* collisionConfiguration_ = nullptr;
    /// Bullet collision dispatcher.
    std::unique_ptr<btDispatcher> collisionDispatcher_;
    /// Bullet collision broadphase.
    std::unique_ptr<btBroadphaseInterface> broadphase_;
    /// Bullet constraint solver.
    std::unique_ptr<btConstraintSolver> solver_;
    /// Bullet physics world.
    std::unique_ptr<btDiscreteDynamicsWorld> world_;
    /// Debug renderer.
    DebugRenderer* debugRenderer_ = nullptr;
    /// Debug draw depth test mode.
    bool debugDepthTest_;

};
#define L_D(Classname) Classname##Private *d = (Classname##Private *)private_data

PhysicsWorldConfig PhysicsWorld::config;
namespace {
const int MAX_SOLVER_ITERATIONS = 256;
const int DEFAULT_FPS = 60;
const Vector3 DEFAULT_GRAVITY = Vector3(0.0f, -9.81f, 0.0f);

bool CompareRaycastResults(const PhysicsRaycastResult& lhs, const PhysicsRaycastResult& rhs)
{
    return lhs.distance_ < rhs.distance_;
}

void InternalPreTickCallback(btDynamicsWorld *world, btScalar timeStep)
{
    static_cast<PhysicsWorldPrivate*>(world->getWorldUserInfo())->PreStep(timeStep);
}

void InternalTickCallback(btDynamicsWorld *world, btScalar timeStep)
{
    static_cast<PhysicsWorldPrivate*>(world->getWorldUserInfo())->PostStep(timeStep);
}

bool CustomMaterialCombinerCallback(btManifoldPoint& cp, const btCollisionObjectWrapper* colObj0Wrap, int partId0, int index0, const btCollisionObjectWrapper* colObj1Wrap, int partId1, int index1)
{
    btAdjustInternalEdgeContacts(cp, colObj1Wrap, colObj0Wrap, partId1, index1);

    cp.m_combinedFriction = colObj0Wrap->getCollisionObject()->getFriction() * colObj1Wrap->getCollisionObject()->getFriction();
    cp.m_combinedRestitution = colObj0Wrap->getCollisionObject()->getRestitution() * colObj1Wrap->getCollisionObject()->getRestitution();

    return true;
}

/// Callback for physics world queries.
struct PhysicsQueryCallback : public btCollisionWorld::ContactResultCallback
{
    /// Construct.
    PhysicsQueryCallback(std::unordered_set<RigidBody*>& result, unsigned collisionMask) : result_(result),
        collisionMask_(collisionMask)
    {
    }

    /// Add a contact result.
    virtual btScalar addSingleResult(btManifoldPoint &, const btCollisionObjectWrapper *colObj0Wrap, int, int, const btCollisionObjectWrapper *colObj1Wrap, int, int) override
    {
        RigidBody* body = reinterpret_cast<RigidBody*>(colObj0Wrap->getCollisionObject()->getUserPointer());
        if (body && (body->GetCollisionLayer() & collisionMask_) )
            result_.insert(body);
        body = reinterpret_cast<RigidBody*>(colObj1Wrap->getCollisionObject()->getUserPointer());
        if (body && (body->GetCollisionLayer() & collisionMask_))
            result_.insert(body);
        return 0.0f;
    }

    /// Found rigid bodies.
    std::unordered_set<RigidBody*>& result_;
    /// Collision mask for the query.
    unsigned collisionMask_;
};
}

PhysicsWorld::PhysicsWorld(Context* context) :
    Component(context),
    fps_(DEFAULT_FPS),
    maxSubSteps_(0),
    timeAcc_(0.0f),
    maxNetworkAngularVelocity_(DEFAULT_MAX_NETWORK_ANGULAR_VELOCITY),
    updateEnabled_(true),
    interpolation_(true),
    internalEdge_(true),
    applyingTransforms_(false),
    simulating_(false),
    private_data(new PhysicsWorldPrivate(this))
{
    gContactAddedCallback = CustomMaterialCombinerCallback;
    init(context->m_signal_allocator);
}

PhysicsWorld::~PhysicsWorld()
{
    if (scene_)
    {
        // Force all remaining constraints, rigid bodies and collision shapes to release themselves
        for (Constraint * elem : constraints_)
            elem->ReleaseConstraint();

        for (RigidBody * elem : rigidBodies_)
            elem->ReleaseBody();

        for (CollisionShape* elem : collisionShapes_)
            elem->ReleaseShape();
    }

    delete private_data;
}

void PhysicsWorld::RegisterObject(Context* context)
{
    context->RegisterFactory<PhysicsWorld>(SUBSYSTEM_CATEGORY);

    URHO3D_MIXED_ACCESSOR_ATTRIBUTE("Gravity", GetGravity, SetGravity, Vector3, DEFAULT_GRAVITY, AM_DEFAULT);
    URHO3D_ATTRIBUTE("Physics FPS", int, fps_, DEFAULT_FPS, AM_DEFAULT);
    URHO3D_ATTRIBUTE("Max Substeps", int, maxSubSteps_, 0, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Solver Iterations", GetNumIterations, SetNumIterations, int, 10, AM_DEFAULT);
    URHO3D_ATTRIBUTE("Net Max Angular Vel.", float, maxNetworkAngularVelocity_, DEFAULT_MAX_NETWORK_ANGULAR_VELOCITY, AM_DEFAULT);
    URHO3D_ATTRIBUTE("Interpolation", bool, interpolation_, true, AM_FILE);
    URHO3D_ATTRIBUTE("Internal Edge Utility", bool, internalEdge_, true, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Split Impulse", GetSplitImpulse, SetSplitImpulse, bool, false, AM_DEFAULT);
}

PhysicsWorldPrivate::PhysicsWorldPrivate(PhysicsWorld * o) : owner(o) {
    if (PhysicsWorld::config.collisionConfig_)
        collisionConfiguration_ = PhysicsWorld::config.collisionConfig_;
    else
        collisionConfiguration_ = new btDefaultCollisionConfiguration();
    collisionDispatcher_.reset(new btCollisionDispatcher(collisionConfiguration_));
    broadphase_.reset(new btDbvtBroadphase());
    solver_.reset(new btSequentialImpulseConstraintSolver);
    world_.reset(new btDiscreteDynamicsWorld(collisionDispatcher_.get(), broadphase_.get(), solver_.get(), collisionConfiguration_));

    world_->setGravity(ToBtVector3(DEFAULT_GRAVITY));
    world_->getDispatchInfo().m_useContinuous = true;
    world_->getSolverInfo().m_splitImpulse = false; // Disable by default for performance
    world_->setDebugDrawer(this);
    world_->setInternalTickCallback(InternalPreTickCallback, static_cast<void*>(this), true);
    world_->setInternalTickCallback(InternalTickCallback, static_cast<void*>(this), false);
    world_->setSynchronizeAllMotionStates(true);
}

PhysicsWorldPrivate::~PhysicsWorldPrivate()
{
    world_.reset();
    solver_.reset();
    broadphase_.reset();
    collisionDispatcher_.reset();

    // Delete configuration only if it was the default created by PhysicsWorld
    if (!PhysicsWorld::config.collisionConfig_)
        delete collisionConfiguration_;
    collisionConfiguration_ = nullptr;
}

bool PhysicsWorldPrivate::isVisible(const btVector3& aabbMin, const btVector3& aabbMax)
{
    if (debugRenderer_)
        return debugRenderer_->IsInside(BoundingBox(ToVector3(aabbMin), ToVector3(aabbMax)));
    else
        return false;
}

void PhysicsWorldPrivate::drawLine(const btVector3& from, const btVector3& to, const btVector3& color)
{
    if (debugRenderer_)
        debugRenderer_->AddLine(ToVector3(from), ToVector3(to), Color(color.x(), color.y(), color.z()), debugDepthTest_);
}

void PhysicsWorld::DrawDebugGeometry(DebugRenderer* debug, bool depthTest)
{
    if (debug)
    {
        URHO3D_PROFILE(PhysicsDrawDebug);

        L_D(PhysicsWorld);
        d->debugRenderer_ = debug;
        d->debugDepthTest_ = depthTest;
        d->world_->debugDrawWorld();
        d->debugRenderer_ = nullptr;
    }
}

void PhysicsWorldPrivate::reportErrorWarning(const char* warningString)
{
    URHO3D_LOGWARNING("Physics: " + QString(warningString));
}

void PhysicsWorldPrivate::drawContactPoint(const btVector3& pointOnB, const btVector3& normalOnB, btScalar distance, int lifeTime, const btVector3& color)
{
}

void PhysicsWorldPrivate::draw3dText(const btVector3& location, const char* textString)
{
}

void PhysicsWorld::Update(float timeStep)
{
    URHO3D_PROFILE(UpdatePhysics);

    float internalTimeStep = 1.0f / fps_;
    int maxSubSteps = (int)(timeStep * fps_) + 1;
    if (maxSubSteps_ < 0)
    {
        internalTimeStep = timeStep;
        maxSubSteps = 1;
    }
    else if (maxSubSteps_ > 0)
        maxSubSteps = Min(maxSubSteps, maxSubSteps_);

    delayedWorldTransforms_.clear();
    simulating_ = true;

    L_D(PhysicsWorld);
    if (interpolation_)
        d->world_->stepSimulation(timeStep, maxSubSteps, internalTimeStep);
    else
    {
        timeAcc_ += timeStep;
        while (timeAcc_ >= internalTimeStep && maxSubSteps > 0)
        {
            d->world_->stepSimulation(internalTimeStep, 0, internalTimeStep);
            timeAcc_ -= internalTimeStep;
            --maxSubSteps;
        }
    }

    simulating_ = false;
    // Apply delayed (parented) world transforms now
    while (!delayedWorldTransforms_.isEmpty())
    {
        for (auto i = delayedWorldTransforms_.begin(); i != delayedWorldTransforms_.end(); )
        {
            const DelayedWorldTransform& transform(MAP_VALUE(i));

            // If parent's transform has already been assigned, can proceed
            if (!delayedWorldTransforms_.contains(transform.parentRigidBody_))
            {
                transform.rigidBody_->ApplyWorldTransform(transform.worldPosition_, transform.worldRotation_);
                i = delayedWorldTransforms_.erase(i);
            }
            else
                ++i;
        }
    }
}

void PhysicsWorld::UpdateCollisions()
{
    L_D(PhysicsWorld);
    d->world_->performDiscreteCollisionDetection();
}

void PhysicsWorld::SetFps(int fps)
{
    fps_ = Clamp(fps, 1, 1000);

    MarkNetworkUpdate();
}

void PhysicsWorld::SetGravity(const Vector3& gravity)
{
    L_D(PhysicsWorld);
    d->world_->setGravity(ToBtVector3(gravity));

    MarkNetworkUpdate();
}

void PhysicsWorld::SetMaxSubSteps(int num)
{
    maxSubSteps_ = num;
    MarkNetworkUpdate();
}

void PhysicsWorld::SetNumIterations(int num)
{
    L_D(PhysicsWorld);
    num = Clamp(num, 1, MAX_SOLVER_ITERATIONS);
    d->world_->getSolverInfo().m_numIterations = num;

    MarkNetworkUpdate();
}

void PhysicsWorld::SetUpdateEnabled(bool enable)
{
    updateEnabled_ = enable;
}
void PhysicsWorld::SetInterpolation(bool enable)
{
    interpolation_ = enable;
}

void PhysicsWorld::SetInternalEdge(bool enable)
{
    internalEdge_ = enable;

    MarkNetworkUpdate();
}

void PhysicsWorld::SetSplitImpulse(bool enable)
{
    L_D(PhysicsWorld);
    d->world_->getSolverInfo().m_splitImpulse = enable;

    MarkNetworkUpdate();
}

void PhysicsWorld::SetMaxNetworkAngularVelocity(float velocity)
{
    maxNetworkAngularVelocity_ = Clamp(velocity, 1.0f, 32767.0f);

    MarkNetworkUpdate();
}

void PhysicsWorld::Raycast(std::vector<PhysicsRaycastResult>& result, const Ray& ray, float maxDistance, unsigned collisionMask)
{
    URHO3D_PROFILE(PhysicsRaycast);
    if (maxDistance >= M_INFINITY)
        URHO3D_LOGWARNING("Infinite maxDistance in physics raycast is not supported");

    btCollisionWorld::AllHitsRayResultCallback rayCallback(ToBtVector3(ray.origin_), ToBtVector3(ray.origin_ + maxDistance * ray.direction_));
    rayCallback.m_collisionFilterGroup = (short)0xffff;
    rayCallback.m_collisionFilterMask = (short)collisionMask;
    L_D(PhysicsWorld);

    d->world_->rayTest(rayCallback.m_rayFromWorld, rayCallback.m_rayToWorld, rayCallback);

    for (int i = 0; i < rayCallback.m_collisionObjects.size(); ++i)
    {
        PhysicsRaycastResult newResult;
        newResult.body_ = static_cast<RigidBody*>(rayCallback.m_collisionObjects[i]->getUserPointer());
        newResult.position_ = ToVector3(rayCallback.m_hitPointWorld[i]);
        newResult.normal_ = ToVector3(rayCallback.m_hitNormalWorld[i]);
        newResult.distance_ = (newResult.position_ - ray.origin_).Length();
        newResult.hitFraction_ = rayCallback.m_closestHitFraction;
        result.push_back(newResult);
    }

    std::sort(result.begin(), result.end(), CompareRaycastResults);
}

void PhysicsWorld::RaycastSingle(PhysicsRaycastResult& result, const Ray& ray, float maxDistance, unsigned collisionMask)
{
    URHO3D_PROFILE(PhysicsRaycastSingle);
    if (maxDistance >= M_INFINITY)
        URHO3D_LOGWARNING("Infinite maxDistance in physics raycast is not supported");

    btCollisionWorld::ClosestRayResultCallback rayCallback(ToBtVector3(ray.origin_),
                                                           ToBtVector3(ray.origin_ + maxDistance * ray.direction_));
    rayCallback.m_collisionFilterGroup = (short)0xffff;
    rayCallback.m_collisionFilterMask = (short)collisionMask;
    L_D(PhysicsWorld);

    d->world_->rayTest(rayCallback.m_rayFromWorld, rayCallback.m_rayToWorld, rayCallback);

    if (rayCallback.hasHit())
    {
        result.position_ = ToVector3(rayCallback.m_hitPointWorld);
        result.normal_ = ToVector3(rayCallback.m_hitNormalWorld);
        result.distance_ = (result.position_ - ray.origin_).Length();
        result.hitFraction_ = rayCallback.m_closestHitFraction;
        result.body_ = static_cast<RigidBody*>(rayCallback.m_collisionObject->getUserPointer());
    }
    else
    {
        result.position_ = Vector3::ZERO;
        result.normal_ = Vector3::ZERO;
        result.distance_ = M_INFINITY;
        result.hitFraction_ = 0.0f;
        result.body_ = nullptr;
    }
}

void PhysicsWorld::RaycastSingleSegmented(PhysicsRaycastResult& result, const Ray& ray, float maxDistance, float segmentDistance, unsigned collisionMask)
{
    URHO3D_PROFILE(PhysicsRaycastSingleSegmented);

    if (maxDistance >= M_INFINITY)
        URHO3D_LOGWARNING("Infinite maxDistance in physics raycast is not supported");

    btVector3 start = ToBtVector3(ray.origin_);
    btVector3 end;
    btVector3 direction = ToBtVector3(ray.direction_);
    float distance;
    L_D(PhysicsWorld);

    for (float remainingDistance = maxDistance; remainingDistance > 0; remainingDistance -= segmentDistance)
    {
        distance = Min(remainingDistance, segmentDistance);

        end = start + distance * direction;

        btCollisionWorld::ClosestRayResultCallback rayCallback(start, end);
        rayCallback.m_collisionFilterGroup = (short)0xffff;
        rayCallback.m_collisionFilterMask = (short)collisionMask;

        d->world_->rayTest(rayCallback.m_rayFromWorld, rayCallback.m_rayToWorld, rayCallback);

        if (rayCallback.hasHit())
        {
            result.position_ = ToVector3(rayCallback.m_hitPointWorld);
            result.normal_ = ToVector3(rayCallback.m_hitNormalWorld);
            result.distance_ = (result.position_ - ray.origin_).Length();
            result.hitFraction_ = rayCallback.m_closestHitFraction;
            result.body_ = static_cast<RigidBody*>(rayCallback.m_collisionObject->getUserPointer());
            // No need to cast the rest of the segments
            return;
        }

        // Use the end position as the new start position
        start = end;
    }

    // Didn't hit anything
    result.position_ = Vector3::ZERO;
    result.normal_ = Vector3::ZERO;
    result.distance_ = M_INFINITY;
    result.hitFraction_ = 0.0f;
    result.body_ = 0;
}
void PhysicsWorld::SphereCast(PhysicsRaycastResult& result, const Ray& ray, float radius, float maxDistance, unsigned collisionMask)
{
    URHO3D_PROFILE(PhysicsSphereCast);
    if (maxDistance >= M_INFINITY)
        URHO3D_LOGWARNING("Infinite maxDistance in physics sphere cast is not supported");
    L_D(PhysicsWorld);

    btSphereShape shape(radius);
    Vector3 endPos = ray.origin_ + maxDistance * ray.direction_;

    btCollisionWorld::ClosestConvexResultCallback
            convexCallback(ToBtVector3(ray.origin_), ToBtVector3(endPos));
    convexCallback.m_collisionFilterGroup = (short)0xffff;
    convexCallback.m_collisionFilterMask = (short)collisionMask;

    d->world_->convexSweepTest(&shape, btTransform(btQuaternion::getIdentity(), convexCallback.m_convexFromWorld),
                            btTransform(btQuaternion::getIdentity(), convexCallback.m_convexToWorld), convexCallback);

    if (convexCallback.hasHit())
    {
        result.body_ = static_cast<RigidBody*>(convexCallback.m_hitCollisionObject->getUserPointer());
        result.position_ = ToVector3(convexCallback.m_hitPointWorld);
        result.normal_ = ToVector3(convexCallback.m_hitNormalWorld);
        result.distance_ = convexCallback.m_closestHitFraction * (endPos - ray.origin_).Length();
        result.hitFraction_ = convexCallback.m_closestHitFraction;
    }
    else
    {
        result.body_ = nullptr;
        result.position_ = Vector3::ZERO;
        result.normal_ = Vector3::ZERO;
        result.distance_ = M_INFINITY;
        result.hitFraction_ = 0.0f;
    }
}

void PhysicsWorld::ConvexCast(PhysicsRaycastResult& result, CollisionShape* shape, const Vector3& startPos, const Quaternion& startRot, const Vector3& endPos, const Quaternion& endRot, unsigned collisionMask)
{
    if (!shape || !shape->GetCollisionShape())
    {
        URHO3D_LOGERROR("Null collision shape for convex cast");
        result.body_ = nullptr;
        result.position_ = Vector3::ZERO;
        result.normal_ = Vector3::ZERO;
        result.distance_ = M_INFINITY;
        result.hitFraction_ = 0.0f;
        return;
    }

    // If shape is attached in a rigidbody, set its collision group temporarily to 0 to make sure it is not returned in the sweep result
    RigidBody* bodyComp = shape->GetComponent<RigidBody>();
    btRigidBody* body = bodyComp ? bodyComp->GetBody() : nullptr;
    btBroadphaseProxy* proxy = body ? body->getBroadphaseProxy() : nullptr;
    short group = 0;
    if (proxy)
    {
        group = proxy->m_collisionFilterGroup;
        proxy->m_collisionFilterGroup = 0;
    }
    // Take the shape's offset position & rotation into account
    Node* shapeNode = shape->GetNode();
    Matrix3x4 startTransform(startPos, startRot, shapeNode ? shapeNode->GetWorldScale() : Vector3::ONE);
    Matrix3x4 endTransform(endPos, endRot, shapeNode ? shapeNode->GetWorldScale() : Vector3::ONE);
    Vector3 effectiveStartPos = startTransform * shape->GetPosition();
    Vector3 effectiveEndPos = endTransform * shape->GetPosition();
    Quaternion effectiveStartRot = startRot * shape->GetRotation();
    Quaternion effectiveEndRot = endRot * shape->GetRotation();

    ConvexCast(result, shape->GetCollisionShape(), effectiveStartPos, effectiveStartRot, effectiveEndPos, effectiveEndRot, collisionMask);

    // Restore the collision group
    if (proxy)
        proxy->m_collisionFilterGroup = group;
}

void PhysicsWorld::ConvexCast(PhysicsRaycastResult& result, btCollisionShape* shape, const Vector3& startPos, const Quaternion& startRot, const Vector3& endPos, const Quaternion& endRot, unsigned collisionMask)
{
    if (!shape)
    {
        URHO3D_LOGERROR("Null collision shape for convex cast");
        result.body_ = nullptr;
        result.position_ = Vector3::ZERO;
        result.normal_ = Vector3::ZERO;
        result.distance_ = M_INFINITY;
        result.hitFraction_ = 0.0f;
        return;
    }

    if (!shape->isConvex())
    {
        URHO3D_LOGERROR("Can not use non-convex collision shape for convex cast");
        result.body_ = nullptr;
        result.position_ = Vector3::ZERO;
        result.normal_ = Vector3::ZERO;
        result.distance_ = M_INFINITY;
        result.hitFraction_ = 0.0f;
        return;
    }
    L_D(PhysicsWorld);

    URHO3D_PROFILE(PhysicsConvexCast);

    btCollisionWorld::ClosestConvexResultCallback convexCallback(ToBtVector3(startPos), ToBtVector3(endPos));
    convexCallback.m_collisionFilterGroup = (short)0xffff;
    convexCallback.m_collisionFilterMask = (short)collisionMask;

    d->world_->convexSweepTest(static_cast<btConvexShape *>(shape),
                               btTransform(ToBtQuaternion(startRot), convexCallback.m_convexFromWorld),
                               btTransform(ToBtQuaternion(endRot), convexCallback.m_convexToWorld), convexCallback);

    if (convexCallback.hasHit())
    {
        result.body_ = static_cast<RigidBody*>(convexCallback.m_hitCollisionObject->getUserPointer());
        result.position_ = ToVector3(convexCallback.m_hitPointWorld);
        result.normal_ = ToVector3(convexCallback.m_hitNormalWorld);
        result.distance_ = convexCallback.m_closestHitFraction * (endPos - startPos).Length();
        result.hitFraction_ = convexCallback.m_closestHitFraction;
    }
    else
    {
        result.body_ = nullptr;
        result.position_ = Vector3::ZERO;
        result.normal_ = Vector3::ZERO;
        result.distance_ = M_INFINITY;
        result.hitFraction_ = 0.0f;
    }
}

void PhysicsWorld::RemoveCachedGeometry(Model* model)
{
    for (auto i = triMeshCache_.begin(), fin = triMeshCache_.end(); i!=fin; )
    {
        if (MAP_KEY(i).first == model)
            i=triMeshCache_.erase(i);
        else
            ++i;
    }
    for (auto i = convexCache_.begin(); i != convexCache_.end();)
    {
        if (MAP_KEY(i).first == model)
            i=convexCache_.erase(i);
        else
            ++i;
    }
}

void PhysicsWorld::GetRigidBodies(std::unordered_set<RigidBody*>& result, const Sphere& sphere, unsigned collisionMask)
{
    URHO3D_PROFILE(PhysicsSphereQuery);

    result.clear();
    L_D(PhysicsWorld);

    btSphereShape sphereShape(sphere.radius_);
    std::unique_ptr<btRigidBody> tempRigidBody(new btRigidBody(1.0f, nullptr, &sphereShape));
    tempRigidBody->setWorldTransform(btTransform(btQuaternion::getIdentity(), ToBtVector3(sphere.center_)));
    // Need to activate the temporary rigid body to get reliable results from static, sleeping objects
    tempRigidBody->activate();
    d->world_->addRigidBody(tempRigidBody.get());

    PhysicsQueryCallback callback(result, collisionMask);
    d->world_->contactTest(tempRigidBody.get(), callback);

    d->world_->removeRigidBody(tempRigidBody.get());
}

void PhysicsWorld::GetRigidBodies(std::unordered_set<RigidBody*>& result, const BoundingBox& box, unsigned collisionMask)
{
    URHO3D_PROFILE(PhysicsBoxQuery);
    L_D(PhysicsWorld);

    result.clear();

    btBoxShape boxShape(ToBtVector3(box.HalfSize()));
    std::unique_ptr<btRigidBody> tempRigidBody(new btRigidBody(1.0f, nullptr, &boxShape));
    tempRigidBody->setWorldTransform(btTransform(btQuaternion::getIdentity(), ToBtVector3(box.Center())));
    tempRigidBody->activate();
    d->world_->addRigidBody(tempRigidBody.get());

    PhysicsQueryCallback callback(result, collisionMask);
    d->world_->contactTest(tempRigidBody.get(), callback);

    d->world_->removeRigidBody(tempRigidBody.get());
}

void PhysicsWorld::GetRigidBodies(std::unordered_set<RigidBody*>& result, const RigidBody* body)
{
    URHO3D_PROFILE(PhysicsBodyQuery);

    result.clear();

    if (!body || !body->GetBody())
        return;
    L_D(PhysicsWorld);

    PhysicsQueryCallback callback(result, body->GetCollisionMask());
    d->world_->contactTest(body->GetBody(), callback);

    // Remove the body itself from the returned list
    result.erase((RigidBody *)body);
}

void PhysicsWorld::GetCollidingBodies(std::unordered_set<RigidBody*>& result, const RigidBody* body)
{
    URHO3D_PROFILE(GetCollidingBodies);

    result.clear();

    for (auto & elem : currentCollisions_.keys())
    {
        if (elem.first == body) {
            assert(elem.second);
            result.insert(elem.second);
        }
        else if (elem.second == body) {
            assert(elem.first);
            result.insert(elem.first);
        }
    }
}

Vector3 PhysicsWorld::GetGravity() const
{
    L_D(PhysicsWorld);
    return ToVector3(d->world_->getGravity());
}

int PhysicsWorld::GetNumIterations() const
{
    L_D(PhysicsWorld);
    return d->world_->getSolverInfo().m_numIterations;
}

bool PhysicsWorld::GetSplitImpulse() const
{
    L_D(PhysicsWorld);
    return d->world_->getSolverInfo().m_splitImpulse != 0;
}

void PhysicsWorld::AddRigidBody(RigidBody* body)
{
    rigidBodies_.insert(body);
}

void PhysicsWorld::RemoveRigidBody(RigidBody* body)
{
    rigidBodies_.erase(body);
    // Remove possible dangling pointer from the delayedWorldTransforms structure
    delayedWorldTransforms_.remove(body);
}

void PhysicsWorld::AddCollisionShape(CollisionShape* shape)
{
    collisionShapes_.insert(shape);
}

void PhysicsWorld::RemoveCollisionShape(CollisionShape* shape)
{
    collisionShapes_.erase(shape);
}

void PhysicsWorld::AddConstraint(Constraint* constraint)
{
    constraints_.push_back(constraint);
}

void PhysicsWorld::RemoveConstraint(Constraint* constraint)
{
    auto iter = std::find(constraints_.begin(),constraints_.end(),constraint);
    assert(iter!=constraints_.end());
    constraints_.erase(iter);
}

void PhysicsWorld::AddDelayedWorldTransform(const DelayedWorldTransform& transform)
{
    delayedWorldTransforms_[transform.rigidBody_] = transform;
}

void PhysicsWorld::DrawDebugGeometry(bool depthTest)
{
    DebugRenderer* debug = GetComponent<DebugRenderer>();
    DrawDebugGeometry(debug, depthTest);
}

void PhysicsWorld::SetDebugRenderer(DebugRenderer* debug)
{
    L_D(PhysicsWorld);
    d->debugRenderer_ = debug;
}

void PhysicsWorld::SetDebugDepthTest(bool enable)
{
    L_D(PhysicsWorld);
    d->debugDepthTest_ = enable;
}

void PhysicsWorld::CleanupGeometryCache()
{
    // Remove cached shapes whose only reference is the cache itself
    for (auto i = triMeshCache_.begin(); i != triMeshCache_.end(); )
    {
        if (MAP_VALUE(i).Refs() == 1)
            i=triMeshCache_.erase(i);
        else
            ++i;
    }
    for (auto i = convexCache_.begin(); i != convexCache_.end();)
    {
        if (MAP_VALUE(i).Refs() == 1)
            i=convexCache_.erase(i);
        else
            ++i;
    }
}

void PhysicsWorld::OnSceneSet(Scene* scene)
{
    // Subscribe to the scene subsystem update, which will trigger the physics simulation step
    if (scene) {
        scene_ = GetScene();
        scene->sceneSubsystemUpdate.Connect(this,&PhysicsWorld::HandleSceneSubsystemUpdate);
    }
    else {
        assert(scene_);
        scene_->sceneSubsystemUpdate.Disconnect(this, &PhysicsWorld::HandleSceneSubsystemUpdate);
    }

}

void PhysicsWorld::HandleSceneSubsystemUpdate(Scene *, float ts)
{
    if (!updateEnabled_)
        return;
    Update(ts);
}

void PhysicsWorld::PreStep(float timeStep)
{
    // Send pre-step event
    pre_step.Emit(this,timeStep);

    // Start profiling block for the actual simulation step
#ifdef LUTEFISK3D_PROFILING
    Profiler* profiler = context_->m_ProfilerSystem.get();
    if (profiler)
        profiler->BeginBlock("StepSimulation");
#endif
}

void PhysicsWorld::PostStep(float timeStep)
{
#ifdef LUTEFISK3D_PROFILING
    Profiler* profiler = context_->m_ProfilerSystem.get();
    if (profiler)
        profiler->EndBlock();
#endif

    SendCollisionEvents();

    // Send post-step event
    post_step.Emit(this,timeStep);
}

void PhysicsWorld::SendCollisionEvents()
{
    URHO3D_PROFILE(SendCollisionEvents);

    currentCollisions_.clear();
    L_D(PhysicsWorld);

    int numManifolds = d->collisionDispatcher_->getNumManifolds();

    if (numManifolds)
    {
        for (int i = 0; i < numManifolds; ++i)
        {
            btPersistentManifold* contactManifold = d->collisionDispatcher_->getManifoldByIndexInternal(i);
            // First check that there are actual contacts, as the manifold exists also when objects are close but not touching
            if (!contactManifold->getNumContacts())
                continue;

            const btCollisionObject* objectA = contactManifold->getBody0();
            const btCollisionObject* objectB = contactManifold->getBody1();

            RigidBody* bodyA = static_cast<RigidBody*>(objectA->getUserPointer());
            RigidBody* bodyB = static_cast<RigidBody*>(objectB->getUserPointer());
            // If it's not a rigidbody, maybe a ghost object
            if (!bodyA || !bodyB)
                continue;

            // Skip collision event signaling if both objects are static, or if collision event mode does not match
            if (bodyA->GetMass() == 0.0f && bodyB->GetMass() == 0.0f)
                continue;
            if (bodyA->GetCollisionEventMode() == COLLISION_NEVER || bodyB->GetCollisionEventMode() == COLLISION_NEVER)
                continue;
            if (bodyA->GetCollisionEventMode() == COLLISION_ACTIVE && bodyB->GetCollisionEventMode() == COLLISION_ACTIVE &&
                    !bodyA->IsActive() && !bodyB->IsActive())
                continue;

            WeakPtr<RigidBody> bodyWeakA(bodyA);
            WeakPtr<RigidBody> bodyWeakB(bodyB);

            // First only store the collision pair as weak pointers and the manifold pointer, so user code can safely destroy
            // objects during collision event handling
            std::pair<WeakPtr<RigidBody>, WeakPtr<RigidBody> > bodyPair;
            if (bodyA < bodyB)
            {
                bodyPair = std::make_pair(bodyWeakA, bodyWeakB);
                currentCollisions_[bodyPair].manifold_ = contactManifold;
            }
            else
            {
                bodyPair = std::make_pair(bodyWeakB, bodyWeakA);
                currentCollisions_[bodyPair].flippedManifold_ = contactManifold;
            }
        }

        for (auto elem = currentCollisions_.begin(),fin=currentCollisions_.end(); elem!=fin; ++elem)
        {
            RigidBody* bodyA = MAP_KEY(elem).first;
            RigidBody* bodyB = MAP_KEY(elem).second;
            if (!bodyA || !bodyB)
                continue;

            btPersistentManifold* contactManifold = MAP_VALUE(elem).manifold_;

            Node* nodeA = bodyA->GetNode();
            Node* nodeB = bodyB->GetNode();
            WeakPtr<Node> nodeWeakA(nodeA);
            WeakPtr<Node> nodeWeakB(nodeB);

            bool trigger = bodyA->IsTrigger() || bodyB->IsTrigger();
            bool newCollision = !previousCollisions_.contains(MAP_KEY(elem));

            contacts_.clear();

            // "Pointers not flipped"-manifold, send unmodified normals
            if (contactManifold)
            {
                for (int j = 0; j < contactManifold->getNumContacts(); ++j)
                {
                    btManifoldPoint& point = contactManifold->getContactPoint(j);
                    contacts_.WriteVector3(ToVector3(point.m_positionWorldOnB));
                    contacts_.WriteVector3(ToVector3(point.m_normalWorldOnB));
                    contacts_.WriteFloat(point.m_distance1);
                    contacts_.WriteFloat(point.m_appliedImpulse);
                }
            }
            // "Pointers flipped"-manifold, flip normals also
            contactManifold = MAP_VALUE(elem).flippedManifold_;
            if (contactManifold)
            {
                for (int j = 0; j < contactManifold->getNumContacts(); ++j)
                {
                    btManifoldPoint& point = contactManifold->getContactPoint(j);
                    contacts_.WriteVector3(ToVector3(point.m_positionWorldOnB));
                    contacts_.WriteVector3(-ToVector3(point.m_normalWorldOnB));
                    contacts_.WriteFloat(point.m_distance1);
                    contacts_.WriteFloat(point.m_appliedImpulse);
                }
            }

            // Send separate collision start event if collision is new
            if (newCollision)
            {
                collisionStart.Emit(this,nodeA,nodeB,bodyA,bodyB,trigger,contacts_.GetBuffer());
                // Skip rest of processing if either of the nodes or bodies is removed as a response to the event
                if (!nodeWeakA || !nodeWeakB || !MAP_KEY(elem).first || !MAP_KEY(elem).second)
                    continue;
            }
            collision.Emit(this,nodeA,nodeB,bodyA,bodyB,trigger,contacts_.GetBuffer());
            if (!nodeWeakA || !nodeWeakB || !MAP_KEY(elem).first || !MAP_KEY(elem).second)
                continue;

            if (newCollision)
            {
                auto iter = nodeCollisionStart.find(nodeA);
                if(iter!=nodeCollisionStart.end()) {
                    iter->second.Emit(bodyA,nodeB,bodyB,trigger,contacts_.GetBuffer());
                    if (!nodeWeakA || !nodeWeakB || !MAP_KEY(elem).first || !MAP_KEY(elem).second)
                        continue;
                }
            }
            auto iter = nodeCollision.find(nodeA);
            if(iter!=nodeCollision.end()) {
                iter->second.Emit(bodyA,nodeB,bodyB,trigger,contacts_.GetBuffer());
                if (!nodeWeakA || !nodeWeakB || !MAP_KEY(elem).first || !MAP_KEY(elem).second)
                    continue;
            }

            // Flip perspective to body B
            contacts_.clear();
            contactManifold = MAP_VALUE(elem).manifold_;
            if (contactManifold)
            {
                for (int j = 0; j < contactManifold->getNumContacts(); ++j)
                {
                    btManifoldPoint& point = contactManifold->getContactPoint(j);
                    contacts_.WriteVector3(ToVector3(point.m_positionWorldOnB));
                    contacts_.WriteVector3(-ToVector3(point.m_normalWorldOnB));
                    contacts_.WriteFloat(point.m_distance1);
                    contacts_.WriteFloat(point.m_appliedImpulse);
                }
            }
            contactManifold = MAP_VALUE(elem).flippedManifold_;
            if (contactManifold)
            {
                for (int j = 0; j < contactManifold->getNumContacts(); ++j)
                {
                    btManifoldPoint& point = contactManifold->getContactPoint(j);
                    contacts_.WriteVector3(ToVector3(point.m_positionWorldOnB));
                    contacts_.WriteVector3(ToVector3(point.m_normalWorldOnB));
                    contacts_.WriteFloat(point.m_distance1);
                    contacts_.WriteFloat(point.m_appliedImpulse);
                }
            }

            if (newCollision)
            {
                auto iter = nodeCollisionStart.find(nodeB);
                if(iter!=nodeCollisionStart.end()) {
                    iter->second.Emit(bodyB,nodeA,bodyA,trigger,contacts_.GetBuffer());
                    if (!nodeWeakA || !nodeWeakB || !MAP_KEY(elem).first || !MAP_KEY(elem).second)
                        continue;
                }
            }
            iter = nodeCollision.find(nodeB);
            if(iter!=nodeCollision.end()) {
                iter->second.Emit(bodyB,nodeA,bodyA,trigger,contacts_.GetBuffer());
            }
        }
    }

    // Send collision end events as applicable
    {
        for (auto & elem : previousCollisions_.keys())
        {
            if (!currentCollisions_.contains(elem))
            {
                RigidBody* bodyA = elem.first;
                RigidBody* bodyB = elem.second;
                if (!bodyA || !bodyB)
                    continue;

                bool trigger = bodyA->IsTrigger() || bodyB->IsTrigger();

                // Skip collision event signaling if both objects are static, or if collision event mode does not match
                if (bodyA->GetMass() == 0.0f && bodyB->GetMass() == 0.0f)
                    continue;
                if (bodyA->GetCollisionEventMode() == COLLISION_NEVER || bodyB->GetCollisionEventMode() == COLLISION_NEVER)
                    continue;
                if (bodyA->GetCollisionEventMode() == COLLISION_ACTIVE && bodyB->GetCollisionEventMode() == COLLISION_ACTIVE &&
                        !bodyA->IsActive() && !bodyB->IsActive())
                    continue;

                Node* nodeA = bodyA->GetNode();
                Node* nodeB = bodyB->GetNode();
                WeakPtr<Node> nodeWeakA(nodeA);
                WeakPtr<Node> nodeWeakB(nodeB);

                collisionEnd.Emit(this,nodeA,nodeB,bodyA,bodyB,trigger);
                // Skip rest of processing if either of the nodes or bodies is removed as a response to the event
                if (!nodeWeakA || !nodeWeakB || !elem.first || !elem.second)
                    continue;

                auto iter = nodeCollisionEnd.find(nodeA);
                if(iter!=nodeCollisionEnd.end()) {
                    iter->second.Emit(bodyA,nodeB,bodyB,trigger);
                    if (!nodeWeakA || !nodeWeakB || !elem.first || !elem.second)
                        continue;
                }
                iter = nodeCollisionEnd.find(nodeB);
                if(iter!=nodeCollisionEnd.end()) {
                    iter->second.Emit(bodyB,nodeA,bodyA,trigger);
                }
            }
        }
    }

    previousCollisions_ = currentCollisions_;
}

void RegisterPhysicsLibrary(Context* context)
{
    CollisionShape::RegisterObject(context);
    RigidBody::RegisterObject(context);
    Constraint::RegisterObject(context);
    PhysicsWorld::RegisterObject(context);
}

btDiscreteDynamicsWorld *Urho3D::PhysicsWorld::GetWorld() {
    L_D(PhysicsWorld);
    return d->world_.get();
}

}
