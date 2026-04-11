phony:
	run, new

new:
	cmake -B .build
	cmake --build .build
	rm -f ./log.log
	./.build/async-service --config static_config.yaml

run:
	rm -f ./log.log
	./.build/async-service --config static_config.yaml

