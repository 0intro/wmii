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

import sys
import traceback

from pyxp import fields
from pyxp.dial import dial
from threading import *
Condition = Condition().__class__

__all__ = 'Mux',

class Mux(object):
    def __init__(self, con, process, flush=None, mintag=0, maxtag=1<<16 - 1):
        self.queue = set()
        self.lock = RLock()
        self.rendez = Condition(self.lock)
        self.outlock = RLock()
        self.inlock = RLock()
        self.process = process
        self.flush = flush
        self.wait = {}
        self.free = set(range(mintag, maxtag))
        self.mintag = mintag
        self.maxtag = maxtag
        self.muxer = None

        self.async_mux = Queue(self.mux)
        self.async_dispatch = Queue(self.async_dispatch)

        if isinstance(con, basestring):
            con = dial(con)
        self.fd = con

        if self.fd is None:
            raise Exception("No connection")

    def mux(self, rpc):
        try:
            self.lock.acquire()
            rpc.waiting = True
            while self.muxer and self.muxer != rpc and rpc.data is None:
                rpc.wait()

            if rpc.data is None:
                assert not self.muxer or self.muxer is rpc
                self.muxer = rpc
                self.lock.release()
                try:
                    while rpc.data is None:
                        data = self.recv()
                        if data is None:
                            self.lock.acquire()
                            self.queue.remove(rpc)
                            raise Exception("unexpected eof")
                        self.dispatch(data)
                finally:
                    self.lock.acquire()
                    self.electmuxer()
        except Exception, e:
            traceback.print_exc(sys.stderr)
            if self.flush:
                self.flush(self, rpc.data)
            raise e
        finally:
            if self.lock._is_owned():
                self.lock.release()

        return rpc.data

    def rpc(self, dat, async=None):
        rpc = self.newrpc(dat, async)
        if async:
            self.async_mux.push(rpc)
        else:
            return self.mux(rpc)

    def async_dispatch(self, rpc):
        self.async_mux.pop(rpc)
        rpc.async(self, rpc.data)

    def electmuxer(self):
        for rpc in self.queue:
            if self.muxer != rpc and rpc.waiting:
                self.muxer = rpc
                rpc.notify()
                return
        self.muxer = None

    def dispatch(self, dat):
        tag = dat.tag
        rpc = None
        with self.lock:
            rpc = self.wait.get(tag, None)
            if rpc is None or rpc not in self.queue:
                #print "bad rpc tag: %u (no one waiting on it)" % dat.tag
                return
            self.puttag(rpc)
            self.queue.remove(rpc)
            rpc.dispatch(dat)

    def gettag(self, r):
        tag = 0

        while not self.free:
            self.rendez.wait()

        tag = self.free.pop()

        if tag in self.wait:
            raise Exception("nwait botch")

        self.wait[tag] = r

        r.tag = tag
        r.orig.tag = r.tag
        return r.tag

    def puttag(self, rpc):
        if rpc.tag in self.wait:
            del self.wait[rpc.tag]
        self.free.add(rpc.tag)
        self.rendez.notify()

    def send(self, dat):
        data = ''.join(dat.marshall())
        n = self.fd.send(data)
        return n == len(data)
    def recv(self):
        try:
            with self.inlock:
                data = self.fd.recv(4)
                if data:
                    len = fields.Int.decoders[4](data, 0)
                    data += self.fd.recv(len - 4)
                    return self.process(data)
        except Exception, e:
            traceback.print_exc(sys.stderr)
            return None

    def newrpc(self, dat, async=None):
        rpc = Rpc(self, dat, async)
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
    def __init__(self, mux, data, async=None):
        super(Rpc, self).__init__(mux.lock)
        self.mux = mux
        self.orig = data
        self.data = None
        self.async = async
        self.waiting = False

    def dispatch(self, data=None):
        self.data = data
        self.notify()
        if callable(self.async):
            self.mux.async_dispatch(self)

class Queue(Thread):
    def __init__(self, op):
        super(Queue, self).__init__()
        self.cond = Condition()
        self.op = op
        self.queue = []
        self.daemon = True

    def __call__(self, item):
        return self.push(item)

    def push(self, item):
        with self.cond:
            self.queue.append(item)
            if not self.is_alive():
                self.start()
            self.cond.notify()
    def pop(self, item):
        with self.cond:
            if item in self.queue:
                self.queue.remove(item)
                return True
            return False

    def run(self):
        self.cond.acquire()
        while True:
            while self.queue:
                item = self.queue.pop(0)
                self.cond.release()
                try:
                    self.op(item)
                except Exception, e:
                    traceback.print_exc(sys.stderr)
                self.cond.acquire()
            self.cond.wait()

# vim:se sts=4 sw=4 et:
