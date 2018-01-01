#include <Lutefisk3D/Core/Mutex.h>
#include <Lutefisk3D/Core/Condition.h>
#include <Lutefisk3D/Core/Thread.h>
#include <Lutefisk3D/Core/Context.h>

#include <QtGui/QGuiApplication>
#include <QtCore/QDebug>
#include <chrono>
#include <thread>
#include <condition_variable>
#include <mutex>

using namespace std::chrono;
using namespace Urho3D;
struct QtAppThread
{
private:
    std::unique_ptr<std::thread> m_thread;
    std::condition_variable cv;
    std::mutex mutex_p;
    bool shouldRun=true;
    bool gui_thread_running=false;
    // Thread interface
    void markAsRunning()
    {
        std::unique_lock<std::mutex> lk(mutex_p);
        gui_thread_running = true;
    }
    void ThreadFunction(int argc,char **argv)
    {
        QGuiApplication app(argc,argv);
        //FakePlatform *platform = FakePlatform::get(); // ensure that fake platform lives on Qt thread
        {
            markAsRunning();
            cv.notify_all();
        }
        while(shouldRun)
            app.processEvents();
    }
public:
    void WaitUntilQUIIsReady()
    {
        assert(m_thread && std::this_thread::get_id()!=m_thread->get_id());
        {
            std::unique_lock<std::mutex> lk(mutex_p);
            // ensure Gui thread is initialized
            while (!gui_thread_running) {
                cv.wait(lk);
            }
        }
    }
    void CreateQUIThread(int argc,char **argv)
    {
        shouldRun = true;
        m_thread.reset(new std::thread(&QtAppThread::ThreadFunction,this,argc,argv));
    }
    void Stop() {
        shouldRun = false;
        if(m_thread)
            m_thread->join();
    }
};
struct QUI {
    void RegisterObject(Context* context)
    {
    }

};
void RegisterQUISystem(Urho3D::Context* context)
{
}
int main(int argc,char **argv)
{
    auto start_clock=steady_clock::now();
    QtAppThread app_thread;
    app_thread.CreateQUIThread(argc,argv);
    app_thread.WaitUntilQUIIsReady();
    qDebug() << "Waited" << duration_cast<milliseconds>(steady_clock::now()-start_clock).count() << "ms";
    app_thread.Stop();
}
