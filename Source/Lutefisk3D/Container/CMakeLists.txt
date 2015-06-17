set(INCLUDES
Allocator.h
ArrayPtr.h
Hash.h
HashMap.h
HashTable.h
LinkedList.h
List.h
Pair.h
Ptr.h
RefCounted.h
SmallOrig.h
SmallVector.h
Str.h
Swap.h
VectorBase.h
Vector.h
)
set(SOURCE
    Allocator.cpp
)

add_library(Lutefisk_Containers OBJECT ${SOURCE} ${INCLUDE})