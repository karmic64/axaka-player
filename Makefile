ifdef COMSPEC
DOTEXE:=.exe
else
DOTEXE:=
endif


SRC:=axaka-player gb-sound axaka-sound axaka-sequencer axaka-sequencer-files machine
DEP:=$(addprefix dep/,$(addsuffix .dep,$(SRC)))
OBJ:=$(addprefix obj/,$(addsuffix .obj,$(SRC)))


CFLAGS:=-Ofast -flto -Wall -Wextra -Wpedantic $(shell sdl2-config --cflags)
LDFLAGS:=-s
LDLIBS:=$(shell sdl2-config --libs) -lSDL2_gfx -lm


dep/%.dep: %.c
	$(CC) $(CFLAGS) -M -MF $@ -MP -MT obj/$*.obj $<

obj/%.obj: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

axaka-player$(DOTEXE): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)


-include $(DEP)


.PHONY: default clean
default: axaka-player$(DOTEXE)

clean:
	$(RM) $(DEP)
	$(RM) $(OBJ)
	$(RM) axaka-player axaka-player.exe


