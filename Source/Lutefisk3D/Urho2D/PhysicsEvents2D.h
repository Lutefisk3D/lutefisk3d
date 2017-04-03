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

#include "../Core/Object.h"

// For prestep / poststep events, which are the same for 2D and 3D physics. The events themselves don't depend
// on whether 3D physics support or Bullet has been compiled in.
#include "../Physics/PhysicsEvents.h"
namespace Urho3D
{


/// Physics begin contact. Global event sent by PhysicsWorld2D.
URHO3D_EVENT(E_PHYSICSBEGINCONTACT2D, PhysicsBeginContact2D)
{
    URHO3D_PARAM(P_WORLD, World);                  // PhysicsWorld2D pointer
    URHO3D_PARAM(P_BODYA, BodyA);                  // RigidBody2D pointer
    URHO3D_PARAM(P_BODYB, BodyB);                  // RigidBody2D pointer
    URHO3D_PARAM(P_NODEA, NodeA);                  // Node pointer
    URHO3D_PARAM(P_NODEB, NodeB);                  // Node pointer
    URHO3D_PARAM(P_CONTACT, Contact);              // b2Contact pointer
}

/// Physics end contact. Global event sent by PhysicsWorld2D.
URHO3D_EVENT(E_PHYSICSENDCONTACT2D, PhysicsEndContact2D)
{
    URHO3D_PARAM(P_WORLD, World);                  // PhysicsWorld2D pointer
    URHO3D_PARAM(P_BODYA, BodyA);                  // RigidBody2D pointer
    URHO3D_PARAM(P_BODYB, BodyB);                  // RigidBody2D pointer
    URHO3D_PARAM(P_NODEA, NodeA);                  // Node pointer
    URHO3D_PARAM(P_NODEB, NodeB);                  // Node pointer
    URHO3D_PARAM(P_CONTACT, Contact);              // b2Contact pointer
}

/// Node begin contact. Sent by scene nodes participating in a collision.
URHO3D_EVENT(E_NODEBEGINCONTACT2D, NodeBeginContact2D)
{
    URHO3D_PARAM(P_BODY, Body);                    // RigidBody2D pointer
    URHO3D_PARAM(P_OTHERNODE, OtherNode);          // Node pointer
    URHO3D_PARAM(P_OTHERBODY, OtherBody);          // RigidBody2D pointer
    URHO3D_PARAM(P_CONTACT, Contact);              // b2Contact pointer
}

/// Node end contact. Sent by scene nodes participating in a collision.
URHO3D_EVENT(E_NODEENDCONTACT2D, NodeEndContact2D)
{
    URHO3D_PARAM(P_BODY, Body);                    // RigidBody2D pointer
    URHO3D_PARAM(P_OTHERNODE, OtherNode);          // Node pointer
    URHO3D_PARAM(P_OTHERBODY, OtherBody);          // RigidBody2D pointer
    URHO3D_PARAM(P_CONTACT, Contact);              // b2Contact pointer
}

}
