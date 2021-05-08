#

# ソースの依存関係ファイル (自動生成)
DEPS = Dependencies
DEPENDENCIES_OUTPUT := $(DEPS)

# デバイスドライバー標準ルール
ifdef BD
  include $(BD)/driver/etc/makerules
else
  include ../../etc/makerules
endif

# ----------------------------------------------------------------------------

# 作成対象
TARGET = vmxscreen

# ソースファイルのサーチパス
S = ../src
VPATH = $(S)

CFLAGS += -Wall
HEADER += $(S)
SRC	= main.c common.c conf.c vmsvga.c bga.c none.c
OBJ	= $(addsuffix .o, $(basename $(SRC)))
SRC.C	= $(filter %.C, $(SRC))
LDLIBS += -lbms

ifneq ($(filter cmap256, $(options)), )
  CFLAGS += -DCOLOR_CMAP256
endif
ifneq ($(filter rgb565, $(options)), )
  CFLAGS += -DCOLOR_RGB565
endif

# ----------------------------------------------------------------------------

.PHONY: all clean install

ALL = $(TARGET) $(TARGET).map

all: $(ALL)

$(TARGET): $(OBJ)
	$(LINK.o) $(LDOBJS) $^ $(LOADLIBES) $(LDLIBS) $(OUTPUT_OPTION)

clean:
	$(RM) $(OBJ) $(SRC.C:%.C=%.c) $(ALL) $(DEPS)

install: $(ALL:%=$(EXE_INSTALLDIR)/%)

$(TARGET:%=$(EXE_INSTALLDIR)/%): $(EXE_INSTALLDIR)/%: %
	$(EXE_INSTALL_BZCOMP)

# ソースの依存関係
ifdef DEPENDENCIES_OUTPUT
  $(DEPS): ; touch $(DEPS)
else
  $(DEPS): $(SRC) ; $(MAKEDEPS) $@ $?
endif
-include $(DEPS)

$(SRC.C:%.C=%.c):
