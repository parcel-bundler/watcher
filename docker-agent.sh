#!/bin/sh

npm run prebuild

cd /input
yarn install
yarn test
