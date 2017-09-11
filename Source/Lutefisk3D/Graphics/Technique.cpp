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

#include "Technique.h"

#include "Lutefisk3D/Core/Context.h"
#include "Graphics.h"
#include "Lutefisk3D/IO/Log.h"
#include "Lutefisk3D/Core/ProcessUtils.h"
#include "Lutefisk3D/Core/StringUtils.h"
#include "Lutefisk3D/Core/Profiler.h"
#include "Lutefisk3D/Resource/ResourceCache.h"
#include "ShaderVariation.h"
#include "Lutefisk3D/Resource/XMLFile.h"



namespace Urho3D
{

extern const char* cullModeNames[];

const char* blendModeNames[MAX_BLENDMODES+1] =
{
    "replace",
    "add",
    "multiply",
    "alpha",
    "addalpha",
    "premulalpha",
    "invdestalpha",
    "subtract",
    "subtractalpha",
    "zeroinvsrc",
    nullptr
};

static const char* compareModeNames[] =
{
    "always",
    "equal",
    "notequal",
    "less",
    "lessequal",
    "greater",
    "greaterequal",
    nullptr
};

static const char* lightingModeNames[] =
{
    "unlit",
    "pervertex",
    "perpixel",
    nullptr
};

Pass::Pass(const QString& name) :
    blendMode_(BLEND_REPLACE),
    cullMode_(MAX_CULLMODES),
    depthTestMode_(CMP_LESSEQUAL),
    lightingMode_(LIGHTING_UNLIT),
    shadersLoadedFrameNumber_(0),
    alphaToCoverage_(false),
    depthWrite_(true)
{
    name_ = name.toLower();
    index_ = Technique::GetPassIndex(name_);
    // Guess default lighting mode from pass name
    if (index_ == Technique::basePassIndex ||
			index_ == Technique::alphaPassIndex || 
			index_ == Technique::materialPassIndex ||
			index_ == Technique::deferredPassIndex)
        lightingMode_ = LIGHTING_PERVERTEX;
    else if (index_ == Technique::lightPassIndex ||
				index_ == Technique::litBasePassIndex || 
				index_ == Technique::litAlphaPassIndex)
        lightingMode_ = LIGHTING_PERPIXEL;
}

Pass::~Pass()
{
}
/// Set blend mode.
void Pass::SetBlendMode(BlendMode mode)
{
    blendMode_ = mode;
}
/// Set culling mode override. By default culling mode is read from the material instead. Set the illegal culling mode MAX_CULLMODES to disable override again.
void Pass::SetCullMode(CullMode mode)
{
    cullMode_ = mode;
}
/// Set depth compare mode.
void Pass::SetDepthTestMode(CompareMode mode)
{
    depthTestMode_ = mode;
}
/// Set pass lighting mode, affects what shader variations will be attempted to be loaded.
void Pass::SetLightingMode(PassLightingMode mode)
{
    lightingMode_ = mode;
}
/// Set depth write on/off.
void Pass::SetDepthWrite(bool enable)
{
    depthWrite_ = enable;
}
/// Set alpha-to-coverage on/off.
void Pass::SetAlphaToCoverage(bool enable)
{
    alphaToCoverage_ = enable;
}
/// Set vertex shader name.
void Pass::SetVertexShader(const QString& name)
{
    vertexShaderName_ = name;
    ReleaseShaders();
}
/// Set pixel shader name.
void Pass::SetPixelShader(const QString& name)
{
    pixelShaderName_ = name;
    ReleaseShaders();
}
/// Set vertex shader defines. Separate multiple defines with spaces.
void Pass::SetVertexShaderDefines(const QString& defines)
{
    vertexShaderDefines_ = defines;
    ReleaseShaders();
}
/// Set pixel shader defines. Separate multiple defines with spaces.
void Pass::SetPixelShaderDefines(const QString& defines)
{
    pixelShaderDefines_ = defines;
    ReleaseShaders();
}
/// Set vertex shader define excludes. Use to mark defines that the shader code will not recognize, to prevent compiling redundant shader variations.
void Pass::SetVertexShaderDefineExcludes(const QString& excludes)
{
    vertexShaderDefineExcludes_ = excludes;
    ReleaseShaders();
}
/// Set pixel shader define excludes. Use to mark defines that the shader code will not recognize, to prevent compiling redundant shader variations.
void Pass::SetPixelShaderDefineExcludes(const QString& excludes)
{
    pixelShaderDefineExcludes_ = excludes;
    ReleaseShaders();
}
/// Reset shader pointers.
void Pass::ReleaseShaders()
{
    vertexShaders_.clear();
    pixelShaders_.clear();
    extraVertexShaders_.clear();
    extraPixelShaders_.clear();
}
/// Mark shaders loaded this frame.
void Pass::MarkShadersLoaded(unsigned frameNumber)
{
    shadersLoadedFrameNumber_ = frameNumber;
}
QString Pass::GetEffectiveVertexShaderDefines() const
{
    // Prefer to return just the original defines if possible
    if (vertexShaderDefineExcludes_.isEmpty())
        return vertexShaderDefines_;

    QStringList vsDefines = vertexShaderDefines_.split(' ');
    QStringList vsExcludes = vertexShaderDefineExcludes_.split(' ');
    for (unsigned i = 0; i < vsExcludes.size(); ++i)
        vsDefines.removeAll(vsExcludes[i]);

    return vsDefines.join(" ");
}

QString Pass::GetEffectivePixelShaderDefines() const
{
    // Prefer to return just the original defines if possible
    if (pixelShaderDefineExcludes_.isEmpty())
        return pixelShaderDefines_;

    QStringList psDefines = pixelShaderDefines_.split(' ');
    QStringList psExcludes = pixelShaderDefineExcludes_.split(' ');
    for (unsigned i = 0; i < psExcludes.size(); ++i)
        psDefines.removeAll(psExcludes[i]);

    return psDefines.join(" ");
}
std::vector<SharedPtr<ShaderVariation> >& Pass::GetVertexShaders(const StringHash& extraDefinesHash)
{
    // If empty hash, return the base shaders
    if (!extraDefinesHash.Value())
        return vertexShaders_;
	
	return extraVertexShaders_[extraDefinesHash];
}

std::vector<SharedPtr<ShaderVariation> >& Pass::GetPixelShaders(const StringHash& extraDefinesHash)
{
    if (!extraDefinesHash.Value())
        return pixelShaders_;
	
	return extraPixelShaders_[extraDefinesHash];
}
unsigned Technique::basePassIndex = 0;
unsigned Technique::alphaPassIndex = 0;
unsigned Technique::materialPassIndex = 0;
unsigned Technique::deferredPassIndex = 0;
unsigned Technique::lightPassIndex = 0;
unsigned Technique::litBasePassIndex = 0;
unsigned Technique::litAlphaPassIndex = 0;
unsigned Technique::shadowPassIndex = 0;
HashMap<QString, unsigned> Technique::passIndices;

Technique::Technique(Context* context) :
    Resource(context)
{
}

Technique::~Technique()
{
}

void Technique::RegisterObject(Context* context)
{
    context->RegisterFactory<Technique>();
}

bool Technique::BeginLoad(Deserializer& source)
{
    passes_.clear();
    cloneTechniques_.clear();

    SetMemoryUse(sizeof(Technique));

    SharedPtr<XMLFile> xml(new XMLFile(context_));
    if (!xml->Load(source))
        return false;

    XMLElement rootElem = xml->GetRoot();

    QString globalVS = rootElem.GetAttribute("vs");
    QString globalPS = rootElem.GetAttribute("ps");
    QString globalVSDefines = rootElem.GetAttribute("vsdefines");
    QString globalPSDefines = rootElem.GetAttribute("psdefines");
    // End with space so that the pass-specific defines can be appended
    if (!globalVSDefines.isEmpty())
        globalVSDefines += ' ';
    if (!globalPSDefines.isEmpty())
        globalPSDefines += ' ';

    XMLElement passElem = rootElem.GetChild("pass");
    for (;passElem; passElem = passElem.GetNext("pass"))
    {
		if (!passElem.HasAttribute("name")) {
			URHO3D_LOGERROR("Missing pass name");
			continue;
		}

	    Pass* newPass = CreatePass(passElem.GetAttribute("name"));

	    // Append global defines only when pass does not redefine the shader
	    if (passElem.HasAttribute("vs"))
	    {
		    newPass->SetVertexShader(passElem.GetAttribute("vs"));
		    newPass->SetVertexShaderDefines(passElem.GetAttribute("vsdefines"));
	    }
	    else
	    {
		    newPass->SetVertexShader(globalVS);
		    newPass->SetVertexShaderDefines(globalVSDefines + passElem.GetAttribute("vsdefines"));
	    }
	    if (passElem.HasAttribute("ps"))
	    {
		    newPass->SetPixelShader(passElem.GetAttribute("ps"));
		    newPass->SetPixelShaderDefines(passElem.GetAttribute("psdefines"));
	    }
	    else
	    {
		    newPass->SetPixelShader(globalPS);
		    newPass->SetPixelShaderDefines(globalPSDefines + passElem.GetAttribute("psdefines"));
	    }

	    newPass->SetVertexShaderDefineExcludes(passElem.GetAttribute("vsexcludes"));
	    newPass->SetPixelShaderDefineExcludes(passElem.GetAttribute("psexcludes"));
	    if (passElem.HasAttribute("lighting"))
	    {
		    QString lighting = passElem.GetAttributeLower("lighting");
		    newPass->SetLightingMode((PassLightingMode)GetStringListIndex(lighting, lightingModeNames,
		                                                                  LIGHTING_UNLIT));
	    }

	    if (passElem.HasAttribute("blend"))
	    {
		    QString blend = passElem.GetAttributeLower("blend");
		    newPass->SetBlendMode((BlendMode)GetStringListIndex(blend, blendModeNames, BLEND_REPLACE));
	    }

	    if (passElem.HasAttribute("cull"))
	    {
		    QString cull = passElem.GetAttributeLower("cull");
		    newPass->SetCullMode((CullMode)GetStringListIndex(cull, cullModeNames, MAX_CULLMODES));
	    }

	    if (passElem.HasAttribute("depthtest"))
	    {
		    QString depthTest = passElem.GetAttributeLower("depthtest");
		    if (depthTest == "false")
			    newPass->SetDepthTestMode(CMP_ALWAYS);
		    else
			    newPass->SetDepthTestMode((CompareMode)GetStringListIndex(depthTest, compareModeNames, CMP_LESS));
	    }

	    if (passElem.HasAttribute("depthwrite"))
		    newPass->SetDepthWrite(passElem.GetBool("depthwrite"));

	    if (passElem.HasAttribute("alphatocoverage"))
		    newPass->SetAlphaToCoverage(passElem.GetBool("alphatocoverage"));
    }

    return true;
}

void Technique::ReleaseShaders()
{
    for (SharedPtr<Pass> & pass : passes_) {
        if(pass)
            pass->ReleaseShaders();
    }
}

SharedPtr<Technique> Technique::Clone(const QString& cloneName) const
{
    SharedPtr<Technique> ret(new Technique(context_));
    ret->SetName(cloneName);

    // Deep copy passes
    for (const auto &i : passes_)
    {
        Pass* srcPass = i.Get();
        if (!srcPass)
            continue;

        Pass* newPass = ret->CreatePass(srcPass->GetName());
        newPass->SetBlendMode(srcPass->GetBlendMode());
        newPass->SetDepthTestMode(srcPass->GetDepthTestMode());
        newPass->SetLightingMode(srcPass->GetLightingMode());
        newPass->SetDepthWrite(srcPass->GetDepthWrite());
        newPass->SetAlphaToCoverage(srcPass->GetAlphaToCoverage());
        newPass->SetVertexShader(srcPass->GetVertexShader());
        newPass->SetPixelShader(srcPass->GetPixelShader());
        newPass->SetVertexShaderDefines(srcPass->GetVertexShaderDefines());
        newPass->SetPixelShaderDefines(srcPass->GetPixelShaderDefines());
        newPass->SetVertexShaderDefineExcludes(srcPass->GetVertexShaderDefineExcludes());
        newPass->SetPixelShaderDefineExcludes(srcPass->GetPixelShaderDefineExcludes());
    }

    return ret;
}


Pass* Technique::CreatePass(const QString& name)
{
    Pass* oldPass = GetPass(name);
    if (oldPass)
        return oldPass;

    SharedPtr<Pass> newPass(new Pass(name));
    unsigned passIndex = newPass->GetIndex();
    //TODO: passes_ is essentialy an pass_id => Pass dictionary, mark it as one
    if (passIndex >= passes_.size())
        passes_.resize(passIndex + 1);
    passes_[passIndex] = newPass;

    // Calculate memory use now
    SetMemoryUse(unsigned(sizeof(Technique) + GetNumPasses() * sizeof(Pass)));
    return newPass;
}

void Technique::RemovePass(const QString& name)
{
    HashMap<QString, unsigned>::const_iterator i = passIndices.find(name.toLower());
    if (i == passIndices.end())
        return;

    if (MAP_VALUE(i) < passes_.size() && passes_[MAP_VALUE(i)].Get())
    {
        passes_[MAP_VALUE(i)].Reset();
        SetMemoryUse((unsigned)(sizeof(Technique) + GetNumPasses() * sizeof(Pass)));
    }
}

bool Technique::HasPass(const QString& name) const
{
    HashMap<QString, unsigned>::const_iterator i = passIndices.find(name.toLower());
    return i != passIndices.end() ? HasPass(MAP_VALUE(i)) : false;
}

Pass* Technique::GetPass(const QString& name) const
{
    HashMap<QString, unsigned>::const_iterator i = passIndices.find(name.toLower());
    return i != passIndices.end() ? GetPass(MAP_VALUE(i)) : nullptr;
}

Pass* Technique::GetSupportedPass(const QString& name) const
{
    HashMap<QString, unsigned>::const_iterator i = passIndices.find(name.toLower());
    return i != passIndices.end() ? GetSupportedPass(MAP_VALUE(i)) : nullptr;
}

unsigned Technique::GetNumPasses() const
{
    unsigned ret = 0;

    for (std::vector<SharedPtr<Pass> >::const_iterator i = passes_.begin(); i != passes_.end(); ++i)
    {
        if (i->Get())
            ++ret;
    }

    return ret;
}

std::vector<QString> Technique::GetPassNames() const
{
    std::vector<QString> ret;
    ret.reserve(passes_.size());
    for (const SharedPtr<Pass> &pass : passes_)
    {
        if (pass)
            ret.push_back(pass->GetName());
    }

    return ret;
}

std::vector<Pass*> Technique::GetPasses() const
{
    std::vector<Pass*> ret;
    ret.reserve(passes_.size());
    for (const SharedPtr<Pass> &pass : passes_)
    {
        if (pass)
            ret.push_back(pass);
    }
    return ret;
}
SharedPtr<Technique> Technique::CloneWithDefines(const QString& vsDefines, const QString& psDefines)
{
    // Return self if no actual defines
    if (vsDefines.isEmpty() && psDefines.isEmpty())
        return SharedPtr<Technique>(this);

    std::pair<StringHash, StringHash> key = std::make_pair(StringHash(vsDefines), StringHash(psDefines));

    // Return existing if possible
    auto iter = cloneTechniques_.find(key);
    if (iter != cloneTechniques_.end())
        return MAP_VALUE(iter);

    // Set same name as the original for the clones to ensure proper serialization of the material. This should not be a problem
    // since the clones are never stored to the resource cache
    iter = cloneTechniques_.insert(std::make_pair(key, Clone(GetName()))).first;

    for (Pass *pass : MAP_VALUE(iter)->passes_)
    {
        if (!pass)
            continue;
        if (!vsDefines.isEmpty())
            pass->SetVertexShaderDefines(pass->GetVertexShaderDefines() + " " + vsDefines);
        if (!psDefines.isEmpty())
            pass->SetPixelShaderDefines(pass->GetPixelShaderDefines() + " " + psDefines);
    }

    return MAP_VALUE(iter);
}
unsigned Technique::GetPassIndex(const QString& passName)
{
    // Initialize built-in pass indices on first call
    if (passIndices.empty())
    {
        basePassIndex = passIndices["base"] = 0;
        alphaPassIndex = passIndices["alpha"] = 1;
        materialPassIndex = passIndices["material"] = 2;
        deferredPassIndex = passIndices["deferred"] = 3;
        lightPassIndex = passIndices["light"] = 4;
        litBasePassIndex = passIndices["litbase"] = 5;
        litAlphaPassIndex = passIndices["litalpha"] = 6;
        shadowPassIndex = passIndices["shadow"] = 7;
    }

    QString nameLower = passName.toLower();
    HashMap<QString, unsigned>::iterator i = passIndices.find(nameLower);
    if (i != passIndices.end())
        return MAP_VALUE(i);
    unsigned newPassIndex = passIndices.size();
    passIndices[nameLower] = newPassIndex;
    return newPassIndex;
}
}
