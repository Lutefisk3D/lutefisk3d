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
#include "Engine.h"
#include "EngineDefs.h"
#include "EngineEvents.h"

#include "Lutefisk3D/Audio/Audio.h"
#include "Lutefisk3D/UI/Console.h"
#include "Lutefisk3D/Container/Allocator.h"
#include "Lutefisk3D/Core/Context.h"
#include "Lutefisk3D/Core/Variant.h"
#include "Lutefisk3D/Core/CoreEvents.h"
#include "Lutefisk3D/UI/DebugHud.h"
#include "Lutefisk3D/IO/Log.h"
#include "Lutefisk3D/IO/IOEvents.h"
#include "Lutefisk3D/IO/FileSystem.h"
#include "Lutefisk3D/Graphics/Graphics.h"
#include "Lutefisk3D/Graphics/GraphicsEvents.h"
#include "Lutefisk3D/Graphics/Renderer.h"
#include "Lutefisk3D/Core/StringUtils.h"
#ifdef LUTEFISK3D_PROFILING
#include "Lutefisk3D/Core/EventProfiler.h"
#endif
#ifdef LUTEFISK3D_INPUT
#include "Lutefisk3D/Input/Input.h"
#include "Lutefisk3D/Input/InputEvents.h"
#endif
#include "Lutefisk3D/IO/Log.h"
#include "Lutefisk3D/IO/PackageFile.h"
#ifdef LUTEFISK3D_NAVIGATION
#include "Lutefisk3D/Navigation/NavigationMesh.h"
#endif
#ifdef LUTEFISK3D_NETWORK
#include "Lutefisk3D/Network/Network.h"
#endif
#ifdef LUTEFISK3D_PHYSICS
#include "Lutefisk3D/Physics/PhysicsWorld.h"
#endif
#include "Lutefisk3D/Core/ProcessUtils.h"
#include "Lutefisk3D/Core/Profiler.h"
#include "Lutefisk3D/Graphics/Renderer.h"
#include "Lutefisk3D/Resource/ResourceCache.h"
#include "Lutefisk3D/Scene/Scene.h"
#include "Lutefisk3D/Scene/SceneEvents.h"
#ifdef LUTEFISK3D_UI
#include "Lutefisk3D/UI/UI.h"
#endif
#ifdef LUTEFISK3D_2D
#include "Lutefisk3D/2D/Urho2D.h"
#endif
#include "Lutefisk3D/Core/WorkQueue.h"
#include "Lutefisk3D/Resource/XMLFile.h"
#include "jlsignal/StaticSignalConnectionAllocators.h"


#if defined(_MSC_VER) && defined(_DEBUG)
// From dbgint.h
#define nNoMansLandSize 4

typedef struct _CrtMemBlockHeader
{
    struct _CrtMemBlockHeader* pBlockHeaderNext;
    struct _CrtMemBlockHeader* pBlockHeaderPrev;
    char* szFileName;
    int nLine;
    size_t nDataSize;
    int nBlockUse;
    long lRequest;
    unsigned char gap[nNoMansLandSize];
} _CrtMemBlockHeader;
#endif
namespace {
template <int T>
struct AllocatorWrapper : public jl::ScopedAllocator
{
    Urho3D::AllocatorBlock* allocator_;
public:
    AllocatorWrapper() {
        allocator_ = Urho3D::AllocatorInitialize(T,1024);
    }
    void *Alloc(size_t nBytes) override
    {
        assert(nBytes==T);
        return static_cast<void *>(Urho3D::AllocatorReserve(allocator_));
    }
    void Free(void *pObject) override
    {
        Urho3D::AllocatorFree(allocator_,pObject);

    }
};
struct SignalAllocator : public AllocatorWrapper<jl::Signal<>::eAllocationSize>
{
};
struct ObserverAllocator : public AllocatorWrapper<jl::SignalObserver::eAllocationSize>
{
};

enum { eMaxConnections = 32000 };
SignalAllocator oSignalConnectionAllocator;
ObserverAllocator oObserverConnectionAllocator;
/// Get an engine startup parameter, with default value if missing.
const Urho3D::Variant& GetParameter(const Urho3D::VariantMap& parameters, const QString& parameter, const Urho3D::Variant& defaultValue = Urho3D::Variant::EMPTY)
{
    Urho3D::StringHash nameHash(parameter);
    Urho3D::VariantMap::const_iterator i = parameters.find(nameHash);
    return i != parameters.end() ? MAP_VALUE(i) : defaultValue;
}

} // end of anonymous namespace
namespace Urho3D
{

extern const char* logLevelPrefixes[];

Engine::Engine(Context* context) :
    Object(context),
    SignalObserver(nullptr),
    timeStep_(0.0f),
    timeStepSmoothing_(2),
    minFps_(10),
    maxFps_(200),
    maxInactiveFps_(60),
    pauseMinimized_(false),
    #ifdef LUTEFISK3D_TESTING
    timeOut_(0),
    #endif
    autoExit_(true),
    initialized_(false),
    exiting_(false),
    headless_(false),
    audioPaused_(false)
{

    // Initialize the signal system with allocators
    context->m_signal_allocator = &oSignalConnectionAllocator;
    context->m_observer_allocator = &oObserverConnectionAllocator;
    jl::SignalBase::SetCommonConnectionAllocator( &oSignalConnectionAllocator );
    SetConnectionAllocator(&oObserverConnectionAllocator);
    g_coreSignals.init(&oSignalConnectionAllocator);
    g_consoleSignals.init(&oSignalConnectionAllocator);
    g_graphicsSignals.init(&oSignalConnectionAllocator);
    g_resourceSignals.init(&oSignalConnectionAllocator);
    g_sceneSignals.init(&oSignalConnectionAllocator);
    g_uiSignals.init(&oSignalConnectionAllocator);
    g_inputSignals.init(&oSignalConnectionAllocator);
    g_ioSignals.init(&oSignalConnectionAllocator);
    g_LogSignals.init(&oSignalConnectionAllocator);

    // Register self as a subsystem
    context_->RegisterSubsystem(StringHash("Engine"),this);

    // Create subsystems which do not depend on engine initialization or startup parameters
    context_->m_TimeSystem.reset(new Time(context_));
    context_->m_WorkQueueSystem.reset(new WorkQueue(context_));
#ifdef LUTEFISK3D_PROFILING
    context_->m_ProfilerSystem.reset(new Profiler(context_));
#endif
    context_->m_FileSystem.reset(new FileSystem(context_));
#ifdef LUTEFISK3D_LOGGING
    context_->m_LogSystem.reset(new Log(context_));
#endif
    context_->m_ResourceCache.reset(new ResourceCache(context_));
#ifdef LUTEFISK3D_NETWORK
    context_->RegisterSubsystem(StringHash("Network"),new Network(context_));
#endif
#ifdef LUTEFISK3D_INPUT
    context_->m_InputSystem.reset(new Input(context_));
#endif
    context_->RegisterSubsystem(StringHash("Audio"),new Audio(context_));
#ifdef LUTEFISK3D_UI
    context_->m_UISystem.reset(new UI(context_));
#endif

    // Register object factories for libraries which are not automatically registered along with subsystem creation
    RegisterSceneLibrary(context_);

#ifdef LUTEFISK3D_PHYSICS
    RegisterPhysicsLibrary(context_);
#endif

#ifdef LUTEFISK3D_NAVIGATION
    RegisterNavigationLibrary(context_);
#endif

#ifdef LUTEFISK3D_INPUT
    g_inputSignals.exitRequested.Connect(this,&Engine::HandleExitRequested);
#endif
}

/// Initialize engine using parameters given and show the application window. Return true if successful.
bool Engine::Initialize(const VariantMap& parameters)
{
    if (initialized_)
        return true;

    URHO3D_PROFILE_CTX(context_,InitEngine);

    // Set headless mode
    headless_ = GetParameter(parameters, EP_HEADLESS, false).GetBool();

    // Register the rest of the subsystems
    if (!headless_)
    {
        context_->m_Graphics.reset(new Graphics(context_));
        context_->m_Renderer.reset(new Renderer(context_));
    }
    else
    {
        // Register graphics library objects explicitly in headless mode to allow them to work without using actual GPU resources
        RegisterGraphicsLibrary(context_);
    }

#ifdef LUTEFISK3D_2D
    // 2D graphics library is dependent on 3D graphics library
    RegisterUrho2DLibrary(context_);
#endif

    // Start logging
    Log* log = context_->m_LogSystem.get();
    if (log)
    {
        if (HasParameter(parameters, EP_LOG_LEVEL))
            log->SetLoggingLevel(GetParameter(parameters, EP_LOG_LEVEL).GetInt());
        log->SetQuiet(GetParameter(parameters, EP_LOG_QUIET, false).GetBool());
        log->SetTargetFilename(GetParameter(parameters, EP_LOG_NAME, "Urho3D.log").GetString());
    }

    // Set maximally accurate low res timer
    context_->m_TimeSystem->SetTimerPeriod(1);

    // Configure max FPS
    if (GetParameter(parameters, EP_FRAME_LIMITER, true) == false)
        SetMaxFps(0);

    // Set amount of worker threads according to the available physical CPU cores. Using also hyperthreaded cores results in
    // unpredictable extra synchronization overhead. Also reserve one core for the main thread
    unsigned numThreads = GetParameter(parameters, EP_WORKER_THREADS, true).GetBool() ? GetNumPhysicalCPUs() - 1 : 0;
    if (numThreads)
    {
        context_->m_WorkQueueSystem->CreateThreads(numThreads);

        URHO3D_LOGINFO(QString("Created %1 worker thread%2").arg(numThreads).arg(numThreads > 1 ? "s" : ""));
    }

    // Add resource paths
    if (!InitializeResourceCache(parameters, false))
        return false;

    ResourceCache* cache = context_->m_ResourceCache.get();
    FileSystem* fileSystem = context_->m_FileSystem.get();
    // Initialize graphics & audio output
    if (!headless_)
    {
        Graphics* graphics = context_->m_Graphics.get();
        Renderer* renderer = context_->m_Renderer.get();

        if (HasParameter(parameters, EP_EMBEDDED_WINDOW))
            graphics->SetEmbeddedWindow();
        graphics->SetWindowTitle(GetParameter(parameters, EP_WINDOW_TITLE, "Urho3D").GetString());
        graphics->SetWindowIcon(cache->GetResource<Image>(GetParameter(parameters, EP_WINDOW_ICON, QString()).GetString()));
        graphics->SetFlushGPU(GetParameter(parameters, EP_FLUSH_GPU, false).GetBool());

        if (HasParameter(parameters, EP_WINDOW_POSITION_X) && HasParameter(parameters, EP_WINDOW_POSITION_Y))
            graphics->SetWindowPosition(GetParameter(parameters, EP_WINDOW_POSITION_X).GetInt(), GetParameter(parameters, EP_WINDOW_POSITION_Y).GetInt());

        if (!graphics->SetMode(
            GetParameter(parameters, EP_WINDOW_WIDTH, 0).GetInt(),
            GetParameter(parameters, EP_WINDOW_HEIGHT, 0).GetInt(),
            GetParameter(parameters, EP_FULL_SCREEN, true).GetBool(),
            GetParameter(parameters, EP_BORDERLESS, false).GetBool(),
            GetParameter(parameters, EP_WINDOW_RESIZABLE, false).GetBool(),
            GetParameter(parameters, EP_HIGH_DPI, true).GetBool(),
            GetParameter(parameters, EP_VSYNC, false).GetBool(),
            GetParameter(parameters, EP_TRIPLE_BUFFER, false).GetBool(),
            GetParameter(parameters, EP_MULTI_SAMPLE, 1).GetInt(),
            GetParameter(parameters, EP_MONITOR, 0).GetInt(),
            GetParameter(parameters, EP_REFRESH_RATE, 0).GetInt()
                    ))
            return false;

        graphics->SetShaderCacheDir(GetParameter(parameters, EP_SHADER_CACHE_DIR, fileSystem->GetAppPreferencesDir("urho3d", "shadercache")).GetString());

        if (HasParameter(parameters, EP_DUMP_SHADERS))
            graphics->BeginDumpShaders(GetParameter(parameters, EP_DUMP_SHADERS, QString()).GetString());
        if (HasParameter(parameters, EP_RENDER_PATH))
            renderer->SetDefaultRenderPath(cache->GetResource<XMLFile>(GetParameter(parameters, EP_RENDER_PATH).GetString()));

        renderer->SetDrawShadows(GetParameter(parameters, EP_SHADOWS, true).GetBool());
        if (renderer->GetDrawShadows() && GetParameter(parameters, EP_LOW_QUALITY_SHADOWS, false).GetBool())
            renderer->SetShadowQuality(SHADOWQUALITY_SIMPLE_16BIT);
        renderer->SetMaterialQuality(eQuality(GetParameter(parameters, EP_MATERIAL_QUALITY, QUALITY_HIGH).GetInt()));
        renderer->SetTextureQuality(eQuality(GetParameter(parameters, EP_TEXTURE_QUALITY, QUALITY_HIGH).GetInt()));
        renderer->SetTextureFilterMode((TextureFilterMode)GetParameter(parameters, EP_TEXTURE_FILTER_MODE, FILTER_TRILINEAR).GetInt());
        renderer->SetTextureAnisotropy(GetParameter(parameters, EP_TEXTURE_ANISOTROPY, 4).GetInt());

        if (GetParameter(parameters, EP_SOUND, true).GetBool())
        {
            GetSubsystem<Audio>()->SetMode(
                GetParameter(parameters, EP_SOUND_BUFFER, 100).GetInt(),
                GetParameter(parameters, EP_SOUND_MIX_RATE, 0).GetInt());
        }
    }

    // Init FPU state of main thread
    InitFPU();

    // Initialize network
#ifdef LUTEFISK3D_NETWORK
    if (HasParameter(parameters, EP_PACKAGE_CACHE_DIR))
        GetSubsystem<Network>()->SetPackageCacheDir(GetParameter(parameters, EP_PACKAGE_CACHE_DIR).GetString());
#endif
#ifdef LUTEFISK3D_TESTING
    if (HasParameter(parameters, EP_TIME_OUT))
        timeOut_ = GetParameter(parameters, EP_TIME_OUT, 0).GetInt() * 1000000LL;
#endif

#ifdef LUTEFISK3D_PROFILING
    if (GetParameter(parameters, EP_EVENT_PROFILER, true).GetBool())
    {
        context_->m_EventProfilerSystem.reset(new EventProfiler(context_));
        EventProfiler::SetActive(true);
    }
#endif
    frameTimer_.Reset();

    URHO3D_LOGINFO("Initialized engine");
    initialized_ = true;
    return true;
}
/// Reinitialize resource cache subsystem using parameters given. Implicitly called by Initialize.
/// \returns true if successful.
bool Engine::InitializeResourceCache(const VariantMap &parameters, bool removeOld /*= true*/)
{
    ResourceCache* cache = context_->m_ResourceCache.get();
    FileSystem* fileSystem = context_->m_FileSystem.get();

    // Remove all resource paths and packages
    if (removeOld)
    {
        const QStringList &resourceDirs(cache->GetResourceDirs());
        const std::vector<SharedPtr<PackageFile> > &packageFiles(cache->GetPackageFiles());
        for (unsigned i = 0; i < resourceDirs.size(); ++i)
            cache->RemoveResourceDir(resourceDirs[i]);
        for (unsigned i = 0; i < packageFiles.size(); ++i)
            cache->RemovePackageFile(packageFiles[i]);
    }

    // Add resource paths
    QStringList resourcePrefixPaths = GetParameter(parameters, EP_RESOURCE_PREFIX_PATHS).GetString().split(';',QString::KeepEmptyParts);
    for (QString &path : resourcePrefixPaths)
        path = AddTrailingSlash( IsAbsolutePath(path) ? path : fileSystem->GetProgramDir() + path);

    QStringList resourcePaths = GetParameter(parameters, EP_RESOURCE_PATHS, "Data;CoreData").GetString().split(';',QString::SkipEmptyParts);
    QStringList resourcePackages = GetParameter(parameters, EP_RESOURCE_PACKAGES).GetString().split(';',QString::SkipEmptyParts);
    QStringList autoLoadPaths = GetParameter(parameters, EP_AUTOLOAD_PATHS, "Autoload").GetString().split(';',QString::SkipEmptyParts);

    for (QString & resourcePath : resourcePaths)
    {

        // If path is not absolute, prefer to add it as a package if possible
        if (!IsAbsolutePath(resourcePath))
        {
            unsigned j = 0;
            for (; j < resourcePrefixPaths.size(); ++j)
            {
                QString packageName = resourcePrefixPaths[j] + resourcePath + ".pak";
                if (fileSystem->FileExists(packageName))
                {
                    if (!cache->AddPackageFile(packageName))
                        return false;   // The root cause of the error should have already been logged
                    break;
                }
                QString pathName = resourcePrefixPaths[j] + resourcePath;
                if (fileSystem->DirExists(pathName))
                {
                    if (!cache->AddResourceDir(pathName))
                        return false;
                    break;
                }
            }
            if (j == resourcePrefixPaths.size())
            {
                URHO3D_LOGERROR(QString("Failed to add resource path '%1', "
                                        "check the documentation on how to set the 'resource prefix path'").arg(resourcePath));
                return false;
            }

        }
        else
        {
            if (fileSystem->DirExists(resourcePath))
                if (!cache->AddResourceDir(resourcePath))
                    return false;
        }
    }

    // Then add specified packages
    for (const QString & resourcePackage : resourcePackages)
    {
        int j=0;
        for(QString &prefix : resourcePrefixPaths) {
            ++j;
            QString packageName = prefix + resourcePackage;
            if (fileSystem->FileExists(packageName))
            {
                if (!cache->AddPackageFile(packageName))
                    return false;
                break;
            }
        }
        if (j == resourcePrefixPaths.size())
        {
            URHO3D_LOGERROR(
                        QString("Failed to add resource package '%1', check the documentation on how to set "
                                "the 'resource prefix path'").arg(resourcePackage));
            return false;
        }
    }

    // Add auto load folders. Prioritize these (if exist) before the default folders
    for (QString & autoLoadPaths_i : autoLoadPaths)
    {
        bool autoLoadPathExist = false;
        for(QString &prefix : resourcePrefixPaths) {
            QString autoLoadPath(autoLoadPaths_i);
            if (!IsAbsolutePath(autoLoadPath))
                autoLoadPath = prefix + autoLoadPath;

            if (fileSystem->DirExists(autoLoadPath))
            {
                autoLoadPathExist = true;
                // Add all the subdirs (non-recursive) as resource directory
                QStringList subdirs;
                fileSystem->ScanDir(subdirs, autoLoadPath, "*", SCAN_DIRS, false);
                for (QString & subdir : subdirs)
                {
                    QString dir = subdir;
                    if (dir.startsWith("."))
                        continue;

                    QString autoResourceDir = autoLoadPath + "/" + dir;
                    if (!cache->AddResourceDir(autoResourceDir, 0))
                        return false;
                }

                // Add all the found package files (non-recursive)
                QStringList paks;
                fileSystem->ScanDir(paks, autoLoadPath, "*.pak", SCAN_FILES, false);
                for (QString & paks_y : paks)
                {
                    QString pak = paks_y;
                    if (pak.startsWith("."))
                        continue;

                    QString autoPackageName = autoLoadPath + "/" + pak;
                    if (!cache->AddPackageFile(autoPackageName, 0))
                        return false;
                }
            }
        }
        // The following debug message is confusing when user is not aware of the autoload feature
        // Especially because the autoload feature is enabled by default without user intervention
        // The following extra conditional check below is to suppress unnecessary debug log entry under such default situation
        // The cleaner approach is to not enable the autoload by default, i.e. do not use 'Autoload' as default value for 'AutoloadPaths' engine parameter
        // However, doing so will break the existing applications that rely on this
        if (!autoLoadPathExist && (autoLoadPaths.size() > 1 || autoLoadPaths[0] != "Autoload"))
            URHO3D_LOGDEBUG(QString("Skipped autoload path '%1' as it does not exist, check the documentation on how to set the 'resource prefix path'")
                            .arg(autoLoadPaths_i));
    }

    return true;
}
/// Run one frame.
void Engine::RunFrame()
{
    assert(initialized_);

    // If not headless, and the graphics subsystem no longer has a window open, assume we should exit
    if (!headless_ && !context_->m_Graphics->IsInitialized())
        exiting_ = true;

    if (exiting_)
        return;

    // Note: there is a minimal performance cost to looking up subsystems (uses a hashmap); if they would be looked up several
    // times per frame it would be better to cache the pointers
    Time* time = context_->m_TimeSystem.get();
    bool isMinimized = false;
#ifdef LUTEFISK3D_INPUT
    Input* input = context_->m_InputSystem.get();
    isMinimized = input->IsMinimized();
#endif
    Audio* audio = GetSubsystem<Audio>();

#ifdef LUTEFISK3D_PROFILING
    if (EventProfiler::IsActive())
    {
        if (context_->m_EventProfilerSystem)
            context_->m_EventProfilerSystem->BeginFrame();
    }
#endif

    time->BeginFrame(timeStep_);

    // If pause when minimized -mode is in use, stop updates and audio as necessary
    if (pauseMinimized_ && isMinimized)
    {
        if (audio->IsPlaying())
        {
            audio->Stop();
            audioPaused_ = true;
        }
    }
    else
    {
        // Only unpause when it was paused by the engine
        if (audioPaused_)
        {
            audio->Play();
            audioPaused_ = false;
        }

        Update();
    }

    Render();
    ApplyFrameLimit();

    time->EndFrame();
}
/// Create the console and return it.
/// May return null if engine configuration does not allow creation (headless mode.)
Console* Engine::CreateConsole()
{
    if (headless_ || !initialized_)
        return nullptr;

    // Return existing console if possible
    Console* console = GetSubsystem<Console>();
    if (!console)
    {
        console = new Console(context_);
        context_->RegisterSubsystem(StringHash("Console"),console);
    }

    return console;
}
/// Create the debug hud.
DebugHud* Engine::CreateDebugHud()
{
    if (headless_ || !initialized_)
        return nullptr;

    // Return existing debug HUD if possible
    DebugHud* debugHud = GetSubsystem<DebugHud>();
    if (!debugHud)
    {
        debugHud = new DebugHud(context_);
        context_->RegisterSubsystem(StringHash("DebugHud"),debugHud);
    }

    return debugHud;
}
/// Set how many frames to average for timestep smoothing. Default is 2. 1 disables smoothing.
void Engine::SetTimeStepSmoothing(int frames)
{
    timeStepSmoothing_ = (unsigned)Clamp(frames, 1, 20);
}
/// Set minimum frames per second. If FPS goes lower than this, time will appear to slow down.
void Engine::SetMinFps(int fps)
{
    minFps_ = (unsigned)std::max(fps, 0);
}
/// Set maximum frames per second. The engine will sleep if FPS is higher than this.
void Engine::SetMaxFps(unsigned fps)
{
    maxFps_ = fps;
}
/// Set maximum frames per second when the application does not have input focus.
void Engine::SetMaxInactiveFps(unsigned fps)
{
    maxInactiveFps_ = fps;
}

void Engine::Exit()
{
    DoExit();
}

void Engine::DumpProfiler()
{
#ifdef LUTEFISK3D_LOGGING
    if (!Thread::IsMainThread())
        return;

    if (context_->m_ProfilerSystem)
        URHO3D_LOGRAW(context_->m_ProfilerSystem->PrintData(true, true) + "\n");
#endif
}

void Engine::DumpResources(bool dumpFileName)
{
#ifdef LUTEFISK3D_LOGGING
    if (!Thread::IsMainThread())
        return;

    ResourceCache* cache = context_->m_ResourceCache.get();
    const HashMap<StringHash, ResourceGroup>& resourceGroups = cache->GetAllResources();
    if (dumpFileName)
    {
        URHO3D_LOGRAW("Used resources:\n");
        for (const auto & entry : resourceGroups)
        {
            const ResourceGroup & resourceGroup(ELEMENT_VALUE(entry));
            const HashMap<StringHash, SharedPtr<Resource> >& resources = resourceGroup.resources_;
            if (dumpFileName) {
                for (auto j : resources)
                    URHO3D_LOGRAW(ELEMENT_VALUE(j)->GetName() + "\n");
            }
        }
    }
    else
        URHO3D_LOGRAW(cache->PrintMemoryUsage()+ "\n");
#endif
}

void Engine::DumpMemory()
{
#ifdef LUTEFISK3D_LOGGING
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtMemState state;
    _CrtMemCheckpoint(&state);
    _CrtMemBlockHeader* block = state.pBlockHeader;
    unsigned total = 0;
    unsigned blocks = 0;

    for (;;)
    {
        if (block && block->pBlockHeaderNext)
            block = block->pBlockHeaderNext;
        else
            break;
    }

    while (block)
    {
        if (block->nBlockUse > 0)
        {
            if (block->szFileName)
                URHO3D_LOGRAW("Block " + QString::number((int)block->lRequest) + ": " + QString::number(block->nDataSize) + " bytes, file " + block->szFileName + " line " + QString::number(block->nLine) + "\n");
            else
                URHO3D_LOGRAW("Block " + QString::number((int)block->lRequest) + ": " + QString::number(block->nDataSize) + " bytes\n");

            total += block->nDataSize;
            ++blocks;
        }
        block = block->pBlockHeaderPrev;
    }

    URHO3D_LOGRAW("Total allocated memory " + QString::number(total) + " bytes in " + QString::number(blocks) + " blocks\n\n");
#else
    URHO3D_LOGRAW("DumpMemory() supported on MSVC debug mode only\n\n");
#endif
#endif
}
/// Send frame update events.
void Engine::Update()
{
    URHO3D_PROFILE_CTX(context_,Update);
    if (!Thread::IsMainThread())
    {
        URHO3D_LOGERROR("Sending events is only supported from the main thread");
        return;
    }
    g_coreSignals.update(timeStep_);

    // Logic post-update event
    g_coreSignals.postUpdate(timeStep_);

    // Rendering update event
    g_coreSignals.renderUpdate(timeStep_);

    // Post-render update event
    g_coreSignals.postRenderUpdate(timeStep_);
}
/// Render after frame update.
void Engine::Render()
{
    if (headless_)
        return;

    URHO3D_PROFILE_CTX(context_,Render);

    // If device is lost, BeginFrame will fail and we skip rendering
    Graphics* graphics = context_->m_Graphics.get();
    if (!graphics->BeginFrame())
        return;

    context_->m_Renderer->Render();
#ifdef LUTEFISK3D_UI
    context_->m_UISystem->Render();
#endif
    graphics->EndFrame();
}
/// Get the timestep for the next frame and sleep for frame limiting if necessary.
void Engine::ApplyFrameLimit()
{
    if (!initialized_)
        return;

    unsigned maxFps = maxFps_;
#ifdef LUTEFISK3D_INPUT
    Input* input = context_->m_InputSystem.get();
    if (input && !input->HasFocus())
        maxFps = std::min(maxInactiveFps_, maxFps);
#endif

    long long elapsed = 0;

    // Perform waiting loop if maximum FPS set
    if (maxFps)
    {
        URHO3D_PROFILE_CTX(context_,ApplyFrameLimit);

        long long targetMax = 1000000LL / maxFps;

        for (;;)
        {
            elapsed = frameTimer_.GetUSecS();
            if (elapsed >= targetMax)
                break;

            // Sleep if 1 ms or more off the frame limiting goal
            if (targetMax - elapsed >= 1000LL)
            {
                unsigned sleepTime = (unsigned)((targetMax - elapsed) / 1000LL);
                Time::Sleep(sleepTime);
            }
        }
    }

    elapsed = frameTimer_.GetUSec(true);
#ifdef LUTEFISK3D_TESTING
    if (timeOut_ > 0)
    {
        timeOut_ -= elapsed;
        if (timeOut_ <= 0)
            Exit();
    }
#endif

    // If FPS lower than minimum, clamp elapsed time
    if (minFps_)
    {
        long long targetMin = 1000000LL / minFps_;
        if (elapsed > targetMin)
            elapsed = targetMin;
    }

    // Perform timestep smoothing
    timeStep_ = 0.0f;
    lastTimeSteps_.push_back(elapsed / 1000000.0f);
    if (lastTimeSteps_.size() > timeStepSmoothing_)
    {
        // If the smoothing configuration was changed, ensure correct amount of samples
        lastTimeSteps_.erase(lastTimeSteps_.begin(), lastTimeSteps_.begin() + lastTimeSteps_.size() - timeStepSmoothing_);
        for (float elem : lastTimeSteps_)
            timeStep_ += elem;
        timeStep_ /= lastTimeSteps_.size();
    }
    else
        timeStep_ = lastTimeSteps_.back();
}

VariantMap Engine::ParseParameters(const QStringList& arguments)
{
    VariantMap ret;

    // Pre-initialize the parameters with environment variable values when they are set
    if (const char* paths = getenv("URHO3D_PREFIX_PATH"))
        ret[EP_RESOURCE_PREFIX_PATHS] = paths;

    for (unsigned i = 0; i < arguments.size(); ++i)
    {
        if (arguments[i].length() > 1 && arguments[i][0] == '-')
        {
            QString argument = arguments[i].mid(1).toLower();
            QString value = i + 1 < arguments.size() ? arguments[i + 1] : QString::null;

            if (argument == "headless")
                ret[EP_HEADLESS] = true;
            else if (argument == "nolimit")
                ret[EP_FRAME_LIMITER] = false;
            else if (argument == "flushgpu")
                ret[EP_FLUSH_GPU] = true;
            else if (argument == "nosound")
                ret[EP_SOUND] = false;
            else if (argument == "prepass")
                ret[EP_RENDER_PATH] = "RenderPaths/Prepass.xml";
            else if (argument == "deferred")
                ret[EP_RENDER_PATH] = "RenderPaths/Deferred.xml";
            else if (argument == "renderpath" && !value.isEmpty())
            {
                ret[EP_RENDER_PATH] = value;
                ++i;
            }
            else if (argument == "noshadows")
                ret[EP_SHADOWS] = false;
            else if (argument == "lqshadows")
                ret[EP_LOW_QUALITY_SHADOWS] = true;
            else if (argument == "nothreads")
                ret[EP_WORKER_THREADS] = false;
            else if (argument == "v")
                ret[EP_VSYNC] = true;
            else if (argument == "t")
                ret[EP_TRIPLE_BUFFER] = true;
            else if (argument == "w")
                ret[EP_FULL_SCREEN] = false;
            else if (argument == "borderless")
                ret[EP_BORDERLESS] = true;
            else if (argument == "s")
                ret[EP_WINDOW_RESIZABLE] = true;
            else if (argument == "q")
                ret[EP_LOG_QUIET] = true;
            else if (argument == "log" && !value.isEmpty())
            {
                unsigned logLevel = logLevelNameToIndex(value);
                if (logLevel != M_MAX_UNSIGNED)
                {
                    ret[EP_LOG_LEVEL] = logLevel;
                    ++i;
                }
            }
            else if (argument == "x" && !value.isEmpty())
            {
                ret[EP_WINDOW_WIDTH] = value.toInt();
                ++i;
            }
            else if (argument == "y" && !value.isEmpty())
            {
                ret[EP_WINDOW_HEIGHT] = value.toInt();
                ++i;
            }
            else if (argument == "monitor" && !value.isEmpty()) {
                ret[EP_MONITOR] = value.toInt();
                ++i;
            }
            else if (argument == "hz" && !value.isEmpty()) {
                ret[EP_REFRESH_RATE] = value.toInt();
                ++i;
            }
            else if (argument == "m" && !value.isEmpty())
            {
                ret[EP_MULTI_SAMPLE] = value.toInt();
                ++i;
            }
            else if (argument == "b" && !value.isEmpty())
            {
                ret[EP_SOUND_BUFFER] = value.toInt();
                ++i;
            }
            else if (argument == "r" && !value.isEmpty())
            {
                ret[EP_SOUND_MIX_RATE] = value.toInt();
                ++i;
            }
            else if (argument == "pp" && !value.isEmpty())
            {
                ret[EP_RESOURCE_PREFIX_PATHS] = value;
                ++i;
            }
            else if (argument == "p" && !value.isEmpty())
            {
                ret[EP_RESOURCE_PATHS] = value;
                ++i;
            }
            else if (argument == "pf" && !value.isEmpty())
            {
                ret[EP_RESOURCE_PACKAGES] = value;
                ++i;
            }
            else if (argument == "ap" && !value.isEmpty())
            {
                ret[EP_AUTOLOAD_PATHS] = value;
                ++i;
            }
            else if (argument == "ds" && !value.isEmpty())
            {
                ret[EP_DUMP_SHADERS] = value;
                ++i;
            }
            else if (argument == "mq" && !value.isEmpty())
            {
                ret[EP_MATERIAL_QUALITY] = value.toInt();
                ++i;
            }
            else if (argument == "tq" && !value.isEmpty())
            {
                ret[EP_TEXTURE_QUALITY] = value.toInt();
                ++i;
            }
            else if (argument == "tf" && !value.isEmpty())
            {
                ret[EP_TEXTURE_FILTER_MODE] = value.toInt();
                ++i;
            }
            else if (argument == "af" && !value.isEmpty())
            {
                ret[EP_TEXTURE_FILTER_MODE] = FILTER_ANISOTROPIC;
                ret[EP_TEXTURE_ANISOTROPY] = value.toInt();
                ++i;
            }
#ifdef LUTEFISK3D_TESTING
            else if (argument == "timeout" && !value.isEmpty())
            {
                ret[EP_TIME_OUT] = value.toInt();
                ++i;
            }
#endif
        }
    }

    return ret;
}

bool Engine::HasParameter(const VariantMap& parameters, const QString& parameter)
{
    StringHash nameHash(parameter);
    return parameters.find(nameHash) != parameters.end();
}

/// Handle exit requested event. Auto-exit if enabled.
void Engine::HandleExitRequested()
{
    if (autoExit_)
    {
        // Do not call Exit() here, as it contains mobile platform -specific tests to not exit.
        // If we do receive an exit request from the system on those platforms, we must comply
        DoExit();
    }
}
/// Actually perform the exit actions.
void Engine::DoExit()
{
    if (context_->m_Graphics)
        context_->m_Graphics->Close();
    exiting_ = true;
}

}
