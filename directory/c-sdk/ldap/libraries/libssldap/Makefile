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

MOD_DEPTH	= ../../..
srcdir		= .
topsrcdir 	= ../../..

include $(MOD_DEPTH)/config/autoconf.mk
include $(topsrcdir)/build.mk

SRCS		= clientinit.c \
		  ldapsinit.c \
		  errormap.c

REALOBJS        = $(SRCS:.c=.$(OBJ_SUFFIX))

SSLOBJDEST	= $(OBJDIR_NAME)
OBJS		= $(addprefix $(SSLOBJDEST)/, $(REALOBJS))

DISTHDIR	= $(DIST)/public/ldap
HDIR		= $(topsrcdir)/ldap/include

LIBSSLDAP	= $(addprefix $(SSLOBJDEST)/, $(LIB_PREFIX)$(SSLDAP_LIBNAME).$(LIB_SUFFIX))
DLLSSLDAP	= $(addprefix $(SSLOBJDEST)/, $(LIB_PREFIX)$(SSLDAP_LIBNAME).$(DLL_SUFFIX))

INSTALLDIR      = $(DIST)/$(OBJDIR_NAME)

include $(topsrcdir)/config/rules.mk

LOCAL_INCLUDES  = -I$(PUBLIC)/nspr
INCLUDES	+= -I$(DISTHDIR) -I$(HDIR) -I$(INSTALLDIR)/include \
		   -I$(DIST)/include \
		   -I$(DIST)/public/security \
		   -I$(DIST)/public/nss \
                   -I$(DIST)/include/nspr
DEFINES		+= $(DEFS)

PLATFORMCFLAGS	= -DUSE_WAITPID -DNEEDPROTOS
PLATFORMLIBS	=
THREADS		=
THREADSLIB	=

#
# shared library symbol export definitions
#
ifeq ($(USE_DLL_EXPORTS_FILE), 1)
ifeq ($(OS_ARCH), WINNT)
GENEXPORTS=cmd /c  $(PERL) $(topsrcdir)/ldap/build/genexports.pl
else
GENEXPORTS=$(PERL) $(topsrcdir)/ldap/build/genexports.pl
endif

# variable definitions for exported symbols
ifeq ($(OS_ARCH), WINNT)
        SSLDAP_EXPORT_DEFS= $(srcdir)/../msdos/winsock/nsldapssl32.def
else
        SSLDAP_EXPORT_DEFS= $(SSLOBJDEST)/libldap_ssl.exp
endif

SSLDAP_EXPORT_FLAGS=$(addprefix $(DLLEXPORTS_PREFIX), $(SSLDAP_EXPORT_DEFS))

GENEXPARGS=$(BUILD_DEBUG) $(SSLDAPVERS_SUFFIX) $(SSLDAPVERS)
endif # USE_DLL_EXPORTS_FILE

ifeq ($(OS_ARCH), SunOS)
EXTRA_LIBS = -L(dist_libdir) -l$(LDAP_LIBNAME) 
EXTRA_LIBS += $(DYNAMICNSPR)
endif

ifeq ($(OS_ARCH), WINNT)
EXTRA_LIBS =wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib \
            comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib \
            rpcrt4.lib uuid.lib odbc32.lib odbccp32.lib winmm.lib
EXTRA_LIBS += $(dist_libdir)/$(LDAP_LIBNAME).lib
EXTRA_LIBS += $(dist_libdir)/$(PRLDAP_LIBNAME).lib
EXTRA_LIBS += $(dist_libdir)/$(PLC_BASENAME).lib
EXTRA_LIBS += $(dist_libdir)/$(PLDS_BASENAME).lib
EXTRA_LIBS += $(dist_libdir)/$(NSPR_BASENAME).lib
EXTRA_LIBS += $(dist_libdir)/$(NSS_LIBNAME).lib
ifeq ($(NSS_DYNAMIC_SOFTOKN),1)
EXTRA_LIBS += $(dist_libdir)/$(SOFTOKN_LIBNAME).lib
endif
EXTRA_LIBS += $(dist_libdir)/$(SSL_LIBNAME).lib
endif

ifeq ($(OS_ARCH),OS2)
EXTRA_LIBS = $(OS_LIBS)
EXTRA_LIBS += $(dist_libdir)/$(LDAP_LIBNAME).lib
EXTRA_LIBS += $(dist_libdir)/$(PRLDAP_LIBNAME).lib
EXTRA_LIBS += $(dist_libdir)/$(PLCBASE).lib
EXTRA_LIBS += $(dist_libdir)/$(PLDSBASE).lib
EXTRA_LIBS += $(dist_libdir)/$(NSPRBASE).lib
EXTRA_LIBS += $(dist_libdir)/$(NSS_LIBNAME).lib
ifeq ($(NSS_DYNAMIC_SOFTOKN),1)
EXTRA_LIBS += $(dist_libdir)/$(SOFTOKN_LIBNAME).lib
endif
EXTRA_LIBS += $(dist_libdir)/$(SSL_LIBNAME).lib
endif

ifeq ($(OS_ARCH), OSF1)
EXTRA_LIBS = -L$(dist_libdir) -l$(LDAP_LIBNAME)
EXTRA_LIBS += $(DYNAMICNSPR)
EXTRA_LIBS += -lcxx -lpthread -lrt -lmach -lexc
endif

ifeq ($(OS_ARCH), AIX)
EXTRA_LIBS = -L$(dist_libdir) -l$(LDAP_LIBNAME)
EXTRA_LIBS += $(DYNAMICNSPR) -l$(NSS_LIBNAME)
ifeq ($(NSS_DYNAMIC_SOFTOKN),1)
EXTRA_LIBS += -l$(SOFTOKN_LIBNAME)
endif
EXTRA_LIBS += -l$(SSL_LIBNAME) -l $(PRLDAP_LIBNAME)
EXTRA_LIBS += -ldl -brtl -lpthreads -lc_r -lm
endif

ifeq ($(OS_ARCH), IRIX)
EXTRA_LIBS = -L$(dist_libdir) -l$(LDAP_LIBNAME)
EXTRA_LIBS += $(DYNAMICNSPR) -l$(NSS_LIBNAME)
ifeq ($(NSS_DYNAMIC_SOFTOKN),1)
EXTRA_LIBS += -l$(SOFTOKN_LIBNAME)
endif
EXTRA_LIBS += -l$(SSL_LIBNAME) -l $(PRLDAP_LIBNAME)
EXTRA_LIBS += $(OS_LIBS) -lc
endif

ifeq ($(OS_ARCH), Darwin)
EXTRA_LIBS = -L$(dist_libdir) -l$(LDAP_LIBNAME) -l$(PRLDAP_LIBNAME)
ifeq ($(NSS_DYNAMIC_SOFTOKN),1)
EXTRA_LIBS += -l$(SOFTOKN_LIBNAME)
endif
EXTRA_LIBS += -l$(SSL_LIBNAME) -l$(NSS_LIBNAME)
EXTRA_LIBS += $(DYNAMICNSPR) 
EXTRA_LIBS += $(OS_LIBS)
endif

ifeq ($(OS_ARCH), BeOS)
EXTRA_LIBS = -L$(dist_libdir) -l$(LDAP_LIBNAME) -l$(PRLDAP_LIBNAME)
ifeq ($(NSS_DYNAMIC_SOFTOKN),1)
EXTRA_LIBS += -l$(SOFTOKN_LIBNAME)
endif
EXTRA_LIBS += -l$(SSL_LIBNAME) -l$(NSS_LIBNAME)
EXTRA_LIBS += $(DYNAMICNSPR) 
EXTRA_LIBS += $(OS_LIBS)
endif

OBJDEST = $(OBJDIR_NAME)

###########################################################################

ifeq ($(USE_DLL_EXPORTS_FILE), 1)
# recursive gmake rule to create exported symbols file
$(SSLDAP_EXPORT_DEFS):: ../libldap_ssl.ex
ifeq ($(OS_ARCH), WINNT)
	$(GENEXPORTS) Win32 Standard $< $(GENEXPARGS) > $@
else
	$(GENEXPORTS) $(OS_ARCH) Standard $< $(GENEXPARGS) > $@
endif
endif # USE_DLL_EXPORTS_FILE

versiont.c:	Makefile.client Version.c
	@$(RM) $@
	@(u="$${USER-root}" v="$(shell cat ../../build/version)" d="$(shell pwd)" \
	h="$(shell hostname)" t="$(shell date)"; $(SED) -e "s|%WHEN%|$${t}|" \
	-e "s|%WHOANDWHERE%|$${u}@$${h}:$${d}|" \
	-e "s|%VERSION%|$${v}|" \
	< Version.c > $@)

export::    $(OBJDEST) $(LIBDIR) $(OBJS) $(DLLSSLDAP)

$(LIBDIR):
	$(MKDIR) $(LIBDIR)

$(LIBSSLDAP): $(OBJS) $(LIBDIR) $(SSLDAP_EXPORT_DEFS)
	@echo ======= making $(LIBSSLDAP)
 ifdef SO_FILES_TO_REMOVE
	-$(RM) $(SO_FILES_TO_REMOVE)
 endif
	$(LINK_LIB) $(EXTRA_LIBS)

$(DLLSSLDAP): $(OBJS) $(LIBDIR) $(SSLDAP_EXPORT_DEFS)
	@echo ======= making $(DLLSSLDAP)
ifdef SO_FILES_TO_REMOVE
	-$(RM) $(SO_FILES_TO_REMOVE)
endif
	$(LINK_DLL) $(SSLDAP_EXPORT_FLAGS) $(EXTRA_LIBS)

veryclean:: clean

$(OBJDEST):
	$(MKDIR) $(OBJDEST)

export:: $(DLLSSLDAP)
ifeq ($(OS_ARCH), WINNT)
	$(INSTALL) -m 555 $(LIBSSLDAP) $(dist_libdir)
	$(INSTALL) -m 555 $(DLLSSLDAP) $(dist_libdir)
endif
ifdef MKSHLIB
	$(INSTALL) -m 555 $(DLLSSLDAP) $(dist_libdir)
	$(INSTALL) -m 444 $(DLLSSLDAP) $(dist_bindir)
endif
