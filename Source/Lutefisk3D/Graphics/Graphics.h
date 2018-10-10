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

#include "Lutefisk3D/Container/DataHandle.h"
#include "Lutefisk3D/Math/Color.h"
#include "Lutefisk3D/Graphics/GraphicsDefs.h"
#include "Lutefisk3D/Core/Mutex.h"
#include "Lutefisk3D/Core/Object.h"
#include "Lutefisk3D/Math/Plane.h"
#include "Lutefisk3D/Math/Rect.h"
#include "Lutefisk3D/Container/Str.h"
#include <utility>
#include <functional>
#include <array>
namespace std {
extern template
class unique_ptr<uint8_t[]>;
}
typedef struct GLFWwindow GLFWwindow;
namespace Urho3D
{
enum CompressedFormat : unsigned;
class Deserializer;
class ConstantBuffer;
class File;
class Image;
class IndexBuffer;
class GPUObject;
class GraphicsImpl;
class RenderSurface;
class Shader;
class ShaderPrecache;
class ShaderProgram;
class ShaderVariation;
class Texture;
class Texture2D;
class Texture2DArray;
class TextureCube;
class Vector3;
class Vector4;
class VertexBuffer;
struct ShaderParameter;

using VertexBufferHandle = DataHandle<VertexBuffer,20,20>;
/// CPU-side scratch buffer for vertex data updates.
struct ScratchBuffer
{
    /// Buffer data.
    std::unique_ptr<uint8_t[]> data_;
    /// Data size.
    unsigned size_=0;
    /// Reserved flag.
    bool reserved_=false;
};

/// %Graphics subsystem. Manages the application window, rendering state and GPU resources.
class LUTEFISK3D_EXPORT Graphics : public RefCounted
{
public:
    Graphics(Context* context_);
    /// Destruct. Release the device context and close the window.
    virtual ~Graphics();

    /// Inform graphics that our SDL_Window is wrapped in a toolkit's own window.
    void SetEmbeddedWindow(void *true_window) { assert(!window2_); ourWindowIsEmbedded_=true; }
    bool WeAreEmbedded() const { return ourWindowIsEmbedded_; }
    /// Set window title.
    void SetWindowTitle(const QString& windowTitle);
    /// Set window icon.
    void SetWindowIcon(Image* windowIcon);
    /// Set window position. Sets initial position if window is not created yet.
    void SetWindowPosition(const IntVector2& position);
    /// Set window position. Sets initial position if window is not created yet.
    void SetWindowPosition(int x, int y);
    /// Set screen mode. Return true if successful.
    bool SetMode(int width, int height, bool fullscreen, bool borderless, bool resizable, bool highDPI, bool vsync, bool tripleBuffer,            int multiSample, int monitor, int refreshRate);
    /// Set screen resolution only. Return true if successful.
    bool SetMode(int width, int height);
    /// Set whether the main window uses sRGB conversion on write.
    void SetSRGB(bool enable);
    /// Set whether rendering output is dithered. Default true on OpenGL. No effect on Direct3D.
    void SetDither(bool enable);
    /// Set whether to flush the GPU command buffer to prevent multiple frames being queued and uneven frame timesteps. Default off, may decrease performance if enabled. Not currently implemented on OpenGL.
    void SetFlushGPU(bool enable);
    /// Toggle between full screen and windowed mode. Return true if successful.
    bool ToggleFullscreen();
    /// Close the window.
    void Close();
    /// Take a screenshot. Return true if successful.
    bool TakeScreenShot(Image& destImage);
    /// Begin frame rendering. Return true if device available and can render.
    bool BeginFrame();
    /// End frame rendering and swap buffers.
    void EndFrame();
    /// Clear any or all of rendertarget, depth buffer and stencil buffer.
    void Clear(unsigned flags, const Color& color = Color(0.0f, 0.0f, 0.0f, 0.0f), float depth = 1.0f, unsigned stencil = 0);
    /// Resolve multisampled backbuffer to a texture rendertarget. The texture's size should match the viewport size.
    bool ResolveToTexture(Texture2D* destination, const IntRect& viewport);
    /// Resolve a multisampled texture on itself.
    bool ResolveToTexture(Texture2D* texture);
    /// Resolve a multisampled cube texture on itself.
    bool ResolveToTexture(TextureCube* texture);
    /// Draw non-indexed geometry.
    void Draw(PrimitiveType type, unsigned vertexStart, unsigned vertexCount);
    /// Draw indexed geometry.
    void Draw(PrimitiveType type, unsigned indexStart, unsigned indexCount, unsigned minVertex, unsigned vertexCount);
    /// Draw indexed geometry with vertex index offset.
    void Draw(PrimitiveType type, unsigned indexStart, unsigned indexCount, unsigned baseVertexIndex, unsigned minVertex, unsigned vertexCount);
    /// Draw indexed, instanced geometry. An instancing vertex buffer must be set.
    void DrawInstanced(PrimitiveType type, unsigned indexStart, unsigned indexCount, unsigned minVertex, unsigned vertexCount, unsigned instanceCount);
    /// Draw indexed, instanced geometry with vertex index offset.
    void DrawInstanced(PrimitiveType type, unsigned indexStart, unsigned indexCount, unsigned baseVertexIndex, unsigned minVertex,
        unsigned vertexCount, unsigned instanceCount);
    /// Set vertex buffer.
    void SetVertexBuffer(VertexBuffer* buffer);
    /// Set multiple vertex buffers.
    bool SetVertexBuffers(const std::vector<Urho3D::VertexBuffer *> &buffers, unsigned instanceOffset = 0);
    /// Set index buffer.
    void SetIndexBuffer(IndexBuffer* buffer);
    /// Set shaders.
    void SetShaders(ShaderVariation* vs, ShaderVariation* ps);
    /// Set shader float constants.
    void SetShaderParameter(StringHash param, const float* data, unsigned count);
    /// Set shader float constant.
    void SetShaderParameter(StringHash param, float value);
    /// Set shader integer constant.
    void SetShaderParameter(StringHash param, int value);
    /// Set shader boolean constant.
    void SetShaderParameter(StringHash param, bool value);
    /// Set shader color constant.
    void SetShaderParameter(StringHash param, const Color& color);
    /// Set shader 2D vector constant.
    void SetShaderParameter(StringHash param, const Vector2& vector);
    /// Set shader 3x3 matrix constant.
    void SetShaderParameter(StringHash param, const Matrix3& matrix);
    /// Set shader 3D vector constant.
    void SetShaderParameter(StringHash param, const Vector3& vector);
    /// Set shader 4x4 matrix constant.
    void SetShaderParameter(StringHash param, const Matrix4& matrix);
    /// Set shader 4D vector constant.
    void SetShaderParameter(StringHash param, const Vector4& vector);
    /// Set shader 3x4 matrix constant.
    void SetShaderParameter(StringHash param, const Matrix3x4& matrix);
    /// Set shader constant from a variant. Supported variant types: bool, float, vector2, vector3, vector4, color.
    void SetShaderParameter(StringHash param, const Variant& value);
    /// Check whether a shader parameter group needs update. Does not actually check whether parameters exist in the shaders.
    bool NeedParameterUpdate(ShaderParameterGroup group, const void* source);
    /// Check whether a shader parameter exists on the currently set shaders.
    bool HasShaderParameter(StringHash param);
    /// Check whether the current vertex or pixel shader uses a texture unit.
    bool HasTextureUnit(TextureUnit unit);
    /// Clear remembered shader parameter source group.
    void ClearParameterSource(ShaderParameterGroup group);
    /// Clear remembered shader parameter sources.
    void ClearParameterSources();
    /// Clear remembered transform shader parameter sources.
    void ClearTransformSources();
    /// Set texture.
    void SetTexture(unsigned index, Texture* texture);
    /// Bind texture unit 0 for update. Called by Texture. Used only on OpenGL.
    void SetTextureForUpdate(Texture* texture);
    /// Dirty texture parameters of all textures (when global settings change.)
    void SetTextureParametersDirty();
    /// Set default texture filtering mode. Called by Renderer before rendering.
    void SetDefaultTextureFilterMode(TextureFilterMode mode);
    /// Set default texture anisotropy level. Called by Renderer before rendering.
    void SetDefaultTextureAnisotropy(unsigned level);
    /// Reset all rendertargets, depth-stencil surface and viewport.
    void ResetRenderTargets();
    /// Reset specific rendertarget.
    void ResetRenderTarget(unsigned index);
    /// Reset depth-stencil surface.
    void ResetDepthStencil();
    /// Set rendertarget.
    void SetRenderTarget(unsigned index, RenderSurface* renderTarget);
    /// Set rendertarget.
    void SetRenderTarget(unsigned index, Texture2D* texture);
    /// Set depth-stencil surface.
    void SetDepthStencil(RenderSurface* depthStencil);
    /// Set depth-stencil surface.
    void SetDepthStencil(Texture2D* texture);
    /// Set viewport.
    void SetViewport(const IntRect& rect);
    void SetBlendMode(BlendMode mode, bool alphaToCoverage = false);
    void SetColorWrite(bool enable);
    void SetCullMode(CullMode mode);
    void SetDepthBias(float constantBias, float slopeScaledBias);
    void SetDepthTest(CompareMode mode);
    void SetDepthWrite(bool enable);
    /// Set polygon fill mode.
    void SetFillMode(FillMode mode);
    /// Set line antialiasing on/off.
    void SetLineAntiAlias(bool enable);
    /// Set scissor test.
    void SetScissorTest(bool enable, const Rect& rect = Rect::FULL, bool borderInclusive = true);
    /// Set scissor test.
    void SetScissorTest(bool enable, const IntRect& rect);
    /// Set stencil test.
    void SetStencilTest(bool enable, CompareMode mode = CMP_ALWAYS, StencilOp pass = OP_KEEP, StencilOp fail = OP_KEEP, StencilOp zFail = OP_KEEP, unsigned stencilRef = 0, unsigned compareMask = M_MAX_UNSIGNED, unsigned writeMask = M_MAX_UNSIGNED);
    /// Set a custom clipping plane. The plane is specified in world space, but is dependent on the view and projection matrices.
    void SetClipPlane(bool enable, const Plane& clipPlane = Plane::UP, const Matrix3x4& view = Matrix3x4::IDENTITY, const Matrix4& projection = Matrix4::IDENTITY);
    /// Begin dumping shader variation names to an XML file for precaching.
    void BeginDumpShaders(const QString& fileName);
    /// End dumping shader variations names.
    void EndDumpShaders();
    /// Precache shader variations from an XML file generated with BeginDumpShaders().
    void PrecacheShaders(Deserializer& source);
    /// Set shader cache directory, Direct3D only. This can either be an absolute path or a path within the resource system.
    void SetShaderCacheDir(const QString& path);

