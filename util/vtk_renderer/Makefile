.PHONY: all

CXX = c++
CXXFLAGS=-std=c++20 -O3 -I/nix/store/31vfnljfr5jjh74h4j4v5a1r6w57g6zp-vtk-9.2.6/include/vtk
LIBS=-I/nix/store/31vfnljfr5jjh74h4j4v5a1r6w57g6zp-vtk-9.2.6/lib -lvtkCommonCore -lvtkRenderingCore -lvtkRenderingOpenGL2 -lvtksys -lvtkFiltersGeometry -lvtkCommonExecutionModel -lvtkIOImage -lvtkIOCore -lvtkIOGeometry -lvtkIOLegacy

TARGET = kel_vtk_renderer

all: ${TARGET}

${TARGET}: ${TARGET}.cpp
	${CXX} ${CXXFLAGS} ${LIBS} -o $@ $^
