# CREDITS:
# This boiler plate code is heavily adapted from Intel Connectivity
# Academy course ICA-1132: "Barefoot Runtime Interface & PTF"

ifndef SDE_INSTALL
$(error Environment variable SDE_INSTALL is not set)
endif

PROG=dyso

CPPFLAGS = -I$(SDE_INSTALL)/include -DSDE_INSTALL=\"$(SDE_INSTALL)\" \
           -DPROG_NAME=\"$(PROG)\" -D__DIR__=\"${CURDIR}\"
CXXFLAGS = -g -std=c++11 -Wall -Wextra -Werror -MMD -MF $@.d
CFLAGS   = -g -Wall -Wextra -Werror -MMD -MF $@.d  # added as per latest ICA-1132 slides
BF_LIBS  = -L$(SDE_INSTALL)/lib -ldriver -lbfutils # -lbfsys #-lbf_switch_lib
LDLIBS   = $(BF_LIBS) -lm -ldl -lpthread
LDFLAGS  = -Wl,-rpath,$(SDE_INSTALL)/lib

DEPS := $(OBJS:.o=.o.d)

.PHONY: all clean

# Final target
all: $(PROG)

clean:
	-@rm -rf $(PROG) *~ *.o *.d *.tofino *.tofino2 zlog-cfg-cur bf_drivers.log*
