.PHONY: build run

current_dir = $(shell pwd)

build:
	# docker build -t jasperdm/centos-watchman -f ./docker-agents/centos.Dockerfile .
	docker build -t jasperdm/alpine-watchman -f ./docker-agents/alpine.Dockerfile .
	