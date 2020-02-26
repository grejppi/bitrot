#!/usr/bin/env python

import glob
import json
import os
import subprocess
import sys

import os.path

from collections import deque

def is_string(s):
    if isinstance(s, str):
        return True
    try:
        return isinstance(s, unicode)
    except NameError:
        return False

class TurtleWriter:
    @staticmethod
    def write(f, syntax):
        indent = 0
        newline = False
        while len(syntax.tokens) != 0:
            token = syntax.tokens.popleft()
            need = 0 if token == Token.PopBlank else 1
            if newline:
                f.write('  ' * indent * need)
                newline = False
                was_newline = True
            else:
                was_newline = False
            if token == Token.Prefix:
                newline = True
                f.write('@prefix {0}: <{1}> .\n'.format(
                    syntax.strings.popleft(),
                    syntax.strings.popleft(),
                ))
            elif token == Token.PushBlank:
                indent += 1
                newline = True
                f.write(' [\n')
            elif token == Token.PopBlank:
                indent -= 1
                f.write('{0}]'.format('  ' * indent))
            elif token == Token.Comma:
                f.write(' ,')
            elif token == Token.Semicolon:
                newline = True
                f.write(' ;\n')
            elif token == Token.Dot:
                newline = True
                indent -= 1
                f.write(' .\n')
            elif token == Token.Newline:
                newline = True
                f.write('\n')
            elif token == Token.Indent:
                indent += 1
            elif token == Token.Dedent:
                indent -= 1
            elif token == Token.Comment:
                newline = True
                f.write('# {0}\n'.format(syntax.strings.popleft()))
            elif token == Token.Whatever:
                f.write('{0}{1}'.format(
                    '' if was_newline else ' ',
                    syntax.strings.popleft(),
                ))


class Token:
    Comma = 1
    Comment = 2
    Dedent = 3
    Dot = 4
    Indent = 5
    Newline = 6
    PopBlank = 7
    Prefix = 8
    PushBlank = 9
    Semicolon = 10
    Whatever = 11


class Syntax:
    __slots__ = 'tokens', 'strings'

    def __init__(self):
        self.tokens = deque()
        self.strings = deque()

    def merge(self, other):
        self.tokens += other.tokens
        self.strings += other.strings


class Comment(Syntax):
    def __init__(self, comment):
        Syntax.__init__(self)
        self.tokens.append(Token.Comment)
        self.strings.append(comment)


class Prefix(Syntax):
    def __init__(self, prefix, base):
        Syntax.__init__(self)
        self.tokens.append(Token.Prefix)
        self.strings.append(prefix)
        self.strings.append(base)


class Statement(Syntax):
    def __init__(self, subject, value):
        Syntax.__init__(self)
        self.tokens.append(Token.Newline)
        self.tokens.append(Token.Whatever)
        self.strings.append(subject)
        self.tokens.append(Token.Indent)
        self.tokens.append(Token.Newline)
        self.merge(value)
        self.tokens.append(Token.Dot)


class Subject(Syntax):
    def __init__(self):
        Syntax.__init__(self)

    def predicate(self, predicate, object):
        if len(self.tokens) != 0:
            self.tokens.append(Token.Semicolon)
        self.tokens.append(Token.Whatever)
        self.strings.append(predicate)
        if is_string(object):
            object = Object().value(object)
        self.merge(object)
        return self


class Object(Syntax):
    def __init__(self):
        Syntax.__init__(self)

    def value(self, object):
        if len(self.tokens) != 0:
            self.tokens.append(Token.Comma)
        if is_string(object):
            self.tokens.append(Token.Whatever)
            self.strings.append(object)
        elif isinstance(object, Subject):
            self.tokens.append(Token.PushBlank)
            self.merge(object)
            self.tokens.append(Token.Newline)
            self.tokens.append(Token.PopBlank)
        else:
            raise TypeError
        return self


def uri(uri):
    return uri.join('<>')


def string(value, quote='"'):
    return str(value).join((quote, quote))


def integer(value):
    return str(value)


def decimal(value):
    ret = str(value)
    if '.' not in ret:
        ret += '.0'
    return ret


