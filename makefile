COMMANDS = "source setpath.sh; bash"
RUN_COMMANDS = "source setpath.sh; mitsuba scene/bathroom/scene.xml; bash"

.PHONY: build
build: Makefile dockerbuild.log
	docker run --rm \
	-v $(shell pwd):/mitsuba \
	-w /mitsuba \
	mitsuba \
	sh -c 'scons -j 8'

.PHONY: webrun
webrun: build
	./docker_env/run.sh -c mitsuba-web-gui -i mitsuba

.PHONY: render
render: commands interactive

.PHONY: commands 
commands:
	rm -f *.log
	#rm -rf subscene/*
	$(eval COMMANDS = $(RUN_COMMANDS))

.PHONY: interactive 
interactive:
	docker run --rm \
	-v $(shell pwd):/mitsuba \
	-w /mitsuba \
	-e DISPLAY=$(DISPLAY) \
	-v /tmp/.X11-unix:/tmp/.X11-unix \
	-it mitsuba \
	bash -c $(COMMANDS)

Makefile: CMakeLists.txt
	docker run --rm \
	-v $(shell pwd):/mitsuba \
	-w /mitsuba \
	mitsuba \
	sh -c 'cmake CMakeLists.txt'


dockerbuild.log:
	docker build -t mitsuba ./docker_env/ | tee ./docker-build.log
.PHONY: clean
clean:
	docker run --rm \
	-v $(shell pwd):/mitsuba \
	-w /mitsuba \
	mitsuba \
	sh -c 'make --makefile=Makefile clean'
