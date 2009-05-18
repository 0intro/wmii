from pygmi import client
from pygmi.fs import *

__all__ = 'monitors', 'defmonitor', 'Monitor'

monitors = {}

def defmonitor(*args, **kwargs):
    def monitor(fn):
        kwargs['action'] = fn
        if not args and 'name' not in kwargs:
            kwargs['name'] = fn.__name__
        monitor = Monitor(*args, **kwargs)
        monitors[monitor.name] = monitor
        return monitor
    if args and callable(args[0]):
        fn = args[0]
        args = args[1:]
        return monitor(fn)
    return monitor

class MonitorBase(type):
    def __new__(cls, name, bases, attrs):
        new_cls = super(MonitorBase, cls).__new__(cls, name, bases, attrs)
        if name not in attrs:
            new_cls.name = new_cls.__name__.lower()
        try:
            Monitor
            if new_cls.name not in monitors:
                monitors[new_cls.name] = new_cls()
        except Exception, e:
            pass
        return new_cls

class Monitor(object):

    side = 'right'
    interval = 1.0

    def __init__(self, name=None, interval=None, side=None,
                 action=None, colors=None, label=None):
        if side:
            self.side = side
        if name:
            self.name = name
        if interval:
            self.interval = interval
        if action:
            self.action = action

        self.timer = None
        self.button = Button(self.side, self.name, colors, label)
        self.tick()

    def tick(self):
        from pygmi import events
        mon = monitors.get(self.name, None)
        if self.timer and not (events.alive and mon is self):
            if client and (not mon or mon is self):
                self.button.remove()
            return
        if self.active:
            from threading import Timer
            label = self.getlabel()
            if isinstance(label, basestring):
                label = None, label
            self.button.create(*label)
            self.timer = Timer(self.interval, self.tick)
            self.timer.start()

    def getlabel(self):
        if self.action:
            return self.action()
        return ()

    _active = True
    def _set_active(self, val):
        self._active = bool(val)
        if val:
            self.tick()
        else:
            self.button.remove()

    active = property(
        lambda self: self._active,
        _set_active)

# vim:se sts=4 sw=4 et:
