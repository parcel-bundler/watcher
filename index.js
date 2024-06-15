const binding = require('node-gyp-build')(__dirname);
const { createWrapper } = require('./wrapper');

const wrapper = createWrapper(binding);
exports.writeSnapshot = wrapper.writeSnapshot;
exports.getEventsSince = wrapper.getEventsSince;
exports.subscribe = wrapper.subscribe;
exports.unsubscribe = wrapper.unsubscribe;
