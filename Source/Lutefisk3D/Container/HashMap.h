#pragma once
#include "../Container/Vector.h"
//#include <mct/hash-map.hpp>
#include "../ThirdParty/sherwood_map/sherwood_map.hpp"
#ifdef USE_QT_HASHMAP
#include <QtCore/QHash>
#else
#include <unordered_map>
#include <unordered_set>
#include <set>
#endif
namespace Urho3D {
#ifdef USE_QT_HASHMAP
#define MAP_VALUE(i) (i.value())
#define MAP_KEY(i) (i.key())
#define ELEMENT_VALUE(e) e
#define ELEMENT_KEY(e) static_assert(false,"QHash cannot access key from dereferenced iterator");
template<typename T,typename U>
using HashMap = ::QHash<T,U> ;
#else
#define MAP_VALUE(i) (i->second)
#define MAP_KEY(i) (i->first)
#define ELEMENT_VALUE(e) e.second
#define ELEMENT_KEY(e) e.first

template <typename T,typename U>
class HashMap : public std::unordered_map<T,U> {
    typedef std::unordered_map<T,U> ParentClass;
public:
    HashMap() {}
    typedef typename ParentClass::iterator iterator;
    typedef typename ParentClass::const_iterator const_iterator;

    constexpr bool contains(const T &v) const { return this->find(v)!=this->cend();}
    /// Erase an element if found.
    bool remove(const T& value)
    {
        iterator i = this->find(value);
        if (i == this->end())
            return false;
        this->erase(i);
        return true;
    }
    constexpr bool isEmpty() const { return this->empty(); }
    Vector<T> keys() const {
        Vector<T> result;
        result.reserve(this->size());
        for(const std::pair<const T,U> v : *this) {
            result.push_back(v.first);
        }
        return result;
    }
    Vector<U> values() const {
        Vector<U> result;
        result.reserve(this->size());
        for(const std::pair<const T,U> v : *this) {
            result.push_back(v.second);
        }
        return result;
    }
};
template <typename T,typename U>
class FasterHashMap : public sherwood_map<T,U> {
    typedef sherwood_map<T,U> ParentClass;
public:
    FasterHashMap() {}
    typedef typename ParentClass::iterator iterator;
    typedef typename ParentClass::const_iterator const_iterator;

    constexpr bool contains(const T &v) const { return this->find(v)!=this->cend();}
    /// Erase an element if found.
    bool remove(const T& value)
    {
        iterator i = this->find(value);
        if (i == this->end())
            return false;
        this->erase(i);
        return true;
    }
    constexpr bool isEmpty() const { return this->empty(); }
    Vector<T> keys() const {
        Vector<T> result;
        result.reserve(this->size());
        for(const std::pair<const T,U> v : *this) {
            result.push_back(v.first);
        }
        return result;
    }
    Vector<U> values() const {
        Vector<U> result;
        result.reserve(this->size());
        for(const std::pair<const T,U> v : *this) {
            result.push_back(v.second);
        }
        return result;
    }
};

template <typename T>
class HashSet : public std::unordered_set<T> {
public:
    typedef typename std::unordered_set<T>::iterator iterator;
    typedef typename std::unordered_set<T>::const_iterator const_iterator;

    constexpr bool contains(const T &v) const { return this->find(v)!=this->cend();}
    /// Erase an element if found.
    bool remove(const T& value)
    {
        iterator i = this->find(value);
        if (i == this->end())
            return false;
        this->erase(i);
        return true;
    }
    constexpr bool isEmpty() const { return this->empty(); }
};
template <typename T,int N>
class SmallMembershipSet {
    PODVectorN<T,N> members;
public:
    constexpr bool contains(const T &v) const {
        for(const T&elem : members)
            if(v==elem)
                return true;
        return false;
    }
    /// Erase an element if found.
    void clear()
    {
        members.resize(0);
    }
    void insert(const T &val) {
        if(!members.empty() && contains(val))
            return;
        members.push_back(val);
    }
    bool empty() const { return members.empty(); }

};
#endif
}

