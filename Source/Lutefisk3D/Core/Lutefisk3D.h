#pragma once
#include "Lutefisk3D/lutefisk3d_export.h"
#ifdef LUTEFISK_PRECOMPILED_HEADERS
// semi stable includes
#include "Lutefisk3D/Container/RefCounted.h"
#include "Lutefisk3D/Container/SmallVector.h"
#include "Lutefisk3D/Container/sherwood_map.hpp"
#include "Lutefisk3D/Container/Str.h"
#include "Lutefisk3D/Container/HashMap.h"
#include "Lutefisk3D/Container/Ptr.h"
#include "Lutefisk3D/Math/Rect.h"
#include "Lutefisk3D/Math/Matrix3x4.h"
#include "Lutefisk3D/Math/StringHash.h"
#include "Lutefisk3D/Math/Color.h"
// Not so stable but used in many places
#include "Lutefisk3D/Core/Variant.h"

#include <jlsignal/SignalBase.h>
#include <jlsignal/Signal.h>

#include <QtCore/QString>
#include <QtCore/QSet>
#include <QtCore/QStringList>
#include <vector>
#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <cstddef>
#include <functional>
#include <array>
#include <cassert>
#endif
