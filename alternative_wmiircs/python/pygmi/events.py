import os
import re
import sys
import traceback

import pygmi
from pygmi import monitor, client, curry, call, program_list, _

__all__ = ('keys', 'bind_events', 'event_loop', 'event', 'Match',
           'Desc')

keydefs = {}
events = {}
eventmatchers = {}

alive = True

class Match(object):
    def __init__(self, *args):
        self.args = args
        self.matchers = []
        for a in args:
            if a is _:
                a = lambda k: True
            elif isinstance(a, basestring):
                a = a.__eq__
            elif isinstance(a, (list, tuple)):
                a = curry(lambda ary, k: k in ary, a)
            elif hasattr(a, 'search'):
                a = a.search
            else:
                a = str(a).__eq__
            self.matchers.append(a)

    def match(self, string):
        ary = string.split(' ', len(self.matchers))
        if all(m(a) for m, a in zip(self.matchers, ary)):
            return ary

def flatten(items):
    for k, v in items:
        if not isinstance(k, (list, tuple)):
            k = k,
        for key in k:
            yield key, v

def bind_events(items={}, **kwargs):
    kwargs.update(items)
    for k, v in flatten(kwargs.iteritems()):
        if isinstance(k, Match):
            eventmatchers[k] = v
        else:
            events[k] = v

def event(fn):
    bind_events({fn.__name__: fn})

class Desc(object):
    def __init__(self, doc):
        self.doc = doc
    def __unicode__(self):
        return self.doc
    def __str__(self):
        return str(self.doc)

class Keys(object):
    def __init__(self):
        self.modes = {}
        self.modelist = []
        self.mode = 'main'
        bind_events(Key=self.dispatch)

    def _add_mode(self, mode):
        if mode not in self.modes:
            self.modes[mode] = { 'name': mode, 'desc': {}, 'groups': [],
                                 'keys': {}, 'import': {} }
            self.modelist.append(mode)

    def _set_mode(self, mode):
        self._add_mode(mode)
        self._mode = mode
        self._keys = dict((k % keydefs, v) for k, v in
                          self.modes[mode]['keys'].items() +
                          self.modes[mode]['keys'].items());
        client.write('/keys', '\n'.join(self._keys.keys()) + '\n')
    mode = property(lambda self: self._mode, _set_mode)

    @property
    def help(self):
        return '\n\n'.join(
            ('Mode %s\n' % mode['name']) +
            '\n\n'.join(('  %s\n' % str(group or '')) +
                        '\n'.join('    %- 20s %s' % (key % keydefs,
                                                     mode['desc'][key])
                                  for key in mode['desc'][group])
                        for group in mode['groups'])
            for mode in (self.modes[name]
                         for name in self.modelist))

    def bind(self, mode='main', keys=(), import_={}):
        self._add_mode(mode)
        mode = self.modes[mode]
        group = None
        key = None
        def add_desc(key, desc):
            mode['desc'][key] = desc
            if group not in mode['desc']:
                mode['desc'][group] = []
                mode['groups'].append(group)
            if key not in mode['desc'][group]:
                mode['desc'][group].append(key);

        for k in keys:
            if key and isinstance(k, Desc):
                add_desc(key, k)
            elif isinstance(k, Desc):
                group = str(k)
            elif key:
                mode['keys'][key] = k
                key = None
            else:
                key = k

        for k, v in flatten((v, k) for k, v in import_.iteritems()):
            mode['import'][k % keydefs] = v

    def dispatch(self, key):
        mode = self.modes[self.mode]
        if key in self._keys:
            return self._keys[key](key)
keys = Keys()

def dispatch(event, args=''):
    try:
        if event in events:
            events[event](args)
        for matcher, action in eventmatchers.iteritems():
            ary = matcher.match(' '.join((event, args)))
            if ary is not None:
                action(*ary)
    except Exception, e:
        traceback.print_exc(sys.stderr)

def event_loop():
    from pygmi import events
    keys.mode = 'main'
    for line in client.readlines('/event'):
        if not events.alive:
            break
        dispatch(*line.split(' ', 1))
    events.alive = False

class Actions(object):
    def __getattr__(self, name):
        if name.startswith('_') or name.endswith('_'):
            raise AttributeError()
        if hasattr(self, name + '_'):
            return getattr(self, name + '_')
        def action(args=''):
            cmd = pygmi.find_script(name)
            if cmd:
                call(pygmi.shell, '-c', '$* %s' % args, '--', cmd,
                     background=True)
        return action

    def _call(self, args):
        a = args.split(' ', 1)
        if a:
            getattr(self, a[0])(*a[1:])

    @property
    def _choices(self):
        return sorted(
            program_list(pygmi.confpath) +
            [re.sub('_$', '', k) for k in dir(self)
             if not re.match('^_', k) and callable(getattr(self, k))])


# vim:se sts=4 sw=4 et:
