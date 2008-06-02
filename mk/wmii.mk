VERS = hg$$(hg identify -n)
VERS = hg$$(hg log -r $$(hg id | awk -F'[+ ]' '{print $$1}') --template '{rev}')
VERSION = $(VERS)
VERSION := $(shell echo $(VERS) 2>/dev/null)
VERSION != echo $(VERS) 2>/dev/null
CONFVERSION = hg

