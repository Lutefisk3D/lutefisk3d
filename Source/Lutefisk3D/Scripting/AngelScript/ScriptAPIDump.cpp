//
// Copyright (c) 2008-2015 the Urho3D project.
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

#include "../Core/Context.h"
#include "../IO/File.h"
#include "../IO/FileSystem.h"
#include "../IO/Log.h"
#include "../Script/Script.h"
#include "../Container/Pair.h"

#include <AngelScript/angelscript.h>

#include "../DebugNew.h"

namespace Urho3D
{

/// %Object property info for scripting API dump.
struct PropertyInfo
{
    /// Construct.
    PropertyInfo() :
        read_(false),
        write_(false),
        indexed_(false)
    {
    }

    /// Property name.
    QString name_;
    /// Property data type.
    QString type_;
    /// Reading supported flag.
    bool read_;
    /// Writing supported flag.
    bool write_;
    /// Indexed flag.
    bool indexed_;
};

/// Header information for dumping events.
struct HeaderFile
{
    /// Full path to header file.
    QString fileName;
    /// Event section name.
    QString sectionName;
};

bool CompareHeaderFiles(const HeaderFile& lhs, const HeaderFile& rhs)
{
    return lhs.sectionName < rhs.sectionName;
}
void ExtractPropertyInfo(const QString& functionName, const QString& declaration, std::vector<PropertyInfo>& propertyInfos)
{
    QString propertyName = functionName.mid(4);
    PropertyInfo* info = nullptr;
    for (unsigned k = 0; k < propertyInfos.size(); ++k)
    {
        if (propertyInfos[k].name_ == propertyName)
        {
            info = &propertyInfos[k];
            break;
        }
    }
    if (!info)
    {
        propertyInfos.resize(propertyInfos.size() + 1);
        info = &propertyInfos.back();
        info->name_ = propertyName;
    }
    if (functionName.contains("get_"))
    {
        info->read_ = true;
        // Extract type from the return value
        QStringList parts = declaration.split(' ');
        if (parts.size())
        {
            if (parts[0] != "const")
                info->type_ = parts[0];
            else if (parts.size() > 1)
                info->type_ = parts[1];
        }
        // If get method has parameters, it is indexed
        if (!declaration.contains("()"))
        {
            info->indexed_ = true;
            info->type_ += "[]";
        }

        // Sanitate the reference operator away
        info->type_.replace("&", "");
    }
    if (functionName.contains("set_"))
    {
        info->write_ = true;
        if (info->type_.isEmpty())
        {
            // Extract type from parameters
            unsigned begin = declaration.indexOf(',');
            if (begin == -1)
                begin = declaration.indexOf('(');
            else
                info->indexed_ = true;

            if (begin != -1)
            {
                ++begin;
                unsigned end = declaration.indexOf(')');
                if (end != -1)
                {
                    info->type_ = declaration.mid(begin, end - begin);
                    // Sanitate const & reference operator away
                    info->type_.replace("const ", "");
                    info->type_.replace("&in", "");
                    info->type_.replace("&", "");
                }
            }
        }
    }
}

bool ComparePropertyStrings(const QString& lhs, const QString& rhs)
{
    int spaceLhs = lhs.indexOf(' ');
    int spaceRhs = rhs.indexOf(' ');
    if (spaceLhs != -1 && spaceRhs != -1)
        return lhs.midRef(spaceLhs).compare(rhs.midRef(spaceRhs)) < 0;
    else
        return lhs.compare(rhs) < 0;
}

bool ComparePropertyInfos(const PropertyInfo& lhs, const PropertyInfo& rhs)
{
    return lhs.name_.compare(rhs.name_) < 0;
}

void Script::OutputAPIRow(DumpMode mode, const QString& row, bool removeReference, QString separator)
{
    QString out(row);
    ///\todo We need C++11 <regex> in String class to handle REGEX whole-word replacement correctly. Can't do that since we still support VS2008.
    // Commenting out to temporary fix property name like 'doubleClickInterval' from being wrongly replaced.
    // Fortunately, there is no occurence of type 'double' in the API at the moment.
    //out.Replace("double", "float");   // s/\bdouble\b/float/g
    out.replace("&in", "&");
    out.replace("&out", "&");
    if (removeReference)
        out.replace("&", "");

    if (mode == DOXYGEN)
        Log::WriteRaw("- " + out + "\n");
    else if (mode == C_HEADER)
    {
        out.replace("@", "");
        out.replace("?&", "void*");

        // s/(\w+)\[\]/Array<\1>/g
        unsigned posBegin = -1;
        while (1)   // Loop to cater for array of array of T
        {
            unsigned posEnd = out.indexOf("[]");
            if (posEnd == -1)
                break;
            if (posBegin > posEnd)
                posBegin = posEnd - 1;
            while (posBegin < posEnd && out[posBegin].isLetterOrNumber())
                --posBegin;
            ++posBegin;
            out.replace(posBegin, posEnd - posBegin + 2, "Array<" + out.mid(posBegin, posEnd - posBegin) + ">");
        }

        Log::WriteRaw(out + separator + "\n");
    }
}

void Script::DumpAPI(DumpMode mode, const QString& sourceTree)
{
    // Does not use LOGRAW macro here to ensure the messages are always dumped regardless of URHO3D_LOGGING compiler directive
    // and of Log subsystem availability

    // Dump event descriptions and attribute definitions in Doxygen mode. For events, this means going through the header files,
    // as the information is not available otherwise.
    /// \todo Dump events + attributes before the actual script API because the remarks (readonly / writeonly) seem to throw off
    // Doxygen parsing and the following page definition(s) may not be properly recognized
    if (mode == DOXYGEN)
    {
        Log::WriteRaw("namespace Urho3D\n{\n\n/**\n");

        FileSystem* fileSystem = GetSubsystem<FileSystem>();
        QStringList headerFileNames;
        QString path = AddTrailingSlash(sourceTree);
        if (!path.isEmpty())
            path.append("Source/Urho3D/");

        fileSystem->ScanDir(headerFileNames, path, "*.h", SCAN_FILES, true);
        /// \hack Rename any Events2D to 2DEvents to work with the event category creation correctly (currently PhysicsEvents2D)
        std::vector<HeaderFile> headerFiles;
        for (unsigned i = 0; i < headerFileNames.size(); ++i)
        {
            HeaderFile entry;
            entry.fileName = headerFileNames[i];
            entry.sectionName = GetFileNameAndExtension(entry.fileName).replace("Events2D", "2DEvents");
            if (entry.sectionName.endsWith("Events.h"))
                headerFiles.push_back(entry);
        }
        if (!headerFiles.empty())
        {
            Log::WriteRaw("\n\\page EventList Event list\n");
            std::sort(headerFiles.begin(), headerFiles.end(),CompareHeaderFiles);

            for (unsigned i = 0; i < headerFiles.size(); ++i)
                {
                    SharedPtr<File> file(new File(context_, path + headerFiles[i].fileName, FILE_READ));
                    if (!file->IsOpen())
                        continue;

                const QString& sectionName = headerFiles[i].sectionName;
                    unsigned start = sectionName.indexOf('/') + 1;
                    unsigned end = sectionName.indexOf("Events.h");
                    Log::WriteRaw("\n## %" + sectionName.mid(start, end - start) + " events\n");

                    while (!file->IsEof())
                    {
                        QString line = file->ReadLine();
                        if (line.startsWith("EVENT"))
                        {
                            QStringList parts = line.split(',');
                            if (parts.size() == 2)
                                Log::WriteRaw("\n### " + parts[1].mid(0, parts[1].length() - 1).trimmed() + "\n");
                        }
                        if (line.contains("PARAM"))
                        {
                            QStringList parts = line.split(',');
                            if (parts.size() == 2)
                            {
                                QString paramName = parts[1].mid(0, parts[1].indexOf(')')).trimmed();
                                QString paramType = parts[1].mid(parts[1].indexOf("// ") + 3);
                                if (!paramName.isEmpty() && !paramType.isEmpty())
                                    Log::WriteRaw("- %" + paramName + " : " + paramType + "\n");
                        }
                    }
                }
            }

            Log::WriteRaw("\n");
        }

        Log::WriteRaw("\n\\page AttributeList Attribute list\n");

        const HashMap<StringHash, std::vector<AttributeInfo> >& attributes(context_->GetAllAttributes());

        QStringList objectTypes;
#ifdef USE_QT_HASHMAP
        for (const auto & attribute : attributes.keys())
            objectTypes.push_back(context_->GetTypeName(attribute));
#else
        for (const auto & attribute : attributes)
            objectTypes.push_back(context_->GetTypeName(attribute.first));
#endif
        std::sort(objectTypes.begin(), objectTypes.end());

        for (const QString &obType : objectTypes)
        {
            const std::vector<AttributeInfo>& attrs(MAP_VALUE(attributes.find(obType)));
            unsigned usableAttrs = 0;
            for (unsigned j = 0; j < attrs.size(); ++j)
            {
                // Attributes that are not shown in the editor are typically internal and not usable for eg. attribute
                // animation
                if (attrs[j].mode_ & AM_NOEDIT)
                    continue;
                ++usableAttrs;
            }

            if (!usableAttrs)
                continue;

            Log::WriteRaw("\n### " + obType + "\n");

            for (unsigned j = 0; j < attrs.size(); ++j)
            {
                if (attrs[j].mode_ & AM_NOEDIT)
                    continue;
                // Prepend each word in the attribute name with % to prevent unintended links
                QStringList nameParts = attrs[j].name_.split(' ');
                for (unsigned k = 0; k < nameParts.size(); ++k)
                {
                    if (nameParts[k].length() > 1 && nameParts[k][0].isLetter())
                        nameParts[k] = "%" + nameParts[k];
                }
                QString name;
                name = nameParts.join(" ");
                QString type = Variant::GetTypeName(attrs[j].type_);
                // Variant typenames are all uppercase. Convert primitive types to the proper lowercase form for the documentation
                if (type == "Int" || type == "Bool" || type == "Float")
                    type[0] = type[0].toLower();

                Log::WriteRaw("- " + name + " : " + type + "\n");
            }
        }

        Log::WriteRaw("\n");
    }

    if (mode == DOXYGEN)
        Log::WriteRaw("\n\\page ScriptAPI Scripting API\n\n");
    else if (mode == C_HEADER)
        Log::WriteRaw("// Script API header intended to be 'force included' in IDE for AngelScript content assist / code completion\n\n"
            "#define int8 signed char\n"
            "#define int16 signed short\n"
            "#define int64 long\n"
            "#define uint8 unsigned char\n"
            "#define uint16 unsigned short\n"
            "#define uint64 unsigned long\n"
            "#define null 0\n");

    unsigned types = scriptEngine_->GetObjectTypeCount();
    std::vector<Pair<QString, unsigned> > sortedTypes;
    for (unsigned i = 0; i < types; ++i)
    {
        asIObjectType* type = scriptEngine_->GetObjectTypeByIndex(i);
        if (type)
        {
            QString typeName(type->GetName());
            sortedTypes.push_back(MakePair(typeName, i));
        }
    }
    std::sort(sortedTypes.begin(), sortedTypes.end());

    if (mode == DOXYGEN)
    {
        Log::WriteRaw("\\section ScriptAPI_TableOfContents Table of contents\n"
            "\\ref ScriptAPI_ClassList \"Class list\"<br>\n"
            "\\ref ScriptAPI_Classes \"Classes\"<br>\n"
            "\\ref ScriptAPI_Enums \"Enumerations\"<br>\n"
            "\\ref ScriptAPI_GlobalFunctions \"Global functions\"<br>\n"
            "\\ref ScriptAPI_GlobalProperties \"Global properties\"<br>\n"
            "\\ref ScriptAPI_GlobalConstants \"Global constants\"<br>\n\n");

        Log::WriteRaw("\\section ScriptAPI_ClassList Class list\n\n");

        for (unsigned i = 0; i < sortedTypes.size(); ++i)
        {
            asIObjectType* type = scriptEngine_->GetObjectTypeByIndex(sortedTypes[i].second_);
            if (type)
            {
                QString typeName(type->GetName());
                Log::WriteRaw("<a href=\"#Class_" + typeName + "\"><b>" + typeName + "</b></a>\n");
            }
        }

        Log::WriteRaw("\n\\section ScriptAPI_Classes Classes\n");
    }
    else if (mode == C_HEADER)
        Log::WriteRaw("\n// Classes\n");

    for (unsigned i = 0; i < sortedTypes.size(); ++i)
    {
        asIObjectType* type = scriptEngine_->GetObjectTypeByIndex(sortedTypes[i].second_);
        if (type)
        {
            QString typeName(type->GetName());
            QStringList methodDeclarations;
            std::vector<PropertyInfo> propertyInfos;

            if (mode == DOXYGEN)
            {
                Log::WriteRaw("<a name=\"Class_" + typeName + "\"></a>\n");
                Log::WriteRaw("\n### " + typeName + "\n");
            }
            else if (mode == C_HEADER)
            {
                ///\todo Find a cleaner way to do this instead of hardcoding
                if (typeName == "Array")
                    Log::WriteRaw("\ntemplate <class T> class " + typeName + "\n{\n");
                else
                    Log::WriteRaw("\nclass " + typeName + "\n{\n");
            }

            unsigned methods = type->GetMethodCount();
            for (unsigned j = 0; j < methods; ++j)
            {
                asIScriptFunction* method = type->GetMethodByIndex(j);
                QString methodName(method->GetName());
                QString declaration(method->GetDeclaration());
                // Recreate tab escape sequences
                declaration.replace("\t", "\\t");
                if (methodName.contains("get_") || methodName.contains("set_"))
                    ExtractPropertyInfo(methodName, declaration, propertyInfos);
                else
                {
                    // Sanitate the method name. \todo For now, skip the operators
                    if (!declaration.contains("::op"))
                    {
                        QString prefix(typeName + "::");
                        declaration.replace(prefix, "");
                        ///\todo Is there a better way to mark deprecated API bindings for AngelScript?
                        unsigned posBegin = declaration.lastIndexOf("const String&in = \"deprecated:");
                        if (posBegin != -1)
                        {
                            // Assume this 'mark' is added as the last parameter
                            unsigned posEnd = declaration.indexOf(')', posBegin);
                            if (posBegin != -1)
                            {
                                declaration.replace(posBegin, posEnd - posBegin, "");
                                posBegin = declaration.indexOf(", ", posBegin - 2);
                                if (posBegin != -1)
                                    declaration.replace(posBegin, 2, "");
                                if (mode == DOXYGEN)
                                    declaration += " // deprecated";
                                else if (mode == C_HEADER)
                                    declaration = "/* deprecated */\n" + declaration;
                            }
                        }
                        methodDeclarations.push_back(declaration);
                    }
                }
            }

            // Assume that the same property is never both an accessor property, and a direct one
            unsigned properties = type->GetPropertyCount();
            for (unsigned j = 0; j < properties; ++j)
            {
                const char* propertyName;
                const char* propertyDeclaration;
                int typeId;

                type->GetProperty(j, &propertyName, &typeId);
                propertyDeclaration = scriptEngine_->GetTypeDeclaration(typeId);

                PropertyInfo newInfo;
                newInfo.name_ = QString(propertyName);
                newInfo.type_ = QString(propertyDeclaration);
                newInfo.read_ = newInfo.write_ = true;
                propertyInfos.push_back(newInfo);
            }

            std::sort(methodDeclarations.begin(), methodDeclarations.end(), ComparePropertyStrings);
            std::sort(propertyInfos.begin(), propertyInfos.end(), ComparePropertyInfos);

            if (!methodDeclarations.empty())
            {
                if (mode == DOXYGEN)
                    Log::WriteRaw("\nMethods:\n\n");
                else if (mode == C_HEADER)
                    Log::WriteRaw("// Methods:\n");
                for (unsigned j = 0; j < methodDeclarations.size(); ++j)
                    OutputAPIRow(mode, methodDeclarations[j]);
            }

            if (!propertyInfos.empty())
            {
                if (mode == DOXYGEN)
                    Log::WriteRaw("\nProperties:\n\n");
                else if (mode == C_HEADER)
                    Log::WriteRaw("\n// Properties:\n");
                for (unsigned j = 0; j < propertyInfos.size(); ++j)
                {
                    QString remark;
                    QString cppdoc;
                    if (!propertyInfos[j].write_)
                        remark = "readonly";
                    else if (!propertyInfos[j].read_)
                        remark = "writeonly";
                    if (!remark.isEmpty())
                    {
                        if (mode == DOXYGEN)
                        {
                            remark = " // " + remark;
                        }
                        else if (mode == C_HEADER)
                        {
                            cppdoc = "/* " + remark + " */\n";
                            remark.clear();
                        }
                    }

                    OutputAPIRow(mode, cppdoc + propertyInfos[j].type_ + " " + propertyInfos[j].name_ + remark);
                }
            }

            if (mode == DOXYGEN)
                Log::WriteRaw("\n");
            else if (mode == C_HEADER)
                Log::WriteRaw("};\n");
        }
    }

    std::vector<PropertyInfo> globalPropertyInfos;
    QStringList globalFunctions;

    unsigned functions = scriptEngine_->GetGlobalFunctionCount();
    for (unsigned i = 0; i < functions; ++i)
    {
        asIScriptFunction* function = scriptEngine_->GetGlobalFunctionByIndex(i);
        QString functionName(function->GetName());
        QString declaration(function->GetDeclaration());
        // Recreate tab escape sequences
        declaration.replace("\t", "\\t");

        if (functionName.contains("set_") || functionName.contains("get_"))
            ExtractPropertyInfo(functionName, declaration, globalPropertyInfos);
        else
            globalFunctions.push_back(declaration);
    }

    std::sort(globalFunctions.begin(), globalFunctions.end(), ComparePropertyStrings);
    std::sort(globalPropertyInfos.begin(), globalPropertyInfos.end(), ComparePropertyInfos);

    if (mode == DOXYGEN)
        Log::WriteRaw("\\section ScriptAPI_Enums Enumerations\n");
    else if (mode == C_HEADER)
        Log::WriteRaw("\n// Enumerations\n");

    unsigned enums = scriptEngine_->GetEnumCount();
    std::vector<Pair<QString, unsigned> > sortedEnums;
    for (unsigned i = 0; i < enums; ++i)
    {
        int typeId;
        sortedEnums.push_back(MakePair(QString(scriptEngine_->GetEnumByIndex(i, &typeId)), i));
    }
    std::sort(sortedEnums.begin(), sortedEnums.end());

    for (unsigned i = 0; i < sortedEnums.size(); ++i)
    {
        int typeId = 0;
        if (mode == DOXYGEN)
            Log::WriteRaw("\n### " + QString(scriptEngine_->GetEnumByIndex(sortedEnums[i].second_, &typeId)) + "\n\n");
        else if (mode == C_HEADER)
            Log::WriteRaw("\nenum " + QString(scriptEngine_->GetEnumByIndex(sortedEnums[i].second_, &typeId)) + "\n{\n");

        for (unsigned j = 0; j < (unsigned)scriptEngine_->GetEnumValueCount(typeId); ++j)
        {
            int value = 0;
            const char* name = scriptEngine_->GetEnumValueByIndex(typeId, j, &value);
            OutputAPIRow(mode, QString(name), false, ",");
        }

        if (mode == DOXYGEN)
            Log::WriteRaw("\n");
        else if (mode == C_HEADER)
            Log::WriteRaw("};\n");
    }

    if (mode == DOXYGEN)
        Log::WriteRaw("\\section ScriptAPI_GlobalFunctions Global functions\n");
    else if (mode == C_HEADER)
        Log::WriteRaw("\n// Global functions\n");

    for (unsigned i = 0; i < globalFunctions.size(); ++i)
        OutputAPIRow(mode, globalFunctions[i]);

    if (mode == DOXYGEN)
        Log::WriteRaw("\\section ScriptAPI_GlobalProperties Global properties\n");
    else if (mode == C_HEADER)
        Log::WriteRaw("\n// Global properties\n");

    for (unsigned i = 0; i < globalPropertyInfos.size(); ++i)
        OutputAPIRow(mode, globalPropertyInfos[i].type_ + " " + globalPropertyInfos[i].name_, true);

    if (mode == DOXYGEN)
        Log::WriteRaw("\\section ScriptAPI_GlobalConstants Global constants\n");
    else if (mode == C_HEADER)
        Log::WriteRaw("\n// Global constants\n");

    QStringList globalConstants;
    unsigned properties = scriptEngine_->GetGlobalPropertyCount();
    for (unsigned i = 0; i < properties; ++i)
    {
        const char* propertyName;
        const char* propertyDeclaration;
        int typeId;
        scriptEngine_->GetGlobalPropertyByIndex(i, &propertyName, nullptr, &typeId);
        propertyDeclaration = scriptEngine_->GetTypeDeclaration(typeId);

        QString type(propertyDeclaration);
        globalConstants.push_back(type + " " + QString(propertyName));
    }

    std::sort(globalConstants.begin(), globalConstants.end(), ComparePropertyStrings);

    for (unsigned i = 0; i < globalConstants.size(); ++i)
        OutputAPIRow(mode, globalConstants[i], true);

    if (mode == DOXYGEN)
        Log::WriteRaw("*/\n\n}\n");
}

}
