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

#include "Network.h"

#include "../Core/Context.h"
#include "../Core/CoreEvents.h"
#include "../Core/StringUtils.h"
#include "../Engine/EngineEvents.h"
#include "../IO/FileSystem.h"
#include "HttpRequest.h"
#include "../Input/InputEvents.h"
#include "../IO/IOEvents.h"
#include "../IO/Log.h"
#include "../IO/MemoryBuffer.h"
#include "NetworkEvents.h"
#include "NetworkPriority.h"
#include "../Core/Profiler.h"
#include "Protocol.h"
#include "../Scene/Scene.h"

#include <kNet.h>

namespace Urho3D
{

static const int DEFAULT_UPDATE_FPS = 30;

Network::Network(Context* context) :
    Object(context),
    updateFps_(DEFAULT_UPDATE_FPS),
    simulatedLatency_(0),
    simulatedPacketLoss_(0.0f),
    updateInterval_(1.0f / (float)DEFAULT_UPDATE_FPS),
    updateAcc_(0.0f)
{
    network_ = new kNet::Network();

    // Register Network library object factories
    RegisterNetworkLibrary(context_);

    SubscribeToEvent(E_BEGINFRAME, HANDLER(Network, HandleBeginFrame));
    SubscribeToEvent(E_RENDERUPDATE, HANDLER(Network, HandleRenderUpdate));

    // Blacklist remote events which are not to be allowed to be registered in any case
    blacklistedRemoteEvents_.insert(E_CONSOLECOMMAND);
    blacklistedRemoteEvents_.insert(E_LOGMESSAGE);
    blacklistedRemoteEvents_.insert(E_BEGINFRAME);
    blacklistedRemoteEvents_.insert(E_UPDATE);
    blacklistedRemoteEvents_.insert(E_POSTUPDATE);
    blacklistedRemoteEvents_.insert(E_RENDERUPDATE);
    blacklistedRemoteEvents_.insert(E_ENDFRAME);
    blacklistedRemoteEvents_.insert(E_MOUSEBUTTONDOWN);
    blacklistedRemoteEvents_.insert(E_MOUSEBUTTONUP);
    blacklistedRemoteEvents_.insert(E_MOUSEMOVE);
    blacklistedRemoteEvents_.insert(E_MOUSEWHEEL);
    blacklistedRemoteEvents_.insert(E_KEYDOWN);
    blacklistedRemoteEvents_.insert(E_KEYUP);
    blacklistedRemoteEvents_.insert(E_TEXTINPUT);
    blacklistedRemoteEvents_.insert(E_JOYSTICKCONNECTED);
    blacklistedRemoteEvents_.insert(E_JOYSTICKDISCONNECTED);
    blacklistedRemoteEvents_.insert(E_JOYSTICKBUTTONDOWN);
    blacklistedRemoteEvents_.insert(E_JOYSTICKBUTTONUP);
    blacklistedRemoteEvents_.insert(E_JOYSTICKAXISMOVE);
    blacklistedRemoteEvents_.insert(E_JOYSTICKHATMOVE);
    blacklistedRemoteEvents_.insert(E_TOUCHBEGIN);
    blacklistedRemoteEvents_.insert(E_TOUCHEND);
    blacklistedRemoteEvents_.insert(E_TOUCHMOVE);
    blacklistedRemoteEvents_.insert(E_GESTURERECORDED);
    blacklistedRemoteEvents_.insert(E_GESTUREINPUT);
    blacklistedRemoteEvents_.insert(E_MULTIGESTURE);
    blacklistedRemoteEvents_.insert(E_DROPFILE);
    blacklistedRemoteEvents_.insert(E_INPUTFOCUS);
    blacklistedRemoteEvents_.insert(E_MOUSEVISIBLECHANGED);
    blacklistedRemoteEvents_.insert(E_EXITREQUESTED);
    blacklistedRemoteEvents_.insert(E_SERVERCONNECTED);
    blacklistedRemoteEvents_.insert(E_SERVERDISCONNECTED);
    blacklistedRemoteEvents_.insert(E_CONNECTFAILED);
    blacklistedRemoteEvents_.insert(E_CLIENTCONNECTED);
    blacklistedRemoteEvents_.insert(E_CLIENTDISCONNECTED);
    blacklistedRemoteEvents_.insert(E_CLIENTIDENTITY);
    blacklistedRemoteEvents_.insert(E_CLIENTSCENELOADED);
    blacklistedRemoteEvents_.insert(E_NETWORKMESSAGE);
    blacklistedRemoteEvents_.insert(E_NETWORKUPDATE);
    blacklistedRemoteEvents_.insert(E_NETWORKUPDATESENT);
    blacklistedRemoteEvents_.insert(E_NETWORKSCENELOADFAILED);
}

Network::~Network()
{
    // If server connection exists, disconnect, but do not send an event because we are shutting down
    Disconnect(100);
    serverConnection_.Reset();

    clientConnections_.clear();

    delete network_;
    network_ = nullptr;
}

void Network::HandleMessage(kNet::MessageConnection *source, kNet::packet_id_t packetId, kNet::message_id_t msgId, const char *data, size_t numBytes)
{
    // Only process messages from known sources
    Connection* connection = GetConnection(source);
    if (connection)
    {
        MemoryBuffer msg(data, (unsigned)numBytes);
        if (connection->ProcessMessage(msgId, msg))
            return;

        // If message was not handled internally, forward as an event
        using namespace NetworkMessage;

        VariantMap& eventData = GetEventDataMap();
        eventData[P_CONNECTION] = connection;
        eventData[P_MESSAGEID] = (int)msgId;
        eventData[P_DATA].SetBuffer(msg.GetData(), msg.GetSize());
        connection->SendEvent(E_NETWORKMESSAGE, eventData);
    }
    else
        LOGWARNING("Discarding message from unknown MessageConnection " + ToString((void*)source));
}

u32 Network::ComputeContentID(kNet::message_id_t msgId, const char* data, size_t numBytes)
{
    switch (msgId)
    {
    case MSG_CONTROLS:
        // Return fixed content ID for controls
        return CONTROLS_CONTENT_ID;

    case MSG_NODELATESTDATA:
    case MSG_COMPONENTLATESTDATA:
        {
            // Return the node or component ID, which is first in the message
            MemoryBuffer msg(data, (unsigned)numBytes);
            return msg.ReadNetID();
        }

    default:
        // By default return no content ID
        return 0;
    }
}

void Network::NewConnectionEstablished(kNet::MessageConnection* connection)
{
    connection->RegisterInboundMessageHandler(this);

    // Create a new client connection corresponding to this MessageConnection
    SharedPtr<Connection> newConnection(new Connection(context_, true, kNet::SharedPtr<kNet::MessageConnection>(connection)));
    newConnection->ConfigureNetworkSimulator(simulatedLatency_, simulatedPacketLoss_);
    clientConnections_[connection] = newConnection;
    LOGINFO("Client " + newConnection->ToString() + " connected");

    using namespace ClientConnected;

    VariantMap& eventData = GetEventDataMap();
    eventData[P_CONNECTION] = newConnection;
    newConnection->SendEvent(E_CLIENTCONNECTED, eventData);
}

void Network::ClientDisconnected(kNet::MessageConnection* connection)
{
    connection->Disconnect(0);

    // Remove the client connection that corresponds to this MessageConnection
    auto i = clientConnections_.find(connection);
    if (i != clientConnections_.end())
    {
        Connection* connection = MAP_VALUE(i);
        LOGINFO("Client " + connection->ToString() + " disconnected");

        using namespace ClientDisconnected;

        VariantMap& eventData = GetEventDataMap();
        eventData[P_CONNECTION] = connection;
        connection->SendEvent(E_CLIENTDISCONNECTED, eventData);

        clientConnections_.erase(i);
    }
}

bool Network::Connect(const QString& address, unsigned short port, Scene* scene, const VariantMap& identity)
{
    PROFILE(Connect);

    // If a previous connection already exists, disconnect it and wait for some time for the connection to terminate
    if (serverConnection_)
    {
        serverConnection_->Disconnect(100);
        OnServerDisconnected();
    }

    kNet::SharedPtr<kNet::MessageConnection> connection = network_->Connect(qPrintable(address), port, kNet::SocketOverUDP, this);
    if (connection)
    {
        serverConnection_ = new Connection(context_, false, connection);
        serverConnection_->SetScene(scene);
        serverConnection_->SetIdentity(identity);
        serverConnection_->SetConnectPending(true);
        serverConnection_->ConfigureNetworkSimulator(simulatedLatency_, simulatedPacketLoss_);
        LOGINFO("Connecting to server " + serverConnection_->ToString());
        return true;
    }
    else
    {
        LOGERROR("Failed to connect to server " + address + ":" + QString::number(port));
        SendEvent(E_CONNECTFAILED);
        return false;
    }
}

void Network::Disconnect(int waitMSec)
{
    if (!serverConnection_)
        return;

    PROFILE(Disconnect);
    serverConnection_->Disconnect(waitMSec);
}

bool Network::StartServer(unsigned short port)
{
    if (IsServerRunning())
        return true;

    PROFILE(StartServer);

    if (network_->StartServer(port, kNet::SocketOverUDP, this, true) != nullptr)
    {
        LOGINFO("Started server on port " + QString::number(port));
        return true;
    }
    else
    {
        LOGERROR("Failed to start server on port " + QString::number(port));
        return false;
    }
}

void Network::StopServer()
{
    if (!IsServerRunning())
        return;

    PROFILE(StopServer);

    clientConnections_.clear();
    network_->StopServer();
    LOGINFO("Stopped server");
}

void Network::BroadcastMessage(int msgID, bool reliable, bool inOrder, const VectorBuffer& msg, unsigned contentID)
{
    BroadcastMessage(msgID, reliable, inOrder, msg.GetData(), msg.GetSize(), contentID);
}

void Network::BroadcastMessage(int msgID, bool reliable, bool inOrder, const unsigned char* data, unsigned numBytes,
    unsigned contentID)
{
   // Make sure not to use kNet internal message ID's
    if (msgID <= 0x4 || msgID >= 0x3ffffffe)
    {
        LOGERROR("Can not send message with reserved ID");
        return;
    }

    kNet::NetworkServer* server = network_->GetServer();
    if (server)
        server->BroadcastMessage(msgID, reliable, inOrder, 0, contentID, (const char*)data, numBytes);
    else
        LOGERROR("Server not running, can not broadcast messages");
}

void Network::BroadcastRemoteEvent(StringHash eventType, bool inOrder, const VariantMap& eventData)
{
    for (auto & elem : clientConnections_)
        ELEMENT_VALUE(elem)->SendRemoteEvent(eventType, inOrder, eventData);
}

void Network::BroadcastRemoteEvent(Scene* scene, StringHash eventType, bool inOrder, const VariantMap& eventData)
{
    for (auto & elem : clientConnections_)
    {
        if (ELEMENT_VALUE(elem)->GetScene() == scene)
            ELEMENT_VALUE(elem)->SendRemoteEvent(eventType, inOrder, eventData);
    }
}

void Network::BroadcastRemoteEvent(Node* node, StringHash eventType, bool inOrder, const VariantMap& eventData)
{
    if (!node)
    {
        LOGERROR("Null sender node for remote node event");
        return;
    }
    if (node->GetID() >= FIRST_LOCAL_ID)
    {
        LOGERROR("Sender node has a local ID, can not send remote node event");
        return;
    }

    Scene* scene = node->GetScene();
    for (auto & elem : clientConnections_)
    {
        if (ELEMENT_VALUE(elem)->GetScene() == scene)
            ELEMENT_VALUE(elem)->SendRemoteEvent(node, eventType, inOrder, eventData);
    }
}

void Network::SetUpdateFps(int fps)
{
    updateFps_ = Max(fps, 1);
    updateInterval_ = 1.0f / (float)updateFps_;
    updateAcc_ = 0.0f;
}

void Network::SetSimulatedLatency(int ms)
{
    simulatedLatency_ = Max(ms, 0);
    ConfigureNetworkSimulator();
}

void Network::SetSimulatedPacketLoss(float loss)
{
    simulatedPacketLoss_ = Clamp(loss, 0.0f, 1.0f);
    ConfigureNetworkSimulator();
}
void Network::RegisterRemoteEvent(StringHash eventType)
{
    if (blacklistedRemoteEvents_.find(eventType) != blacklistedRemoteEvents_.end())
    {
        LOGERROR("Attempted to register blacklisted remote event type " + eventType.ToString());
        return;
    }

    allowedRemoteEvents_.insert(eventType);
}

void Network::UnregisterRemoteEvent(StringHash eventType)
{
    allowedRemoteEvents_.remove(eventType);
}

void Network::UnregisterAllRemoteEvents()
{
    allowedRemoteEvents_.clear();
}

void Network::SetPackageCacheDir(const QString& path)
{
    packageCacheDir_ = AddTrailingSlash(path);
}

void Network::SendPackageToClients(Scene* scene, PackageFile* package)
{
    if (!scene)
    {
        LOGERROR("Null scene specified for SendPackageToClients");
        return;
    }
    if (!package)
    {
        LOGERROR("Null package specified for SendPackageToClients");
        return;
    }

    for (auto & elem : clientConnections_)
    {
        if (ELEMENT_VALUE(elem)->GetScene() == scene)
            ELEMENT_VALUE(elem)->SendPackageToClient(package);
    }
}

SharedPtr<HttpRequest> Network::MakeHttpRequest(const QString& url, const QString& verb, const std::vector<QString>& headers, const QString& postData)
{
    PROFILE(MakeHttpRequest);
    assert(false && "Convert this to use QNetwork classes" );
    // The initialization of the request will take time, can not know at this point if it has an error or not
//    SharedPtr<HttpRequest> request(new HttpRequest(url, verb, headers, postData));
//    return request;
    SharedPtr<HttpRequest> request(nullptr);
    return request;
}

Connection* Network::GetConnection(kNet::MessageConnection* connection) const
{
    if (serverConnection_ && serverConnection_->GetMessageConnection() == connection)
        return serverConnection_;
    else
    {
        auto i = clientConnections_.find(connection);
        if (i != clientConnections_.end())
            return MAP_VALUE(i);
        return nullptr;
    }
}

Connection* Network::GetServerConnection() const
{
    return serverConnection_;
}

std::vector<SharedPtr<Connection> > Network::GetClientConnections() const
{
    std::vector<SharedPtr<Connection> > ret;
    for (const auto & elem : clientConnections_)
        ret.push_back(ELEMENT_VALUE(elem));

    return ret;
}

bool Network::IsServerRunning() const
{
    return network_->GetServer();
}

bool Network::CheckRemoteEvent(StringHash eventType) const
{
    return allowedRemoteEvents_.contains(eventType);
}

void Network::Update(float timeStep)
{
    PROFILE(UpdateNetwork);

    // Process server connection if it exists
    if (serverConnection_)
    {
        kNet::MessageConnection* connection = serverConnection_->GetMessageConnection();

        // Receive new messages
        connection->Process();

        // Process latest data messages waiting for the correct nodes or components to be created
        serverConnection_->ProcessPendingLatestData();

        // Check for state transitions
        kNet::ConnectionState state = connection->GetConnectionState();
        if (serverConnection_->IsConnectPending() && state == kNet::ConnectionOK)
            OnServerConnected();
        else if (state == kNet::ConnectionPeerClosed)
            serverConnection_->Disconnect();
        else if (state == kNet::ConnectionClosed)
            OnServerDisconnected();
    }

    // Process the network server if started
    kNet::SharedPtr<kNet::NetworkServer> server = network_->GetServer();
    if (server)
        server->Process();
}

void Network::PostUpdate(float timeStep)
{
    PROFILE(PostUpdateNetwork);

    // Check if periodic update should happen now
    updateAcc_ += timeStep;
    bool updateNow = updateAcc_ >= updateInterval_;

    if (updateNow)
    {
        // Notify of the impending update to allow for example updated client controls to be set
        SendEvent(E_NETWORKUPDATE);
        updateAcc_ = fmodf(updateAcc_, updateInterval_);

        if (IsServerRunning())
        {
            // Collect and prepare all networked scenes
            {
                PROFILE(PrepareServerUpdate);

                networkScenes_.clear();
                for (auto & elem : clientConnections_)
                {
                    Scene* scene = ELEMENT_VALUE(elem)->GetScene();
                    if (scene)
                        networkScenes_.insert(scene);
                }

                for (Scene* net_scene : networkScenes_)
                    net_scene->PrepareNetworkUpdate();
            }

            {
                PROFILE(SendServerUpdate);

                // Then send server updates for each client connection
                for (auto & elem : clientConnections_)
                {
                    ELEMENT_VALUE(elem)->SendServerUpdate();
                    ELEMENT_VALUE(elem)->SendRemoteEvents();
                    ELEMENT_VALUE(elem)->SendPackages();
                }
            }
        }

        if (serverConnection_)
        {
            // Send the client update
            serverConnection_->SendClientUpdate();
            serverConnection_->SendRemoteEvents();
        }

        // Notify that the update was sent
        SendEvent(E_NETWORKUPDATESENT);
    }
}

void Network::HandleBeginFrame(StringHash eventType, VariantMap& eventData)
{
    using namespace BeginFrame;

    Update(eventData[P_TIMESTEP].GetFloat());
}

void Network::HandleRenderUpdate(StringHash eventType, VariantMap& eventData)
{
    using namespace RenderUpdate;

    PostUpdate(eventData[P_TIMESTEP].GetFloat());
}

void Network::OnServerConnected()
{
    serverConnection_->SetConnectPending(false);

    LOGINFO("Connected to server");

    // Send the identity map now
    VectorBuffer msg;
    msg.WriteVariantMap(serverConnection_->GetIdentity());
    serverConnection_->SendMessage(MSG_IDENTITY, true, true, msg);

    SendEvent(E_SERVERCONNECTED);
}

void Network::OnServerDisconnected()
{
    // Differentiate between failed connection, and disconnection
    bool failedConnect = serverConnection_ && serverConnection_->IsConnectPending();
    serverConnection_.Reset();

    if (!failedConnect)
    {
        LOGINFO("Disconnected from server");
        SendEvent(E_SERVERDISCONNECTED);
    }
    else
    {
        LOGERROR("Failed to connect to server");
        SendEvent(E_CONNECTFAILED);
    }
}

void Network::ConfigureNetworkSimulator()
{
    if (serverConnection_)
        serverConnection_->ConfigureNetworkSimulator(simulatedLatency_, simulatedPacketLoss_);

    for (auto &elem : clientConnections_)
        ELEMENT_VALUE(elem)->ConfigureNetworkSimulator(simulatedLatency_, simulatedPacketLoss_);
}

void RegisterNetworkLibrary(Context* context)
{
    NetworkPriority::RegisterObject(context);
}

}
