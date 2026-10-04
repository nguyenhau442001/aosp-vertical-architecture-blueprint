AVD ?= Automotive_15_ARM64

.PHONY: help run start unlock test test-py test-native codegen android-build android-test android-bridge

help:
	@echo "AOSP Vertical Architecture Blueprint"
	@echo ""
	@echo "Usage:"
	@echo "  make run      Launch Android 15 Automotive & unlock partitions automatically"
	@echo "  make start    Start emulator only"
	@echo "  make unlock   Unlock partitions (disable-verity & remount)"
	@echo "  make test     Run cansim (Python) and native (C++) host tests"
	@echo "  make codegen  Regenerate native/canbridge/generated/ from vehicle/dbc/"
	@echo ""
	@echo "  make android-build   Cross-compile native/ with the NDK"
	@echo "  make android-test    Push + run the gtests on the emulator"
	@echo "  make android-bridge  Push + run bp-canbridge on the emulator (feed it with cansim serve)"
	@echo ""
	@echo "Adding a vehicle signal: docs/vhal/adding-a-new-signal.md"
	@echo ""

run:
	@./scripts/run.sh $(AVD)

start:
	@./scripts/start_emulator.sh $(AVD)

unlock:
	@./scripts/unlock_partitions.sh

test: test-py test-native

test-py:
	@cd tools/cansim && python3 -m pytest -q

test-native:
	@cmake -S native -B native/build -DCMAKE_BUILD_TYPE=Debug > /dev/null
	@cmake --build native/build -j8
	@ctest --test-dir native/build --output-on-failure

codegen:
	@cd tools/cansim && python3 -m cansim codegen

android-build:
	@./scripts/build_android.sh

android-test: android-build
	@./scripts/android_native.sh test

android-bridge: android-build
	@./scripts/android_native.sh bridge
