.Phony: test clean

build: 
	cmake -S src -B src/.build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
	cmake --build src/.build

new:
	rm -f log.log
	rm -rf src/.build
	$(MAKE) build

build_asan:
	cmake -S src -B src/.build -DENABLE_ASAN=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
	cmake --build src/.build

test:
	/workspace/src/.build/runtests-userver-service | tee /workspace/tests/tests.log
