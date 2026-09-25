// Runs the web build's soak session (index.html?soak) in headless Chromium and
// prints its SOAK_RESULT line: every allocation made after startup, counted
// at the malloc level (so SDL and WebGL count too), per phase of the session.
//
// Usage: node run_web_soak.mjs <site dir>

import { chromium } from 'playwright-core';
import { serve } from './web_server.mjs';

const [siteDir] = process.argv.slice(2);

const server = await serve(siteDir);
const browser = await chromium.launch({
  headless: true,
  args: ['--use-angle=swiftshader', '--enable-unsafe-swiftshader',
         '--autoplay-policy=no-user-gesture-required'],
});
try {
  const page = await browser.newPage({ viewport: { width: 1280, height: 1000 } });
  const errors = [];
  const result = new Promise((resolve, reject) => {
    page.on('console', (msg) => {
      const text = msg.text();
      if (text.startsWith('SOAK_RESULT ')) resolve(text);
      if (msg.type() === 'error') errors.push(text);
    });
    page.on('pageerror', (err) => reject(err));
    setTimeout(() => reject(new Error(`timed out; console errors: ${errors.join(' | ')}`)),
               10 * 60 * 1000).unref();
  });
  await page.goto(`http://127.0.0.1:${server.address().port}/index.html?soak`);
  console.log(await result);
} finally {
  await browser.close();
  server.close();
}
