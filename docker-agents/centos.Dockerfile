FROM centos/devtoolset-7-toolchain-centos7

USER 0

ENV WATCHMAN_VERSION=4.9.0 \
    WATCHMAN_SHA256=1f6402dc70b1d056fffc3748f2fdcecff730d8843bb6936de395b3443ce05322

RUN curl -sL https://rpm.nodesource.com/setup_12.x | bash -

RUN yum install -y \
    nodejs \
    wget \
    curl \
    libgcc \ 
    libstdc++ \ 
    ca-certificates  \ 
    openssl \
    automake \
    autoconf \
    bash \
    libtool \
    openssl-devel \
    python-devel \
    gcc \
    git \
    which

RUN cd /tmp \
    && wget -O watchman.tar.gz "https://github.com/facebook/watchman/archive/v${WATCHMAN_VERSION}.tar.gz" \
    && echo "$WATCHMAN_SHA256 *watchman.tar.gz" | sha256sum -c - \
    && tar -xz -f watchman.tar.gz -C /tmp/ \
    && rm -rf watchman.tar.gz

RUN cd /tmp/watchman-${WATCHMAN_VERSION} \
    && ./autogen.sh \
    && ./configure \
    && make \
    && make install \
    && cd $HOME \
    && rm -rf /tmp/*

# Install yarn
RUN curl --silent --location https://dl.yarnpkg.com/rpm/yarn.repo | tee /etc/yum.repos.d/yarn.repo \
    && rpm --import https://dl.yarnpkg.com/rpm/pubkey.gpg \
    && yum install -y yarn

RUN mkdir -p /home/default

RUN printenv

RUN watchman --version

RUN which node

RUN yarn --version

# Set Node runtime
LABEL "com.azure.dev.pipelines.agent.handler.node.path"="/usr/bin/node"
CMD [ "node" ]
