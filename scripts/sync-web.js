const fs = require('fs');
const path = require('path');

const root = path.resolve(__dirname, '..');
const www = path.join(root, 'www');

if (fs.existsSync(www)) fs.rmSync(www, { recursive: true });
fs.mkdirSync(www, { recursive: true });

fs.copyFileSync(path.join(root, 'index.html'), path.join(www, 'index.html'));

function copyDir(src, dest) {
  fs.mkdirSync(dest, { recursive: true });
  for (const entry of fs.readdirSync(src, { withFileTypes: true })) {
    const s = path.join(src, entry.name);
    const d = path.join(dest, entry.name);
    entry.isDirectory() ? copyDir(s, d) : fs.copyFileSync(s, d);
  }
}
copyDir(path.join(root, 'lib'), path.join(www, 'lib'));

console.log('Web assets synced to www/');
