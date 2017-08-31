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

#include "Lutefisk3D/Container/HashMap.h"
#include "Lutefisk3D/Core/Variant.h"

#include <QString>


namespace Urho3D
{

class Context;

/// JSON value type.
enum JSONValueType
{
    /// JSON null type.
    JSON_NULL = 0,
    /// JSON boolean type.
    JSON_BOOL,
    /// JSON number type.
    JSON_NUMBER,
    /// JSON string type.
    JSON_STRING,
    /// JSON array type.
    JSON_ARRAY,
    /// JSON object type.
    JSON_OBJECT
};
/// JSON number type.
enum JSONNumberType
{
    /// Not a number.
    JSONNT_NAN = 0,
    /// Integer.
    JSONNT_INT,
    /// Unsigned integer.
    JSONNT_UINT,
    /// Float or double.
    JSONNT_FLOAT_DOUBLE
};

class JSONValue;

/// JSON array type.
typedef std::vector<JSONValue> JSONArray;
/// JSON object type.
typedef HashMap<QString, JSONValue> JSONObject;

/// JSON value class.
class LUTEFISK3D_EXPORT JSONValue
{
public:
    /// Construct null value.
    JSONValue() :
        type_(0)
    {
    }
    /// Construct with a boolean.
    JSONValue(bool value) :
        type_(0)
    {
        *this = value;
    }
    /// Construct with a integer.
    JSONValue(int value) :
        type_(0)
    {
        *this = value;
    }
    /// Construct with a unsigned integer.
    JSONValue(unsigned value) :
        type_(0)
    {
        *this = value;
    }
    /// Construct with a float.
    JSONValue(float value) :
        type_(0)
    {
        *this = value;
    }
    /// Construct with a double.
    JSONValue(double value) :
        type_(0)
    {
        *this = value;
    }
    /// Construct with a string.
    JSONValue(const QString& value) :
        type_(0)
    {
        *this = value;
    }
    /// Construct with a C string.
    JSONValue(const char* value) :
        type_(0)
    {
        *this = value;
    }
    /// Construct with a JSON array.
    JSONValue(const JSONArray& value) :
        type_(0)
    {
        *this = value;
    }
    /// Construct with a JSON object.
    JSONValue(const JSONObject& value) :
        type_(0)
    {
        *this = value;
    }
    /// Copy-construct from another JSON value.
    JSONValue(const JSONValue& value) :
        type_(0)
    {
        *this = value;
    }
    /// Destruct.
    ~JSONValue()
    {
        SetType(JSON_NULL);
    }

    /// Assign from a boolean.
    JSONValue& operator =(bool rhs);
    /// Assign from an integer.
    JSONValue& operator =(int rhs);
    /// Assign from an unsigned integer.
    JSONValue& operator =(unsigned rhs);
    /// Assign from a float.
    JSONValue& operator =(float rhs);
    /// Assign from a double.
    JSONValue& operator =(double rhs);
    /// Assign from a string.
    JSONValue& operator =(const QString& rhs);
    /// Assign from a C string.
    JSONValue& operator =(const char* rhs);
    /// Assign from a JSON array.
    JSONValue& operator =(const JSONArray& rhs);
    /// Assign from a JSON object.
    JSONValue& operator =(const JSONObject& rhs);
    /// Assign from another JSON value.
    JSONValue& operator = (const JSONValue& rhs);

    /// Return value type.
    JSONValueType GetValueType() const;
    /// Return number type.
    JSONNumberType GetNumberType() const;
    /// Return value type's name.
    QString GetValueTypeName() const;
    /// Return number type's name.
    QString GetNumberTypeName() const;
    /// Check is null.
    bool IsNull() const { return GetValueType() == JSON_NULL; }
    /// Check is boolean.
    bool IsBool() const { return GetValueType() == JSON_BOOL; }
    /// Check is number.
    bool IsNumber() const { return GetValueType() == JSON_NUMBER; }
    /// Check is string.
    bool IsString() const { return GetValueType() == JSON_STRING; }
    /// Check is array.
    bool IsArray() const { return GetValueType() == JSON_ARRAY; }
    /// Check is object.
    bool IsObject() const { return GetValueType() == JSON_OBJECT; }

    /// Return boolean value.
    bool GetBool() const { return IsBool() ? boolValue_ : false;}
    /// Return integer value.
    int GetInt() const { return IsNumber() ? (int)numberValue_ : 0; }
    /// Return unsigned integer value.
    unsigned GetUInt() const { return IsNumber() ? (unsigned)numberValue_ : 0; }
    /// Return float value.
    float GetFloat() const { return IsNumber() ? (float)numberValue_ : 0.0f; }
    /// Return double value.
    double GetDouble() const { return IsNumber() ? numberValue_ : 0.0; }
    /// Return string value.
    const QString& GetString() const { return IsString() ? *stringValue_ : s_dummy;}
    /// Return JSON array value.
    const JSONArray& GetArray() const { return IsArray() ? *arrayValue_ : emptyArray; }
    /// Return JSON object value.
    const JSONObject& GetObject() const { return IsObject() ? *objectValue_ : emptyObject; }

    // JSON array functions
    /// Return JSON value at index.
    JSONValue& operator [](unsigned index);
    /// Return JSON value at index.
    const JSONValue& operator [](unsigned index) const;
    /// Add JSON value at end.
    void Push(const JSONValue& value);
    /// Remove the last JSON value.
    void Pop();
    /// Insert an JSON value at position.
    void Insert(unsigned pos, const JSONValue& value);
    /// Erase a range of JSON values.
    void Erase(unsigned pos, unsigned length = 1);
    /// Resize array.
    void Resize(unsigned newSize);
    /// Return size of array or number of keys in object.
    unsigned Size() const;

    // JSON object functions
    /// Return JSON value with key.
    JSONValue& operator [](const QString& key);
    /// Return JSON value with key.
    const JSONValue& operator [](const QString& key) const;
    /// Set JSON value with key.
    void Set(const QString& key, const JSONValue& value);
    /// Return JSON value with key.
    const JSONValue& Get(const QString& key) const;
    /// Erase a pair by key.
    bool Erase(const QString& key);
    /// Return whether contains a pair with key.
    bool Contains(const QString& key) const;
    /// Clear array or object.
    void Clear();

    /// Set value type and number type, internal function.
    void SetType(JSONValueType valueType, JSONNumberType numberType = JSONNT_NAN);

    /// Set variant, context must provide for resource ref.
    void SetVariant(const Variant& variant, Context* context = 0);
    /// Return a variant.
    Variant GetVariant() const;
    /// Set variant value, context must provide for resource ref.
    void SetVariantValue(const Variant& variant, Context* context = 0);
    /// Return a variant with type.
    Variant GetVariantValue(VariantType type) const;
    /// Set variant map, context must provide for resource ref.
    void SetVariantMap(const VariantMap& variantMap, Context* context = 0);
    /// Return a variant map.
    VariantMap GetVariantMap() const;
    /// Set variant vector, context must provide for resource ref.
    void SetVariantVector(const VariantVector& variantVector, Context* context = 0);
    /// Return a variant vector.
    VariantVector GetVariantVector() const;

    /// Empty JSON value.
    static const JSONValue EMPTY;
    /// Empty JSON array.
    static const JSONArray emptyArray;
    /// Empty JSON object.
    static const JSONObject emptyObject;
    /// Return name corresponding to a value type.
    static QString GetValueTypeName(JSONValueType type);
    /// Return name corresponding to a number type.
    static QString GetNumberTypeName(JSONNumberType type);
    /// Return a value type from name; null if unrecognized.
    static JSONValueType GetValueTypeFromName(const QString& typeName);
    /// Return a value type from name; null if unrecognized.
    static JSONValueType GetValueTypeFromName(const char* typeName);
    /// Return a number type from name; NaN if unrecognized.
    static JSONNumberType GetNumberTypeFromName(const QString& typeName);
    /// Return a value type from name; NaN if unrecognized.
    static JSONNumberType GetNumberTypeFromName(const char* typeName);

private:
    /// type.
    unsigned type_;
    union
    {
        /// Boolean value.
        bool boolValue_;
        /// Number value.
        double numberValue_;
        /// String value.
        QString* stringValue_;
        /// Array value.
        JSONArray* arrayValue_;
        /// Object value.
        JSONObject* objectValue_;
    };
};

}