    /// Return whether rendering initialized.
    bool IsInitialized() const;
    /// Return graphics implementation, which holds the actual API-specific resources.
    GraphicsImpl* GetImpl() const { return impl_; }
    /// Return GLFW window.
    GLFWwindow * GetWindow() const { return window2_; }
    /// Return window title.
    const QString& GetWindowTitle() const { return windowTitle_; }
    /// Return graphics API name.
    const QString& GetApiName() const { return apiName_; }
    /// Return window position.
    IntVector2 GetWindowPosition() const;
    /// Return window width in pixels.
    int GetWidth() const { return width_; }
    /// Return window height in pixels.
    int GetHeight() const { return height_; }
    /// Return multisample mode (1 = no multisampling.)
    int GetMultiSample() const { return multiSample_; }
    /// Return window size in pixels.
    IntVector2 GetSize() const { return IntVector2(width_, height_); }
    /// Return whether window is fullscreen.
    bool GetFullscreen() const { return fullscreen_; }
    /// Return whether window is borderless.
    bool GetBorderless() const { return borderless_; }
    /// Return whether window is resizable.
    bool GetResizable() const { return resizable_; }
    /// Return whether window is high DPI.
    bool GetHighDPI() const { return highDPI_; }
    /// Return whether vertical sync is on.
    bool GetVSync() const { return vsync_; }
    /// Return refresh rate when using vsync in fullscreen
    int GetRefreshRate() const { return refreshRate_; }
    /// Return the current monitor index. Effective on in fullscreen
    int GetMonitor() const { return monitor_; }
    /// Return whether triple buffering is enabled.
    bool GetTripleBuffer() const { return tripleBuffer_; }
    /// Return whether the main window is using sRGB conversion on write.
    bool GetSRGB() const { return sRGB_; }
    /// Return whether rendering output is dithered.
    bool GetDither() const;

