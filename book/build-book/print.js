const { chromium } = require('playwright');
(async () => {
  const b = await chromium.launch();
  const p = await b.newPage();
  await p.goto('file:///home/claude/book/build-book/book.html', { waitUntil: 'load' });
  await p.pdf({ path: 'body.pdf', width: '7in', height: '9.25in', printBackground: true,
    margin: { top: '0.8in', bottom: '0.85in', left: '0.75in', right: '0.75in' },
    displayHeaderFooter: true, headerTemplate: '<span></span>',
    footerTemplate: '<div style="width:100%;text-align:center;font-family:DejaVu Serif,serif;font-size:8pt;color:#555"><span class="pageNumber"></span></div>',
    outline: true, tagged: true });
  const c = await b.newPage();
  await c.setContent('<html><body style="margin:0"><img src="file:///home/claude/book/assets/cover.png" style="width:7in;height:9.25in;display:block"></body></html>');
  await c.goto('data:text/html,'); // reset
  await c.setContent('<html><body style="margin:0"><img id=i style="width:7in;height:9.25in;display:block"></body></html>');
  const fs = require('fs');
  const b64 = fs.readFileSync('/home/claude/book/assets/cover.png').toString('base64');
  await c.evaluate(s => new Promise(r => { const i = document.getElementById('i'); i.onload = r; i.src = s; }), 'data:image/png;base64,' + b64);
  await c.pdf({ path: 'cover.pdf', width: '7in', height: '9.25in', printBackground: true, margin: { top: 0, bottom: 0, left: 0, right: 0 } });
  await b.close();
})();
