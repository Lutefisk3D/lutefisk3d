#include "Signal.h"

using namespace jl;

ScopedAllocator* jl::SignalObserver::s_pCommonAllocator = NULL;
ScopedAllocator* SignalBase::s_pCommonAllocator = NULL;

jl::SignalObserver::~SignalObserver()
{
    DisconnectAllSignals();
}

void jl::SignalObserver::DisconnectSignal( SignalBase* pSignal )
{
    for ( SignalBase* sig : m_oSignals)
    {
        if ( sig == pSignal )
        {
            JL_SIGNAL_LOG( "Observer %p disconnecting signal %p", this, pSignal );
            sig->OnObserverDisconnect( this );
            break;
        }
    }    
}

void jl::SignalObserver::DisconnectAllSignals()
{
    JL_SIGNAL_LOG( "Observer %p disconnecting all signals\n", this );
    
    for ( SignalBase* sig : m_oSignals )
        sig->OnObserverDisconnect( this );
    
    m_oSignals.clear();
}

void jl::SignalObserver::OnSignalConnect( SignalBase* pSignal )
{
    JL_SIGNAL_LOG( "\tObserver %p received connection message from signal %p\n", this, pSignal );
    const bool bAdded = m_oSignals.Add( pSignal );
    JL_ASSERT( bAdded );
}

void jl::SignalObserver::OnSignalDisconnect( SignalBase* pSignal )
{
    JL_SIGNAL_LOG( "\tObserver %p received disconnect message from signal %p\n", this, pSignal );
    
    OnSignalDisconnectInternal( pSignal );
    
    for ( SignalList::iterator i = m_oSignals.begin(); i.isValid(); )
    {
        if ( *i == pSignal )
        {
            JL_SIGNAL_LOG( "\t\tRemoving connection to signal %p\n", pSignal );
            m_oSignals.erase( i );
        }
        else
        {
            ++i;
        }
    }
}
