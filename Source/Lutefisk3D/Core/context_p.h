#pragma once
#include "Lutefisk3D.h"
#include "Lutefisk3D/Container/RefCounted.h"
#include <vector>
namespace Urho3D
{
class Object;

class LUTEFISK3D_EXPORT EventReceiverGroup : public RefCounted
{
public:
    void BeginSendEvent();
    void EndSendEvent();
    void Add(Object *object);
    void Remove(Object *object);
    std::vector<Object *> receivers_;

private:
    unsigned inSend_ = 0;
    bool     dirty_  = false;
};
}
