const fs = require('fs');
const path = require('path');

function checkHtml(htmlPath) {
  console.log('Checking', htmlPath);
  const html = fs.readFileSync(htmlPath, 'utf8');
  const srcRegex = /<script\s+[^>]*src=["']([^"']+)["']/g;
  const linkRegex = /<link\s+[^>]*href=["']([^"']+)["']/g;
  let m;
  let missing = 0;
  while ((m = srcRegex.exec(html)) !== null) {
    const src = m[1];
    if (src.startsWith('http')) continue;
    let full;
    if (src.startsWith('/docs/')) {
      full = path.resolve('docs', src.substring(6));
    } else {
      full = path.resolve(path.dirname(htmlPath), src);
    }
    if (!fs.existsSync(full)) {
      console.error('  FAIL - MISSING SCRIPT:', src, '->', full);
      missing++;
    } else {
      console.log('  OK script:', src);
    }
  }
  while ((m = linkRegex.exec(html)) !== null) {
    const href = m[1];
    if (href.startsWith('http') || href.startsWith('data:')) continue;
    const full = path.resolve(path.dirname(htmlPath), href);
    if (!fs.existsSync(full)) {
      console.error('  FAIL - MISSING LINK:', href, '->', full);
      missing++;
    } else {
      console.log('  OK css:', href);
    }
  }
  if (missing === 0) {
    console.log('  PASS: All references exist on disk for', htmlPath, '\n');
  } else {
    console.error('  FAIL: Missing', missing, 'references in', htmlPath, '\n');
    process.exit(1);
  }
}

checkHtml('tools/amy-studio/index.html');
checkHtml('docs/studio/index.html');
