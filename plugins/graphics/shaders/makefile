# Regenerate the embedded headers, like bgfx/examples/common/imgui.
BGFX_DIR ?= ../3rdparty/bgfx/bgfx

VS_FLAGS += --varyingdef varying.def.sc
FS_FLAGS += --varyingdef varying.def.sc
# Resolve the common includes used by shaders copied from bgfx examples.
VS_FLAGS += -i $(BGFX_DIR)/examples/common/
FS_FLAGS += -i $(BGFX_DIR)/examples/common/
CS_FLAGS += -i $(BGFX_DIR)/examples/common/

include $(BGFX_DIR)/scripts/shader-embeded.mk

# Use the renderer-owned tool by default. SHADERC=... can select a local build.
SHADERC := "../tools/bin/$(OS)/shaderc$(if $(filter windows,$(OS)),.exe)"

# Direct3D compilation is available on Windows and Linux (Linux DXBC needs Wine).
# macOS builds the portable profiles and Metal. Override SHADER_PROFILES to select
# a different set; every selected profile must succeed before a header is replaced.
SHADER_PROFILES ?= glsl essl spv wgsl $(if $(filter windows linux,$(OS)),dxbc dxil) mtl
ifneq ($(strip $(filter-out glsl essl spv wgsl dxbc dxil mtl,$(SHADER_PROFILES))),)
$(error Unknown SHADER_PROFILES: $(filter-out glsl essl spv wgsl dxbc dxil mtl,$(SHADER_PROFILES)))
endif
ifeq ($(strip $(SHADER_PROFILES)),)
$(error SHADER_PROFILES must select at least one profile)
endif

define frost_embed_shader
	@echo [$<]
	$(SILENT) set -eu; \
		stage="$@.stage"; profile="$@.profile"; \
		trap 'rm -f "$$stage" "$$profile"' 0 1 2 3 15; \
		: > "$$stage"; \
		for variant in $(SHADER_PROFILES); do \
			case $$variant in \
				glsl) set -- linux 430 3 ;; \
				essl) set -- android $(2) 3 ;; \
				spv)  set -- linux spirv 3 ;; \
				wgsl) set -- linux wgsl 3 ;; \
				dxbc) set -- windows s_5_0 $(3) ;; \
				dxil) set -- windows s_6_0 $(3) ;; \
				mtl)  set -- ios metal 3 ;; \
			esac; \
			$(SHADERC) $(1) --platform "$$1" -p "$$2" -O "$$3" -f "$<" -o "$$profile" --bin2c $(basename $<)_$$variant; \
			cat "$$profile" >> "$$stage"; \
		done; \
		printf "extern const uint8_t* $(basename $<)_pssl;\nextern const uint32_t $(basename $<)_pssl_size;\n" >> "$$stage"; \
		mv -f "$$stage" "$@"
endef

vs_%.bin.h: vs_%.sc
	$(call frost_embed_shader,$(VS_FLAGS),300_es,3)

fs_%.bin.h: fs_%.sc
	$(call frost_embed_shader,$(FS_FLAGS),300_es,3)

cs_%.bin.h: cs_%.sc
	$(call frost_embed_shader,$(CS_FLAGS),310_es,1)

# Compare settings directly: older Make versions only compare whole-second
# timestamps. Publish settings after all headers succeed, so failures are retried.
SHADER_SETTINGS := $(strip $(SHADER_PROFILES))|$(SHADERC)|$(VS_FLAGS)|$(FS_FLAGS)|$(CS_FLAGS)
ifneq ($(shell cat .shader-settings 2>/dev/null),$(SHADER_SETTINGS))
.PHONY: frost-shader-settings
$(BIN): frost-shader-settings
all:
	@printf '%s\n' '$(SHADER_SETTINGS)' > .shader-settings.tmp
	@mv -f .shader-settings.tmp .shader-settings
endif

$(BIN): $(wildcard $(subst ",,$(SHADERC))) $(wildcard *.sh) makefile varying.def.sc \
	$(wildcard $(BGFX_DIR)/examples/common/*.sh) $(wildcard $(BGFX_DIR)/src/*.sh) \
	$(BGFX_DIR)/scripts/shader-embeded.mk $(BGFX_DIR)/scripts/tools.mk
