from pygmi.util import call

__all__ = 'Menu', 'ClickMenu'

def inthread(fn, action):
    def run():
        res = fn()
        if action:
            return action(res)
        return res
    if not action:
        return run()
    from threading import Thread
    t = Thread(target=run)
    t.daemon = True
    t.start()

class Menu(object):
    def __init__(self, choices=(), action=None,
                 histfile=None, nhist=None):
        self.choices = choices
        self.action = action
        self.histfile = histfile
        self.nhist = nhist

    def call(self, choices=None):
        if choices is None:
            choices = self.choices
        if callable(choices):
            choices = choices()
        def act():
            args = ['wimenu']
            if self.histfile:
                args += ['-h', self.histfile]
            if self.nhist:
                args += ['-n', self.nhist]
            return call(*map(str, args), input='\n'.join(choices))
        return inthread(act, self.action)

class ClickMenu(object):
    def __init__(self, choices=(), action=None,
                 histfile=None, nhist=None):
        self.choices = choices
        self.action = action
        self.prev = None

    def call(self, choices=None):
        if choices is None:
            choices = self.choices
        if callable(choices):
            choices = choices()
        def act():
            args = ['wmii9menu']
            if self.prev:
                args += ['-i', self.prev]
            args += ['--'] + list(choices)
            return call(*map(str, args)).replace('\n', '')
        return inthread(act, self.action)

# vim:se sts=4 sw=4 et:
