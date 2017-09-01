#pragma once

#include "FastFunc.h"
#include "Utils.h"
#include "SignalBase.h"
#include <utility>
/**
 * The content of the following classes (Signal0 -> Signal8) is identical,
 * except for the number of parameters specified in the Connect() and Emit()
 * functions.
 */

#ifdef JL_SIGNAL_ENABLE_LOGSPAM
#include <stdio.h>
#define JL_SIGNAL_LOG( ... ) printf( __VA_ARGS__ )
#else
#define JL_SIGNAL_LOG( ... )
#endif

#if defined( JL_SIGNAL_ASSERT_ON_DOUBLE_CONNECT )
#define JL_SIGNAL_DOUBLE_CONNECTED_FUNCTION_ASSERT( _function ) JL_ASSERT( ! isConnected(_function) )
#define JL_SIGNAL_DOUBLE_CONNECTED_INSTANCE_METHOD_ASSERT( _obj, _method ) JL_ASSERT( ! isConnected(_obj, _method) )
#else
#define JL_SIGNAL_DOUBLE_CONNECTED_FUNCTION_ASSERT( _function )
#define JL_SIGNAL_DOUBLE_CONNECTED_INSTANCE_METHOD_ASSERT( _obj, _method )
#endif

namespace jl {

/**
 * Signal1: signals with 1 argument
 */
template< class ... Types >
class Signal final : public SignalBase
{
public:
    //typedef fastdelegate::FastDelegate1< _P1 > Delegate;
    typedef ssvu::FastFunc< void(Types...) > Delegate;
    struct Connection
    {
        Delegate d;
        SignalObserver* pObserver;
    };

    typedef DoublyLinkedList<Connection> ConnectionList;

    enum { eAllocationSize = sizeof(typename ConnectionList::Node) };

private:
    typedef typename ConnectionList::iterator ConnectionIter;
    typedef typename ConnectionList::const_iterator ConnectionConstIter;

    ConnectionList m_oConnections;

public:
    Signal() { SetAllocator( s_pCommonAllocator ); }
    Signal( ScopedAllocator* pAllocator ) { SetAllocator( pAllocator ); }

    virtual ~Signal()
    {
        JL_SIGNAL_LOG( "Destroying Signal1 %p\n", this );
        DisconnectAll();
    }

    void SetAllocator( ScopedAllocator* pAllocator ) { m_oConnections.Init( pAllocator ); }
    unsigned CountConnections() const override { return m_oConnections.size(); }

    // Connects non-instance functions.
    void Connect( void (*fpFunction)(Types...) )
    {
        JL_SIGNAL_DOUBLE_CONNECTED_FUNCTION_ASSERT( fpFunction );
        JL_SIGNAL_LOG( "Signal1 %p connection to non-instance function %p", this, BruteForceCast<void*>(fpFunction) );

        Connection c = { Delegate(fpFunction), NULL };
        JL_CHECKED_CALL( m_oConnections.Add( c ) );
    }

    // Connects instance methods. Class X should be equal to Y, or an ancestor type.
    template< class X, class Y >
    void Connect( Y* pObject, void (X::*fpMethod)(Types...) )
    {
        if ( ! pObject )
        {
            return;
        }

        JL_SIGNAL_DOUBLE_CONNECTED_INSTANCE_METHOD_ASSERT( pObject, fpMethod );
        SignalObserver* pObserver = static_cast<SignalObserver*>( pObject );
        JL_SIGNAL_LOG( "Signal1 %p connecting to Observer %p (object %p, method %p)\n", this, pObserver, pObject, BruteForceCast<void*>(fpMethod) );

        Connection c = { Delegate(pObject, fpMethod), pObserver };
        JL_CHECKED_CALL( m_oConnections.Add( c ) );
        NotifyObserverConnect( pObserver );
    }

    // Connects const instance methods. Class X should be equal to Y, or an ancestor type.
    template< class X, class Y >
    void Connect( Y* pObject, void (X::*fpMethod)(Types...) const )
    {
        if ( ! pObject )
            return;

        JL_SIGNAL_DOUBLE_CONNECTED_INSTANCE_METHOD_ASSERT( pObject, fpMethod );
        SignalObserver* pObserver = static_cast<SignalObserver*>( pObject );
        JL_SIGNAL_LOG( "Signal1 %p connecting to Observer %p (object %p, method %p)\n", this, pObserver, pObject, BruteForceCast<void*>(fpMethod) );

        Connection c = { Delegate(pObject, fpMethod), pObserver };
        JL_CHECKED_CALL( m_oConnections.Add( c ) );
        NotifyObserverConnect( pObserver );
    }

    // Returns true if the given observer and non-instance function are connected to this signal.
    bool isConnected( void (*fpFunction)(Types...) ) const
    {
        return isConnected( Delegate(fpFunction) );
    }

    // Returns true if the given observer and instance method are connected to this signal.
    template< class X, class Y >
    bool isConnected( Y* pObject, void (X::*fpMethod)(Types...) ) const
    {
        return isConnected( Delegate(pObject, fpMethod) );
    }

    // Returns true if the given observer and const instance method are connected to this signal.
    template< class X, class Y >
    bool isConnected( Y* pObject, void (X::*fpMethod)(Types...) const ) const
    {
        return isConnected( Delegate(pObject, fpMethod) );
    }

    void Emit( Types... p1 ) const
    {
        for ( const Connection &conn : m_oConnections )
        {
            conn.d( std::forward<Types>(p1)... );
        }
    }

    //void operator()( Types... p1 ) const { Emit( p1... ); }

    // Disconnects a non-instance method.
    void Disconnect( void (*fpFunction)(Types...) )
    {
        JL_SIGNAL_LOG( "Signal1 %p removing connections to non-instance method %p\n", this, BruteForceCast<void*>(fpFunction) );
        const Delegate d(fpFunction);

        for ( ConnectionIter i = m_oConnections.begin(); i.isValid(); )
        {
            if ( (*i).d == d )
            {
                JL_ASSERT( (*i).pObserver == NULL );
                JL_SIGNAL_LOG( "\tRemoving connection...\n" );
                m_oConnections.erase( i );
            }
            else
            {
                ++i;
            }
        }
    }

    // Disconnects instance methods. Class X should be equal to Y, or an ancestor type.
    template< class X, class Y >
    void Disconnect( Y* pObject, void (X::*fpMethod)(Types...) )
    {
        if ( ! pObject )
        {
            return;
        }

        SignalObserver* pObserver = static_cast<SignalObserver*>( pObject );
        JL_SIGNAL_LOG( "Signal1 %p removing connections to Observer %p, instance method (object %p, method %p)\n", this, pObserver, pObject, BruteForceCast<void*>(fpMethod) );
        DisconnectObserverDelegate( pObserver, Delegate(pObject, fpMethod) );
    }

    // Disconnects const instance methods. Class X should be equal to Y, or an ancestor type.
    template< class X, class Y >
    void Disconnect( Y* pObject, void (X::*fpMethod)(Types...) const )
    {
        if ( ! pObject )
        {
            return;
        }

        SignalObserver* pObserver = static_cast<SignalObserver*>( pObject );
        JL_SIGNAL_LOG( "Signal1 %p removing connections to Observer %p, const instance method (object %p, method %p)\n", this, pObserver, pObject, BruteForceCast<void*>(fpMethod) );
        DisconnectObserverDelegate( pObserver, Delegate(pObject, fpMethod) );
    }

    // Disconnects all connected instance methods from a single observer. Calls NotifyObserverDisconnect()
    // if any disconnections are made.
    void Disconnect( SignalObserver* pObserver )
    {
        if ( ! pObserver )
        {
            return;
        }

        JL_SIGNAL_LOG( "Signal1 %p removing all connections to Observer %p\n", this, pObserver );
        unsigned nDisconnections = 0;

        for ( ConnectionIter i = m_oConnections.begin(); i.isValid(); )
        {
            if ( (*i).pObserver == pObserver )
            {
                JL_SIGNAL_LOG( "\tRemoving connection to observer\n" );
                m_oConnections.erase( i ); // advances iterator
                ++nDisconnections;
            }
            else
            {
                ++i;
            }
        }

        if ( nDisconnections > 0 )
        {
            NotifyObserverDisconnect( pObserver );
        }
    }

