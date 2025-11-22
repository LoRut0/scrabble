.Phony: test clean

test:
	cmake -S src/userver -B .build
	cmake --build .build

clean:
	rm log.log
	rm -rf src/userver/build
