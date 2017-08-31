#pragma once

#include "DataHandle.h"

#include <stdint.h>
#include <vector>

template<class T>
class HandleManager
{
public:
    using Handle = DataHandle<T,20,20>;
private:
    struct IndexEntry {
        uint64_t index : 20;
        uint64_t generational_id : 20;
    };
protected:
    uint64_t _freelist_dequeue=Handle::invalidIdx();
    uint64_t _freelist_enqueue=Handle::invalidIdx();
    std::vector<IndexEntry> m_indices;
    std::vector<uint64_t> m_element_to_index;
    std::vector<T> m_elements;
private:
    uint64_t nextIndexEntry()
    {
        if(_freelist_dequeue==Handle::invalidIdx()) {
            // exhausted entries in the freelist, allocate new index entry
            m_indices.emplace_back(IndexEntry{Handle::invalidIdx(),0});
            return m_indices.size()-1;
        }
        uint64_t res = _freelist_dequeue;
        _freelist_dequeue = m_indices[_freelist_dequeue].index;
        return res;
    }
    void addIndexToFreelist(uint64_t idx) {
        if(_freelist_enqueue<m_indices.size()) {
            m_indices[_freelist_enqueue].index = idx;

        }
        _freelist_enqueue = idx;
        if(_freelist_dequeue>=m_indices.size())
            _freelist_dequeue=_freelist_enqueue;
    }
public:
    T &get(Handle id) {
        assert(id.index<m_indices.size());
        assert(id.generation==m_indices[id.index].generational_id);
        return m_elements[m_indices[id.index].index];
    }
    bool valid(Handle h) {
        return h.index<m_indices.size() && m_indices[h.index].generational_id==h.generation;
    }
    template<class ...Args>
    Handle add(Args... args) {
        uint64_t indexarray_entry = nextIndexEntry();
        IndexEntry &in = m_indices[indexarray_entry];
        in.index = m_elements.size();

        m_elements.emplace_back(args...);
        m_element_to_index.push_back(indexarray_entry);
        return Handle{indexarray_entry,in.generational_id,0};
    }
    template<class ...Args>
    std::vector<Handle> add_n(int n,Args... args) {
        std::vector<Handle> res;
        res.reserve(n);
        while(n--) {
            uint64_t indexarray_entry = nextIndexEntry();
            IndexEntry &in = m_indices[indexarray_entry];
            in.index = m_elements.size();

            m_elements.emplace_back(args...);
            m_element_to_index.push_back(indexarray_entry);
            res.emplace_back({indexarray_entry,in.generational_id,0});
        }
        return res;
    }
    void release(Handle id) {
        assert(valid(id));
        assert(!m_elements.empty());

        IndexEntry &in = m_indices[id.index];
        in.generational_id++;
        if(id.index!=m_elements.size()-1) {
            uint64_t index_loc = m_element_to_index.back();
            m_elements[in.index] = std::move(m_elements.back());
            m_element_to_index[in.index] = std::move(m_element_to_index.back());
            m_indices[index_loc].index=in.index;
        }
        m_elements.pop_back();
        m_element_to_index.pop_back();
        addIndexToFreelist(id.index);
        in.index = Handle::invalidIdx();
    }
};
