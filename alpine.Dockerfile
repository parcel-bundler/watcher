FROM node:lts-alpine

RUN addgroup -g 2000 travis && \
  adduser -u 2000 -G travis -s /bin/sh -D travis && \
  apk --no-cache add g++ gcc libgcc libstdc++ linux-headers make python git fts-dev

USER node
WORKDIR /home/node

COPY --chown=node:node . .

# Run the build
CMD ./docker-agent.sh
