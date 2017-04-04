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

#include "../../Graphics/ShaderVariation.h"

#include "../../Graphics/Graphics.h"
#include "../../Graphics/GraphicsImpl.h"
#include "../../IO/Log.h"
#include "../../Graphics/Shader.h"
#include "../../Graphics/ShaderProgram.h"
#include <QString>
#include <QDebug>
using namespace gl;

namespace Urho3D
{
const char* ShaderVariation::elementSemanticNames[] =
{
    "POS",
    "NORMAL",
    "BINORMAL",
    "TANGENT",
    "TEXCOORD",
    "COLOR",
    "BLENDWEIGHT",
    "BLENDINDICES",
    "OBJECTINDEX"
};

void ShaderVariation::OnDeviceLost()
{
    GPUObject::OnDeviceLost();

    compilerOutput_.clear();

}

void ShaderVariation::Release()
{
    if (object_)
    {
        if (!graphics_)
            return;

        if (!graphics_->IsDeviceLost())
        {
            if (type_ == VS)
            {
                if (graphics_->GetVertexShader() == this)
                    graphics_->SetShaders(nullptr, nullptr);
            }
            else
            {
                if (graphics_->GetPixelShader() == this)
                    graphics_->SetShaders(nullptr, nullptr);
            }

            glDeleteShader(object_);
        }

        object_ = 0;
        graphics_->CleanupShaderPrograms(this);
    }

    compilerOutput_.clear();
}

bool ShaderVariation::Create()
{
    Release();

    if (!owner_)
    {
        compilerOutput_ = "Owner shader has expired";
        return false;
    }

    object_ = glCreateShader(type_ == VS ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER);
    if (!object_)
    {
        compilerOutput_ = "Could not create shader object";
        return false;
    }

    const QString& originalShaderCode = owner_->GetSourceCode(type_);
    QString shaderCode;

    // Check if the shader code contains a version define
    unsigned verStart = originalShaderCode.indexOf('#');
    unsigned verEnd = 0;
    if (verStart != -1)
    {
        if (originalShaderCode.midRef(verStart + 1, 7) == "version")
        {
            verEnd = verStart + 9;
            while (verEnd < originalShaderCode.length())
            {
                if (originalShaderCode[verEnd].isDigit())
                    ++verEnd;
                else
                    break;
            }
            // If version define found, insert it first
            QString versionDefine = originalShaderCode.mid(verStart, verEnd - verStart);
            shaderCode += versionDefine + "\n";
        }
    }
    // Force GLSL version 150 if no version define and GL3 is being used
    if (!verEnd)
        shaderCode += "#version 150\n";

    // Distinguish between VS and PS compile in case the shader code wants to include/omit different things
    shaderCode += type_ == VS ? "#define COMPILEVS\n" : "#define COMPILEPS\n";

    // Add define for the maximum number of supported bones
    shaderCode += "#define MAXBONES " + QString::number(Graphics::GetMaxBones()) + "\n";
    // Prepend the defines to the shader code
    QStringList defineVec = defines_.split(' ',QString::SkipEmptyParts);
    for (unsigned i = 0; i < defineVec.size(); ++i)
    {
        // Add extra space for the checking code below
        QString defineString = "#define " + defineVec[i].replace('=', ' ') + " \n";
        shaderCode += defineString;

        // In debug mode, check that all defines are referenced by the shader code
        #ifdef _DEBUG
        QString defineCheck = defineString.mid(8, defineString.indexOf(' ', 8) - 8);
        if (originalShaderCode.indexOf(defineCheck) == -1)
            URHO3D_LOGWARNING("Shader " + GetFullName() + " does not use the define " + defineCheck);
        #endif
    }

    shaderCode += "#define GL3\n";

    // When version define found, do not insert it a second time
    if (verEnd > 0)
        shaderCode += originalShaderCode.midRef(verEnd);
    else
        shaderCode += originalShaderCode;
    QByteArray shaderCodeBytes= shaderCode.toLatin1();
    const char* shaderCStr = shaderCodeBytes.data();
    glShaderSource(object_, 1, &shaderCStr, nullptr);
    glCompileShader(object_);

    int compiled, length;
    glGetShaderiv(object_, GL_COMPILE_STATUS, &compiled);
    if (!compiled)
    {
        glGetShaderiv(object_, GL_INFO_LOG_LENGTH, &length);
        QByteArray compilerOutputBytes(length,0);
        int outLength;
        glGetShaderInfoLog(object_, length, &outLength, compilerOutputBytes.data());
        glDeleteShader(object_);
        object_ = 0;
        compilerOutput_ = compilerOutputBytes;
    }
    else
        compilerOutput_.clear();

    return object_ != 0;
}

void ShaderVariation::SetDefines(const QString& defines)
{
    defines_ = defines;
}

}
