.PHONY: docker-build-centos docker-build-alpine docker-push-centos

current_dir = $(shell pwd)
GIT_SHA = $(shell git log -1 --pretty=%h)

git_sha:
	echo GIT_SHA is $(GIT_SHA)

docker-build-centos:
	docker build -t jasperdm/centos-watchman:latest -t jasperdm/centos-watchman:$(GIT_SHA) -f ./docker-agents/centos.Dockerfile . --progress plain

docker-build-alpine:
	docker build -t jasperdm/alpine-watchman:latest -t jasperdm/alpine-watchman:$(GIT_SHA) -f ./docker-agents/alpine.Dockerfile . --progress plain

docker-push-centos:
	docker push jasperdm/centos-watchman:${GIT_SHA}
	docker push jasperdm/centos-watchman:latest

docker-push-alpine:
	docker push jasperdm/alpine-watchman:${GIT_SHA}
	docker push jasperdm/alpine-watchman:latest
