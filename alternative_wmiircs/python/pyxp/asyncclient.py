from pyxp import client, fcall
from pyxp.client import *
from functools import wraps

def send(iter, val, default=None):
    try:
        return iter.send(val)
    except StopIteration:
        return default

def awithfile(fn):
    @wraps(fn)
    def wrapper(self, path, *args, **kwargs):
        gen = fn(self, *args, **kwargs)
        callback, fail, mode = next(gen)
        def cont(file):
            send(gen, file)
        self.aopen(path, cont, fail=fail or callback, mode=mode)
    return wrapper

def requestchain(fn):
    @wraps(fn)
    def wrapper(self, *args, **kwargs):
        gen = fn(self, *args, **kwargs)
        callback, fail = next(gen)

        def cont(val):
            data = gen.send(val)
            if isinstance(data, fcall.Fcall):
                self._dorpc(data, cont, fail or callback)
            else:
                Client.respond(callback, data)
        cont(None)
    return wrapper

class Client(client.Client):
    ROOT_FID = 0

    def _awalk(fn):
        @wraps(fn)
        @requestchain
        def wrapper(self, *args, **kwargs):
            gen = fn(self, *args, **kwargs)
            path, callback, fail = next(gen)

            path = self._splitpath(path)
            fid  = self._getfid()
            ofid = ROOT_FID

            def fail_(resp, exc, tb):
                if ofid != ROOT_FID:
                    self._aclunk(fid)
                self.respond(fail or callback, resp, exc, tb)
            yield callback, fail_

            while path:
                wname = path[:fcall.MAX_WELEM]
                path = path[fcall.MAX_WELEM:]

                resp = yield fcall.Twalk(fid=ofid, newfid=fid, wname=wname)
                ofid = fid

            resp = fid
            while resp is not None:
                resp = yield send(gen, resp)

        return wrapper

    _file = property(lambda self: File)

    @_awalk
    def _aopen(self, path, mode, fcall, callback, fail=None, origpath=None):
        path = self._splitpath(path)

        fcall.fid = yield path, callback, fail
        resp = yield fcall
        yield self._file(self, origpath or '/'.join(path), resp, fcall.fid, mode,
                         cleanup=lambda: self._clunk(fcall.fid))

    def aopen(self, path, callback=True, fail=None, mode=OREAD):
        assert callable(callback)
        self._aopen(path, mode, fcall.Topen(mode=mode), callback, fail)

    def acreate(self, path, callback=True, fail=None, mode=OREAD, perm=0):
        path = self._splitpath(path)
        name = path.pop()

        self._aopen(path, mode,
                    fcall.Tcreate(mode=mode, name=name, perm=perm),
                    callback if callable(callback) else lambda resp: resp and resp.close(),
                    fail, origpath='/'.join(path + [name]))

    @_awalk
    def aremove(self, path, callback=True, fail=None):
        yield fcall.Tremove(fid=(yield path, callback, fail))

    @_awalk
    def astat(self, path, callback, fail=None):
        resp = yield fcall.Tstat(fid=(yield path, callback, fail))
        yield resp.stat

    @awithfile
    def aread(self, callback, fail=None, count=None, offset=None, buf=''):
        file = yield callback, fail, OREAD
        file.aread(callback, fail, count, offset, buf)

    @awithfile
    def awrite(self, data, callback=True, fail=None, offset=None):
        file = yield callback, fail, OWRITE
        file.awrite(data, callback, fail, offset)

    @awithfile
    def areadlines(self, callback):
        file = yield callback, fail, OREAD
        file.areadlines(callback)

class File(client.File):

    @requestchain
    def stat(self, callback, fail=None):
        yield callback, fail
        resp = yield fcall.Tstat()
        yield resp.stat

    @requestchain
    def aread(self, callback, fail=None, count=None, offset=None, buf=''):
        yield callback, fail

        setoffset = offset is None
        if count is None:
            count = self.iounit
        if offset is  None:
            offset = self.offset

        res = []
        while count > 0:
            n = min(count, self.iounit)
            count -= n
            resp = yield fcall.Tread(offset=offset, count=n)
            res.append(resp.data)
            offset += len(resp.data)
            if len(resp.data) == 0:
                break

        if setoffset:
            self.offset = offset
        yield ''.join(res)

    def areadlines(self, callback):
        class ctxt:
            last = None
        def cont(data, exc, tb):
            res = True
            if data:
                lines = data.split('\n')
                if ctxt.last:
                    lines[0] = ctxt.last + lines[0]
                for i in range(0, len(lines) - 1):
                    res = callback(lines[i])
                    if res is False:
                        return
                ctxt.last = lines[-1]
                self.aread(cont)
            else:
                if ctxt.last:
                    callback(ctxt.last)
                callback(None)
        self.aread(cont)

    @requestchain
    def awrite(self, data, callback=True, fail=None, offset=None):
        yield callback, fail
        setoffset = offset is None
        if offset is None:
            offset = self.offset

        off = 0
        while off < len(data):
            n = min(len(data), self.iounit)
            resp = yield fcall.Twrite(offset=offset, data=data[off:off+n])
            off    += resp.count
            offset += resp.count

        if setoffset:
            self.offset = offset
        yield off

    @requestchain
    def aremove(self, callback=True, fail=None):
        yield callback, fail
        yield fcall.Tremove()
        self.close()
        yield True

# vim:se sts=4 sw=4 et:
