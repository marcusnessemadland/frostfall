# Regenerate the embedded headers, like bgfx/examples/common/imgui.
BGFX_DIR ?= ../3rdparty/bgfx/bgfx

VS_FLAGS += --varyingdef varying.def.sc
FS_FLAGS += --varyingdef varying.def.sc

include $(BGFX_DIR)/scripts/shader-embeded.mk

# Use the renderer-owned tool by default. SHADERC=... can select a local build.
SHADERC := "../tools/bin/$(OS)/shaderc$(if $(filter windows,$(OS)),.exe)"

# Keep the upstream profiles/flags, but fail on every profile error. The upstream
# example ignores optional compiler failures and can concatenate stale temporary
# data; engine headers must be published only after all variants succeed.
define cevy_embed_shader
	@echo [$<]
	$(SILENT) $(SHADERC) $(1) --platform linux -p 430 -f $< -o "$@.profile" --bin2c $(basename $<)_glsl
	@cat "$@.profile" > "$@.stage"
	$(SILENT) $(SHADERC) $(1) --platform android -p $(2) -f $< -o "$@.profile" --bin2c $(basename $<)_essl
	@cat "$@.profile" >> "$@.stage"
	$(SILENT) $(SHADERC) $(1) --platform linux -p spirv -f $< -o "$@.profile" --bin2c $(basename $<)_spv
	@cat "$@.profile" >> "$@.stage"
	$(SILENT) $(SHADERC) $(1) --platform linux -p wgsl -f $< -o "$@.profile" --bin2c $(basename $<)_wgsl
	@cat "$@.profile" >> "$@.stage"
	$(SILENT) $(SHADERC) $(1) --platform windows -p s_5_0 -O $(3) -f $< -o "$@.profile" --bin2c $(basename $<)_dxbc
	@cat "$@.profile" >> "$@.stage"
	$(SILENT) $(SHADERC) $(1) --platform windows -p s_6_0 -O $(3) -f $< -o "$@.profile" --bin2c $(basename $<)_dxil
	@cat "$@.profile" >> "$@.stage"
	$(SILENT) $(SHADERC) $(1) --platform ios -p metal -O 3 -f $< -o "$@.profile" --bin2c $(basename $<)_mtl
	@cat "$@.profile" >> "$@.stage"
	@printf "extern const uint8_t* $(basename $<)_pssl;\nextern const uint32_t $(basename $<)_pssl_size;\n" >> "$@.stage"
	@mv -f "$@.stage" "$@"
	@rm -f "$@.profile"
endef

vs_%.bin.h: vs_%.sc
	$(call cevy_embed_shader,$(VS_FLAGS),300_es,3)

fs_%.bin.h: fs_%.sc
	$(call cevy_embed_shader,$(FS_FLAGS),300_es,3)

cs_%.bin.h: cs_%.sc
	$(call cevy_embed_shader,$(CS_FLAGS),310_es,1)

$(BIN): $(wildcard *.sh) makefile varying.def.sc $(BGFX_DIR)/src/bgfx_shader.sh \
	$(BGFX_DIR)/scripts/shader-embeded.mk $(BGFX_DIR)/scripts/tools.mk
