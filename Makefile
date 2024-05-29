NODE_PATH := $(dir $(shell which node))
HEADERS_URL := $(shell node -p "process.release.headersUrl")
NODE_VERSION := $(shell node -p "process.version")

INCS_Debug := \
	-Ibuild/node-$(NODE_VERSION)/include/node \
	-I$(shell node -p "require('node-addon-api').include_dir")

SRC := src/binding.cc src/Watcher.cc src/Backend.cc src/DirTree.cc src/Glob.cc src/Debounce.cc src/shared/BruteForceBackend.cc src/unix/legacy.cc src/wasm/WasmBackend.cc
FLAGS := $(INCS_Debug) \
	-Oz \
	-flto \
	-fwasm-exceptions \
	-DNAPI_HAS_THREADS=1 \
	-sEXPORTED_FUNCTIONS="['_napi_register_wasm_v1', '_wasm_backend_event_handler', '_malloc', '_free', '_on_timeout']" \
	-sERROR_ON_UNDEFINED_SYMBOLS=0 \
	-s INITIAL_MEMORY=524288000

build/node-headers.tar.gz:
	curl $(HEADERS_URL) -o build/node-headers.tar.gz

build/node-$(NODE_VERSION): build/node-headers.tar.gz
	tar -xvf build/node-headers.tar.gz -C build
	touch build/node-$(NODE_VERSION)

build/Release/watcher.wasm: build/node-$(NODE_VERSION) $(SRC)
	mkdir -p build/Release
	em++ $(FLAGS) -sDECLARE_ASM_MODULE_EXPORTS=0 -o build/Release/watcher.js $(SRC)

build/Debug/watcher.wasm: build/node-$(NODE_VERSION) $(SRC)
	mkdir -p build/Debug
	em++ -g $(FLAGS) -o build/Debug/watcher.js $(SRC)

wasm: build/Release/watcher.wasm
wasm-debug: build/Debug/watcher.wasm

.PHONY: wasm wasm-debug
