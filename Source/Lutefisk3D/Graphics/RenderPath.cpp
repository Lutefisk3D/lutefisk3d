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

#include "RenderPath.h"

#include "Graphics.h"
#include "../IO/Log.h"
#include "Material.h"
#include "../Core/StringUtils.h"
#include "../Resource/XMLFile.h"

namespace Urho3D
{
static const char* commandTypeNames[] =
{
    "none",
    "clear",
    "scenepass",
    "quad",
    "forwardlights",
    "lightvolumes",
    "renderui",
    "sendevent",
    nullptr
};

static const char* sortModeNames[] =
{
    "fronttoback",
    "backtofront",
    nullptr
};

extern const char* blendModeNames[];

TextureUnit ParseTextureUnitName(QString name);

void RenderTargetInfo::Load(const XMLElement& element)
{
    name_ = element.GetAttribute("name");
    tag_ = element.GetAttribute("tag");
    if (element.HasAttribute("enabled"))
        enabled_ = element.GetBool("enabled");
    if (element.HasAttribute("cubemap"))
        cubemap_ = element.GetBool("cubemap");

    QString formatName = element.GetAttribute("format");
    format_ = Graphics::GetFormat(formatName);

    if (element.HasAttribute("filter"))
        filtered_ = element.GetBool("filter");

    if (element.HasAttribute("srgb"))
        sRGB_ = element.GetBool("srgb");

    if (element.HasAttribute("persistent"))
        persistent_ = element.GetBool("persistent");

    if (element.HasAttribute("size"))
        size_ = element.GetVector2("size");
    if (element.HasAttribute("sizedivisor"))
    {
        size_ = element.GetVector2("sizedivisor");
        sizeMode_ = SIZE_VIEWPORTDIVISOR;
    }
    else if (element.HasAttribute("rtsizedivisor"))
    {
        // Deprecated rtsizedivisor mode, acts the same as sizedivisor mode now
        URHO3D_LOGWARNING("Deprecated rtsizedivisor mode used in rendertarget definition");
        size_ = element.GetVector2("rtsizedivisor");
        sizeMode_ = SIZE_VIEWPORTDIVISOR;
    }
    else if (element.HasAttribute("sizemultiplier"))
    {
        size_ = element.GetVector2("sizemultiplier");
        sizeMode_ = SIZE_VIEWPORTMULTIPLIER;
    }

    if (element.HasAttribute("width"))
        size_.x_ = element.GetFloat("width");
    if (element.HasAttribute("height"))
        size_.y_ = element.GetFloat("height");
    if (element.HasAttribute("multisample"))
        multiSample_ = Clamp(element.GetInt("multisample"), 1, 16);
    if (element.HasAttribute("autoresolve"))
        autoResolve_ = element.GetBool("autoresolve");
}

void RenderPathCommand::Load(const XMLElement& element)
{
    type_ = (RenderCommandType)GetStringListIndex(element.GetAttributeLower("type"), commandTypeNames, CMD_NONE);
    tag_ = element.GetAttribute("tag");
    if (element.HasAttribute("enabled"))
        enabled_ = element.GetBool("enabled");
    if (element.HasAttribute("metadata"))
        metadata_ = element.GetAttribute("metadata");

    switch (type_)
    {
    case CMD_CLEAR:
        if (element.HasAttribute("color"))
        {
            clearFlags_ |= CLEAR_COLOR;
            if (element.GetAttributeLower("color") == "fog")
                useFogColor_ = true;
            else
                clearColor_ = element.GetColor("color");
        }
        if (element.HasAttribute("depth"))
        {
            clearFlags_ |= CLEAR_DEPTH;
            clearDepth_ = element.GetFloat("depth");
        }
        if (element.HasAttribute("stencil"))
        {
            clearFlags_ |= CLEAR_STENCIL;
            clearStencil_ = element.GetInt("stencil");
        }
        break;

    case CMD_SCENEPASS:
        pass_ = element.GetAttribute("pass");
        sortMode_ = (RenderCommandSortMode)GetStringListIndex(element.GetAttributeLower("sort"), sortModeNames, SORT_FRONTTOBACK);
        if (element.HasAttribute("marktostencil"))
            markToStencil_ = element.GetBool("marktostencil");
        if (element.HasAttribute("vertexlights"))
            vertexLights_ = element.GetBool("vertexlights");
        break;

    case CMD_FORWARDLIGHTS:
        pass_ = element.GetAttribute("pass");
        if (element.HasAttribute("uselitbase"))
            useLitBase_ = element.GetBool("uselitbase");
        break;

    case CMD_LIGHTVOLUMES:
    case CMD_QUAD:
        vertexShaderName_ = element.GetAttribute("vs");
        pixelShaderName_ = element.GetAttribute("ps");
        vertexShaderDefines_ = element.GetAttribute("vsdefines");
        pixelShaderDefines_ = element.GetAttribute("psdefines");

        if (type_ == CMD_QUAD)
        {
            if (element.HasAttribute("blend"))
            {
                QString blend = element.GetAttributeLower("blend");
                blendMode_ = ((BlendMode)GetStringListIndex(blend, blendModeNames, BLEND_REPLACE));
            }
            XMLElement parameterElem = element.GetChild("parameter");
            while (parameterElem)
            {
                QString name = parameterElem.GetAttribute("name");
                shaderParameters_[name] = Material::ParseShaderParameterValue(parameterElem.GetAttribute("value"));
                parameterElem = parameterElem.GetNext("parameter");
            }
        }
        break;
    case CMD_SENDEVENT:
        eventName_ = element.GetAttribute("name");
        break;
    default:
        break;
    }

    // By default use 1 output, which is the viewport
    outputs_.clear();
    outputs_.emplace_back(QString("viewport"), FACE_POSITIVE_X);
    if (element.HasAttribute("output"))
        outputs_[0].first = element.GetAttribute("output");
    if (element.HasAttribute("face"))
        outputs_[0].second = (CubeMapFace)element.GetInt("face");
    if (element.HasAttribute("depthstencil"))
        depthStencilName_ = element.GetAttribute("depthstencil");
    // Check for defining multiple outputs
    XMLElement outputElem = element.GetChild("output");
    while (outputElem)
    {
        unsigned index = outputElem.GetInt("index");
        if (index < MAX_RENDERTARGETS)
        {
            if (index >= outputs_.size())
                outputs_.resize(index + 1);
            outputs_[index].first = outputElem.GetAttribute("name");
            outputs_[index].second = outputElem.HasAttribute("face") ? (CubeMapFace)outputElem.GetInt("face") : FACE_POSITIVE_X;
        }
        outputElem = outputElem.GetNext("output");
    }

    XMLElement textureElem = element.GetChild("texture");
    while (textureElem)
    {
        TextureUnit unit = TU_DIFFUSE;
        if (textureElem.HasAttribute("unit"))
            unit = ParseTextureUnitName(textureElem.GetAttribute("unit"));
        if (unit < MAX_TEXTURE_UNITS)
        {
            QString name = textureElem.GetAttribute("name");
            textureNames_[unit] = name;
        }

        textureElem = textureElem.GetNext("texture");
    }
}

void RenderPathCommand::SetTextureName(TextureUnit unit, const QString& name)
{
    if (unit < MAX_TEXTURE_UNITS)
        textureNames_[unit] = name;
}

void RenderPathCommand::SetShaderParameter(const QString& name, const Variant& value)
{
    shaderParameters_[name] = value;
}

void RenderPathCommand::RemoveShaderParameter(const QString& name)
{
    shaderParameters_.remove(name);
}

void RenderPathCommand::SetNumOutputs(unsigned num)
{
    num = Clamp(num, 1U, MAX_RENDERTARGETS);
    outputs_.resize(num);
}

void RenderPathCommand::SetOutput(unsigned index, const QString& name, CubeMapFace face)
{
    if (index < outputs_.size())
        outputs_[index] = {name, face};
    else if (index == outputs_.size() && index < MAX_RENDERTARGETS)
        outputs_.emplace_back(name, face);
}

void RenderPathCommand::SetOutputName(unsigned index, const QString& name)
{
    if (index < outputs_.size())
        outputs_[index].first = name;
    else if (index == outputs_.size() && index < MAX_RENDERTARGETS)
        outputs_.emplace_back(name, FACE_POSITIVE_X);
}

void RenderPathCommand::SetOutputFace(unsigned index, CubeMapFace face)
{
    if (index < outputs_.size())
        outputs_[index].second = face;
    else if (index == outputs_.size() && index < MAX_RENDERTARGETS)
        outputs_.emplace_back(QString(), face);
}
void RenderPathCommand::SetDepthStencilName(const QString& name)
{
    depthStencilName_ = name;
}

const QString& RenderPathCommand::GetTextureName(TextureUnit unit) const
{
    return unit < MAX_TEXTURE_UNITS ? textureNames_[unit] : s_dummy;
}

const Variant& RenderPathCommand::GetShaderParameter(const QString& name) const
{
    VariantMap::const_iterator i = shaderParameters_.find(name);
    return i != shaderParameters_.end() ? MAP_VALUE(i) : Variant::EMPTY;
}

const QString& RenderPathCommand::GetOutputName(unsigned index) const
{
    return index < outputs_.size() ? outputs_[index].first : s_dummy;
}

CubeMapFace RenderPathCommand::GetOutputFace(unsigned index) const
{
    return index < outputs_.size() ? outputs_[index].second : FACE_POSITIVE_X;
}

RenderPath::RenderPath()
{
}

RenderPath::~RenderPath()
{
}

SharedPtr<RenderPath> RenderPath::Clone()
{
    SharedPtr<RenderPath> newRenderPath(new RenderPath());
    newRenderPath->renderTargets_ = renderTargets_;
    newRenderPath->commands_ = commands_;
    return newRenderPath;
}

bool RenderPath::Load(XMLFile* file)
{
    renderTargets_.clear();
    commands_.clear();

    return Append(file);
}

bool RenderPath::Append(XMLFile* file)
{
    if (!file)
        return false;

    XMLElement rootElem = file->GetRoot();
    if (!rootElem)
        return false;

    XMLElement rtElem = rootElem.GetChild("rendertarget");
    while (rtElem)
    {
        RenderTargetInfo info;
        info.Load(rtElem);
        if (!info.name_.trimmed().isEmpty())
            renderTargets_.push_back(info);

        rtElem = rtElem.GetNext("rendertarget");
    }

    XMLElement cmdElem = rootElem.GetChild("command");
    while (cmdElem)
    {
        RenderPathCommand cmd;
        cmd.Load(cmdElem);
        if (cmd.type_ != CMD_NONE)
            commands_.push_back(cmd);

        cmdElem = cmdElem.GetNext("command");
    }

    return true;
}

void RenderPath::SetEnabled(const QString& tag, bool active)
{
    for (unsigned i = 0; i < renderTargets_.size(); ++i)
    {
        if (!renderTargets_[i].tag_.compare(tag, Qt::CaseInsensitive))
            renderTargets_[i].enabled_ = active;
    }

    for (unsigned i = 0; i < commands_.size(); ++i)
    {
        if (!commands_[i].tag_.compare(tag, Qt::CaseInsensitive))
            commands_[i].enabled_ = active;
    }
}

void RenderPath::ToggleEnabled(const QString& tag)
{
    for (unsigned i = 0; i < renderTargets_.size(); ++i)
    {
        if (!renderTargets_[i].tag_.compare(tag, Qt::CaseInsensitive))
            renderTargets_[i].enabled_ = !renderTargets_[i].enabled_;
    }

    for (unsigned i = 0; i < commands_.size(); ++i)
    {
        if (!commands_[i].tag_.compare(tag, Qt::CaseInsensitive))
            commands_[i].enabled_ = !commands_[i].enabled_;
    }
}

void RenderPath::SetRenderTarget(unsigned index, const RenderTargetInfo& info)
{
    if (index < renderTargets_.size())
        renderTargets_[index] = info;
    else if (index == renderTargets_.size())
        AddRenderTarget(info);
}

void RenderPath::AddRenderTarget(const RenderTargetInfo& info)
{
    renderTargets_.push_back(info);
}

void RenderPath::RemoveRenderTarget(unsigned index)
{
    renderTargets_.erase(renderTargets_.begin()+index);
}

void RenderPath::RemoveRenderTarget(const QString& name)
{
    for (unsigned i = 0; i < renderTargets_.size(); ++i)
    {
        if (!renderTargets_[i].name_.compare(name, Qt::CaseInsensitive))
        {
            renderTargets_.erase(renderTargets_.begin()+i);
            return;
        }
    }
}

void RenderPath::RemoveRenderTargets(const QString& tag)
{
    for (unsigned i = renderTargets_.size() - 1; i < renderTargets_.size(); --i)
    {
        if (!renderTargets_[i].tag_.compare(tag, Qt::CaseInsensitive))
            renderTargets_.erase(renderTargets_.begin()+i);
    }
}

void RenderPath::SetCommand(unsigned index, const RenderPathCommand& command)
{
    if (index < commands_.size())
        commands_[index] = command;
    else if (index == commands_.size())
        AddCommand(command);
}

void RenderPath::AddCommand(const RenderPathCommand& command)
{
    commands_.push_back(command);
}

void RenderPath::InsertCommand(unsigned index, const RenderPathCommand& command)
{
    commands_.insert(commands_.begin()+index, command);
}

void RenderPath::RemoveCommand(unsigned index)
{
    commands_.erase(commands_.begin()+index);
}

void RenderPath::RemoveCommands(const QString& tag)
{
    for (unsigned i = commands_.size() - 1; i < commands_.size(); --i)
    {
        if (!commands_[i].tag_.compare(tag, Qt::CaseInsensitive))
            commands_.erase(commands_.begin()+i);
    }
}

void RenderPath::SetShaderParameter(const QString& name, const Variant& value)
{
    StringHash nameHash(name);

    for (unsigned i = 0; i < commands_.size(); ++i)
    {
        VariantMap::iterator j = commands_[i].shaderParameters_.find(nameHash);
        if (j != commands_[i].shaderParameters_.end())
            MAP_VALUE(j) = value;
    }
}

const Variant& RenderPath::GetShaderParameter(const QString& name) const
{
    StringHash nameHash(name);

    for (unsigned i = 0; i < commands_.size(); ++i)
    {
        VariantMap::const_iterator j = commands_[i].shaderParameters_.find(nameHash);
        if (j != commands_[i].shaderParameters_.end())
            return MAP_VALUE(j);
    }

    return Variant::EMPTY;
}

}
