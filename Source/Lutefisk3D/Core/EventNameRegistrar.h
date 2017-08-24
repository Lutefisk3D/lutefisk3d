#ifndef EVENTNAMEREGISTRAR_H
#define EVENTNAMEREGISTRAR_H
#include "Lutefisk3D/Core/Lutefisk3D.h"
#include "Lutefisk3D/Math/StringHash.h"
#include "Lutefisk3D/Container/HashMap.h"
namespace Urho3D {
/// Register event names.
struct URHO3D_API EventNameRegistrar
{
    /// Register an event name for hash reverse mapping.
    static StringHash RegisterEventName(const char* eventName);
    /// Return Event name or empty string if not found.
    static const char * GetEventName(StringHash eventID) {
        static char buf[256];
        snprintf(buf,256,"Hash[%08x]",eventID.Value());
        return buf;
    }
    /// Return Event name map.
    static HashMap<StringHash, QString>& GetEventNameMap();
};
}
#endif // EVENTNAMEREGISTRAR_H
