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

#include "Lutefisk3D/Container/HashMap.h"
#include "Lutefisk3D/Core/Object.h"
#include "jlsignal/Signal.h"
namespace Urho3D
{
class Component; class PhysicsWorld;
class Node;
class RigidBody;
struct PhysicsSignals
{
    jl::ScopedAllocator *m_allocator;
    // World/Node/Node/Body/Body/Trigger/Contacts(Buffer containing position (Vector3), normal (Vector3), distance
    // (float), impulse (float) for each contact)
    using PhysicsCollision =
        jl::Signal<PhysicsWorld *, Node *, Node *, RigidBody *, RigidBody *, bool, const std::vector<uint8_t> &>;
    using PhysicsCollisionEnd = jl::Signal<PhysicsWorld *, Node *, Node *, RigidBody *, RigidBody *, bool>;
    using NodeCollision       = jl::Signal<RigidBody *, Node *, RigidBody *, bool, const std::vector<uint8_t> &>;
    using NodeCollisionEnd    = jl::Signal<RigidBody *, Node *, RigidBody *, bool>;

    // Component * is used here since both PhysicsWorld and PhysicsWorld2D can emit this signal
    /// Physics world is about to be stepped.
    jl::Signal<Component *, float> pre_step;
    /// Physics world has been stepped.
    jl::Signal<Component *, float> post_step;
    /// Physics collision started. Global event sent by the PhysicsWorld.
    PhysicsCollision collisionStart;
    /// Physics collision ongoing. Global event sent by the PhysicsWorld.
    PhysicsCollision collision;
    /// Physics collision ended. Global event sent by the PhysicsWorld.
    PhysicsCollisionEnd collisionEnd;
    /// Node's physics collision started. Source is a nodes participating in a collision.
    HashMap<void *, NodeCollision> nodeCollisionStart;
    /// Node's physics collision ongoing. Sent by scene nodes participating in a collision.
    HashMap<void *, NodeCollision> nodeCollision;
    /// Node's physics collision ended. Sent by scene nodes participating in a collision.
    HashMap<void *, NodeCollisionEnd> nodeCollisionEnd;

    template <class Y,class X>
    void connectNodeCollision(void *src, Y *pObject,
                              void (X::*fpMethod)(RigidBody *, Node *, RigidBody *, bool, const std::vector<uint8_t> &))
    {
        auto iter = nodeCollision.find(src);
        if (iter == nodeCollision.end())
        {
            nodeCollision[src].SetAllocator(m_allocator);
        }
        nodeCollision[src].Connect(pObject, fpMethod);
    }
    template <class Y>
    void disconnectNodeCollision(void *src, Y *pObject)
    {
        auto iter = nodeCollision.find(src);
        if (iter != nodeCollision.end())
        {
            iter->second.Disconnect(pObject);
        }
    }
    void init(jl::ScopedAllocator *allocator)
    {
        m_allocator = allocator;
        pre_step.SetAllocator(m_allocator);
        post_step.SetAllocator(m_allocator);
        collisionStart.SetAllocator(m_allocator);
        collision.SetAllocator(m_allocator);
        collisionEnd.SetAllocator(m_allocator);
    }
};
}
