# Shared relay math: built into the firmware (host and STM32F407 targets pull the
# sources directly), into a host static library for tools, and into a WASM
# module for the setting tool.
CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O2
BUILD = build
SRCS = toc_curve.c pu_scale.c settings_rules.c
TOOL_WASM = ../../tool/src/wasm/relay_math.js
EMCC ?= emcc
EXPORTS = _rm_curve_count,_rm_curve_name,_rm_curve_from_name,_rm_toc_operate_time,_rm_toc_reset_time,\
_rm_a_per_count,_rm_ct_ratio,_rm_secondary_a,_rm_base_a_from_mva,_rm_pu_from_a,\
_rm_rules_input_count,_rm_rules_input_name,_rm_rule_count,_rm_rule_id,_rm_rule_message,\
_rm_rules_check,_rm_violation_code,_rm_violation_detail,_rm_violation_value,_malloc,_free

.PHONY: all lib vectors wasm clean

all: lib vectors

$(BUILD):
	mkdir -p $(BUILD)

lib: $(BUILD)/librelay_math.a

$(BUILD)/librelay_math.a: $(SRCS) *.h | $(BUILD)
	$(CC) $(CFLAGS) -c $(SRCS)
	mv *.o $(BUILD)/
	ar rcs $@ $(BUILD)/*.o

$(BUILD)/gen_vectors: gen_vectors.c $(SRCS) *.h | $(BUILD)
	$(CC) $(CFLAGS) gen_vectors.c $(SRCS) -o $@ -lm

vectors: $(BUILD)/gen_vectors
	$(BUILD)/gen_vectors > vectors/parity_vectors.json
	@python3 -c "import json;json.load(open('vectors/parity_vectors.json'))" && echo "vectors/parity_vectors.json ok"

# Single-file ES module (wasm embedded) so Vite needs no asset plumbing.
wasm: $(SRCS) wasm_exports.c *.h
	mkdir -p $(dir $(TOOL_WASM))
	$(EMCC) -O2 -std=c11 -Wall -Wextra -Werror $(SRCS) wasm_exports.c -o $(TOOL_WASM) \
	  -sEXPORTED_FUNCTIONS=$(EXPORTS) -sEXPORTED_RUNTIME_METHODS=ccall,cwrap,UTF8ToString,HEAPF32 \
	  -sMODULARIZE=1 -sEXPORT_ES6=1 -sENVIRONMENT=web,node -sSINGLE_FILE=1 -sALLOW_MEMORY_GROWTH=0 \
	  -sFILESYSTEM=0 -sASSERTIONS=0
	@echo "wrote $(TOOL_WASM)"

clean:
	rm -rf $(BUILD)
