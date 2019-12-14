const watcher = require("./index");

const NUM_ROUNDS = 50;

async function run() {
  let minTime;
  let maxTime = 0;
  let total = 0;

  for (let i = 0; i < NUM_ROUNDS; i++) {
    let start = Date.now();

    await watcher.writeSnapshot(
      "/Users/jasperdemoor/Documents/open-source/parcel",
      "snapshot.txt",
      { backend: "brute-force" }
    );

    let took = Date.now() - start;
    if (!minTime || minTime > took) {
      minTime = took;
    }

    if (maxTime < took) {
      maxTime = took;
    }

    total += took;

    console.log("Test Took:", took);
  }

  console.log("=== Summary ===");
  console.log("Min:", minTime, "ms");
  console.log("Max:", maxTime, "ms");
  console.log("Average:", total / NUM_ROUNDS, "ms");
}

run();
