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

#include "../../Graphics/ConstantBuffer.h"
#include "../../Graphics/Graphics.h"
#include "../../Graphics/GraphicsImpl.h"
#include "../../Graphics/ShaderProgram.h"
#include "../../Graphics/ShaderVariation.h"
#include "../../IO/Log.h"

using namespace gl;

namespace Urho3D
{

static const char* shaderParameterGroups[] = {
    "frame",
    "camera",
    "zone",
    "light",
    "material",
    "object",
    "custom"
};

static unsigned NumberPostfix(const QString& str)
{
    for (unsigned i = 0; i < str.length(); ++i)
    {
        if (str[i].isDigit())
            return str.midRef(i).toUInt();
    }

    return M_MAX_UNSIGNED;
}

unsigned ShaderProgram::globalFrameNumber = 0;
const void* ShaderProgram::globalParameterSources[MAX_SHADER_PARAMETER_GROUPS];
ShaderProgram::ShaderProgram(Graphics* graphics, ShaderVariation* vertexShader, ShaderVariation* pixelShader) :
    GPUObject(graphics),
    vertexShader_(vertexShader),
    pixelShader_(pixelShader),
    usedVertexAttributes_(0),
    frameNumber_(0)
{
    for (auto & elem : useTextureUnit_)
        elem = false;
    for (auto & elem : parameterSources_)
        elem = (const void*)M_MAX_UNSIGNED;
}

ShaderProgram::~ShaderProgram()
{
    Release();
}
void ShaderProgram::OnDeviceLost()
{
    GPUObject::OnDeviceLost();

    if (graphics_ && graphics_->GetShaderProgram() == this)
        graphics_->SetShaders(nullptr, nullptr);


    linkerOutput_.clear();
}

void ShaderProgram::Release()
{
    if (object_)
    {
        if (!graphics_)
            return;

        if (!graphics_->IsDeviceLost())
        {
            if (graphics_->GetShaderProgram() == this)
                graphics_->SetShaders(nullptr, nullptr);

            gl::glDeleteProgram(object_);
        }

        object_ = 0;
        linkerOutput_.clear();
        shaderParameters_.clear();
        vertexAttributes_.clear();
        usedVertexAttributes_ = 0;

        for (auto & elem : useTextureUnit_)
            elem = false;
        for (unsigned i = 0; i < MAX_SHADER_PARAMETER_GROUPS; ++i)
            constantBuffers_[i].Reset();
    }
}

bool ShaderProgram::Link()
{
    Release();

    if (!vertexShader_ || !pixelShader_ || !vertexShader_->GetGPUObject() || !pixelShader_->GetGPUObject())
        return false;

    object_ = gl::glCreateProgram();
    if (!object_)
    {
        linkerOutput_ = "Could not create shader program";
        return false;
    }

    gl::glAttachShader(object_, vertexShader_->GetGPUObject());
    gl::glAttachShader(object_, pixelShader_->GetGPUObject());
    gl::glLinkProgram(object_);

    int linked, length;
    gl::glGetProgramiv(object_, gl::GL_LINK_STATUS, &linked);
    if (!linked)
    {
        gl::glGetProgramiv(object_, gl::GL_INFO_LOG_LENGTH, &length);
        QByteArray linkerMessage(length,0);
        int outLength;
        gl::glGetProgramInfoLog(object_, length, &outLength, linkerMessage.data());
        gl::glDeleteProgram(object_);
        linkerOutput_ = linkerMessage;
        object_ = 0;
    }
    else
        linkerOutput_.clear();

    if (!object_)
        return false;

    const int MAX_NAME_LENGTH = 256;
    char nameBuffer[MAX_NAME_LENGTH];
    int attributeCount, uniformCount, elementCount, nameLength;
    GLenum type;

    glUseProgram(object_);

    // Check for vertex attributes
    glGetProgramiv(object_, GL_ACTIVE_ATTRIBUTES, &attributeCount);
    for (int i = 0; i < attributeCount; ++i)
    {
        glGetActiveAttrib(object_, i, (GLsizei)MAX_NAME_LENGTH, &nameLength, &elementCount, &type, nameBuffer);

        QString name = QString::fromLatin1(nameBuffer, nameLength);
        VertexElementSemantic semantic = MAX_VERTEX_ELEMENT_SEMANTICS;
        unsigned char semanticIndex = 0;

        // Go in reverse order so that "binormal" is detected before "normal"
        for (unsigned j = MAX_VERTEX_ELEMENT_SEMANTICS - 1; j < MAX_VERTEX_ELEMENT_SEMANTICS; --j)
        {
            if (name.contains(ShaderVariation::elementSemanticNames[j], Qt::CaseInsensitive))
            {
                semantic = (VertexElementSemantic)j;
                unsigned index = NumberPostfix(name);
                if (index != M_MAX_UNSIGNED)
                    semanticIndex = (unsigned char)index;
                break;
            }
        }

        if (semantic == MAX_VERTEX_ELEMENT_SEMANTICS)
        {
            URHO3D_LOGWARNING("Found vertex attribute " + name + " with no known semantic in shader program " +
                              vertexShader_->GetFullName() + " " + pixelShader_->GetFullName());
            continue;
        }

        int location = glGetAttribLocation(object_, qPrintable(name));
        vertexAttributes_[std::make_pair((unsigned char)semantic, semanticIndex)] = location;
        usedVertexAttributes_ |= (1 << location);
    }

    // Check for constant buffers
    HashMap<unsigned, unsigned> blockToBinding;

    int numUniformBlocks = 0;

    glGetProgramiv(object_, GL_ACTIVE_UNIFORM_BLOCKS, &numUniformBlocks);
    for (int i = 0; i < numUniformBlocks; ++i)
    {
        int nameLength;
        glGetActiveUniformBlockName(object_, i, MAX_NAME_LENGTH, &nameLength, nameBuffer);

        QString name = QString::fromLatin1(nameBuffer, nameLength);

        unsigned blockIndex = glGetUniformBlockIndex(object_, qPrintable(name));
        unsigned group = M_MAX_UNSIGNED;

        // Try to recognize the use of the buffer from its name
        for (unsigned j = 0; j < MAX_SHADER_PARAMETER_GROUPS; ++j)
        {
            if (name.contains(shaderParameterGroups[j], Qt::CaseInsensitive))
            {
                group = j;
                break;
            }
        }

        // If name is not recognized, search for a digit in the name and use that as the group index
        if (group == M_MAX_UNSIGNED)
        {
            group = NumberPostfix(name);
        }

        if (group >= MAX_SHADER_PARAMETER_GROUPS)
        {
            URHO3D_LOGWARNING("Skipping unrecognized uniform block " + name + " in shader program " + vertexShader_->GetFullName() +
                              " " + pixelShader_->GetFullName());
            continue;
        }

        // Find total constant buffer data size
        int dataSize;
        glGetActiveUniformBlockiv(object_, blockIndex, GL_UNIFORM_BLOCK_DATA_SIZE, &dataSize);
        if (!dataSize)
            continue;

        unsigned bindingIndex = group;
        // Vertex shader constant buffer bindings occupy slots starting from zero to maximum supported, pixel shader bindings
        // from that point onward
        ShaderType shaderType = VS;
        if (name.contains("PS", Qt::CaseInsensitive))
        {
            bindingIndex += MAX_SHADER_PARAMETER_GROUPS;
            shaderType = PS;
        }

        glUniformBlockBinding(object_, blockIndex, bindingIndex);
        blockToBinding[blockIndex] = bindingIndex;

        constantBuffers_[bindingIndex] = graphics_->GetOrCreateConstantBuffer(shaderType, bindingIndex, (unsigned)dataSize);
    }
    // Check for shader parameters and texture units
    glGetProgramiv(object_, GL_ACTIVE_UNIFORMS, &uniformCount);
    for (int i = 0; i < uniformCount; ++i)
    {
        GLenum type;

        glGetActiveUniform(object_, (GLuint)i, MAX_NAME_LENGTH, 0, &elementCount, &type, nameBuffer);
        int location = glGetUniformLocation(object_, nameBuffer);

        // Check for array index included in the name and strip it
        QString name(nameBuffer);
        unsigned index = name.indexOf('[');
        if (index != -1)
        {
            // If not the first index, skip
            if (name.indexOf("[0]", index) == -1)
                continue;

            name = name.mid(0, index);
        }

        if (name[0] == 'c')
        {
            // Store constant uniform
            QString paramName = name.mid(1);
            ShaderParameter newParam;
            newParam.name_ = paramName;
            newParam.glType_ = type;
            newParam.location_ = location;
            bool store = location >= 0;

            // If running OpenGL 3, the uniform may be inside a constant buffer
            if (newParam.location_ < 0)
            {
                int blockIndex, blockOffset;
                glGetActiveUniformsiv(object_, 1, (const GLuint*)&i, GL_UNIFORM_BLOCK_INDEX, &blockIndex);
                glGetActiveUniformsiv(object_, 1, (const GLuint*)&i, GL_UNIFORM_OFFSET, &blockOffset);
                if (blockIndex >= 0)
                {
                    newParam.offset_ = blockOffset;
                    newParam.bufferPtr_ = constantBuffers_[blockToBinding[blockIndex]];
                    store = true;
                }
            }

            if (store)
                shaderParameters_[StringHash(paramName)] = newParam;
        }
        else if (location >= 0 && name[0] == 's')
        {
            // Set the samplers here so that they do not have to be set later
            unsigned unit = graphics_->GetTextureUnit(name.mid(1));
            if (unit >= MAX_TEXTURE_UNITS)
            {
                // If texture unit name is not recognized, search for a digit in the name and use that as the unit index
                unit = NumberPostfix(name);
            }

            if (unit < MAX_TEXTURE_UNITS)
            {
                useTextureUnit_[unit] = true;
                glUniform1iv(location, 1, reinterpret_cast<int*>(&unit));
            }
        }
    }

    // Rehash the parameter map to ensure minimal load factor
    // load factor is automatically maintained by unordered_map
    // vertexAttributes_.Rehash(NextPowerOfTwo(vertexAttributes_.Size()));
    // shaderParameters_.Rehash(NextPowerOfTwo(shaderParameters_.Size()));

    return true;
}

ShaderVariation* ShaderProgram::GetVertexShader() const
{
    return vertexShader_;
}

ShaderVariation* ShaderProgram::GetPixelShader() const
{
    return pixelShader_;
}

bool ShaderProgram::HasParameter(StringHash param) const
{
    return shaderParameters_.contains(param);
}

const ShaderParameter* ShaderProgram::GetParameter(StringHash param) const
{
    auto i = shaderParameters_.find(param);
    if (i != shaderParameters_.end())
        return &(MAP_VALUE(i));
    else
        return nullptr;
}

bool ShaderProgram::NeedParameterUpdate(ShaderParameterGroup group, const void* source)
{
    // If global framenumber has changed, invalidate all per-program parameter sources now
    if (globalFrameNumber != frameNumber_)
    {
        for (auto & elem : parameterSources_)
            elem = (const void*)M_MAX_UNSIGNED;
        frameNumber_ = globalFrameNumber;
    }

    // The shader program may use a mixture of constant buffers and individual uniforms even in the same group
    bool useBuffer = constantBuffers_[group].Get() || constantBuffers_[group + MAX_SHADER_PARAMETER_GROUPS].Get();
    bool useIndividual = !constantBuffers_[group].Get() || !constantBuffers_[group + MAX_SHADER_PARAMETER_GROUPS].Get();
    bool needUpdate = false;

    if (useBuffer && globalParameterSources[group] != source)
    {
        globalParameterSources[group] = source;
        needUpdate = true;
    }

    if (useIndividual && parameterSources_[group] != source)
    {
        parameterSources_[group] = source;
        needUpdate = true;
    }

    return needUpdate;
}

void ShaderProgram::ClearParameterSource(ShaderParameterGroup group)
{
    // The shader program may use a mixture of constant buffers and individual uniforms even in the same group
    bool useBuffer = constantBuffers_[group].Get() || constantBuffers_[group + MAX_SHADER_PARAMETER_GROUPS].Get();
    bool useIndividual = !constantBuffers_[group].Get() || !constantBuffers_[group + MAX_SHADER_PARAMETER_GROUPS].Get();

    if (useBuffer)
        globalParameterSources[group] = (const void*)M_MAX_UNSIGNED;
    if (useIndividual)
        parameterSources_[group] = (const void*)M_MAX_UNSIGNED;
}

void ShaderProgram::ClearParameterSources()
{
    ++globalFrameNumber;
    if (!globalFrameNumber)
        ++globalFrameNumber;

    for (auto & globalParameterSource : globalParameterSources)
        globalParameterSource = (const void*)M_MAX_UNSIGNED;
}

void ShaderProgram::ClearGlobalParameterSource(ShaderParameterGroup group)
{
    globalParameterSources[group] = (const void*)M_MAX_UNSIGNED;
}

}
