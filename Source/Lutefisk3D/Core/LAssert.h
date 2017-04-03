#pragma once

#define ASSERT  Q_ASSERT


#define ERROR(cond,text) \
    if(!(cond)) \
        URHO3D_LOGERROR(text);\
    ASSERT(cond);

