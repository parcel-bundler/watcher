.PHONY: build run

current_dir = $(shell pwd)

docker-build:
	docker build -t jasperdm/centos-watchman -f ./docker-agents/centos.Dockerfile .
	docker build -t jasperdm/alpine-watchman -f ./docker-agents/alpine.Dockerfile .

test-linux:
	docker build -t parcel-watcher/linux-test .
	docker run -v $(current_dir):/input parcel-watcher/linux-test
	