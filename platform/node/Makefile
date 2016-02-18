all: build

pkgconfig:
	@if [[ `which pkg-config` ]]; then echo "Success: Found pkg-config"; else "echo you need pkg-config installed" && exit 1; fi;

./build: pkgconfig
	npm install --build-from-source --clang=1

debug: pkgconfig
	npm install --build-from-source --debug --clang=1

coverage: pkgconfig
	npm install --build-from-source --debug --clang=1 --coverage=true

verbose: pkgconfig
	npm install --build-from-source --debug --clang=1 --verbose

clean:
	rm -rf ./lib/binding/*
	rm -rf ./node_modules/
	rm -f ./*tgz

grind:
	valgrind --leak-check=full node node_modules/.bin/_mocha

#shm:
#	../../build/osrm-datastore ../../test/data/berlin-latest.osrm

test:# shm
	npm test

.PHONY: test clean build shm
