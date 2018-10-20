//
// Copyright (c) 2008-2018 the Urho3D project.
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

#include "Lutefisk3D/Core/Context.h"
#include "Lutefisk3D/Engine/PluginApplication.h"
#include <cr/cr.h>


namespace Urho3D
{

PluginApplication::~PluginApplication()
{
    for (const auto& pair : registeredTypes_)
    {
        if (pair.second != nullptr)
            context_->RemoveFactory(pair.first, pair.second);
        else
            context_->RemoveFactory(pair.first);
        context_->RemoveAllAttributes(pair.first);
        context_->RemoveSubsystem(pair.first);
    }
}

int PluginMain(void* ctx_, size_t operation, PluginApplication*(*factory)(Context*),
    void(*destroyer)(PluginApplication*))
{
    assert(ctx_);
    auto* ctx = static_cast<cr_plugin*>(ctx_);

    switch (operation)
    {
    case CR_LOAD:
    {
        auto* context = static_cast<Context*>(ctx->userdata);
        auto* application = factory(context);
        application->Start();
        ctx->userdata = application;
        return 0;
    }
    case CR_UNLOAD:
    case CR_CLOSE:
    {
        auto* application = static_cast<PluginApplication*>(ctx->userdata);
        application->Stop();
        ctx->userdata = application->GetContext();
        destroyer(application);
        return 0;
    }
    case CR_STEP:
    {
        return 0;
    }
    default:
		break;
    }
	assert(false);
	return -3;
}

}
