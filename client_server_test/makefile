LDLIBS = -lzmq
CPPFLAGS = -g -pthread

TARGETS = client server router

main: $(TARGETS) getAddress.sh

clean:
	rm -rf $(TARGETS)