FROM prebuild/centos7-devtoolset7

USER node
WORKDIR /home/node

COPY --chown=node:node . .

# Run the build
CMD ./docker-agent.sh
