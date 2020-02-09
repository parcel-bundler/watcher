FROM jasperdm/alpine-watchman

USER node
WORKDIR /home/node

COPY --chown=node:node . .

# Run the build
CMD ./docker-agent.sh
