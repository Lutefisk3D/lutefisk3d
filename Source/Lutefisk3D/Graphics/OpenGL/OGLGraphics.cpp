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

#include "../AnimatedModel.h"
#include "../Animation.h"
#include "../AnimationController.h"
#include "../BillboardSet.h"
#include "../Camera.h"
#include "../ConstantBuffer.h"
#include "Lutefisk3D/Core/Context.h"
#include "Lutefisk3D/Core/StringUtils.h"
#include "../CustomGeometry.h"
#include "../DebugRenderer.h"
#include "../DecalSet.h"
#include "Lutefisk3D/IO/File.h"
#include "../Graphics.h"
#include "../GraphicsEvents.h"
#include "../GraphicsImpl.h"
#include "../IndexBuffer.h"
#include "Lutefisk3D/IO/Log.h"
#include "../Material.h"
#include "Lutefisk3D/Core/Mutex.h"
#include "../Octree.h"
#include "../ParticleEffect.h"
#include "../ParticleEmitter.h"
#include "Lutefisk3D/Core/ProcessUtils.h"
#include "Lutefisk3D/Core/Profiler.h"
#include "../RenderSurface.h"
#include "Lutefisk3D/Resource/ResourceCache.h"
#include "../RibbonTrail.h"
#include "../Shader.h"
#include "../ShaderPrecache.h"
#include "../ShaderProgram.h"
#include "../ShaderVariation.h"
#include "../Skybox.h"
#include "../StaticModelGroup.h"
#include "../Technique.h"
#include "../Terrain.h"
#include "../TerrainPatch.h"
#include "../VertexBuffer.h"
#include "../Zone.h"
#include "Lutefisk3D/Graphics/Texture2D.h"
#include "../Texture3D.h"
#include "../TextureCube.h"
#include "../Texture2DArray.h"
#include <GL/glew.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <stdio.h>
//using namespace gl;
#ifdef _WIN32
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
static const std::pair<GLenum,GLenum> glTranslatedBlend[MAX_BLENDMODES] = {
    {GL_ONE,GL_ZERO},   //BLEND_REPLACE
    {GL_ONE,GL_ONE},    //BLEND_ADD
    {GL_DST_COLOR,GL_ZERO},//BLEND_MULTIPLY
    {GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA}, //BLEND_ALPHA
    {GL_SRC_ALPHA,GL_ONE},                 //BLEND_ADDALPHA
    {GL_ONE,GL_ONE_MINUS_SRC_ALPHA},
    {GL_ONE_MINUS_DST_ALPHA,GL_DST_ALPHA},
    {GL_ONE,GL_ONE}, // subtract
    {GL_SRC_ALPHA,GL_ONE}, // subtract
    {GL_ZERO,GL_ONE_MINUS_SRC_COLOR},
};

static const GLenum glBlendOp[MAX_BLENDMODES] =
{
    GL_FUNC_ADD,
    GL_FUNC_ADD,
    GL_FUNC_ADD,
    GL_FUNC_ADD,
    GL_FUNC_ADD,
    GL_FUNC_ADD,
    GL_FUNC_ADD,
    GL_FUNC_REVERSE_SUBTRACT,
    GL_FUNC_REVERSE_SUBTRACT,
    GL_FUNC_ADD,
};

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

static const GLenum glElementTypes[] =
{
    GL_INT,
    GL_FLOAT,
    GL_FLOAT,
    GL_FLOAT,
    GL_FLOAT,
    GL_UNSIGNED_BYTE,
    GL_UNSIGNED_BYTE
};

static const unsigned glElementComponents[] = {1, 1, 2, 3, 4, 4, 4};

static QString extensions;

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

Graphics::Graphics(Context* context_) :
    m_context(context_),
    impl_(new GraphicsImpl()),
    position_(-1, -1),
    multiSample_(1),
    fullscreen_(false),
    borderless_(false),
    resizable_(false),
    highDPI_(false),
    vsync_(false),
    refreshRate_(0),
    monitor_(0),
    tripleBuffer_(false),
    sRGB_(false),
    ourWindowIsEmbedded_(false),
    lightPrepassSupport_(false),
    deferredSupport_(false),
    hardwareShadowSupport_(false),
    instancingSupport_(false),
    numPrimitives_(0),
    numBatches_(0),
    maxScratchBufferRequest_(0),
    dummyColorFormat_(GL_NONE),
    shadowMapFormat_(GL_DEPTH_COMPONENT16),
    hiresShadowMapFormat_(GL_DEPTH_COMPONENT24),
    defaultTextureFilterMode_(FILTER_TRILINEAR),
    defaultTextureAnisotropy_(4),
    shaderPath_("Shaders/GLSL/"),
    shaderExtension_(".glsl"),
    apiName_("GL3")
{
    SetTextureUnitMappings();
    ResetCachedState();
    glfwInit();
    // Register Graphics library object factories
    RegisterGraphicsLibrary(context_);
}

Graphics::~Graphics()
{
    Close();

    delete impl_;
    impl_ = nullptr;
}

