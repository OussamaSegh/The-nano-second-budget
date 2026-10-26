// Usage: node render.js in.html out.pdf [--outline]
// Renders an HTML file to PDF with the page size and margins from its CSS.
const { chromium } = require('playwright');
const path = require('path');
(async () => {
  const [inp, out, flag] = process.argv.slice(2);
  const b = await chromium.launch();
  const p = await b.newPage();
  await p.goto('file://' + path.resolve(inp), { waitUntil: 'load' });
  await p.evaluate(() => document.fonts.ready);
  await p.pdf({ path: out, preferCSSPageSize: true, printBackground: true,
                outline: flag === '--outline', tagged: flag === '--outline' });
  await b.close();
})();
