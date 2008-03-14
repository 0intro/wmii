VERS = hg$$(hg identify -n)
VERSION = $(VERS)
VERSION := $(shell echo $(VERS) 2>/dev/null)
VERSION != echo $(VERS) 2>/dev/null
CONFVERSION = 3.5
