VERS = hg$$(hg identify -n)
VERS = hg$$(hg log -r $$(hg id 2>/dev/null | awk -F'[+ ]' '{print $$1}') --template '{rev}' 2>/dev/null)
VERSION = $(VERS)
VERSION := $(shell echo $(VERS))
VERSION != echo $(VERS)
CONFVERSION = hg