    void DisconnectAll()
    {
        JL_SIGNAL_LOG( "Signal1 %p disconnecting all observers\n", this );

        for ( ConnectionIter i = m_oConnections.begin(); i.isValid(); ++i )
        {
            SignalObserver* pObserver = (*i).pObserver;

            //HACK - call this each time we encounter a valid observer pointer. This means
            // this will be called repeatedly for observers that are connected multiple times
            // to this signal.
            if ( pObserver )
            {
                NotifyObserverDisconnect( pObserver );
            }
        }

        m_oConnections.clear();
    }

private:
    bool isConnected( const Delegate& d ) const
    {
        for ( ConnectionConstIter i = m_oConnections.begin(); i.isValid(); ++i )
        {
            if ( (*i).d == d )
            {
                return true;
            }
        }

        return false;
    }

    // Disconnects a specific slot on an observer. Calls NotifyObserverDisconnect() if
    // the observer is completely disconnected from this signal.
    void DisconnectObserverDelegate( SignalObserver* pObserver, const Delegate& d )
    {
        unsigned nDisconnections = 0; // number of disconnections. This is 0 or 1 unless you connected the same slot twice.
        unsigned nObserverConnectionCount = 0; // number of times the observer is connected to this signal

        for ( ConnectionIter i = m_oConnections.begin(); i.isValid(); )
        {
            if ( (*i).d == d )
            {
                JL_ASSERT( (*i).pObserver == pObserver );
                JL_SIGNAL_LOG( "\tRemoving connection...\n" );
                m_oConnections.erase( i ); // advances iterator
                ++nDisconnections;
            }
            else
            {
                if ( (*i).pObserver == pObserver )
                {
                    ++nObserverConnectionCount;
                }

                ++i;
            }
        }

        if ( nDisconnections > 0 && nObserverConnectionCount == 0 )
        {
            JL_SIGNAL_LOG( "\tCompletely disconnected observer %p!", pObserver );
            NotifyObserverDisconnect( pObserver );
        }
    }

    void OnObserverDisconnect( SignalObserver* pObserver ) override
    {
        JL_SIGNAL_LOG( "Signal1 %p received disconnect message from observer %p\n", this, pObserver );

        for ( ConnectionIter i = m_oConnections.begin(); i.isValid(); )
        {
            if ( (*i).pObserver == pObserver )
            {
                JL_SIGNAL_LOG( "\tRemoving connection to observer\n" );
                m_oConnections.erase( i );
            }
            else
            {
                ++i;
            }
        }
    }
};
template <>
class Signal<void> final : public SignalBase {
public:
    typedef ssvu::FastFunc< void(void) > Delegate;
    struct Connection
    {
        Delegate d;
        SignalObserver* pObserver;
    };

    typedef DoublyLinkedList<Connection> ConnectionList;

    enum { eAllocationSize = sizeof(typename ConnectionList::Node) };

private:
    typedef typename ConnectionList::iterator ConnectionIter;
    typedef typename ConnectionList::const_iterator ConnectionConstIter;

    ConnectionList m_oConnections;

public:
    Signal() { SetAllocator( s_pCommonAllocator ); }
    Signal( ScopedAllocator* pAllocator ) { SetAllocator( pAllocator ); }

    virtual ~Signal()
    {
        JL_SIGNAL_LOG( "Destroying Signal1 %p\n", this );
        DisconnectAll();
    }

    void SetAllocator( ScopedAllocator* pAllocator ) { m_oConnections.Init( pAllocator ); }
    unsigned CountConnections() const { return m_oConnections.size(); }

    // Connects non-instance functions.
    void Connect( void (*fpFunction)(void) )
    {
        JL_SIGNAL_DOUBLE_CONNECTED_FUNCTION_ASSERT( fpFunction );
        JL_SIGNAL_LOG( "Signal1 %p connection to non-instance function %p", this, BruteForceCast<void*>(fpFunction) );

        Connection c = { Delegate(fpFunction), nullptr };
        JL_CHECKED_CALL( m_oConnections.Add( c ) );
    }

    // Connects instance methods. Class X should be equal to Y, or an ancestor type.
    template< class X, class Y >
    void Connect( Y* pObject, void (X::*fpMethod)(void) )
    {
        if ( ! pObject )
            return;

        JL_SIGNAL_DOUBLE_CONNECTED_INSTANCE_METHOD_ASSERT( pObject, fpMethod );
        SignalObserver* pObserver = static_cast<SignalObserver*>( pObject );
        JL_SIGNAL_LOG( "Signal1 %p connecting to Observer %p (object %p, method %p)\n", this, pObserver, pObject, BruteForceCast<void*>(fpMethod) );

        Connection c = { Delegate(pObject, fpMethod), pObserver };
        JL_CHECKED_CALL( m_oConnections.Add( c ) );
        NotifyObserverConnect( pObserver );
    }

    // Connects const instance methods. Class X should be equal to Y, or an ancestor type.
    template< class X, class Y >
    void Connect( Y* pObject, void (X::*fpMethod)(void) const )
    {
        if ( ! pObject )
            return;

        JL_SIGNAL_DOUBLE_CONNECTED_INSTANCE_METHOD_ASSERT( pObject, fpMethod );
        SignalObserver* pObserver = static_cast<SignalObserver*>( pObject );
        JL_SIGNAL_LOG( "Signal1 %p connecting to Observer %p (object %p, method %p)\n", this, pObserver, pObject, BruteForceCast<void*>(fpMethod) );

        Connection c = { Delegate(pObject, fpMethod), pObserver };
        JL_CHECKED_CALL( m_oConnections.Add( c ) );
        NotifyObserverConnect( pObserver );
    }

    // Returns true if the given observer and non-instance function are connected to this signal.
    bool isConnected( void (*fpFunction)(void) ) const
    {
        return isConnected( Delegate(fpFunction) );
    }

    // Returns true if the given observer and instance method are connected to this signal.
    template< class X, class Y >
    bool isConnected( Y* pObject, void (X::*fpMethod)(void) ) const
    {
        return isConnected( Delegate(pObject, fpMethod) );
    }

    // Returns true if the given observer and const instance method are connected to this signal.
    template< class X, class Y >
    bool isConnected( Y* pObject, void (X::*fpMethod)(void) const ) const
    {
        return isConnected( Delegate(pObject, fpMethod) );
    }

    void Emit( void ) const
    {
        for ( const Connection &conn : m_oConnections )
            conn.d();
    }

    void operator()( void ) const { Emit( ); }

    // Disconnects a non-instance method.
    void Disconnect( void (*fpFunction)(void) )
    {
        JL_SIGNAL_LOG( "Signal1 %p removing connections to non-instance method %p\n", this, BruteForceCast<void*>(fpFunction) );
        const Delegate d(fpFunction);

        for ( ConnectionIter i = m_oConnections.begin(); i.isValid(); )
        {
            if ( (*i).d == d )
            {
                JL_ASSERT( (*i).pObserver == NULL );
                JL_SIGNAL_LOG( "\tRemoving connection...\n" );
                m_oConnections.erase( i );
            }
            else
            {
                ++i;
            }
        }
    }

    // Disconnects instance methods. Class X should be equal to Y, or an ancestor type.
    template< class X, class Y >
    void Disconnect( Y* pObject, void (X::*fpMethod)(void) )
    {
        if ( ! pObject )
        {
            return;
        }

        SignalObserver* pObserver = static_cast<SignalObserver*>( pObject );
        JL_SIGNAL_LOG( "Signal1 %p removing connections to Observer %p, instance method (object %p, method %p)\n", this, pObserver, pObject, BruteForceCast<void*>(fpMethod) );
        DisconnectObserverDelegate( pObserver, Delegate(pObject, fpMethod) );
    }

