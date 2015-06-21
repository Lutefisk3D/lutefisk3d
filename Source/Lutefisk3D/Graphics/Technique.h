//
// Copyright (c) 2008-2015 the Urho3D project.
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

#include "../Graphics/GraphicsDefs.h"
#include "../Resource/Resource.h"

namespace Urho3D
{

class ShaderVariation;

/// Lighting mode of a pass.
enum PassLightingMode
{
    LIGHTING_UNLIT,
    LIGHTING_PERVERTEX,
    LIGHTING_PERPIXEL
};

/// %Material rendering pass, which defines shaders and render state.
class Pass : public RefCounted
{
public:
    /// Construct.
    Pass(const QString& passName);
    /// Destruct.
    ~Pass();

    /// Set blend mode.
    void SetBlendMode(BlendMode mode);
    /// Set depth compare mode.
    void SetDepthTestMode(CompareMode mode);
    /// Set pass lighting mode, affects what shader variations will be attempted to be loaded.
    void SetLightingMode(PassLightingMode mode);
    /// Set depth write on/off.
    void SetDepthWrite(bool enable);
    /// Set alpha masking hint. Completely opaque draw calls will be performed before alpha masked.
    void SetAlphaMask(bool enable);
    /// Set whether requires desktop level hardware.
    void SetIsDesktop(bool enable);
    /// Set vertex shader name.
    void SetVertexShader(const QString& name);
    /// Set pixel shader name.
    void SetPixelShader(const QString& name);
    /// Set vertex shader defines.
    void SetVertexShaderDefines(const QString& defines);
    /// Set pixel shader defines.
    void SetPixelShaderDefines(const QString& defines);
    /// Reset shader pointers.
    void ReleaseShaders();
    /// Mark shaders loaded this frame.
    void MarkShadersLoaded(unsigned frameNumber);

    /// Return pass name.
    const QString& GetName() const { return name_; }
    /// Return pass index. This is used for optimal render-time pass queries that avoid map lookups.
    unsigned GetIndex() const { return index_; }
    /// Return blend mode.
    BlendMode GetBlendMode() const { return blendMode_; }
    /// Return depth compare mode.
    CompareMode GetDepthTestMode() const { return depthTestMode_; }
    /// Return pass lighting mode.
    PassLightingMode GetLightingMode() const { return lightingMode_; }
    /// Return last shaders loaded frame number.
    unsigned GetShadersLoadedFrameNumber() const { return shadersLoadedFrameNumber_; }
    /// Return depth write mode.
    bool GetDepthWrite() const { return depthWrite_; }
    /// Return alpha masking hint.
    bool GetAlphaMask() const { return alphaMask_; }
    /// Return whether requires desktop level hardware.
    bool IsDesktop() const { return isDesktop_; }
    /// Return vertex shader name.
    const QString& GetVertexShader() const { return vertexShaderName_; }
    /// Return pixel shader name.
    const QString& GetPixelShader() const { return pixelShaderName_; }
    /// Return vertex shader defines.
    const QString& GetVertexShaderDefines() const { return vertexShaderDefines_; }
    /// Return pixel shader defines.
    const QString& GetPixelShaderDefines() const { return pixelShaderDefines_; }
    /// Return vertex shaders.
    std::vector<SharedPtr<ShaderVariation> >& GetVertexShaders() { return vertexShaders_; }
    /// Return pixel shaders.
    std::vector<SharedPtr<ShaderVariation> >& GetPixelShaders() { return pixelShaders_; }

private:
    /// Pass index.
    unsigned index_;
    /// Blend mode.
    BlendMode blendMode_;
    /// Depth compare mode.
    CompareMode depthTestMode_;
    /// Lighting mode.
    PassLightingMode lightingMode_;
    /// Last shaders loaded frame number.
    unsigned shadersLoadedFrameNumber_;
    /// Depth write mode.
    bool depthWrite_;
    /// Alpha masking hint.
    bool alphaMask_;
    /// Require desktop level hardware flag.
    bool isDesktop_;
    /// Vertex shader name.
    QString vertexShaderName_;
    /// Pixel shader name.
    QString pixelShaderName_;
    /// Vertex shader defines.
    QString vertexShaderDefines_;
    /// Pixel shader defines.
    QString pixelShaderDefines_;
    /// Vertex shaders.
    std::vector<SharedPtr<ShaderVariation> > vertexShaders_;
    /// Pixel shaders.
    std::vector<SharedPtr<ShaderVariation> > pixelShaders_;
    /// Pass name.
    QString name_;
};

/// %Material technique. Consists of several passes.
class Technique : public Resource
{
    OBJECT(Technique);

    friend class Renderer;

public:
    /// Construct.
    Technique(Context* context);
    /// Destruct.
    ~Technique();
    /// Register object factory.
    static void RegisterObject(Context* context);

    /// Load resource from stream. May be called from a worker thread. Return true if successful.
    virtual bool BeginLoad(Deserializer& source) override;

    /// Set whether requires desktop level hardware.
    void SetIsDesktop(bool enable);
    /// Create a new pass.
    Pass* CreatePass(const QString& passName);
    /// Remove a pass.
    void RemovePass(const QString& passName);
    /// Reset shader pointers in all passes.
    void ReleaseShaders();

    /// Return whether requires desktop level hardware.
    bool IsDesktop() const { return isDesktop_; }
    /// Return whether technique is supported by the current hardware.
    bool IsSupported() const { return !isDesktop_ || desktopSupport_; }
    /// Return whether has a pass.
    bool HasPass(unsigned passIndex) const { return passIndex < passes_.size() && passes_[passIndex].Get() != nullptr; }
    /// Return whether has a pass by name. This overload should not be called in time-critical rendering loops; use a pre-acquired pass index instead.
    bool HasPass(const QString& passName) const;

    /// Return a pass, or null if not found.
    Pass* GetPass(unsigned passIndex) const { return passIndex < passes_.size() ? passes_[passIndex].Get() : nullptr; }
    /// Return a pass by name, or null if not found. This overload should not be called in time-critical rendering loops; use a pre-acquired pass index instead.
    Pass* GetPass(const QString& passName) const;

    /// Return a pass that is supported for rendering, or null if not found.
    Pass* GetSupportedPass(unsigned passIndex) const
    {
        Pass* pass = passIndex < passes_.size() ? passes_[passIndex].Get() : nullptr;
        return pass && (!pass->IsDesktop() || desktopSupport_) ? pass : nullptr;
    }
    /// Return a supported pass by name. This overload should not be called in time-critical rendering loops; use a pre-acquired pass index instead.
    Pass* GetSupportedPass(const QString& passName) const;

    /// Return number of passes.
    unsigned GetNumPasses() const;
    /// Return all pass names.
    std::vector<QString> GetPassNames() const;
    /// Return all passes.
    std::vector<Pass*> GetPasses() const;
    /// Return a pass type index by name. Allocate new if not used yet.
    static unsigned GetPassIndex(const QString& passName);

    /// Index for base pass. Initialized once GetPassIndex() has been called for the first time.
    static unsigned basePassIndex;
    /// Index for alpha pass. Initialized once GetPassIndex() has been called for the first time.
    static unsigned alphaPassIndex;
    /// Index for prepass material pass. Initialized once GetPassIndex() has been called for the first time.
    static unsigned materialPassIndex;
    /// Index for deferred G-buffer pass. Initialized once GetPassIndex() has been called for the first time.
    static unsigned deferredPassIndex;
    /// Index for per-pixel light pass. Initialized once GetPassIndex() has been called for the first time.
    static unsigned lightPassIndex;
    /// Index for lit base pass. Initialized once GetPassIndex() has been called for the first time.
    static unsigned litBasePassIndex;
    /// Index for lit alpha pass. Initialized once GetPassIndex() has been called for the first time.
    static unsigned litAlphaPassIndex;
    /// Index for shadow pass. Initialized once GetPassIndex() has been called for the first time.
    static unsigned shadowPassIndex;

private:
    /// Require desktop GPU flag.
    bool isDesktop_;
    /// Cached desktop GPU support flag.
    bool desktopSupport_;
    /// Passes.
    std::vector<SharedPtr<Pass> > passes_;

    /// Pass index assignments.
    static HashMap<QString, unsigned> passIndices;
};

}
