# Derived from libmux, available in Plan 9 under /sys/src/libmux
# under the following terms:
#
# Copyright (C) 2003-2006 Russ Cox, Massachusetts Institute of Technology
# 
# Permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
# 
# The above copyright notice and this permission notice shall be
# included in all copies or substantial portions of the Software.

from pyxp import fields
from pyxp.dial import dial
from threading import *
Condition = Condition().__class__

__all__ = 'Mux',

class Mux(object):
    def __init__(self, con, process, mintag=0, maxtag=1<<16 - 1):
        self.queue = set()
        self.lock = RLock()
        self.rendez = Condition(self.lock)
        self.outlock = RLock()
        self.inlock = RLock()
        self.process = process
        self.wait = {}
        self.free = set(range(mintag, maxtag))
        self.mintag = mintag
        self.maxtag = maxtag
        self.muxer = None

        if isinstance(con, basestring):
            con = dial(con)
        self.fd = con

        if self.fd is None:
            raise Exception("No connection")

    def rpc(self, dat):
        r = self.newrpc(dat)

        try:
            self.lock.acquire()
            while self.muxer and self.muxer != r and r.data is None:
                r.wait()

            if r.data is None:
                if self.muxer and self.muxer != r:
                    self.fail()
                self.muxer = r
                self.lock.release()
                try:
                    while r.data is None:
                        data = self.recv()
                        if data is None:
                            self.lock.acquire()
                            self.queue.remove(r)
                            raise Exception("unexpected eof")
                        self.dispatch(data)
                    self.lock.acquire()
                finally:
                    self.electmuxer()
            self.puttag(r)
        except Exception, e:
            import sys
            import traceback
            traceback.print_exc(sys.stdout)
            print e
        finally:
            if self.lock._is_owned():
                self.lock.release()
        return r.data

    def electmuxer(self):
        for rpc in self.queue:
            if self.muxer != rpc and rpc.async == False:
                self.muxer = rpc
                rpc.notify()
                return
        self.muxer = None

    def dispatch(self, dat):
        tag = dat.tag - self.mintag
        r = None
        with self.lock:
            r = self.wait.get(tag, None)
            if r is None or r not in self.queue:
                print "bad rpc tag: %u (no one waiting on it)" % dat.tag
                return
            self.queue.remove(r)
            r.data = dat
            r.notify()

    def gettag(self, r):
        tag = 0

        while not self.free:
            self.rendez.wait()

        tag = self.free.pop()

        if tag in self.wait:
            raise Exception("nwait botch")

        self.wait[tag] = r

        r.tag = tag
        r.data.tag = r.tag
        r.data = None
        return r.tag

    def puttag(self, r):
        t = r.tag
        if self.wait.get(t, None) != r:
            self.fail()
        del self.wait[t]
        self.free.add(t)
        self.rendez.notify()

    def send(self, dat):
        data = ''.join(dat.marshall())
        n = self.fd.send(data)
        return n == len(data)
    def recv(self):
        data = self.fd.recv(4)
        if data:
            len = fields.Int.decoders[4](data, 0)
            data += self.fd.recv(len - 4)
            return self.process(data)

    def fail():
        raise Exception()

    def newrpc(self, dat):
        rpc = Rpc(self, dat)
        tag = None

        with self.lock:
            self.gettag(rpc)
            self.queue.add(rpc)

        if rpc.tag >= 0 and self.send(dat):
            return rpc

        with self.lock:
            self.queue.remove(rpc)
            self.puttag(rpc)

class Rpc(Condition):
    def __init__(self, mux, data):
        super(Rpc, self).__init__(mux.lock)
        self.mux = mux
        self.data = data
        self.waiting = True
        self.async = False

# vim:se sts=4 sw=4 et:
