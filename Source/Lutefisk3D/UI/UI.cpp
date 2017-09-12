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
#include "UI.h"

#include "CheckBox.h"
#include "Cursor.h"
#include "DropDownList.h"
#include "FileSelector.h"
#include "Font.h"
#include "LineEdit.h"
#include "ListView.h"
#include "MessageBox.h"
#include "ScrollBar.h"
#include "Slider.h"
#include "UIComponent.h"
#include "Sprite.h"
#include "Text.h"
#include "Text3D.h"
#include "ToolTip.h"
#include "UIEvents.h"
#include "View3D.h"
#include "Window.h"

#include "Lutefisk3D/Core/Context.h"
#include "Lutefisk3D/Core/CoreEvents.h"
#include "Lutefisk3D/Core/Profiler.h"
#include "Lutefisk3D/Graphics/Graphics.h"
#include "Lutefisk3D/Graphics/GraphicsEvents.h"
#include "Lutefisk3D/Graphics/RenderSurface.h"
#include "Lutefisk3D/Graphics/Shader.h"
#include "Lutefisk3D/Graphics/ShaderVariation.h"
#include "Lutefisk3D/Graphics/Texture2D.h"
#include "Lutefisk3D/Graphics/VertexBuffer.h"
#include "Lutefisk3D/IO/Log.h"
#include "Lutefisk3D/Input/Input.h"
#include "Lutefisk3D/Input/InputEvents.h"
#include "Lutefisk3D/Math/Matrix3x4.h"
#include "Lutefisk3D/Resource/ResourceCache.h"

#include <SDL2/SDL.h>

#define TOUCHID_MASK(id) (1 << id)

