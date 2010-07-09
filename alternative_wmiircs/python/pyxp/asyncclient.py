from pyxp import client, fcall
from pyxp.client import *
from functools import wraps

def awithfile(*oargs, **okwargs):
    def wrapper(fn):
        @wraps(fn)
        def next(self, path, *args, **kwargs):
            def next(file, exc, tb):
                fn(self, (file, exc, tb), *args, **kwargs)
            self.aopen(path, next, *oargs, **okwargs)
        return next
    return wrapper

def wrap_callback(fn, file):
    def callback(data, exc, tb):
        file.close()
        Client.respond(fn, data, exc, tb)
    return callback

class Client(client.Client):
    ROOT_FID = 0

    def _awalk(self, path, callback, fail=None):
        path = self._splitpath(path)
        ctxt = dict(path=path, fid=self._getfid(), ofid=ROOT_FID)

        def next(resp=None, exc=None, tb=None):
            if exc and ctxt['ofid'] != ROOT_FID:
                self._aclunk(ctxt['fid'])
                ctxt['fid'] = None

            if not ctxt['path'] and resp or exc:
                return self.respond(fail if exc and fail else callback,
                                    ctxt['fid'], exc, tb)

            wname = ctxt['path'][:fcall.MAX_WELEM]
            ofid = ctxt['ofid']
            ctxt['path'] = ctxt['path'][fcall.MAX_WELEM:]
            if resp:
                ctxt['ofid'] = ctxt['fid']

            self._dorpc(fcall.Twalk(fid=ofid, newfid=ctxt['fid'], wname=wname),
                        next)
        next()

    _file = property(lambda self: File)
    def _aopen(self, path, mode, fcall, callback, fail=None, origpath=None):
        path = self._splitpath(path)

        def next(fid, exc, tb):
            def next(resp, exc, tb):
                file = self._file(self, origpath or '/'.join(path), resp, fid, mode,
                                  cleanup=lambda: self._clunk(fid))
                self.respond(callback, file)
            fcall.fid = fid
            self._dorpc(fcall, next, fail or callback)
        self._awalk(path, next, fail or callback)

    def aopen(self, path, callback=True, fail=None, mode=OREAD):
        assert callable(callback)
        return self._aopen(path, mode, fcall.Topen(mode=mode),
                           callback, fail)

    def acreate(self, path, callback=True, fail=None, mode=OREAD, perm=0):
        path = self._splitpath(path)
        name = path.pop()

        if not callable(callback):
            callback = lambda resp: resp and resp.close()

        return self._aopen(path, mode, fcall.Tcreate(mode=mode, name=name, perm=perm),
                           callback, fail, origpath='/'.join(path + [name]))

    def aremove(self, path, callback=True, fail=None):
        def next(fid):
            self._dorpc(fcall.Tremove(fid=fid), callback, fail)
        self._awalk(path, next, fail or callback)

    def astat(self, path, callback, fail=None):
        def next(fid):
            def next(resp):
                self.respond(callback, resp.stat)
            self._dorpc(fcall.Tstat(fid=fid), next, fail or callback)
        self._awalk(self, next, fail or callback)

    @awithfile()
    def aread(self, (file, exc, tb), callback, *args, **kwargs):
        if exc:
            self.respond(callback, file, exc, tb)
        else:
            file.aread(wrap_callback(callback, file), *args, **kwargs)

    @awithfile(mode=OWRITE)
    def awrite(self, (file, exc, tb), data, callback=True, *args, **kwargs):
        if exc:
            self.respond(callback, file, exc, tb)
        else:
            file.awrite(data, wrap_callback(callback, file), *args, **kwargs)

    @awithfile()
    def areadlines(self, (file, exc, tb), fn):
        def callback(resp):
            if resp is None:
                file.close()
            if fn(resp) is False:
                file.close()
                return False
        if exc:
            callback(None)
        else:
            file.sreadlines(callback)

class File(client.File):

    def stat(self, callback):
        def next(resp, exc, tb):
            Client.respond(callback, resp.stat, exc, tb)
        resp = self._dorpc(fcall.Tstat(), next, callback)

    def aread(self, callback, fail=None, count=None, offset=None, buf=''):
        ctxt = dict(res=[], count=self.iounit, offset=self.offset)

        if count is not None:
            ctxt['count'] = count
        if offset is not None:
            ctxt['offset'] = offset

        def next(resp=None, exc=None, tb=None):
            if resp and resp.data:
                ctxt['res'].append(resp.data)
                ctxt['offset'] += len(resp.data)

            if ctxt['count'] == 0:
                if offset is None:
                    self.offset = ctxt['offset']
                return Client.respond(callback, ''.join(ctxt['res']), exc, tb)

            n = min(ctxt['count'], self.iounit)
            ctxt['count'] -= n

            self._dorpc(fcall.Tread(offset=ctxt['offset'], count=n),
                        next, fail or callback)
        next()

    def areadlines(self, callback):
        ctxt = dict(last=None)
        def next(data, exc, tb):
            res = True
            if data:
                lines = data.split('\n')
                if ctxt['last']:
                    lines[0] = ctxt['last'] + lines[0]
                for i in range(0, len(lines) - 1):
                    res = callback(lines[i])
                    if res is False:
                        break
                ctxt['last'] = lines[-1]
                if res is not False:
                    self.aread(next)
            else:
                if ctxt['last']:
                    callback(ctxt['last'])
                callback(None)
        self.aread(next)

    def awrite(self, data, callback=True, fail=None, offset=None):
        ctxt = dict(offset=self.offset, off=0)
        if offset is not None:
            ctxt['offset'] = offset

        def next(resp=None, exc=None, tb=None):
            if resp:
                ctxt['off'] += resp.count
                ctxt['offset'] += resp.count
            if ctxt['off'] < len(data) or not (exc or resp):
                n = min(len(data), self.iounit)

                self._dorpc(fcall.Twrite(offset=ctxt['offset'],
                                         data=data[ctxt['off']:ctxt['off']+n]),
                            next, fail or callback)
            else:
                if offset is None:
                    self.offset = ctxt['offset']
                Client.respond(callback, ctxt['off'], exc, tb)
        next()

    def aremove(self, callback=True, fail=None):
        def next(resp, exc, tb):
            self.close()
            if exc and fail:
                Client.respond(fail, resp and True, exc, tb)
            else:
                Client.respond(callback, resp and True, exc, tb)
        self._dorpc(fcall.Tremove(), next)

# vim:se sts=4 sw=4 et:
