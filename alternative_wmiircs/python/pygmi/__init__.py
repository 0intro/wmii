import os

from pyxp import Client

if 'WMII_ADDRESS' in os.environ:
    client = Client(os.environ['WMII_ADDRESS'])
else:
    client = Client(namespace='wmii')

def call(*args, **kwargs):
    background = kwargs.pop('background', False)
    input = kwargs.pop('input', None)
    import subprocess
    p = subprocess.Popen(args, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE, cwd=os.environ['HOME'],
                         **kwargs)
    if not background:
        return p.communicate(input)[0].rstrip('\n')

def program_list(path):
    names = []
    for d in path:
        try:
            for f in os.listdir(d):
                if f not in names and os.access('%s/%s' % (d, f),
                                                os.X_OK):
                    names.append(f)
        except Exception:
            pass
    return sorted(names)

def curry(func, *args, **kwargs):
    def curried(*newargs, **newkwargs):
        return func(*(args + newargs), **dict(kwargs, **newkwargs))
    curried.__name__ = func.__name__ + '__curried__'
    return curried

from pygmi import events, fs, menu, monitor
from pygmi.events import *
from pygmi.fs import *
from pygmi.menu import *
from pygmi.monitor import *

__all__ = (fs.__all__ + monitor.__all__ + events.__all__ +
           menu.__all__ + ('client', 'call', 'curry', 'program_list'))

# vim:se sts=4 sw=4 et:
