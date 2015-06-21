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

#include "../Script/Addons.h"
#include "../Engine/EngineEvents.h"
#include "../IO/Log.h"
#include "../Core/Profiler.h"
#include "../Scene/Scene.h"
#include "../Script/Script.h"
#include "../Script/ScriptAPI.h"
#include "../Script/ScriptFile.h"
#include "../Script/ScriptInstance.h"

#include <AngelScript/angelscript.h>

#include "../DebugNew.h"

namespace Urho3D
{

Script::Script(Context* context) :
    Object(context),
    scriptEngine_(nullptr),
    immediateContext_(nullptr),
    scriptNestingLevel_(0),
    executeConsoleCommands_(false)
{
    scriptEngine_ = asCreateScriptEngine(ANGELSCRIPT_VERSION);
    if (!scriptEngine_)
    {
        LOGERROR("Could not create AngelScript engine");
        return;
    }

    scriptEngine_->SetUserData(this);
    scriptEngine_->SetEngineProperty(asEP_USE_CHARACTER_LITERALS, true);
    scriptEngine_->SetEngineProperty(asEP_ALLOW_UNSAFE_REFERENCES, true);
    scriptEngine_->SetEngineProperty(asEP_ALLOW_IMPLICIT_HANDLE_TYPES, true);
    scriptEngine_->SetEngineProperty(asEP_BUILD_WITHOUT_LINE_CUES, true);
    //scriptEngine_->SetEngineProperty(asEP_DISALLOW_VALUE_ASSIGN_FOR_REF_TYPE,true);
    scriptEngine_->SetMessageCallback(asMETHOD(Script, MessageCallback), this, asCALL_THISCALL);

    // Create the context for immediate execution
    immediateContext_ = scriptEngine_->CreateContext();
    immediateContext_->SetExceptionCallback(asMETHOD(Script, ExceptionCallback), this, asCALL_THISCALL);

    // Register Script library object factories
    RegisterScriptLibrary(context_);

    // Register the Array, String & Dictionary API
    RegisterArray(scriptEngine_);
    RegisterString(scriptEngine_);
    RegisterDictionary(scriptEngine_);
    RegisterScriptInterfaceAPI(scriptEngine_);

    // Register the rest of the script API
    RegisterMathAPI(scriptEngine_);
    RegisterCoreAPI(scriptEngine_);
    RegisterIOAPI(scriptEngine_);
    RegisterResourceAPI(scriptEngine_);
    RegisterSceneAPI(scriptEngine_);
    RegisterGraphicsAPI(scriptEngine_);
    RegisterInputAPI(scriptEngine_);
    RegisterAudioAPI(scriptEngine_);
    RegisterUIAPI(scriptEngine_);
#ifdef URHO3D_NETWORK
    RegisterNetworkAPI(scriptEngine_);
#endif
#ifdef URHO3D_PHYSICS
    RegisterPhysicsAPI(scriptEngine_);
#endif
#ifdef URHO3D_NAVIGATION
    RegisterNavigationAPI(scriptEngine_);
#endif
#ifdef URHO3D_URHO2D
    RegisterUrho2DAPI(scriptEngine_);
#endif
    RegisterScriptAPI(scriptEngine_);
    RegisterEngineAPI(scriptEngine_);

    // Subscribe to console commands
    SetExecuteConsoleCommands(true);
}

Script::~Script()
{
    if (immediateContext_)
    {
        immediateContext_->Release();
        immediateContext_ = nullptr;
    }

    for (unsigned i = 0 ; i < scriptFileContexts_.size(); ++i)
        scriptFileContexts_[i]->Release();

    if (scriptEngine_)
    {
        scriptEngine_->Release();
        scriptEngine_ = nullptr;
    }
}

bool Script::Execute(const QString& line)
{
    // Note: compiling code each time is slow. Not to be used for performance-critical or repeating activity
    PROFILE(ExecuteImmediate);

    ClearObjectTypeCache();

    QString wrappedLine = "void f(){\n" + line + ";\n}";

    // If no immediate mode script file set, create a dummy module for compiling the line
    asIScriptModule* module = nullptr;
    if (defaultScriptFile_)
        module = defaultScriptFile_->GetScriptModule();
    if (!module)
        module = scriptEngine_->GetModule("ExecuteImmediate", asGM_CREATE_IF_NOT_EXISTS);
    if (!module)
        return false;

    asIScriptFunction *function = nullptr;
    if (module->CompileFunction("", qPrintable(wrappedLine), -1, 0, &function) < 0)
        return false;

    if (immediateContext_->Prepare(function) < 0)
    {
        function->Release();
        return false;
    }

    bool success = immediateContext_->Execute() >= 0;
    immediateContext_->Unprepare();
    function->Release();

    return success;
}

void Script::SetDefaultScriptFile(ScriptFile* file)
{
    defaultScriptFile_ = file;
}

void Script::SetDefaultScene(Scene* scene)
{
    defaultScene_ = scene;
}

void Script::SetExecuteConsoleCommands(bool enable)
{
    if (enable == executeConsoleCommands_)
        return;

    executeConsoleCommands_ = enable;
    if (enable)
        SubscribeToEvent(E_CONSOLECOMMAND, HANDLER(Script, HandleConsoleCommand));
    else
        UnsubscribeFromEvent(E_CONSOLECOMMAND);
}

void Script::MessageCallback(const asSMessageInfo* msg)
{
    QString message = QString("%1:%2,%3 %4").arg(msg->section).arg(msg->row).arg(msg->col).arg(msg->message);

    switch (msg->type)
    {
    case asMSGTYPE_ERROR:
        LOGERROR(message);
        break;

    case asMSGTYPE_WARNING:
        LOGWARNING(message);
        break;

    default:
        LOGINFO(message);
        break;
    }
}

void Script::ExceptionCallback(asIScriptContext* context)
{
    QString message = QString("- Exception '%1' in '%2'\n%3")
            .arg(context->GetExceptionString())
            .arg(context->GetExceptionFunction()->GetDeclaration())
            .arg(GetCallStack(context))
            ;
    QByteArray message_data= message.toLocal8Bit();
    asSMessageInfo msg;
    msg.row = context->GetExceptionLineNumber(&msg.col, &msg.section);
    msg.type = asMSGTYPE_ERROR;
    msg.message = message_data.data();

    MessageCallback(&msg);
}

QString Script::GetCallStack(asIScriptContext* context)
{
    QString str("AngelScript callstack:\n");

    // Append the call stack
    for (asUINT i = 0; i < context->GetCallstackSize(); i++)
    {
        asIScriptFunction* func;
        const char* scriptSection;
        int line, column;
        func = context->GetFunction(i);
        line = context->GetLineNumber(i, &column, &scriptSection);
        str += QString("\t%1:%2:%3,%4\n").arg(scriptSection).arg(func->GetDeclaration()).arg(line).arg(column);
    }

    return str;
}

ScriptFile* Script::GetDefaultScriptFile() const
{
    return defaultScriptFile_;
}

Scene* Script::GetDefaultScene() const
{
    return defaultScene_;
}

void Script::ClearObjectTypeCache()
{
    objectTypes_.clear();
}

asIObjectType* Script::GetObjectType(const char* declaration)
{
    HashMap<const char*, asIObjectType*>::const_iterator i = objectTypes_.find(declaration);
    if (i != objectTypes_.end())
        return MAP_VALUE(i);

    asIObjectType* type = scriptEngine_->GetObjectTypeById(scriptEngine_->GetTypeIdByDecl(declaration));
    objectTypes_[declaration] = type;
    return type;
}

asIScriptContext* Script::GetScriptFileContext()
{
    while (scriptNestingLevel_ >= scriptFileContexts_.size())
    {
        asIScriptContext* newContext = scriptEngine_->CreateContext();
        newContext->SetExceptionCallback(asMETHOD(Script, ExceptionCallback), this, asCALL_THISCALL);
        scriptFileContexts_.push_back(newContext);
    }

    return scriptFileContexts_[scriptNestingLevel_];
}

void Script::HandleConsoleCommand(StringHash eventType, VariantMap& eventData)
{
    using namespace ConsoleCommand;
    if (eventData[P_ID].GetString() == GetTypeName())
        Execute(eventData[P_COMMAND].GetString());
}

void RegisterScriptLibrary(Context* context)
{
    ScriptFile::RegisterObject(context);
    ScriptInstance::RegisterObject(context);
}

}
