// **********************************************************************
//
// Copyright (c) 2003-2009 ZeroC, Inc. All rights reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// ICE_LICENSE file included in this distribution.
//
// **********************************************************************

#include <Ice/Ice.h>
#include <NrvoI.h>

using namespace std;

class NrvotServer : public Ice::Application
{
public:

    virtual int run(int, char*[]);
};

int
main(int argc, char* argv[])
{
    NrvotServer app;
    return app.main(argc, argv, "config.server");
}

int
NrvotServer::run(int argc, char* argv[])
{
    if(argc > 1)
    {
        cerr << appName() << ": too many arguments" << endl;
        return EXIT_FAILURE;
    }

    Ice::ObjectAdapterPtr adapter = communicator()->createObjectAdapter("Nrvo");
    Demo::NrvoPtr servant = new NrvoI();
    adapter->add(servant, communicator()->stringToIdentity("nrvo"));
    adapter->activate();
    communicator()->waitForShutdown();
    return EXIT_SUCCESS;
}