def write_ttl(metagen):
    metadata = subprocess.check_output(metagen)
    metadata = json.loads(metadata)

    with open('manifest.{0}'.format(metadata['ttlname']), 'w') as f:
        plugin = Subject()
        plugin.predicate('a', 'lv2:Plugin')
        plugin.predicate('lv2:binary', uri(metadata['dllname']))
        if metadata['replaced_uri']:
            plugin.predicate('dc:replaces', uri(metadata['replaced_uri']))
        plugin.predicate('rdfs:seeAlso', uri(metadata['ttlname']))
        TurtleWriter.write(f, Statement(uri(metadata['uri']), plugin))

    ttl    = (Prefix('bufsz', 'http://lv2plug.in/ns/ext/buf-size#'))
    ttl.merge(Prefix('dc', 'http://purl.org/dc/elements/1.1/'))
    ttl.merge(Prefix('doap', 'http://usefulinc.com/ns/doap#'))
    ttl.merge(Prefix('lv2', 'http://lv2plug.in/ns/lv2core#'))
    ttl.merge(Prefix('opts', 'http://lv2plug.in/ns/ext/options#'))
    ttl.merge(Prefix('pprops', 'http://lv2plug.in/ns/ext/port-props#'))
    ttl.merge(Prefix('rdfs', 'http://www.w3.org/2000/01/rdf-schema#'))
    ttl.merge(Prefix('state', 'http://lv2plug.in/ns/ext/state#'))
    ttl.merge(Prefix('urid', 'http://lv2plug.in/ns/ext/urid#'))

    plugin = Subject()
    plugin.predicate('a', 'lv2:Plugin')

    plugin.predicate('lv2:extensionData', 'state:interface')

    plugin.predicate(
        'lv2:requiredFeature',
        Object()
            .value('opts:options')
            .value('urid:map')
    )

    if metadata['is_rt_safe']:
        plugin.predicate('lv2:optionalFeature', 'lv2:hardRTCapable')

    plugin.predicate(
        'opts:supportedOption',
        Object()
            .value('bufsz:nominalBlockLength')
            .value('bufsz:maxBlockLength')
            .value('bufsz:sampleRate')
    )

    plugin.predicate('doap:name', string(metadata['name']))
    plugin.predicate('lv2:project', uri('https://github.com/grejppi/bitrot'))

    plugin.predicate('doap:license', uri(
        'https://spdx.org/licenses/{0}'.format(metadata['license']),
    ))

    minor = integer(metadata['version']['minor'])
    micro = integer(metadata['version']['micro'])

    plugin.predicate('lv2:minorVersion', integer(int(float(minor))))
    plugin.predicate('lv2:microVersion', integer(int(float(micro))))

    plugin.predicate('rdfs:comment', string(metadata['description'], '"""'))

    ports = (Object()
        .value(Subject()
            .predicate(
                'a',
                Object()
                    .value('lv2:InputPort')
                    .value('lv2:AudioPort'))
            .predicate('lv2:index', integer(0))
            .predicate('lv2:symbol', string('lin'))
            .predicate('lv2:name', string('Left In')))
        .value(Subject()
            .predicate(
                'a',
                Object()
                    .value('lv2:InputPort')
                    .value('lv2:AudioPort'))
            .predicate('lv2:index', integer(1))
            .predicate('lv2:symbol', string('rin'))
            .predicate('lv2:name', string('Right In')))
        .value(Subject()
            .predicate(
                'a',
                Object()
                    .value('lv2:OutputPort')
                    .value('lv2:AudioPort'))
            .predicate('lv2:index', integer(2))
            .predicate('lv2:symbol', string('lout'))
            .predicate('lv2:name', string('Left Out')))
        .value(Subject()
            .predicate(
                'a',
                Object()
                    .value('lv2:OutputPort')
                    .value('lv2:AudioPort'))
            .predicate('lv2:index', integer(3))
            .predicate('lv2:symbol', string('rout'))
            .predicate('lv2:name', string('Right Out'))))

    index = 4
    for param in metadata['params']:
        port = Subject()

        port.predicate('a', Object().value('lv2:InputPort').value('lv2:ControlPort'))
        port.predicate('lv2:index', integer(index))
        port.predicate('lv2:symbol', string(param['symbol']))
        port.predicate('lv2:name', string(param['name']))
        port.predicate('lv2:minimum', decimal(param['minimum']))
        port.predicate('lv2:maximum', decimal(param['maximum']))
        port.predicate('lv2:default', decimal(param['default']))

        pprops = Object()
        if param['trigger']:
            pprops.value('pprops:trigger')
        if param['toggled']:
            pprops.value('lv2:toggled')
        if param['integer']:
            pprops.value('lv2:integer')
        if param['logarithmic']:
            pprops.value('pprops:logarithmic')
        if not param['automatable']:
            pprops.value('pprops:expensive')
        if len(pprops.tokens) != 0:
            port.predicate('lv2:portProperty', pprops)

        ports.value(port)
        index += 1

    plugin.predicate('lv2:port', ports)
    ttl.merge(Statement(uri(metadata['uri']), plugin))

    with open(metadata['ttlname'], 'w') as f:
        TurtleWriter.write(f, ttl)


def finish_manifest():
    with open('manifest.ttl', 'w') as f:
        ttl     = Prefix('dc', 'http://purl.org/dc/terms/')
        ttl.merge(Prefix('doap', 'http://usefulinc.com/ns/doap#'))
        ttl.merge(Prefix('foaf', 'http://xmlns.com/foaf/0.1/'))
        ttl.merge(Prefix('lv2', 'http://lv2plug.in/ns/lv2core#'))
        ttl.merge(Prefix('rdfs', 'http://www.w3.org/2000/01/rdf-schema#'))

        project = Subject()
        project.predicate('a', 'doap:Project')
        project.predicate('doap:name', string('Bitrot'))

        maintainer = Subject()
        maintainer.predicate('a', 'foaf:Person')
        maintainer.predicate('foaf:nick', string('grejppi'))
        project.predicate('doap:maintainer', Object().value(maintainer))

        ttl.merge(Statement(
            uri('https://github.com/grejppi/bitrot'),
            project))

        TurtleWriter.write(f, ttl)

        for fname in glob.glob(os.path.abspath('manifest.*.ttl')):
            with open(fname) as part:
                f.write(part.read())


if __name__ == '__main__':
    args = deque(sys.argv)
    args.popleft()
    try:
        write_ttl(args.popleft())
    except IndexError:
        finish_manifest()
