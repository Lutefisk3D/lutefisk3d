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

#pragma once

#include "Lutefisk3D/Scene/Component.h"

namespace Urho3D
{
class JSONValue;
/// Placeholder for allowing unregistered components to be loaded & saved along with scenes.
class URHO3D_API UnknownComponent : public Component
{
public:
    /// Construct.
    UnknownComponent(Context* context);

    /// Register object factory.
    static void RegisterObject(Context* context);

    /// Return type of the stored component.
    virtual StringHash GetType() const override { return typeHash_; }
    /// Return type name of the stored component.
    virtual const QString& GetTypeName() const  override { return typeName_; }
    /// Return attribute descriptions, or null if none defined.
    virtual const std::vector<AttributeInfo>* GetAttributes() const  override { return &xmlAttributeInfos_; }
    /// Load from binary data. Return true if successful.
    virtual bool Load(Deserializer& source, bool setInstanceDefault = false)  override;
    /// Load from XML data. Return true if successful.
    virtual bool LoadXML(const XMLElement& source, bool setInstanceDefault = false) override;
    /// Load from JSON data. Return true if successful.
    virtual bool LoadJSON(const JSONValue& source, bool setInstanceDefault = false) override;
    /// Save as binary data. Return true if successful.
    virtual bool Save(Serializer& dest) const override;
    /// Save as XML data. Return true if successful.
    virtual bool SaveXML(XMLElement& dest) const override;
    /// Save as JSON data. Return true if successful.
    virtual bool SaveJSON(JSONValue& dest) const override;

    /// Initialize the type name. Called by Node when loading.
    void SetTypeName(const QString& typeName);
   /// Initialize the type hash only when type name not known. Called by Node when loading.
    void SetType(StringHash typeHash);

    /// Return the XML format attributes. Empty when loaded with binary serialization.
    const QStringList& GetXMLAttributes() const { return xmlAttributes_; }
    /// Return the binary attributes. Empty when loaded with XML serialization.
    const std::vector<unsigned char>& GetBinaryAttributes() const { return binaryAttributes_; }
    /// Return whether was loaded using XML data.
    bool GetUseXML() const { return useXML_; }

    /// Return static type.
    static StringHash GetTypeStatic() {
        static const StringHash typeStatic("UnknownComponent");
        return typeStatic;
    }
    /// Return static type name.
    static const QString& GetTypeNameStatic() {
        static const QString typeNameStatic("UnknownComponent");
        return typeNameStatic;
    }

private:
    /// Type of stored component.
    StringHash typeHash_;
    /// Type name of the stored component.
    QString typeName_;
    /// XML format attribute infos.
    std::vector<AttributeInfo> xmlAttributeInfos_;
    /// XML format attribute data (as strings)
    QStringList xmlAttributes_;
    /// Binary attributes.
    std::vector<unsigned char> binaryAttributes_;
    /// Flag of whether was loaded using XML/JSON data.
    bool useXML_;
};

} // namespace Urho3D