    /// Return whether the GPU command buffer is flushed each frame.
    bool GetFlushGPU() const { return flushGPU_; }

    /// Return whether graphics context is lost and can not render or load GPU resources.
    bool IsDeviceLost() const;
    /// Return number of primitives drawn this frame.
    unsigned GetNumPrimitives() const { return numPrimitives_; }
    /// Return number of batches drawn this frame.
    unsigned GetNumBatches() const { return numBatches_; }
    /// Return dummy color texture format for shadow maps. Is "GL_NONE" (consume no video memory) if supported.
    uint32_t GetDummyColorFormat() const { return dummyColorFormat_; }
    /// Return shadow map depth texture format, or 0 if not supported.
    uint32_t GetShadowMapFormat() const { return shadowMapFormat_; }
    /// Return 24-bit shadow map depth texture format, or 0 if not supported.
    uint32_t GetHiresShadowMapFormat() const { return hiresShadowMapFormat_; }
    /// Return whether hardware instancing is supported.
    bool GetInstancingSupport() const { return instancingSupport_; }
    /// Return whether light pre-pass rendering is supported.
    bool GetLightPrepassSupport() const { return lightPrepassSupport_; }
    /// Return whether deferred rendering is supported.
    bool GetDeferredSupport() const { return deferredSupport_; }
    /// Return whether shadow map depth compare is done in hardware.
    bool GetHardwareShadowSupport() const { return hardwareShadowSupport_; }
    /// Return whether a readable hardware depth format is available.
    bool GetReadableDepthSupport() const { return (unsigned)GetReadableDepthFormat() != 0; }
    /// Return supported fullscreen resolutions (third component is refreshRate). Will be empty if listing the resolutions is not supported on the platform (e.g. Web).
    std::vector<IntVector3> GetResolutions(int monitor) const;
    /// Return supported multisampling levels.
    std::vector<int> GetMultiSampleLevels() const;
    /// Return the desktop resolution.
    IntVector2 GetDesktopResolution(int monitor) const;
    /// Return the number of currently connected monitors.
    int GetMonitorCount() const;
    /// Returns the index of the display containing the center of the window on success or a negative error code on failure.
    int GetCurrentMonitor() const;
    /// Returns true if window is maximized or runs in full screen mode.
    bool GetMaximized() const;
    /// Return display dpi information: (hdpi, vdpi, ddpi). On failure returns zero vector.
    Vector3 GetDisplayDPI(int monitor=0) const;
    /// Return hardware format for a compressed image format, or 0 if unsupported.
    uint32_t GetFormat(CompressedFormat format) const;
    ShaderVariation* GetShader(ShaderType type, const QString& name, const QString& defines = QString()) const;
    /// Return a shader variation by name and defines.
    ShaderVariation* GetShader(ShaderType type, const char* name, const char* defines) const;
    /// Return current vertex buffer by index.
    VertexBuffer* GetVertexBuffer(unsigned index) const;
    /// Return current index buffer.
    IndexBuffer* GetIndexBuffer() const { return indexBuffer_; }
    /// Return current vertex shader.
    ShaderVariation* GetVertexShader() const { return vertexShader_; }
    /// Return current pixel shader.
    ShaderVariation* GetPixelShader() const { return pixelShader_; }
    /// Return shader program. This is an API-specific class and should not be used by applications.
    ShaderProgram* GetShaderProgram() const;
    /// Return texture unit index by name.
    TextureUnit GetTextureUnit(const QString& name);
    /// Return texture unit name by index.
    const QString& GetTextureUnitName(TextureUnit unit);
    /// Return current texture by texture unit index.
    Texture* GetTexture(unsigned index) const;
    /// Return default texture filtering mode.
    TextureFilterMode GetDefaultTextureFilterMode() const { return defaultTextureFilterMode_; }
    /// Return default texture max. anisotropy level.
    unsigned GetDefaultTextureAnisotropy() const { return defaultTextureAnisotropy_; }

