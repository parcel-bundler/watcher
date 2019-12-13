const os = require("os");
const { execSync } = require("child_process");

if (os.platform() !== "linux") {
  // windows and mac
  execSync("npm run prebuild:current");
} else {
  // linux and alpine
  execSync("npm run prebuild:docker");
}
