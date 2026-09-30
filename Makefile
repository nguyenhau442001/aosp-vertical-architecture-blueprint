AVD ?= Automotive_15_ARM64

.PHONY: help run start unlock

help:
	@echo "AOSP Vertical Architecture Blueprint"
	@echo ""
	@echo "Usage:"
	@echo "  make run      Launch Android 15 Automotive & unlock partitions automatically"
	@echo "  make start    Start emulator only"
	@echo "  make unlock   Unlock partitions (disable-verity & remount)"
	@echo ""

run:
	@./scripts/run.sh $(AVD)

start:
	@./scripts/start_emulator.sh $(AVD)

unlock:
	@./scripts/unlock_partitions.sh
