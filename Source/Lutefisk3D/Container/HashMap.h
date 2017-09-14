#pragma once

#include "sherwood_map.hpp"

#ifdef USE_QT_HASHMAP
#include <QtCore/QHash>
#else
#include <unordered_map>
#include <unordered_set>
#endif

#include <vector>
#include "SmallVector.h"

namespace std {
template<class T>
struct hash< T *const > {
    size_t operator()(const T *const key) const {
        return hash<const void *>()((const void *)key);
    }
};

template<class T,class U>
struct hash< pair<T, U> > {
    size_t operator()(const pair<T, U> & key) const {
        return hash<T>()(key.first) ^ (hash<U>()(key.second)<<15);
    }
};
}

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


template <typename T,int N>
class PODVectorN : public lls::SmallVector<T,N> {
public:
    typedef typename lls::SmallVector<T,N>::iterator iterator;
    typedef typename lls::SmallVector<T,N>::const_iterator const_iterator;
    PODVectorN() {}
    PODVectorN(int sz) : lls::SmallVector<T,N>(sz) {}
    constexpr bool contains(const T &v) const { return find(v)!=this->end();}
    iterator find(const T &v) { return std::find(this->begin(),this->end(),v);}
    const_iterator find(const T &v) const { return std::find(this->begin(),this->end(),v);}
    /// Erase an element if found.
    bool remove(const T& value)
    {
        iterator i = find(value);
        if (i == this->end())
            return false;
        this->erase(i);
        return true;
    }
};

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
    std::vector<T> keys() const {
        std::vector<T> result;
        result.reserve(this->size());
        for(const std::pair<const T,U> v : *this) {
            result.push_back(v.first);
        }
        return result;
    }
    std::vector<U> values() const {
        std::vector<U> result;
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
    std::vector<T> keys() const {
        std::vector<T> result;
        result.reserve(this->size());
        for(const std::pair<const T,U> v : *this) {
            result.push_back(v.first);
        }
        return result;
    }
    std::vector<U> values() const {
        std::vector<U> result;
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
    using iterator=typename PODVectorN<T,N>::iterator;
    bool contains(const T &v) const {
        for(const T & elem : members)
            if(v==elem)
                return true;
        return false;
    }
    /// Erase an element if found.
    void remove(const T&v) {
        members.remove(v);
    }
    iterator erase(iterator i ) {
        return members.erase(i);
    }
    void clear()
    {
        members.clear();
    }
    void insert(const T &val) {
        if(!members.empty() && contains(val))
            return;
        members.push_back(val);
    }
    bool empty() const { return members.empty(); }
    size_t size() const { return members.size(); }
    typename PODVectorN<T,N>::iterator begin() { return members.begin(); }
    typename PODVectorN<T,N>::iterator end() { return members.end(); }
};
#endif
}

