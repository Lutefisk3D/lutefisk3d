#ifndef _JL_SIGNAL_BASE_H_
#define _JL_SIGNAL_BASE_H_

#include "Lutefisk3D/Core/Lutefisk3D.h"

#include "Utils.h"
#include "DoublyLinkedList.h"

namespace jl {

// Forward declarations
class SignalBase;

// Derive from this class to receive signals
class LUTEFISK3D_EXPORT SignalObserver
{
    // Public interface
public:
    virtual ~SignalObserver();

    void DisconnectAllSignals();
    void DisconnectSignal( SignalBase* pSignal );

    void SetConnectionAllocator( ScopedAllocator* pAllocator ) { m_oSignals.Init( pAllocator ); }
    unsigned CountSignalConnections() const { return m_oSignals.size(); }

    // Interface for child classes
protected:
    // Disallow instances of this class
    SignalObserver( ScopedAllocator* pAllocator ) { SetConnectionAllocator( pAllocator ); }

    // Hmm, a bit of a hack, but if a derived type caches pointers to signals,
    // we may need this
    virtual void OnSignalDisconnectInternal( SignalBase* pSignal ) { JL_UNUSED(pSignal); }

    // Private interface (to SignalBase)
private:
    friend class SignalBase;

    void OnSignalConnect( SignalBase* pSignal )
    {
#if defined( JL_ENABLE_ASSERT ) && ! defined ( JL_DISABLE_ASSERT ) && ! defined (NDEBUG)
        const bool bAdded =
#endif
        m_oSignals.Add( pSignal );
        JL_ASSERT( bAdded );
    }
    void OnSignalDisconnect( SignalBase* pSignal )
    {
        OnSignalDisconnectInternal( pSignal );
        for ( SignalList::iterator i = m_oSignals.begin(); i.isValid(); )
        {
            if ( *i == pSignal )
                m_oSignals.erase( i );
            else
                ++i;
        }
    }

    // Signal list
public:
    using SignalList = DoublyLinkedList<SignalBase*>;
    enum { eAllocationSize = sizeof(SignalList::Node) };

private:
    SignalList m_oSignals;
};

class LUTEFISK3D_EXPORT SignalBase
{
public:
    virtual ~SignalBase() = default;

    // Interface for derived signal classes
protected:
    // Disallow instances    of this class
    SignalBase() {}

    // Called on any connection to the observer.
    void NotifyObserverConnect( SignalObserver* pObserver ) { pObserver->OnSignalConnect(this); }
    ScopedAllocator* defaultAllocator();
    // Called when no more connections exist to the observer.
    void NotifyObserverDisconnect( SignalObserver* pObserver ) { pObserver->OnSignalDisconnect(this); }

    // Private interface (for SignalObserver)
private:
    friend class SignalObserver;
    virtual void OnObserverDisconnect( SignalObserver* pObserver ) = 0;

    // Global allocator
public:
    static void SetCommonConnectionAllocator( ScopedAllocator* pAllocator );
};

} // namespace jl

#endif // ! defined( _JL_SIGNAL_BASE_H_ )
