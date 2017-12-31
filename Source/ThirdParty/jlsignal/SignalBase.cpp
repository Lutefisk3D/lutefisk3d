#include "Signal.h"

using namespace jl;

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

ScopedAllocator *SignalBase::defaultAllocator() { return s_pCommonAllocator; }
