import collections

from pyxp import *
from pyxp.client import *
from pygmi import *

__all__ = ('wmii', 'Tags', 'Tag', 'Area', 'Frame', 'Client',
           'Button', 'Colors', 'Color')

class Ctl(object):
    sentinel = {}
    ctl_types = {}
    ctl_hasid = False

    def __init__(self):
        pass

    def ctl(self, msg):
        client.awrite(self.ctl_path, msg)

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
        if key in self.ctl_types:
            val = self.ctl_types[key][1](val)
        self.ctl('%s %s\n' % (key, val))

    def get(self, key, default=sentinel):
        try:
            val = self[key]
        except KeyError, e:
            if default is not self.sentinel:
                return default
            raise e
    def set(self, key, val):
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
        lines = tuple(client.readlines(self.ctl_path))
        if self.ctl_hasid:
            lines = lines[1:]
        return lines

    _id = None
    @property
    def id(self):
        if self._id is None and self.ctl_hasid:
            return client.read(self.ctl_path).split('\n', 1)[0]
        return self._id

class Dir(Ctl):
    ctl_hasid = True

    def __init__(self, id):
        if id != 'sel':
            self._id = id

    def __eq__(self, other):
        return (self.__class__ == other.__class__ and
                self.id == other.id)

    class ctl_property(object):
        def __init__(self, key):
            self.key = key
        def __get__(self, dir, cls):
            return dir[self.key]
        def __set__(self, dir, val):
            dir[self.key] = val
    class toggle_property(ctl_property):
        props = {
            'on': True,
            'off': False,
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
        def __init__(self, name, writable=False):
            self.name = name
            self.writable = writable
        def __get__(self, dir, cls):
            return client.read('%s/%s' % (dir.path, self.name))
        def __set__(self, dir, val):
            if not self.writable:
                raise NotImplementedError('File %s is not writable' % self.name)
            return client.awrite('%s/%s' % (dir.path, self.name), val)

    @property
    def ctl_path(self):
        return '%s/ctl' % self.path

    @property
    def path(self):
        return '%s/%s' % (self.base_path, self._id or 'sel')

    @classmethod
    def all(cls):
        return (cls(s.name)
                for s in client.readdir(cls.base_path)
                if s.name != 'sel')

    def __repr__(self):
        return '%s(%s)' % (self.__class__.__name__,
                           repr(self._id or 'sel'))

class Client(Dir):
    base_path = '/client'

    fullscreen = Dir.toggle_property('Fullscreen')
    urgent = Dir.toggle_property('Urgent')

    label = Dir.file_property('label', writable=True)
    tags  = Dir.file_property('tags', writable=True)
    props = Dir.file_property('props')

    def kill(self):
        self.ctl('kill')
    def slay(self):
        self.ctl('slay')

class liveprop(object):
    def __init__(self, get):
        self.get = get
        self.attr = str(self)
    def __get__(self, area, cls):
        if getattr(area, self.attr, None) is not None:
            return getattr(area, self.attr)
        return self.get(area)
    def __set__(self, area, val):
        setattr(area, self.attr, val)

class Area(object):
    def __init__(self, tag, ord, offset=None, width=None, height=None, frames=None):
        self.tag = tag
        self.ord = str(ord)
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

    def _get_mode(self):
        for k, v in self.tag.iteritems():
            if k == 'colmode':
                v = v.split(' ')
                if v[0] == self.ord:
                    return v[1]
    mode = property(
        _get_mode,
        lambda self, val: self.tag.set('colmode %s' % self.ord, val))

    def grow(self, dir, amount=None):
        self.tag.grow(self, dir, amount)
    def nudge(self, dir, amount=None):
        self.tag.nudge(self, dir, amount)

class Frame(object):
    live = False

    def __init__(self, client, area=None, ord=None, offset=None, height=None):
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

    def _set_selected(self, frame):
        if not isinstance(frame, basestring) or ' ' not in frame:
            frame = self.framespec(frame)
        self['select'] = frame
    selected = property(lambda self: tuple(self['select'].split(' ')),
                        _set_selected)

    def _get_selclient(self):
        for k, v in self.iteritems():
            if k == 'select' and 'client' in v:
                return Client(v.split(' ')[1])
        return None
    selclient = property(_get_selclient,
                         lambda self, val: self.set('select',
                                                    self.framespec(val)))

    @property
    def selcol(self):
        return Area(self, self.selected[0])

    @property
    def index(self):
        areas = []
        for l in [l.split(' ')
                  for l in client.readlines('%s/index' % self.path)
                  if l]:
            if l[0] == '#':
                if l[1] == '~':
                    area = Area(tag=self, ord=l[1], width=l[2], height=l[3],
                                frames=[])
                else:
                    area = Area(tag=self, ord=l[1], offset=l[2], width=l[3],
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

class Button(object):
    sides = {
        'left': 'lbar',
        'right': 'rbar',
    }
    def __init__(self, side, name, colors=None, label=None):
        self.side = side
        self.name = name
        self.base_path = self.sides[side]
        self.path = '%s/%s' % (self.base_path, self.name)
        self.create(colors, label)

    def create(self, colors=None, label=None):
        with client.create(self.path, OWRITE) as f:
            f.write(self.getval(colors, label))
    def remove(self):
        client.aremove(self.path)

    def getval(self, colors=None, label=None):
        if colors is None:
            colors = self.colors
        if label is None:
            label = self.label
        return ' '.join([Color(c).hex for c in colors] + [str(label)])

    colors = property(
        lambda self: tuple(map(Color, client.read(self.path).split(' ')[:3])),
        lambda self, val: client.awrite(self.path, self.getval(colors=val)))

    label = property(
        lambda self: client.read(self.path).split(' ', 3)[3],
        lambda self, val: client.write(self.path, self.getval(label=val)))

    @classmethod
    def all(cls, side):
        return (Button(side, s.name)
                for s in client.readdir(cls.sides[side])
                if s.name != 'sel')

class Colors(object):
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

    def __str__(self):
        return str(unicode(self))
    def __unicode__(self):
        return ' '.join(c.hex for c in self.vals)
    def __repr__(self):
        return 'Colors(%s, %s, %s)' % tuple(repr(c.rgb) for c in self.vals)

class Color(object):
    def __init__(self, colors):
        if isinstance(colors, Color):
            colors = colors.rgb
        elif isinstance(colors, basestring):
            match = re.match(r'^#(..)(..)(..)$', colors)
            colors = tuple(int(match.group(group), 16) for group in range(1, 4))
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
        return '#%02x%02x%02x' % self.rgb

    def __str__(self):
        return str(unicode(self))
    def __unicode__(self):
        return 'rgb(%d, %d, %d)' % self.rgb
    def __repr__(self):
        return 'Color(%s)' % repr(self.rgb)

class Rules(collections.MutableMapping):
    regex = re.compile(r'^\s*/(.*?)/\s*(?:->)?\s*(.*)$')

    def __get__(self, obj, cls):
        return self
    def __set__(self, obj, val):
        self.setitems(val)

    def __init__(self, path, rules=None):
        self.path = path
        if rules:
            self.setitems(rules)

    def __getitem__(self, key):
        for k, v in self.iteritems():
            if k == key:
                return v
        raise KeyError()
    def __setitem__(self, key, val):
        items = []
        for k, v in self.iteritems():
            if key == k:
                v = val
                key = None
            items.append((k, v))
        if key is not None:
            items.append((key, val))
        self.setitems(items)
    def __delitem__(self, key):
        self.setitems((k, v) for k, v in self.iteritems() if k != key)

    def __len__(self):
        return len(tuple(self.iteritems()))
    def __iter__(self):
        for k, v in self.iteritems():
            yield k
    def __list__(self):
        return list(iter(self))
    def __tuple__(self):
        return tuple(iter(self))

    def append(self, item):
        self.setitems(self + (item,))
    def __add__(self, items):
        return tuple(self.iteritems()) + tuple(items)

    def setitems(self, items):
        lines = []
        for k, v in items:
            assert '/' not in k and '\n' not in v
            lines.append('/%s/ -> %s' % (k, v))
        lines.append('')
        client.awrite(self.path, '\n'.join(lines))

    def iteritems(self):
        for line in client.readlines(self.path):
            match = self.regex.match(line)
            if match:
                yield match.groups()
    def items(self):
        return list(self.iteritems())

@apply
class wmii(Ctl):
    ctl_path = '/ctl'
    ctl_types = {
        'normcolors': (Colors.from_string, lambda c: str(Colors(*c))),
        'focuscolors': (Colors.from_string, lambda c: str(Colors(*c))),
        'border': (int, str),
    }

    clients = property(lambda self: Client.all())
    tags = property(lambda self: Tag.all())
    lbuttons = property(lambda self: Button.all('left'))
    rbuttons = property(lambda self: Button.all('right'))

    tagrules = Rules('/tagrules')
    colrules = Rules('/colrules')

class Tags(object):
    PREV = []
    NEXT = []

    def __init__(self, normcol=None, focuscol=None):
        self.tags = {}
        self.sel = None
        self.normcol = normcol or wmii['normcolors']
        self.focuscol = focuscol or wmii['focuscolors']
        for t in wmii.tags:
            self.add(t.id)
        for b in wmii.lbuttons:
            if b.name not in self.tags:
                b.aremove()
        self.focus(Tag('sel').id)

        self.mru = [self.sel.id]
        self.idx = -1
        Tags.instance = self

    def add(self, tag):
        self.tags[tag] = Tag(tag)
        self.tags[tag].button = Button('left', tag, self.normcol, tag)
    def delete(self, tag):
        self.tags.pop(tag).button.remove()

    def focus(self, tag):
        self.sel = self.tags[tag]
        self.sel.button.colors = self.focuscol
    def unfocus(self, tag):
        self.tags[tag].button.colors = self.normcol

    def set_urgent(self, tag, urgent=True):
        self.tags[tag].button.label = urgent and '*' + tag or tag

    def next(self, reverse=False):
        tags = list(wmii.tags)
        tags.append(tags[0])
        if reverse:
            tags.reverse()
        for i in range(0, len(tags)):
            if tags[i] == self.sel:
                return tags[i+1]
        return self.sel

    def select(self, tag):
        if tag is self.PREV:
            self.idx -= 1
        elif tag is self.NEXT:
            self.idx += 1
        else:
            if isinstance(tag, Tag):
                tag = tag.id
            wmii['view'] = tag

            if tag != self.mru[-1]:
                self.mru.append(tag)
                self.mru = self.mru[-10:]
            return

        self.idx = min(-1, max(-len(self.mru), self.idx))
        wmii['view'] = self.mru[self.idx]

if __name__ == '__main__':
    c = Client('sel')
    #print c.id
    #print c.items()
    #print c.urgent

    #print list(wmii.clients)
    #print list(wmii.tags)

    #print [a.frames for a in Tag('sel').index]
    #print Tag('sel').selclient
    #print Tag('sel').selclient.label
    #print Tag('sel').selclient.tags
    #print Tag('sel').selclient.props
    #a = Area(Tag('sel'), 1)
    #print a.width
    #print a.frames

    #print [[c.hex for c in b.colors] for b in wmii.lbuttons]
    #print [[c.hex for c in b.colors] for b in wmii.rbuttons]
    Button('left', '1').colors = ((0., 0., 0.), (1., 1., 1.), (0., 0., 0.))
    Button('left', '1').label = 'foo'
    Button('left', '5', label='baz')
    print repr(wmii['normcolors'])

# vim:se sts=4 sw=4 et:
