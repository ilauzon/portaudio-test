CC = g++
EXEC = audiotest
CFLAGS = -I./lib/portaudio/include ./lib/portaudio/lib/.libs/libportaudio.a -lrt -lasound -ljack -pthread

$(EXEC): main.cpp
	$(CC) -o $@ $^ $(CFLAGS)

install-deps:
	mkdir -p lib

	curl -L http://files.portaudio.com/archives/pa_stable_v190700_20210406.tgz | tar -zx -C lib
	cd lib/portaudio && ./configure && $(MAKE) -j
.PHONY: install-deps

uninstall-deps:
	cd lib/portaudio && $(MAKE) uninstall
	rm -rf lib/portaudio
.PHONY: uninstall-deps

run:
	./$(EXEC)

clean:
	rm -f $(EXEC)
.PHONY: clean