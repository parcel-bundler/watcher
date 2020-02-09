FROM node:12-alpine

RUN apk update
RUN apk upgrade

# Azure Pipelines stuff
RUN apk add --no-cache --virtual .pipeline-deps readline linux-pam \
  && apk add bash sudo shadow \
  && apk del .pipeline-deps

RUN apk add --no-cache \
  libcrypto1.1 \
  libgcc \ 
  libstdc++ \ 
  ca-certificates  \ 
  openssl \
  automake \
  autoconf \
  bash \
  build-base \
  libtool \
  linux-headers \
  openssl-dev \
  python-dev \
  g++ \ 
  gcc \
  git \
  fts-dev \
  python
  
ENV WATCHMAN_VERSION=4.9.0 \
  WATCHMAN_SHA256=1f6402dc70b1d056fffc3748f2fdcecff730d8843bb6936de395b3443ce05322

RUN cd /tmp \
 && wget -O watchman.tar.gz "https://github.com/facebook/watchman/archive/v${WATCHMAN_VERSION}.tar.gz" \
 && echo "$WATCHMAN_SHA256 *watchman.tar.gz" | sha256sum -c - \
 && tar -xz -f watchman.tar.gz -C /tmp/ \
 && rm -rf watchman.tar.gz

RUN cd /tmp/watchman-${WATCHMAN_VERSION} \
 && ./autogen.sh \
 && ./configure --enable-lenient \
 && make \
 && make install \
 && cd $HOME \
 && rm -rf /tmp/*

RUN watchman --version
RUN command -v bash

# Set Node runtime as it's using musl
LABEL "com.azure.dev.pipelines.agent.handler.node.path"="/usr/local/bin/node"
CMD [ "node" ]
