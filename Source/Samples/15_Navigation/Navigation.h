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

#include "../Sample.h"
#include <deque>

namespace Urho3D
{

class Drawable;
class Node;
class Scene;

}

/// Navigation example.
/// This sample demonstrates:
///     - Generating a navigation mesh into the scene
///     - Performing path queries to the navigation mesh
///     - Rebuilding the navigation mesh partially when adding or removing objects
///     - Visualizing custom debug geometry
///     - Raycasting drawable components
///     - Making a node follow the Detour path
class Navigation : public Sample
{
public:
    /// Construct.
    Navigation(Context* context);

    /// Setup after engine initialization and before running the main loop.
    virtual void Start() override;

private:
    /// Construct the scene content.
    void CreateScene();
    /// Construct user interface elements.
    void CreateUI();
    /// Set up a viewport for displaying the scene.
    void SetupViewport();
    /// Subscribe to application-wide logic update and post-render update events.
    void SubscribeToEvents();
    /// Read input and moves the camera.
    void MoveCamera(float timeStep);
    /// Set path start or end point.
    void SetPathPoint();
    /// Add or remove object.
    void AddOrRemoveObject();
    /// Create a mushroom object at position.
    Node* CreateMushroom(const Vector3& pos);
    /// Utility function to raycast to the cursor position. Return true if hit
    bool Raycast(float maxDistance, Vector3& hitPos, Drawable*& hitDrawable);
    /// Make Jack follow the Detour path.
    void FollowPath(float timeStep);
    /// Handle the logic update event.
    void HandleUpdate(float timeStep);
    /// Handle the post-render update event.
    void HandlePostRenderUpdate(float);

    /// Last calculated path.
    std::deque<Vector3> currentPath_;
    /// Path end position.
    Vector3 endPos_;
    /// Jack scene node.
    SharedPtr<Node> jackNode_;
    /// Flag for drawing debug geometry.
    bool drawDebug_;
};