bool Graphics::SetMode(int width, int height, bool fullscreen, bool borderless, bool resizable, bool highDPI, bool vsync, bool tripleBuffer, int multiSample, int monitor, int refreshRate)
{
    URHO3D_PROFILE_CTX(m_context,SetScreenMode);

    bool maximize = false;
    // Make sure monitor index is not bigger than the currently detected monitors
    int monitor_count=0;
    GLFWmonitor** known_monitors = glfwGetMonitors(&monitor_count);
    if (monitor >= monitor_count || monitor < 0)
        monitor = 0; // this monitor is not present, use first monitor
    GLFWmonitor* selected_monitor = known_monitors[monitor];

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
        glfwSwapInterval(vsync ? 1 : 0);
        vsync_ = vsync;
        return true;
    }
    const GLFWvidmode *current_mode=nullptr;
    // If zero dimensions in windowed mode, set windowed mode to maximize and set a predefined default restored window size.
    // If zero in fullscreen, use desktop mode
    if (!width || !height)
    {
        if (fullscreen || borderless)
        {
            current_mode = glfwGetVideoMode(selected_monitor);
            width = current_mode->width;
            height = current_mode->height;
        }
        else
        {
            maximize = resizable;
            width = 1024;
            height = 768;
        }
    }

    // Check fullscreen mode validity (desktop only). Use a closest match if not found
    if (fullscreen)
    {
        std::vector<IntVector3> resolutions = GetResolutions(monitor);
        if (resolutions.empty())
            fullscreen = false; // todo: report failure?
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
            refreshRate = resolutions[best].z_;
        }
    }

    if (!window2_)
    {
        // Close the existing window and OpenGL context, mark GPU objects as lost
        glfwWindowHint(GLFW_DOUBLEBUFFER,1);
        glfwWindowHint(GLFW_DEPTH_BITS,24);
        glfwWindowHint(GLFW_RED_BITS,8);
        glfwWindowHint(GLFW_GREEN_BITS,8);
        glfwWindowHint(GLFW_BLUE_BITS,8);
        glfwWindowHint(GLFW_ALPHA_BITS,0);
        glfwWindowHint(GLFW_STENCIL_BITS,8);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,2);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
        glfwWindowHint(GLFW_OPENGL_CORE_PROFILE,1);

        if (multiSample > 1)
        {
            glfwWindowHint(GLFW_SAMPLES,multiSample);
        }
        else
        {
            glfwWindowHint(GLFW_SAMPLES,0);
        }

        // Reposition the window on the specified monitor
        int monx=0,mony=0;
        glfwGetMonitorPos(selected_monitor,&monx,&mony);
        current_mode = glfwGetVideoMode(selected_monitor);

        bool reposition = fullscreen || (borderless && width >= current_mode->width && height >= current_mode->height);
        int x = reposition ? 0 : position_.x_;
        int y = reposition ? 0 : position_.y_;

        if (borderless) {
            // make it a borderless window by using current mode
            const GLFWvidmode* mode = glfwGetVideoMode(selected_monitor);
            glfwWindowHint(GLFW_RED_BITS, mode->redBits);
            glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
            glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
            glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
            //flags |= SDL_WINDOW_BORDERLESS;
        }
        if (resizable)
            glfwWindowHint(GLFW_RESIZABLE,1);
        if (highDPI) {
            //TODO: waiting for glfw3.3 ?
        }

        for (;;)
        {
            if(fullscreen||borderless)
                window2_= glfwCreateWindow(width,height,qPrintable(windowTitle_),selected_monitor,nullptr);
            else
                window2_= glfwCreateWindow(width,height,qPrintable(windowTitle_),nullptr,nullptr);
            if (window2_) {
                glfwSetWindowPos(window2_,x,y);
                break;
            }
            else
            {
                if (multiSample > 1)
                {
                    // If failed with multisampling, retry first without
                    glfwWindowHint(GLFW_SAMPLES,0);
                }
                else
                {
                    URHO3D_LOGERROR(QString("Could not create window"));
                    return false;
                }
            }
        }

        CreateWindowIcon();

        if (maximize)
        {
            Maximize();
            glfwGetFramebufferSize(window2_,&width, &height);
        }

        // Create/restore context and GPU objects and set initial renderstate
        Restore();

        // Specific error message is already logged by Restore() when context creation or OpenGL extensions check fails
        if (!glfwGetCurrentContext())
            return false;
    }

    // Set vsync
    glfwMakeContextCurrent(window2_);
    glfwSwapInterval(vsync ? 1 : 0);
    fullscreen_ = fullscreen;
    borderless_ = borderless;
    resizable_ = resizable;
    vsync_ = vsync;
    tripleBuffer_ = tripleBuffer;
    multiSample_ = multiSample;
    monitor_ = monitor;
    refreshRate_ = refreshRate;

    glfwGetFramebufferSize(window2_, &width_, &height_);
    if (!fullscreen)
        glfwGetWindowPos(window2_, &position_.x_, &position_.y_);

    int logicalWidth, logicalHeight;
    glfwGetWindowSize(window2_, &logicalWidth, &logicalHeight);
    highDPI_ = (width_ != logicalWidth) || (height_ != logicalHeight);

    // Reset rendertargets and viewport for the new screen mode
    ResetRenderTargets();

    // Clear the initial window contents to black
    Clear(CLEAR_COLOR);
    glfwSwapBuffers(window2_);

    CheckFeatureSupport();

#ifdef LUTEFISK3D_LOGGING
    QString msg  = QString("Set screen mode %1x%2 %3 monitor %4").arg(width_).arg(height_).arg((fullscreen_ ? "fullscreen" : "windowed")).arg(monitor);
    if (borderless_)
        msg.append(" borderless");
    if (resizable_)
        msg.append(" resizable");
    if (multiSample > 1)
        msg += QString(" multisample %1").arg(multiSample);
    URHO3D_LOGINFO(msg);
#endif

    g_graphicsSignals.newScreenMode(width_,height_,fullscreen_,borderless_,resizable_,highDPI_,
                                         monitor_,refreshRate_);
    return true;
}

bool Graphics::SetMode(int width, int height)
{
    return SetMode(width, height, fullscreen_, borderless_, resizable_, highDPI_, vsync_, tripleBuffer_, multiSample_, monitor_, refreshRate_);
}

void Graphics::SetSRGB(bool enable)
{
    if (enable != sRGB_)
    {
        sRGB_ = enable;
        impl_->fboDirty_ = true;
    }
}

void Graphics::SetDither(bool enable)
{
    if (enable)
        glEnable(GL_DITHER);
    else
        glDisable(GL_DITHER);
}

void Graphics::SetFlushGPU(bool enable)
{
    // Currently unimplemented on OpenGL
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
    URHO3D_PROFILE_CTX(m_context,TakeScreenShot);
    if (!IsInitialized())
        return false;

    if (IsDeviceLost())
    {
        URHO3D_LOGERROR("Can not take screenshot while device is lost");
        return false;
    }

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
    if (ourWindowIsEmbedded_)
    {
        int width, height;

        glfwGetFramebufferSize(window2_, &width, &height);
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

    g_graphicsSignals.beginRendering();

    return true;
}

void Graphics::EndFrame()
{
    if (!IsInitialized())
        return;

    URHO3D_PROFILE_CTX(m_context,Present);

    {
        URHO3D_PROFILE_CTX(m_context,EndRendering);
        g_graphicsSignals.endRendering();
    }

    {
        URHO3D_PROFILE_CTX(m_context,Swap);
        glfwSwapBuffers(window2_);
    }

    // Clean up too large scratch buffers
    {
        URHO3D_PROFILE_CTX(m_context,CleanScratch);
        CleanupScratchBuffers();
    }
}

void Graphics::Clear(unsigned flags, const Color& color, float depth, unsigned stencil)
{
    PrepareDraw();

    bool oldColorWrite = colorWrite_;
    bool oldDepthWrite = depthWrite_;

    if (flags & CLEAR_COLOR && !oldColorWrite)
        SetColorWrite(true);
    if (flags & CLEAR_DEPTH && !oldDepthWrite)
        SetDepthWrite(true);
    if (flags & CLEAR_STENCIL && stencilWriteMask_ != M_MAX_UNSIGNED)
        glStencilMask(M_MAX_UNSIGNED);

    uint32_t glFlags(GL_NONE);
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

    URHO3D_PROFILE_CTX(m_context,ResolveToTexture);

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

bool Graphics::ResolveToTexture(Texture2D* texture)
{
    if (!texture)
        return false;
    RenderSurface* surface = texture->GetRenderSurface();
    if (!surface || !surface->GetRenderBuffer())
        return false;

    URHO3D_PROFILE_CTX(m_context,ResolveToTexture);

    texture->SetResolveDirty(false);
    surface->SetResolveDirty(false);

    // Use separate FBOs for resolve to not disturb the currently set rendertarget(s)
    if (!impl_->resolveSrcFBO_)
        impl_->resolveSrcFBO_ = CreateFramebuffer();
    if (!impl_->resolveDestFBO_)
        impl_->resolveDestFBO_ = CreateFramebuffer();

    glBindFramebuffer(GL_READ_FRAMEBUFFER, impl_->resolveSrcFBO_);
    glFramebufferRenderbuffer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, surface->GetRenderBuffer());
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, impl_->resolveDestFBO_);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture->GetGPUObject(), 0);
    glBlitFramebuffer(0, 0, texture->GetWidth(), texture->GetHeight(), 0, 0, texture->GetWidth(), texture->GetHeight(),
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    // Restore previously bound FBO
    BindFramebuffer(impl_->boundFBO_);
    return true;
}

bool Graphics::ResolveToTexture(TextureCube* texture)
{
    if (!texture)
        return false;

    URHO3D_PROFILE_CTX(m_context,ResolveToTexture);

    texture->SetResolveDirty(false);

    // Use separate FBOs for resolve to not disturb the currently set rendertarget(s)
    if (!impl_->resolveSrcFBO_)
        impl_->resolveSrcFBO_ = CreateFramebuffer();
    if (!impl_->resolveDestFBO_)
        impl_->resolveDestFBO_ = CreateFramebuffer();

    for (unsigned i = 0; i < MAX_CUBEMAP_FACES; ++i)
    {
        RenderSurface* surface = texture->GetRenderSurface((CubeMapFace)i);
        if (!surface->IsResolveDirty())
            continue;

        surface->SetResolveDirty(false);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, impl_->resolveSrcFBO_);
        glFramebufferRenderbuffer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, surface->GetRenderBuffer());
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, impl_->resolveDestFBO_);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                               texture->GetGPUObject(), 0);
        glBlitFramebuffer(0, 0, texture->GetWidth(), texture->GetHeight(), 0, 0, texture->GetWidth(), texture->GetHeight(),
                          GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }

    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    // Restore previously bound FBO
    BindFramebuffer(impl_->boundFBO_);
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
void Graphics::Draw(PrimitiveType type, unsigned indexStart, unsigned indexCount, unsigned baseVertexIndex, unsigned minVertex, unsigned vertexCount)
{
    if (!indexCount || !indexBuffer_ || !indexBuffer_->GetGPUObject())
        return;

    PrepareDraw();

    unsigned indexSize = indexBuffer_->GetIndexSize();
    unsigned primitiveCount;
    GLenum glPrimitiveType;

    GetGLPrimitiveType(indexCount, type, primitiveCount, glPrimitiveType);
    GLenum indexType = indexSize == sizeof(unsigned short) ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
    glDrawElementsBaseVertex(glPrimitiveType, indexCount, indexType, reinterpret_cast<GLvoid*>(indexStart * indexSize), baseVertexIndex);

    numPrimitives_ += primitiveCount;
    ++numBatches_;
}
void Graphics::DrawInstanced(PrimitiveType type, unsigned indexStart, unsigned indexCount, unsigned minVertex, unsigned vertexCount, unsigned instanceCount)
{
    if (!indexCount || !indexBuffer_ || !indexBuffer_->GetGPUObject() || !instancingSupport_)
        return;

    PrepareDraw();

    unsigned indexSize = indexBuffer_->GetIndexSize();
    unsigned primitiveCount;
    GLenum glPrimitiveType;

    GetGLPrimitiveType(indexCount, type, primitiveCount, glPrimitiveType);
    GLenum indexType = indexSize == sizeof(unsigned short) ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
    glDrawElementsInstanced(glPrimitiveType, indexCount, indexType, reinterpret_cast<const GLvoid*>(indexStart * indexSize),
                            instanceCount);

    numPrimitives_ += instanceCount * primitiveCount;
    ++numBatches_;
}
void Graphics::DrawInstanced(PrimitiveType type, unsigned indexStart, unsigned indexCount, unsigned baseVertexIndex, unsigned minVertex,
                             unsigned vertexCount, unsigned instanceCount)
{
    if (!indexCount || !indexBuffer_ || !indexBuffer_->GetGPUObject() || !instancingSupport_)
        return;

    PrepareDraw();

    unsigned indexSize = indexBuffer_->GetIndexSize();
    unsigned primitiveCount;
    GLenum glPrimitiveType;

    GetGLPrimitiveType(indexCount, type, primitiveCount, glPrimitiveType);
    GLenum indexType = indexSize == sizeof(unsigned short) ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;

    glDrawElementsInstancedBaseVertex(glPrimitiveType, indexCount, indexType, reinterpret_cast<const GLvoid*>(indexStart * indexSize),
                                      instanceCount, baseVertexIndex);

    numPrimitives_ += instanceCount * primitiveCount;
    ++numBatches_;
}
void Graphics::SetVertexBuffer(VertexBuffer* buffer)
{
    // Note: this is not multi-instance safe
    static std::vector<VertexBuffer*> vertexBuffers(1);
    vertexBuffers[0] = buffer;
    SetVertexBuffers(vertexBuffers);
}

bool Graphics::SetVertexBuffers(const std::vector<VertexBuffer*>& buffers, unsigned instanceOffset)
{
    if (buffers.size() > MAX_VERTEX_STREAMS)
    {
        URHO3D_LOGERROR("Too many vertex buffers");
        return false;
    }
    if (instanceOffset != impl_->lastInstanceOffset_)
    {
        impl_->lastInstanceOffset_ = instanceOffset;
        impl_->vertexBuffersDirty_ = true;
    }

    for (unsigned i = 0; i < MAX_VERTEX_STREAMS; ++i)
    {
        VertexBuffer* buffer = nullptr;
        if (i < buffers.size())
            buffer = buffers[i];
        if (buffer != vertexBuffers_[i])
        {
            vertexBuffers_[i] = buffer;
            impl_->vertexBuffersDirty_ = true;
        }
    }

    return true;
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
            URHO3D_PROFILE_CTX(m_context,CompileVertexShader);

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
            URHO3D_PROFILE_CTX(m_context,CompilePixelShader);

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
        vertexShader_ = 0;
        pixelShader_ = 0;
        impl_->shaderProgram_ = 0;
    }
    else
    {
        vertexShader_ = vs;
        pixelShader_ = ps;

        std::pair<ShaderVariation*, ShaderVariation*> combination(vs, ps);
        ShaderProgramMap::iterator i = impl_->shaderPrograms_.find(combination);

        if (i != impl_->shaderPrograms_.end())
        {
            // Use the existing linked program
            if (MAP_VALUE(i)->GetGPUObject())
            {
                glUseProgram(MAP_VALUE(i)->GetGPUObject());
                impl_->shaderProgram_ = MAP_VALUE(i);
            }
            else
            {
                glUseProgram(0);
                impl_->shaderProgram_ = nullptr;
            }
        }
        else
        {
            // Link a new combination
            URHO3D_PROFILE_CTX(m_context,LinkShaders);

            SharedPtr<ShaderProgram> newProgram(new ShaderProgram(this, vs, ps));
            if (newProgram->Link())
            {
                URHO3D_LOGDEBUG("Linked vertex shader " + vs->GetFullName() + " and pixel shader " + ps->GetFullName());
                // Note: Link() calls glUseProgram() to set the texture sampler uniforms,
                // so it is not necessary to call it again
                impl_->shaderProgram_ = newProgram;
            }
            else
            {
                URHO3D_LOGERROR("Failed to link vertex shader " + vs->GetFullName() + " and pixel shader " + ps->GetFullName() + ":\n" +
                                newProgram->GetLinkerOutput());
                glUseProgram(0);
                impl_->shaderProgram_ = nullptr;
            }

            impl_->shaderPrograms_[combination] = newProgram;
        }
    }

    // Update the clip plane uniform on GL3, and set constant buffers
    if (impl_->shaderProgram_)
    {
        const SharedPtr<ConstantBuffer>* constantBuffers = impl_->shaderProgram_->GetConstantBuffers();
        for (unsigned i = 0; i < MAX_SHADER_PARAMETER_GROUPS * 2; ++i)
        {
            ConstantBuffer* buffer = constantBuffers[i].Get();
            if (buffer != impl_->constantBuffers_[i])
            {
                unsigned object = buffer ? buffer->GetGPUObject() : 0;
                glBindBufferBase(GL_UNIFORM_BUFFER, i, object);
                // Calling glBindBufferBase also affects the generic buffer binding point
                impl_->boundUBO_ = object;
                impl_->constantBuffers_[i] = buffer;
                ShaderProgram::ClearGlobalParameterSource((ShaderParameterGroup)(i % MAX_SHADER_PARAMETER_GROUPS));
            }
        }

        SetShaderParameter(VSP_CLIPPLANE, useClipPlane_ ? clipPlane_ : Vector4(0.0f, 0.0f, 0.0f, 1.0f));
    }

    // Store shader combination if shader dumping in progress
    if (shaderPrecache_)
        shaderPrecache_->StoreShaders(vertexShader_, pixelShader_);
    if (impl_->shaderProgram_)
    {
        impl_->usedVertexAttributes_ = impl_->shaderProgram_->GetUsedVertexAttributes();
        impl_->vertexAttributes_ = &impl_->shaderProgram_->GetVertexAttributes();
    }
    else
    {
        impl_->usedVertexAttributes_ = 0;
        impl_->vertexAttributes_ = 0;
    }

    impl_->vertexBuffersDirty_ = true;
}

void Graphics::SetShaderParameter(StringHash param, const float* data, unsigned count)
{
    if (impl_->shaderProgram_)
    {
        const ShaderParameter* info = impl_->shaderProgram_->GetParameter(param);
        if (info)
        {
            if (info->bufferPtr_)
            {
                ConstantBuffer* buffer = info->bufferPtr_;
                if (!buffer->IsDirty())
                    impl_->dirtyConstantBuffers_.push_back(buffer);
                buffer->SetParameter(info->offset_, (unsigned)(count * sizeof(float)), data);
                return;
            }
            switch (info->glType_)
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
    if (impl_->shaderProgram_)
    {
        const ShaderParameter* info = impl_->shaderProgram_->GetParameter(param);
        if (info)
        {
            if (info->bufferPtr_)
            {
                ConstantBuffer* buffer = info->bufferPtr_;
                if (!buffer->IsDirty())
                    impl_->dirtyConstantBuffers_.push_back(buffer);
                buffer->SetParameter(info->offset_, sizeof(float), &value);
                return;
            }
            glUniform1fv(info->location_, 1, &value);
        }
    }
}

void Graphics::SetShaderParameter(StringHash param, int value)
{
    if (impl_->shaderProgram_)
    {
        const ShaderParameter* info = impl_->shaderProgram_->GetParameter(param);
        if (info)
        {
            if (info->bufferPtr_)
            {
                ConstantBuffer* buffer = info->bufferPtr_;
                if (!buffer->IsDirty())
                    impl_->dirtyConstantBuffers_.push_back(buffer);
                buffer->SetParameter(info->offset_, sizeof(int), &value);
                return;
            }

            glUniform1i(info->location_, value);
        }
    }
}

void Graphics::SetShaderParameter(StringHash param, bool value)
{
    // \todo Not tested
    if (impl_->shaderProgram_)
    {
        const ShaderParameter* info = impl_->shaderProgram_->GetParameter(param);
        if (info)
        {
            if (info->bufferPtr_)
            {
                ConstantBuffer* buffer = info->bufferPtr_;
                if (!buffer->IsDirty())
                    impl_->dirtyConstantBuffers_.push_back(buffer);
                buffer->SetParameter(info->offset_, sizeof(bool), &value);
                return;
            }

            glUniform1i(info->location_, (int)value);
        }
    }
}
void Graphics::SetShaderParameter(StringHash param, const Color& color)
{
    SetShaderParameter(param, color.Data(), 4);
}

void Graphics::SetShaderParameter(StringHash param, const Vector2& vector)
{
    if (impl_->shaderProgram_)
    {
        const ShaderParameter* info = impl_->shaderProgram_->GetParameter(param);
        if (info)
        {
            if (info->bufferPtr_)
            {
                ConstantBuffer* buffer = info->bufferPtr_;
                if (!buffer->IsDirty())
                    impl_->dirtyConstantBuffers_.push_back(buffer);
                buffer->SetParameter(info->offset_, sizeof(Vector2), &vector);
                return;
            }
            // Check the uniform type to avoid mismatch
            switch (info->glType_)
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
    if (impl_->shaderProgram_)
    {
        const ShaderParameter* info = impl_->shaderProgram_->GetParameter(param);
        if (info)
        {
            if (info->bufferPtr_)
            {
                ConstantBuffer* buffer = info->bufferPtr_;
                if (!buffer->IsDirty())
                    impl_->dirtyConstantBuffers_.push_back(buffer);
                buffer->SetVector3ArrayParameter(info->offset_, 3, &matrix);
                return;
            }

            glUniformMatrix3fv(info->location_, 1, GL_FALSE, matrix.Data());
        }
    }
}

void Graphics::SetShaderParameter(StringHash param, const Vector3& vector)
{
    if (impl_->shaderProgram_)
    {
        const ShaderParameter* info = impl_->shaderProgram_->GetParameter(param);
        if (info)
        {
            if (info->bufferPtr_)
            {
                ConstantBuffer* buffer = info->bufferPtr_;
                if (!buffer->IsDirty())
                    impl_->dirtyConstantBuffers_.push_back(buffer);
                buffer->SetParameter(info->offset_, sizeof(Vector3), &vector);
                return;
            }
            // Check the uniform type to avoid mismatch
            switch (info->glType_)
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
    if (impl_->shaderProgram_)
    {
        const ShaderParameter* info = impl_->shaderProgram_->GetParameter(param);
        if (info)
        {
            if (info->bufferPtr_)
            {
                ConstantBuffer* buffer = info->bufferPtr_;
                if (!buffer->IsDirty())
                    impl_->dirtyConstantBuffers_.push_back(buffer);
                buffer->SetParameter(info->offset_, sizeof(Matrix4), &matrix);
                return;
            }

            glUniformMatrix4fv(info->location_, 1, GL_FALSE, matrix.Data());
        }
    }
}

void Graphics::SetShaderParameter(StringHash param, const Vector4& vector)
{
    if (impl_->shaderProgram_)
    {
        const ShaderParameter* info = impl_->shaderProgram_->GetParameter(param);
        if (info)
        {
            if (info->bufferPtr_)
            {
                ConstantBuffer* buffer = info->bufferPtr_;
                if (!buffer->IsDirty())
                    impl_->dirtyConstantBuffers_.push_back(buffer);
                buffer->SetParameter(info->offset_, sizeof(Vector4), &vector);
                return;
            }
            // Check the uniform type to avoid mismatch
            switch (info->glType_)
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
    if (impl_->shaderProgram_)
    {
        const ShaderParameter* info = impl_->shaderProgram_->GetParameter(param);
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
                    impl_->dirtyConstantBuffers_.push_back(buffer);
                buffer->SetParameter(info->offset_, sizeof(Matrix4), &fullMatrix);
                return;
            }

            glUniformMatrix4fv(info->location_, 1, GL_FALSE, fullMatrix.Data());
        }
    }
}

bool Graphics::NeedParameterUpdate(ShaderParameterGroup group, const void* source)
{
    return impl_->shaderProgram_ ? impl_->shaderProgram_->NeedParameterUpdate(group, source) : false;
}

bool Graphics::HasShaderParameter(StringHash param)
{
    return impl_->shaderProgram_ && impl_->shaderProgram_->HasParameter(param);
}

bool Graphics::HasTextureUnit(TextureUnit unit)
{
    return impl_->shaderProgram_ && impl_->shaderProgram_->HasTextureUnit(unit);
}

void Graphics::ClearParameterSource(ShaderParameterGroup group)
{
    if (impl_->shaderProgram_)
        impl_->shaderProgram_->ClearParameterSource(group);
}

void Graphics::ClearParameterSources()
{
    ShaderProgram::ClearParameterSources();
}

void Graphics::ClearTransformSources()
{
    if (impl_->shaderProgram_)
    {
        impl_->shaderProgram_->ClearParameterSource(SP_CAMERA);
        impl_->shaderProgram_->ClearParameterSource(SP_OBJECT);
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
        else
        {
            // Resolve multisampled texture now as necessary
            if (texture->GetMultiSample() > 1 && texture->GetAutoResolve() && texture->IsResolveDirty())
            {
                if (texture->GetType() == Texture2D::GetTypeStatic())
                    ResolveToTexture(static_cast<Texture2D*>(texture));
                if (texture->GetType() == TextureCube::GetTypeStatic())
                    ResolveToTexture(static_cast<TextureCube*>(texture));
            }
        }
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
            if (GL_NONE!=impl_->textureTypes_[index] && impl_->textureTypes_[index] != glType)
                glBindTexture(impl_->textureTypes_[index], 0);
            glBindTexture(glType, texture->GetGPUObject());
            impl_->textureTypes_[index] = glType;

            if (texture->GetParametersDirty())
                texture->UpdateParameters();
            if (texture->GetLevelsDirty())
                texture->RegenerateLevels();
        }
        else if (GL_NONE!=impl_->textureTypes_[index])
        {
            glBindTexture(impl_->textureTypes_[index], 0);
            impl_->textureTypes_[index] = GL_NONE;
        }
        textures_[index] = texture;
    }
    else
    {
        if (texture && (texture->GetParametersDirty() || texture->GetLevelsDirty()))
        {
            if (impl_->activeTexture_ != index)
            {
                glActiveTexture(GL_TEXTURE0 + index);
                impl_->activeTexture_ = index;
            }

            glBindTexture(texture->GetTarget(), texture->GetGPUObject());
            if (texture->GetParametersDirty())
                texture->UpdateParameters();
            if (texture->GetLevelsDirty())
                texture->RegenerateLevels();
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
    if (impl_->textureTypes_[0]!=GL_NONE && impl_->textureTypes_[0] != glType)
        glBindTexture(impl_->textureTypes_[0], 0);
    glBindTexture(glType, texture->GetGPUObject());
    impl_->textureTypes_[0] = glType;
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

void Graphics::SetDefaultTextureAnisotropy(unsigned level)
{
    level = Max(level, 1U);

    if (level != defaultTextureAnisotropy_)
    {
        defaultTextureAnisotropy_ = level;
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
            // If multisampled, mark the texture & surface needing resolve
            if (parentTexture->GetMultiSample() > 1 && parentTexture->GetAutoResolve())
            {
                parentTexture->SetResolveDirty(true);
                renderTarget->SetResolveDirty(true);
            }
            // If mipmapped, mark the levels needing regeneration
            if (parentTexture->GetLevels() > 1)
                parentTexture->SetLevelsDirty();
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
    // Only do this for non-multisampled rendertargets; when using multisampled target a similarly multisampled
    // depth-stencil should also be provided (backbuffer depth isn't compatible)
    if (renderTargets_[0] && renderTargets_[0]->GetMultiSample() == 1 && !depthStencil)
    {
        int width = renderTargets_[0]->GetWidth();
        int height = renderTargets_[0]->GetHeight();

        // Direct3D9 default depth-stencil can not be used when rendertarget is larger than the window.
        // Check size similarly
        if (width <= width_ && height <= height_)
        {
            int searchKey = (width << 16) | height;
            auto i = impl_->depthTextures_.find(searchKey);
            if (i != impl_->depthTextures_.end())
                depthStencil = MAP_VALUE(i)->GetRenderSurface();
            else
            {
                SharedPtr<Texture2D> newDepthTexture(new Texture2D(m_context));
                newDepthTexture->SetSize(width, height, GetDepthStencilFormat(), TEXTURE_DEPTHSTENCIL);
                impl_->depthTextures_[searchKey] = newDepthTexture;
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
/// Set blending and alpha-to-coverage modes.
void Graphics::SetBlendMode(BlendMode mode, bool alphaToCoverage)
{
    if (mode != blendMode_)
    {
        if (mode == BLEND_REPLACE)
            glDisable(GL_BLEND);
        else
        {
            glEnable(GL_BLEND);
            glBlendFunc(glTranslatedBlend[mode].first, glTranslatedBlend[mode].second);
            glBlendEquation(glBlendOp[mode]);
        }

        blendMode_ = mode;
    }
    if (alphaToCoverage != alphaToCoverage_)
    {
        if (alphaToCoverage)
            glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
        else
            glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);

        alphaToCoverage_ = alphaToCoverage;
    }
}
/// Set color write on/off.
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
/// Set hardware culling mode.
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
        if (slopeScaledBias != 0.0f)
        {
            // OpenGL constant bias is unreliable and dependent on depth buffer bitdepth, apply in the projection matrix instead
            glEnable(GL_POLYGON_OFFSET_FILL);
            glPolygonOffset(slopeScaledBias, 0.0f);
        }
        else
            glDisable(GL_POLYGON_OFFSET_FILL);

        constantDepthBias_ = constantBias;
        slopeScaledDepthBias_ = slopeScaledBias;
        // Force update of the projection matrix shader parameter
        ClearParameterSource(SP_CAMERA);
    }
}
/// Set depth compare.
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
    if (mode != fillMode_)
    {
        glPolygonMode(GL_FRONT_AND_BACK, glFillMode[mode]);
        fillMode_ = mode;
    }
}

void Graphics::SetLineAntiAlias(bool enable)
{
    if (enable != lineAntiAlias_)
    {
        if (enable)
            glEnable(GL_LINE_SMOOTH);
        else
            glDisable(GL_LINE_SMOOTH);
        lineAntiAlias_ = enable;
    }
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
    }
}


void Graphics::SetStencilTest(bool enable, CompareMode mode, StencilOp pass, StencilOp fail, StencilOp zFail, unsigned stencilRef, unsigned compareMask, unsigned writeMask)
{
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
}

bool Graphics::IsInitialized() const
{
    return window2_ != nullptr;
}

bool Graphics::GetDither() const
{
    return glIsEnabled(GL_DITHER) ? true : false;
}

bool Graphics::IsDeviceLost() const
{
    return window2_==nullptr;
}

std::vector<int> Graphics::GetMultiSampleLevels() const
{
    std::vector<int> ret;
    // No multisampling always supported
    ret.push_back(1);
    int maxSamples = 0;
    glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
    for (int i = 2; i <= maxSamples && i <= 16; i *= 2)
        ret.push_back(i);

    return ret;
}

uint32_t Graphics::GetFormat(CompressedFormat format) const
{
    switch (format)
    {
    case CF_RGBA:
        return GL_RGBA;

    case CF_DXT1:
        return GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;

    case CF_DXT3:
        return GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;

    case CF_DXT5:
        return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;

    default:
        return GL_NONE;
    }
}

unsigned Graphics::GetMaxBones()
{
    return 128;
}
/// Return a shader variation by name and defines.
ShaderVariation* Graphics::GetShader(ShaderType type, const QString& name, const QString& defines) const
{
    return GetShader(type, qPrintable(name), qPrintable(defines));
}

ShaderVariation* Graphics::GetShader(ShaderType type, const char* name, const char* defines) const
{
    if (lastShaderName_ != name || !lastShader_)
    {
        ResourceCache* cache = m_context->m_ResourceCache.get();

        QString fullShaderName = shaderPath_ + name + shaderExtension_;
        // Try to reduce repeated error log prints because of missing shaders
        if (lastShaderName_ == name && !cache->Exists(fullShaderName))
            return nullptr;

        lastShader_ = cache->GetResource<Shader>(fullShaderName);
        lastShaderName_ = name;
    }

    return lastShader_ ? lastShader_->GetVariation(type, defines) : nullptr;
}

VertexBuffer* Graphics::GetVertexBuffer(unsigned index) const
{
    return index < MAX_VERTEX_STREAMS ? vertexBuffers_[index] : nullptr;
}

ShaderProgram* Graphics::GetShaderProgram() const
{
    return impl_->shaderProgram_;
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

void Graphics::OnWindowResized()
{
    if (!window2_)
        return;

    int newWidth, newHeight;

    glfwGetFramebufferSize(window2_, &newWidth, &newHeight);
    if (newWidth == width_ && newHeight == height_)
        return;

    width_ = newWidth;
    height_ = newHeight;

    int logicalWidth, logicalHeight;
    glfwGetWindowSize(window2_, &logicalWidth, &logicalHeight);
    highDPI_ = (width_ != logicalWidth) || (height_ != logicalHeight);

    // Reset rendertargets and viewport for the new screen size. Also clean up any FBO's, as they may be screen size
    // dependent
    CleanupFramebuffers();
    ResetRenderTargets();

    URHO3D_LOGDEBUG(QString("Window was resized to %1x%2").arg(width_).arg(height_));

    g_graphicsSignals.newScreenMode(width_,height_,fullscreen_,borderless_,resizable_,highDPI_,
                                         monitor_,refreshRate_);

}

void Graphics::OnWindowMoved()
{
    if (!window2_ || fullscreen_)
        return;

    int newX, newY;

    glfwGetWindowPos(window2_, &newX, &newY);
    if (newX == position_.x_ && newY == position_.y_)
        return;

    position_.x_ = newX;
    position_.y_ = newY;

    URHO3D_LOGDEBUG(QString("Window was moved to %1,%2").arg(position_.x_).arg(position_.y_));

    g_graphicsSignals.windowPos(position_.x_,position_.y_);
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
                BindColorAttachment(j, GL_TEXTURE_2D, 0,false);
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
    for (ShaderProgramMap::iterator i = impl_->shaderPrograms_.begin(); i != impl_->shaderPrograms_.end();)
    {
        if (MAP_VALUE(i)->GetVertexShader() == variation || MAP_VALUE(i)->GetPixelShader() == variation)
            i = impl_->shaderPrograms_.erase(i);
        else
            ++i;
    }

    if (vertexShader_ == variation || pixelShader_ == variation)
        impl_->shaderProgram_ = nullptr;
}

ConstantBuffer* Graphics::GetOrCreateConstantBuffer(ShaderType /*type*/, unsigned bindingIndex, unsigned size)
{
    // Note: shaderType parameter is not used on OpenGL, instead binding index should already use the PS range
    // for PS constant buffers

    unsigned key = (bindingIndex << 16) | size;
    auto i = impl_->allConstantBuffers_.find(key);
    if (i != impl_->allConstantBuffers_.end())
        return MAP_VALUE(i).Get();

    auto iter=impl_->allConstantBuffers_.emplace(key, SharedPtr<ConstantBuffer>(new ConstantBuffer(m_context))).first->second;
    iter->SetSize(size);
    return iter.Get();
}
void Graphics::Release(bool clearGPUObjects, bool closeWindow)
{
    if (!window2_)
        return;

    {
        MutexLock lock(gpuObjectMutex_);

        if (clearGPUObjects)
        {
            // Shutting down: release all GPU objects that still exist
            // Shader programs are also GPU objects; clear them first to avoid list modification during iteration
            impl_->shaderPrograms_.clear();

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
            impl_->shaderPrograms_.clear();
            g_graphicsSignals.deviceLost();
        }
    }

    CleanupFramebuffers();
    impl_->depthTextures_.clear();

    // End fullscreen mode first to counteract transition and getting stuck problems on OS X
#if defined(__APPLE__)
    if (closeWindow && fullscreen_ && !externalWindow_)
        SDL_SetWindowFullscreen(window_, 0);
#endif

    if (closeWindow)
    {
        glfwSetInputMode(window2_, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

        // Do not destroy external window except when shutting down
        if (!ourWindowIsEmbedded_ || clearGPUObjects)
        {
            glfwDestroyWindow(window2_);
            window2_ = nullptr;
        }
    }
}

void Graphics::Restore()
{
    if (!window2_)
        return;

    // Ensure first that the context exists
    glfwMakeContextCurrent(window2_);

    // Initialize OpenGL extensions library (desktop only)
    glewExperimental=true;
    glewInit();

    if (!GLEW_VERSION_3_2)
    {
        URHO3D_LOGERROR(QString("Lutefisk does not support OpenGL older than 3.2"));
        return;
    }
    apiName_ = "GL3";

    // Create and bind a vertex array object that will stay in use throughout
    unsigned vertexArrayObject;
    glGenVertexArrays(1, &vertexArrayObject);
    glBindVertexArray(vertexArrayObject);
    // Enable seamless cubemap if possible
    // Note: even though we check the extension, this can lead to software fallback on some old GPU's
    // See https://github.com/urho3d/Urho3D/issues/1380 or
    // http://distrustsimplicity.net/articles/gl_texture_cube_map_seamless-on-os-x/
    // In case of trouble or for wanting maximum compatibility, simply remove the glEnable below.
    if (GLEW_ARB_seamless_cube_map)
        glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    // Set up texture data read/write alignment. It is important that this is done before uploading any texture data
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    ResetCachedState();

    {
        MutexLock lock(gpuObjectMutex_);

        for (GPUObject * elem : gpuObjects_)
            elem->OnDeviceReset();
    }
    g_graphicsSignals.deviceReset();
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
    if (impl_->boundUBO_ != object)
    {
        if (object)
            glBindBuffer(GL_UNIFORM_BUFFER, object);
        impl_->boundUBO_ = object;
    }
}

uint32_t Graphics::GetAlphaFormat()
{
    // Alpha format is deprecated on OpenGL 3+
    return GL_R8;
}

uint32_t Graphics::GetLuminanceFormat()
{
    // Luminance format is deprecated on OpenGL 3+
    return GL_R8;
}

uint32_t Graphics::GetLuminanceAlphaFormat()
{
    // Luminance alpha format is deprecated on OpenGL 3+
    return GL_RG8;
}

uint32_t Graphics::GetRGBFormat()
{
    return GL_RGB;
}

uint32_t Graphics::GetRGBAFormat()
{
    return GL_RGBA;
}

uint32_t Graphics::GetRGBA16Format()
{
    return GL_RGBA16;
}

uint32_t Graphics::GetRGBAFloat16Format()
{
    return GL_RGBA16F_ARB;
}

uint32_t Graphics::GetRGBAFloat32Format()
{
    return GL_RGBA32F_ARB;
}

uint32_t Graphics::GetRG16Format()
{
    return GL_RG16;
}

uint32_t Graphics::GetRGFloat16Format()
{
    return GL_RG16F;
}

uint32_t Graphics::GetRGFloat32Format()
{
    return GL_RG32F;
}

uint32_t Graphics::GetFloat16Format()
{
    return GL_R16F;
}

uint32_t Graphics::GetFloat32Format()
{
    return GL_R32F;
}

uint32_t Graphics::GetLinearDepthFormat()
{
    // OpenGL 3 can use different color attachment formats
    return GL_R32F;
}

uint32_t Graphics::GetDepthStencilFormat()
{
    return GL_DEPTH24_STENCIL8_EXT;
}

uint32_t Graphics::GetReadableDepthFormat()
{
    return GL_DEPTH_COMPONENT24;
}

uint32_t Graphics::GetFormat(const QString& formatName)
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

void Graphics::CheckFeatureSupport()
{
    // Check supported features: light pre-pass, deferred rendering and hardware depth texture
    lightPrepassSupport_ = false;
    deferredSupport_ = false;

    int numSupportedRTs = 1;
    instancingSupport_ = glDrawElementsInstanced != nullptr && glVertexAttribDivisor != nullptr;

    glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &numSupportedRTs);

    // Must support 2 rendertargets for light pre-pass, and 4 for deferred
    if (numSupportedRTs >= 2)
        lightPrepassSupport_ = true;
    if (numSupportedRTs >= 4)
        deferredSupport_ = true;

#if defined(__APPLE__)
    // On OS X check for an Intel driver and use shadow map RGBA dummy color textures, because mixing
    // depth-only FBO rendering and backbuffer rendering will bug, resulting in a black screen in full
    // screen mode, and incomplete shadow maps in windowed mode
    String renderer((const char*)glGetString(GL_RENDERER));
    if (renderer.contains("Intel", false))
        dummyColorFormat_ = GetRGBAFormat();
#endif

    // Consider OpenGL shadows always hardware sampled, if supported at all
    hardwareShadowSupport_ = shadowMapFormat_ != GL_NONE;
}

void Graphics::PrepareDraw()
{
    for (std::vector<ConstantBuffer*>::iterator i = impl_->dirtyConstantBuffers_.begin(); i != impl_->dirtyConstantBuffers_.end(); ++i)
        (*i)->Apply();
    impl_->dirtyConstantBuffers_.clear();
    if (impl_->fboDirty_)
    {
        impl_->fboDirty_ = false;

        // First check if no framebuffer is needed. In that case simply return to backbuffer rendering
        bool noFbo = !depthStencil_;
        if (noFbo)
        {
            for (unsigned i = 0; i < MAX_RENDERTARGETS; ++i)
            {
                if (renderTargets_[i])
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

            // Disable/enable sRGB write
            bool sRGBWrite = sRGB_;
            if (sRGBWrite != impl_->sRGBWrite_)
            {
                if (sRGBWrite)
                    glEnable(GL_FRAMEBUFFER_SRGB_EXT);
                else
                    glDisable(GL_FRAMEBUFFER_SRGB_EXT);
                impl_->sRGBWrite_ = sRGBWrite;
            }
            return;
        }

        // Search for a new framebuffer based on format & size, or create new
        IntVector2 rtSize = Graphics::GetRenderTargetDimensions();
        GLenum format = GL_NONE;
        if (renderTargets_[0])
            format = renderTargets_[0]->GetParentTexture()->GetFormat();
        else if (depthStencil_)
            format = depthStencil_->GetParentTexture()->GetFormat();

        uint64_t fboKey = (rtSize.x_ << 16 | rtSize.y_) | (((uint64_t)format) << 32);

        HashMap<uint64_t, FrameBufferObject>::iterator i = impl_->frameBuffers_.find(fboKey);
        if (i == impl_->frameBuffers_.end())
        {
            FrameBufferObject newFbo;
            newFbo.fbo_ = CreateFramebuffer();
            i = impl_->frameBuffers_.insert({fboKey, newFbo}).first;
        }

        if (impl_->boundFBO_ != MAP_VALUE(i).fbo_)
        {
            BindFramebuffer(MAP_VALUE(i).fbo_);
            impl_->boundFBO_ = MAP_VALUE(i).fbo_;
        }

        // Setup readbuffers & drawbuffers if needed
        if (MAP_VALUE(i).readBuffers_ != GL_NONE)
        {
            glReadBuffer(GL_NONE);
            MAP_VALUE(i).readBuffers_ = int(GL_NONE);
        }

        // Calculate the bit combination of non-zero color rendertargets to first check if the combination changed
        unsigned newDrawBuffers = 0;
        for (unsigned j = 0; j < MAX_RENDERTARGETS; ++j)
        {
            if (renderTargets_[j])
                newDrawBuffers |= 1 << j;
        }

        if (newDrawBuffers != MAP_VALUE(i).drawBuffers_)
        {
            // Check for no color rendertargets (depth rendering only)
            if (!newDrawBuffers)
                glDrawBuffer(GL_NONE);
            else
            {
                int drawBufferIds[MAX_RENDERTARGETS];
                unsigned drawBufferCount = 0;

                for (unsigned j = 0; j < MAX_RENDERTARGETS; ++j)
                {
                    if (renderTargets_[j])
                    {
                        drawBufferIds[drawBufferCount++] = int(GL_COLOR_ATTACHMENT0) + j;
                    }
                }
                glDrawBuffers(drawBufferCount, (const GLenum*)drawBufferIds);
            }

            MAP_VALUE(i).drawBuffers_ = newDrawBuffers;
        }

        for (unsigned j = 0; j < MAX_RENDERTARGETS; ++j)
        {
            if (renderTargets_[j])
            {
                Texture* texture = renderTargets_[j]->GetParentTexture();

                // Bind either a renderbuffer or texture, depending on what is available
                unsigned renderBufferID = renderTargets_[j]->GetRenderBuffer();
                if (!renderBufferID)
                {
                    // If texture's parameters are dirty, update before attaching
                    if (texture->GetParametersDirty())
                    {
                        SetTextureForUpdate(texture);
                        texture->UpdateParameters();
                        SetTexture(0, nullptr);
                    }

                    if (MAP_VALUE(i).colorAttachments_[j] != renderTargets_[j])
                    {
                        BindColorAttachment(j, renderTargets_[j]->GetTarget(), texture->GetGPUObject(),false);
                        MAP_VALUE(i).colorAttachments_[j] = renderTargets_[j];
                    }
                }
                else
                {
                    if (MAP_VALUE(i).colorAttachments_[j] != renderTargets_[j])
                    {
                        BindColorAttachment(j, renderTargets_[j]->GetTarget(), renderBufferID,true);
                        MAP_VALUE(i).colorAttachments_[j] = renderTargets_[j];
                    }
                }
            }
            else
            {
                if (MAP_VALUE(i).colorAttachments_[j])
                {
                    BindColorAttachment(j, GL_TEXTURE_2D, 0, false);
                    MAP_VALUE(i).colorAttachments_[j] = 0;
                }
            }
        }
        if (depthStencil_)
        {
            // Bind either a renderbuffer or a depth texture, depending on what is available
            Texture* texture = depthStencil_->GetParentTexture();
            bool hasStencil = texture->GetFormat() == GL_DEPTH24_STENCIL8_EXT;
            unsigned renderBufferID = depthStencil_->GetRenderBuffer();
            if (!renderBufferID)
            {
                // If texture's parameters are dirty, update before attaching
                if (texture->GetParametersDirty())
                {
                    SetTextureForUpdate(texture);
                    texture->UpdateParameters();
                    SetTexture(0, 0);
                }

                if (MAP_VALUE(i).depthAttachment_ != depthStencil_)
                {
                    BindDepthAttachment(texture->GetGPUObject(), false);
                    BindStencilAttachment(hasStencil ? texture->GetGPUObject() : 0, false);
                    MAP_VALUE(i).depthAttachment_ = depthStencil_;
                }
            }
            else
            {
                if (MAP_VALUE(i).depthAttachment_ != depthStencil_)
                {
                    BindDepthAttachment(renderBufferID, true);
                    BindStencilAttachment(hasStencil ? renderBufferID : 0, true);
                    MAP_VALUE(i).depthAttachment_ = depthStencil_;
                }
            }
        }
        else
        {
            if (MAP_VALUE(i).depthAttachment_)
            {
                BindDepthAttachment(0, false);
                BindStencilAttachment(0, false);
                MAP_VALUE(i).depthAttachment_ = 0;
            }
        }

        // Disable/enable sRGB write
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

    if (impl_->vertexBuffersDirty_)
    {
        // Go through currently bound vertex buffers and set the attribute pointers that are available & required
        // Use reverse order so that elements from higher index buffers will override lower index buffers
        unsigned assignedLocations = 0;

        for (unsigned i = MAX_VERTEX_STREAMS - 1; i < MAX_VERTEX_STREAMS; --i)
        {
            VertexBuffer* buffer = vertexBuffers_[i];
            // Beware buffers with missing OpenGL objects, as binding a zero buffer object means accessing CPU memory for vertex data,
            // in which case the pointer will be invalid and cause a crash
            if (!buffer || !buffer->GetGPUObject() || !impl_->vertexAttributes_)
                continue;

            const std::vector<VertexElement>& elements = buffer->GetElements();

            for (auto j = elements.cbegin(); j != elements.cend(); ++j)
            {
                const VertexElement& element = *j;
                auto k = impl_->vertexAttributes_->find(std::make_pair((unsigned char)element.semantic_, element.index_));

                if (k != impl_->vertexAttributes_->end())
                {
                    unsigned location = MAP_VALUE(k);
                    unsigned locationMask = 1 << location;
                    if (assignedLocations & locationMask)
                        continue; // Already assigned by higher index vertex buffer
                    assignedLocations |= locationMask;

                    // Enable attribute if not enabled yet
                    if (!(impl_->enabledVertexAttributes_ & locationMask))
                    {
                        glEnableVertexAttribArray(location);
                        impl_->enabledVertexAttributes_ |= locationMask;
                    }

                    // Enable/disable instancing divisor as necessary
                    unsigned dataStart = element.offset_;
                    if (element.perInstance_)
                    {
                        dataStart += impl_->lastInstanceOffset_ * buffer->GetVertexSize();
                        if (!(impl_->instancingVertexAttributes_ & locationMask))
                        {
                            SetVertexAttribDivisor(location, 1);
                            impl_->instancingVertexAttributes_ |= locationMask;
                        }
                    }
                    else
                    {
                        if (impl_->instancingVertexAttributes_ & locationMask)
                        {
                            SetVertexAttribDivisor(location, 0);
                            impl_->instancingVertexAttributes_ &= ~locationMask;
                        }
                    }

                    SetVBO(buffer->GetGPUObject());
                    glVertexAttribPointer(location, glElementComponents[element.type_], glElementTypes[element.type_],
                            element.type_ == TYPE_UBYTE4_NORM ? GL_TRUE : GL_FALSE, (unsigned)buffer->GetVertexSize(),
                            (const void *)(size_t)dataStart);
                }
            }
        }

        // Finally disable unnecessary vertex attributes
        unsigned disableVertexAttributes = impl_->enabledVertexAttributes_ & (~impl_->usedVertexAttributes_);
        unsigned location = 0;
        while (disableVertexAttributes)
        {
            if (disableVertexAttributes & 1)
            {
                glDisableVertexAttribArray(location);
                impl_->enabledVertexAttributes_ &= ~(1 << location);
            }
            ++location;
            disableVertexAttributes >>= 1;
        }

        impl_->vertexBuffersDirty_ = false;
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
        if (impl_->resolveSrcFBO_)
            DeleteFramebuffer(impl_->resolveSrcFBO_);
        if (impl_->resolveDestFBO_)
            DeleteFramebuffer(impl_->resolveDestFBO_);
    }
    else
    {
        impl_->boundFBO_ = 0;
        impl_->resolveSrcFBO_ = 0;
        impl_->resolveDestFBO_ = 0;
    }
    impl_->frameBuffers_.clear();
}

void Graphics::ResetCachedState()
{
    for (unsigned i = 0; i < MAX_VERTEX_STREAMS; ++i)
        vertexBuffers_[i] = nullptr;

    for (unsigned i = 0; i < MAX_TEXTURE_UNITS; ++i)
    {
        textures_[i] = nullptr;
        impl_->textureTypes_[i] = GL_NONE;
    }

    for (auto & elem : renderTargets_)
        elem = nullptr;

    depthStencil_ = nullptr;
    viewport_ = IntRect(0, 0, 0, 0);
    indexBuffer_ = nullptr;
    vertexShader_ = nullptr;
    pixelShader_ = nullptr;
    blendMode_ = BLEND_REPLACE;
    alphaToCoverage_ = false;
    colorWrite_ = true;
    cullMode_ = CULL_NONE;
    constantDepthBias_ = 0.0f;
    slopeScaledDepthBias_ = 0.0f;
    depthTestMode_ = CMP_ALWAYS;
    depthWrite_ = false;
    lineAntiAlias_ = false;
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
    impl_->shaderProgram_ = nullptr;
    impl_->lastInstanceOffset_ = 0;
    impl_->activeTexture_ = 0;
    impl_->enabledVertexAttributes_ = 0;
    impl_->usedVertexAttributes_ = 0;
    impl_->instancingVertexAttributes_ = 0;
    impl_->boundFBO_ = impl_->systemFBO_;
    impl_->boundVBO_ = 0;
    impl_->boundUBO_ = 0;
    impl_->sRGBWrite_ = false;
    if(window2_)
    {
        glEnable(GL_DEPTH_TEST);
        SetCullMode(CULL_CCW);
        SetDepthTest(CMP_LESSEQUAL);
        SetDepthWrite(true);
    }
    for (auto & elem : impl_->constantBuffers_)
        elem = nullptr;
    impl_->dirtyConstantBuffers_.clear();
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
    textureUnits_["VolumeMap"] = TU_VOLUMEMAP;
    textureUnits_["FaceSelectCubeMap"] = TU_FACESELECT;
    textureUnits_["IndirectionCubeMap"] = TU_INDIRECTION;
    textureUnits_["DepthBuffer"] = TU_DEPTHBUFFER;
    textureUnits_["LightBuffer"] = TU_LIGHTBUFFER;
    textureUnits_["ZoneCubeMap"] = TU_ZONE;
    textureUnits_["ZoneVolumeMap"] = TU_ZONE;
}

unsigned Graphics::CreateFramebuffer()
{
    unsigned newFbo = 0;
    glGenFramebuffers(1, &newFbo);
    return newFbo;
}

void Graphics::DeleteFramebuffer(unsigned fbo)
{
    glDeleteFramebuffers(1, &fbo);
}

void Graphics::BindFramebuffer(unsigned fbo)
{
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
}

void Graphics::BindColorAttachment(unsigned index, GLenum target, unsigned object,bool isRenderBuffer)
{
    if (!object)
        isRenderBuffer = false;
    if (!isRenderBuffer)
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + index, target, object, 0);
    else
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + index, GL_RENDERBUFFER, object);
}

void Graphics::BindDepthAttachment(unsigned object, bool isRenderBuffer)
{
    if (!object)
        isRenderBuffer = false;

    if (!isRenderBuffer)
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, object, 0);
    else
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, object);
}

void Graphics::BindStencilAttachment(unsigned object, bool isRenderBuffer)
{
    if (!object)
        isRenderBuffer = false;

    if (!isRenderBuffer)
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, object, 0);
    else
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, object);
}

bool Graphics::CheckFramebuffer()
{
    return glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
}
void Graphics::SetVertexAttribDivisor(unsigned location, unsigned divisor)
{
    if (instancingSupport_)
        glVertexAttribDivisor(location, divisor);
}

}
