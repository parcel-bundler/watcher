#!/bin/sh

npm run prebuild:current

cp -rf prebuilds/@parcel/* /input/prebuilds/@parcel