namespace Urho3D
{

StringHash VAR_ORIGIN("Origin");
const StringHash VAR_ORIGINAL_PARENT("OriginalParent");
const StringHash VAR_ORIGINAL_CHILD_INDEX("OriginalChildIndex");
const StringHash VAR_PARENT_CHANGED("ParentChanged");

const float DEFAULT_DOUBLECLICK_INTERVAL = 0.5f;
const float DEFAULT_DRAGBEGIN_INTERVAL = 0.5f;
const float DEFAULT_TOOLTIP_DELAY = 0.5f;
const int DEFAULT_DRAGBEGIN_DISTANCE = 5;
const int DEFAULT_FONT_TEXTURE_MAX_SIZE = 2048;

const char* UI_CATEGORY = "UI";

UI::UI(Context* context) :
    m_context(context),
    rootElement_(new UIElement(context)),
    rootModalElement_(new UIElement(context)),
    doubleClickInterval_(DEFAULT_DOUBLECLICK_INTERVAL),
    dragBeginInterval_(DEFAULT_DRAGBEGIN_INTERVAL),
    defaultToolTipDelay_(DEFAULT_TOOLTIP_DELAY),
    dragBeginDistance_(DEFAULT_DRAGBEGIN_DISTANCE),
    mouseButtons_(0),
    lastMouseButtons_(0),
    qualifiers_(0),
    maxFontTextureSize_(DEFAULT_FONT_TEXTURE_MAX_SIZE),
    initialized_(false),
    usingTouchInput_(false),
    #ifdef _WIN32
    nonFocusedMouseWheel_(false),    // Default MS Windows behaviour
    #else
    nonFocusedMouseWheel_(true),     // Default Mac OS X and Linux behaviour
    #endif
    useSystemClipboard_(false),
    useScreenKeyboard_(false),
    useMutableGlyphs_(false),
    forceAutoHint_(false),
    fontHintLevel_(FONT_HINT_LEVEL_NORMAL),
    fontSubpixelThreshold_(12),
    fontOversampling_(2),
    uiRendered_(false),
    nonModalBatchSize_(0),
    dragElementsCount_(0),
    dragConfirmedCount_(0),
    uiScale_(1.0f),
    customSize_(IntVector2::ZERO)
{
    rootElement_->SetTraversalMode(TM_DEPTH_FIRST);
    rootModalElement_->SetTraversalMode(TM_DEPTH_FIRST);

    // Register UI library object factories
    RegisterUILibrary(m_context);
    g_graphicsSignals.newScreenMode.Connect(this,&UI::HandleScreenMode);
    g_inputSignals.mouseButtonDown.Connect(this,&UI::HandleMouseButtonDown);
    g_inputSignals.mouseButtonUp.Connect(this,&UI::HandleMouseButtonUp);
    g_inputSignals.mouseMove.Connect(this,&UI::HandleMouseMove);
    g_inputSignals.mouseWheel.Connect(this,&UI::HandleMouseWheel);
    g_inputSignals.touchBegun.Connect(this,&UI::HandleTouchBegin);
    g_inputSignals.touchEnd.Connect(this,&UI::HandleTouchEnd);
    g_inputSignals.touchMove.Connect(this,&UI::HandleTouchMove);
    g_inputSignals.keyDown.Connect(this,&UI::HandleKeyDown);
    g_inputSignals.textInput.Connect(this,&UI::HandleTextInput);
    g_inputSignals.dropFile.Connect(this,&UI::HandleDropFile);

    // Try to initialize right now, but skip if screen mode is not yet set
    Initialize();
}

UI::~UI()
{
}

void UI::SetCursor(Cursor* cursor)
{
    // Remove old cursor (if any) and set new
    if (cursor_)
    {
        rootElement_->RemoveChild(cursor_);
        cursor_.Reset();
    }
    if (cursor)
    {
        rootElement_->AddChild(cursor);
        cursor_ = cursor;

        IntVector2 pos = cursor_->GetPosition();
        const IntVector2& rootSize = rootElement_->GetSize();
        const IntVector2& rootPos = rootElement_->GetPosition();
        pos.x_ = Clamp(pos.x_, rootPos.x_, rootPos.x_ + rootSize.x_ - 1);
        pos.y_ = Clamp(pos.y_, rootPos.y_, rootPos.y_ + rootSize.y_ - 1);
        cursor_->SetPosition(pos);
    }
}

void UI::SetFocusElement(UIElement* element, bool byKey)
{
    UIElement* originalElement = element;

    if (element)
    {
        // Return if already has focus
        if (focusElement_ == element)
            return;

        // Only allow child elements of the modal element to receive focus
        if (HasModalElement())
        {
            UIElement* topLevel = element->GetParent();
            while (topLevel && topLevel->GetParent() != rootElement_)
                topLevel = topLevel->GetParent();
            if (topLevel)   // If parented to non-modal root then ignore
                return;
        }

        // Search for an element in the hierarchy that can alter focus. If none found, exit
        element = GetFocusableElement(element);
        if (!element)
            return;
    }

    // Remove focus from the old element
    if (focusElement_)
    {
        UIElement* oldFocusElement = focusElement_;
        focusElement_.Reset();
        //TODO: something wonky/unclear going on here, focused element is released, and than sends an event ?
        oldFocusElement->defocused.Emit(oldFocusElement);
    }

    // Then set focus to the new
    if (element && element->GetFocusMode() >= FM_FOCUSABLE)
    {
        focusElement_ = element;

        element->focused.Emit(element,byKey);
    }
    g_uiSignals.focusChanged.Emit(element,originalElement);
}

bool UI::SetModalElement(UIElement* modalElement, bool enable)
{
    if (!modalElement)
        return false;

    // Currently only allow modal window
    if (modalElement->GetType() != Window::GetTypeStatic())
        return false;

    assert(rootModalElement_);
    UIElement* currParent = modalElement->GetParent();
    if (enable)
    {
        // Make sure it is not already the child of the root modal element
        if (currParent == rootModalElement_)
            return false;

        // Adopt modal root as parent
        modalElement->SetVar(VAR_ORIGINAL_PARENT, currParent);
        modalElement->SetVar(VAR_ORIGINAL_CHILD_INDEX, currParent ? currParent->FindChild(modalElement) : M_MAX_UNSIGNED);
        modalElement->SetParent(rootModalElement_);

        // If it is a popup element, bring along its top-level parent
        UIElement* originElement = static_cast<UIElement*>(modalElement->GetVar(VAR_ORIGIN).GetPtr());
        if (originElement)
        {
            UIElement* element = originElement;
            while (element && element->GetParent() != rootElement_)
                element = element->GetParent();
            if (element)
            {
                originElement->SetVar(VAR_PARENT_CHANGED, element);
                UIElement* oriParent = element->GetParent();
                element->SetVar(VAR_ORIGINAL_PARENT, oriParent);
                element->SetVar(VAR_ORIGINAL_CHILD_INDEX, oriParent ? oriParent->FindChild(element) : M_MAX_UNSIGNED);
                element->SetParent(rootModalElement_);
            }
        }

        return true;
    }
    else
    {
        // Only the modal element can disable itself
        if (currParent != rootModalElement_)
            return false;

        // Revert back to original parent
        modalElement->SetParent(static_cast<UIElement*>(modalElement->GetVar(VAR_ORIGINAL_PARENT).GetPtr()),
                                modalElement->GetVar(VAR_ORIGINAL_CHILD_INDEX).GetUInt());
        VariantMap& vars = const_cast<VariantMap&>(modalElement->GetVars());
        vars.remove(VAR_ORIGINAL_PARENT);
        vars.remove(VAR_ORIGINAL_CHILD_INDEX);

        // If it is a popup element, revert back its top-level parent
        UIElement* originElement = static_cast<UIElement*>(modalElement->GetVar(VAR_ORIGIN).GetPtr());
        if (originElement)
        {
            UIElement* element = static_cast<UIElement*>(originElement->GetVar(VAR_PARENT_CHANGED).GetPtr());
            if (element)
            {
                const_cast<VariantMap&>(originElement->GetVars()).remove(VAR_PARENT_CHANGED);
                element->SetParent(static_cast<UIElement*>(element->GetVar(VAR_ORIGINAL_PARENT).GetPtr()),
                                   element->GetVar(VAR_ORIGINAL_CHILD_INDEX).GetUInt());
                vars = const_cast<VariantMap&>(element->GetVars());
                vars.remove(VAR_ORIGINAL_PARENT);
                vars.remove(VAR_ORIGINAL_CHILD_INDEX);
            }
        }

        return true;
    }
}

void UI::Clear()
{
    rootElement_->RemoveAllChildren();
    rootModalElement_->RemoveAllChildren();
    if (cursor_)
        rootElement_->AddChild(cursor_);
}

void UI::Update(float timeStep)
{
    assert(rootElement_ && rootModalElement_);

    URHO3D_PROFILE_CTX(m_context,UpdateUI);

    // Expire hovers
    for (auto & elem : hoveredElements_)
        ELEMENT_VALUE(elem) = false;

    Input* input = m_context->m_InputSystem.get();
    bool mouseGrabbed = input->IsMouseGrabbed();

    IntVector2 cursorPos;
    bool cursorVisible;
    GetCursorPositionAndVisible(cursorPos, cursorVisible);

    // Drag begin based on time
    if (dragElementsCount_ > 0 && !mouseGrabbed)
    {
        for (auto i = dragElements_.begin(); i != dragElements_.end(); )
        {
            WeakPtr<UIElement> dragElement = MAP_KEY(i);
            UI::DragData* dragData = MAP_VALUE(i);

            if (!dragElement)
            {
                i = DragElementErase(i);
                continue;
            }

            if (!dragData->dragBeginPending)
            {
                ++i;
                continue;
            }

            if (dragData->dragBeginTimer.GetMSec(false) >= (unsigned)(dragBeginInterval_ * 1000))
            {
                dragData->dragBeginPending = false;
                IntVector2 beginSendPos = dragData->dragBeginSumPos / dragData->numDragButtons;
                dragConfirmedCount_ ++;
                if (!usingTouchInput_)
                    dragElement->OnDragBegin(dragElement->ScreenToElement(beginSendPos), beginSendPos,
                                             dragData->dragButtons, qualifiers_, cursor_);
                else
                    dragElement->OnDragBegin(dragElement->ScreenToElement(beginSendPos), beginSendPos,
                                             dragData->dragButtons, 0, nullptr);

                IntVector2 relativePos = dragElement->ScreenToElement(cursorPos);
                dragElement->dragBegin.Emit(dragElement, cursorPos.x_, cursorPos.y_, relativePos.x_, relativePos.y_,
                                            dragData->dragButtons, dragData->numDragButtons);
            }

            ++i;
        }
    }

    // Mouse hover
    if (!mouseGrabbed && !input->GetTouchEmulation())
    {
        if (!usingTouchInput_ && cursorVisible)
            ProcessHover(cursorPos, mouseButtons_, qualifiers_, cursor_);
    }

    // Touch hover
    unsigned numTouches = input->GetNumTouches();
    for (unsigned i = 0; i < numTouches; ++i)
    {
        TouchState* touch = input->GetTouch(i);
        IntVector2 touchPos = touch->position_;
        touchPos.x_ = (int)(touchPos.x_ / uiScale_);
        touchPos.y_ = (int)(touchPos.y_ / uiScale_);
        ProcessHover(touchPos, TOUCHID_MASK(touch->touchID_), 0, nullptr);
    }

    // End hovers that expired without refreshing
    for (auto i = hoveredElements_.begin(); i != hoveredElements_.end();)
    {
        if (MAP_KEY(i).Expired() || !MAP_VALUE(i))
        {
            UIElement* element = MAP_KEY(i);
            if (element)
            {
                element->hoverEnd.Emit(element);
            }
            i = hoveredElements_.erase(i);
        }
        else
            ++i;
    }

    Update(timeStep, rootElement_);
    Update(timeStep, rootModalElement_);
}

void UI::RenderUpdate()
{
    assert(rootElement_ && rootModalElement_ && graphics_);

    URHO3D_PROFILE_CTX(m_context,GetUIBatches);
    uiRendered_ = false;

    // If the OS cursor is visible, do not render the UI's own cursor
    bool osCursorVisible = m_context->m_InputSystem->IsMouseVisible();

    // Get rendering batches from the non-modal UI elements
    batches_.clear();
    vertexData_.clear();
    const IntVector2& rootSize = rootElement_->GetSize();
    const IntVector2& rootPos = rootElement_->GetPosition();
    // Note: the scissors operate on unscaled coordinates. Scissor scaling is only performed during render
    IntRect currentScissor = IntRect(rootPos.x_, rootPos.y_, rootPos.x_ + rootSize.x_, rootPos.y_ + rootSize.y_);
    if (rootElement_->IsVisible())
        GetBatches(batches_, vertexData_, rootElement_, currentScissor);

    // Save the batch size of the non-modal batches for later use
    nonModalBatchSize_ = batches_.size();

    // Get rendering batches from the modal UI elements
    GetBatches(batches_, vertexData_, rootModalElement_, currentScissor);

    // Get batches from the cursor (and its possible children) last to draw it on top of everything
    if (cursor_ && cursor_->IsVisible() && !osCursorVisible)
    {
        currentScissor = IntRect(0, 0, rootSize.x_, rootSize.y_);
        cursor_->GetBatches(batches_, vertexData_, currentScissor);
        GetBatches(batches_, vertexData_, cursor_, currentScissor);
    }

    // Get batches for UI elements rendered into textures. Each element rendered into texture is treated as root element.
    for (auto it = renderToTexture_.begin(); it != renderToTexture_.end();)
    {
        WeakPtr<UIComponent> component = *it;
        if (component.Null() || !component->IsEnabled())
            it = renderToTexture_.erase(it);
        else if (component->IsEnabled())
        {
            component->batches_.clear();
            component->vertexData_.clear();
            UIElement* element = component->GetRoot();
            const IntVector2& size = element->GetSize();
            const IntVector2& pos = element->GetPosition();
            // Note: the scissors operate on unscaled coordinates. Scissor scaling is only performed during render
            IntRect scissor = IntRect(pos.x_, pos.y_, pos.x_ + size.x_, pos.y_ + size.y_);
            GetBatches(component->batches_, component->vertexData_, element, scissor);

            // UIElement does not have anything to show. Insert dummy batch that will clear the texture.
            if (component->batches_.empty())
            {
                UIBatch batch(element, BLEND_REPLACE, scissor, 0, &component->vertexData_);
                batch.SetColor(Color::BLACK);
                batch.AddQuad(scissor.left_, scissor.top_, scissor.right_, scissor.bottom_, 0, 0);
                component->batches_.push_back(batch);
            }
            ++it;
        }
    }
}

void UI::Render(bool renderUICommand)
{
    URHO3D_PROFILE_CTX(m_context,RenderUI);

    // If the OS cursor is visible, apply its shape now if changed
    if (!renderUICommand)
    {
    bool osCursorVisible = m_context->m_InputSystem->IsMouseVisible();
    if (cursor_ && osCursorVisible)
        cursor_->ApplyOSCursorShape();
    }

    // Perform the default backbuffer render only if not rendered yet, or additional renders through RenderUI command
    if (renderUICommand || !uiRendered_)
    {
    SetVertexData(vertexBuffer_, vertexData_);
    SetVertexData(debugVertexBuffer_, debugVertexData_);

        if (!renderUICommand)
            graphics_->ResetRenderTargets();
    // Render non-modal batches
        Render(vertexBuffer_, batches_, 0, nonModalBatchSize_);
    // Render debug draw
        Render(debugVertexBuffer_, debugDrawBatches_, 0, debugDrawBatches_.size());
    // Render modal batches
        Render(vertexBuffer_, batches_, nonModalBatchSize_, batches_.size());
    }

    // Render to UIComponent textures. This is skipped when called from the RENDERUI command
    if (!renderUICommand)
    {
        for (auto it = renderToTexture_.begin(); it != renderToTexture_.end(); it++)
        {
            WeakPtr<UIComponent> component = *it;
            if (component->IsEnabled())
            {
                SetVertexData(component->vertexBuffer_, component->vertexData_);
                SetVertexData(component->debugVertexBuffer_, component->debugVertexData_);

                RenderSurface* surface = component->GetTexture()->GetRenderSurface();
                graphics_->SetRenderTarget(0, surface);
                graphics_->SetViewport(IntRect(0, 0, surface->GetWidth(), surface->GetHeight()));
                graphics_->Clear(Urho3D::CLEAR_COLOR);

                Render(component->vertexBuffer_, component->batches_, 0, component->batches_.size());
                Render(component->debugVertexBuffer_, component->debugDrawBatches_, 0, component->debugDrawBatches_.size());
                component->debugDrawBatches_.clear();
                component->debugVertexData_.clear();
            }
        }

        if (renderToTexture_.size())
            graphics_->ResetRenderTargets();
    }

    // Clear the debug draw batches and data
    debugDrawBatches_.clear();
    debugVertexData_.clear();
    uiRendered_ = true;
}

void UI::DebugDraw(UIElement* element)
{
    if (element)
    {
        UIElement* root = element->GetRoot();
        if (!root)
            root = element;
        const IntVector2& rootSize = root->GetSize();
        const IntVector2& rootPos = root->GetPosition();
        IntRect scissor(rootPos.x_, rootPos.y_, rootPos.x_ + rootSize.x_, rootPos.y_ + rootSize.y_);
        if (root == rootElement_ || root == rootModalElement_)
            element->GetDebugDrawBatches(debugDrawBatches_, debugVertexData_, scissor);
        else
        {
            for (auto it = renderToTexture_.begin(); it != renderToTexture_.end(); it++)
            {
                WeakPtr<UIComponent> component = *it;
                if (component.NotNull() && component->GetRoot() == root && component->IsEnabled())
                {
                    element->GetDebugDrawBatches(component->debugDrawBatches_, component->debugVertexData_, scissor);
                    break;
                }
            }
        }
    }
}

SharedPtr<UIElement> UI::LoadLayout(Deserializer& source, XMLFile* styleFile)
{
    SharedPtr<XMLFile> xml(new XMLFile(m_context));
    if (!xml->Load(source))
        return SharedPtr<UIElement>();
    else
        return LoadLayout(xml, styleFile);
}

SharedPtr<UIElement> UI::LoadLayout(XMLFile* file, XMLFile* styleFile)
{
    URHO3D_PROFILE_CTX(m_context,LoadUILayout);

    SharedPtr<UIElement> root;

    if (!file)
    {
        URHO3D_LOGERROR("Null UI layout XML file");
        return root;
    }

    URHO3D_LOGDEBUG("Loading UI layout " + file->GetName());

    XMLElement rootElem = file->GetRoot("element");
    if (!rootElem)
    {
        URHO3D_LOGERROR("No root UI element in " + file->GetName());
        return root;
    }

    QString typeName = rootElem.GetAttribute("type");
    if (typeName.isEmpty())
        typeName = "UIElement";

    root = DynamicCast<UIElement>(m_context->CreateObject(typeName));
    if (!root)
    {
        URHO3D_LOGERROR("Could not create unknown UI element " + typeName);
        return root;
    }

    // Use default style file of the root element if it has one
    if (!styleFile)
        styleFile = rootElement_->GetDefaultStyle(false);
    // Set it as default for later use by children elements
    if (styleFile)
        root->SetDefaultStyle(styleFile);

    root->LoadXML(rootElem, styleFile);
    return root;
}

bool UI::SaveLayout(Serializer& dest, UIElement* element)
{
    URHO3D_PROFILE_CTX(m_context,SaveUILayout);

    return element && element->SaveXML(dest);
}

void UI::SetClipboardText(const QString& text)
{
    clipBoard_ = text;
    if (useSystemClipboard_)
        SDL_SetClipboardText(qPrintable(text));
}

void UI::SetDoubleClickInterval(float interval)
{
    doubleClickInterval_ = Max(interval, 0.0f);
}

void UI::SetDragBeginInterval(float interval)
{
    dragBeginInterval_ = Max(interval, 0.0f);
}

void UI::SetDragBeginDistance(int pixels)
{
    dragBeginDistance_ = Max(pixels, 0);
}

void UI::SetDefaultToolTipDelay(float delay)
{
    defaultToolTipDelay_ = Max(delay, 0.0f);
}

void UI::SetMaxFontTextureSize(int size)
{
    if (IsPowerOfTwo(size) && size >= FONT_TEXTURE_MIN_SIZE)
    {
        if (size != maxFontTextureSize_)
        {
            maxFontTextureSize_ = size;
            ReleaseFontFaces();
        }
    }
}

void UI::SetNonFocusedMouseWheel(bool nonFocusedMouseWheel)
{
    nonFocusedMouseWheel_ = nonFocusedMouseWheel;
}

void UI::SetUseSystemClipboard(bool enable)
{
    useSystemClipboard_ = enable;
}

void UI::SetUseScreenKeyboard(bool enable)
{
    useScreenKeyboard_ = enable;
}

void UI::SetUseMutableGlyphs(bool enable)
{
    if (enable != useMutableGlyphs_)
    {
        useMutableGlyphs_ = enable;
        ReleaseFontFaces();
    }
}

void UI::SetForceAutoHint(bool enable)
{
    if (enable != forceAutoHint_)
    {
        forceAutoHint_ = enable;
        ReleaseFontFaces();
    }
}

void UI::SetFontHintLevel(FontHintLevel level)
{
    if (level != fontHintLevel_)
    {
        fontHintLevel_ = level;
        ReleaseFontFaces();
    }
}

void UI::SetFontSubpixelThreshold(float threshold)
{
    assert(threshold >= 0);
    if (threshold != fontSubpixelThreshold_)
    {
        fontSubpixelThreshold_ = threshold;
        ReleaseFontFaces();
    }
}

void UI::SetFontOversampling(int oversampling)
{
    assert(oversampling >= 1);
    oversampling = Clamp(oversampling, 1, 8);
    if (oversampling != fontOversampling_)
    {
        fontOversampling_ = oversampling;
        ReleaseFontFaces();
    }
}
void UI::SetScale(float scale)
{
    uiScale_ = Max(scale, M_EPSILON);
    ResizeRootElement();
}

void UI::SetWidth(float width)
{
    IntVector2 size = GetEffectiveRootElementSize(false);
    SetScale((float)size.x_ / width);
}

void UI::SetHeight(float height)
{
    IntVector2 size = GetEffectiveRootElementSize(false);
    SetScale((float)size.y_ / height);
}

void UI::SetCustomSize(const IntVector2& size)
{
    customSize_ = IntVector2(Max(0, size.x_), Max(0, size.y_));
    ResizeRootElement();
}

void UI::SetCustomSize(int width, int height)
{
    customSize_ = IntVector2(Max(0, width), Max(0, height));
    ResizeRootElement();
}
IntVector2 UI::GetCursorPosition() const
{
    return cursor_ ? cursor_->GetPosition() : m_context->m_InputSystem->GetMousePosition();
}
UIElement* UI::GetElementAt(const IntVector2& position, bool enabledOnly, IntVector2* elementScreenPosition)
{
    UIElement* result = 0;

    if (HasModalElement())
        result = GetElementAt(rootModalElement_, position, enabledOnly);

    if (!result)
        result = GetElementAt(rootElement_, position, enabledOnly);

    // Mouse was not hovering UI element. Check elements rendered on 3D objects.
    if (!result && !renderToTexture_.empty())
    {
        for (auto it = renderToTexture_.begin(); it != renderToTexture_.end(); it++)
        {
            WeakPtr<UIComponent> component = *it;
            if (component.Null() || !component->IsEnabled())
                continue;

            IntVector2 screenPosition;
            if (component->ScreenToUIPosition(position, screenPosition))
            {
                result = GetElementAt(component->GetRoot(), screenPosition, enabledOnly);
                if (result)
                {
                    if (elementScreenPosition)
                        *elementScreenPosition = screenPosition;
                    break;
                }
            }
        }
    }
    else if (elementScreenPosition)
        *elementScreenPosition = position;

    return result;
}
UIElement* UI::GetElementAt(const IntVector2& position, bool enabledOnly)
{
    return GetElementAt(position, enabledOnly, 0);
}

UIElement* UI::GetElementAt(UIElement* root, const IntVector2& position, bool enabledOnly)
{
    IntVector2 positionCopy(position);
    const IntVector2& rootSize = root->GetSize();
    const IntVector2& rootPos = root->GetPosition();

    // If position is out of bounds of root element return null.
    if (position.x_ < rootPos.x_ || position.x_ > rootPos.x_ + rootSize.x_)
        return 0;

    if (position.y_ < rootPos.y_ || position.y_ > rootPos.y_ + rootSize.y_)
        return 0;

    // If UI is smaller than the screen, wrap if necessary
    if (rootSize.x_ > 0 && rootSize.y_ > 0)
    {
        if (positionCopy.x_ >= rootPos.x_ + rootSize.x_)
            positionCopy.x_ = rootPos.x_ + ((positionCopy.x_ - rootPos.x_) % rootSize.x_);
        if (positionCopy.y_ >= rootPos.y_ + rootSize.y_)
            positionCopy.y_ = rootPos.y_ + ((positionCopy.y_ - rootPos.y_) % rootSize.y_);
    }

    UIElement* result = 0;
    GetElementAt(result, root, positionCopy, enabledOnly);
    return result;
}

UIElement* UI::GetElementAt(int x, int y, bool enabledOnly)
{
    return GetElementAt(IntVector2(x, y), enabledOnly);
}

UIElement* UI::GetFrontElement() const
{
    const std::vector<SharedPtr<UIElement> >& rootChildren = rootElement_->GetChildren();
    int maxPriority = M_MIN_INT;
    UIElement* front = nullptr;

    for (unsigned i = 0; i < rootChildren.size(); ++i)
    {
        // Do not take into account input-disabled elements, hidden elements or those that are always in the front
        if (!rootChildren[i]->IsEnabled() || !rootChildren[i]->IsVisible() || !rootChildren[i]->GetBringToBack())
            continue;

        int priority = rootChildren[i]->GetPriority();
        if (priority > maxPriority)
        {
            maxPriority = priority;
            front = rootChildren[i];
        }
    }

    return front;
}

const std::vector<UIElement*> UI::GetDragElements()
{
    // Do not return the element until drag begin event has actually been posted
    if (!dragElementsConfirmed_.empty())
        return dragElementsConfirmed_;

    for (auto i = dragElements_.begin(); i != dragElements_.end(); )
    {
        WeakPtr<UIElement> dragElement = MAP_KEY(i);
        UI::DragData* dragData = MAP_VALUE(i);

        if (!dragElement)
        {
            i = DragElementErase(i);
            continue;
        }

        if (!dragData->dragBeginPending)
            dragElementsConfirmed_.push_back(dragElement);

        ++i;
    }

    return dragElementsConfirmed_;
}

UIElement* UI::GetDragElement(unsigned index)
{
    GetDragElements();
    if (index >= dragElementsConfirmed_.size())
        return nullptr;

    return dragElementsConfirmed_[index];
}

const QString& UI::GetClipboardText() const
{
    if (useSystemClipboard_)
    {
        char* text = SDL_GetClipboardText();
        clipBoard_ = QString(text);
        if (text)
            SDL_free(text);
    }

    return clipBoard_;
}

bool UI::HasModalElement() const
{
    return rootModalElement_->GetNumChildren() > 0;
}

void UI::Initialize()
{
    Graphics* graphics = m_context->m_Graphics.get();

    if (!graphics || !graphics->IsInitialized())
        return;

    URHO3D_PROFILE_CTX(m_context,InitUI);

    graphics_ = graphics;
    UIBatch::posAdjust = Vector3(Graphics::GetPixelUVOffset(), 0.0f);

    // Set initial root element size
    ResizeRootElement();

    vertexBuffer_ = new VertexBuffer(m_context);
    debugVertexBuffer_ = new VertexBuffer(m_context);

    initialized_ = true;
    g_coreSignals.beginFrame.Connect(this,&UI::HandleBeginFrame);
    g_coreSignals.postUpdate.Connect(this,&UI::Update);
    g_coreSignals.renderUpdate.Connect(this,&UI::HandleRenderUpdate);

    URHO3D_LOGINFO("Initialized user interface");
}

void UI::Update(float timeStep, UIElement* element)
{
    // Keep a weak pointer to the element in case it destroys itself on update
    WeakPtr<UIElement> elementWeak(element);

    element->Update(timeStep);
    if (elementWeak.Expired())
        return;

    const std::vector<SharedPtr<UIElement> >& children = element->GetChildren();
    // Update of an element may modify its child vector. Use just index-based iteration to be safe
    for (unsigned i = 0; i < children.size(); ++i)
        Update(timeStep, children[i]);
}

void UI::SetVertexData(VertexBuffer* dest, const std::vector<float>& vertexData)
{
    if (vertexData.empty())
        return;

    // Update quad geometry into the vertex buffer
    // Resize the vertex buffer first if too small or much too large
    unsigned numVertices = vertexData.size() / UI_VERTEX_SIZE;
    if (dest->GetVertexCount() < numVertices || dest->GetVertexCount() > numVertices * 2)
        dest->SetSize(numVertices, MASK_POSITION | MASK_COLOR | MASK_TEXCOORD1, true);

    dest->SetData(&vertexData[0]);
}

void UI::Render(VertexBuffer* buffer, const std::vector<UIBatch>& batches, unsigned batchStart, unsigned batchEnd)
{
    // Engine does not render when window is closed or device is lost
    assert(graphics_ && graphics_->IsInitialized() && !graphics_->IsDeviceLost());

    if (batches.empty())
        return;

    RenderSurface* surface = graphics_->GetRenderTarget(0);
    IntVector2 viewSize = graphics_->GetViewport().Size();
    Vector2 invScreenSize(1.0f / (float)viewSize.x_, 1.0f / (float)viewSize.y_);
    Vector2 scale(2.0f * invScreenSize.x_, -2.0f * invScreenSize.y_);
    Vector2 offset(-1.0f, 1.0f);
    if (surface)
    {
        // On OpenGL, flip the projection if rendering to a texture so that the texture can be addressed in the
        // same way as a render texture produced on Direct3D.
        offset.y_ = -offset.y_;
        scale.y_ = -scale.y_;
    }

    Matrix4 projection(Matrix4::IDENTITY);
    projection.m00_ = scale.x_ * uiScale_;
    projection.m03_ = offset.x_;
    projection.m11_ = scale.y_ * uiScale_;
    projection.m13_ = offset.y_;
    projection.m22_ = 1.0f;
    projection.m23_ = 0.0f;
    projection.m33_ = 1.0f;

    graphics_->ClearParameterSources();
    graphics_->SetColorWrite(true);
    // Reverse winding if rendering to texture on OpenGL
    if (surface)
        graphics_->SetCullMode(CULL_CW);
    else
        graphics_->SetCullMode(CULL_CCW);
    graphics_->SetDepthTest(CMP_ALWAYS);
    graphics_->SetDepthWrite(false);
    graphics_->SetFillMode(FILL_SOLID);
    graphics_->SetStencilTest(false);
    graphics_->SetVertexBuffer(buffer);

    ShaderVariation* noTextureVS = graphics_->GetShader(VS, "Basic", "VERTEXCOLOR");
    ShaderVariation* diffTextureVS = graphics_->GetShader(VS, "Basic", "DIFFMAP VERTEXCOLOR");
    ShaderVariation* noTexturePS = graphics_->GetShader(PS, "Basic", "VERTEXCOLOR");
    ShaderVariation* diffTexturePS = graphics_->GetShader(PS, "Basic", "DIFFMAP VERTEXCOLOR");
    ShaderVariation* diffMaskTexturePS = graphics_->GetShader(PS, "Basic", "DIFFMAP ALPHAMASK VERTEXCOLOR");
    ShaderVariation* alphaTexturePS = graphics_->GetShader(PS, "Basic", "ALPHAMAP VERTEXCOLOR");

    gl::GLenum alphaFormat = Graphics::GetAlphaFormat();

    for (unsigned i = batchStart; i < batchEnd; ++i)
    {
        const UIBatch& batch = batches[i];
        if (batch.vertexStart_ == batch.vertexEnd_)
            continue;

        ShaderVariation* ps;
        ShaderVariation* vs;

        if (!batch.texture_)
        {
            ps = noTexturePS;
            vs = noTextureVS;
        }
        else
        {
            // If texture contains only an alpha channel, use alpha shader (for fonts)
            vs = diffTextureVS;

            if (batch.texture_->GetFormat() == alphaFormat)
                ps = alphaTexturePS;
            else if (batch.blendMode_ != BLEND_ALPHA && batch.blendMode_ != BLEND_ADDALPHA && batch.blendMode_ != BLEND_PREMULALPHA)
                ps = diffMaskTexturePS;
            else
                ps = diffTexturePS;
        }

        graphics_->SetShaders(vs, ps);
        if (graphics_->NeedParameterUpdate(SP_OBJECT, this))
            graphics_->SetShaderParameter(VSP_MODEL, Matrix3x4::IDENTITY);
        if (graphics_->NeedParameterUpdate(SP_CAMERA, this))
            graphics_->SetShaderParameter(VSP_VIEWPROJ, projection);
        if (graphics_->NeedParameterUpdate(SP_MATERIAL, this))
            graphics_->SetShaderParameter(PSP_MATDIFFCOLOR, Color(1.0f, 1.0f, 1.0f, 1.0f));

        float elapsedTime = m_context->m_TimeSystem->GetElapsedTime();
        graphics_->SetShaderParameter(VSP_ELAPSEDTIME, elapsedTime);
        graphics_->SetShaderParameter(PSP_ELAPSEDTIME, elapsedTime);

        IntRect scissor = batch.scissor_;
        scissor.left_ = (int)(scissor.left_ * uiScale_);
        scissor.top_ = (int)(scissor.top_ * uiScale_);
        scissor.right_ = (int)(scissor.right_ * uiScale_);
        scissor.bottom_ = (int)(scissor.bottom_ * uiScale_);
        // Flip scissor vertically if using OpenGL texture rendering
        if (surface)
        {
            int top = scissor.top_;
            int bottom = scissor.bottom_;
            scissor.top_ = viewSize.y_ - bottom;
            scissor.bottom_ = viewSize.y_ - top;
        }
        graphics_->SetBlendMode(batch.blendMode_);
        graphics_->SetScissorTest(true, scissor);
        graphics_->SetTexture(0, batch.texture_);
        graphics_->Draw(TRIANGLE_LIST, batch.vertexStart_ / UI_VERTEX_SIZE,
              (batch.vertexEnd_ - batch.vertexStart_) / UI_VERTEX_SIZE);
    }
}

void UI::GetBatches(std::vector<UIBatch>& batches, std::vector<float>& vertexData, UIElement* element, IntRect currentScissor)
{
    // Set clipping scissor for child elements. No need to draw if zero size
    element->AdjustScissor(currentScissor);
    if (currentScissor.left_ == currentScissor.right_ || currentScissor.top_ == currentScissor.bottom_)
        return;

    element->SortChildren();
    const std::vector<SharedPtr<UIElement> >& children = element->GetChildren();
    if (children.empty())
        return;

    // For non-root elements draw all children of same priority before recursing into their children: assumption is that they have
    // same renderstate
    std::vector<SharedPtr<UIElement> >::const_iterator i = children.begin();
    if (element->GetTraversalMode() == TM_BREADTH_FIRST)
    {
        std::vector<SharedPtr<UIElement> >::const_iterator j = i;
        while (i != children.end())
        {
            int currentPriority = (*i)->GetPriority();
            while (j != children.end() && (*j)->GetPriority() == currentPriority)
            {
                if ((*j)->IsWithinScissor(currentScissor) && (*j) != cursor_)
                    (*j)->GetBatches(batches, vertexData, currentScissor);
                ++j;
            }
            // Now recurse into the children
            while (i != j)
            {
                if ((*i)->IsVisible() && (*i) != cursor_)
                    GetBatches(batches, vertexData, *i, currentScissor);
                ++i;
            }
        }
    }
    // On the root level draw each element and its children immediately after to avoid artifacts
    else
    {
        while (i != children.end())
        {
            if ((*i) != cursor_)
            {
                if ((*i)->IsWithinScissor(currentScissor))
                    (*i)->GetBatches(batches, vertexData, currentScissor);
                if ((*i)->IsVisible())
                    GetBatches(batches, vertexData, *i, currentScissor);
            }
            ++i;
        }
    }
}

void UI::GetElementAt(UIElement*& result, UIElement* current, const IntVector2& position, bool enabledOnly)
{
    if (!current)
        return;

    current->SortChildren();
    const std::vector<SharedPtr<UIElement> >& children = current->GetChildren();
    LayoutMode parentLayoutMode = current->GetLayoutMode();

    for (unsigned i = 0; i < children.size(); ++i)
    {
        UIElement* element = children[i];
        bool hasChildren = element->GetNumChildren() > 0;

        if (element != cursor_.Get() && element->IsVisible())
        {
            if (element->IsInside(position, true))
            {
                // Store the current result, then recurse into its children. Because children
                // are sorted from lowest to highest priority, the topmost match should remain
                if (element->IsEnabled() || !enabledOnly)
                    result = element;

                if (hasChildren)
                    GetElementAt(result, element, position, enabledOnly);
                // Layout optimization: if the element has no children, can break out after the first match
                else if (parentLayoutMode != LM_FREE)
                    break;
            }
            else
            {
                if (hasChildren)
                {
                    if (element->IsInsideCombined(position, true))
                        GetElementAt(result, element, position, enabledOnly);
                }
                // Layout optimization: if position is much beyond the visible screen, check how many elements we can skip,
                // or if we already passed all visible elements
                else if (parentLayoutMode != LM_FREE)
                {
                    if (!i)
                    {
                        int screenPos = (parentLayoutMode == LM_HORIZONTAL) ? element->GetScreenPosition().x_ :
                                                                              element->GetScreenPosition().y_;
                        int layoutMaxSize = current->GetLayoutElementMaxSize();

                        if (screenPos < 0 && layoutMaxSize > 0)
                        {
                            unsigned toSkip = (unsigned)(-screenPos / layoutMaxSize);
                            if (toSkip > 0)
                                i += (toSkip - 1);
                        }
                    }
                    // Note: we cannot check for the up / left limits of positioning, since the element may be off the visible
                    // screen but some of its layouted children will yet be visible. In down & right directions we can terminate
                    // the loop, since all further children will be further down or right.
                    else if (parentLayoutMode == LM_HORIZONTAL)
                    {
                        if (element->GetScreenPosition().x_ >= rootElement_->GetPosition().x_ + rootElement_->GetSize().x_)
                            break;
                    }
                    else if (parentLayoutMode == LM_VERTICAL)
                    {
                        if (element->GetScreenPosition().y_ >= rootElement_->GetPosition().y_ + rootElement_->GetSize().y_)
                            break;
                    }
                }
            }
        }
    }
}

UIElement* UI::GetFocusableElement(UIElement* element)
{
    while (element)
    {
        if (element->GetFocusMode() != FM_NOTFOCUSABLE)
            break;
        element = element->GetParent();
    }
    return element;
}

void UI::GetCursorPositionAndVisible(IntVector2& pos, bool& visible)
{
    // Prefer software cursor then OS-specific cursor
    if (cursor_ && cursor_->IsVisible())
    {
        pos = cursor_->GetPosition();
        visible = true;
    }
    else if (m_context->m_InputSystem->GetMouseMode() == MM_RELATIVE)
        visible = true;
    else
    {
        Input* input = m_context->m_InputSystem.get();
        pos = input->GetMousePosition();
        visible = input->IsMouseVisible();

        if (!visible && cursor_)
            pos = cursor_->GetPosition();
    }
    pos.x_ = (int)(pos.x_ / uiScale_);
    pos.y_ = (int)(pos.y_ / uiScale_);
}

void UI::SetCursorShape(CursorShape shape)
{
    if (cursor_)
        cursor_->SetShape(shape);
}

void UI::ReleaseFontFaces()
{
    URHO3D_LOGDEBUG("Reloading font faces");

    std::vector<Font*> fonts;
    m_context->m_ResourceCache->GetResources<Font>(fonts);

    for (unsigned i = 0; i < fonts.size(); ++i)
        fonts[i]->ReleaseFaces();
}

void UI::ProcessHover(const IntVector2& cursorPos, int buttons, int qualifiers, Cursor* cursor)
{
    WeakPtr<UIElement> element(GetElementAt(cursorPos));

    for (auto i = dragElements_.begin(); i != dragElements_.end();)
    {
        WeakPtr<UIElement> dragElement = MAP_KEY(i);
        UI::DragData* dragData = MAP_VALUE(i);

        if (!dragElement)
        {
            i = DragElementErase(i);
            continue;
        }

        bool dragSource = dragElement && (dragElement->GetDragDropMode() & DD_SOURCE) != 0;
        bool dragTarget = element && (element->GetDragDropMode() & DD_TARGET) != 0;
        bool do_dragDropTest = dragSource && dragTarget && element != dragElement;
        // If drag start event has not been posted yet, do not do drag handling here
        if (dragData->dragBeginPending)
            dragSource = dragTarget = do_dragDropTest = false;

        // Hover effect
        // If a drag is going on, transmit hover only to the element being dragged, unless it's a drop target
        if (element && element->IsEnabled())
        {
            if (dragElement == element || do_dragDropTest)
            {
                element->OnHover(element->ScreenToElement(cursorPos), cursorPos, buttons, qualifiers, cursor);

                // Begin hover event
                if (!hoveredElements_.contains(element))
                {
                    IntVector2 relativePos = element->ScreenToElement(cursorPos);
                    element->hoverBegin.Emit(element,cursorPos.x_,cursorPos.y_,relativePos.x_,relativePos.y_);
                    // Exit if element is destroyed by the event handling
                    if (!element)
                        return;
                }
                hoveredElements_[element] = true;
            }
        }

        // Drag and drop test
        if (do_dragDropTest)
        {
            bool accept = element->OnDragDropTest(dragElement);
            if (accept)
            {
                g_uiSignals.dragDropTest.Emit(dragElement.Get(),element.Get(),accept);
            }

            if (cursor)
                cursor->SetShape(accept ? CS_ACCEPTDROP : CS_REJECTDROP);
        }
        else if (dragSource && cursor)
            cursor->SetShape(dragElement == element ? CS_ACCEPTDROP : CS_REJECTDROP);

        ++i;
    }

    // Hover effect
    // If no drag is going on, transmit hover event.
    if (element && element->IsEnabled())
    {
        if (dragElementsCount_ == 0)
        {
            element->OnHover(element->ScreenToElement(cursorPos), cursorPos, buttons, qualifiers, cursor);

            // Begin hover event
            if (!hoveredElements_.contains(element))
            {
                IntVector2 relativePos = element->ScreenToElement(cursorPos);
                element->hoverBegin.Emit(element,cursorPos.x_,cursorPos.y_,relativePos.x_,relativePos.y_);
                // Exit if element is destroyed by the event handling
                if (!element)
                    return;
            }
            hoveredElements_[element] = true;
        }
    }
}

void UI::ProcessClickBegin(const IntVector2& cursorPos, int button, int buttons, int qualifiers, Cursor* cursor, bool cursorVisible)
{
    if (cursorVisible)
    {
        WeakPtr<UIElement> element(GetElementAt(cursorPos));

        bool newButton;
        if (usingTouchInput_)
            newButton = (button & buttons) == 0;
        else
            newButton = true;
        buttons |= button;

        if (element)
            SetFocusElement (element);

        // Focus change events may destroy the element, check again.
        if (element)
        {
            // Handle focusing & bringing to front
            element->BringToFront();

            // Handle click
            element->OnClickBegin(element->ScreenToElement(cursorPos), cursorPos, button, buttons, qualifiers, cursor);
            // Send also element version of the event
            element->click.Emit(element,cursorPos.x_,cursorPos.y_,button,buttons,qualifiers);
            g_uiSignals.mouseClickUI.Emit(element,cursorPos.x_,cursorPos.y_,button,buttons,qualifiers);


            // Fire double click event if element matches and is in time
            if (doubleClickElement_ && element == doubleClickElement_ && clickTimer_.GetMSec(true) <
                    (unsigned)(doubleClickInterval_ * 1000) && lastMouseButtons_ == buttons)
            {
                element->OnDoubleClick(element->ScreenToElement(cursorPos), cursorPos, button, buttons, qualifiers, cursor);
                doubleClickElement_.Reset();
                element->doubleClick.Emit(element,cursorPos.x_,cursorPos.y_,button,buttons,qualifiers);
                g_uiSignals.mouseDoubleClickUI.Emit(element,cursorPos.x_,cursorPos.y_,button,buttons,qualifiers);
            }
            else
            {
                doubleClickElement_ = element;
                clickTimer_.Reset();
            }

            // Handle start of drag. Click handling may have caused destruction of the element, so check the pointer again
            bool dragElementsContain = dragElements_.contains(element);
            if (element && !dragElementsContain)
            {
                DragData* dragData = new DragData();
                dragElements_[element] = dragData;
                dragData->dragBeginPending = true;
                dragData->sumPos = cursorPos;
                dragData->dragBeginSumPos = cursorPos;
                dragData->dragBeginTimer.Reset();
                dragData->dragButtons = button;
                dragData->numDragButtons = CountSetBits(dragData->dragButtons);
                dragElementsCount_++;

                dragElementsContain = dragElements_.contains(element);
            }
            if (element && dragElementsContain && newButton)
            {
                DragData* dragData = dragElements_[element];
                dragData->sumPos += cursorPos;
                dragData->dragBeginSumPos += cursorPos;
                dragData->dragButtons |= button;
                dragData->numDragButtons = CountSetBits(dragData->dragButtons);
            }
        }
        else
        {
            // If clicked over no element, or a disabled element, lose focus (but not if there is a modal element)
            if (!HasModalElement())
                SetFocusElement(nullptr);
            if(element)
                element->click.Emit(element,cursorPos.x_,cursorPos.y_,button,buttons,qualifiers);
            g_uiSignals.mouseClickUI.Emit(element,cursorPos.x_,cursorPos.y_,button,buttons,qualifiers);

            if (clickTimer_.GetMSec(true) < (unsigned)(doubleClickInterval_ * 1000) && lastMouseButtons_ == buttons) {
                if(element)
                    element->doubleClick.Emit(element,cursorPos.x_,cursorPos.y_,button,buttons,qualifiers);
                g_uiSignals.mouseDoubleClickUI.Emit(element,cursorPos.x_,cursorPos.y_,button,buttons,qualifiers);
            }
        }

        lastMouseButtons_ = buttons;
    }
}

void UI::ProcessClickEnd(const IntVector2& cursorPos, int button, int buttons, int qualifiers, Cursor* cursor, bool cursorVisible)
{
    WeakPtr<UIElement> element;
    if (cursorVisible)
        element = GetElementAt(cursorPos);

    // Handle end of drag
    for (auto i = dragElements_.begin(); i != dragElements_.end();)
    {
        WeakPtr<UIElement> dragElement = MAP_KEY(i);
        UI::DragData* dragData = MAP_VALUE(i);

        if (!dragElement || !cursorVisible)
        {
            i = DragElementErase(i);
            continue;
        }

        if (dragData->dragButtons & button)
        {
            // Handle end of click
            if (element)
            {
                element->OnClickEnd(element->ScreenToElement(cursorPos), cursorPos, button, buttons, qualifiers, cursor, dragElement);
                element->clickEnd.Emit(element,dragElement,cursorPos.x_,cursorPos.y_,button,buttons,qualifiers);
            }
            g_uiSignals.mouseClickEndUI.Emit(element,dragElement,cursorPos.x_,cursorPos.y_,button,buttons,qualifiers);

            if (dragElement && dragElement->IsEnabled() && dragElement->IsVisible() && !dragData->dragBeginPending)
            {
                dragElement->OnDragEnd(dragElement->ScreenToElement(cursorPos), cursorPos, dragData->dragButtons, buttons, cursor);
                IntVector2 relativePos = dragElement->ScreenToElement(cursorPos);
                dragElement->dragEnd.Emit(dragElement, cursorPos.x_, cursorPos.y_, relativePos.x_, relativePos.y_,
                                          dragData->dragButtons, dragData->numDragButtons);

                bool dragSource = dragElement && (dragElement->GetDragDropMode() & DD_SOURCE) != 0;
                if (dragSource)
                {
                    bool dragTarget = element && (element->GetDragDropMode() & DD_TARGET) != 0;
                    bool do_dragDropFinish = dragSource && dragTarget && element != dragElement;

                    if (do_dragDropFinish)
                    {
                        bool accept = element->OnDragDropFinish(dragElement);

                        // OnDragDropFinish() may have caused destruction of the elements, so check the pointers again
                        if (accept && dragElement && element)
                        {
                            g_uiSignals.dragDropFinish.Emit(dragElement.Get(),element.Get(),accept);
                        }
                    }
                }
            }

            i = DragElementErase(i);
        }
        else
            ++i;
    }
}

void UI::ProcessMove(const IntVector2& cursorPos, const IntVector2& cursorDeltaPos, int buttons, int qualifiers, Cursor* cursor, bool cursorVisible)
{
    if (cursorVisible && dragElementsCount_ > 0 && buttons)
    {
        bool mouseGrabbed = m_context->m_InputSystem->IsMouseGrabbed();
        for (auto i = dragElements_.begin(); i != dragElements_.end();)
        {
            WeakPtr<UIElement> dragElement = MAP_KEY(i);
            UI::DragData* dragData = MAP_VALUE(i);

            if (!dragElement)
            {
                i = DragElementErase(i);
                continue;
            }

            if (!(dragData->dragButtons & buttons))
            {
                ++i;
                continue;
            }

            // Calculate the position that we should send for this drag event.
            IntVector2 sendPos;
            if (usingTouchInput_)
            {
                dragData->sumPos += cursorDeltaPos;
                sendPos.x_ = dragData->sumPos.x_ / dragData->numDragButtons;
                sendPos.y_ = dragData->sumPos.y_ / dragData->numDragButtons;
            }
            else
            {
                dragData->sumPos = cursorPos;
                sendPos = cursorPos;
            }

            if (dragElement->IsEnabled() && dragElement->IsVisible())
            {
                // Signal drag begin if distance threshold was exceeded

                if (dragData->dragBeginPending && !mouseGrabbed)
                {
                    IntVector2 beginSendPos;
                    beginSendPos.x_ = dragData->dragBeginSumPos.x_ / dragData->numDragButtons;
                    beginSendPos.y_ = dragData->dragBeginSumPos.y_ / dragData->numDragButtons;

                    IntVector2 offset = cursorPos - beginSendPos;
                    if (Abs(offset.x_) >= dragBeginDistance_ || Abs(offset.y_) >= dragBeginDistance_)
                    {
                        dragData->dragBeginPending = false;
                        dragConfirmedCount_ ++;
                        dragElement->OnDragBegin(dragElement->ScreenToElement(beginSendPos), beginSendPos, buttons, qualifiers, cursor);
                        IntVector2 relativePos = dragElement->ScreenToElement(beginSendPos);
                        dragElement->dragBegin.Emit(dragElement, beginSendPos.x_, beginSendPos.y_,
                                                    relativePos.x_, relativePos.y_,
                                                    dragData->dragButtons, dragData->numDragButtons);

                    }
                }

                if (!dragData->dragBeginPending)
                {
                    dragElement->OnDragMove(dragElement->ScreenToElement(sendPos), sendPos, cursorDeltaPos, buttons, qualifiers, cursor);
                    IntVector2 relativePos = dragElement->ScreenToElement(sendPos);
                    dragElement->dragMove.Emit(dragElement, sendPos.x_, sendPos.y_, cursorDeltaPos, relativePos.x_, relativePos.y_, dragData->dragButtons,
                                               dragData->numDragButtons);
                }
            }
            else
            {
                dragElement->OnDragEnd(dragElement->ScreenToElement(sendPos), sendPos, dragData->dragButtons, buttons, cursor);
                IntVector2 relativePos = dragElement->ScreenToElement(sendPos);
                dragElement->dragEnd.Emit(dragElement, sendPos.x_, sendPos.y_,
                                           relativePos.x_, relativePos.y_, dragData->dragButtons,
                                           dragData->numDragButtons);

                dragElement.Reset();
            }

            ++i;
        }
    }
}

void UI::HandleScreenMode(int,int,bool,bool,bool,bool,int,int)
{
    if (!initialized_)
        Initialize();
    else
        ResizeRootElement();
}

void UI::HandleMouseButtonDown(int button, unsigned buttons, int quals)
{
    mouseButtons_ = buttons;
    qualifiers_ = quals;
    usingTouchInput_ = false;

    IntVector2 cursorPos;
    bool cursorVisible;
    GetCursorPositionAndVisible(cursorPos, cursorVisible);

    // Handle drag cancelling
    ProcessDragCancel();

    if (!m_context->m_InputSystem->IsMouseGrabbed())
        ProcessClickBegin(cursorPos, button, mouseButtons_, qualifiers_, cursor_, cursorVisible);
}

void UI::HandleMouseButtonUp(int Button,unsigned Buttons,int Qualifiers)
{
    mouseButtons_ = Buttons;
    qualifiers_ = Qualifiers;

    IntVector2 cursorPos;
    bool cursorVisible;
    GetCursorPositionAndVisible(cursorPos, cursorVisible);

    ProcessClickEnd(cursorPos, Button, mouseButtons_, qualifiers_, cursor_, cursorVisible);
}

void UI::HandleMouseMove(int x, int y, int DX, int DY, unsigned buttons, int quals)
{
    mouseButtons_ = buttons;
    qualifiers_ = quals;
    usingTouchInput_ = false;

    Input* input = m_context->m_InputSystem.get();
    const IntVector2& rootSize = rootElement_->GetSize();
    const IntVector2& rootPos = rootElement_->GetPosition();

    IntVector2 DeltaP = IntVector2(DX, DY);

    if (cursor_)
    {
        if (!input->IsMouseVisible())
        {
            if (!input->IsMouseLocked())
                cursor_->SetPosition(IntVector2(x, y));
            else if (cursor_->IsVisible())
            {
                // Relative mouse motion: move cursor only when visible
                IntVector2 pos = cursor_->GetPosition() + DeltaP;
                pos.x_ = Clamp(pos.x_, rootPos.x_, rootPos.x_ + rootSize.x_ - 1);
                pos.y_ = Clamp(pos.y_, rootPos.y_, rootPos.y_ + rootSize.y_ - 1);
                cursor_->SetPosition(pos);
            }
        }
        else
        {
            // Absolute mouse motion: move always
            cursor_->SetPosition(IntVector2(x, y));
        }
    }

    IntVector2 cursorPos;
    bool cursorVisible;
    GetCursorPositionAndVisible(cursorPos, cursorVisible);

    ProcessMove(cursorPos, DeltaP, mouseButtons_, qualifiers_, cursor_, cursorVisible);
}

void UI::HandleMouseWheel(int Wheel,unsigned Buttons,int Qualifiers)
{
    if (m_context->m_InputSystem->IsMouseGrabbed())
        return;

    mouseButtons_ = Buttons;
    qualifiers_ = Qualifiers;
    int delta = Wheel;
    usingTouchInput_ = false;

    IntVector2 cursorPos;
    bool cursorVisible;
    GetCursorPositionAndVisible(cursorPos, cursorVisible);

    UIElement* element;
    if (!nonFocusedMouseWheel_&& (element = focusElement_))
        element->OnWheel(delta, mouseButtons_, qualifiers_);
    else
    {
        // If no element has actual focus or in non-focused mode, get the element at cursor
        if (cursorVisible)
        {
            element = GetElementAt(cursorPos);
            if (nonFocusedMouseWheel_)
            {
                // Going up the hierarchy chain to find element that could handle mouse wheel
                while (element)
                {
                    if (element->GetType() == ListView::GetTypeStatic() ||
                            element->GetType() == ScrollView::GetTypeStatic())
                        break;
                    element = element->GetParent();
                }
            }
            else
                // If the element itself is not focusable, search for a focusable parent,
                // although the focusable element may not actually handle mouse wheel
                element = GetFocusableElement(element);

            if (element && (nonFocusedMouseWheel_ || element->GetFocusMode() >= FM_FOCUSABLE))
                element->OnWheel(delta, mouseButtons_, qualifiers_);
        }
    }
}

void UI::HandleTouchBegin(unsigned touchID,int x,int y,float pressure)
{
    if (m_context->m_InputSystem->IsMouseGrabbed())
        return;

    IntVector2 pos(x, y);
    pos.x_ = int(pos.x_ / uiScale_);
    pos.y_ = int(pos.y_ / uiScale_);
    usingTouchInput_ = true;

    int touchMask = TOUCHID_MASK(touchID);
    WeakPtr<UIElement> element(GetElementAt(pos));

    if (element)
    {
        ProcessClickBegin(pos, touchMask, touchDragElements_[element], 0, nullptr, true);
        touchDragElements_[element] |= touchMask;
    }
    else
        ProcessClickBegin(pos, touchMask, touchMask, 0, nullptr, true);
}

void UI::HandleTouchEnd(unsigned touchID,int x,int y)
{
    IntVector2 pos(x, y);
    pos.x_ = int(pos.x_ / uiScale_);
    pos.y_ = int(pos.y_ / uiScale_);

    // Get the touch index
    int touchMask = TOUCHID_MASK(touchID);

    // Transmit hover end to the position where the finger was lifted
    WeakPtr<UIElement> element(GetElementAt(pos));

    // Clear any drag events that were using the touch id
    for (auto i = touchDragElements_.begin(),fin=touchDragElements_.end(); i != fin; )
    {
        int touches = MAP_VALUE(i);
        if (touches & touchMask)
            i = touchDragElements_.erase(i);
        else
            ++i;
    }

    if (element && element->IsEnabled())
        element->OnHover(element->ScreenToElement(pos), pos, 0, 0, nullptr);

    ProcessClickEnd(pos, touchMask, 0, 0, nullptr, true);
}

void UI::HandleTouchMove(unsigned touchID,int x,int y,int dX,int dY, float Pressure)
{
    IntVector2 pos(x, y);
    IntVector2 deltaPos(dX, dY);
    pos.x_ = int(pos.x_ / uiScale_);
    pos.y_ = int(pos.y_ / uiScale_);
    deltaPos.x_ = int(deltaPos.x_ / uiScale_);
    deltaPos.y_ = int(deltaPos.y_ / uiScale_);
    usingTouchInput_ = true;

    int touchMask = TOUCHID_MASK(touchID);

    ProcessMove(pos, deltaPos, touchMask, 0, nullptr, true);
}

void UI::HandleKeyDown(int key,int ,unsigned buttons,int qualifiers, bool )
{
    mouseButtons_ = buttons;
    qualifiers_ = qualifiers;

    // Cancel UI dragging
    if (key == KEY_ESCAPE && dragElementsCount_ > 0)
    {
        ProcessDragCancel();

        return;
    }

    // Dismiss modal element if any when ESC key is pressed
    if (key == KEY_ESCAPE && HasModalElement())
    {
        UIElement* element = rootModalElement_->GetChild(rootModalElement_->GetNumChildren() - 1);
        if (element->GetVars().contains(VAR_ORIGIN))
            // If it is a popup, dismiss by defocusing it
            SetFocusElement(nullptr);
        else
        {
            // If it is a modal window, by resetting its modal flag
            Window* window = dynamic_cast<Window*>(element);
            if (window && window->GetModalAutoDismiss())
                window->SetModal(false);
        }

        return;
    }

    UIElement* element = focusElement_;
    if (element)
    {
        // Switch focus between focusable elements in the same top level window
        if (key == KEY_TAB)
        {
            UIElement* topLevel = element->GetParent();
            while (topLevel && topLevel->GetParent() != rootElement_ && topLevel->GetParent() != rootModalElement_)
                topLevel = topLevel->GetParent();
            if (topLevel)
            {
                topLevel->GetChildren(tempElements_, true);
                for (std::vector<UIElement*>::iterator i = tempElements_.begin(); i != tempElements_.end();)
                {
                    if ((*i)->GetFocusMode() < FM_FOCUSABLE)
                        i = tempElements_.erase(i);
                    else
                        ++i;
                }
                for (unsigned i = 0; i < tempElements_.size(); ++i)
                {
                    if (tempElements_[i] == element)
                    {
                        int dir = (qualifiers_ & QUAL_SHIFT) ? -1 : 1;
                        unsigned nextIndex = (tempElements_.size() + i + dir) % tempElements_.size();
                        UIElement* next = tempElements_[nextIndex];
                        SetFocusElement(next, true);
                        return;
                    }
                }
            }
        }
        // Defocus the element
        else if (key == KEY_ESCAPE && element->GetFocusMode() == FM_FOCUSABLE_DEFOCUSABLE)
            element->SetFocus(false);
        // If none of the special keys, pass the key to the focused element
        else
            element->OnKey(key, mouseButtons_, qualifiers_);
    }
}

void UI::HandleTextInput(const QString &txt)
{
    UIElement* element = focusElement_;
    if (element)
        element->OnTextInput(txt);
}

void UI::HandleBeginFrame(unsigned,float)
{
    // If have a cursor, and a drag is not going on, reset the cursor shape. Application logic that wants to apply
    // custom shapes can do it after this, but needs to do it each frame
    if (cursor_ && dragElementsCount_ == 0)
        cursor_->SetShape(CS_NORMAL);
}

void UI::HandleRenderUpdate(float)
{
    RenderUpdate();
}

void UI::HandleDropFile(const QString &name)
{
    // Sending the UI variant of the event only makes sense if the OS cursor is visible (not locked to window center)
    if (m_context->m_InputSystem->IsMouseVisible())
    {
        IntVector2 screenPos = m_context->m_InputSystem->GetMousePosition();
        screenPos.x_ = int(screenPos.x_ / uiScale_);
        screenPos.y_ = int(screenPos.y_ / uiScale_);
        UIElement* element = GetElementAt(screenPos);
        IntVector2 relativePos={0,0};
        if (element)
        {
            relativePos = element->ScreenToElement(screenPos);
        }
        g_uiSignals.dropFileUI.Emit(name,element,screenPos.x_,screenPos.y_,relativePos.x_,relativePos.y_);
    }
}

HashMap<WeakPtr<UIElement>, UI::DragData*>::iterator UI::DragElementErase(HashMap<WeakPtr<UIElement>, DragData*>::iterator i)
{
    // If running the engine frame in response to an event (re-entering UI frame logic) the dragElements_ may already be empty
    if (dragElements_.empty())
        return dragElements_.end();

    dragElementsConfirmed_.clear();

    DragData* dragData = MAP_VALUE(i);

    if (!dragData->dragBeginPending)
        --dragConfirmedCount_;
    i = dragElements_.erase(i);
    --dragElementsCount_;

    delete dragData;
    return i;
}

void UI::ProcessDragCancel()
{
    // How to tell difference between drag cancel and new selection on multi-touch?
    if (usingTouchInput_)
        return;

    IntVector2 cursorPos;
    bool cursorVisible;
    GetCursorPositionAndVisible(cursorPos, cursorVisible);

    for (auto i = dragElements_.begin(); i != dragElements_.end();)
    {
        WeakPtr<UIElement> dragElement = MAP_KEY(i);
        UI::DragData* dragData = MAP_VALUE(i);

        if (dragElement && dragElement->IsEnabled() && dragElement->IsVisible() && !dragData->dragBeginPending)
        {
            dragElement->OnDragCancel(dragElement->ScreenToElement(cursorPos), cursorPos, dragData->dragButtons, mouseButtons_, cursor_);
            IntVector2 relativePos = dragElement->ScreenToElement(cursorPos);
            dragElement->dragCancel.Emit(dragElement, cursorPos.x_, cursorPos.y_,
                                       relativePos.x_, relativePos.y_, dragData->dragButtons,
                                       dragData->numDragButtons);

            i = DragElementErase(i);
        }
        else
            ++i;
    }
}

IntVector2 UI::SumTouchPositions(UI::DragData* dragData, const IntVector2& oldSendPos)
{
    IntVector2 sendPos = oldSendPos;
    if (usingTouchInput_)
    {
        int buttons = dragData->dragButtons;
        dragData->sumPos = IntVector2::ZERO;
        Input* input = m_context->m_InputSystem.get();
        for (int i = 0; (1 << i) <= buttons; i++)
        {
            if ((1 << i) & buttons)
            {
                TouchState* ts = input->GetTouch(i);
                if (!ts)
                    break;
                IntVector2 pos = ts->position_;
                dragData->sumPos.x_ += (int)(pos.x_ / uiScale_);
                dragData->sumPos.y_ += (int)(pos.y_ / uiScale_);
            }
        }
        sendPos.x_ = dragData->sumPos.x_ / dragData->numDragButtons;
        sendPos.y_ = dragData->sumPos.y_ / dragData->numDragButtons;
    }
    return sendPos;
}

void UI::ResizeRootElement()
{
    IntVector2 effectiveSize = GetEffectiveRootElementSize();
    rootElement_->SetSize(effectiveSize);
    rootModalElement_->SetSize(effectiveSize);
}

IntVector2 UI::GetEffectiveRootElementSize(bool applyScale) const
{
    // Use a fake size in headless mode
    IntVector2 size = graphics_ ? IntVector2(graphics_->GetWidth(), graphics_->GetHeight()) : IntVector2(1024, 768);
    if (customSize_.x_ > 0 && customSize_.y_ > 0)
        size = customSize_;

    if (applyScale)
    {
        size.x_ = (int)((float)size.x_ / uiScale_ + 0.5f);
        size.y_ = (int)((float)size.y_ / uiScale_ + 0.5f);
    }

    return size;
}
void UI::SetRenderToTexture(UIComponent* component, bool enable)
{
    WeakPtr<UIComponent> weak(component);
    if (enable)
    {
        if (!renderToTexture_.contains(weak))
            renderToTexture_.insert(weak);
    }
    else
        renderToTexture_.remove(weak);
}
void RegisterUILibrary(Context* context)
{
    Font::RegisterObject(context);

    UIElement::RegisterObject(context);
    BorderImage::RegisterObject(context);
    Sprite::RegisterObject(context);
    Button::RegisterObject(context);
    CheckBox::RegisterObject(context);
    Cursor::RegisterObject(context);
    Text::RegisterObject(context);
    Text3D::RegisterObject(context);
    Window::RegisterObject(context);
    View3D::RegisterObject(context);
    LineEdit::RegisterObject(context);
    Slider::RegisterObject(context);
    ScrollBar::RegisterObject(context);
    ScrollView::RegisterObject(context);
    ListView::RegisterObject(context);
    Menu::RegisterObject(context);
    DropDownList::RegisterObject(context);
    FileSelector::RegisterObject(context);
    MessageBox::RegisterObject(context);
    ToolTip::RegisterObject(context);
    UIComponent::RegisterObject(context);
}

}
