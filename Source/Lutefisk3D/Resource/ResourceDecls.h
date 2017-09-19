#pragma once
#include <Lutefisk3D/Container/Ptr.h>
namespace Urho3D {
class JSONFile;
extern template class SharedPtr<JSONFile>;
class XMLFile;
extern template class SharedPtr<XMLFile>;
class Resource;
extern template class SharedPtr<Resource>;

};
