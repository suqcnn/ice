# **********************************************************************
#
# Copyright (c) 2003-2004 ZeroC, Inc. All rights reserved.
#
# This copy of Ice is licensed to you under the terms described in the
# ICE_LICENSE file included in this distribution.
#
# **********************************************************************

import sys, Ice, Test, ServerPrivate, _Top

class TestI(_Top.Test):
    def __init__(self, adapter):
        self._adapter = adapter

    def shutdown(self, current=None):
        self._adapter.getCommunicator().shutdown()

    def baseAsBase(self, current=None):
        b = _Top.Base()
        b.b = "Base.b"
        raise b

    def unknownDerivedAsBase(self, current=None):
        d = _Top.UnknownDerived()
        d.b = "UnknownDerived.b"
        d.ud = "UnknownDerived.ud"
        raise d

    def knownDerivedAsBase(self, current=None):
        d = _Top.KnownDerived()
        d.b = "KnownDerived.b"
        d.kd = "KnownDerived.kd"
        raise d

    def knownDerivedAsKnownDerived(self, current=None):
        d = _Top.KnownDerived()
        d.b = "KnownDerived.b"
        d.kd = "KnownDerived.kd"
        raise d

    def unknownIntermediateAsBase(self, current=None):
        ui = _Top.UnknownIntermediate()
        ui.b = "UnknownIntermediate.b"
        ui.ui = "UnknownIntermediate.ui"
        raise ui

    def knownIntermediateAsBase(self, current=None):
        ki = _Top.KnownIntermediate()
        ki.b = "KnownIntermediate.b"
        ki.ki = "KnownIntermediate.ki"
        raise ki

    def knownMostDerivedAsBase(self, current=None):
        kmd = _Top.KnownMostDerived()
        kmd.b = "KnownMostDerived.b"
        kmd.ki = "KnownMostDerived.ki"
        kmd.kmd = "KnownMostDerived.kmd"
        raise kmd

    def knownIntermediateAsKnownIntermediate(self, current=None):
        ki = _Top.KnownIntermediate()
        ki.b = "KnownIntermediate.b"
        ki.ki = "KnownIntermediate.ki"
        raise ki

    def knownMostDerivedAsKnownIntermediate(self, current=None):
        kmd = _Top.KnownMostDerived()
        kmd.b = "KnownMostDerived.b"
        kmd.ki = "KnownMostDerived.ki"
        kmd.kmd = "KnownMostDerived.kmd"
        raise kmd

    def knownMostDerivedAsKnownMostDerived(self, current=None):
        kmd = _Top.KnownMostDerived()
        kmd.b = "KnownMostDerived.b"
        kmd.ki = "KnownMostDerived.ki"
        kmd.kmd = "KnownMostDerived.kmd"
        raise kmd

    def unknownMostDerived1AsBase(self, current=None):
        umd1 = _Top.UnknownMostDerived1()
        umd1.b = "UnknownMostDerived1.b"
        umd1.ki = "UnknownMostDerived1.ki"
        umd1.umd1 = "UnknownMostDerived1.umd1"
        raise umd1

    def unknownMostDerived1AsKnownIntermediate(self, current=None):
        umd1 = _Top.UnknownMostDerived1()
        umd1.b = "UnknownMostDerived1.b"
        umd1.ki = "UnknownMostDerived1.ki"
        umd1.umd1 = "UnknownMostDerived1.umd1"
        raise umd1

    def unknownMostDerived2AsBase(self, current=None):
        umd2 = _Top.UnknownMostDerived2()
        umd2.b = "UnknownMostDerived2.b"
        umd2.ui = "UnknownMostDerived2.ui"
        umd2.umd2 = "UnknownMostDerived2.umd2"
        raise umd2

def run(args, communicator):
    properties = communicator.getProperties()
    properties.setProperty("Ice.Warn.Dispatch", "0")
    properties.setProperty("TestAdapter.Endpoints", "default -p 12345 -t 10000")
    adapter = communicator.createObjectAdapter("TestAdapter")
    object = TestI(adapter)
    adapter.add(object, Ice.stringToIdentity("Test"))
    adapter.activate()
    communicator.waitForShutdown()
    return True

try:
    communicator = Ice.initialize(sys.argv)
    status = run(sys.argv, communicator)
except Ice.Exception, ex:
    print ex
    status = False

if communicator:
    try:
        communicator.destroy()
    except Ice.Exception, ex:
        print ex
        status = False

sys.exit(not status)