    // Disconnects const instance methods. Class X should be equal to Y, or an ancestor type.
    template< class X, class Y >
    void Disconnect( Y* pObject, void (X::*fpMethod)(void) const )
    {
        if ( ! pObject )
        {
            return;
        }

        SignalObserver* pObserver = static_cast<SignalObserver*>( pObject );
        JL_SIGNAL_LOG( "Signal1 %p removing connections to Observer %p, const instance method (object %p, method %p)\n", this, pObserver, pObject, BruteForceCast<void*>(fpMethod) );
        DisconnectObserverDelegate( pObserver, Delegate(pObject, fpMethod) );
    }

    // Disconnects all connected instance methods from a single observer. Calls NotifyObserverDisconnect()
    // if any disconnections are made.
    void Disconnect( SignalObserver* pObserver )
    {
        if ( ! pObserver )
        {
            return;
        }

        JL_SIGNAL_LOG( "Signal1 %p removing all connections to Observer %p\n", this, pObserver );
        unsigned nDisconnections = 0;

        for ( ConnectionIter i = m_oConnections.begin(); i.isValid(); )
        {
            if ( (*i).pObserver == pObserver )
            {
                JL_SIGNAL_LOG( "\tRemoving connection to observer\n" );
                m_oConnections.erase( i ); // advances iterator
                ++nDisconnections;
            }
            else
            {
                ++i;
            }
        }

        if ( nDisconnections > 0 )
        {
            NotifyObserverDisconnect( pObserver );
        }
    }

    void DisconnectAll()
    {
        JL_SIGNAL_LOG( "Signal1 %p disconnecting all observers\n", this );

        for ( ConnectionIter i = m_oConnections.begin(); i.isValid(); ++i )
        {
            SignalObserver* pObserver = (*i).pObserver;

            //HACK - call this each time we encounter a valid observer pointer. This means
            // this will be called repeatedly for observers that are connected multiple times
            // to this signal.
            if ( pObserver )
            {
                NotifyObserverDisconnect( pObserver );
            }
        }

        m_oConnections.clear();
    }

private:
    bool isConnected( const Delegate& d ) const
    {
        for ( ConnectionConstIter i = m_oConnections.begin(); i.isValid(); ++i )
        {
            if ( (*i).d == d )
            {
                return true;
            }
        }

        return false;
    }

    // Disconnects a specific slot on an observer. Calls NotifyObserverDisconnect() if
    // the observer is completely disconnected from this signal.
    void DisconnectObserverDelegate( SignalObserver* pObserver, const Delegate& d )
    {
        unsigned nDisconnections = 0; // number of disconnections. This is 0 or 1 unless you connected the same slot twice.
        unsigned nObserverConnectionCount = 0; // number of times the observer is connected to this signal

        for ( ConnectionIter i = m_oConnections.begin(); i.isValid(); )
        {
            if ( (*i).d == d )
            {
                JL_ASSERT( (*i).pObserver == pObserver );
                JL_SIGNAL_LOG( "\tRemoving connection...\n" );
                m_oConnections.erase( i ); // advances iterator
                ++nDisconnections;
            }
            else
            {
                if ( (*i).pObserver == pObserver )
                {
                    ++nObserverConnectionCount;
                }

                ++i;
            }
        }

        if ( nDisconnections > 0 && nObserverConnectionCount == 0 )
        {
            JL_SIGNAL_LOG( "\tCompletely disconnected observer %p!", pObserver );
            NotifyObserverDisconnect( pObserver );
        }
    }

    void OnObserverDisconnect( SignalObserver* pObserver )
    {
        JL_SIGNAL_LOG( "Signal1 %p received disconnect message from observer %p\n", this, pObserver );

        for ( ConnectionIter i = m_oConnections.begin(); i.isValid(); )
        {
            if ( (*i).pObserver == pObserver )
            {
                JL_SIGNAL_LOG( "\tRemoving connection to observer\n" );
                m_oConnections.erase( i );
            }
            else
            {
                ++i;
            }
        }
    }

};

} // namespace jl
