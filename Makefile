.PHONY: build run

current_dir = $(shell pwd)

docker-build:
	docker build -t jasperdm/centos-watchman -f ./docker-agents/centos.Dockerfile . --progress plain
	docker build -t jasperdm/alpine-watchman -f ./docker-agents/alpine.Dockerfile . --progress plain
	