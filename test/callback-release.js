const assert = require('assert');
const {spawnSync} = require('child_process');
const path = require('path');

describe('callback reference release', () => {
  (typeof WeakRef === 'function' && !process.env.TEST_WASM ? it : it.skip)(
    'allows unsubscribed callbacks to be collected',
    function () {
      this.timeout(30000);
      const child = spawnSync(
        process.execPath,
        ['--expose-gc', path.join(__dirname, 'fixtures/callback-release.js')],
        {encoding: 'utf8', timeout: 25000},
      );
      assert.ifError(child.error);
      assert.strictEqual(child.signal, null, child.stderr);
      assert.strictEqual(child.status, 0, child.stderr);
      assert(child.stdout.includes('callbacks-released'));
    },
  );
});
