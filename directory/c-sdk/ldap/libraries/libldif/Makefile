# Generated automatically from Makefile.in by configure.
# 
# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#  
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#  
# The Original Code is Mozilla Communicator client code, released
# March 31, 1998.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation. Portions created by Netscape are
# Copyright (C) 1998-1999 Netscape Communications Corporation. All
# Rights Reserved.
# 
# Contributor(s): 
#

MOD_DEPTH = ../../..
srcdir = .
topsrcdir = ../../..

include $(MOD_DEPTH)/config/autoconf.mk
include $(topsrcdir)/build.mk

SRCS		= line64.c

REALOBJS	= $(SRCS:.c=.$(OBJ_SUFFIX))
OBJS		= $(addprefix $(OBJDIR_NAME)/, $(REALOBJS))

HDIR		= $(topsrcdir)/ldap/include

LIBLDIF		= $(addprefix $(OBJDIR_NAME)/, $(LIB_PREFIX)$(LDIF_LIBNAME).$(LIB_SUFFIX))

INSTALLDIR      = $(DIST)/$(OBJDIR_NAME)

include $(topsrcdir)/config/rules.mk

LOCAL_INCLUDES  = -I$(PUBLIC)/nspr
INCLUDES	+= -I$(HDIR) -I$(INSTALLDIR)/include -I$(DIST)/include
DEFINES		+= $(DEFS)

PLATFORMCFLAGS	= -DUSE_WAITPID -DNEEDPROTOS
PLATFORMLIBS	=
THREADS		=
THREADSLIB	=

###########################################################################

versiont.c:	Makefile.client Version.c
	@$(RM) $@
	@(u="$${USER-root}" v="$(shell cat ../../build/version)" d="$(shell pwd)" \
	h="$(shell hostname)" t="$(shell date)"; $(SED) -e "s|%WHEN%|$${t}|" \
	-e "s|%WHOANDWHERE%|$${u}@$${h}:$${d}|" \
	-e "s|%VERSION%|$${v}|" \
	< Version.c > $@)

export::    $(OBJDEST) $(LIBDIR) $(OBJS) $(LIBLDIF)

$(LIBDIR):
	$(MKDIR) $(LIBDIR)

$(LIBLDIF): $(OBJS) $(LIBDIR)
	@echo ======= making $(LIBLDIF)
ifdef SO_FILES_TO_REMOVE
	-$(RM) $(SO_FILES_TO_REMOVE)
endif
	$(LINK_LIB)

veryclean:: clean

$(OBJDEST):
	$(MKDIR) $(OBJDEST)

export::	$(LIBLDIF)
	$(INSTALL) -m 444 $(LIBLDIF) $(dist_libdir)
