#!/usr/bin/env python3

from   collections import defaultdict
import json
import re



taglist = defaultdict(dict)
taglist['0'] =\
{
    'id': 0,
    'name': 'Empty',
    'type': 'token',
    'rebuild': 0
}

with open('../json/charactermap.json') as f:
    characters = json.load(f)
for character in characters:
    character['type'] = 'character'
    for key in ['id', 'type', 'name', 'world']:
        taglist[character['id']][key] = character[key]
    taglist[character['name']] = taglist[character['id']]

with open('../json/tokenmap.json') as f:
    tokens = json.load(f)
for token in tokens:
    token['type'] = 'token'
    del token['upgrademap']
    token['name'] = re.sub('^\* ', '', token['name'])
    for key in ['id', 'type', 'name', 'rebuild']:
        taglist[token['id']][key] = token[key]
    taglist[token['name']] = taglist[token['id']]

with open('../json/taglist.json', 'w') as f:
    f.write(json.dumps(taglist, indent=4))
