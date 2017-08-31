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

#include "Localization.h"
#include "ResourceCache.h"
#include "JSONFile.h"
#include "ResourceEvents.h"
#include "Lutefisk3D/IO/Log.h"
#include "Lutefisk3D/Core/Context.h"
namespace Urho3D
{

Localization::Localization(Context* context) :
    Object(context),
    languageIndex_(-1)
{
}

Localization::~Localization()
{
}

int Localization::GetLanguageIndex(const QString& language)
{
    if (language.isEmpty())
    {
        URHO3D_LOGWARNING("Localization::GetLanguageIndex(language): language name is empty");
        return -1;
    }
    if (GetNumLanguages() == 0)
    {
        URHO3D_LOGWARNING("Localization::GetLanguageIndex(language): no loaded languages");
        return -1;
    }
    for (int i = 0; i < GetNumLanguages(); i++)
    {
        if (languages_[i] == language)
            return i;
    }
    return -1;
}

QString Localization::GetLanguage()
{
    if (languageIndex_ == -1)
    {
        URHO3D_LOGWARNING("Localization::GetLanguage(): no loaded languages");
        return s_dummy;
    }
    return languages_[languageIndex_];
}

QString Localization::GetLanguage(int index)
{
    if (GetNumLanguages() == 0)
    {
        URHO3D_LOGWARNING("Localization::GetLanguage(index): no loaded languages");
        return s_dummy;
    }
    if (index < 0 || index >= GetNumLanguages())
    {
        URHO3D_LOGWARNING("Localization::GetLanguage(index): index out of range");
        return s_dummy;
    }
    return languages_[index];
}

void Localization::SetLanguage(int index)
{
    if (GetNumLanguages() == 0)
    {
        URHO3D_LOGWARNING("Localization::SetLanguage(index): no loaded languages");
        return;
    }
    if (index < 0 || index >= GetNumLanguages())
    {
        URHO3D_LOGWARNING("Localization::SetLanguage(index): index out of range");
        return;
    }
    if (index != languageIndex_)
    {
        languageIndex_ = index;
        g_resourceSignals.changeLanguage.Emit();
    }
}

void Localization::SetLanguage(const QString& language)
{
    if (language.isEmpty())
    {
        URHO3D_LOGWARNING("Localization::SetLanguage(language): language name is empty");
        return;
    }
    if (GetNumLanguages() == 0)
    {
        URHO3D_LOGWARNING("Localization::SetLanguage(language): no loaded languages");
        return;
    }
    int index = GetLanguageIndex(language);
    if (index == -1)
    {
        URHO3D_LOGWARNING("Localization::SetLanguage(language): language not found");
        return;
    }
    SetLanguage(index);
}

QString Localization::Get(const QString& id)
{
    if (id.isEmpty())
        return s_dummy;
    if (GetNumLanguages() == 0)
    {
        URHO3D_LOGWARNING("Localization::Get(id): no loaded languages");
        return id;
    }
    QString result = strings_[StringHash(GetLanguage())][StringHash(id)];
    if (result.isEmpty())
    {
        URHO3D_LOGWARNING("Localization::Get(\"" + id + "\") not found translation, language=\"" + GetLanguage() + "\"");
        return id;
    }
    return result;
}

void Localization::Reset()
{
    languages_.clear();
    languageIndex_ = -1;
    strings_.clear();
}

void Localization::LoadJSON(const JSONValue& source)
{
    for (JSONObject::const_iterator i = source.GetObject().begin(); i != source.GetObject().end(); ++i)
    {
        QString id = MAP_KEY(i);
        if (id.isEmpty())
        {
            URHO3D_LOGWARNING("Localization::LoadJSON(source): string ID is empty");
            continue;
        }
        const JSONObject& langs = MAP_VALUE(i).GetObject();
        for (JSONObject::const_iterator j = langs.begin(); j != langs.end(); ++j)
        {
            const QString& lang = MAP_KEY(j);
            if (lang.isEmpty())
            {
                URHO3D_LOGWARNING("Localization::LoadJSON(source): language name is empty, string ID=\"" + id + "\"");
                continue;
            }
            const QString& string = MAP_VALUE(j).GetString();
            if (string.isEmpty())
            {
                URHO3D_LOGWARNING(
                    "Localization::LoadJSON(source): translation is empty, string ID=\"" + id + "\", language=\"" + lang + "\"");
                continue;
            }
            if (strings_[StringHash(lang)][StringHash(id)] != s_dummy)
            {
                URHO3D_LOGWARNING(
                    "Localization::LoadJSON(source): override translation, string ID=\"" + id + "\", language=\"" + lang + "\"");
            }
            strings_[StringHash(lang)][StringHash(id)] = string;
            if (!languages_.contains(lang))
                languages_.push_back(lang);
            if (languageIndex_ == -1)
                languageIndex_ = 0;
        }
    }
}

void Localization::LoadJSONFile(const QString& name)
{
    ResourceCache* cache = context_->m_ResourceCache.get();
    JSONFile* jsonFile = cache->GetResource<JSONFile>(name);
    if (jsonFile)
        LoadJSON(jsonFile->GetRoot());
}

}
