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

#include "Lutefisk3D/Core/Object.h"
#include "Lutefisk3D/Engine/jlsignal/Signal.h"

// For prestep / poststep events, which are the same for 2D and 3D physics. The events themselves don't depend
// on whether 3D physics support or Bullet has been compiled in.
#include "Lutefisk3D/Physics/PhysicsEvents.h"

namespace Urho3D
{
class PhysicsWorld2D;
class RigidBody2D;
class Node;
class CollisionShape2D;
struct Physics2DWorldSignals
{
    /// Node update contact. Sent by scene nodes participating in a collision.
    using ContactsUpdated = jl::Signal<RigidBody2D *, Node *, RigidBody2D *, const std::vector<uint8_t> &,
                                       CollisionShape2D *, CollisionShape2D *, bool &>;
    /// Node begin contact. Sent by scene nodes participating in a collision.
    using ContactStarted = jl::Signal<RigidBody2D *, Node *, RigidBody2D *,  const std::vector<uint8_t> &,
    CollisionShape2D *, CollisionShape2D *>;
    /// Node end contact. Sent by scene nodes participating in a collision.
    using NodeContactEnded = jl::Signal<RigidBody2D *, Node *, RigidBody2D *,  const std::vector<uint8_t> &,
    CollisionShape2D *, CollisionShape2D *>;

    // vector<> is a buffer containing position (Vector2), normal (Vector2), negative overlap distance (float). Normal
    // is the same for all points.
    /// Physics update contact. Global event sent by PhysicsWorld2D.
    jl::Signal<PhysicsWorld2D *, RigidBody2D *, RigidBody2D *, Node *, Node *, const std::vector<uint8_t> &,
               CollisionShape2D *, CollisionShape2D *, bool &/*enabled inout*/>
        update_contact;
    /// Physics begin contact. Global event sent by PhysicsWorld2D.
    jl::Signal<PhysicsWorld2D *, RigidBody2D *, RigidBody2D *, Node *, Node *, const std::vector<uint8_t> &,
               CollisionShape2D *, CollisionShape2D *>
        begin_contact;
    /// Physics end contact. Global event sent by PhysicsWorld2D.
    jl::Signal<PhysicsWorld2D *, RigidBody2D *, RigidBody2D *, Node *, Node *, const std::vector<uint8_t> &,
               CollisionShape2D *, CollisionShape2D *>
        end_contact;
};
struct Physics2DNodeSignals {
    /// Node update contact. Sent by scene nodes participating in a collision.
    jl::Signal<RigidBody2D *, Node *, RigidBody2D *,  const std::vector<uint8_t> &,
               CollisionShape2D *, CollisionShape2D *, bool &/*enabled inout*/>
        updated_contacts;
    /// Node begin contact. Sent by scene nodes participating in a collision.
    jl::Signal<RigidBody2D *, Node *, RigidBody2D *,  const std::vector<uint8_t> &,
               CollisionShape2D *, CollisionShape2D *>
        begin_contact;
    /// Node end contact. Sent by scene nodes participating in a collision.
    jl::Signal<RigidBody2D *, Node *, RigidBody2D *,  const std::vector<uint8_t> &,
               CollisionShape2D *, CollisionShape2D *>
        end_contact;
};
}
