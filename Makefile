# Thin wrapper around CMake so the everyday commands are short.
#   make        -> configure (once) + build
#   make run    -> build + launch the game
#   make test   -> build + run unit tests
#   make format -> clang-format all sources
#   make clean  -> remove the build directory

BUILD_DIR := build/debug

.PHONY: build run test format clean assets

# Regenerate all hand-authored pixel art: ground tiles/props + character sheets.
assets:
	@python3 tools/build_tiles.py lua
	@sh tools/gen_sprites.sh

build:
	@test -d $(BUILD_DIR) || cmake --preset debug
	@cmake --build $(BUILD_DIR)

run: build
	@./$(BUILD_DIR)/jeu

test: build
	@ctest --test-dir $(BUILD_DIR) --output-on-failure

format:
	@clang-format -i src/*.hpp src/*.cpp tests/*.cpp
	@echo "formatted."

clean:
	@rm -rf build
	@echo "cleaned."
