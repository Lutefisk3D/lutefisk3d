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
#include <QtCore/QString>

namespace Urho3D
{
class ConstantBuffer;
class Shader;

/// %Shader parameter definition.
struct LUTEFISK3D_EXPORT ShaderParameter
{
    /// Name of the parameter.
    QString name_;
    /// %Shader type.
    ShaderType type_;

    union
    {
        /// Offset in constant buffer.
        unsigned offset_;
        /// OpenGL uniform location.
        int location_;
    };

    /// Parameter OpenGL type.
    uint32_t glType_;

    /// Constant buffer index. Only used on Direct3D11.
    unsigned buffer_;
    /// Constant buffer pointer. Defined only in shader programs.
    ConstantBuffer* bufferPtr_ = nullptr;
};

/// Vertex or pixel shader on the GPU.
class LUTEFISK3D_EXPORT ShaderVariation : public RefCounted, public GPUObject
{
public:
    /// Construct.
    ShaderVariation(Shader* owner, ShaderType type);
    /// Destruct.
    ~ShaderVariation() override;

    /// Mark the GPU resource destroyed on graphics context destruction.
    void OnDeviceLost() override;
    /// Release the shader.
    void Release() override;

    /// Compile the shader. Return true if successful.
    bool Create();
    /// Set name.
    void SetName(const QString& name);
    /// Set defines.
    void SetDefines(const QString& defines);

    /// Return the owner resource.
    Shader* GetOwner() const;
    /// Return shader type.
    ShaderType GetShaderType() const { return type_; }
    /// Return shader name.
    const QString& GetName() const { return name_; }
    /// Return full shader name.
    QString GetFullName() const { return name_ + "(" + defines_ + ")"; }

    /// Return whether uses a texture unit (only for pixel shaders.) Not applicable on OpenGL, where this information is contained in ShaderProgram instead.
    bool HasTextureUnit(TextureUnit unit) const { return useTextureUnit_[unit]; }

    /// Return vertex element hash.
    uint64_t GetElementHash() const { return elementHash_; }

    /// Return shader bytecode. Stored persistently on Direct3D11 only.
    const std::vector<uint8_t>& GetByteCode() const { return byteCode_; }

    /// Return defines.
    const QString& GetDefines() const { return defines_; }
    /// Return compile error/warning string.
    const QString& GetCompilerOutput() const { return compilerOutput_; }

    /// Return constant buffer data sizes.
    const unsigned* GetConstantBufferSizes() const { return &constantBufferSizes_[0]; }

    /// Return defines with the CLIPPLANE define appended. Used internally on Direct3D11 only, will be empty on other APIs.
    const QString& GetDefinesClipPlane() { return definesClipPlane_; }

    /// vertex semantic names. Used internally.
    static const char* elementSemanticNames[];
private:
    /// Load bytecode from a file. Return true if successful.
    bool LoadByteCode(const QString& binaryShaderName);
    /// Compile from source. Return true if successful.
    bool Compile();
    /// Inspect the constant parameters and input layout (if applicable) from the shader bytecode.
    void ParseParameters(uint8_t* bufData, unsigned bufSize);
    /// Save bytecode to a file.
    void SaveByteCode(const QString& binaryShaderName);
    /// Calculate constant buffer sizes from parameters.
    void CalculateConstantBufferSizes();
    /// Shader this variation belongs to.
    WeakPtr<Shader> owner_;
    /// Shader type.
    ShaderType type_;
    /// Vertex element hash for vertex shaders. Zero for pixel shaders. Note that hashing is different than vertex buffers.
    uint64_t elementHash_;
    /// Texture unit use flags.
    bool useTextureUnit_[MAX_TEXTURE_UNITS];
    /// Constant buffer sizes. 0 if a constant buffer slot is not in use.
    unsigned constantBufferSizes_[MAX_SHADER_PARAMETER_GROUPS];
    /// Shader bytecode. Needed for inspecting the input signature and parameters. Not used on OpenGL.
    std::vector<uint8_t> byteCode_;
    /// Shader name.
    QString name_;
    /// Defines to use in compiling.
    QString defines_;
    /// Defines to use in compiling + CLIPPLANE define appended. Used only on Direct3D11.
    QString definesClipPlane_;
    /// Shader compile error string.
    QString compilerOutput_;
};

}
