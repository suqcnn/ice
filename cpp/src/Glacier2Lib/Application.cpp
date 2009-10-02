// **********************************************************************
//
// Copyright (c) 2003-2009 ZeroC, Inc. All rights reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// ICE_LICENSE file included in this distribution.
//
// **********************************************************************

#include <Glacier2/Application.h>
#include <IceUtil/IceUtil.h>
#include <IceUtil/ArgVector.h>

using namespace std;
using namespace Ice;
    
Ice::ObjectAdapterPtr Glacier2::Application::_adapter;
Glacier2::RouterPrx Glacier2::Application::_router;
Glacier2::SessionPrx Glacier2::Application::_session;
bool Glacier2::Application::_createdSession = false;

namespace
{

class SessionPingThread : virtual public IceUtil::Shared
{
    
public:
    
    virtual void done() = 0;
};
typedef IceUtil::Handle<SessionPingThread> SessionPingThreadPtr;

class AMI_Router_refreshSessionI : public Glacier2::AMI_Router_refreshSession
{

public:

    AMI_Router_refreshSessionI(Glacier2::Application* app, const SessionPingThreadPtr& pinger) :
        _app(app),
        _pinger(pinger)
    {
    }
    
    void
    ice_response()
    {
    }

    void
    ice_exception(const Ice::Exception& ex)
    {
        // Here the session has gone. The thread
        // terminates, and we notify the
        // application that the session has been
        // destroyed.
        _pinger->done();
        _app->sessionDestroyed();
    }

private:
        
    Glacier2::Application* _app;
    SessionPingThreadPtr _pinger;
};

class SessionPingThreadI : virtual public IceUtil::Thread, virtual public SessionPingThread
{
    
public:
    
    SessionPingThreadI(Glacier2::Application* app, const Glacier2::RouterPrx& router, long period) :
        _app(app),
        _router(router),
        _period(period),
        _done(false)
    {
    }

    void
    run()
    {
        IceUtil::Monitor<IceUtil::Mutex>::Lock lock(_monitor);
        while(true)
        {
            _router->refreshSession_async(new AMI_Router_refreshSessionI(_app, this));

            if(!_done)
            {
                _monitor.timedWait(IceUtil::Time::seconds((int)_period));
            }
            if(_done)
            {
                break;
            }
        }
    }

    void
    done()
    {
        IceUtil::Monitor<IceUtil::Mutex>::Lock lock(_monitor);
        if(!_done)
        {
            _done = true;
            _monitor.notify();
        }
    }

private:
    
    Glacier2::Application* _app;
    Glacier2::RouterPrx _router;
    long _period;
    bool _done;
    IceUtil::Monitor<IceUtil::Mutex> _monitor;
};
typedef IceUtil::Handle<SessionPingThreadI> SessionPingThreadIPtr;

}

string
Glacier2::RestartSessionException::ice_name() const
{
    return "RestartSessionException";
}

Exception*
Glacier2::RestartSessionException::ice_clone() const
{
    return new RestartSessionException(*this);
}

void
Glacier2::RestartSessionException::ice_throw() const
{
    throw *this;
}

Ice::ObjectAdapterPtr
Glacier2::Application::objectAdapter()
{
    if(!_adapter)
    {
        // TODO: Depending on the resolution of
        // http://bugzilla/bugzilla/show_bug.cgi?id=4264 the OA
        // name could be an empty string.
        _adapter = communicator()->createObjectAdapterWithRouter(IceUtil::generateUUID(), router());
        _adapter->activate();
    }
    return _adapter;
}

Ice::ObjectPrx
Glacier2::Application::addWithUUID(const Ice::ObjectPtr& servant)
{
    return objectAdapter()->add(servant, createCallbackIdentity(IceUtil::generateUUID()));
}

Ice::Identity
Glacier2::Application::createCallbackIdentity(const string& name)
{
    Ice::Identity id;
    id.name = name;
    id.category = categoryForClient();
    return id;
}

std::string
Glacier2::Application::categoryForClient()
{
    if(!_router)
    {
        SessionNotExistException ex;
        throw ex;
    }
    return router()->getCategoryForClient();
}

int
Glacier2::Application::doMain(int argc, char* argv[], const Ice::InitializationData& initData)
{
    // Set the default properties for all Glacier2 applications.
    initData.properties->setProperty("Ice.ACM.Client", "0");
    initData.properties->setProperty("Ice.RetryIntervals", "-1");

    bool restart;
    int ret = 0;
    do
    {
        // A copy of the initialization data and the string seq
        // needs to be passed to doMainInternal, as these can be
        // changed by the application.
        Ice::InitializationData id(initData);
        id.properties = id.properties->clone();
        Ice::StringSeq args = Ice::argsToStringSeq(argc, argv);

        restart = doMain(args, id, ret);
    }
    while(restart);
    return ret;
}

