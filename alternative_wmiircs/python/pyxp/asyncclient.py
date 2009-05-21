from pyxp import client, fcall
from pyxp.client import *

def awithfile(*oargs, **okwargs):
    def wrapper(fn):
        def next(self, path, *args, **kwargs):
            def next(file, exc, tb):
                fn(self, (file, exc, tb), *args, **kwargs)
            self.aopen(path, next, *oargs, **okwargs)
        return next
    return wrapper
def wrap_callback(fn, file):
    file.called = 0
    def callback(data, exc, tb):
        file.called += 1
        file.close()
        if callable(fn):
            fn(data, exc, tb)
    return callback

class Client(client.Client):
    ROOT_FID = 0

    def awalk(self, path, async, fail=None):
        ctxt = dict(path=path, fid=self.getfid(), ofid=ROOT_FID)
        def next(resp=None, exc=None, tb=None):
            if exc and ctxt['ofid'] != ROOT_FID:
                self.aclunk(ctxt['fid'])
            if not ctxt['path'] and resp or exc:
                if exc and fail:
                    return self.respond(fail, None, exc, tb)
                return self.respond(async, ctxt['fid'], exc, tb)
            wname = ctxt['path'][:fcall.MAX_WELEM]
            ofid = ctxt['ofid']
            ctxt['path'] = ctxt['path'][fcall.MAX_WELEM:]
            if resp:
                ctxt['ofid'] = ctxt['fid']
            self.dorpc(fcall.Twalk(fid=ofid,
                                   newfid=ctxt['fid'],
                                   wname=wname),
                       next)
        next()

    def _open(self, path, mode, open, origpath=None):
        resp = None

        with self.walk(path) as nfid:
            fid = nfid
            resp = self.dorpc(open(fid))

        def cleanup():
            self.aclunk(fid)
        file = File(self, origpath or '/'.join(path), resp, fid, mode, cleanup)
        self.files[fid] = file

        return file

    def _aopen(self, path, mode, open, callback, origpath=None):
        resp = None
        def next(fid, exc, tb):
            def next(resp, exc, tb):
                def cleanup():
                    self.clunk(fid)
                file = File(self, origpath or '/'.join(path), resp, fid, mode, cleanup)
                self.files[fid] = file
                self.respond(callback, file)
            self.dorpc(open(fid), next, callback)
        self.awalk(path, next, callback)

    def aopen(self, path, callback=True, mode=OREAD):
        assert callable(callback)
        path = self.splitpath(path)
        def open(fid):
            return fcall.Topen(fid=fid, mode=mode)
        return self._aopen(path, mode, open, callback)

    def acreate(self, path, callback=True, mode=OREAD, perm=0):
        path = self.splitpath(path)
        name = path.pop()
        def open(fid):
            return fcall.Tcreate(fid=fid, mode=mode, name=name, perm=perm)
        if not callable(callback):
            def callback(resp, exc, tb):
                if resp:
                    resp.close()
        return self._aopen(path, mode, open, async,
                           origpath='/'.join(path + [name]))

    def aremove(self, path, callback=True):
        path = self.splitpath(path)
        def next(fid, exc, tb):
            self.dorpc(fcall.Tremove(fid=fid), callback)
        self.awalk(path, next, callback)

    def astat(self, path, callback):
        path = self.splitpath(path)
        def next(fid, exc, tb):
            def next(resp, exc, tb):
                callback(resp.stat, exc, tb)
            self.dorpc(fcall.Tstat(fid=fid), next, callback)

    @awithfile()
    def aread(self, (file, exc, tb), callback, *args, **kwargs):
        if exc:
            callback(file, exc, tb)
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
    @staticmethod
    def respond(callback, data, exc=None, tb=None):
        if callable(callback):
            callback(data, exc, tb)

    def stat(self, callback):
        def next(resp, exc, tb):
            callback(resp.stat, exc, tb)
        resp = self.dorpc(fcall.Tstat(), next, callback)

    def aread(self, callback, count=None, offset=None, buf=''):
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
                return callback(''.join(ctxt['res']), exc, tb)

            n = min(ctxt['count'], self.iounit)
            ctxt['count'] -= n

            self.dorpc(fcall.Tread(offset=ctxt['offset'], count=n),
                       next, callback)
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

    def awrite(self, data, callback=True, offset=None):
        ctxt = dict(offset=self.offset, off=0)
        if offset is not None:
            ctxt['offset'] = offset
        def next(resp=None, exc=None, tb=None):
            if resp:
                ctxt['off'] += resp.count
                ctxt['offset'] += resp.count
            if ctxt['off'] < len(data):
                n = min(len(data), self.iounit)

                self.dorpc(fcall.Twrite(offset=ctxt['offset'],
                                        data=data[ctxt['off']:ctxt['off']+n]),
                           next, callback)
            else:
                if offset is None:
                    self.offset = ctxt['offset']
                self.respond(callback, ctxt['off'], exc, tb)
        next()

    def aremove(self, callback=True):
        def next(resp, exc, tb):
            self.close()
            self.respond(resp and True, exc, tb)
        self.dorpc(fcall.Tremove(), next)

# vim:se sts=4 sw=4 et:
