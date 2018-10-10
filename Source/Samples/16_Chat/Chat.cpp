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
#include "Chat.h"

#include <Lutefisk3D/Audio/Audio.h>
#include <Lutefisk3D/UI/Button.h>
#include <Lutefisk3D/Engine/Engine.h>
#include <Lutefisk3D/UI/Font.h>
#include <Lutefisk3D/Graphics/Graphics.h>
#include <Lutefisk3D/Input/Input.h>
#include <Lutefisk3D/IO/IOEvents.h>
#include <Lutefisk3D/UI/LineEdit.h>
#include <Lutefisk3D/IO/Log.h>
#include <Lutefisk3D/IO/MemoryBuffer.h>
#include <Lutefisk3D/Network/Network.h>
#include <Lutefisk3D/Network/NetworkEvents.h>
#include <Lutefisk3D/Resource/ResourceCache.h>
#include <Lutefisk3D/Scene/Scene.h>
#include <Lutefisk3D/Audio/Sound.h>
#include <Lutefisk3D/UI/Text.h>
#include <Lutefisk3D/UI/UI.h>
#include <Lutefisk3D/UI/UIEvents.h>
#include <Lutefisk3D/IO/VectorBuffer.h>
#include <Lutefisk3D/Graphics/Zone.h>


// Undefine Windows macro, as our Connection class has a function called SendMessage
#ifdef SendMessage
#undef SendMessage
#endif

// Identifier for the chat network messages
const int MSG_CHAT = 32;
// UDP port we will use
const unsigned short CHAT_SERVER_PORT = 2345;

URHO3D_DEFINE_APPLICATION_MAIN(Chat)

Chat::Chat(Context* context) :
    Sample("Char",context)
{
}

void Chat::Start()
{
    // Execute base class startup
    Sample::Start();

    // Enable OS cursor
    m_context->m_InputSystem->SetMouseVisible(true);

    // Create the user interface
    CreateUI();

    // Subscribe to UI and network events
    SubscribeToEvents();
}

void Chat::CreateUI()
{
    SetLogoVisible(false); // We need the full rendering window

    Graphics* graphics = m_context->m_Graphics.get();
    UIElement* root = m_context->m_UISystem->GetRoot();
    ResourceCache* cache = m_context->m_ResourceCache.get();
    XMLFile* uiStyle = cache->GetResource<XMLFile>("UI/DefaultStyle.xml");
    // Set style to the UI root so that elements will inherit it
    root->SetDefaultStyle(uiStyle);

    Font* font = cache->GetResource<Font>("Fonts/Anonymous Pro.ttf");
    chatHistoryText_ = root->CreateChild<Text>();
    chatHistoryText_->SetFont(font, 12);

    buttonContainer_ = root->CreateChild<UIElement>();
    buttonContainer_->SetFixedSize(graphics->GetWidth(), 20);
    buttonContainer_->SetPosition(0, graphics->GetHeight() - 20);
    buttonContainer_->SetLayoutMode(LM_HORIZONTAL);

    textEdit_ = buttonContainer_->CreateChild<LineEdit>();
    textEdit_->SetStyleAuto();

    sendButton_ = CreateButton("Send", 70);
    connectButton_ = CreateButton("Connect", 90);
    disconnectButton_ = CreateButton("Disconnect", 100);
    startServerButton_ = CreateButton("Start Server", 110);

    UpdateButtons();

    int rowHeight = chatHistoryText_->GetRowHeight();
    // Row height would be zero if the font failed to load
    if (rowHeight)
        chatHistory_.resize((graphics->GetHeight() - 20) / rowHeight);

    // No viewports or scene is defined. However, the default zone's fog color controls the fill color
    m_context->m_Renderer->GetDefaultZone()->SetFogColor(Color(0.0f, 0.0f, 0.1f));
}

void Chat::SubscribeToEvents()
{
    // Subscribe to UI element events
    textEdit_->textFinished.Connect(this,&Chat::HandleSend);
    sendButton_->released.Connect(this,&Chat::HandleSend);
    connectButton_->released.Connect(this,&Chat::HandleConnect);
    disconnectButton_->released.Connect(this,&Chat::HandleDisconnect);
    startServerButton_->released.Connect(this,&Chat::HandleStartServer);
    // Subscribe to log messages so that we can pipe them to the chat window
    g_LogSignals.logMessageSignal.Connect(this,&Chat::ShowChatText);

    // Subscribe to network events
    SubscribeToEvent(E_NETWORKMESSAGE, URHO3D_HANDLER(Chat, HandleNetworkMessage));
    SubscribeToEvent(E_SERVERCONNECTED, URHO3D_HANDLER(Chat, HandleConnectionStatus));
    SubscribeToEvent(E_SERVERDISCONNECTED, URHO3D_HANDLER(Chat, HandleConnectionStatus));
    SubscribeToEvent(E_CONNECTFAILED, URHO3D_HANDLER(Chat, HandleConnectionStatus));
}

Button* Chat::CreateButton(const QString& text, int width)
{
    ResourceCache* cache = m_context->m_ResourceCache.get();
    Font* font = cache->GetResource<Font>("Fonts/Anonymous Pro.ttf");

    Button* button = buttonContainer_->CreateChild<Button>();
    button->SetStyleAuto();
    button->SetFixedWidth(width);

    Text* buttonText = button->CreateChild<Text>();
    buttonText->SetFont(font, 12);
    buttonText->SetAlignment(HA_CENTER, VA_CENTER);
    buttonText->SetText(text);

    return button;
}

void Chat::ShowChatText(const QString& row)
{
    chatHistory_.pop_front();
    chatHistory_.push_back(row);

    // Concatenate all the rows in history
    QString allRows;
    for (unsigned i = 0; i < chatHistory_.size(); ++i)
        allRows += chatHistory_[i] + "\n";

    chatHistoryText_->SetText(allRows);
}

void Chat::UpdateButtons()
{
    Network* network = m_context->m_Network.get();
    Connection* serverConnection = network->GetServerConnection();
    bool serverRunning = network->IsServerRunning();

    // Show and hide buttons so that eg. Connect and Disconnect are never shown at the same time
    sendButton_->SetVisible(serverConnection != nullptr);
    connectButton_->SetVisible(!serverConnection && !serverRunning);
    disconnectButton_->SetVisible(serverConnection || serverRunning);
    startServerButton_->SetVisible(!serverConnection && !serverRunning);
}

void Chat::HandleLogMessage(StringHash eventType, VariantMap& eventData)
{
    using namespace LogMessage;

    ShowChatText(eventData[P_MESSAGE].GetString());
}

void Chat::HandleSend(UIElement *,const QString &,float)
{
    QString text = textEdit_->GetText();
    if (text.isEmpty())
        return; // Do not send an empty message

    Network* network = GetSubsystem<Network>();
    Connection* serverConnection = network->GetServerConnection();

    if (serverConnection)
    {
        // A VectorBuffer object is convenient for constructing a message to send
        VectorBuffer msg;
        msg.WriteString(text);
        // Send the chat message as in-order and reliable
        serverConnection->SendMessage(MSG_CHAT, true, true, msg);
        // Empty the text edit after sending
        textEdit_->SetText(QString::null);
    }
}

void Chat::HandleConnect(UIElement *)
{
    Network* network = GetSubsystem<Network>();
    QString address = textEdit_->GetText().trimmed();
    if (address.isEmpty())
        address = "localhost"; // Use localhost to connect if nothing else specified
    // Empty the text edit after reading the address to connect to
    textEdit_->SetText(QString::null);

    // Connect to server, do not specify a client scene as we are not using scene replication, just messages.
    // At connect time we could also send identity parameters (such as username) in a VariantMap, but in this
    // case we skip it for simplicity
    network->Connect(address, CHAT_SERVER_PORT, nullptr);

    UpdateButtons();
}

void Chat::HandleDisconnect(UIElement *)
{
    Network* network = GetSubsystem<Network>();
    Connection* serverConnection = network->GetServerConnection();
    // If we were connected to server, disconnect
    if (serverConnection)
        serverConnection->Disconnect();
    // Or if we were running a server, stop it
    else if (network->IsServerRunning())
        network->StopServer();

    UpdateButtons();
}

void Chat::HandleStartServer(UIElement *)
{
    Network* network = GetSubsystem<Network>();
    network->StartServer(CHAT_SERVER_PORT);

    UpdateButtons();
}

void Chat::HandleNetworkMessage(StringHash eventType, VariantMap& eventData)
{
    Network* network = GetSubsystem<Network>();

    using namespace NetworkMessage;

    int msgID = eventData[P_MESSAGEID].GetInt();
    if (msgID == MSG_CHAT)
    {
        const std::vector<unsigned char>& data = eventData[P_DATA].GetBuffer();
        // Use a MemoryBuffer to read the message data so that there is no unnecessary copying
        MemoryBuffer msg(data);
        QString text = msg.ReadString();

        // If we are the server, prepend the sender's IP address and port and echo to everyone
        // If we are a client, just display the message
        if (network->IsServerRunning())
        {
            Connection* sender = static_cast<Connection*>(eventData[P_CONNECTION].GetPtr());

            text = sender->ToString() + " " + text;

            VectorBuffer sendMsg;
            sendMsg.WriteString(text);
            // Broadcast as in-order and reliable
            network->BroadcastMessage(MSG_CHAT, true, true, sendMsg);
        }

        ShowChatText(text);
    }
}

void Chat::HandleConnectionStatus(StringHash eventType, VariantMap& eventData)
{
    UpdateButtons();
}
