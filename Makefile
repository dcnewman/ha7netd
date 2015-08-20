# Copyright (c) 2005, Daniel C. Newman <dan.newman@mtbaldy.us>
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
#  + Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#
#  + Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in
#    the documentation and/or other materials provided with the
#    distribution.
#
#  + Neither the name of mtbaldy.us nor the names of its contributors
#    may be used to endorse or promote products derived from this
#   software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
# FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
# COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
# OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
# AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
# OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.

UNAME_OS := $(subst /,_,$(shell uname -s))
UNAME_ARCH := $(subst /,_,$(shell uname -p))

ifeq ($(UNAME_OS), SunOS)
BUILD_OS = Solaris
endif

ifeq ($(UNAME_OS), Darwin)
BUILD_OS = Darwin
endif

ifeq ($(BUILD_OS), Darwin)
RM = rm -f
MKDIR = mkdir -p
SED = sed
OBJ = o
CC = cc
CFLAGS = -g 
LDLIBS = -lpthread
MAKEDEP = $(CC) -MM
endif

ifeq ($(BUILD_OS), Solaris)
RM = rm -f
MKDIR = mkdir -p
SED = sed
OBJ = o
CC = cc
CFLAGS = -g -mt
LDLIBS = -lresolv -lsocket -lnsl -lpthread -lrt -lm
MAKEDEP = $(CC) -xM1
endif

OBJDIR = $(BUILD_OS)_$(UNAME_ARCH)_obj

HDR_TARGETS = bm_const.h xml_const.h xml_const.xsl

EXE_TARGETS = \
	$(OBJDIR)/make_includes \
	$(OBJDIR)/crc \
	$(OBJDIR)/ha7netd \
	$(OBJDIR)/search

EXE_SRCS = \
	make_includes.c \
	crc_cli.c \
	ha7netd.c \
	ha7netd_opt.c \
	ha7netd_os.c \
	search.c

LIB_SRCS = \
	atmos.c \
	bm.c \
	convert.c \
	crc.c \
	daily.c \
	device.c \
	ds18s20.c \
	eds_aprobe.c \
	err.c \
	glob.c \
	hbi_h3r1.c \
	ha7net.c \
	http.c \
	opt.c \
	os.c \
	os_socket.c \
	tai_8540.c \
	tai_8570.c \
	utils.c \
	vapor.c \
	weather.c \
	xml.c

LIB_OBJECTS = $(addprefix $(OBJDIR)/,$(LIB_SRCS:%.c=%.o))

all : $(EXE_TARGETS)

main : $(OBJDIR) $(HDR_TARGETS) $(EXE_TARGETS)

$(OBJDIR) :
	-@$(MKDIR) $(OBJDIR)

$(OBJDIR)/make_includes : $(OBJDIR)/make_includes.$(OBJ) $(OBJDIR)/opt.$(OBJ) \
			  $(OBJDIR)/os.$(OBJ) $(OBJDIR)/err.$(OBJ) \
			  $(OBJDIR)/bm.$(OBJ)
	-@$(MKDIR) $(OBJDIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

$(OBJDIR)/crc : $(OBJDIR)/crc_cli.$(OBJ) $(LIB_OBJECTS)
	-@$(MKDIR) $(OBJDIR)
	$(CC) -o $@ $^ $(LDLIBS)

$(OBJDIR)/ha7netd : $(OBJDIR)/ha7netd.$(OBJ) $(OBJDIR)/ha7netd_os.$(OBJ) \
		    $(OBJDIR)/ha7netd_opt.$(OBJ) $(LIB_OBJECTS)
	-@$(MKDIR) $(OBJDIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

$(OBJDIR)/search : $(OBJDIR)/search.$(OBJ) $(LIB_OBJECTS)
	-@$(MKDIR) $(OBJDIR)
	$(CC) -o $@ $^ $(LDLIBS)

bm_const.h xml_const.h xml_const.xsl : $(OBJDIR)/make_includes \
				       make_includes.conf
	@$(OBJDIR)/make_includes make_includes.conf $@

-include $(addprefix $(OBJDIR)/,$(LIB_SRCS:.c=.d))
-include $(addprefix $(OBJDIR)/,$(EXE_SRCS:.c=.d))

$(OBJDIR)/%.d : %.c bm_const.h xml_const.h xml_const.xsl
	@-set -e; $(MKDIR) $(OBJDIR); $(RM) $@; \
	  $(MAKEDEP) $(CPPFLAGS) $< > $@.tmp; \
	  $(SED) -e 's|\($*\)\.o[ :]|\$(OBJDIR)/\1.\$(OBJ) $@ : |g' \
	  < $@.tmp > $@; $(RM) $@.tmp

$(OBJDIR)/%.o : %.c
	-@$(MKDIR) $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

clean :
	-@$(RM) $(EXE_TARGETS) $(OBJDIR)/*.$(OBJ) $(OBJDIR)/*.d $(HDR_TARGETS)
