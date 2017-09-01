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

#include "Shader.h"

#include "ShaderVariation.h"
#include "Graphics.h"
#include "Lutefisk3D/Core/Context.h"
#include "Lutefisk3D/IO/Deserializer.h"
#include "Lutefisk3D/IO/FileSystem.h"
#include "Lutefisk3D/IO/File.h"
#include "Lutefisk3D/IO/Log.h"
#include "Lutefisk3D/Core/Profiler.h"
#include "Lutefisk3D/Resource/ResourceCache.h"
#include "Lutefisk3D/Resource/XMLFile.h"

namespace Urho3D
{

void CommentOutFunction(QString& code, const QString& signature)
{
    unsigned startPos = code.indexOf(signature);
    unsigned braceLevel = 0;
    if (startPos == -1)
        return;

    code.insert(startPos, "/*");

    for (unsigned i = startPos + 2 + signature.length(); i < code.length(); ++i)
    {
        if (code[i] == '{')
            ++braceLevel;
        else if (code[i] == '}')
        {
            --braceLevel;
            if (braceLevel == 0)
            {
                code.insert(i + 1, "*/");
                return;
            }
        }
    }
}

Shader::Shader(Context* context) :
    Resource(context),
    timeStamp_(0),
    numVariations_(0)
{
    RefreshMemoryUse();
}

Shader::~Shader()
{
    ResourceCache* cache = context_->m_ResourceCache.get();
    if(cache)
        cache->ResetDependencies(this);
}

void Shader::RegisterObject(Context* context)
{
    context->RegisterFactory<Shader>();
}

bool Shader::BeginLoad(Deserializer& source)
{
    Graphics* graphics = context_->m_Graphics.get();
    if (!graphics)
        return false;

    // Load the shader source code and resolve any includes
    timeStamp_ = 0;
    QString shaderCode;
    if (!ProcessSource(shaderCode, source))
        return false;

    // Comment out the unneeded shader function
    vsSourceCode_ = shaderCode;
    psSourceCode_ = shaderCode;
    CommentOutFunction(vsSourceCode_, "void PS(");
    CommentOutFunction(psSourceCode_, "void VS(");

    // OpenGL: rename either VS() or PS() to main()
    vsSourceCode_.replace("void VS(", "void main(");
    psSourceCode_.replace("void PS(", "void main(");

    RefreshMemoryUse();
    return true;
}

bool Shader::EndLoad()
{
    // If variations had already been created, release them and require recompile
    for (auto & elem : vsVariations_)
        ELEMENT_VALUE(elem)->Release();
    for (auto & elem : psVariations_)
        ELEMENT_VALUE(elem)->Release();

    return true;
}

ShaderVariation* Shader::GetVariation(ShaderType type, const QString& defines)
{
    return GetVariation(type, qPrintable(defines));
}

ShaderVariation* Shader::GetVariation(ShaderType type, const char* defines)
{
    StringHash definesHash(defines);
    HashMap<StringHash, SharedPtr<ShaderVariation> > & variations(type == VS ? vsVariations_ : psVariations_);
    HashMap<StringHash, SharedPtr<ShaderVariation> >::iterator i = variations.find(definesHash);
    if (i == variations.end())
    {
        // If shader not found, normalize the defines (to prevent duplicates) and check again. In that case make an alias
        // so that further queries are faster
        QString normalizedDefines = NormalizeDefines(defines);
        StringHash normalizedHash(normalizedDefines);

        i = variations.find(normalizedHash);
        if (i != variations.end())
            variations.insert(std::make_pair(definesHash, MAP_VALUE(i)));
        else
        {
            // No shader variation found. Create new
            i = variations.insert(std::make_pair(normalizedHash, SharedPtr<ShaderVariation>(new ShaderVariation(this, type)))).first;
            if (definesHash != normalizedHash)
                variations.insert(std::make_pair(definesHash, MAP_VALUE(i)));

            MAP_VALUE(i)->SetName(GetFileName(GetName()));
            MAP_VALUE(i)->SetDefines(normalizedDefines);
            ++numVariations_;
            RefreshMemoryUse();
        }
    }

    return MAP_VALUE(i);
}

bool Shader::ProcessSource(QString& code, Deserializer& source)
{
    ResourceCache* cache = context_->m_ResourceCache.get();

    // If the source if a non-packaged file, store the timestamp
    File* file = dynamic_cast<File*>(&source);
    if (file && !file->IsPackaged())
    {
        FileSystem* fileSystem = context_->m_FileSystem.get();
        QString fullName = cache->GetResourceFileName(file->GetName());
        unsigned fileTimeStamp = fileSystem->GetLastModifiedTime(fullName);
        if (fileTimeStamp > timeStamp_)
            timeStamp_ = fileTimeStamp;
    }

    // Store resource dependencies for includes so that we know to reload if any of them changes
    if (source.GetName() != GetName())
        cache->StoreResourceDependency(this, source.GetName());

    while (!source.IsEof())
    {
        QString line = source.ReadLine();

        if (line.startsWith("#include"))
        {
            QString includeFileName = GetPath(source.GetName()) + line.mid(9).replace("\"", "").trimmed();

            std::unique_ptr<File> includeFile = cache->GetFile(includeFileName);
            if (!includeFile)
                return false;

            // Add the include file into the current code recursively
            if (!ProcessSource(code, *includeFile))
                return false;
        }
        else
        {
            code += line;
            code += "\n";
        }
    }

    // Finally insert an empty line to mark the space between files
    code += "\n";

    return true;
}

QString Shader::NormalizeDefines(const QString& defines)
{
    QStringList definesVec = defines.toUpper().trimmed().split(' ',QString::SkipEmptyParts);
    std::sort(definesVec.begin(), definesVec.end());
    return definesVec.join(" ");
}

void Shader::RefreshMemoryUse()
{
    SetMemoryUse(sizeof(Shader) + vsSourceCode_.length() + psSourceCode_.length() + numVariations_ * sizeof(ShaderVariation));
}

}
