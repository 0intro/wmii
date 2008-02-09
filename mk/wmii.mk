VERSION = $$(hg tip --template 'hg{rev}' 2>/dev/null)
VERSION := $(shell hg tip --template 'hg{rev}' 2>/dev/null)
VERSION != hg tip --template 'hg{rev}' 2>/dev/null
CONFVERSION = 3.5
