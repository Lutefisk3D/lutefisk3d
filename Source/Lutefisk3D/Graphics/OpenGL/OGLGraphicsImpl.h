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

#include "../../Math/Color.h"
#include "../../Container/HashMap.h"
#include "../../Core/Timer.h"
#include "../GraphicsDefs.h"
#include "../ShaderVariation.h"
#include "../ConstantBuffer.h"
#include "../ShaderProgram.h"
#include "../Texture2D.h"
#include "glbinding/gl33ext/enum.h"
#include "glbinding/gl33ext/functions.h"

typedef void *SDL_GLContext;

namespace Urho3D
{
class Context;
class RenderSurface;


typedef HashMap<unsigned, SharedPtr<ConstantBuffer> > ConstantBufferMap;
typedef HashMap<std::pair<ShaderVariation*, ShaderVariation*>, SharedPtr<ShaderProgram> > ShaderProgramMap;

/// Cached state of a frame buffer object
struct FrameBufferObject
{
    FrameBufferObject() :
        fbo_(0),
        depthAttachment_(nullptr),
        readBuffers_(M_MAX_UNSIGNED),
        drawBuffers_(M_MAX_UNSIGNED)
    {
        for (auto & elem : colorAttachments_)
            elem = nullptr;
    }

    /// Frame buffer handle.
    unsigned fbo_;
    /// Bound color attachment textures.
    RenderSurface* colorAttachments_[MAX_RENDERTARGETS];
    /// Bound depth/stencil attachment.
    RenderSurface* depthAttachment_;
    /// Read buffer bits.
    unsigned readBuffers_;
    /// Draw buffer bits.
    unsigned drawBuffers_;
};

/// %Graphics subsystem implementation. Holds API-specific objects.
class LUTEFISK3D_EXPORT GraphicsImpl
{
    friend class LUTEFISK3D_EXPORT Graphics;

public:
    /// Construct.
    GraphicsImpl();

    /// Return the GL Context.
    const SDL_GLContext& GetGLContext() { return context_; }

private:
    /// SDL OpenGL context.
    SDL_GLContext context_;
    /// IOS system framebuffer handle.
    unsigned systemFBO_;
    /// Active texture unit.
    unsigned activeTexture_;
    /// Enabled vertex attributes bitmask.
    unsigned enabledVertexAttributes_;
    /// Vertex attributes bitmask used by the current shader program.
    unsigned usedVertexAttributes_;
    /// Vertex attribute instancing bitmask for keeping track of divisors.
    unsigned instancingVertexAttributes_;
    /// Current mapping of vertex attribute locations by semantic. The map is owned by the shader program, so care must be taken to switch a null shader program when it's destroyed.
    const HashMap<std::pair<unsigned char, unsigned char>, unsigned>* vertexAttributes_;
    /// Currently bound frame buffer object.
    unsigned boundFBO_;
    /// Currently bound vertex buffer object.
    unsigned boundVBO_;
    /// Currently bound uniform buffer object.
    unsigned boundUBO_;
    /// Read frame buffer for multisampled texture resolves.
    unsigned resolveSrcFBO_;
    /// Write frame buffer for multisampled texture resolves.
    unsigned resolveDestFBO_;
    /// Current pixel format.
    int pixelFormat_;
    /// Map for FBO's per resolution and format.
    HashMap<unsigned long long, FrameBufferObject> frameBuffers_;
    /// OpenGL texture types in use.
    gl::GLenum textureTypes_[MAX_TEXTURE_UNITS];
    /// Constant buffer search map.
    ConstantBufferMap allConstantBuffers_;
    /// Currently bound constant buffers.
    ConstantBuffer* constantBuffers_[MAX_SHADER_PARAMETER_GROUPS * 2];
    /// Dirty constant buffers.
    std::vector<ConstantBuffer*> dirtyConstantBuffers_;
    /// Last used instance data offset.
    unsigned lastInstanceOffset_;
    /// Map for additional depth textures, to emulate Direct3D9 ability to mix render texture and backbuffer rendering.
    HashMap<int, SharedPtr<Texture2D> > depthTextures_;
    /// Shader program in use.
    ShaderProgram* shaderProgram_;
    /// Linked shader programs.
    ShaderProgramMap shaderPrograms_;
    /// Need FBO commit flag.
    bool fboDirty_;
    /// Need vertex attribute pointer update flag.
    bool vertexBuffersDirty_;
    /// sRGB write mode flag.
    bool sRGBWrite_;
};

}
