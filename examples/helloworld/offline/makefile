# Compile the example mesh with the renderer-owned tool, like graphics shaders.
BGFX_DIR ?= ../../../plugins/graphics/3rdparty/bgfx/bgfx

include $(BGFX_DIR)/scripts/tools.mk

# Set BUILD_DIR and CONFIG to select another CMake build, or override MESHC.
BUILD_DIR ?= ../../../build
CONFIG ?= Debug
MESHC ?= "$(BUILD_DIR)/$(CONFIG)/$(FROST_TOOLS_PREFIX)meshc$(if $(filter windows,$(OS)),.exe)"

.PHONY: all clean

all: ../runtime/cube.mesh

../runtime/cube.mesh: cube.obj makefile $(wildcard $(subst ",,$(MESHC)))
	$(MESHC) -f "$<" -o "$@" --guid a79c1f66-a424-4e05-8bcd-4a2ac19d14fb --packnormal 1 --tangent

clean:
	$(RM) ../runtime/cube.mesh
