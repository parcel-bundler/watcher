const assert = require('assert');
const {spawnSync} = require('child_process');
const path = require('path');

describe('failed subscription callback lifetime', () => {
  // FSEvents rejects a missing root in the subscription worker. Other backends
  // can report a missing root asynchronously or continue watching its parent.
  (process.platform === 'darwin' && !process.env.TEST_WASM ? it : it.skip)(
    'does not corrupt JS references while unrelated filesystem work completes',
    function () {
      this.timeout(30000);
      const child = spawnSync(
        process.execPath,
        [
          '--expose-gc',
          path.join(__dirname, 'fixtures/failed-subscription.js'),
        ],
        {encoding: 'utf8', timeout: 25000},
      );
      assert.ifError(child.error);
      assert.strictEqual(child.signal, null, child.stderr);
      assert.strictEqual(child.status, 0, child.stderr);
      assert(child.stdout.includes('failed-subscriptions-survived'));
    },
  );
});
