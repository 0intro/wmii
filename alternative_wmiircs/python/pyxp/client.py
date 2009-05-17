# Copyright (C) 2007 Kris Maglione
# See PERMISSIONS

import operator
import os
import re
import sys
from threading import *
import traceback

import pyxp
from pyxp import fcall, fields
from pyxp.mux import Mux
from pyxp.types import *

if os.environ.get('NAMESPACE', None):
    namespace = os.environ['NAMESPACE']
else:
    try:
        namespace = '/tmp/ns.%s.%s' % (
            os.environ['USER'], 
            re.sub(r'\.0$', '', os.environ['DISPLAY']))
    except Exception:
        pass
NAMESPACE = namespace

OREAD = 0x00
OWRITE = 0x01
ORDWR = 0x02
OEXEC = 0x03
OEXCL = 0x04
OTRUNC = 0x10
OREXEC = 0x20
ORCLOSE = 0x40
OAPPEND = 0x80

ROOT_FID = 0

class ProtocolException(Exception):
    pass
class RPCError(Exception):
    pass

class Client(object):
    ROOT_FID = 0

    def __enter__(self):
        return self
    def __exit__(self, *args):
        self.cleanup()

    def __init__(self, conn=None, namespace=None, root=None):
        if not conn and namespace:
            conn = 'unix!%s/%s' % (NAMESPACE, namespace)
        try:
            self.lastfid = ROOT_FID
            self.fids = []
            self.files = {}
            self.lock = RLock()

            def process(data):
                return fcall.Fcall.unmarshall(data)[1]
            self.mux = Mux(conn, process, maxtag=256)

            resp = self.dorpc(fcall.Tversion(version=pyxp.VERSION, msize=65535))
            if resp.version != pyxp.VERSION:
                raise ProtocolException, "Can't speak 9P version '%s'" % resp.version
            self.msize = resp.msize

            self.dorpc(fcall.Tattach(fid=ROOT_FID, afid=fcall.NO_FID,
                       uname=os.environ['USER'], aname=''))

            if root:
                path = self.splitpath(root)
                resp = self.dorpc(fcall.Twalk(fid=ROOT_FID,
                                              newfid=ROOT_FID,
                                              wname=path))
        except Exception, e:
            traceback.print_exc(sys.stdout)
            if getattr(self, 'mux', None):
                self.mux.fd.close()
            raise e

    def cleanup(self):
        try:
            for f in self.files:
                f.close()
        finally:
            self.mux.fd.close()
            self.mux = None

    def dorpc(self, req):
        resp = self.mux.rpc(req)
        if isinstance(resp, fcall.Rerror):
            raise RPCError, "RPC returned error (%s): %s" % (
                req.__class__.__name__, resp.ename)
        if req.type != resp.type ^ 1:
            raise ProtocolException, "Missmatched RPC message types: %s => %s" % (
                req.__class__.__name__, resp.__class__.__name__)
        return resp

    def splitpath(self, path):
        return [v for v in path.split('/') if v != '']

    def getfid(self):
        with self.lock:
            if len(self.fids):
                return self.fids.pop()
            else:
                self.lastfid += 1
                return self.lastfid
    def putfid(self, fid):
        with self.lock:
            self.files.pop(fid)
            self.fids.append(fid)

    def clunk(self, fid):
        try:
            self.dorpc(fcall.Tclunk(fid=fid))
        finally:
            self.putfid(fid)

    def walk(self, path):
        fid = self.getfid()
        ofid = ROOT_FID
        while True:
            self.dorpc(fcall.Twalk(fid=ofid, newfid=fid,
                                   wname=path[0:fcall.MAX_WELEM]))
            path = path[fcall.MAX_WELEM:]
            ofid = fid
            if len(path) == 0:
                break

        @apply
        class Res:
            def __enter__(self):
                return fid
            def __exit__(self, exc_type, exc_value, traceback):
                if exc_type:
                    self.clunk(fid)
        return Res

    def _open(self, path, mode, open):
        resp = None

        with self.walk(path) as nfid:
            fid = nfid
            resp = self.dorpc(open(fid))

        def cleanup():
            self.clunk(fid)
        file = File(self, resp, fid, mode, cleanup)
        self.files[fid] = file

        return file

    def open(self, path, mode = OREAD):
        path = self.splitpath(path)

        def open(fid):
            return fcall.Topen(fid=fid, mode=mode)
        return self._open(path, mode, open)

    def create(self, path, mode = OREAD, perm = 0):
        path = self.splitpath(path)
        name = path.pop()

        def open(fid):
            return fcall.Tcreate(fid=fid, mode=mode, name=name, perm=perm)
        return self._open(path, mode, open)

    def remove(self, path):
        path = self.splitpath(path)

        with self.walk(path) as fid:
            self.dorpc(fcall.Tremove(fid=fid))

    def stat(self, path):
        path = self.splitpath(path)

        try:
            with self.walk(path) as fid:
                resp = self.dorpc(fcall.Tstat(fid= fid))
                st = resp.stat()
                self.clunk(fid)
            return st
        except RPCError:
            return None

    def read(self, path, *args, **kwargs):
        with self.open(path) as f:
            return f.read(*args, **kwargs)
    def readlines(self, path, *args, **kwargs):
        with self.open(path) as f:
            for l in f.readlines(*args, **kwargs):
                yield l
    def readdir(self, path, *args, **kwargs):
        with self.open(path) as f:
            for s in f.readdir(*args, **kwargs):
                yield s
    def write(self, path, *args, **kwargs):
        with self.open(path, OWRITE) as f:
            return f.write(*args, **kwargs)

class File(object):

    def __enter__(self):
        return self
    def __exit__(self, *args):
        self.close()

    def __init__(self, client, fcall, fid, mode, cleanup):
        self.lock = RLock()
        self.client = client
        self.fid = fid
        self.cleanup = cleanup
        self.mode = mode
        self.iounit = fcall.iounit
        self.qid = fcall.qid

        self.offset = 0
        self.fd = None

    def dorpc(self, fcall):
        if hasattr(fcall, 'fid'):
            fcall.fid = self.fid
        return self.client.dorpc(fcall)

    def stat(self):
        resp = self.dorpc(fcall.Tstat())
        return resp.stat

    def read(self, count=None, offset=None, buf=''):
        if count is None:
            count = self.iounit
        res = []
        with self.lock:
            offs = self.offset
            if offset is not None:
                offs = offset
            while count > 0:
                n = min(count, self.iounit)
                count -= n

                resp = self.dorpc(fcall.Tread(offset=offs, count=n))
                data = resp.data

                offs += len(data)
                res.append(data)

                if len(data) < n:
                    break
            if offset is None:
                self.offset = offs
        res = ''.join(res)
        if len(res) > 0:
            return res
    def readlines(self):
        last = None
        while True:
            data = self.read()
            if not data:
                break
            lines = data.split('\n')
            for i in range(0, len(lines) - 1):
                yield lines[i]
            last = lines[-1]
        if last:
            yield last
    def write(self, data, offset=None):
        if offset is None:
            offset = self.offset
        off = 0
        with self.lock:
            offs = self.offset
            if offset is not None:
                offs = offset
            while off < len(data):
                n = min(len(data), self.iounit)

                resp = self.dorpc(fcall.Twrite(offset=offs,
                                               data=data[off:off+n]))
                off += resp.count
                offs += resp.count
                if resp.count < n:
                    break
            if offset is None:
                self.offset = offs
        return off
    def readdir(self):
        if not self.qid.type & Qid.QTDIR:
            raise Exception, "Can only call readdir on a directory"
        off = 0
        while True:
            data = self.read(self.iounit, off)
            if not data:
                break
            off += len(data)
            for s in Stat.unmarshall_list(data):
                yield s

    def close(self):
        try:
            if self.fd:
                self.fd_close()
        finally:
            self.cleanup()
            self.tg = None
            self.fid = None
            self.client = None
            self.qid = None

    def remove(self):
        try:
            self.dorpc(fcall.Tremove())
        finally:
            try:
                self.close()
            except Exception:
                pass

    def fd_close(self):
        try:
            self.fd.close()
        finally:
            self.fd = None

# vim:se sts=4 sw=4 et:
