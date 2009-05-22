import os
import subprocess

import pygmi

__all__ = 'call', 'program_list', 'curry', 'find_script', '_'

def _():
    pass

def call(*args, **kwargs):
    background = kwargs.pop('background', False)
    input = kwargs.pop('input', None)
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
    if _ in args:
        blank = [i for i in range(0, len(args)) if args[i] is _]
        def curried(*newargs, **newkwargs):
            ary = list(args)
            for k, v in zip(blank, newargs):
                ary[k] = v
            ary = tuple(ary) + newargs[len(blank):]
            return func(*ary, **dict(kwargs, **newkwargs))
    else:
        def curried(*newargs, **newkwargs):
            return func(*(args + newargs), **dict(kwargs, **newkwargs))
    curried.__name__ = func.__name__ + '__curried__'
    return curried

def find_script(name):
    for path in pygmi.confpath:
        if os.access('%s/%s' % (path, name), os.X_OK):
            return '%s/%s' % (path, name)

# vim:se sts=4 sw=4 et:
