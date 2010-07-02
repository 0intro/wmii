import collections
from datetime import datetime, timedelta
import re

from pyxp import *
from pyxp.client import *
from pygmi import *
from pygmi.util import prop

__all__ = ('wmii', 'Tags', 'Tag', 'Area', 'Frame', 'Client',
           'Button', 'Colors', 'Color', 'Toggle', 'Always', 'Never')

spacere = re.compile(r'\s')
sentinel = {}

class utf8(object):
    def __str__(self):
        return unicode(self).encode('utf-8')

@apply
class Toggle(utf8):
    def __unicode__(self):
        return unicode(self.__class__.__name__)
@apply
class Always(Toggle.__class__):
    pass
@apply
class Never(Toggle.__class__):
    pass

def constrain(min, max, val):
    return min if val < min else max if val > max else val

class Map(collections.Mapping):
    def __init__(self, cls, *args):
        self.cls = cls
        self.args = args
    def __repr__(self):
        return 'Map(%s%s)' % (self.cls.__name__, (', %s' % ', '.join(map(repr, self.args)) if self.args else ''))
    def __getitem__(self, item):
        ret = self.cls(*(self.args + (item,)))
        if not ret.exists:
            raise KeyError('no such %s %s' % (self.cls.__name__.lower(), repr(item)))
        return ret
    def __len__(self):
        return len(iter(self))
    def __keys__(self):
        return [v for v in self.cls.all(*self.args)]
    def __iter__(self):
        return (v for v in self.cls.all(*self.args))
    def iteritems(self):
        return ((v, self.cls(*(self.args + (v,)))) for v in self.cls.all(*self.args))
    def itervalues(self):
        return (self.cls(*(self.args + (v,))) for v in self.cls.all(*self.args))

class Ctl(object):
    """
    An abstract class to represent the 'ctl' files of the wmii filesystem.
    Instances act as live, writable dictionaries of the settings represented
    in the file.

    Abstract roperty ctl_path: The path to the file represented by this
            control.
    Property ctl_hasid: When true, the first line of the represented
            file is treated as an id, rather than a key-value pair. In this
            case, the value is available via the 'id' property.
    Property ctl_types: A dict mapping named dictionary keys to two valued
            tuples, each containing a decoder and encoder function for the
            property's plain text value.
    """
    ctl_types = {}
    ctl_hasid = False
    ctl_open = 'aopen'
    ctl_file = None

    def __eq__(self, other):
        if self.ctl_hasid and isinstance(other, Ctl) and other.ctl_hasid:
            return self.id == other.id
        return False

    def __init__(self):
        self.cache = {}

    def ctl(self, *args):
        """
        Arguments are joined by ascii spaces and written to the ctl file.
        """
        def next(file, exc=None, tb=None):
            if exc:
                print exc
            if file:
                self.ctl_file = file
                file.awrite(u' '.join(map(unicode, args)))
        if self.ctl_file:
            return next(self.ctl_file)
        getattr(client, self.ctl_open)(self.ctl_path, callback=next, mode=OWRITE)

    def __getitem__(self, key):
        for line in self.ctl_lines():
            key_, rest = line.split(' ', 1)
            if key_ == key:
                if key in self.ctl_types:
                    return self.ctl_types[key][0](rest)
                return rest
        raise KeyError()
    def __hasitem__(self, key):
        return key in self.keys()
    def __setitem__(self, key, val):
        assert '\n' not in key
        self.cache[key] = val
        if key in self.ctl_types:
            if self.ctl_types[key][1] is None:
                raise NotImplementedError('%s: %s is not writable' % (self.ctl_path, key))
            val = self.ctl_types[key][1](val)
        self.ctl(key, val)

    def get(self, key, default=sentinel):
        """
        Gets the instance's dictionary value for 'key'. If the key doesn't
        exist, 'default' is returned. If 'default' isn't provided and the key
        doesn't exist, a KeyError is raised.
        """
        try:
            return self[key]
        except KeyError, e:
            if default is not self.sentinel:
                return default
            raise e
    def set(self, key, val):
        """
        Sets the dictionary value for 'key' to 'val', as self[key] = val
        """
        self[key] = val

    def keys(self):
        return [line.split(' ', 1)[0]
                for line in self.ctl_lines()]
    def iteritems(self):
        return (tuple(line.split(' ', 1))
                for line in self.ctl_lines())
    def items(self):
        return [tuple(line.split(' ', 1))
                for line in self.ctl_lines()]

    def ctl_lines(self):
        """
        Returns the lines of the ctl file as a tuple, with the first line
        stripped if #ctl_hasid is set.
        """
        lines = tuple(client.readlines(self.ctl_path))
        if self.ctl_hasid:
            lines = lines[1:]
        return lines

    _id = None
    @prop(doc="If #ctl_hasid is set, returns the id of this ctl file.")
    def id(self):
        if self._id is None and self.ctl_hasid:
            return self.name_read(client.read(self.ctl_path).split('\n', 1)[0])
        return self._id

class Dir(Ctl):
    """
    An abstract class representing a directory in the wmii filesystem with a
    ctl file and sub-objects.

    Abstract property base_path: The path directly under which all objects
            represented by this class reside. e.g., /client, /tag
    """
    ctl_hasid = True
    name_read = unicode
    name_write = unicode

    def __init__(self, id):
        """
        Initializes the directory object.

        Param id: The id of the object in question. If 'sel', the object
                dynamically represents the selected object, even as it
                changes. In this case, #id will return the actual ID of the
                object.
        """
        super(Dir, self).__init__()
        if isinstance(id, Dir):
            id = id.id
        if id != 'sel':
            self._id = self.name_read(id)

    def __eq__(self, other):
        return (self.__class__ == other.__class__ and
                self.id == other.id)

    class ctl_property(object):
        """
        A class which maps instance properties to ctl file properties.
        """
        def __init__(self, key):
            self.key = key
        def __get__(self, dir, cls):
            return dir.get(self.key, None)
        def __set__(self, dir, val):
            dir[self.key] = val

    class toggle_property(ctl_property):
        """
        A class which maps instance properties to ctl file properties. The
        values True and False map to the strings "on" and "off" in the
        filesystem.
        """
        props = {
            'on': True,
            'off': False,
            'toggle': Toggle,
            'always': Always,
            'never': Never
        }
        def __get__(self, dir, cls):
            val = dir[self.key]
            if val in self.props:
                return self.props[val]
            return val
        def __set__(self, dir, val):
            for k, v in self.props.iteritems():
                if v == val:
                    val = k
                    break
            dir[self.key] = val

    class file_property(object):
        """
        A class which maps instance properties to files in the directory
        represented by this object.
        """
        def __init__(self, name, writable=False):
            self.name = name
            self.writable = writable
        def __get__(self, dir, cls):
            return client.read('%s/%s' % (dir.path, self.name))
        def __set__(self, dir, val):
            if not self.writable:
                raise NotImplementedError('File %s is not writable' % self.name)
            return client.awrite('%s/%s' % (dir.path, self.name),
                                 str(val))

    @prop(doc="The path to this directory's ctl file")
    def ctl_path(self):
        return '%s/ctl' % self.path

    @prop(doc="The path to this directory")
    def path(self):
        return '%s/%s' % (self.base_path, self.name_write(self._id or 'sel'))
    @prop(doc="True if the given object exists in the wmii filesystem")
    def exists(self):
        return bool(client.stat(self.path))

    @classmethod
    def all(cls):
        """
        Returns all of the objects that exist for this type of directory.
        """
        return (cls.name_read(s.name)
                for s in client.readdir(cls.base_path)
                if s.name != 'sel')
    @classmethod
    def map(cls, *args):
        return Map(cls, *args)

    def __repr__(self):
        return '%s(%s)' % (self.__class__.__name__,
                           repr(self._id or 'sel'))

class Client(Dir):
    """
    A class which represents wmii clients. Maps to the directories directly
    below /client.
    """
    base_path = '/client'
    ctl_types = {
        'group': (lambda s: int(s, 16), str),
        'pid': (int, None),
    }
    @staticmethod
    def name_read(name):
        if isinstance(name, int):
            return name
        try:
            return int(name, 16)
        except:
            return unicode(name)
    name_write = lambda self, name: name if isinstance(name, basestring) else '%#x' % name

    allow  = Dir.ctl_property('allow')
    fullscreen = Dir.toggle_property('fullscreen')
    group  = Dir.ctl_property('group')
    pid    = Dir.ctl_property('pid')
    tags   = Dir.ctl_property('tags')
    urgent = Dir.toggle_property('urgent')

    label = Dir.file_property('label', writable=True)
    props = Dir.file_property('props')

    def kill(self):
        """Politely asks a client to quit."""
        self.ctl('kill')

    def slay(self):
        """Forcibly severs a client's connection to the X server."""
        self.ctl('slay')

class liveprop(object):
    def __init__(self, get):
        self.get = get
        self.attr = str(self)
    def __get__(self, area, cls):
        if getattr(area, self.attr, sentinel) is not sentinel:
            return getattr(area, self.attr)
        return self.get(area)
    def __set__(self, area, val):
        setattr(area, self.attr, val)

class Area(object):
    def __init__(self, tag, ord, screen='sel', offset=sentinel, width=sentinel, height=sentinel, frames=sentinel):
        self.tag = tag
        if ':' in str(ord):
            screen, ord = ord.split(':', 2)
        self.ord = str(ord)
        self.screen = str(screen)
        self.offset = offset
        self.width = width
        self.height = height
        self.frames = frames

    def prop(key):
        @liveprop
        def prop(self):
            for area in self.tag.index:
                if str(area.ord) == str(self.ord):
                    return getattr(area, key)
        return prop
    offset = prop('offset')
    width = prop('width')
    height = prop('height')
    frames = prop('frames')

    @property
    def spec(self):
        if self.screen is not None:
            return '%s:%s' % (self.screen, self.ord)
        return self.ord

    @property
    def mode(self):
        for k, v in self.tag.iteritems():
            if k == 'colmode':
                v = v.split(' ')
                if v[0] == self.ord:
                    return v[1]
    @mode.setter
    def mode(self, val):
        self.tag['colmode %s' % self.spec] = val

    def grow(self, dir, amount=None):
        self.tag.grow(self, dir, amount)
    def nudge(self, dir, amount=None):
        self.tag.nudge(self, dir, amount)

class Frame(object):
    live = False

    def __init__(self, client, area=sentinel, ord=sentinel, offset=sentinel, height=sentinel):
        self.client = client
        self.ord = ord
        self.offset = offset
        self.height = height

    @property
    def width(self):
        return self.area.width

    def prop(key):
        @liveprop
        def prop(self):
            for area in self.tag.index:
                for frame in area.frames:
                    if frame.client == self.client:
                        return getattr(frame, key)
        return prop
    offset = prop('area')
    offset = prop('ord')
    offset = prop('offset')
    height = prop('height')

    def grow(self, dir, amount=None):
        self.area.tag.grow(self, dir, amount)
    def nudge(self, dir, amount=None):
        self.area.tag.nudge(self, dir, amount)

class Tag(Dir):
    base_path = '/tag'

    @classmethod
    def framespec(cls, frame):
        if isinstance(frame, Frame):
            frame = frame.client
        if isinstance(frame, Area):
            frame = (frame.ord, 'sel')
        if isinstance(frame, Client):
            if frame._id is None:
                return 'sel sel'
            return 'client %s' % frame.id
        elif isinstance(frame, basestring):
            return frame
        else:
            return '%s %s' % tuple(map(str, frame))
    def dirspec(cls, dir):
        if isinstance(dir, tuple):
            dir = ' '.join(dir)
        return dir

    @property
    def selected(self):
        return tuple(self['select'].split(' '))
    @selected.setter
    def selected(self, frame):
        if not isinstance(frame, basestring) or ' ' not in frame:
            frame = self.framespec(frame)
        self['select'] = frame

    @property
    def selclient(self):
        for k, v in self.iteritems():
            if k == 'select' and 'client' in v:
                return Client(v.split(' ')[1])
        return None
    @selclient.setter
    def selclient(self, val):
        self['select'] = self.framespec(val)

    @property
    def selcol(self):
        return Area(self, self.selected[0])

    @property
    def index(self):
        areas = []
        for l in (l.split(' ')
                  for l in client.readlines('%s/index' % self.path)
                  if l):
            if l[0] == '#':
                m = re.match(r'(?:(\d+):)?(\d+|~)', l[1])
                if m.group(2) == '~':
                    area = Area(tag=self, screen=m.group(1), ord=l[1], width=l[2],
                                height=l[3], frames=[])
                else:
                    area = Area(tag=self, screen=m.group(1) or 0,
                                height=None, ord=m.group(2), offset=l[2], width=l[3],
                                frames=[])
                areas.append(area)
                i = 0
            else:
                area.frames.append(
                    Frame(client=Client(l[1]), area=area, ord=i,
                          offset=l[2], height=l[3]))
                i += 1
        return areas

    def delete(self):
        id = self.id
        for a in self.index:
            for f in a.frames:
                if f.client.tags == id:
                    f.client.kill()
                else:
                    f.client.tags = '-%s' % id
        if self == Tag('sel'):
            Tags.instance.select(Tags.instance.next())

    def select(self, frame, stack=False):
        self['select'] = '%s %s' % (
            self.framespec(frame),
            stack and 'stack' or '')

    def send(self, src, dest, stack=False, cmd='send'):
        if isinstance(src, tuple):
            src = ' '.join(src)
        if isinstance(src, Frame):
            src = src.client
        if isinstance(src, Client):
            src = src._id or 'sel'

        if isinstance(dest, tuple):
            dest = ' '.join(dest)

        self[cmd] = '%s %s' % (src, dest)

    def swap(self, src, dest):
        self.send(src, dest, cmd='swap')
    
    def nudge(self, frame, dir, amount=None):
        frame = self.framespec(frame)
        self['nudge'] = '%s %s %s' % (frame, dir, str(amount or ''))
    def grow(self, frame, dir, amount=None):
        frame = self.framespec(frame)
        self['grow'] = '%s %s %s' % (frame, dir, str(amount or ''))

class Color(utf8):
    def __init__(self, colors):
        if isinstance(colors, Color):
            colors = colors.rgb
        elif isinstance(colors, basestring):
            match = (re.match(r'^#(..)(..)(..)((?:..)?)$', colors) or
                     re.match(r'^rgba:(..)/(..)/(..)/(..)$', colors))
            colors = tuple(int(match.group(group), 16) for group in range(1, 4))
            if match.group(4):
                colors += int(match.group(4), 16),
        def toint(val):
            if isinstance(val, float):
                val = int(255 * val)
            assert 0 <= val <= 255
            return val
        self.rgb = tuple(map(toint, colors))

    def __getitem__(self, key):
        if isinstance(key, basestring):
            key = {'red': 0, 'green': 1, 'blue': 2}[key]
        return self.rgb[key]

    @property
    def hex(self):
        if len(self.rgb) > 3:
            return 'rgba:%02x/%02x/%02x/%02x' % self.rgb
        return '#%02x%02x%02x' % self.rgb

    def __unicode__(self):
        if len(self.rgb) > 3:
            return 'rgba(%d, %d, %d, %d)' % self.rgb
        return 'rgb(%d, %d, %d)' % self.rgb
    def __repr__(self):
        return 'Color(%s)' % repr(self.rgb)

class Colors(utf8):
    def __init__(self, foreground=None, background=None, border=None):
        vals = foreground, background, border
        self.vals = tuple(map(Color, vals))

    def __iter__(self):
        return iter(self.vals)
    def __list__(self):
        return list(self.vals)
    def __tuple__(self):
        return self.vals

    @classmethod
    def from_string(cls, val):
        return cls(*val.split(' '))

    def __getitem__(self, key):
        if isinstance(key, basestring):
            key = {'foreground': 0, 'background': 1, 'border': 2}[key]
        return self.vals[key]

    def __unicode__(self):
        return ' '.join(c.hex for c in self.vals)
    def __repr__(self):
        return 'Colors(%s, %s, %s)' % tuple(repr(c.rgb) for c in self.vals)

class Button(Ctl):
    sides = {
        'left': 'lbar',
        'right': 'rbar',
    }
    ctl_types = {
        'colors': (Colors.from_string, lambda c: str(Colors(*c))),
    }
    ctl_open = 'acreate'
    colors = Dir.ctl_property('colors')
    label  = Dir.ctl_property('label')

    def __init__(self, side, name, colors=None, label=None):
        super(Button, self).__init__()
        self.side = side
        self.name = name
        self.base_path = self.sides[side]
        self.ctl_path = '%s/%s' % (self.base_path, self.name)
        self.file = None
        if colors or label:
            self.create(colors, label)

    def create(self, colors=None, label=None):
        def fail(resp, exc, tb):
            self.file = None
        if not self.file:
            self.file = client.create(self.ctl_path, ORDWR)
        if colors:
            self.colors = colors
        if label:
            self.label = label

    def remove(self):
        if self.file:
            self.file.aremove()
            self.file = None

    @property
    def exists(self):
        return bool(self.file.stat() if self.file else client.stat(self.ctl_path))

    @classmethod
    def all(cls, side):
        return (s.name
                for s in client.readdir(cls.sides[side])
                if s.name != 'sel')
    @classmethod
    def map(cls, *args):
        return Map(cls, *args)

class Rules(collections.MutableMapping, utf8):

    _items = ()
    def __init__(self, path, rules=None):
        self.path = path
        if rules:
            self.setitems(rules)

    _quotere = re.compile(ur'(\\(.)|/)')
    @classmethod
    def quoteslash(cls, str):
        return cls._quotere.sub(lambda m: m.group(0) if m.group(2) else r'\/', str)

    __get__ = lambda self, obj, cls: self
    def __set__(self, obj, val):
        self.setitems(val)

    def __getitem__(self, key):
        for k, v in self.iteritems():
            if k == key:
                return v
        raise KeyError()
    def __setitem__(self, key, val):
        items = [(k, v) for k, v in self.iteritems() if k != key]
        items.append((key, val))
        self.setitems(items)
    def __delitem__(self, key):
        self.setitems((k, v) for k, v in self.iteritems() if k != key)

    def __len__(self):
        return len(tuple(self.iteritems()))
    def __iter__(self):
        return (k for k, v in self.iteritems())
    def __list__(self):
        return list(iter(self))
    def __tuple__(self):
        return tuple(iter(self))

    def append(self, item):
        self.setitems(self + (item,))
    def __add__(self, items):
        return tuple(self.iteritems()) + tuple(items)

    def rewrite(self):
        client.awrite(self.path, unicode(self))
    def setitems(self, items):
        self._items = [(k, v if isinstance(v, Rule) else Rule(self, k, v))
                       for (k, v) in items]
        self.rewrite();

    def __unicode__(self):
        return u''.join(unicode(value) for (key, value) in self.iteritems()) or u'\n'

    def iteritems(self):
        return iter(self._items)
    def items(self):
        return list(self._items())

class Rule(collections.MutableMapping, utf8):
    _items = ()
    parent = None

    @classmethod
    def quotekey(cls, key):
        if key.endswith('_'):
            key = key[:-1]
        return key.replace('_', '-')
    @classmethod
    def quotevalue(cls, val):
        if val is True:   return "on"
        if val is False:  return "off"
        if val in (Toggle, Always, Never):
            return unicode(val).lower()
        return unicode(val)

    def __get__(self, obj, cls):
        return self
    def __set__(self, obj, val):
        self.setitems(val)

    def __init__(self, parent, key, items={}):
        self.key = key
        self._items = []
        self.setitems(items.iteritems() if isinstance(items, dict) else items)
        self.parent = parent

    def __getitem__(self, key):
        for k, v in reversed(self._items):
            if k == key:
                return v
        raise KeyError()

    def __setitem__(self, key, val):
        items = [(k, v) for k, v in self.iteritems() if k != key]
        items.append((key, val))
        self.setitems(items)

    def __delitem__(self, key):
        self.setitems([(k, v) for k, v in self.iteritems() if k != key])

    def __len__(self):
        return len(self._items)
    def __iter__(self):
        return iter(self._items)
    def __list__(self):
        return list(iter(self))
    def __tuple__(self):
        return tuple(iter(self))

    def append(self, item):
        self.setitems(self + (item,))
    def __add__(self, items):
        return tuple(self.iteritems()) + tuple(items)

    def setitems(self, items):
        items = list(items)
        assert not any('=' in key or
                       spacere.search(self.quotekey(key)) or
                       spacere.search(self.quotevalue(val)) for (key, val) in items)
        self._items = items
        if self.parent:
            self.parent.rewrite()

    def __unicode__(self):
        return u'/%s/ %s\n' % (
            Rules.quoteslash(self.key),
            u' '.join(u'%s=%s' % (self.quotekey(k), self.quotevalue(v))
                      for (k, v) in self.iteritems()))

    def iteritems(self):
        return iter(self._items)
    def items(self):
        return list(self._items)


@apply
class wmii(Ctl):
    ctl_path = '/ctl'
    ctl_types = {
        'normcolors': (Colors.from_string, lambda c: str(Colors(*c))),
        'focuscolors': (Colors.from_string, lambda c: str(Colors(*c))),
        'border': (int, str),
    }

    clients = Client.map()
    tags = Tag.map()
    lbuttons = Button.map('left')
    rbuttons = Button.map('right')

    rules    = Rules('/rules')

class Tags(object):
    PREV = []
    NEXT = []

    def __init__(self, normcol=None, focuscol=None):
        self.ignore = set()
        self.tags = {}
        self.sel = None
        self.normcol = normcol
        self.focuscol = focuscol
        self.lastselect = datetime.now()
        for t in wmii.tags:
            self.add(t)
        for b in wmii.lbuttons.itervalues():
            if b.name not in self.tags:
                b.remove()
        self.focus(Tag('sel').id)

        self.mru = [self.sel.id]
        self.idx = -1
        Tags.instance = self

    def add(self, tag):
        self.tags[tag] = Tag(tag)
        self.tags[tag].button = Button('left', tag, self.normcol or wmii.cache['normcolors'], tag)
    def delete(self, tag):
        self.tags.pop(tag).button.remove()

    def focus(self, tag):
        self.sel = self.tags[tag]
        self.sel.button.colors = self.focuscol or wmii.cache['focuscolors']
    def unfocus(self, tag):
        self.tags[tag].button.colors = self.normcol or wmii.cache['normcolors']

    def set_urgent(self, tag, urgent=True):
        self.tags[tag].button.label = urgent and '*' + tag or tag

    def next(self, reverse=False):
        tags = [t for t in wmii.tags if t not in self.ignore]
        tags.append(tags[0])
        if reverse:
            tags.reverse()
        for i in range(0, len(tags)):
            if tags[i] == self.sel.id:
                return tags[i+1]
        return self.sel

    def select(self, tag, take_client=None):
        def goto(tag):
            if take_client:
                # Make a new instance in case this is Client('sel'),
                # which would cause problems given 'sel' changes in the
                # process.
                client = Client(take_client.id)

                sel = Tag('sel').id
                client.tags = '+%s' % tag
                wmii['view'] = tag
                if tag != sel:
                    client.tags = '-%s' % sel
            else:
                wmii['view'] = tag

        if tag is self.PREV:
            if self.sel.id not in self.ignore:
                self.idx -= 1
        elif tag is self.NEXT:
            self.idx += 1
        else:
            if isinstance(tag, Tag):
                tag = tag.id
            goto(tag)

            if tag not in self.ignore:
                if self.idx < -1:
                    self.mru = self.mru[:self.idx + 1]
                    self.idx = -1
                if self.mru and datetime.now() - self.lastselect < timedelta(seconds=.5):
                    self.mru[self.idx] = tag
                elif tag != self.mru[-1]:
                    self.mru.append(tag)
                    self.mru = self.mru[-10:]
                self.lastselect = datetime.now()
            return

        self.idx = constrain(-len(self.mru), -1, self.idx)
        goto(self.mru[self.idx])

# vim:se sts=4 sw=4 et:
