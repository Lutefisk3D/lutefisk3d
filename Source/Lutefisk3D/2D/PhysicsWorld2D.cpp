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
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHERContactInfo
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
#include "PhysicsWorld2D.h"

#include "CollisionShape2D.h"
#include "PhysicsEvents2D.h"
#include "PhysicsUtils2D.h"
#include "RigidBody2D.h"

#include "Lutefisk3D/Core/Context.h"
#include "Lutefisk3D/Graphics/DebugRenderer.h"
#include "Lutefisk3D/Graphics/Graphics.h"
#include "Lutefisk3D/IO/Log.h"
#include "Lutefisk3D/Core/Profiler.h"
#include "Lutefisk3D/Graphics/Renderer.h"
#include "Lutefisk3D/Scene/Scene.h"
#include "Lutefisk3D/Scene/SceneEvents.h"
#include "Lutefisk3D/Graphics/Viewport.h"

#include  <Box2D/Box2D.h>


namespace Urho3D
{

namespace
{
/// Contact info.
struct ContactInfo
{
    ContactInfo();
    ContactInfo(b2Contact* contact)
    {
        b2Fixture* fixtureA = contact->GetFixtureA();
        b2Fixture* fixtureB = contact->GetFixtureB();
        bodyA_ = (RigidBody2D*)(fixtureA->GetBody()->GetUserData());
        bodyB_ = (RigidBody2D*)(fixtureB->GetBody()->GetUserData());
        nodeA_ = bodyA_->GetNode();
        nodeB_ = bodyB_->GetNode();
        shapeA_ = (CollisionShape2D*)fixtureA->GetUserData();
        shapeB_ = (CollisionShape2D*)fixtureB->GetUserData();
        b2WorldManifold worldManifold;
        contact->GetWorldManifold(&worldManifold);
        numPoints_ = contact->GetManifold()->pointCount;
        worldNormal_ = Vector2(worldManifold.normal.x, worldManifold.normal.y);
        for (int i = 0; i < numPoints_; ++i)
        {
            worldPositions_[i] = Vector2(worldManifold.points[i].x, worldManifold.points[i].y);
            separations_[i] = worldManifold.separations[i];
        }
    }

    SharedPtr<RigidBody2D> bodyA_;
    SharedPtr<RigidBody2D> bodyB_;
    SharedPtr<Node> nodeA_;
    SharedPtr<Node> nodeB_;
    SharedPtr<CollisionShape2D> shapeA_;
    SharedPtr<CollisionShape2D> shapeB_;
    /// Number of contact points.
    int numPoints_;
    /// Contact normal in world space.
    Vector2 worldNormal_;
    /// Contact positions in world space.
    Vector2 worldPositions_[b2_maxManifoldPoints];
    /// Contact overlap values.
    float separations_[b2_maxManifoldPoints];

    /// Write contact info to buffer.
    const std::vector<unsigned char>& Serialize(VectorBuffer& buffer) const
    {
        buffer.clear();
        for (int i = 0; i < numPoints_; ++i)
        {
            buffer.WriteVector2(worldPositions_[i]);
            buffer.WriteVector2(worldNormal_);
            buffer.WriteFloat(separations_[i]);
        }
        return buffer.GetBuffer();
    }

};
struct PhysicsWorld2D_private : public b2ContactListener, public b2Draw
{
    PhysicsWorld2D_private(PhysicsWorld2D *owner) : m_owner(owner)
    {
        // Set default debug draw flags
        m_drawFlags = e_shapeBit;
    }
    // Implement b2ContactListener
    /// Called when two fixtures begin to touch.
    void BeginContact(b2Contact* contact) final
    {
        // Only handle contact event while stepping the physics simulation
        if (!physicsStepping_)
            return;

        b2Fixture* fixtureA = contact->GetFixtureA();
        b2Fixture* fixtureB = contact->GetFixtureB();
        if (!fixtureA || !fixtureB)
            return;

        beginContactInfos_.push_back(ContactInfo(contact));
    }
    /// Called when two fixtures cease to touch.
    void EndContact(b2Contact* contact) final
    {
        if (!physicsStepping_)
            return;

        b2Fixture* fixtureA = contact->GetFixtureA();
        b2Fixture* fixtureB = contact->GetFixtureB();
        if (!fixtureA || !fixtureB)
            return;

        endContactInfos_.push_back(ContactInfo(contact));
    }
    /// Called when contact is updated.
    void PreSolve(b2Contact* contact, const b2Manifold* oldManifold) final
    {
        b2Fixture* fixtureA = contact->GetFixtureA();
        b2Fixture* fixtureB = contact->GetFixtureB();
        if (!fixtureA || !fixtureB)
            return;

        ContactInfo contactInfo(contact);

        auto serializedContacts(contactInfo.Serialize(contacts_));
        // Send global event
        bool enabled=contact->IsEnabled();
        m_owner->update_contact.Emit(
                    m_owner,
                    contactInfo.bodyA_.Get(),
                    contactInfo.bodyB_.Get(),
                    contactInfo.nodeA_.Get(),
                    contactInfo.nodeB_.Get(),
                    serializedContacts,
                    contactInfo.shapeA_.Get(),
                    contactInfo.shapeB_.Get(),
                    enabled
                    );
        contact->SetEnabled(enabled);

        // Send node event
        enabled = contact->IsEnabled();

        if (contactInfo.nodeA_ && contactInfo.nodeA_->physics2dSignals_)
        {
            contactInfo.nodeA_->physics2dSignals_->updated_contacts.Emit(
                        contactInfo.bodyA_.Get(),
                        contactInfo.nodeB_.Get(),
                        contactInfo.bodyB_.Get(),
                        serializedContacts,
                        contactInfo.shapeA_.Get(),
                        contactInfo.shapeB_.Get(),
                        enabled
                        );
        }
        // TODO: consider here the fact that contact can be disabled be signal handler on nodeA, but the nodeB_ handler will
        // still be notified
        if (contactInfo.nodeB_ && contactInfo.nodeA_->physics2dSignals_)
        {
            contactInfo.nodeB_->physics2dSignals_->updated_contacts.Emit(
                        contactInfo.bodyB_.Get(),
                        contactInfo.nodeA_.Get(),
                        contactInfo.bodyA_.Get(),
                        serializedContacts,
                        contactInfo.shapeB_.Get(),
                        contactInfo.shapeA_.Get(),
                        enabled
                        );
        }

        contact->SetEnabled(enabled);
    }

    // Implement b2Draw
    /// Draw a closed polygon provided in CCW order.
    void DrawPolygon(const b2Vec2* vertices, int32 vertexCount, const b2Color& color) override;
    /// Draw a solid closed polygon provided in CCW order.
    void DrawSolidPolygon(const b2Vec2* vertices, int32 vertexCount, const b2Color& color) override;
    /// Draw a circle.
    void DrawCircle(const b2Vec2& center, float32 radius, const b2Color& color) override;
    /// Draw a solid circle.
    void DrawSolidCircle(const b2Vec2& center, float32 radius, const b2Vec2& axis, const b2Color& color) override;
    /// Draw a line segment.
    void DrawSegment(const b2Vec2& p1, const b2Vec2& p2, const b2Color& color) override;
    /// Draw a transform. Choose your own length scale.
    void DrawTransform(const b2Transform& xf) override;
    /// Draw a point.
    void DrawPoint(const b2Vec2& p, float32 size, const b2Color& color) override;

    bool GetDrawShape() const { return (m_drawFlags & e_shapeBit) != 0; }
    bool GetDrawJoint() const { return (m_drawFlags & e_jointBit) != 0; }
    bool GetDrawAabb() const { return (m_drawFlags & e_aabbBit) != 0; }
    bool GetDrawPair() const { return (m_drawFlags & e_pairBit) != 0; }
    bool GetDrawCenterOfMass() const { return (m_drawFlags & e_centerOfMassBit) != 0; }

    void setDrawFlag(uint32_t flg) { m_drawFlags |= flg; }
    /// Whether is currently stepping the world. Used internally.
    bool physicsStepping_=false;
    /// Begin contact infos.
    std::vector<ContactInfo> beginContactInfos_;
    /// End contact infos.
    std::vector<ContactInfo> endContactInfos_;
    /// Temporary buffer with contact data.
    VectorBuffer contacts_;
    PhysicsWorld2D *m_owner;
    /// Debug renderer.
    DebugRenderer* debugRenderer_ = nullptr;
    /// Debug draw depth test mode.
    bool debugDepthTest_ = false;



public:
    void SetDrawShape(bool drawShape)
    {
        if (drawShape)
            m_drawFlags |= e_shapeBit;
        else
            m_drawFlags &= ~e_shapeBit;

    }
    void SetDrawJoint(bool drawJoint)
    {
        if (drawJoint)
            m_drawFlags |= e_jointBit;
        else
            m_drawFlags &= ~e_jointBit;
    }
    void SetDrawAabb(bool drawAabb)
    {
        if (drawAabb)
            m_drawFlags |= e_aabbBit;
        else
            m_drawFlags &= ~e_aabbBit;
    }
    void SetDrawPair(bool drawPair)
    {
        if (drawPair)
            m_drawFlags |= e_pairBit;
        else
            m_drawFlags &= ~e_pairBit;
    }
    void SetDrawCenterOfMass(bool drawCenterOfMass)
    {
        if (drawCenterOfMass)
            m_drawFlags |= e_centerOfMassBit;
        else
            m_drawFlags &= ~e_centerOfMassBit;
    }
    void SendBeginContactEvents()
    {
        if (beginContactInfos_.empty())
            return;


        for (ContactInfo& contactInfo : beginContactInfos_)
        {
            auto serializedContacts(contactInfo.Serialize(contacts_));
            m_owner->begin_contact.Emit(
                        m_owner,
                        contactInfo.bodyA_.Get(),
                        contactInfo.bodyB_.Get(),
                        contactInfo.nodeA_.Get(),
                        contactInfo.nodeB_.Get(),
                        serializedContacts,
                        contactInfo.shapeA_.Get(),
                        contactInfo.shapeB_.Get()

                        );

            if (contactInfo.nodeA_ && contactInfo.nodeA_->physics2dSignals_)
            {
                contactInfo.nodeA_->physics2dSignals_->begin_contact.Emit(
                            contactInfo.bodyA_.Get(),
                            contactInfo.nodeB_.Get(),
                            contactInfo.bodyB_.Get(),
                            serializedContacts,
                            contactInfo.shapeA_.Get(),
                            contactInfo.shapeB_.Get()
                            );
            }

            if (contactInfo.nodeB_ && contactInfo.nodeB_->physics2dSignals_)
            {
                contactInfo.nodeB_->physics2dSignals_->begin_contact.Emit(
                            contactInfo.bodyB_.Get(),
                            contactInfo.nodeA_.Get(),
                            contactInfo.bodyA_.Get(),
                            serializedContacts,
                            contactInfo.shapeB_.Get(),
                            contactInfo.shapeA_.Get()
                            );
            }
        }
        beginContactInfos_.clear();
    }
    void SendEndContactEvents()
    {
        if (endContactInfos_.empty())
            return;

        for (ContactInfo& contactInfo : endContactInfos_)
        {
            auto serializedContacts(contactInfo.Serialize(contacts_));
            m_owner->end_contact.Emit(
                        m_owner,
                        contactInfo.bodyA_.Get(),
                        contactInfo.bodyB_.Get(),
                        contactInfo.nodeA_.Get(),
                        contactInfo.nodeB_.Get(),
                        serializedContacts,
                        contactInfo.shapeA_.Get(),
                        contactInfo.shapeB_.Get()

                        );

            if (contactInfo.nodeA_ && contactInfo.nodeA_->physics2dSignals_)
            {
                contactInfo.nodeA_->physics2dSignals_->end_contact.Emit(
                            contactInfo.bodyA_.Get(),
                            contactInfo.nodeB_.Get(),
                            contactInfo.bodyB_.Get(),
                            serializedContacts,
                            contactInfo.shapeA_.Get(),
                            contactInfo.shapeB_.Get()
                            );
            }

            if (contactInfo.nodeB_ && contactInfo.nodeB_->physics2dSignals_)
            {
                contactInfo.nodeB_->physics2dSignals_->end_contact.Emit(
                            contactInfo.bodyB_.Get(),
                            contactInfo.nodeA_.Get(),
                            contactInfo.bodyA_.Get(),
                            serializedContacts,
                            contactInfo.shapeB_.Get(),
                            contactInfo.shapeA_.Get()
                            );
            }
        }

        endContactInfos_.clear();
    }
};
};
extern const char* SUBSYSTEM_CATEGORY;
static const Vector2 DEFAULT_GRAVITY(0.0f, -9.81f);
static const int DEFAULT_VELOCITY_ITERATIONS = 8;
static const int DEFAULT_POSITION_ITERATIONS = 3;



PhysicsWorld2D::PhysicsWorld2D(Context* context) :
    Component(context),
    gravity_(DEFAULT_GRAVITY),
    velocityIterations_(DEFAULT_VELOCITY_ITERATIONS),
    positionIterations_(DEFAULT_POSITION_ITERATIONS),
    updateEnabled_(true),
    applyingTransforms_(false)
{
    privateData_ = new PhysicsWorld2D_private(this);
    PhysicsWorld2D_private *tgt = static_cast <PhysicsWorld2D_private *>(privateData_);
    // Create Box2D world
    world_.reset(new b2World(ToB2Vec2(gravity_)));
    // Set contact listener
    world_->SetContactListener(tgt);
    // Set debug draw
    world_->SetDebugDraw(tgt);

}

PhysicsWorld2D::~PhysicsWorld2D()
{
    world_->SetContactListener(nullptr);
    delete static_cast <PhysicsWorld2D_private *>(privateData_);
    for (const WeakPtr<RigidBody2D> &rb : rigidBodies_) {
        if (rb)
            rb->ReleaseBody();
    }
}

void PhysicsWorld2D::RegisterObject(Context* context)
{
    context->RegisterFactory<PhysicsWorld2D>(SUBSYSTEM_CATEGORY);

    URHO3D_ACCESSOR_ATTRIBUTE("Draw Shape", GetDrawShape, SetDrawShape, bool, false, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Draw Joint", GetDrawJoint, SetDrawJoint, bool, false, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Draw Aabb", GetDrawAabb, SetDrawAabb, bool, false, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Draw Pair", GetDrawPair, SetDrawPair, bool, false, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Draw CenterOfMass", GetDrawCenterOfMass, SetDrawCenterOfMass, bool, false, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Allow Sleeping", GetAllowSleeping, SetAllowSleeping, bool, false, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Warm Starting", GetWarmStarting, SetWarmStarting, bool, false, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Continuous Physics", GetContinuousPhysics, SetContinuousPhysics, bool, true, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Sub Stepping", GetSubStepping, SetSubStepping, bool, false, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Gravity", GetGravity, SetGravity, Vector2, DEFAULT_GRAVITY, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Auto Clear Forces", GetAutoClearForces, SetAutoClearForces, bool, false, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Velocity Iterations", GetVelocityIterations, SetVelocityIterations, int, DEFAULT_VELOCITY_ITERATIONS, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Position Iterations", GetPositionIterations, SetPositionIterations, int, DEFAULT_POSITION_ITERATIONS, AM_DEFAULT);
}

void PhysicsWorld2D::DrawDebugGeometry(DebugRenderer* debug, bool depthTest)
{
    if (debug)
    {
        URHO3D_PROFILE(Physics2DDrawDebug);
        PhysicsWorld2D_private *tgt = static_cast <PhysicsWorld2D_private *>(privateData_);

        tgt->debugRenderer_ = debug;
        tgt->debugDepthTest_ = depthTest;
        world_->DrawDebugData();
        tgt->debugRenderer_ = nullptr;
    }
}

void PhysicsWorld2D_private::DrawPolygon(const b2Vec2* vertices, int32 vertexCount, const b2Color& color)
{
    if (!debugRenderer_)
        return;

    Color c = ToColor(color);
    for (int i = 0; i < vertexCount - 1; ++i)
        debugRenderer_->AddLine(ToVector3(vertices[i]), ToVector3(vertices[i + 1]), c, debugDepthTest_);

    debugRenderer_->AddLine(ToVector3(vertices[vertexCount - 1]), ToVector3(vertices[0]), c, debugDepthTest_);
}

void PhysicsWorld2D_private::DrawSolidPolygon(const b2Vec2* vertices, int32 vertexCount, const b2Color& color)
{
    if (!debugRenderer_)
        return;

    Vector3 v = ToVector3(vertices[0]);
    Color c(color.r, color.g, color.b, 0.5f);
    for (int i = 1; i < vertexCount - 1; ++i)
        debugRenderer_->AddTriangle(v, ToVector3(vertices[i]), ToVector3(vertices[i + 1]), c, debugDepthTest_);
}

void PhysicsWorld2D_private::DrawCircle(const b2Vec2& center, float32 radius, const b2Color& color)
{
    if (!debugRenderer_)
        return;

    Vector3 p = ToVector3(center);
    Color c = ToColor(color);
    for (unsigned i = 0; i < 360; i += 30)
    {
        unsigned j = i + 30;
        float x1 = radius * Cos((float)i);
        float y1 = radius * Sin((float)i);
        float x2 = radius * Cos((float)j);
        float y2 = radius * Sin((float)j);

        debugRenderer_->AddLine(p + Vector3(x1, y1, 0.0f), p + Vector3(x2, y2, 0.0f), c, debugDepthTest_);
    }
}

extern LUTEFISK3D_EXPORT const float PIXEL_SIZE;

void PhysicsWorld2D_private::DrawPoint(const b2Vec2& p, float32 size, const b2Color& color)
{
    DrawSolidCircle(p, size * 0.5f * PIXEL_SIZE, b2Vec2(), color);
}

void PhysicsWorld2D_private::DrawSolidCircle(const b2Vec2& center, float32 radius, const b2Vec2& axis, const b2Color& color)
{
    if (!debugRenderer_)
        return;

    Vector3 p = ToVector3(center);
    Color c(color.r, color.g, color.b, 0.5f);

    for (unsigned i = 0; i < 360; i += 30)
    {
        unsigned j = i + 30;
        float x1 = radius * Cos((float)i);
        float y1 = radius * Sin((float)i);
        float x2 = radius * Cos((float)j);
        float y2 = radius * Sin((float)j);

        debugRenderer_->AddTriangle(p, p + Vector3(x1, y1, 0.0f), p + Vector3(x2, y2, 0.0f), c, debugDepthTest_);
    }
}

void PhysicsWorld2D_private::DrawSegment(const b2Vec2& p1, const b2Vec2& p2, const b2Color& color)
{
    if (debugRenderer_)
        debugRenderer_->AddLine(ToVector3(p1), ToVector3(p2), ToColor(color), debugDepthTest_);
}

void PhysicsWorld2D_private::DrawTransform(const b2Transform& xf)
{
    if (!debugRenderer_)
        return;

    const float32 axisScale = 0.4f;

    b2Vec2 p1 = xf.p, p2;
    p2 = p1 + axisScale * xf.q.GetXAxis();
    debugRenderer_->AddLine(Vector3(p1.x, p1.y, 0.0f), Vector3(p2.x, p2.y, 0.0f), Color::RED, debugDepthTest_);

    p2 = p1 + axisScale * xf.q.GetYAxis();
    debugRenderer_->AddLine(Vector3(p1.x, p1.y, 0.0f), Vector3(p2.x, p2.y, 0.0f), Color::GREEN, debugDepthTest_);
}

void PhysicsWorld2D::Update(float timeStep)
{
    URHO3D_PROFILE(UpdatePhysics2D);

    pre_step.Emit(this,timeStep);

    PhysicsWorld2D_private *tgt = static_cast <PhysicsWorld2D_private *>(privateData_);
    tgt->physicsStepping_ = true;
    world_->Step(timeStep, velocityIterations_, positionIterations_);
    tgt->physicsStepping_ = false;

    // Apply world transforms. Unparented transforms first
    for (auto iter = rigidBodies_.begin(); iter != rigidBodies_.end();)
    {
        if (*iter)
        {
            (*iter)->ApplyWorldTransform();
            ++iter;
        }
        else
        {
            // Erase possible stale weak pointer
            iter = rigidBodies_.erase(iter);
        }
    }
    // Apply delayed (parented) world transforms now, if any
    while (!delayedWorldTransforms_.empty())
    {
        for (auto i = delayedWorldTransforms_.begin(); i != delayedWorldTransforms_.end();)
        {
            const DelayedWorldTransform2D& transform = MAP_VALUE(i);

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
    tgt->SendBeginContactEvents();
    tgt->SendEndContactEvents();

    post_step.Emit(this,timeStep);
}

void PhysicsWorld2D::DrawDebugGeometry()
{
    DebugRenderer* debug = GetComponent<DebugRenderer>();
    if (debug)
        DrawDebugGeometry(debug, false);
}

void PhysicsWorld2D::SetUpdateEnabled(bool enable)
{
    updateEnabled_ = enable;
}

void PhysicsWorld2D::SetDrawShape(bool drawShape)
{
    PhysicsWorld2D_private *tgt = static_cast <PhysicsWorld2D_private *>(privateData_);
    tgt->SetDrawShape(drawShape);
}

void PhysicsWorld2D::SetDrawJoint(bool drawJoint)
{
    PhysicsWorld2D_private *tgt = static_cast <PhysicsWorld2D_private *>(privateData_);
    tgt->SetDrawJoint(drawJoint);
}

void PhysicsWorld2D::SetDrawAabb(bool drawAabb)
{
    PhysicsWorld2D_private *tgt = static_cast <PhysicsWorld2D_private *>(privateData_);
    tgt->SetDrawAabb(drawAabb);
}

void PhysicsWorld2D::SetDrawPair(bool drawPair)
{
    PhysicsWorld2D_private *tgt = static_cast <PhysicsWorld2D_private *>(privateData_);
    tgt->SetDrawAabb(drawPair);
}

void PhysicsWorld2D::SetDrawCenterOfMass(bool drawCenterOfMass)
{
    PhysicsWorld2D_private *tgt = static_cast <PhysicsWorld2D_private *>(privateData_);
    tgt->SetDrawAabb(drawCenterOfMass);
}

void PhysicsWorld2D::SetAllowSleeping(bool enable)
{
    world_->SetAllowSleeping(enable);
}

void PhysicsWorld2D::SetWarmStarting(bool enable)
{
    world_->SetWarmStarting(enable);
}

void PhysicsWorld2D::SetContinuousPhysics(bool enable)
{
    world_->SetContinuousPhysics(enable);
}

void PhysicsWorld2D::SetSubStepping(bool enable)
{
    world_->SetSubStepping(enable);
}

void PhysicsWorld2D::SetGravity(const Vector2& gravity)
{
    gravity_ = gravity;

    world_->SetGravity(ToB2Vec2(gravity_));
}

void PhysicsWorld2D::SetAutoClearForces(bool enable)
{
    world_->SetAutoClearForces(enable);
}

void PhysicsWorld2D::SetVelocityIterations(int velocityIterations)
{
    velocityIterations_ = velocityIterations;
}

void PhysicsWorld2D::SetPositionIterations(int positionIterations)
{
    positionIterations_ = positionIterations;
}

void PhysicsWorld2D::AddRigidBody(RigidBody2D* rigidBody)
{
    if (!rigidBody)
        return;

    WeakPtr<RigidBody2D> rigidBodyPtr(rigidBody);
    rigidBodies_.insert(rigidBodyPtr);
}

void PhysicsWorld2D::RemoveRigidBody(RigidBody2D* rigidBody)
{
    if (!rigidBody)
        return;

    WeakPtr<RigidBody2D> rigidBodyPtr(rigidBody);
    rigidBodies_.erase(rigidBodyPtr);
}
void PhysicsWorld2D::AddDelayedWorldTransform(const DelayedWorldTransform2D& transform)
{
    delayedWorldTransforms_[transform.rigidBody_] = transform;
}

// Ray cast call back class.
class RayCastCallback : public b2RayCastCallback
{
public:
    // Construct.
    RayCastCallback(std::vector<PhysicsRaycastResult2D>& results, const Vector2& startPoint, unsigned collisionMask) :
        results_(results),
        startPoint_(startPoint),
        collisionMask_(collisionMask)
    {
    }

    // Called for each fixture found in the query.
    virtual float32 ReportFixture(b2Fixture* fixture, const b2Vec2& point, const b2Vec2& normal, float32 fraction)
    {
        // Ignore sensor
        if (fixture->IsSensor())
            return true;

        if ((fixture->GetFilterData().maskBits & collisionMask_) == 0)
            return true;

        PhysicsRaycastResult2D result;
        result.position_ = ToVector2(point);
        result.normal_ = ToVector2(normal);
        result.distance_ = (result.position_ - startPoint_).Length();
        result.body_ = (RigidBody2D*)(fixture->GetBody()->GetUserData());

        results_.push_back(result);
        return true;
    }

protected:
    // Physics raycast results.
    std::vector<PhysicsRaycastResult2D>& results_;
    // Start point.
    Vector2 startPoint_;
    // Collision mask.
    unsigned collisionMask_;
};

void PhysicsWorld2D::Raycast(std::vector<PhysicsRaycastResult2D>& results, const Vector2& startPoint, const Vector2& endPoint, unsigned collisionMask)
{
    results.clear();

    RayCastCallback callback(results, startPoint, collisionMask);
    world_->RayCast(&callback, ToB2Vec2(startPoint), ToB2Vec2(endPoint));
}

// Single ray cast call back class.
class SingleRayCastCallback : public b2RayCastCallback
{
public:
    // Construct.
    SingleRayCastCallback(PhysicsRaycastResult2D& result, const Vector2& startPoint, unsigned collisionMask) :
        result_(result),
        startPoint_(startPoint),
        collisionMask_(collisionMask),
        minDistance_(M_INFINITY)
    {
    }

    // Called for each fixture found in the query.
    virtual float32 ReportFixture(b2Fixture* fixture, const b2Vec2& point, const b2Vec2& normal, float32 fraction)
    {
        // Ignore sensor
        if (fixture->IsSensor())
            return true;

        if ((fixture->GetFilterData().maskBits & collisionMask_) == 0)
            return true;

        float distance = (ToVector2(point)- startPoint_).Length();
        if (distance < minDistance_)
        {
            minDistance_ = distance;

            result_.position_ = ToVector2(point);
            result_.normal_ = ToVector2(normal);
            result_.distance_ = distance;
            result_.body_ = (RigidBody2D*)(fixture->GetBody()->GetUserData());
        }

        return true;
    }

private:
    // Physics raycast result.
    PhysicsRaycastResult2D& result_;
    // Start point.
    Vector2 startPoint_;
    // Collision mask.
    unsigned collisionMask_;
    // Minimum distance.
    float minDistance_;
};

void PhysicsWorld2D::RaycastSingle(PhysicsRaycastResult2D& result, const Vector2& startPoint, const Vector2& endPoint, unsigned collisionMask)
{
    result.body_ = nullptr;

    SingleRayCastCallback callback(result, startPoint, collisionMask);
    world_->RayCast(&callback, ToB2Vec2(startPoint), ToB2Vec2(endPoint));
}

// Point query callback class.
class PointQueryCallback : public b2QueryCallback
{
public:
    // Construct.
    PointQueryCallback(const b2Vec2& point, unsigned collisionMask) :
        point_(point),
        collisionMask_(collisionMask),
        rigidBody_(nullptr)
    {
    }

    // Called for each fixture found in the query AABB.
    virtual bool ReportFixture(b2Fixture* fixture) override
    {
        // Ignore sensor
        if (fixture->IsSensor())
            return true;

        if ((fixture->GetFilterData().maskBits & collisionMask_) == 0)
            return true;

        if (fixture->TestPoint(point_))
        {
            rigidBody_ = (RigidBody2D*)(fixture->GetBody()->GetUserData());
            return false;
        }

        return true;
    }

    // Return rigid body.
    RigidBody2D* GetRigidBody() const { return rigidBody_; }

private:
    // Point.
    b2Vec2 point_;
    // Collision mask.
    unsigned collisionMask_;
    // Rigid body.
    RigidBody2D* rigidBody_;
};

RigidBody2D* PhysicsWorld2D::GetRigidBody(const Vector2& point, unsigned collisionMask)
{
    PointQueryCallback callback(ToB2Vec2(point), collisionMask);

    b2AABB b2Aabb;
    Vector2 delta(M_EPSILON, M_EPSILON);
    b2Aabb.lowerBound = ToB2Vec2(point - delta);
    b2Aabb.upperBound = ToB2Vec2(point + delta);

    world_->QueryAABB(&callback, b2Aabb);
    return callback.GetRigidBody();
}

RigidBody2D* PhysicsWorld2D::GetRigidBody(int screenX, int screenY, unsigned collisionMask)
{
    Renderer* renderer = context_->m_Renderer.get();
    for (unsigned i = 0; i  < renderer->GetNumViewports(); ++i)
    {
        Viewport* viewport = renderer->GetViewport(i);
        // Find a viewport with same scene
        if (viewport && viewport->GetScene() == GetScene())
        {
            Vector3 worldPoint = viewport->ScreenToWorldPoint(screenX, screenY, 0.0f);
            return GetRigidBody(Vector2(worldPoint.x_, worldPoint.y_), collisionMask);
        }
    }

    return nullptr;
}

// Aabb query callback class.
class AabbQueryCallback : public b2QueryCallback
{
public:
    // Construct.
    AabbQueryCallback(std::vector<RigidBody2D*>& results, unsigned collisionMask) :
        results_(results),
        collisionMask_(collisionMask)
    {
    }

    // Called for each fixture found in the query AABB.
    bool ReportFixture(b2Fixture* fixture) final
    {
        // Ignore sensor
        if (fixture->IsSensor())
            return true;

        if ((fixture->GetFilterData().maskBits & collisionMask_) == 0)
            return true;

        results_.push_back((RigidBody2D*)(fixture->GetBody()->GetUserData()));
        return true;
    }

private:
    // Results.
    std::vector<RigidBody2D*>& results_;
    // Collision mask.
    unsigned collisionMask_;
};

void PhysicsWorld2D::GetRigidBodies(std::vector<RigidBody2D*>& results, const Rect& aabb, unsigned collisionMask)
{
    AabbQueryCallback callback(results, collisionMask);

    b2AABB b2Aabb;
    Vector2 delta(M_EPSILON, M_EPSILON);
    b2Aabb.lowerBound = ToB2Vec2(aabb.min_ - delta);
    b2Aabb.upperBound = ToB2Vec2(aabb.max_ + delta);

    world_->QueryAABB(&callback, b2Aabb);
}

bool PhysicsWorld2D::GetDrawShape() const
{
    return static_cast <PhysicsWorld2D_private *>(privateData_)->GetDrawShape();
}

bool PhysicsWorld2D::GetDrawJoint() const
{
    return static_cast <PhysicsWorld2D_private *>(privateData_)->GetDrawJoint();
}

bool PhysicsWorld2D::GetDrawAabb() const
{
    return static_cast <PhysicsWorld2D_private *>(privateData_)->GetDrawAabb();
}

bool PhysicsWorld2D::GetDrawPair() const
{
    return static_cast <PhysicsWorld2D_private *>(privateData_)->GetDrawPair();
}

bool PhysicsWorld2D::GetDrawCenterOfMass() const
{
    return static_cast <PhysicsWorld2D_private *>(privateData_)->GetDrawCenterOfMass();
}

bool PhysicsWorld2D::GetAllowSleeping() const
{
    return world_->GetAllowSleeping();
}

bool PhysicsWorld2D::GetWarmStarting() const
{
    return world_->GetWarmStarting();
}

bool PhysicsWorld2D::GetContinuousPhysics() const
{
    return world_->GetContinuousPhysics();
}

bool PhysicsWorld2D::GetSubStepping() const
{
    return world_->GetSubStepping();
}

bool PhysicsWorld2D::GetAutoClearForces() const
{
    return world_->GetAutoClearForces();
}

void PhysicsWorld2D::OnSceneSet(Scene *scene)
{
    // Subscribe to the scene subsystem update, which will trigger the physics simulation step
    if (scene)
        scene->sceneSubsystemUpdate.Connect(this,&PhysicsWorld2D::HandleSceneSubsystemUpdate);
    else {
        assert(GetScene());
        GetScene()->sceneSubsystemUpdate.Disconnect(this,&PhysicsWorld2D::HandleSceneSubsystemUpdate);
    }
}

void PhysicsWorld2D::HandleSceneSubsystemUpdate(Scene *,float ts)
{
    if (!updateEnabled_)
        return;
    Update(ts);
}






}
