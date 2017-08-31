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

#include "../Sample.h"

/// Console input example.
/// This sample demonstrates:
///     - Implementing a crude text adventure game, which accepts input both through the engine console,
///       and standard input.
class ConsoleInput : public Sample
{
public:
    /// Construct.
    ConsoleInput(Context* context);

    /// Setup after engine initialization and before running the main loop.
    virtual void Start() override;

private:
    /// Handle console command event.
    void HandleConsoleCommand(const QString &command, const QString &id);
    /// Handle frame update event.
    void HandleUpdate(float timeStep);
    /// Handle ESC key down event to quit the engine.
    void HandleEscKeyDown(StringHash eventType, VariantMap& eventData);
    /// Print intro message and initialize the game state.
    void StartGame();
    /// Print game over message.
    void EndGame(const QString& message);
    /// Advance the game state.
    void Advance();
    /// Handle user input either from the engine console or standard input.
    void HandleInput(const QString& input);
    /// Print text to the engine console and standard output.
    void Print(const QString& output);

    /// Game on flag.
    bool gameOn_;
    /// Food dispensed flag.
    bool foodAvailable_;
    /// Whether ate on the previous turn.
    bool eatenLastTurn_;
    /// Number of turns survived.
    int numTurns_;
    /// Player's hunger level.
    int hunger_;
    /// Threat of Urho level.
    int urhoThreat_;
};
