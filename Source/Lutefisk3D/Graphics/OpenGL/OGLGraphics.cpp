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

#include "../AnimatedModel.h"
#include "../Animation.h"
#include "../AnimationController.h"
#include "../BillboardSet.h"
#include "../Camera.h"
#include "../ConstantBuffer.h"
#include "../../Core/Context.h"
#include "../../Core/StringUtils.h"
#include "../CustomGeometry.h"
#include "../DebugRenderer.h"
#include "../DecalSet.h"
#include "../../IO/File.h"
#include "../Graphics.h"
#include "../GraphicsEvents.h"
#include "../GraphicsImpl.h"
#include "../IndexBuffer.h"
#include "../../IO/Log.h"
#include "../Material.h"
#include "../../Core/Mutex.h"
#include "../Octree.h"
#include "../ParticleEffect.h"
#include "../ParticleEmitter.h"
#include "../../Core/ProcessUtils.h"
#include "../../Core/Profiler.h"
#include "../RenderSurface.h"
#include "../../Resource/ResourceCache.h"
#include "../Shader.h"
#include "../ShaderPrecache.h"
#include "../ShaderProgram.h"
#include "../ShaderVariation.h"
#include "../Skybox.h"
#include "../StaticModelGroup.h"
#include "../Technique.h"
#include "../Terrain.h"
#include "../TerrainPatch.h"
#include "../Texture2D.h"
#include "../Texture3D.h"
#include "../TextureCube.h"
#include "../VertexBuffer.h"
#include "../Zone.h"

#include "glbinding/Binding.h"
#include "glbinding/ContextInfo.h"
#include "glbinding/Version.h"
#include "glbinding/gl/extension.h"
#include "glbinding/gl33ext/enum.h"
#include "glbinding/gl33ext/bitfield.h"
#include "glbinding/gl33ext/boolean.h"

#include <stdio.h>
using namespace gl;
#ifdef WIN32
// Prefer the high-performance GPU on switchable GPU systems
#include <windows.h>
extern "C"
{
    __declspec(dllexport) DWORD NvOptimusEnablement = 1;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

namespace Urho3D
{

static const GLenum glCmpFunc[] =
{
    GL_ALWAYS,
    GL_EQUAL,
    GL_NOTEQUAL,
    GL_LESS,
    GL_LEQUAL,
    GL_GREATER,
    GL_GEQUAL
};

static const GLenum glSrcBlend[] =
{
    GL_ONE,
    GL_ONE,
    GL_DST_COLOR,
    GL_SRC_ALPHA,
    GL_SRC_ALPHA,
    GL_ONE,
    GL_ONE_MINUS_DST_ALPHA,
    GL_ONE,
    GL_SRC_ALPHA
};

static const GLenum glDestBlend[] =
{
    GL_ZERO,
    GL_ONE,
    GL_ZERO,
    GL_ONE_MINUS_SRC_ALPHA,
    GL_ONE,
    GL_ONE_MINUS_SRC_ALPHA,
    GL_DST_ALPHA,
    GL_ONE,
    GL_ONE
};

static const GLenum glBlendOp[] =
{
    GL_FUNC_ADD,
    GL_FUNC_ADD,
    GL_FUNC_ADD,
    GL_FUNC_ADD,
    GL_FUNC_ADD,
    GL_FUNC_ADD,
    GL_FUNC_ADD,
    GL_FUNC_REVERSE_SUBTRACT,
    GL_FUNC_REVERSE_SUBTRACT
};

#ifndef GL_ES_VERSION_2_0
static const GLenum glFillMode[] =
{
    GL_FILL,
    GL_LINE,
    GL_POINT
};

static const GLenum glStencilOps[] =
{
    GL_KEEP,
    GL_ZERO,
    GL_REPLACE,
    GL_INCR_WRAP,
    GL_DECR_WRAP
};
#endif

// Remap vertex attributes on OpenGL so that all usually needed attributes including skinning fit to the first 8.
// This avoids a skinning bug on GLES2 devices which only support 8.
static const unsigned glVertexAttrIndex[] =
{
    0, 1, 2, 3, 4, 8, 9, 5, 6, 7, 10, 11, 12, 13
};


static QString extensions;

bool CheckExtension(const QString& name)
{
    if (extensions.isEmpty())
        extensions = (const char*)glGetString(GL_EXTENSIONS);
    return extensions.contains(name);
}

static void GetGLPrimitiveType(unsigned elementCount, PrimitiveType type, unsigned& primitiveCount, GLenum& glPrimitiveType)
{
    switch (type)
    {
    case TRIANGLE_LIST:
        primitiveCount = elementCount / 3;
        glPrimitiveType = GL_TRIANGLES;
        break;

    case LINE_LIST:
        primitiveCount = elementCount / 2;
        glPrimitiveType = GL_LINES;
        break;

    case POINT_LIST:
        primitiveCount = elementCount;
        glPrimitiveType = GL_POINTS;
        break;

    case TRIANGLE_STRIP:
        primitiveCount = elementCount - 2;
        glPrimitiveType = GL_TRIANGLE_STRIP;
        break;

    case LINE_STRIP:
        primitiveCount = elementCount - 1;
        glPrimitiveType = GL_LINE_STRIP;
        break;

    case TRIANGLE_FAN:
        primitiveCount = elementCount - 2;
        glPrimitiveType = GL_TRIANGLE_FAN;
        break;
    }
}
const Vector2 Graphics::pixelUVOffset(0.0f, 0.0f);
bool Graphics::gl3Support = false;

Graphics::Graphics(Context* context_) :
    Object(context_),
    impl_(new GraphicsImpl()),
    windowIcon_(nullptr),
    externalWindow_(nullptr),
    width_(0),
    height_(0),
    position_(SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED),
    multiSample_(1),
    fullscreen_(false),
    borderless_(false),
    resizable_(false),
    vsync_(false),
    tripleBuffer_(false),
    sRGB_(false),
    forceGL2_(false),
    instancingSupport_(false),
    lightPrepassSupport_(false),
    deferredSupport_(false),
    anisotropySupport_(false),
    dxtTextureSupport_(false),
    etcTextureSupport_(false),
    pvrtcTextureSupport_(false),
    sRGBSupport_(false),
    sRGBWriteSupport_(false),
    numPrimitives_(0),
    numBatches_(0),
    maxScratchBufferRequest_(0),
    dummyColorFormat_(GL_NONE),
    shadowMapFormat_(GL_DEPTH_COMPONENT16),
    hiresShadowMapFormat_(GL_DEPTH_COMPONENT24),
    defaultTextureFilterMode_(FILTER_TRILINEAR),
    shaderPath_("Shaders/GLSL/"),
    shaderExtension_(".glsl"),
    orientations_("LandscapeLeft LandscapeRight"),
    apiName_("GL2")
{
    SetTextureUnitMappings();
    ResetCachedState();

    // Initialize SDL now. Graphics should be the first SDL-using subsystem to be created
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER | SDL_INIT_NOPARACHUTE);

    // Register Graphics library object factories
    RegisterGraphicsLibrary(context_);
}

Graphics::~Graphics()
{
    Close();

    delete impl_;
    impl_ = nullptr;

    // Shut down SDL now. Graphics should be the last SDL-using subsystem to be destroyed
    SDL_Quit();
}

void Graphics::SetExternalWindow(void* window)
{
    if (!impl_->window_)
        externalWindow_ = window;
    else
        URHO3D_LOGERROR("Window already opened, can not set external window");
}

void Graphics::SetWindowTitle(const QString& windowTitle)
{
    windowTitle_ = windowTitle;
    if (impl_->window_)
        SDL_SetWindowTitle(impl_->window_, qPrintable(windowTitle_));
}

void Graphics::SetWindowIcon(Image* windowIcon)
{
    windowIcon_ = windowIcon;
    if (impl_->window_)
        CreateWindowIcon();
}

void Graphics::SetWindowPosition(const IntVector2& position)
{
    if (impl_->window_)
        SDL_SetWindowPosition(impl_->window_, position.x_, position.y_);
    else
        position_ = position; // Sets as initial position for future window creation
}

void Graphics::SetWindowPosition(int x, int y)
{
    SetWindowPosition(IntVector2(x, y));
}

bool Graphics::SetMode(int width, int height, bool fullscreen, bool borderless, bool resizable, bool vsync, bool tripleBuffer, int multiSample)
{
    URHO3D_PROFILE(SetScreenMode);

    bool maximize = false;

    // Fullscreen or Borderless can not be resizable
    if (fullscreen || borderless)
        resizable = false;

    // Borderless cannot be fullscreen, they are mutually exclusive
    if (borderless)
        fullscreen = false;

    multiSample = Clamp(multiSample, 1, 16);

    if (IsInitialized() && width == width_ && height == height_ && fullscreen == fullscreen_ && borderless == borderless_ &&
            resizable == resizable_ && vsync == vsync_ && tripleBuffer == tripleBuffer_ && multiSample == multiSample_)
        return true;

    // If only vsync changes, do not destroy/recreate the context
    if (IsInitialized() && width == width_ && height == height_ && fullscreen == fullscreen_ && borderless == borderless_ &&
            resizable == resizable_ && tripleBuffer == tripleBuffer_ && multiSample == multiSample_ && vsync != vsync_)
    {
        SDL_GL_SetSwapInterval(vsync ? 1 : 0);
        vsync_ = vsync;
        return true;
    }

    // If zero dimensions in windowed mode, set windowed mode to maximize and set a predefined default restored window size.
    // If zero in fullscreen, use desktop mode
    if (!width || !height)
    {
        if (fullscreen || borderless)
        {
            SDL_DisplayMode mode;
            SDL_GetDesktopDisplayMode(0, &mode);
            width = mode.w;
            height = mode.h;
        }
        else
        {
            maximize = resizable;
            width = 1024;
            height = 768;
        }
    }

    // Check fullscreen mode validity (desktop only). Use a closest match if not found
#ifdef DESKTOP_GRAPHICS
    if (fullscreen)
    {
        std::vector<IntVector2> resolutions = GetResolutions();
        if (resolutions.empty())
            fullscreen = false;
        else
        {
            unsigned best = 0;
            unsigned bestError = M_MAX_UNSIGNED;

            for (unsigned i = 0; i < resolutions.size(); ++i)
            {
                unsigned error = Abs(resolutions[i].x_ - width) + Abs(resolutions[i].y_ - height);
                if (error < bestError)
                {
                    best = i;
                    bestError = error;
                }
            }

            width = resolutions[best].x_;
            height = resolutions[best].y_;
        }
    }
#endif

    QString extensions;

    // With an external window, only the size can change after initial setup, so do not recreate context
    if (!externalWindow_ || !impl_->context_)
    {
        // Close the existing window and OpenGL context, mark GPU objects as lost
        Release(false, true);

#ifdef IOS
        // On iOS window needs to be resizable to handle orientation changes properly
        resizable = true;
#endif

        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
#ifndef GL_ES_VERSION_2_0
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);

        if (externalWindow_)
            SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
        else
            SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 0);

        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
        if (!forceGL2_)
        {
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        }
        else
        {
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, 0);
        }
#else
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

        if (multiSample > 1)
        {
            SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
            SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, multiSample);
        }
        else
        {
            SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
            SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
        }

        int x = fullscreen ? 0 : position_.x_;
        int y = fullscreen ? 0 : position_.y_;

        unsigned flags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN;
        if (fullscreen)
            flags |= SDL_WINDOW_FULLSCREEN;
        if (resizable)
            flags |= SDL_WINDOW_RESIZABLE;
        if (borderless)
            flags |= SDL_WINDOW_BORDERLESS;

        SDL_SetHint(SDL_HINT_ORIENTATIONS, qPrintable(orientations_));

        for (;;)
        {
            if (!externalWindow_)
                impl_->window_ = SDL_CreateWindow(qPrintable(windowTitle_), x, y, width, height, flags);
            else
            {
#ifndef __EMSCRIPTEN__
                if (!impl_->window_)
                    impl_->window_ = SDL_CreateWindowFrom(externalWindow_);
                fullscreen = false;
#endif
            }

            if (impl_->window_)
                break;
            else
            {
                if (multiSample > 1)
                {
                    // If failed with multisampling, retry first without
                    multiSample = 1;
                    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
                    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
                }
                else
                {
                    URHO3D_LOGERROR(QString("Could not create window, root cause: '%1'").arg(SDL_GetError()));
                    return false;
                }
            }
        }

        CreateWindowIcon();

        if (maximize)
        {
            Maximize();
            SDL_GetWindowSize(impl_->window_, &width, &height);
        }

        // Create/restore context and GPU objects and set initial renderstate
        Restore();

        // Specific error message is already logged by Restore() when context creation or OpenGL extensions check fails
        if (!impl_->context_)
            return false;
    }

    // Set vsync
    SDL_GL_SetSwapInterval(vsync ? 1 : 0);

    // Store the system FBO on IOS now
#ifdef IOS
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, (GLint*)&impl_->systemFBO_);
#endif

    fullscreen_ = fullscreen;
    resizable_ = resizable;
    borderless_ = borderless;
    vsync_ = vsync;
    tripleBuffer_ = tripleBuffer;
    multiSample_ = multiSample;

    SDL_GetWindowSize(impl_->window_, &width_, &height_);
    if (!fullscreen)
        SDL_GetWindowPosition(impl_->window_, &position_.x_, &position_.y_);

    // Reset rendertargets and viewport for the new screen mode
    ResetRenderTargets();

    // Clear the initial window contents to black
    Clear(CLEAR_COLOR);
    SDL_GL_SwapWindow(impl_->window_);

    CheckFeatureSupport();

#ifdef LUTEFISK3D_LOGGING
    QString msg  = QString("Set screen mode %1x%2 %3").arg(width_).arg(height_).arg((fullscreen_ ? "fullscreen" : "windowed"));
    if (borderless_)
        msg.append(" borderless");
    if (resizable_)
        msg.append(" resizable");
    if (multiSample > 1)
        msg += QString(" multisample %1").arg(multiSample);
    URHO3D_LOGINFO(msg);
#endif

    using namespace ScreenMode;

    VariantMap& eventData = GetEventDataMap();
    eventData[P_WIDTH] = width_;
    eventData[P_HEIGHT] = height_;
    eventData[P_FULLSCREEN] = fullscreen_;
    eventData[P_RESIZABLE] = resizable_;
    eventData[P_BORDERLESS] = borderless_;
    SendEvent(E_SCREENMODE, eventData);

    return true;
}

bool Graphics::SetMode(int width, int height)
{
    return SetMode(width, height, fullscreen_, borderless_, resizable_, vsync_, tripleBuffer_, multiSample_);
}

void Graphics::SetSRGB(bool enable)
{
    enable &= sRGBWriteSupport_;

    if (enable != sRGB_)
    {
        sRGB_ = enable;
        impl_->fboDirty_ = true;
    }
}

void Graphics::SetFlushGPU(bool enable)
{
}

void Graphics::SetForceGL2(bool enable)
{
    if (IsInitialized())
    {
        URHO3D_LOGERROR("OpenGL 2 can only be forced before setting the initial screen mode");
        return;
    }

    forceGL2_ = enable;
}
void Graphics::SetOrientations(const QString& orientations)
{
    orientations_ = orientations.trimmed();
    SDL_SetHint(SDL_HINT_ORIENTATIONS, qPrintable(orientations_));
}

bool Graphics::ToggleFullscreen()
{
    return SetMode(width_, height_, !fullscreen_, borderless_, resizable_, vsync_, tripleBuffer_, multiSample_);
}

void Graphics::Close()
{
    if (!IsInitialized())
        return;

    // Actually close the window
    Release(true, true);
}

bool Graphics::TakeScreenShot(Image& destImage)
{
    URHO3D_PROFILE(TakeScreenShot);

    ResetRenderTargets();

    destImage.SetSize(width_, height_, 3);
    glReadPixels(0, 0, width_, height_, GL_RGB, GL_UNSIGNED_BYTE, destImage.GetData());
    // On OpenGL we need to flip the image vertically after reading
    destImage.FlipVertical();

    return true;
}

bool Graphics::BeginFrame()
{
    if (!IsInitialized() || IsDeviceLost())
        return false;

    // If using an external window, check it for size changes, and reset screen mode if necessary
    if (externalWindow_)
    {
        int width, height;

        SDL_GetWindowSize(impl_->window_, &width, &height);
        if (width != width_ || height != height_)
            SetMode(width, height);
    }

    // Re-enable depth test and depth func in case a third party program has modified it
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(glCmpFunc[depthTestMode_]);

    // Set default rendertarget and depth buffer
    ResetRenderTargets();

    // Cleanup textures from previous frame
    for (unsigned i = 0; i < MAX_TEXTURE_UNITS; ++i)
        SetTexture(i, nullptr);

    // Enable color and depth write
    SetColorWrite(true);
    SetDepthWrite(true);

    numPrimitives_ = 0;
    numBatches_ = 0;

    SendEvent(E_BEGINRENDERING);

    return true;
}

void Graphics::EndFrame()
{
    if (!IsInitialized())
        return;

    URHO3D_PROFILE(Present);

    SendEvent(E_ENDRENDERING);

    SDL_GL_SwapWindow(impl_->window_);

    // Clean up too large scratch buffers
    CleanupScratchBuffers();
}

void Graphics::Clear(unsigned flags, const Color& color, float depth, unsigned stencil)
{
    PrepareDraw();

#ifdef GL_ES_VERSION_2_0
    flags &= ~CLEAR_STENCIL;
#endif

    bool oldColorWrite = colorWrite_;
    bool oldDepthWrite = depthWrite_;

    if (flags & CLEAR_COLOR && !oldColorWrite)
        SetColorWrite(true);
    if (flags & CLEAR_DEPTH && !oldDepthWrite)
        SetDepthWrite(true);
    if (flags & CLEAR_STENCIL && stencilWriteMask_ != M_MAX_UNSIGNED)
        glStencilMask(M_MAX_UNSIGNED);

    ClearBufferMask glFlags(ClearBufferMask::GL_NONE_BIT);
    if (flags & CLEAR_COLOR)
    {
        glFlags |= GL_COLOR_BUFFER_BIT;
        glClearColor(color.r_, color.g_, color.b_, color.a_);
    }
    if (flags & CLEAR_DEPTH)
    {
        glFlags |= GL_DEPTH_BUFFER_BIT;
        glClearDepth(depth);
    }
    if (flags & CLEAR_STENCIL)
    {
        glFlags |= GL_STENCIL_BUFFER_BIT;
        glClearStencil(stencil);
    }

    // If viewport is less than full screen, set a scissor to limit the clear
    /// \todo Any user-set scissor test will be lost
    IntVector2 viewSize = GetRenderTargetDimensions();
    if (viewport_.left_ != 0 || viewport_.top_ != 0 || viewport_.right_ != viewSize.x_ || viewport_.bottom_ != viewSize.y_)
        SetScissorTest(true, IntRect(0, 0, viewport_.Width(), viewport_.Height()));
    else
        SetScissorTest(false);

    glClear(glFlags);

    SetScissorTest(false);
    SetColorWrite(oldColorWrite);
    SetDepthWrite(oldDepthWrite);
    if (flags & CLEAR_STENCIL && stencilWriteMask_ != M_MAX_UNSIGNED)
        glStencilMask(stencilWriteMask_);
}

bool Graphics::ResolveToTexture(Texture2D* destination, const IntRect& viewport)
{
    if (!destination || !destination->GetRenderSurface())
        return false;

    URHO3D_PROFILE(ResolveToTexture);

    IntRect vpCopy = viewport;
    if (vpCopy.right_ <= vpCopy.left_)
        vpCopy.right_ = vpCopy.left_ + 1;
    if (vpCopy.bottom_ <= vpCopy.top_)
        vpCopy.bottom_ = vpCopy.top_ + 1;
    vpCopy.left_ = Clamp(vpCopy.left_, 0, width_);
    vpCopy.top_ = Clamp(vpCopy.top_, 0, height_);
    vpCopy.right_ = Clamp(vpCopy.right_, 0, width_);
    vpCopy.bottom_ = Clamp(vpCopy.bottom_, 0, height_);

    // Make sure the FBO is not in use
    ResetRenderTargets();

    // Use Direct3D convention with the vertical coordinates ie. 0 is top
    SetTextureForUpdate(destination);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, vpCopy.left_, height_ - vpCopy.bottom_, vpCopy.Width(), vpCopy.Height());
    SetTexture(0, nullptr);

    return true;
}

void Graphics::Draw(PrimitiveType type, unsigned vertexStart, unsigned vertexCount)
{
    if (!vertexCount)
        return;

    PrepareDraw();

    unsigned primitiveCount;
    GLenum glPrimitiveType;

    GetGLPrimitiveType(vertexCount, type, primitiveCount, glPrimitiveType);
    glDrawArrays(glPrimitiveType, vertexStart, vertexCount);

    numPrimitives_ += primitiveCount;
    ++numBatches_;
}

void Graphics::Draw(PrimitiveType type, unsigned indexStart, unsigned indexCount, unsigned minVertex, unsigned vertexCount)
{
    if (!indexCount || !indexBuffer_ || !indexBuffer_->GetGPUObject())
        return;

    PrepareDraw();

    unsigned indexSize = indexBuffer_->GetIndexSize();
    unsigned primitiveCount;
    GLenum glPrimitiveType;

    GetGLPrimitiveType(indexCount, type, primitiveCount, glPrimitiveType);
    GLenum indexType = indexSize == sizeof(unsigned short) ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
    glDrawElements(glPrimitiveType, indexCount, indexType, reinterpret_cast<const GLvoid*>(indexStart * indexSize));

    numPrimitives_ += primitiveCount;
    ++numBatches_;
}

void Graphics::DrawInstanced(PrimitiveType type, unsigned indexStart, unsigned indexCount, unsigned minVertex, unsigned vertexCount, unsigned instanceCount)
{
#if !defined(GL_ES_VERSION_2_0) || defined(__EMSCRIPTEN__)
    if (!indexCount || !indexBuffer_ || !indexBuffer_->GetGPUObject() || !instancingSupport_)
        return;

    PrepareDraw();

    unsigned indexSize = indexBuffer_->GetIndexSize();
    unsigned primitiveCount;
    GLenum glPrimitiveType;

    GetGLPrimitiveType(indexCount, type, primitiveCount, glPrimitiveType);
    GLenum indexType = indexSize == sizeof(unsigned short) ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
    if (gl3Support)
    {
        glDrawElementsInstanced(glPrimitiveType, indexCount, indexType, reinterpret_cast<const GLvoid*>(indexStart * indexSize),
                                instanceCount);
    }
    else
    {
        glDrawElementsInstancedARB(glPrimitiveType, indexCount, indexType, reinterpret_cast<const GLvoid*>(indexStart *
                                                                                                           indexSize), instanceCount);
    }

    numPrimitives_ += instanceCount * primitiveCount;
    ++numBatches_;
#endif
}

void Graphics::SetVertexBuffer(VertexBuffer* buffer)
{
    // Note: this is not multi-instance safe
    static std::vector<VertexBuffer*> vertexBuffers(1);
    static std::vector<unsigned> elementMasks(1);
    vertexBuffers[0] = buffer;
    elementMasks[0] = MASK_DEFAULT;
    SetVertexBuffers(vertexBuffers, elementMasks);
}

bool Graphics::SetVertexBuffers(const std::vector<VertexBuffer*>& buffers, const std::vector<unsigned>& elementMasks,
                                unsigned instanceOffset)
{
    if (buffers.size() > MAX_VERTEX_STREAMS)
    {
        URHO3D_LOGERROR("Too many vertex buffers");
        return false;
    }
    if (buffers.size() != elementMasks.size())
    {
        URHO3D_LOGERROR("Amount of element masks and vertex buffers does not match");
        return false;
    }

    bool changed = false;
    unsigned newAttributes = 0;

    for (unsigned i = 0; i < MAX_VERTEX_STREAMS; ++i)
    {
        VertexBuffer* buffer = nullptr;
        unsigned elementMask = 0;

        if (i < buffers.size() && buffers[i])
        {
            buffer = buffers[i];
            if (elementMasks[i] == MASK_DEFAULT)
                elementMask = buffer->GetElementMask();
            else
                elementMask = buffer->GetElementMask() & elementMasks[i];
        }

        // If buffer and element mask have stayed the same, skip to the next buffer
        if (buffer == vertexBuffers_[i] && elementMask == elementMasks_[i] && instanceOffset == lastInstanceOffset_ && !changed)
        {
            newAttributes |= elementMask;
            continue;
        }

        vertexBuffers_[i] = buffer;
        elementMasks_[i] = elementMask;
        changed = true;

        // Beware buffers with missing OpenGL objects, as binding a zero buffer object means accessing CPU memory for vertex data,
        // in which case the pointer will be invalid and cause a crash
        if (!buffer || !buffer->GetGPUObject())
            continue;

        SetVBO(buffer->GetGPUObject());
        unsigned vertexSize = buffer->GetVertexSize();

        for (unsigned j = 0; j < MAX_VERTEX_ELEMENTS; ++j)
        {
            unsigned attrIndex = glVertexAttrIndex[j];
            unsigned elementBit = 1 << j;

            if (elementMask & elementBit)
            {
                newAttributes |= elementBit;

                // Enable attribute if not enabled yet
                if ((impl_->enabledAttributes_ & elementBit) == 0)
                {
                    glEnableVertexAttribArray(attrIndex);
                    impl_->enabledAttributes_ |= elementBit;
                }

                // Set the attribute pointer. Add instance offset for the instance matrix pointers
                unsigned offset = (j >= ELEMENT_INSTANCEMATRIX1 && j < ELEMENT_OBJECTINDEX) ? instanceOffset * vertexSize : 0;
                glVertexAttribPointer(attrIndex, VertexBuffer::elementComponents[j], VertexBuffer::elementType[j],
                                      (GLboolean)VertexBuffer::elementNormalize[j], vertexSize,
                                      reinterpret_cast<const GLvoid*>(buffer->GetElementOffset((VertexElement)j) + offset));
            }
        }
    }

    if (!changed)
        return true;

    lastInstanceOffset_ = instanceOffset;

    // Now check which vertex attributes should be disabled
    unsigned disableAttributes = impl_->enabledAttributes_ & (~newAttributes);
    unsigned disableIndex = 0;

    while (disableAttributes)
    {
        if (disableAttributes & 1)
        {
            glDisableVertexAttribArray(glVertexAttrIndex[disableIndex]);
            impl_->enabledAttributes_ &= ~(1 << disableIndex);
        }
        disableAttributes >>= 1;
        ++disableIndex;
    }

    return true;
}

bool Graphics::SetVertexBuffers(const std::vector<SharedPtr<VertexBuffer> >& buffers, const std::vector<unsigned>&
                                elementMasks, unsigned instanceOffset)
{
    return SetVertexBuffers(reinterpret_cast<const std::vector<VertexBuffer*>&>(buffers), elementMasks, instanceOffset);
}

void Graphics::SetIndexBuffer(IndexBuffer* buffer)
{
    if (indexBuffer_ == buffer)
        return;

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer ? buffer->GetGPUObject() : 0);

    indexBuffer_ = buffer;
}

void Graphics::SetShaders(ShaderVariation* vs, ShaderVariation* ps)
{
    if (vs == vertexShader_ && ps == pixelShader_)
        return;


    // Compile the shaders now if not yet compiled. If already attempted, do not retry
    if (vs && !vs->GetGPUObject())
    {
        if (vs->GetCompilerOutput().isEmpty())
        {
            URHO3D_PROFILE(CompileVertexShader);

            bool success = vs->Create();
            if (success)
                URHO3D_LOGDEBUG("Compiled vertex shader " + vs->GetFullName());
            else
            {
                URHO3D_LOGERROR("Failed to compile vertex shader " + vs->GetFullName() + ":\n" + vs->GetCompilerOutput());
                vs = nullptr;
            }
        }
        else
            vs = nullptr;
    }

    if (ps && !ps->GetGPUObject())
    {
        if (ps->GetCompilerOutput().isEmpty())
        {
            URHO3D_PROFILE(CompilePixelShader);

            bool success = ps->Create();
            if (success)
                URHO3D_LOGDEBUG("Compiled pixel shader " + ps->GetFullName());
            else
            {
                URHO3D_LOGERROR("Failed to compile pixel shader " + ps->GetFullName() + ":\n" + ps->GetCompilerOutput());
                ps = nullptr;
            }
        }
        else
            ps = nullptr;
    }

    if (!vs || !ps)
    {
        glUseProgram(0);
        vertexShader_ = nullptr;
        pixelShader_ = nullptr;
        shaderProgram_ = nullptr;
    }
    else
    {
        vertexShader_ = vs;
        pixelShader_ = ps;

        std::pair<ShaderVariation*, ShaderVariation*> combination(vs, ps);
        ShaderProgramMap::iterator i = shaderPrograms_.find(combination);

        if (i != shaderPrograms_.end())
        {
            // Use the existing linked program
            if (MAP_VALUE(i)->GetGPUObject())
            {
                glUseProgram(MAP_VALUE(i)->GetGPUObject());
                shaderProgram_ = MAP_VALUE(i);
            }
            else
            {
                glUseProgram(0);
                shaderProgram_ = nullptr;
            }
        }
        else
        {
            // Link a new combination
            URHO3D_PROFILE(LinkShaders);

            SharedPtr<ShaderProgram> newProgram(new ShaderProgram(this, vs, ps));
            if (newProgram->Link())
            {
                URHO3D_LOGDEBUG("Linked vertex shader " + vs->GetFullName() + " and pixel shader " + ps->GetFullName());
                // Note: Link() calls glUseProgram() to set the texture sampler uniforms,
                // so it is not necessary to call it again
                shaderProgram_ = newProgram;
            }
            else
            {
                URHO3D_LOGERROR("Failed to link vertex shader " + vs->GetFullName() + " and pixel shader " + ps->GetFullName() + ":\n" +
                         newProgram->GetLinkerOutput());
                glUseProgram(0);
                shaderProgram_ = nullptr;
            }

            shaderPrograms_[combination] = newProgram;
        }
    }

    // Update the clip plane uniform on GL3, and set constant buffers
#ifndef GL_ES_VERSION_2_0
    if (gl3Support && shaderProgram_)
    {
        const SharedPtr<ConstantBuffer>* constantBuffers = shaderProgram_->GetConstantBuffers();
        for (unsigned i = 0; i < MAX_SHADER_PARAMETER_GROUPS * 2; ++i)
        {
            ConstantBuffer* buffer = constantBuffers[i].Get();
            if (buffer != currentConstantBuffers_[i])
            {
                unsigned object = buffer ? buffer->GetGPUObject() : 0;
                glBindBufferBase(GL_UNIFORM_BUFFER, i, object);
                // Calling glBindBufferBase also affects the generic buffer binding point
                impl_->boundUBO_ = object;
                currentConstantBuffers_[i] = buffer;
                ShaderProgram::ClearGlobalParameterSource((ShaderParameterGroup)(i % MAX_SHADER_PARAMETER_GROUPS));
            }
        }

        SetShaderParameter(VSP_CLIPPLANE, useClipPlane_ ? clipPlane_ : Vector4(0.0f, 0.0f, 0.0f, 1.0f));
    }
#endif

    // Store shader combination if shader dumping in progress
    if (shaderPrecache_)
        shaderPrecache_->StoreShaders(vertexShader_, pixelShader_);
}

void Graphics::SetShaderParameter(StringHash param, const float* data, unsigned count)
{
    if (shaderProgram_)
    {
        const ShaderParameter* info = shaderProgram_->GetParameter(param);
        if (info)
        {
            if (info->bufferPtr_)
            {
                ConstantBuffer* buffer = info->bufferPtr_;
                if (!buffer->IsDirty())
                    dirtyConstantBuffers_.push_back(buffer);
                buffer->SetParameter(info->location_, count * sizeof(float), data);
                return;
            }
            switch (info->type_)
            {
            case GL_FLOAT:
                glUniform1fv(info->location_, count, data);
                break;

            case GL_FLOAT_VEC2:
                glUniform2fv(info->location_, count / 2, data);
                break;

            case GL_FLOAT_VEC3:
                glUniform3fv(info->location_, count / 3, data);
                break;

            case GL_FLOAT_VEC4:
                glUniform4fv(info->location_, count / 4, data);
                break;

            case GL_FLOAT_MAT3:
                glUniformMatrix3fv(info->location_, count / 9, GL_FALSE, data);
                break;

            case GL_FLOAT_MAT4:
                glUniformMatrix4fv(info->location_, count / 16, GL_FALSE, data);
                break;
            default: break;
            }
        }
    }
}

void Graphics::SetShaderParameter(StringHash param, float value)
{
    if (shaderProgram_)
    {
        const ShaderParameter* info = shaderProgram_->GetParameter(param);
        if (info)
        {
            if (info->bufferPtr_)
            {
                ConstantBuffer* buffer = info->bufferPtr_;
                if (!buffer->IsDirty())
                    dirtyConstantBuffers_.push_back(buffer);
                buffer->SetParameter(info->location_, sizeof(float), &value);
                return;
            }
            glUniform1fv(info->location_, 1, &value);
        }
    }
}

void Graphics::SetShaderParameter(StringHash param, const Color& color)
{
    SetShaderParameter(param, color.Data(), 4);
}

void Graphics::SetShaderParameter(StringHash param, const Vector2& vector)
{
    if (shaderProgram_)
    {
        const ShaderParameter* info = shaderProgram_->GetParameter(param);
        if (info)
        {
            if (info->bufferPtr_)
            {
                ConstantBuffer* buffer = info->bufferPtr_;
                if (!buffer->IsDirty())
                    dirtyConstantBuffers_.push_back(buffer);
                buffer->SetParameter(info->location_, sizeof(Vector2), &vector);
                return;
            }
            // Check the uniform type to avoid mismatch
            switch (info->type_)
            {
            case GL_FLOAT:
                glUniform1fv(info->location_, 1, vector.Data());
                break;

            case GL_FLOAT_VEC2:
                glUniform2fv(info->location_, 1, vector.Data());
                break;
            default: break;
            }
        }
    }
}

void Graphics::SetShaderParameter(StringHash param, const Matrix3& matrix)
{
    if (shaderProgram_)
    {
        const ShaderParameter* info = shaderProgram_->GetParameter(param);
        if (info)
        {
            if (info->bufferPtr_)
            {
                ConstantBuffer* buffer = info->bufferPtr_;
                if (!buffer->IsDirty())
                    dirtyConstantBuffers_.push_back(buffer);
                buffer->SetVector3ArrayParameter(info->location_, 3, &matrix);
                return;
            }

            glUniformMatrix3fv(info->location_, 1, GL_FALSE, matrix.Data());
        }
    }
}

void Graphics::SetShaderParameter(StringHash param, const Vector3& vector)
{
    if (shaderProgram_)
    {
        const ShaderParameter* info = shaderProgram_->GetParameter(param);
        if (info)
        {
            if (info->bufferPtr_)
            {
                ConstantBuffer* buffer = info->bufferPtr_;
                if (!buffer->IsDirty())
                    dirtyConstantBuffers_.push_back(buffer);
                buffer->SetParameter(info->location_, sizeof(Vector3), &vector);
                return;
            }
            // Check the uniform type to avoid mismatch
            switch (info->type_)
            {
            case GL_FLOAT:
                glUniform1fv(info->location_, 1, vector.Data());
                break;

            case GL_FLOAT_VEC2:
                glUniform2fv(info->location_, 1, vector.Data());
                break;

            case GL_FLOAT_VEC3:
                glUniform3fv(info->location_, 1, vector.Data());
                break;
            default: break;
            }
        }
    }
}

void Graphics::SetShaderParameter(StringHash param, const Matrix4& matrix)
{
    if (shaderProgram_)
    {
        const ShaderParameter* info = shaderProgram_->GetParameter(param);
        if (info)
        {
            if (info->bufferPtr_)
            {
                ConstantBuffer* buffer = info->bufferPtr_;
                if (!buffer->IsDirty())
                    dirtyConstantBuffers_.push_back(buffer);
                buffer->SetParameter(info->location_, sizeof(Matrix4), &matrix);
                return;
            }

            glUniformMatrix4fv(info->location_, 1, GL_FALSE, matrix.Data());
        }
    }
}

void Graphics::SetShaderParameter(StringHash param, const Vector4& vector)
{
    if (shaderProgram_)
    {
        const ShaderParameter* info = shaderProgram_->GetParameter(param);
        if (info)
        {
            if (info->bufferPtr_)
            {
                ConstantBuffer* buffer = info->bufferPtr_;
                if (!buffer->IsDirty())
                    dirtyConstantBuffers_.push_back(buffer);
                buffer->SetParameter(info->location_, sizeof(Vector4), &vector);
                return;
            }
            // Check the uniform type to avoid mismatch
            switch (info->type_)
            {
            case GL_FLOAT:
                glUniform1fv(info->location_, 1, vector.Data());
                break;

            case GL_FLOAT_VEC2:
                glUniform2fv(info->location_, 1, vector.Data());
                break;

            case GL_FLOAT_VEC3:
                glUniform3fv(info->location_, 1, vector.Data());
                break;

            case GL_FLOAT_VEC4:
                glUniform4fv(info->location_, 1, vector.Data());
                break;
            default: break;
            }
        }
    }
}

void Graphics::SetShaderParameter(StringHash param, const Matrix3x4& matrix)
{
    if (shaderProgram_)
    {
        const ShaderParameter* info = shaderProgram_->GetParameter(param);
        if (info)
        {
            // Expand to a full Matrix4
            static Matrix4 fullMatrix;
            fullMatrix.m00_ = matrix.m00_;
            fullMatrix.m01_ = matrix.m01_;
            fullMatrix.m02_ = matrix.m02_;
            fullMatrix.m03_ = matrix.m03_;
            fullMatrix.m10_ = matrix.m10_;
            fullMatrix.m11_ = matrix.m11_;
            fullMatrix.m12_ = matrix.m12_;
            fullMatrix.m13_ = matrix.m13_;
            fullMatrix.m20_ = matrix.m20_;
            fullMatrix.m21_ = matrix.m21_;
            fullMatrix.m22_ = matrix.m22_;
            fullMatrix.m23_ = matrix.m23_;

            if (info->bufferPtr_)
            {
                ConstantBuffer* buffer = info->bufferPtr_;
                if (!buffer->IsDirty())
                    dirtyConstantBuffers_.push_back(buffer);
                buffer->SetParameter(info->location_, sizeof(Matrix4), &fullMatrix);
                return;
            }

            glUniformMatrix4fv(info->location_, 1, GL_FALSE, fullMatrix.Data());
        }
    }
}

void Graphics::SetShaderParameter(StringHash param, const Variant& value)
{
    switch (value.GetType())
    {
    case VAR_BOOL:
        SetShaderParameter(param, value.GetBool());
        break;

    case VAR_FLOAT:
        SetShaderParameter(param, value.GetFloat());
        break;

    case VAR_VECTOR2:
        SetShaderParameter(param, value.GetVector2());
        break;

    case VAR_VECTOR3:
        SetShaderParameter(param, value.GetVector3());
        break;

    case VAR_VECTOR4:
        SetShaderParameter(param, value.GetVector4());
        break;

    case VAR_COLOR:
        SetShaderParameter(param, value.GetColor());
        break;

    case VAR_MATRIX3:
        SetShaderParameter(param, value.GetMatrix3());
        break;

    case VAR_MATRIX3X4:
        SetShaderParameter(param, value.GetMatrix3x4());
        break;

    case VAR_MATRIX4:
        SetShaderParameter(param, value.GetMatrix4());
        break;

    case VAR_BUFFER:
        {
            const std::vector<unsigned char>& buffer = value.GetBuffer();
            if (buffer.size() >= sizeof(float))
                SetShaderParameter(param, reinterpret_cast<const float*>(&buffer[0]), buffer.size() / sizeof(float));
        }
        break;
    default:
        // Unsupported parameter type, do nothing
        break;
    }
}

bool Graphics::NeedParameterUpdate(ShaderParameterGroup group, const void* source)
{
    return shaderProgram_ ? shaderProgram_->NeedParameterUpdate(group, source) : false;
}

bool Graphics::HasShaderParameter(StringHash param)
{
    return shaderProgram_ && shaderProgram_->HasParameter(param);
}

bool Graphics::HasTextureUnit(TextureUnit unit)
{
    return shaderProgram_ && shaderProgram_->HasTextureUnit(unit);
}

void Graphics::ClearParameterSource(ShaderParameterGroup group)
{
    if (shaderProgram_)
        shaderProgram_->ClearParameterSource(group);
}

void Graphics::ClearParameterSources()
{
    ShaderProgram::ClearParameterSources();
}

void Graphics::ClearTransformSources()
{
    if (shaderProgram_)
    {
        shaderProgram_->ClearParameterSource(SP_CAMERA);
        shaderProgram_->ClearParameterSource(SP_OBJECT);
    }
}

void Graphics::SetTexture(unsigned index, Texture* texture)
{
    if (index >= MAX_TEXTURE_UNITS)
        return;

    // Check if texture is currently bound as a rendertarget. In that case, use its backup texture, or blank if not defined
    if (texture)
    {
        if (renderTargets_[0] && renderTargets_[0]->GetParentTexture() == texture)
            texture = texture->GetBackupTexture();
    }

    if (textures_[index] != texture)
    {
        if (impl_->activeTexture_ != index)
        {
            glActiveTexture(GL_TEXTURE0 + index);
            impl_->activeTexture_ = index;
        }

        if (texture)
        {
            GLenum glType = texture->GetTarget();
            // Unbind old texture type if necessary
            if (GL_NONE!=textureTypes_[index] && textureTypes_[index] != glType)
                glBindTexture(textureTypes_[index], 0);
            glBindTexture(glType, texture->GetGPUObject());
            textureTypes_[index] = glType;

            if (texture->GetParametersDirty())
                texture->UpdateParameters();
        }
        else if (GL_NONE!=textureTypes_[index])
        {
            glBindTexture(textureTypes_[index], 0);
            textureTypes_[index] = GL_NONE;
        }
        textures_[index] = texture;
    }
    else
    {
        if (texture && texture->GetParametersDirty())
        {
            if (impl_->activeTexture_ != index)
            {
                glActiveTexture(GL_TEXTURE0 + index);
                impl_->activeTexture_ = index;
            }

            glBindTexture(texture->GetTarget(), texture->GetGPUObject());
            texture->UpdateParameters();
        }
    }
}

void Graphics::SetTextureForUpdate(Texture* texture)
{
    if (impl_->activeTexture_ != 0)
    {
        glActiveTexture(GL_TEXTURE0);
        impl_->activeTexture_ = 0;
    }

    GLenum glType = texture->GetTarget();
    // Unbind old texture type if necessary
    if (textureTypes_[0]!=GL_NONE && textureTypes_[0] != glType)
        glBindTexture(textureTypes_[0], 0);
    glBindTexture(glType, texture->GetGPUObject());
    textureTypes_[0] = glType;
    textures_[0] = texture;
}

void Graphics::SetDefaultTextureFilterMode(TextureFilterMode mode)
{
    if (mode != defaultTextureFilterMode_)
    {
        defaultTextureFilterMode_ = mode;
        SetTextureParametersDirty();
    }
}

void Graphics::SetTextureAnisotropy(unsigned level)
{
    if (level != textureAnisotropy_)
    {
        textureAnisotropy_ = level;
        SetTextureParametersDirty();
    }
}

void Graphics::SetTextureParametersDirty()
{
    MutexLock lock(gpuObjectMutex_);

    for (std::vector<GPUObject*>::iterator i = gpuObjects_.begin(); i != gpuObjects_.end(); ++i)
    {
        Texture* texture = dynamic_cast<Texture*>(*i);
        if (texture)
            texture->SetParametersDirty();
    }
}

void Graphics::ResetRenderTargets()
{
    for (unsigned i = 0; i < MAX_RENDERTARGETS; ++i)
        SetRenderTarget(i, (RenderSurface *)nullptr);
    SetDepthStencil((RenderSurface *)nullptr);
    SetViewport(IntRect(0, 0, width_, height_));
}

void Graphics::ResetRenderTarget(unsigned index)
{
    SetRenderTarget(index, (RenderSurface *)nullptr);
}

void Graphics::ResetDepthStencil()
{
    SetDepthStencil((RenderSurface *)nullptr);
}

void Graphics::SetRenderTarget(unsigned index, RenderSurface* renderTarget)
{
    if (index >= MAX_RENDERTARGETS)
        return;

    if (renderTarget != renderTargets_[index])
    {
        renderTargets_[index] = renderTarget;

        // If the rendertarget is also bound as a texture, replace with backup texture or null
        if (renderTarget)
        {
            Texture* parentTexture = renderTarget->GetParentTexture();

            for (unsigned i = 0; i < MAX_TEXTURE_UNITS; ++i)
            {
                if (textures_[i] == parentTexture)
                    SetTexture(i, textures_[i]->GetBackupTexture());
            }
        }

        impl_->fboDirty_ = true;
    }
}

void Graphics::SetRenderTarget(unsigned index, Texture2D* texture)
{
    RenderSurface* renderTarget = nullptr;
    if (texture)
        renderTarget = texture->GetRenderSurface();

    SetRenderTarget(index, renderTarget);
}

void Graphics::SetDepthStencil(RenderSurface* depthStencil)
{
    // If we are using a rendertarget texture, it is required in OpenGL to also have an own depth-stencil
    // Create a new depth-stencil texture as necessary to be able to provide similar behaviour as Direct3D9
    if (renderTargets_[0] && !depthStencil)
    {
        int width = renderTargets_[0]->GetWidth();
        int height = renderTargets_[0]->GetHeight();

        // Direct3D9 default depth-stencil can not be used when rendertarget is larger than the window.
        // Check size similarly
        if (width <= width_ && height <= height_)
        {
            int searchKey = (width << 16) | height;
            auto i = depthTextures_.find(searchKey);
            if (i != depthTextures_.end())
                depthStencil = MAP_VALUE(i)->GetRenderSurface();
            else
            {
                SharedPtr<Texture2D> newDepthTexture(new Texture2D(context_));
                newDepthTexture->SetSize(width, height, GetDepthStencilFormat(), TEXTURE_DEPTHSTENCIL);
                depthTextures_[searchKey] = newDepthTexture;
                depthStencil = newDepthTexture->GetRenderSurface();
            }
        }
    }

    if (depthStencil != depthStencil_)
    {
        depthStencil_ = depthStencil;
        impl_->fboDirty_ = true;
    }
}

void Graphics::SetDepthStencil(Texture2D* texture)
{
    RenderSurface* depthStencil = nullptr;
    if (texture)
        depthStencil = texture->GetRenderSurface();

    SetDepthStencil(depthStencil);
}

void Graphics::SetViewport(const IntRect& rect)
{
    PrepareDraw();

    IntVector2 rtSize = GetRenderTargetDimensions();

    IntRect rectCopy = rect;

    if (rectCopy.right_ <= rectCopy.left_)
        rectCopy.right_ = rectCopy.left_ + 1;
    if (rectCopy.bottom_ <= rectCopy.top_)
        rectCopy.bottom_ = rectCopy.top_ + 1;
    rectCopy.left_ = Clamp(rectCopy.left_, 0, rtSize.x_);
    rectCopy.top_ = Clamp(rectCopy.top_, 0, rtSize.y_);
    rectCopy.right_ = Clamp(rectCopy.right_, 0, rtSize.x_);
    rectCopy.bottom_ = Clamp(rectCopy.bottom_, 0, rtSize.y_);

    // Use Direct3D convention with the vertical coordinates ie. 0 is top
    glViewport(rectCopy.left_, rtSize.y_ - rectCopy.bottom_, rectCopy.Width(), rectCopy.Height());
    viewport_ = rectCopy;

    // Disable scissor test, needs to be re-enabled by the user
    SetScissorTest(false);
}

void Graphics::SetBlendMode(BlendMode mode)
{
    if (mode != blendMode_)
    {
        if (mode == BLEND_REPLACE)
            glDisable(GL_BLEND);
        else
        {
            glEnable(GL_BLEND);
            glBlendFunc(glSrcBlend[mode], glDestBlend[mode]);
            glBlendEquation(glBlendOp[mode]);
        }

        blendMode_ = mode;
    }
}

void Graphics::SetColorWrite(bool enable)
{
    if (enable != colorWrite_)
    {
        if (enable)
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        else
            glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

        colorWrite_ = enable;
    }
}

void Graphics::SetCullMode(CullMode mode)
{
    if (mode != cullMode_)
    {
        if (mode == CULL_NONE)
            glDisable(GL_CULL_FACE);
        else
        {
            // Use Direct3D convention, ie. clockwise vertices define a front face
            glEnable(GL_CULL_FACE);
            glCullFace(mode == CULL_CCW ? GL_FRONT : GL_BACK);
        }

        cullMode_ = mode;
    }
}

void Graphics::SetDepthBias(float constantBias, float slopeScaledBias)
{
    if (constantBias != constantDepthBias_ || slopeScaledBias != slopeScaledDepthBias_)
    {
#ifndef GL_ES_VERSION_2_0
        if (slopeScaledBias != 0.0f)
        {
            // OpenGL constant bias is unreliable and dependant on depth buffer bitdepth, apply in the projection matrix instead
            float adjustedSlopeScaledBias = slopeScaledBias + 1.0f;
            glEnable(GL_POLYGON_OFFSET_FILL);
            glPolygonOffset(adjustedSlopeScaledBias, 0.0f);
        }
        else
            glDisable(GL_POLYGON_OFFSET_FILL);
#endif

        constantDepthBias_ = constantBias;
        slopeScaledDepthBias_ = slopeScaledBias;
        // Force update of the projection matrix shader parameter
        ClearParameterSource(SP_CAMERA);
    }
}

void Graphics::SetDepthTest(CompareMode mode)
{
    if (mode != depthTestMode_)
    {
        glDepthFunc(glCmpFunc[mode]);
        depthTestMode_ = mode;
    }
}

void Graphics::SetDepthWrite(bool enable)
{
    if (enable != depthWrite_)
    {
        glDepthMask(enable ? GL_TRUE : GL_FALSE);
        depthWrite_ = enable;
    }
}

void Graphics::SetFillMode(FillMode mode)
{
#ifndef GL_ES_VERSION_2_0
    if (mode != fillMode_)
    {
        glPolygonMode(GL_FRONT_AND_BACK, glFillMode[mode]);
        fillMode_ = mode;
    }
#endif
}

void Graphics::SetScissorTest(bool enable, const Rect& rect, bool borderInclusive)
{
    // During some light rendering loops, a full rect is toggled on/off repeatedly.
    // Disable scissor in that case to reduce state changes
    if (rect.min_.x_ <= 0.0f && rect.min_.y_ <= 0.0f && rect.max_.x_ >= 1.0f && rect.max_.y_ >= 1.0f)
        enable = false;

    if (enable)
    {
        IntVector2 rtSize(GetRenderTargetDimensions());
        IntVector2 viewSize(viewport_.Size());
        IntVector2 viewPos(viewport_.left_, viewport_.top_);
        IntRect intRect;
        int expand = borderInclusive ? 1 : 0;

        intRect.left_ = Clamp((int)((rect.min_.x_ + 1.0f) * 0.5f * viewSize.x_) + viewPos.x_, 0, rtSize.x_ - 1);
        intRect.top_ = Clamp((int)((-rect.max_.y_ + 1.0f) * 0.5f * viewSize.y_) + viewPos.y_, 0, rtSize.y_ - 1);
        intRect.right_ = Clamp((int)((rect.max_.x_ + 1.0f) * 0.5f * viewSize.x_) + viewPos.x_ + expand, 0, rtSize.x_);
        intRect.bottom_ = Clamp((int)((-rect.min_.y_ + 1.0f) * 0.5f * viewSize.y_) + viewPos.y_ + expand, 0, rtSize.y_);

        if (intRect.right_ == intRect.left_)
            intRect.right_++;
        if (intRect.bottom_ == intRect.top_)
            intRect.bottom_++;

        if (intRect.right_ < intRect.left_ || intRect.bottom_ < intRect.top_)
            enable = false;

        if (enable && scissorRect_ != intRect)
        {
            // Use Direct3D convention with the vertical coordinates ie. 0 is top
            glScissor(intRect.left_, rtSize.y_ - intRect.bottom_, intRect.Width(), intRect.Height());
            scissorRect_ = intRect;
        }
    }
    else
        scissorRect_ = IntRect::ZERO;

    if (enable != scissorTest_)
    {
        if (enable)
            glEnable(GL_SCISSOR_TEST);
        else
            glDisable(GL_SCISSOR_TEST);
        scissorTest_ = enable;
    }
}

void Graphics::SetScissorTest(bool enable, const IntRect& rect)
{
    IntVector2 rtSize(GetRenderTargetDimensions());
    IntVector2 viewPos(viewport_.left_, viewport_.top_);

    if (enable)
    {
        IntRect intRect;
        intRect.left_ = Clamp(rect.left_ + viewPos.x_, 0, rtSize.x_ - 1);
        intRect.top_ = Clamp(rect.top_ + viewPos.y_, 0, rtSize.y_ - 1);
        intRect.right_ = Clamp(rect.right_ + viewPos.x_, 0, rtSize.x_);
        intRect.bottom_ = Clamp(rect.bottom_ + viewPos.y_, 0, rtSize.y_);

        if (intRect.right_ == intRect.left_)
            intRect.right_++;
        if (intRect.bottom_ == intRect.top_)
            intRect.bottom_++;

        if (intRect.right_ < intRect.left_ || intRect.bottom_ < intRect.top_)
            enable = false;

        if (enable && scissorRect_ != intRect)
        {
            // Use Direct3D convention with the vertical coordinates ie. 0 is top
            glScissor(intRect.left_, rtSize.y_ - intRect.bottom_, intRect.Width(), intRect.Height());
            scissorRect_ = intRect;
        }
    }
    else
        scissorRect_ = IntRect::ZERO;

    if (enable != scissorTest_)
    {
        if (enable)
            glEnable(GL_SCISSOR_TEST);
        else
            glDisable(GL_SCISSOR_TEST);
        scissorTest_ = enable;
    }
}

void Graphics::SetClipPlane(bool enable, const Plane& clipPlane, const Matrix3x4& view, const Matrix4& projection)
{
#ifndef GL_ES_VERSION_2_0
    if (enable != useClipPlane_)
    {
        if (enable)
            glEnable(GL_CLIP_PLANE0);
        else
            glDisable(GL_CLIP_PLANE0);
        useClipPlane_ = enable;
    }

    if (enable)
    {
        Matrix4 viewProj = projection * view;
        clipPlane_ =  clipPlane.Transformed(viewProj).ToVector4();

        if (!gl3Support)
        {
            GLdouble planeData[4];
            planeData[0] = clipPlane_.x_;
            planeData[1] = clipPlane_.y_;
            planeData[2] = clipPlane_.z_;
            planeData[3] = clipPlane_.w_;

            glClipPlane(GL_CLIP_PLANE0, &planeData[0]);
        }
    }
#endif
}


void Graphics::SetStencilTest(bool enable, CompareMode mode, StencilOp pass, StencilOp fail, StencilOp zFail, unsigned stencilRef, unsigned compareMask, unsigned writeMask)
{
#ifndef GL_ES_VERSION_2_0
    if (enable != stencilTest_)
    {
        if (enable)
            glEnable(GL_STENCIL_TEST);
        else
            glDisable(GL_STENCIL_TEST);
        stencilTest_ = enable;
    }

    if (enable)
    {
        if (mode != stencilTestMode_ || stencilRef != stencilRef_ || compareMask != stencilCompareMask_)
        {
            glStencilFunc(glCmpFunc[mode], stencilRef, compareMask);
            stencilTestMode_ = mode;
            stencilRef_ = stencilRef;
            stencilCompareMask_ = compareMask;
        }
        if (writeMask != stencilWriteMask_)
        {
            glStencilMask(writeMask);
            stencilWriteMask_ = writeMask;
        }
        if (pass != stencilPass_ || fail != stencilFail_ || zFail != stencilZFail_)
        {
            glStencilOp(glStencilOps[fail], glStencilOps[zFail], glStencilOps[pass]);
            stencilPass_ = pass;
            stencilFail_ = fail;
            stencilZFail_ = zFail;
        }
    }
#endif
}


void Graphics::BeginDumpShaders(const QString& fileName)
{
    shaderPrecache_ = new ShaderPrecache(context_, fileName);
}

void Graphics::EndDumpShaders()
{
    shaderPrecache_.Reset();
}

void Graphics::PrecacheShaders(Deserializer& source)
{
    URHO3D_PROFILE(PrecacheShaders);

    ShaderPrecache::LoadShaders(this, source);
}

bool Graphics::IsInitialized() const
{
    return impl_->window_ != nullptr;
}

bool Graphics::IsDeviceLost() const
{
    // On iOS treat window minimization as device loss, as it is forbidden to access OpenGL when minimized
#ifdef IOS
    if (impl_->window_ && (SDL_GetWindowFlags(impl_->window_) & SDL_WINDOW_MINIMIZED) != 0)
        return true;
#endif

    return impl_->context_ == nullptr;
}

IntVector2 Graphics::GetWindowPosition() const
{
    if (impl_->window_)
        return position_;
    return IntVector2::ZERO;
}

std::vector<IntVector2> Graphics::GetResolutions() const
{
    std::vector<IntVector2> ret;
    unsigned numModes = SDL_GetNumDisplayModes(0);

    for (unsigned i = 0; i < numModes; ++i)
    {
        SDL_DisplayMode mode;
        SDL_GetDisplayMode(0, i, &mode);
        int width = mode.w;
        int height  = mode.h;

        // Store mode if unique
        bool unique = true;
        for (unsigned j = 0; j < ret.size(); ++j)
        {
            if (ret[j].x_ == width && ret[j].y_ == height)
            {
                unique = false;
                break;
            }
        }

        if (unique)
            ret.push_back(IntVector2(width, height));
    }

    return ret;
}

std::vector<int> Graphics::GetMultiSampleLevels() const
{
    std::vector<int> ret;
    // No multisampling always supported
    ret.push_back(1);
    /// \todo Implement properly, if possible

    return ret;
}

IntVector2 Graphics::GetDesktopResolution() const
{
#if !defined(ANDROID) && !defined(IOS)
    SDL_DisplayMode mode;
    SDL_GetDesktopDisplayMode(0, &mode);
    return IntVector2(mode.w, mode.h);
#else
    // SDL_GetDesktopDisplayMode() may not work correctly on mobile platforms. Rather return the window size
    return IntVector2(width_, height_);
#endif
}

GLenum Graphics::GetFormat(CompressedFormat format) const
{
    switch (format)
    {
    case CF_RGBA:
        return GL_RGBA;

    case CF_DXT1:
        return dxtTextureSupport_ ? GL_COMPRESSED_RGBA_S3TC_DXT1_EXT : GL_NONE;

    case CF_DXT3:
        return dxtTextureSupport_ ? GL_COMPRESSED_RGBA_S3TC_DXT3_EXT : GL_NONE;

    case CF_DXT5:
        return dxtTextureSupport_ ? GL_COMPRESSED_RGBA_S3TC_DXT5_EXT : GL_NONE;

    default:
        return GL_NONE;
    }
}

unsigned Graphics::GetMaxBones()
{
#ifdef RPI
    return 32;
#else
    return gl3Support ? 128 : 64;
#endif
}

ShaderVariation* Graphics::GetShader(ShaderType type, const QString& name, const QString& defines) const
{
    return GetShader(type, qPrintable(name), qPrintable(defines));
}

ShaderVariation* Graphics::GetShader(ShaderType type, const char* name, const char* defines) const
{
    if (lastShaderName_ != name || !lastShader_)
    {
        ResourceCache* cache = GetSubsystem<ResourceCache>();

        QString fullShaderName = shaderPath_ + name + shaderExtension_;
        // Try to reduce repeated error log prints because of missing shaders
        if (lastShaderName_ == name && !cache->Exists(fullShaderName))
            return nullptr;

        lastShader_ = cache->GetResource<Shader>(fullShaderName);
        lastShaderName_ = name;
    }

    return lastShader_ ? lastShader_->GetVariation(type, defines) : (ShaderVariation*)nullptr;
}

VertexBuffer* Graphics::GetVertexBuffer(unsigned index) const
{
    return index < MAX_VERTEX_STREAMS ? vertexBuffers_[index] : nullptr;
}

TextureUnit Graphics::GetTextureUnit(const QString& name)
{
    auto i = textureUnits_.find(name);
    if (i != textureUnits_.end())
        return MAP_VALUE(i);
    else
        return MAX_TEXTURE_UNITS;
}

const QString& Graphics::GetTextureUnitName(TextureUnit unit)
{
    for (auto elem=textureUnits_.begin(),fin=textureUnits_.end(); elem!=fin; ++elem)
    {
        if (MAP_VALUE(elem) == unit)
            return MAP_KEY(elem);
    }
    return s_dummy;
}

Texture* Graphics::GetTexture(unsigned index) const
{
    return index < MAX_TEXTURE_UNITS ? textures_[index] : nullptr;
}

RenderSurface* Graphics::GetRenderTarget(unsigned index) const
{
    return index < MAX_RENDERTARGETS ? renderTargets_[index] : nullptr;
}

IntVector2 Graphics::GetRenderTargetDimensions() const
{
    int width, height;

    if (renderTargets_[0])
    {
        width = renderTargets_[0]->GetWidth();
        height = renderTargets_[0]->GetHeight();
    }
    else if (depthStencil_)
    {
        width = depthStencil_->GetWidth();
        height = depthStencil_->GetHeight();
    }
    else
    {
        width = width_;
        height = height_;
    }

    return IntVector2(width, height);
}

void Graphics::WindowResized()
{
    if (!impl_->window_)
        return;

    int newWidth, newHeight;

    SDL_GetWindowSize(impl_->window_, &newWidth, &newHeight);
    if (newWidth == width_ && newHeight == height_)
        return;

    width_ = newWidth;
    height_ = newHeight;

    // Reset rendertargets and viewport for the new screen size. Also clean up any FBO's, as they may be screen size dependent
    CleanupFramebuffers();
    ResetRenderTargets();

    URHO3D_LOGDEBUG(QString("Window was resized to %1x%2").arg(width_).arg(height_));

    using namespace ScreenMode;

    VariantMap& eventData = GetEventDataMap();
    eventData[P_WIDTH] = width_;
    eventData[P_HEIGHT] = height_;
    eventData[P_FULLSCREEN] = fullscreen_;
    eventData[P_RESIZABLE] = resizable_;
    eventData[P_BORDERLESS] = borderless_;
    SendEvent(E_SCREENMODE, eventData);
}

void Graphics::WindowMoved()
{
    if (!impl_->window_ || fullscreen_)
        return;

    int newX, newY;

    SDL_GetWindowPosition(impl_->window_, &newX, &newY);
    if (newX == position_.x_ && newY == position_.y_)
        return;

    position_.x_ = newX;
    position_.y_ = newY;

    URHO3D_LOGDEBUG(QString("Window was moved to %1,%2").arg(position_.x_).arg(position_.y_));

    using namespace WindowPos;

    VariantMap& eventData = GetEventDataMap();
    eventData[P_X] = position_.x_;
    eventData[P_Y] = position_.y_;
    SendEvent(E_WINDOWPOS, eventData);
}

void Graphics::AddGPUObject(GPUObject* object)
{
    MutexLock lock(gpuObjectMutex_);

    gpuObjects_.push_back(object);
}

void Graphics::RemoveGPUObject(GPUObject* object)
{
    MutexLock lock(gpuObjectMutex_);
    auto iter = std::find(gpuObjects_.begin(),gpuObjects_.end(),object);
    assert(iter!=gpuObjects_.end());

    gpuObjects_.erase(iter);
}

void* Graphics::ReserveScratchBuffer(unsigned size)
{
    if (!size)
        return nullptr;

    if (size > maxScratchBufferRequest_)
        maxScratchBufferRequest_ = size;

    // First check for a free buffer that is large enough
    for (ScratchBuffer & elem : scratchBuffers_)
    {
        if (!elem.reserved_ && elem.size_ >= size)
        {
            elem.reserved_ = true;
            return elem.data_.Get();
        }
    }

    // Then check if a free buffer can be resized
    for (ScratchBuffer & elem : scratchBuffers_)
    {
        if (!elem.reserved_)
        {
            elem.data_ = new unsigned char[size];
            elem.size_ = size;
            elem.reserved_ = true;
            return elem.data_.Get();
        }
    }

    // Finally allocate a new buffer
    ScratchBuffer newBuffer;
    newBuffer.data_ = new unsigned char[size];
    newBuffer.size_ = size;
    newBuffer.reserved_ = true;
    scratchBuffers_.push_back(newBuffer);
    return newBuffer.data_.Get();
}

void Graphics::FreeScratchBuffer(void* buffer)
{
    if (!buffer)
        return;

    for (ScratchBuffer & elem : scratchBuffers_)
    {
        if (elem.reserved_ && elem.data_.Get() == buffer)
        {
            elem.reserved_ = false;
            return;
        }
    }

    URHO3D_LOGWARNING("Reserved scratch buffer " + ToStringHex((unsigned)(size_t)buffer) + " not found");
}

void Graphics::CleanupScratchBuffers()
{
    for (ScratchBuffer & elem : scratchBuffers_)
    {
        if (!elem.reserved_ && elem.size_ > maxScratchBufferRequest_ * 2)
        {
            elem.data_ = maxScratchBufferRequest_ > 0 ? new unsigned char[maxScratchBufferRequest_] : nullptr;
            elem.size_ = maxScratchBufferRequest_;
        }
    }

    maxScratchBufferRequest_ = 0;
}

void Graphics::CleanupRenderSurface(RenderSurface* surface)
{
    if (!surface)
        return;

    // Flush pending FBO changes first if any
    PrepareDraw();

    unsigned currentFBO = impl_->boundFBO_;

    // Go through all FBOs and clean up the surface from them
    for (auto &entry : impl_->frameBuffers_)
    {
        FrameBufferObject &ob(ELEMENT_VALUE(entry));
        for (unsigned j = 0; j < MAX_RENDERTARGETS; ++j)
        {
            if (ob.colorAttachments_[j] == surface)
            {
                if (currentFBO != ob.fbo_)
                {
                    BindFramebuffer(ob.fbo_);
                    currentFBO = ob.fbo_;
                }
                BindColorAttachment(j, GL_TEXTURE_2D, 0);
                ob.colorAttachments_[j] = nullptr;
                // Mark drawbuffer bits to need recalculation
                ob.drawBuffers_ = M_MAX_UNSIGNED;
            }
        }
        if (ob.depthAttachment_ == surface)
        {
            if (currentFBO != ob.fbo_)
            {
                BindFramebuffer(ob.fbo_);
                currentFBO = ob.fbo_;
            }
            BindDepthAttachment(0, false);
            BindStencilAttachment(0, false);
            ob.depthAttachment_ = nullptr;
        }
    }

    // Restore previously bound FBO now if needed
    if (currentFBO != impl_->boundFBO_)
        BindFramebuffer(impl_->boundFBO_);
}

void Graphics::CleanupShaderPrograms(ShaderVariation* variation)
{
    for (ShaderProgramMap::iterator i = shaderPrograms_.begin(); i != shaderPrograms_.end();)
    {
        if (MAP_VALUE(i)->GetVertexShader() == variation || MAP_VALUE(i)->GetPixelShader() == variation)
            i = shaderPrograms_.erase(i);
        else
            ++i;
    }

    if (vertexShader_ == variation || pixelShader_ == variation)
        shaderProgram_ = nullptr;
}

ConstantBuffer* Graphics::GetOrCreateConstantBuffer(unsigned bindingIndex, unsigned size)
{
    unsigned key = (bindingIndex << 16) | size;
    HashMap<unsigned, SharedPtr<ConstantBuffer> >::iterator i = constantBuffers_.find(key);
    if (i != constantBuffers_.end())
        return MAP_VALUE(i).Get();

    ConstantBuffer *buffer = new ConstantBuffer(context_);
    constantBuffers_.emplace(key, SharedPtr<ConstantBuffer>(buffer));
    buffer->SetSize(size);
    return buffer;
}
void Graphics::Release(bool clearGPUObjects, bool closeWindow)
{
    if (!impl_->window_)
        return;


    {
        MutexLock lock(gpuObjectMutex_);

        if (clearGPUObjects)
        {
            // Shutting down: release all GPU objects that still exist
            // Shader programs are also GPU objects; clear them first to avoid list modification during iteration
            shaderPrograms_.clear();

            for (GPUObject * elem : gpuObjects_)
                elem->Release();
            gpuObjects_.clear();
        }
        else
        {
            // We are not shutting down, but recreating the context: mark GPU objects lost
            for (GPUObject * elem : gpuObjects_)
                elem->OnDeviceLost();

            // In this case clear shader programs last so that they do not attempt to delete their OpenGL program
            // from a context that may no longer exist
            shaderPrograms_.clear();

            SendEvent(E_DEVICELOST);
        }
    }

    CleanupFramebuffers();
    depthTextures_.clear();

    // End fullscreen mode first to counteract transition and getting stuck problems on OS X
#if defined(__APPLE__) && !defined(IOS)
    if (closeWindow && fullscreen_ && !externalWindow_)
        SDL_SetWindowFullscreen(impl_->window_, SDL_FALSE);
#endif

    if (impl_->context_)
    {
        // Do not log this message if we are exiting
        if (!clearGPUObjects)
            URHO3D_LOGINFO("OpenGL context lost");

        SDL_GL_DeleteContext(impl_->context_);
        impl_->context_ = nullptr;
    }

    if (closeWindow)
    {
        SDL_ShowCursor(SDL_TRUE);

        // Do not destroy external window except when shutting down
        if (!externalWindow_ || clearGPUObjects)
        {
            SDL_DestroyWindow(impl_->window_);
            impl_->window_ = nullptr;
        }
    }
}

void Graphics::Restore()
{
    if (!impl_->window_)
        return;

#ifdef ANDROID
    // On Android the context may be lost behind the scenes as the application is minimized
    if (impl_->context_ && !SDL_GL_GetCurrentContext())
    {
        impl_->context_ = 0;
        // Mark GPU objects lost without a current context. In this case they just mark their internal state lost
        // but do not perform OpenGL commands to delete the GL objects
        Release(false, false);
    }
#endif

    // Ensure first that the context exists
    if (!impl_->context_)
    {
        impl_->context_ = SDL_GL_CreateContext(impl_->window_);
#ifndef GL_ES_VERSION_2_0
        // If we're trying to use OpenGL 3, but context creation fails, retry with 2
        if (!forceGL2_ && !impl_->context_)
        {
            forceGL2_ = true;
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, 0);
            impl_->context_ = SDL_GL_CreateContext(impl_->window_);
        }
#endif
#ifdef IOS
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, (GLint*)&impl_->systemFBO_);
#endif

        if (!impl_->context_)
        {
            URHO3D_LOGERROR(QString("Could not create OpenGL context, root cause '%1'").arg(SDL_GetError()));
            return;
        }

        // Clear cached extensions string from the previous context
        extensions.clear();

        // Initialize OpenGL extensions library (desktop only)

        glbinding::Binding::initialize();
        auto context_ver = glbinding::ContextInfo::version();
        if (!forceGL2_ && context_ver>=glbinding::Version(3,2))
        {
            gl3Support = true;
            apiName_ = "GL3";

            // Create and bind a vertex array object that will stay in use throughout
            unsigned vertexArrayObject;
            glGenVertexArrays(1, &vertexArrayObject);
            glBindVertexArray(vertexArrayObject);
        }
        else if (context_ver>=glbinding::Version(2,0))
        {
            std::set<std::string> unknownExts;

            const std::set<GLextension> & supportedExts = glbinding::ContextInfo::extensions(&unknownExts);

            if (supportedExts.find(GLextension::GL_EXT_framebuffer_object)==supportedExts.end()
                    || supportedExts.find(GLextension::GL_EXT_packed_depth_stencil)==supportedExts.end())
            {
                URHO3D_LOGERROR("EXT_framebuffer_object and EXT_packed_depth_stencil OpenGL extensions are required");
                return;
            }

            gl3Support = false;
            apiName_ = "GL2";
        }
        else
        {
            URHO3D_LOGERROR("OpenGL 2.0 is required");
            return;
        }

        // Set up texture data read/write alignment. It is important that this is done before uploading any texture data
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        ResetCachedState();
    }

    {
        MutexLock lock(gpuObjectMutex_);

        for (GPUObject * elem : gpuObjects_)
            elem->OnDeviceReset();
    }

    SendEvent(E_DEVICERESET);
}

void Graphics::Maximize()
{
    if (!impl_->window_)
        return;

    SDL_MaximizeWindow(impl_->window_);
}

void Graphics::Minimize()
{
    if (!impl_->window_)
        return;

    SDL_MinimizeWindow(impl_->window_);
}

void Graphics::MarkFBODirty()
{
    impl_->fboDirty_ = true;
}

void Graphics::SetVBO(unsigned object)
{
    if (impl_->boundVBO_ != object)
    {
        if (object)
            glBindBuffer(GL_ARRAY_BUFFER, object);
        impl_->boundVBO_ = object;
    }
}
void Graphics::SetUBO(unsigned object)
{
#ifndef GL_ES_VERSION_2_0
    if (impl_->boundUBO_ != object)
    {
        if (object)
            glBindBuffer(GL_UNIFORM_BUFFER, object);
        impl_->boundUBO_ = object;
    }
#endif
}

gl::GLenum Graphics::GetAlphaFormat()
{
    // Alpha format is deprecated on OpenGL 3+
    if (gl3Support)
        return GL_R8;
    return GL_ALPHA;
}

gl::GLenum Graphics::GetLuminanceFormat()
{
    // Luminance format is deprecated on OpenGL 3+
    if (gl3Support)
        return GL_R8;
    return GL_LUMINANCE;
}

gl::GLenum Graphics::GetLuminanceAlphaFormat()
{
    // Luminance alpha format is deprecated on OpenGL 3+
    if (gl3Support)
        return GL_RG8;
    return GL_LUMINANCE_ALPHA;
}

gl::GLenum Graphics::GetRGBFormat()
{
    return GL_RGB;
}

gl::GLenum Graphics::GetRGBAFormat()
{
    return GL_RGBA;
}

gl::GLenum Graphics::GetRGBA16Format()
{
    return GL_RGBA16;
}

gl::GLenum Graphics::GetRGBAFloat16Format()
{
    return GL_RGBA16F_ARB;
}

gl::GLenum Graphics::GetRGBAFloat32Format()
{
    return GL_RGBA32F_ARB;
}

gl::GLenum Graphics::GetRG16Format()
{
    return GL_RG16;
}

gl::GLenum Graphics::GetRGFloat16Format()
{
    return GL_RG16F;
}

gl::GLenum Graphics::GetRGFloat32Format()
{
    return GL_RG32F;
}

gl::GLenum Graphics::GetFloat16Format()
{
    return GL_R16F;
}

gl::GLenum Graphics::GetFloat32Format()
{
    return GL_R32F;
}

gl::GLenum Graphics::GetLinearDepthFormat()
{
    // OpenGL 3 can use different color attachment formats
    if (gl3Support)
        return GL_R32F;
    // OpenGL 2 requires color attachments to have the same format, therefore encode deferred depth to RGBA manually
    // if not using a readable hardware depth texture
    return GL_RGBA;
}

gl::GLenum Graphics::GetDepthStencilFormat()
{
    return GL_DEPTH24_STENCIL8_EXT;
}

gl::GLenum Graphics::GetReadableDepthFormat()
{
    return GL_DEPTH_COMPONENT24;
}

gl::GLenum Graphics::GetFormat(const QString& formatName)
{
    QString nameLower = formatName.toLower().trimmed();

    if (nameLower == "a")
        return GetAlphaFormat();
    if (nameLower == "l")
        return GetLuminanceFormat();
    if (nameLower == "la")
        return GetLuminanceAlphaFormat();
    if (nameLower == "rgb")
        return GetRGBFormat();
    if (nameLower == "rgba")
        return GetRGBAFormat();
    if (nameLower == "rgba16")
        return GetRGBA16Format();
    if (nameLower == "rgba16f")
        return GetRGBAFloat16Format();
    if (nameLower == "rgba32f")
        return GetRGBAFloat32Format();
    if (nameLower == "rg16")
        return GetRG16Format();
    if (nameLower == "rg16f")
        return GetRGFloat16Format();
    if (nameLower == "rg32f")
        return GetRGFloat32Format();
    if (nameLower == "r16f")
        return GetFloat16Format();
    if (nameLower == "r32f" || nameLower == "float")
        return GetFloat32Format();
    if (nameLower == "lineardepth" || nameLower == "depth")
        return GetLinearDepthFormat();
    if (nameLower == "d24s8")
        return GetDepthStencilFormat();
    if (nameLower == "readabledepth" || nameLower == "hwdepth")
        return GetReadableDepthFormat();

    return GetRGBFormat();
}

void Graphics::CreateWindowIcon()
{
    if (windowIcon_)
    {
        SDL_Surface* surface = windowIcon_->GetSDLSurface();
        if (surface)
        {
            SDL_SetWindowIcon(impl_->window_, surface);
            SDL_FreeSurface(surface);
        }
    }
}

void Graphics::CheckFeatureSupport()
{
    // Check supported features: light pre-pass, deferred rendering and hardware depth texture
    lightPrepassSupport_ = false;
    deferredSupport_ = false;

    int numSupportedRTs = 1;
    if (gl3Support)
    {
        // Work around GLEW failure to check extensions properly from a GL3 context
        instancingSupport_ = true;
        dxtTextureSupport_ = true;
        anisotropySupport_ = true;
        sRGBSupport_ = true;
        sRGBWriteSupport_ = true;

        glVertexAttribDivisor(ELEMENT_INSTANCEMATRIX1, 1);
        glVertexAttribDivisor(ELEMENT_INSTANCEMATRIX2, 1);
        glVertexAttribDivisor(ELEMENT_INSTANCEMATRIX3, 1);

        glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &numSupportedRTs);
    }
    else
    {
        const std::set<gl::GLextension> exts(glbinding::ContextInfo::extensions());
        instancingSupport_ = exts.count(GLextension::GL_ARB_instanced_arrays) != 0;
        dxtTextureSupport_ = exts.count(GLextension::GL_EXT_texture_compression_s3tc) != 0;
        anisotropySupport_ = exts.count(GLextension::GL_EXT_texture_filter_anisotropic) != 0;
        sRGBSupport_       = exts.count(GLextension::GL_EXT_texture_sRGB) != 0;
        sRGBWriteSupport_  = exts.count(GLextension::GL_EXT_framebuffer_sRGB) != 0;

        // Set up instancing divisors if supported
        if (instancingSupport_)
        {
            glVertexAttribDivisorARB(ELEMENT_INSTANCEMATRIX1, 1);
            glVertexAttribDivisorARB(ELEMENT_INSTANCEMATRIX2, 1);
            glVertexAttribDivisorARB(ELEMENT_INSTANCEMATRIX3, 1);
        }
        glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS_EXT, &numSupportedRTs);
    }

    // Must support 2 rendertargets for light pre-pass, and 4 for deferred
    if (numSupportedRTs >= 2)
        lightPrepassSupport_ = true;
    if (numSupportedRTs >= 4)
        deferredSupport_ = true;

#if defined(__APPLE__) && !defined(IOS)
    // On OS X check for an Intel driver and use shadow map RGBA dummy color textures, because mixing
    // depth-only FBO rendering and backbuffer rendering will bug, resulting in a black screen in full
    // screen mode, and incomplete shadow maps in windowed mode
    String renderer((const char*)glGetString(GL_RENDERER));
    if (renderer.Contains("Intel", false))
        dummyColorFormat_ = GetRGBAFormat();
#endif

}

FrameBufferObject &Graphics::getOrCreateFrameBufferObject(unsigned long long fboKey)
{
    HashMap<unsigned long long, FrameBufferObject>::iterator i = impl_->frameBuffers_.find(fboKey);
    if (i == impl_->frameBuffers_.end())
    {
        FrameBufferObject newFbo;
        newFbo.fbo_ = CreateFramebuffer();
        i = impl_->frameBuffers_.insert(std::make_pair(fboKey, newFbo)).first;
    }
    return MAP_VALUE(i);
}

void Graphics::PrepareDraw()
{
#ifndef GL_ES_VERSION_2_0
    if (gl3Support)
    {
        for (ConstantBuffer * cb : dirtyConstantBuffers_)
            cb->Apply();
        dirtyConstantBuffers_.clear();
    }
#endif

    if (impl_->fboDirty_)
    {
        impl_->fboDirty_ = false;

        // First check if no framebuffer is needed. In that case simply return to backbuffer rendering
        bool noFbo = !depthStencil_;
        if (noFbo)
        {
            for (RenderSurface * elem : renderTargets_)
            {
                if (elem)
                {
                    noFbo = false;
                    break;
                }
            }
        }

        if (noFbo)
        {
            if (impl_->boundFBO_ != impl_->systemFBO_)
            {
                BindFramebuffer(impl_->systemFBO_);
                impl_->boundFBO_ = impl_->systemFBO_;
            }

#ifndef GL_ES_VERSION_2_0
            // Disable/enable sRGB write
            if (sRGBWriteSupport_)
            {
                bool sRGBWrite = sRGB_;
                if (sRGBWrite != impl_->sRGBWrite_)
                {
                    if (sRGBWrite)
                        glEnable(GL_FRAMEBUFFER_SRGB_EXT);
                    else
                        glDisable(GL_FRAMEBUFFER_SRGB_EXT);
                    impl_->sRGBWrite_ = sRGBWrite;
                }
            }
#endif

            return;
        }

        // Search for a new framebuffer based on format & size, or create new
        IntVector2 rtSize = Graphics::GetRenderTargetDimensions();
        GLenum format = GL_NONE;
        if (renderTargets_[0])
            format = renderTargets_[0]->GetParentTexture()->GetFormat();
        else if (depthStencil_)
            format = depthStencil_->GetParentTexture()->GetFormat();

        unsigned long long fboKey = (rtSize.x_ << 16 | rtSize.y_) | (((unsigned long long)format) << 32);

        FrameBufferObject &fboBufferTgt(getOrCreateFrameBufferObject(fboKey));
        if (impl_->boundFBO_ != fboBufferTgt.fbo_)
        {
            BindFramebuffer(fboBufferTgt.fbo_);
            impl_->boundFBO_ = fboBufferTgt.fbo_;
        }

#ifndef GL_ES_VERSION_2_0
        // Setup readbuffers & drawbuffers if needed
        if (fboBufferTgt.readBuffers_ != GL_NONE)
        {
            glReadBuffer(GL_NONE);
            fboBufferTgt.readBuffers_ = 0;
        }

        // Calculate the bit combination of non-zero color rendertargets to first check if the combination changed
        unsigned newDrawBuffers = 0;
        for (unsigned j = 0; j < MAX_RENDERTARGETS; ++j)
        {
            if (renderTargets_[j])
                newDrawBuffers |= 1 << j;
        }

        if (newDrawBuffers != fboBufferTgt.drawBuffers_)
        {
            // Check for no color rendertargets (depth rendering only)
            if (!newDrawBuffers)
                glDrawBuffer(GL_NONE);
            else
            {
                GLenum drawBufferIds[MAX_RENDERTARGETS];
                unsigned drawBufferCount = 0;

                for (unsigned j = 0; j < MAX_RENDERTARGETS; ++j)
                {
                    if (renderTargets_[j])
                    {
                        if (!gl3Support)
                            drawBufferIds[drawBufferCount++] = GL_COLOR_ATTACHMENT0_EXT + j;
                        else
                            drawBufferIds[drawBufferCount++] = GL_COLOR_ATTACHMENT0 + j;
                    }
                }
                glDrawBuffers(drawBufferCount, drawBufferIds);
            }

            fboBufferTgt.drawBuffers_ = newDrawBuffers;
        }
#endif

        for (unsigned j = 0; j < MAX_RENDERTARGETS; ++j)
        {
            if (renderTargets_[j])
            {
                Texture* texture = renderTargets_[j]->GetParentTexture();

                // If texture's parameters are dirty, update before attaching
                if (texture->GetParametersDirty())
                {
                    SetTextureForUpdate(texture);
                    texture->UpdateParameters();
                    SetTexture(0, nullptr);
                }

                if (fboBufferTgt.colorAttachments_[j] != renderTargets_[j])
                {
                    BindColorAttachment(j, renderTargets_[j]->GetTarget(), texture->GetGPUObject());
                    fboBufferTgt.colorAttachments_[j] = renderTargets_[j];
                }
            }
            else
            {
                if (fboBufferTgt.colorAttachments_[j])
                {
                    BindColorAttachment(j, GL_TEXTURE_2D, 0);
                    fboBufferTgt.colorAttachments_[j] = nullptr;
                }
            }
        }

        if (depthStencil_)
        {
            // Bind either a renderbuffer or a depth texture, depending on what is available
            Texture* texture = depthStencil_->GetParentTexture();
#ifndef GL_ES_VERSION_2_0
            bool hasStencil = texture->GetFormat() == GL_DEPTH24_STENCIL8_EXT;
#else
            bool hasStencil = texture->GetFormat() == GL_DEPTH24_STENCIL8_OES;
#endif
            unsigned renderBufferID = depthStencil_->GetRenderBuffer();
            if (!renderBufferID)
            {
                // If texture's parameters are dirty, update before attaching
                if (texture->GetParametersDirty())
                {
                    SetTextureForUpdate(texture);
                    texture->UpdateParameters();
                    SetTexture(0, nullptr);
                }

                if (fboBufferTgt.depthAttachment_ != depthStencil_)
                {
                    BindDepthAttachment(texture->GetGPUObject(), false);
                    BindStencilAttachment(hasStencil ? texture->GetGPUObject() : 0, false);
                    fboBufferTgt.depthAttachment_ = depthStencil_;
                }
            }
            else
            {
                if (fboBufferTgt.depthAttachment_ != depthStencil_)
                {
                    BindDepthAttachment(renderBufferID, true);
                    BindStencilAttachment(hasStencil ? renderBufferID : 0, true);
                    fboBufferTgt.depthAttachment_ = depthStencil_;
                }
            }
        }
        else
        {
            if (fboBufferTgt.depthAttachment_)
            {
                BindDepthAttachment(0, false);
                BindStencilAttachment(0, false);
                fboBufferTgt.depthAttachment_ = nullptr;
            }
        }

#ifndef GL_ES_VERSION_2_0
        // Disable/enable sRGB write
        if (sRGBWriteSupport_)
        {
            bool sRGBWrite = renderTargets_[0] ? renderTargets_[0]->GetParentTexture()->GetSRGB() : sRGB_;
            if (sRGBWrite != impl_->sRGBWrite_)
            {
                if (sRGBWrite)
                    glEnable(GL_FRAMEBUFFER_SRGB_EXT);
                else
                    glDisable(GL_FRAMEBUFFER_SRGB_EXT);
                impl_->sRGBWrite_ = sRGBWrite;
            }
        }
#endif
    }

}

void Graphics::CleanupFramebuffers()
{
    if (!IsDeviceLost())
    {
        BindFramebuffer(impl_->systemFBO_);
        impl_->boundFBO_ = impl_->systemFBO_;
        impl_->fboDirty_ = true;

        for (auto & entry : impl_->frameBuffers_)
            DeleteFramebuffer(ELEMENT_VALUE(entry).fbo_);
    }
    else
        impl_->boundFBO_ = 0;
    impl_->frameBuffers_.clear();
}

void Graphics::ResetCachedState()
{
    for (unsigned i = 0; i < MAX_VERTEX_STREAMS; ++i)
    {
        vertexBuffers_[i] = nullptr;
        elementMasks_[i] = 0;
    }

    for (unsigned i = 0; i < MAX_TEXTURE_UNITS; ++i)
    {
        textures_[i] = nullptr;
        textureTypes_[i] = GL_NONE;
    }

    for (auto & elem : renderTargets_)
        elem = nullptr;

    depthStencil_ = nullptr;
    viewport_ = IntRect(0, 0, 0, 0);
    indexBuffer_ = nullptr;
    vertexShader_ = nullptr;
    pixelShader_ = nullptr;
    shaderProgram_ = nullptr;
    blendMode_ = BLEND_REPLACE;
    textureAnisotropy_ = 1;
    colorWrite_ = true;
    cullMode_ = CULL_NONE;
    constantDepthBias_ = 0.0f;
    slopeScaledDepthBias_ = 0.0f;
    depthTestMode_ = CMP_ALWAYS;
    depthWrite_ = false;
    fillMode_ = FILL_SOLID;
    scissorTest_ = false;
    scissorRect_ = IntRect::ZERO;
    stencilTest_ = false;
    stencilTestMode_ = CMP_ALWAYS;
    stencilPass_ = OP_KEEP;
    stencilFail_ = OP_KEEP;
    stencilZFail_ = OP_KEEP;
    stencilRef_ = 0;
    stencilCompareMask_ = M_MAX_UNSIGNED;
    stencilWriteMask_ = M_MAX_UNSIGNED;
    useClipPlane_ = false;
    lastInstanceOffset_ = 0;
    impl_->activeTexture_ = 0;
    impl_->enabledAttributes_ = 0;
    impl_->boundFBO_ = impl_->systemFBO_;
    impl_->boundVBO_ = 0;
    impl_->boundUBO_ = 0;
    impl_->sRGBWrite_ = false;

    // Set initial state to match Direct3D
    if (impl_->context_)
    {
        glEnable(GL_DEPTH_TEST);
        SetCullMode(CULL_CCW);
        SetDepthTest(CMP_LESSEQUAL);
        SetDepthWrite(true);
    }
    for (auto & elem : currentConstantBuffers_)
        elem = nullptr;
    dirtyConstantBuffers_.clear();
}

void Graphics::SetTextureUnitMappings()
{
    textureUnits_["DiffMap"] = TU_DIFFUSE;
    textureUnits_["DiffCubeMap"] = TU_DIFFUSE;
    textureUnits_["AlbedoBuffer"] = TU_ALBEDOBUFFER;
    textureUnits_["NormalMap"] = TU_NORMAL;
    textureUnits_["NormalBuffer"] = TU_NORMALBUFFER;
    textureUnits_["SpecMap"] = TU_SPECULAR;
    textureUnits_["EmissiveMap"] = TU_EMISSIVE;
    textureUnits_["EnvMap"] = TU_ENVIRONMENT;
    textureUnits_["EnvCubeMap"] = TU_ENVIRONMENT;
    textureUnits_["LightRampMap"] = TU_LIGHTRAMP;
    textureUnits_["LightSpotMap"] = TU_LIGHTSHAPE;
    textureUnits_["LightCubeMap"]  = TU_LIGHTSHAPE;
    textureUnits_["ShadowMap"] = TU_SHADOWMAP;
#ifdef DESKTOP_GRAPHICS
    textureUnits_["VolumeMap"] = TU_VOLUMEMAP;
    textureUnits_["FaceSelectCubeMap"] = TU_FACESELECT;
    textureUnits_["IndirectionCubeMap"] = TU_INDIRECTION;
    textureUnits_["DepthBuffer"] = TU_DEPTHBUFFER;
    textureUnits_["LightBuffer"] = TU_LIGHTBUFFER;
    textureUnits_["ZoneCubeMap"] = TU_ZONE;
    textureUnits_["ZoneVolumeMap"] = TU_ZONE;
#endif
}

unsigned Graphics::CreateFramebuffer()
{
    unsigned newFbo = 0;
#ifndef GL_ES_VERSION_2_0
    if (!gl3Support)
        glGenFramebuffersEXT(1, &newFbo);
    else
#endif
        glGenFramebuffers(1, &newFbo);
    return newFbo;
}

void Graphics::DeleteFramebuffer(unsigned fbo)
{
#ifndef GL_ES_VERSION_2_0
    if (!gl3Support)
        glDeleteFramebuffersEXT(1, &fbo);
    else
#endif
        glDeleteFramebuffers(1, &fbo);
}

void Graphics::BindFramebuffer(unsigned fbo)
{
#ifndef GL_ES_VERSION_2_0
    if (!gl3Support)
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo);
    else
#endif
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
}

void Graphics::BindColorAttachment(unsigned index, GLenum target, unsigned object)
{
    if (!gl3Support)
        glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT + index, target, object, 0);
    else
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + index, target, object, 0);
}

void Graphics::BindDepthAttachment(unsigned object, bool isRenderBuffer)
{
    if (!object)
        isRenderBuffer = false;

    if (!gl3Support)
    {
        if (!isRenderBuffer)
            glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, object, 0);
        else
            glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, object);
    }
    else
    {
        if (!isRenderBuffer)
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, object, 0);
        else
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, object);
    }
}

void Graphics::BindStencilAttachment(unsigned object, bool isRenderBuffer)
{
    if (!object)
        isRenderBuffer = false;

#ifndef GL_ES_VERSION_2_0
    if (!gl3Support)
    {
        if (!isRenderBuffer)
            glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_TEXTURE_2D, object, 0);
        else
            glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, object);
    }
    else
#endif
    {
        if (!isRenderBuffer)
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, object, 0);
        else
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, object);
    }
}

bool Graphics::CheckFramebuffer()
{
#ifndef GL_ES_VERSION_2_0
    if (!gl3Support)
        return glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) == GL_FRAMEBUFFER_COMPLETE_EXT;
    else
#endif
        return glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
}

void RegisterGraphicsLibrary(Context* context)
{
    Animation::RegisterObject(context);
    Material::RegisterObject(context);
    Model::RegisterObject(context);
    Shader::RegisterObject(context);
    Technique::RegisterObject(context);
    Texture2D::RegisterObject(context);
    Texture3D::RegisterObject(context);
    TextureCube::RegisterObject(context);
    Camera::RegisterObject(context);
    Drawable::RegisterObject(context);
    Light::RegisterObject(context);
    StaticModel::RegisterObject(context);
    StaticModelGroup::RegisterObject(context);
    Skybox::RegisterObject(context);
    AnimatedModel::RegisterObject(context);
    AnimationController::RegisterObject(context);
    BillboardSet::RegisterObject(context);
    ParticleEffect::RegisterObject(context);
    ParticleEmitter::RegisterObject(context);
    CustomGeometry::RegisterObject(context);
    DecalSet::RegisterObject(context);
    Terrain::RegisterObject(context);
    TerrainPatch::RegisterObject(context);
    DebugRenderer::RegisterObject(context);
    Octree::RegisterObject(context);
    Zone::RegisterObject(context);
}

}
