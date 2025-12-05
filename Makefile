.Phony: test clean

test:
	cmake -S src/userver -B .build
	cmake --build .build

clean:
	rm -f log.log
	rm -rf .build
