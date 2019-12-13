.PHONY: build run

current_dir = $(shell pwd)

build:
	docker build -t parcel-watcher/alpine-builder-agent -f ./alpine.Dockerfile .
	# docker build -t parcel-watcher/centos-builder-agent -f ./centos.Dockerfile .
	
run:
	docker run -v $(current_dir):/input parcel-watcher/alpine-builder-agent
	# docker run -v $(current_dir):/input parcel-watcher/centos-builder-agent