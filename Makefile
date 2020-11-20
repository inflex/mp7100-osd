# 
# VERSION CHANGES
#

#BV=$(shell (git rev-list HEAD --count))
#BD=$(shell (date))
BV=1234
BD=today
SDLFLAGS=$(shell (sdl2-config --static-libs --cflags))
CFLAGS=  -O2 -DBUILD_VER="$(BV)" -DBUILD_DATE=\""$(BD)"\" -DFAKE_SERIAL=$(FAKE_SERIAL)
LIBS=-lSDL2_ttf
CC=gcc
GCC=g++

OBJ=csi3005p

default: $(OBJ)
	@echo
	@echo

csi3005p: csi3005p.cpp
	@echo Build Release $(BV)
	@echo Build Date $(BD)
	${GCC} ${CFLAGS} $(COMPONENTS) csi3005p.cpp $(SDLFLAGS) $(LIBS) ${OFILES} -o ${OBJ} 

clean:
	rm -v ${OBJ} 
