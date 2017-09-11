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

#include "Lutefisk3D/Container/HashMap.h"
#include "Lutefisk3D/Container/RefCounted.h"
#include "Lutefisk3D/Graphics/GPUObject.h"
#include "Lutefisk3D/Graphics/GraphicsDefs.h"
#include "Lutefisk3D/Graphics/ShaderVariation.h"

#include <QtCore/QString>

namespace Urho3D
{

class ConstantBuffer;
class Graphics;
struct ShaderParameter;
/// Linked shader program on the GPU.
class LUTEFISK3D_EXPORT ShaderProgram : public RefCounted, public GPUObject
{
public:
    ShaderProgram(Graphics* graphics, ShaderVariation* vertexShader, ShaderVariation* pixelShader);
    ~ShaderProgram();
	void OnDeviceLost() override;
	void Release() override;
    bool Link();
    ShaderVariation* GetVertexShader() const;
    ShaderVariation* GetPixelShader() const;
    bool HasParameter(StringHash param) const;
    /// Return whether uses a texture unit.
    bool HasTextureUnit(TextureUnit unit) const { return useTextureUnit_[unit]; }
    const ShaderParameter* GetParameter(StringHash param) const;
    /// Return linker output.
    const QString& GetLinkerOutput() const { return linkerOutput_; }
    /// Return semantic to vertex attributes location mappings used by the shader.
    const HashMap<std::pair<uint8_t, uint8_t>, unsigned>& GetVertexAttributes() const { return vertexAttributes_; }
    /// Return attribute location use bitmask.
    unsigned GetUsedVertexAttributes() const { return usedVertexAttributes_; }
    /// Return all constant buffers.
    const SharedPtr<ConstantBuffer>* GetConstantBuffers() const { return &constantBuffers_[0]; }
    bool NeedParameterUpdate(ShaderParameterGroup group, const void* source);
    void ClearParameterSource(ShaderParameterGroup group);
    static void ClearParameterSources();
    static void ClearGlobalParameterSource(ShaderParameterGroup group);

private:
    /// Vertex shader.
    WeakPtr<ShaderVariation> vertexShader_;
    /// Pixel shader.
    WeakPtr<ShaderVariation> pixelShader_;
    /// Shader parameters.
    HashMap<StringHash, ShaderParameter> shaderParameters_;
    /// Texture unit use.
    bool useTextureUnit_[MAX_TEXTURE_UNITS];
    /// Vertex attributes.
    HashMap<std::pair<uint8_t, uint8_t>, unsigned> vertexAttributes_;
    /// Used vertex attribute location bitmask.
    unsigned usedVertexAttributes_;
    /// Constant buffers by binding index.
    SharedPtr<ConstantBuffer> constantBuffers_[MAX_SHADER_PARAMETER_GROUPS * 2];
    /// Remembered shader parameter sources for individual uniform mode.
    const void* parameterSources_[MAX_SHADER_PARAMETER_GROUPS];
    /// Shader link error string.
    QString linkerOutput_;
    /// Shader parameter source framenumber.
    unsigned frameNumber_;
    /// Global shader parameter source framenumber.
    static unsigned globalFrameNumber;
    /// Remembered global shader parameter sources for constant buffer mode.
    static const void* globalParameterSources[MAX_SHADER_PARAMETER_GROUPS];
};

}
