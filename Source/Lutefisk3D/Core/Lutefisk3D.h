#pragma once
#ifdef LUTEFISK3D_STATIC_DEFINE
#  define LUTEFISK3D_EXPORT
#  define LUTEFISK3D_NO_EXPORT
#else
#  ifndef LUTEFISK3D_EXPORT
#    ifdef LUTEFISK3D_EXPORTS
#if _WIN32
/* We are building this library */
#      define LUTEFISK3D_EXPORT __declspec(dllexport)
#else
#      define LUTEFISK3D_EXPORT __attribute__((visibility("default")))
#endif
#    else
#       if _WIN32
/* We are using this library */
#           define LUTEFISK3D_EXPORT __declspec(dllimport)
#       else
#           define LUTEFISK3D_EXPORT __attribute__((visibility("default")))
#       endif
#    endif
#  endif

#  ifndef LUTEFISK3D_NO_EXPORT
#if _WIN32
#    define LUTEFISK3D_NO_EXPORT
#else
#    define LUTEFISK3D_NO_EXPORT __attribute__((visibility("hidden")))
#endif
#  endif
#endif

#ifndef LUTEFISK3D_DEPRECATED
#if _WIN32
#  define LUTEFISK3D_DEPRECATED __declspec(deprecated)
#else
#  define LUTEFISK3D_DEPRECATED __attribute__ ((__deprecated__))
#endif
#endif

#ifndef LUTEFISK3D_DEPRECATED_EXPORT
#  define LUTEFISK3D_DEPRECATED_EXPORT LUTEFISK3D_EXPORT LUTEFISK3D_DEPRECATED
#endif

#ifndef LUTEFISK3D_DEPRECATED_NO_EXPORT
#  define LUTEFISK3D_DEPRECATED_NO_EXPORT LUTEFISK3D_NO_EXPORT LUTEFISK3D_DEPRECATED
#endif

#ifdef LUTEFISK_PRECOMPILED_HEADERS
// semi stable includes
#include "Lutefisk3D/Container/RefCounted.h"
#include "Lutefisk3D/Container/SmallVector.h"
#include "Lutefisk3D/Container/sherwood_map.hpp"
#include "Lutefisk3D/Container/Str.h"
#include "Lutefisk3D/Container/HashMap.h"
#include "Lutefisk3D/Container/Ptr.h"
#include "Lutefisk3D/Math/Rect.h"
#include "Lutefisk3D/Math/Vector2.h"
#include "Lutefisk3D/Math/Vector3.h"
#include "Lutefisk3D/Math/Vector4.h"
#include "Lutefisk3D/Math/Matrix3x4.h"
#include "Lutefisk3D/Math/StringHash.h"
#include "Lutefisk3D/Math/Color.h"
// Not so stable but used in many places
#include "Lutefisk3D/Core/Variant.h"

#include "Lutefisk3D/Engine/jlsignal/SignalBase.h"
#include "Lutefisk3D/Engine/jlsignal/Signal.h"

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
