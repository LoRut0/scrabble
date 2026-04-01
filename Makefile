.Phony: test clean

test:
	cmake -S src -B src/.build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
	cmake --build src/.build

clean:
	rm -f log.log
	rm -rf src/.build
