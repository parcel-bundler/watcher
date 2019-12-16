.PHONY: build run

current_dir = $(shell pwd)

build:
	docker build -t parcel-watcher/centos-builder-agent -f ./centos.Dockerfile .
	docker build -t parcel-watcher/alpine-builder-agent -f ./alpine.Dockerfile .
	
run:
	mkdir -p prebuilds/@parcel
	docker run -v $(current_dir):/input parcel-watcher/centos-builder-agent
	docker run -v $(current_dir):/input parcel-watcher/alpine-builder-agent