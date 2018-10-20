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

#include "Lutefisk3D/Core/Object.h"
#include "Lutefisk3D/Engine/jlsignal/Signal.h"

namespace Urho3D
{
class View;
class Texture;
class RenderSurface;
class Scene;
class Camera;
struct GraphicsSignals {
    /// New screen mode set.
    /// int Width,int Height,bool Fullscreen,bool Borderless,bool Resizable,bool HighDPI,int Monitor,int RefreshRate
    jl::Signal<int,int,bool,bool,bool,bool,int,int> newScreenMode;
    /// Window position changed.
    /// X,Y
    jl::Signal<int,int> windowPos;
    /// Request for queuing rendersurfaces either in manual or always-update mode.
    jl::Signal<> renderSurfaceUpdate;
    /// Frame rendering started.
    jl::Signal<> beginRendering;
    /// Frame rendering ended.
    jl::Signal<> endRendering;
    /// Update of a view started.
    jl::Signal<View *,Texture *,RenderSurface *,Scene *,Camera *> beginViewUpdate;
    /// Update of a view ended.
    jl::Signal<View *,Texture *,RenderSurface *,Scene *,Camera *> endViewUpdate;
    /// Update of a view started.
    jl::Signal<View *,Texture *,RenderSurface *,Scene *,Camera *> beginViewRender;
    /// A view has allocated its screen buffers for rendering. They can be accessed now with View::FindNamedTexture().
    jl::Signal<View *,Texture *,RenderSurface *,Scene *,Camera *> viewBuffersReady;
    /// A view has set global shader parameters for a new combination of vertex/pixel shaders. Custom global parameters can now be set.
    jl::Signal<View *,Texture *,RenderSurface *,Scene *,Camera *> viewGlobalShaderParameters;
    /// Render of a view ended. Its screen buffers are still accessible if needed.
    jl::Signal<View *,Texture *,RenderSurface *,Scene *,Camera *> endViewRender;

    /// Render of all views is finished for the frame.
    jl::Signal<> endAllViewsRender;
    /// A render path event has occurred.
    /// Name
    jl::Signal<const QString &> renderPathEvent;
    /// Graphics context has been lost. Some or all (depending on the API) GPU objects have lost their contents.
    jl::Signal<> deviceLost;
    /// Graphics context has been recreated after being lost. GPU objects in the "data lost" state can be restored now.
    jl::Signal<> deviceReset;

    void init(jl::ScopedAllocator *allocator)
    {
        newScreenMode.SetAllocator(allocator);
        windowPos.SetAllocator(allocator);
        renderSurfaceUpdate.SetAllocator(allocator);
        beginRendering.SetAllocator(allocator);
        endRendering.SetAllocator(allocator);
        beginViewUpdate.SetAllocator(allocator);
        endViewUpdate.SetAllocator(allocator);
        beginViewRender.SetAllocator(allocator);
        viewBuffersReady.SetAllocator(allocator);
        viewGlobalShaderParameters.SetAllocator(allocator);
        endViewRender.SetAllocator(allocator);

        endAllViewsRender.SetAllocator(allocator);
        renderPathEvent.SetAllocator(allocator);
        deviceLost.SetAllocator(allocator);
        deviceReset.SetAllocator(allocator);
    }
};
extern GraphicsSignals LUTEFISK3D_EXPORT g_graphicsSignals;


}
