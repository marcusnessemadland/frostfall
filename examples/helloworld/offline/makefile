# Compile example assets with the renderer-owned tools.
BGFX_DIR ?= ../../../plugins/graphics/3rdparty/bgfx/bgfx

include $(BGFX_DIR)/scripts/tools.mk

BUILD_DIR ?= ../../../build
CONFIG ?= Debug
MESHC ?= "$(BUILD_DIR)/$(CONFIG)/$(FROST_TOOLS_PREFIX)meshc$(if $(filter windows,$(OS)),.exe)"
TEXTUREC := "$(BUILD_DIR)/$(CONFIG)/$(FROST_TOOLS_PREFIX)texturec$(if $(filter windows,$(OS)),.exe)"
MATERIALC ?= "$(BUILD_DIR)/$(CONFIG)/$(FROST_TOOLS_PREFIX)materialc$(if $(filter windows,$(OS)),.exe)"

MESHES := cube orb
TEXTURES := stone-color stone-normal parameters mask
MATERIALS := stone metal masked emissive floor
ASSETS := $(addprefix ../runtime/,$(addsuffix .bin,$(MESHES) $(TEXTURES) $(MATERIALS)))

.PHONY: all clean
all: $(ASSETS)

$(addprefix ../runtime/,$(addsuffix .bin,$(MESHES))): ../runtime/%.bin: %.asset makefile $(wildcard $(subst ",,$(MESHC)))
	$(MESHC) --config "$<" -o "$@"

$(addprefix ../runtime/,$(addsuffix .bin,$(TEXTURES))): ../runtime/%.bin: %.asset makefile $(wildcard $(subst ",,$(TEXTUREC)))
	$(TEXTUREC) --config "$<" -o "$@"

$(addprefix ../runtime/,$(addsuffix .bin,$(MATERIALS))): ../runtime/%.bin: %.asset makefile $(wildcard $(subst ",,$(MATERIALC)))
	$(MATERIALC) -f "$<" -o "$@"

../runtime/cube.bin: cube.obj
../runtime/orb.bin: orb.obj stone.asset metal.asset
../runtime/stone-color.bin: parallax-d.png
../runtime/stone-normal.bin: parallax-n.png
../runtime/parameters.bin: parameters.tga
../runtime/mask.bin: texture-alpha-test.png
../runtime/stone.bin: stone-color.asset stone-normal.asset parameters.asset
../runtime/masked.bin: mask.asset
../runtime/emissive.bin: stone-color.asset

clean:
	$(RM) $(ASSETS)
