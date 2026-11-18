# Compile example assets with the renderer-owned tools.
BGFX_DIR ?= ../../../plugins/graphics/3rdparty/bgfx/bgfx

include $(BGFX_DIR)/scripts/tools.mk

BUILD_DIR ?= ../../../build
CONFIG ?= Debug
MESHC ?= "$(BUILD_DIR)/$(CONFIG)/$(FROST_TOOLS_PREFIX)meshc$(if $(filter windows,$(OS)),.exe)"
TEXTUREC := "$(BUILD_DIR)/$(CONFIG)/$(FROST_TOOLS_PREFIX)texturec$(if $(filter windows,$(OS)),.exe)"
MATERIALC ?= "$(BUILD_DIR)/$(CONFIG)/$(FROST_TOOLS_PREFIX)materialc$(if $(filter windows,$(OS)),.exe)"

MESHES := mesh_cube mesh_bunny
TEXTURES := tex_moon
MATERIALS := mat_floor mat_bunny
ASSETS := $(addprefix ../runtime/,$(addsuffix .bin,$(MESHES) $(TEXTURES) $(MATERIALS)))

.PHONY: all clean
all: $(ASSETS)

$(addprefix ../runtime/,$(addsuffix .bin,$(MESHES))): ../runtime/%.bin: %.asset makefile $(wildcard $(subst ",,$(MESHC)))
	$(MESHC) --config "$<" -o "$@"

$(addprefix ../runtime/,$(addsuffix .bin,$(TEXTURES))): ../runtime/%.bin: %.asset makefile $(wildcard $(subst ",,$(TEXTUREC)))
	$(TEXTUREC) --config "$<" -o "$@"

$(addprefix ../runtime/,$(addsuffix .bin,$(MATERIALS))): ../runtime/%.bin: %.asset makefile $(wildcard $(subst ",,$(MATERIALC)))
	$(MATERIALC) -f "$<" -o "$@"

../runtime/mesh_cube.bin: cube.obj
../runtime/mesh_bunny.bin: bunny.obj
../runtime/tex_moon.bin: moon.jpg

clean:
	$(RM) $(ASSETS)
