LDLIBS = -lzmq
CPPFLAGS = -g -pthread

TARGETS = client2 server2 router

main: $(TARGETS) getAddress.sh

clean:
	rm -rf $(TARGETS)