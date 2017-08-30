set(INCLUDES
    BorderImage.h
    Button.h
    CheckBox.h
    Cursor.h
    DropDownList.h
    FileSelector.h
    FontFaceBitmap.h
    FontFaceFreeType.h
    FontFace.h
    Font.h
    LineEdit.h
    ListView.h
    Menu.h
    MessageBox.h
    ScrollBar.h
    ScrollView.h
    Slider.h
    Sprite.h
    Text3D.h
    Text.h
    ToolTip.h
    UIBatch.h
    UIComponent.h
    UIElement.h
    UIEvents.h
    UI.h
    View3D.h
    Window.h
)
set(UI_COMPONENTS
    Console.h
    Console.cpp
    DebugHud.h
    DebugHud.cpp
)
set(SOURCE
    BorderImage.cpp
    Button.cpp
    CheckBox.cpp
    Cursor.cpp
    DropDownList.cpp
    FileSelector.cpp
    Font.cpp
    FontFaceBitmap.cpp
    FontFace.cpp
    FontFaceFreeType.cpp
    LineEdit.cpp
    ListView.cpp
    Menu.cpp
    MessageBox.cpp
    ScrollBar.cpp
    ScrollView.cpp
    Slider.cpp
    Sprite.cpp
    Text3D.cpp
    Text.cpp
    ToolTip.cpp
    UIComponent.cpp
    UIBatch.cpp
    UI.cpp
    UIElement.cpp
    UIEvents.cpp
    View3D.cpp
    Window.cpp

)

INCLUDE_DIRECTORIES(../../ThirdParty/FreeType/include)

if(LUTEFISK3D_AS_DLLS)
    add_library(Lutefisk3D_UI SHARED ${SOURCE} ${INCLUDES} ${UI_COMPONENTS})
    target_link_libraries(Lutefisk3D_UI Lutefisk3D FreeType )
    qt5_use_modules(Lutefisk3D_UI Core)
else()
    add_library(Lutefisk3D_UI STATIC ${SOURCE} ${INCLUDES} ${UI_COMPONENTS})
    target_include_directories(Lutefisk3D_UI PRIVATE $<TARGET_PROPERTY:Qt5::Core,INTERFACE_INCLUDE_DIRECTORIES>)
    target_compile_definitions(Lutefisk3D_UI PRIVATE $<TARGET_PROPERTY:Qt5::Core,INTERFACE_COMPILE_DEFINITIONS>)
    set_property(TARGET Lutefisk3D_UI PROPERTY INTERFACE_LINK_LIBRARIES FreeType)
endif()

add_dependencies(Lutefisk3D_UI FreeType)
set_property(TARGET Lutefisk3D_UI PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET Lutefisk3D_UI APPEND PROPERTY INTERFACE_LINK_LIBRARIES Lutefisk3D_2D)

if(UNIT_TESTING)
endif()
