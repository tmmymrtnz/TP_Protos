.PHONY: all server admin clean

BIN_DIR := bin

all: $(BIN_DIR) server admin

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

server:
	$(MAKE) -C src

admin:
	$(MAKE) -C admin

clean:
	$(MAKE) -C src clean
	$(MAKE) -C admin clean