bool
Glacier2::Application::doMain(Ice::StringSeq& args, const Ice::InitializationData& initData, int& status)
{
    // Reset internal state variables from Ice.Application. The
    // remainder are reset at the end of this method.
    IceInternal::_callbackInProgress = false;
    IceInternal::_destroyed = false;
    IceInternal::_interrupted = false;

    bool restart = false;
    status = 0;

    SessionPingThreadIPtr ping;
    try
    {
        IceInternal::_communicator = Ice::initialize(args, initData);
        _router = Glacier2::RouterPrx::uncheckedCast(communicator()->getDefaultRouter());
        
        if(!_router)
        {
            Error out(getProcessLogger());
            out << IceInternal::_appName << ": no glacier2 router configured";
            status = 1;
        }
        else
        {
            //
            // The default is to destroy when a signal is received.
            //
            if(IceInternal::_signalPolicy == Ice::HandleSignals)
            {
                destroyOnInterrupt();
            }

            // If createSession throws, we're done.
            try
            {
                _session = createSession();
                _createdSession = true;
            }
            catch(const Ice::LocalException& ex)
            {
                Error out(getProcessLogger());
                out << IceInternal::_appName << ": " << ex;
                status = 1;
            }

            if(_createdSession)
            {
                ping = new SessionPingThreadI(this, _router, (long)_router->getSessionTimeout() / 2);
                ping->start();
                IceUtilInternal::ArgVector a(args);
                status = runWithSession(a.argc, a.argv);
            }
        }
    }
    // We want to restart on those exceptions which indicate a
    // break down in communications, but not those exceptions that
    // indicate a programming logic error (ie: marshal, protocol
    // failure, etc).
    catch(const RestartSessionException&)
    {
        restart = true;
    }
    catch(const Ice::ConnectionRefusedException& ex)
    {
        Error out(getProcessLogger());
        out << IceInternal::_appName << ": " << ex;
        restart = true;
    }
    catch(const Ice::ConnectionLostException& ex)
    {
        Error out(getProcessLogger());
        out << IceInternal::_appName << ": " << ex;
        restart = true;
    }
    catch(const Ice::UnknownLocalException& ex)
    {
        Error out(getProcessLogger());
        out << IceInternal::_appName << ": " << ex;
        restart = true;
    }
    catch(const Ice::RequestFailedException& ex)
    {
        Error out(getProcessLogger());
        out << IceInternal::_appName << ": " << ex;
        restart = true;
    }
    catch(const Ice::TimeoutException& ex)
    {
        Error out(getProcessLogger());
        out << IceInternal::_appName << ": " << ex;
        restart = true;
    }
    catch(const Ice::LocalException& ex)
    {
        Error out(getProcessLogger());
        out << IceInternal::_appName << ": " << ex;
        status = 1;
    }
    catch(const std::exception& ex)
    {
        Error out(getProcessLogger());
        out << IceInternal::_appName << ": std::exception " << ex;
        status = 1;
    }
    catch(const std::string& ex)
    {
        Error out(getProcessLogger());
        out << IceInternal::_appName << ": c++ exception " << ex;
        status = 1;
    }
    catch(const char* ex)
    {
        Error out(getProcessLogger());
        out << IceInternal::_appName << ": char* exception " << ex;
        status = 1;
    }
    catch(...)
    {
        Error out(getProcessLogger());
        out << IceInternal::_appName << ": unknow exception ";
        status = 1;
    }

    //
    // Don't want any new interrupt and at this point (post-run),
    // it would not make sense to release a held signal to run
    // shutdown or destroy.
    //
    if(IceInternal::_signalPolicy == HandleSignals)
    {
        ignoreInterrupt();
    }

    {
        IceUtil::Mutex::Lock lock(*IceInternal::mutex);
        while(IceInternal::_callbackInProgress)
        {
            IceInternal::_condVar->wait(lock);
        }
        if(IceInternal::_destroyed)
        {
            IceInternal::_communicator = 0;
        }
        else
        {
            IceInternal::_destroyed = true;
            //
            // And _communicator != 0, meaning will be destroyed
            // next, _destroyed = true also ensures that any
            // remaining callback won't do anything
            //
        }
        IceInternal::_application = 0;
    }

    if(!ping)
    {
        ping->done();
        while(true)
        {
            ping->getThreadControl().join();
            break;
        }
        ping = 0;
    }

    if(_createdSession && _router)
    {
        try
        {
            _router->destroySession();
        }
        catch(const Ice::ConnectionLostException&)
        {
            // Expected: the router closed the connection.
        }
        catch(const Glacier2::SessionNotExistException&)
        {
            // This can also occur.
        }
        catch(const exception& ex)
        {
            // Not expected.
            Error out(getProcessLogger());
            out << "unexpected exception when destroying the session " << ex;
        }
        _router = 0;
    }

    if(IceInternal::_communicator)
    {
        try
        {
            IceInternal::_communicator->destroy();
        }
        catch(const Ice::LocalException& ex)
        {
            Error out(getProcessLogger());
            out << IceInternal::_appName << ": " << ex;
            status = 1;
        }
        catch(const exception& ex)
        {
            Error out(getProcessLogger());
            out << "unknown exception " << ex;
            status = 1;
        }
        IceInternal::_communicator = 0;
    }


    // Reset internal state. We cannot reset the Application state
    // here, since _destroyed must remain true until we re-run
    // this method.
    _adapter = 0;
    _router = 0;
    _session = 0;
    _createdSession = false;

    return restart;
}