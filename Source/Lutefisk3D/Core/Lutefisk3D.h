#pragma once

#if defined _WIN32 || defined __CYGWIN__
  #ifdef LUTEFISK_AS_DLL
  #ifdef BUILDING_DLL
    #if defined __GNUC__ || defined __clang__
      #define URHO3D_API __attribute__ ((dllexport))
    #else
      #define URHO3D_API __declspec(dllexport) // Note: actually gcc seems to also supports this syntax.
    #endif
  #else
    #if defined __GNUC__ || defined __clang__
      #define URHO3D_API __attribute__ ((dllimport))
    #else
      #define URHO3D_API __declspec(dllimport) // Note: actually gcc seems to also supports this syntax.
    #endif
  #endif
  #define URHO3D_NO_EXPORT
  #else
	#define URHO3D_API 
	#define URHO3D_NO_EXPORT
  #endif
#else
  #if __GNUC__ >= 4 || defined __clang__
    #define URHO3D_API __attribute__ ((visibility ("default")))
    #define URHO3D_NO_EXPORT  __attribute__ ((visibility ("hidden")))
  #else
    #define URHO3D_API
    #define URHO3D_NO_EXPORT
  #endif
#endif
