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

#include "Lutefisk3D/Resource/Resource.h"
#include "Lutefisk3D/Resource/XMLElement.h"

namespace pugi
{
    class xml_document;
    class xml_node;
    class xpath_node;
}

namespace Urho3D
{
/// XML document resource.
class LUTEFISK3D_EXPORT XMLFile : public Resource
{
    URHO3D_OBJECT(XMLFile,Resource)

public:
    explicit XMLFile(Context* context);
    ~XMLFile() override;
    static void RegisterObject(Context* context);

    bool BeginLoad(Deserializer& source) override;
    bool Save(Serializer& dest) const override;
    bool Save(Serializer& dest, const QString& indentation) const;

    bool FromString(const QString& source);
    XMLElement CreateRoot(const QString& name);
    XMLElement GetOrCreateRoot(const QString& name);

    XMLElement GetRoot(const QString& name = QString());
    /// Return the pugixml document.
    pugi::xml_document* GetDocument() const { return document_.get(); }
    QString  ToString(const QString &indentation = "\t") const;

    void Patch(XMLFile* patchFile);
    void Patch(XMLElement patchElement);

private:
    void PatchAdd(const pugi::xml_node& patch, pugi::xpath_node& original) const;
    void PatchReplace(const pugi::xml_node& patch, pugi::xpath_node& original) const;
    void PatchRemove(const pugi::xpath_node& original) const;

    void AddNode(const pugi::xml_node& patch, const pugi::xpath_node& original) const;
    void AddAttribute(const pugi::xml_node& patch, const pugi::xpath_node& original) const;
    bool CombineText(const pugi::xml_node& patch, const pugi::xml_node& original, bool prepend) const;

    /// Pugixml document.
    std::unique_ptr<pugi::xml_document> document_;
};

}
