import os
import re
import sys
import traceback

import pygmi
from pygmi import monitor, client, call, program_list

__all__ = ('bind_keys', 'bind_events', 'toggle_keys', 'event_loop',
           'event')

keydefs = {}
keys = {}
events = {}

alive = True

def flatten(items):
    for k, v in items.iteritems():
        if not isinstance(k, (list, tuple)):
            k = k,
        for key in k:
            yield key, v

def bind_keys(items):
    for k, v in flatten(items):
        keys[k % keydefs] = v

def bind_events(items):
    for k, v in flatten(items):
        events[k] = v

def event(fn):
    bind_events({fn.__name__: fn})

@event
def Key(args):
    if args in keys:
        keys[args](args)

keys_enabled = False
keys_restore = None
def toggle_keys(on=None, restore=None):
    if on is None:
        on = not keys_enabled
    keys_restore = restore
    if on:
        client.write('/keys', '\n'.join(keys.keys()))
    else:
        client.write('/keys', restore or ' ')

def dispatch(event, args=''):
    if event in events:
        try:
            events[event](args)
        except Exception, e:
            traceback.print_exc(sys.stderr)

def event_loop():
    from pygmi import events
    toggle_keys(on=True)
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