    /// Return current rendertarget by index.
    RenderSurface* GetRenderTarget(unsigned index) const;
    /// Return current depth-stencil surface.
    RenderSurface* GetDepthStencil() const { return depthStencil_; }
    /// Return the viewport coordinates.
    IntRect GetViewport() const { return viewport_; }
    /// Return blending mode.
    BlendMode GetBlendMode() const { return blendMode_; }
    /// Return whether alpha-to-coverage is enabled.
    bool GetAlphaToCoverage() const { return alphaToCoverage_; }
    /// Return whether color write is enabled.
    bool GetColorWrite() const { return colorWrite_; }
    /// Return hardware culling mode.
    CullMode GetCullMode() const { return cullMode_; }
    /// Return depth constant bias.
    float GetDepthConstantBias() const { return constantDepthBias_; }
    /// Return depth slope scaled bias.
    float GetDepthSlopeScaledBias() const { return slopeScaledDepthBias_; }
    /// Return depth compare mode.
    CompareMode GetDepthTest() const { return depthTestMode_; }
    /// Return whether depth write is enabled.
    bool GetDepthWrite() const { return depthWrite_; }
    /// Return polygon fill mode.
    FillMode GetFillMode() const { return fillMode_; }
    /// Return whether line antialiasing is enabled.
    bool GetLineAntiAlias() const { return lineAntiAlias_; }
    /// Return whether stencil test is enabled.
    bool GetStencilTest() const { return stencilTest_; }
    /// Return whether scissor test is enabled.
    bool GetScissorTest() const { return scissorTest_; }
    /// Return scissor rectangle coordinates.
    const IntRect& GetScissorRect() const { return scissorRect_; }
    /// Return stencil compare mode.
    CompareMode GetStencilTestMode() const { return stencilTestMode_; }
    /// Return stencil operation to do if stencil test passes.
    StencilOp GetStencilPass() const { return stencilPass_; }
    /// Return stencil operation to do if stencil test fails.
    StencilOp GetStencilFail() const { return stencilFail_; }
    /// Return stencil operation to do if depth compare fails.
    StencilOp GetStencilZFail() const { return stencilZFail_; }
    /// Return stencil reference value.
    unsigned GetStencilRef() const { return stencilRef_; }
    /// Return stencil compare bitmask.
    unsigned GetStencilCompareMask() const { return stencilCompareMask_; }
    /// Return stencil write bitmask.
    unsigned GetStencilWriteMask() const { return stencilWriteMask_; }
    /// Return whether a custom clipping plane is in use.
    bool GetUseClipPlane() const { return useClipPlane_; }
    /// Return shader cache directory, Direct3D only.
    const QString& GetShaderCacheDir() const { return shaderCacheDir_; }

    /// Return current rendertarget width and height.
    IntVector2 GetRenderTargetDimensions() const;

    /// Window was resized through user interaction. Called by Input subsystem.
    void OnWindowResized();
    /// Window was moved through user interaction. Called by Input subsystem.
    void OnWindowMoved();
    /// Restore GPU objects and reinitialize state. Requires an open window. Used only on OpenGL.
    void Restore();
    /// Maximize the window.
    void Maximize();
    /// Minimize the window.
    void Minimize();
    /// Add a GPU object to keep track of. Called by GPUObject.
    void AddGPUObject(GPUObject* object);
    /// Remove a GPU object. Called by GPUObject.
    void RemoveGPUObject(GPUObject* object);
    /// Reserve a CPU-side scratch buffer.
    void* ReserveScratchBuffer(unsigned size);
    /// Free a CPU-side scratch buffer.
    void FreeScratchBuffer(void* buffer);
    /// Clean up too large scratch buffers.
    void CleanupScratchBuffers();
    /// Clean up shader parameters when a shader variation is released or destroyed.
    void CleanupShaderPrograms(ShaderVariation* variation);
    /// Clean up a render surface from all FBOs. Used only on OpenGL.
    void CleanupRenderSurface(RenderSurface* surface);
    /// Get or create a constant buffer. Will be shared between shaders if possible.
    ConstantBuffer* GetOrCreateConstantBuffer(ShaderType type, unsigned index, unsigned size);
    /// Mark the FBO needing an update. Used only on OpenGL.
    void MarkFBODirty();
    /// Bind a VBO, avoiding redundant operation. Used only on OpenGL.
    void SetVBO(unsigned object);
    /// Bind a UBO, avoiding redundant operation. Used only on OpenGL.
    void SetUBO(unsigned object);
    Context *GetContext() const { return m_context; }

    /// Return the API-specific alpha texture format.
    static uint32_t GetAlphaFormat();
    /// Return the API-specific luminance texture format.
    static uint32_t GetLuminanceFormat();
    /// Return the API-specific luminance alpha texture format.
    static uint32_t GetLuminanceAlphaFormat();
    /// Return the API-specific RGB texture format.
    static uint32_t GetRGBFormat();
    /// Return the API-specific RGBA texture format.
    static uint32_t GetRGBAFormat();
    /// Return the API-specific RGBA 16-bit texture format.
    static uint32_t GetRGBA16Format();
    /// Return the API-specific RGBA 16-bit float texture format.
    static uint32_t GetRGBAFloat16Format();
    /// Return the API-specific RGBA 32-bit float texture format.
    static uint32_t GetRGBAFloat32Format();
    /// Return the API-specific RG 16-bit texture format.
    static uint32_t GetRG16Format();
    /// Return the API-specific RG 16-bit float texture format.
    static uint32_t GetRGFloat16Format();
    /// Return the API-specific RG 32-bit float texture format.
    static uint32_t GetRGFloat32Format();
    /// Return the API-specific single channel 16-bit float texture format.
    static uint32_t GetFloat16Format();
    /// Return the API-specific single channel 32-bit float texture format.
    static uint32_t GetFloat32Format();
    /// Return the API-specific linear depth texture format.
    static uint32_t GetLinearDepthFormat();
    /// Return the API-specific hardware depth-stencil texture format.
    static uint32_t GetDepthStencilFormat();
    /// Return the API-specific readable hardware depth format, or 0 if not supported.
    static uint32_t GetReadableDepthFormat();
    /// Return the API-specific texture format from a textual description, for example "rgb".
    static uint32_t GetFormat(const QString& formatName);
    /// Return UV offset required for pixel perfect rendering.
    static const Vector2& GetPixelUVOffset() { return pixelUVOffset; }
    /// Return maximum number of supported bones for skinning.
    static unsigned GetMaxBones();
    /// Returns the index of the display containing the center of the window on success or a negative error code on failure.
    int GetCurrentMonitor();
    /// Returns number of monitors currently connected.
    int GetNumMonitors();
    /// Returns true if window is maximized or runs in full screen mode.
    bool GetMaximized();
    /// Returns resolution of monitor. monitorId should be less or equal to result of GetNumMonitors().
    IntVector2 GetMonitorResolution(int monitorId) const;
    /// Raises window if it was minimized.
    void RaiseWindow();
private:
    /// Create the application window.
    bool OpenWindow(int width, int height, bool resizable, bool borderless);
    /// Create the application window icon.
    void CreateWindowIcon();
    /// Adjust the window for new resolution and fullscreen mode.
    void AdjustWindow(int& newWidth, int& newHeight, bool& newFullscreen, bool& newBorderless, int& monitor);
    /// Create the Direct3D11 device and swap chain. Requires an open window. Can also be called again to recreate swap chain. Return true on success.
    bool CreateDevice(int width, int height, int multiSample);
    /// Update Direct3D11 swap chain state for a new mode and create views for the backbuffer & default depth buffer. Return true on success.
    bool UpdateSwapChain(int width, int height);
    /// Create the Direct3D9 interface.
    bool CreateInterface();
    /// Create the Direct3D9 device.
    bool CreateDevice(unsigned adapter, unsigned deviceType);
    /// Reset the Direct3D9 device.
    void ResetDevice();
    /// Notify all GPU resources so they can release themselves as needed. Used only on Direct3D9.
    void OnDeviceLost();
    /// Notify all GPU resources so they can recreate themselves as needed. Used only on Direct3D9.
    void OnDeviceReset();
    /// Set vertex buffer stream frequency. Used only on Direct3D9.
    void SetStreamFrequency(unsigned index, unsigned frequency);
    /// Reset stream frequencies. Used only on Direct3D9.
    void ResetStreamFrequencies();
    /// Check supported rendering features.
    void CheckFeatureSupport();
    /// Reset cached rendering state.
    void ResetCachedState();
    /// Initialize texture unit mappings.
    void SetTextureUnitMappings();
    /// Process dirtied state before draw.
    void PrepareDraw();
    /// Create intermediate texture for multisampled backbuffer resolve. No-op if already exists.
    void CreateResolveTexture();
    /// Clean up all framebuffers. Called when destroying the context. Used only on OpenGL.
    void CleanupFramebuffers();
    /// Create a framebuffer using either extension or core functionality. Used only on OpenGL.
    unsigned CreateFramebuffer();
    /// Delete a framebuffer using either extension or core functionality. Used only on OpenGL.
    void DeleteFramebuffer(unsigned fbo);
    /// Bind a framebuffer using either extension or core functionality. Used only on OpenGL.
    void BindFramebuffer(unsigned fbo);
    /// Bind a framebuffer color attachment using either extension or core functionality. Used only on OpenGL.
    void BindColorAttachment(unsigned index, uint32_t target, unsigned object, bool isRenderBuffer);
    /// Bind a framebuffer depth attachment using either extension or core functionality. Used only on OpenGL.
    void BindDepthAttachment(unsigned object, bool isRenderBuffer);
    /// Bind a framebuffer stencil attachment using either extension or core functionality. Used only on OpenGL.
    void BindStencilAttachment(unsigned object, bool isRenderBuffer);
    /// Check FBO completeness using either extension or core functionality. Used only on OpenGL.
    bool CheckFramebuffer();
    /// Set vertex attrib divisor. No-op if unsupported. Used only on OpenGL.
    void SetVertexAttribDivisor(unsigned location, unsigned divisor);

    /// Release/clear GPU objects and optionally close the window. Used only on OpenGL.
    void Release(bool clearGPUObjects, bool closeWindow);

    Context *m_context;
    /// Mutex for accessing the GPU objects vector from several threads.
    Mutex gpuObjectMutex_;
    /// Implementation.
    GraphicsImpl* impl_;
    /// GLFW window.
    GLFWwindow* window2_ = nullptr;
    /// Window title.
    QString windowTitle_;
    /// Window icon image.
    WeakPtr<Image> windowIcon_;
    /// Window width in pixels.
    int width_=0;
    /// Window height in pixels.
    int height_=0;
    /// Window position.
    IntVector2 position_;
    /// Multisampling mode.
    int multiSample_;
    /// Fullscreen flag.
    bool fullscreen_;
    /// Borderless flag.
    bool borderless_;
    /// Resizable flag.
    bool resizable_;
    /// High DPI flag.
    bool highDPI_;
    /// Vertical sync flag.
    bool vsync_;
    /// Refresh rate in Hz. Only used in fullscreen, 0 when windowed
    int refreshRate_;
    /// Monitor index. Only used in fullscreen, 0 when windowed
    int monitor_;
    /// Triple buffering flag.
    bool tripleBuffer_;
    /// Flush GPU command buffer flag.
    bool flushGPU_;
    /// sRGB conversion on write flag for the main window.
    bool sRGB_;
    /// If the window we are managing is embedded inside some UI toolkit
    bool ourWindowIsEmbedded_;
    /// Light pre-pass rendering support flag.
    bool lightPrepassSupport_;
    /// Deferred rendering support flag.
    bool deferredSupport_;
    /// Hardware shadow map depth compare support flag.
    bool hardwareShadowSupport_;
    /// Instancing support flag.
    bool instancingSupport_;
    /// sRGB conversion on read support flag.
    bool sRGBSupport_;
    /// sRGB conversion on write support flag.
    bool sRGBWriteSupport_;
    /// Number of primitives this frame.
    unsigned numPrimitives_;
    /// Number of batches this frame.
    unsigned numBatches_;
    /// Largest scratch buffer request this frame.
    unsigned maxScratchBufferRequest_;
    /// GPU objects.
    std::vector<GPUObject*> gpuObjects_;
    /// Scratch buffers.
    std::vector<ScratchBuffer> scratchBuffers_;
    /// Shadow map dummy color texture format.
    uint32_t dummyColorFormat_;
    /// Shadow map depth texture format.
    uint32_t shadowMapFormat_;
    /// Shadow map 24-bit depth texture format.
    uint32_t hiresShadowMapFormat_;
    /// Vertex buffers in use.
    std::array<VertexBuffer*,MAX_VERTEX_STREAMS> vertexBuffers_;
    /// Index buffer in use.
    IndexBuffer* indexBuffer_;
    /// Current vertex declaration hash.
    uint64_t vertexDeclarationHash_;
    /// Current primitive type.
    unsigned primitiveType_;
    /// Vertex shader in use.
    ShaderVariation* vertexShader_;
    /// Pixel shader in use.
    ShaderVariation* pixelShader_;
    /// Textures in use.
    Texture* textures_[MAX_TEXTURE_UNITS];
    /// Texture unit mappings.
    HashMap<QString, TextureUnit> textureUnits_;
    /// Rendertargets in use.
    RenderSurface* renderTargets_[MAX_RENDERTARGETS];
    /// Depth-stencil surface in use.
    RenderSurface* depthStencil_;
    /// Viewport coordinates.
    IntRect viewport_;
    /// Default texture filtering mode.
    TextureFilterMode defaultTextureFilterMode_;
    /// Default texture max. anisotropy level.
    unsigned defaultTextureAnisotropy_;
    /// Blending mode.
    BlendMode blendMode_;
    /// Alpha-to-coverage enable.
    bool alphaToCoverage_;
    /// Color write enable.
    bool colorWrite_;
    /// Hardware culling mode.
    CullMode cullMode_;
    /// Depth constant bias.
    float constantDepthBias_;
    /// Depth slope scaled bias.
    float slopeScaledDepthBias_;
    /// Depth compare mode.
    CompareMode depthTestMode_;
    /// Depth write enable flag.
    bool depthWrite_;
    /// Line antialiasing enable flag.
    bool lineAntiAlias_;
    /// Polygon fill mode.
    FillMode fillMode_;
    /// Scissor test enable flag.
    bool scissorTest_;
    /// Scissor test rectangle.
    IntRect scissorRect_;
    /// Stencil test compare mode.
    CompareMode stencilTestMode_;
    /// Stencil operation on pass.
    StencilOp stencilPass_;
    /// Stencil operation on fail.
    StencilOp stencilFail_;
    /// Stencil operation on depth fail.
    StencilOp stencilZFail_;
    /// Stencil test reference value.
    unsigned stencilRef_;
    /// Stencil compare bitmask.
    unsigned stencilCompareMask_;
    /// Stencil write bitmask.
    unsigned stencilWriteMask_;
    /// Current custom clip plane in post-projection space.
    Vector4 clipPlane_;
    /// Stencil test enable flag.
    bool stencilTest_;
    /// Custom clip plane enable flag.
    bool useClipPlane_;
    /// Remembered shader parameter sources.
    const void* shaderParameterSources_[MAX_SHADER_PARAMETER_GROUPS];
    /// Base directory for shaders.
    QString shaderPath_;
    /// Cache directory for Direct3D binary shaders.
    QString shaderCacheDir_;
    /// File extension for shaders.
    QString shaderExtension_;
    /// Last used shader in shader variation query.
    mutable WeakPtr<Shader> lastShader_;
    /// Last used shader name in shader variation query.
    mutable QString lastShaderName_;
    /// Shader precache utility.
    SharedPtr<ShaderPrecache> shaderPrecache_;
    /// Graphics API name.
    QString apiName_;

    /// Pixel perfect UV offset.
    static const Vector2 pixelUVOffset;
};

/// Register Graphics library objects.
void LUTEFISK3D_EXPORT RegisterGraphicsLibrary(Context* context);

}
