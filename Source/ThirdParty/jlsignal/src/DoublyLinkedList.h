#ifndef _JL_DOUBLY_LINKED_LIST_H_
#define _JL_DOUBLY_LINKED_LIST_H_

#include "Utils.h"
#include "ScopedAllocator.h"

namespace jl {

/**
 * Your basic doubly-linked list, with link nodes allocated outside of the
 * contained type.
 * Requires a ScopedAllocator for node allocation.
 */
template<typename _T>
class DoublyLinkedList
{
public:
    
    //////////////////
    // Data structures
    //////////////////
    
    typedef _T TObject;
    
    struct Node
    {
        TObject object;
        Node* prev;
        Node* next;
    };
    
    class iterator
    {
    public:
        constexpr iterator(DoublyLinkedList<TObject>*l,Node *n) : m_pList(l),m_pCurrent(n) {}
        TObject& operator*()
        {
            return m_pCurrent->object;
        }
        
        TObject& operator->()
        {
            return m_pCurrent->object;
        }
        
        iterator& operator++()
        {
            JL_ASSERT( m_pCurrent );
            
            if ( m_pCurrent )
            {
                m_pCurrent = m_pCurrent->next;
            }
            
            return *this;
        }
        
        bool operator==( const iterator& other ) const
        {
            return m_pList == other.m_pList && m_pCurrent == other.m_pCurrent;
        }
        bool operator!=(const iterator &other) const { return m_pCurrent!=other.m_pCurrent || m_pList!=other.m_pList; }
        bool isValid() const
        {
            return m_pList != nullptr && m_pCurrent != nullptr;
        }
        
    private:
        friend class DoublyLinkedList<TObject>;
        DoublyLinkedList<TObject>* m_pList;
        Node* m_pCurrent;
    };
    
    class const_iterator
    {
    public:
        constexpr const_iterator(const DoublyLinkedList<TObject>*l,const Node *n) : m_pList(l),m_pCurrent(n) {}

        const TObject& operator*()
        {
            return m_pCurrent->object;
        }
        
        const TObject& operator->()
        {
            return m_pCurrent->object;
        }
        const_iterator& operator++()
        {
            JL_ASSERT( m_pCurrent );
            
            if ( m_pCurrent )
            {
                m_pCurrent = m_pCurrent->next;
            }
            
            return *this;
        }
        bool operator!=(const const_iterator &other) const { return m_pCurrent!=other.m_pCurrent || m_pList!=other.m_pList; }
        bool operator==( const const_iterator& other ) const
        {
            return m_pList == other.m_pList && m_pCurrent == other.m_pCurrent;
        }
        
        bool isValid() const
        {
            return m_pList != nullptr && m_pCurrent != nullptr;
        }
        
    private:
        friend class DoublyLinkedList<TObject>;
        const DoublyLinkedList<TObject>* m_pList;
        const Node* m_pCurrent;
    };
    
    /////////////////////
    // Internal interface
    /////////////////////
    
private:
    Node* CreateNode()
    {
        Node* pNode = (Node*)m_pNodeAllocator->Alloc( sizeof(Node) );
        
        if ( ! pNode )
        {
            return nullptr;
        }
        
        // Initialize node pointers
        pNode->next = nullptr;
        pNode->prev = nullptr;
        
        return pNode;
    }
    
    ///////////////////
    // Public interface
    ///////////////////
    
public:
    
    DoublyLinkedList()
    {
        m_pHead = nullptr;
        m_pTail = nullptr;
        m_nObjectCount = 0;
        m_pNodeAllocator = nullptr;
    }
    
    ~DoublyLinkedList()
    {
        clear();
    }
    
    void Init( ScopedAllocator* pNodeAllocator )
    {
        m_pNodeAllocator = pNodeAllocator;
    }
    
    // Returns true if the object was successfully added
    Node* Add( const TObject& object )
    {
        // Create a node to contain the object.
        Node* pNode = CreateNode();
        JL_ASSERT( pNode );
        
        if ( ! pNode )
        {
            return nullptr;
        }
        
        // Place the object in the node.
        pNode->object = object;
        
        // Add node to the end of the list.
        if ( m_pTail )
        {
            m_pTail->next = pNode;
            pNode->prev = m_pTail;
            m_pTail = pNode;
        }
        else
        {
            JL_ASSERT( ! m_pHead );
            m_pHead = pNode;
            m_pTail = pNode;
        }
        
        // Update object count
        m_nObjectCount += 1;
        
        return pNode;
    }
    // Returns true if the object at the iterator position was successfully removed
    // This will advance the iterator if the removal was successful.
    bool erase( iterator& i )
    {
        JL_ASSERT( i.m_pList == this );
        if ( i.m_pList != this )
        {
            return false;
        }
        
        Node* pNext = i.m_pCurrent->next;
        if ( RemoveNode(i.m_pCurrent) )
        {
            i.m_pCurrent = pNext;
            return true;
        }
        
        return false;
    }    
    unsigned size() const
    {
        return m_nObjectCount;
    }
    
    void clear()
    {
        Node* pCurrent = nullptr;
        Node* pNext = m_pHead;
        
        while ( pNext )
        {
            pCurrent = pNext;
            pNext = pCurrent->next;
            
            m_pNodeAllocator->Free( pCurrent );
        }
        
        m_pHead = nullptr;
        m_pTail = nullptr;
        m_nObjectCount = 0;
    }
    
    // Iterator interface
    iterator begin() { return {this,m_pHead}; }
    iterator end() { return {this,nullptr}; }

    const_iterator begin() const {  return {this,m_pHead}; }
    const_iterator end() const { return {this,nullptr}; }

private:
    bool RemoveNode( Node* pNode )
    {
        JL_ASSERT( m_nObjectCount );
        if ( ! m_nObjectCount )
        {
            return false;
        }
        
        // Re-assign head/tail pointers, if necessary
        if ( m_pHead == pNode )
        {
            m_pHead = pNode->next;
        }
        
        if ( m_pTail == pNode )
        {
            m_pTail = pNode->prev;
        }
        
        // Reassign links between previous/next buckets
        if ( pNode->prev )
        {
            pNode->prev->next = pNode->next;
        }
        
        if ( pNode->next )
        {
            pNode->next->prev = pNode->prev;
        }
        
        // Update object count
        m_nObjectCount -= 1;
        
        // Free node object
        m_pNodeAllocator->Free( pNode );
        
        return true;
    }
    
    Node* m_pHead;
    Node* m_pTail;
    unsigned m_nObjectCount;
    ScopedAllocator* m_pNodeAllocator;
};

} // namespace jl

#endif // ! defined( _JL_DOUBLY_LINKED_LIST_H_ )
