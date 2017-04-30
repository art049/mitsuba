.PHONY: build
build: Makefile
	docker run --rm \
	-v $(shell pwd):/mitsuba \
	-w /mitsuba \
	mitsuba \
	sh -c 'make --makefile=Makefile'

.PHONY: webrun
webrun: build
	./run.sh -c mitsuba-web-gui -i mitsuba -r --env="APP=/mitsuba/binaries/mtsgui"

.PHONY: interactive
interactive:
	docker run --rm \
	-v $(shell pwd):/mitsuba \
	-w /mitsuba \
	-e DISPLAY=$(DISPLAY) \
	-v /tmp/.X11-unix:/tmp/.X11-unix \
	-it mitsuba \
	bash

Makefile: CMakeLists.txt
	docker run --rm \
	-v $(shell pwd):/mitsuba \
	-w /mitsuba \
	mitsuba \
	sh -c 'cmake CMakeLists.txt'


dockerimage: dockerbuild.log

dockerbuild.log:
	docker build -t mitsuba ./ | tee ./docker-build.log
.PHONY: clean
clean:
	docker run --rm \
	-v $(shell pwd):/mitsuba \
	-w /mitsuba \
	mitsuba \
	sh -c 'make --makefile=Makefile clean'
