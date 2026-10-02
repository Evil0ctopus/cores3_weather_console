const http = require('node:http');
const fs = require('node:fs');
const path = require('node:path');

const root = path.resolve(__dirname, '..', 'src', 'web', 'web_assets');
const contentTypes = {
  '.html': 'text/html',
  '.css': 'text/css',
  '.js': 'text/javascript',
  '.png': 'image/png',
  '.wav': 'audio/wav',
};

const server = http.createServer((request, response) => {
  let pathname;
  try {
    pathname = decodeURIComponent(new URL(request.url, 'http://localhost').pathname);
  } catch {
    response.writeHead(400).end('Invalid URL');
    return;
  }
  const relative = pathname === '/' ? 'index.html' : pathname.replace(/^\/web_assets\//, '').replace(/^\//, '');
  const file = path.resolve(root, relative);
  if (!file.startsWith(root + path.sep)) {
    response.writeHead(403).end('Forbidden');
    return;
  }
  fs.readFile(file, (error, data) => {
    if (error) {
      if (error.code !== 'ENOENT' && error.code !== 'EISDIR') {
        console.error(error);
        response.writeHead(500).end('Unable to read preview asset');
      } else {
        response.writeHead(404).end('Not found');
      }
      return;
    }
    response.setHeader('Content-Type', contentTypes[path.extname(file)] || 'application/octet-stream');
    response.setHeader('Cache-Control', 'no-store');
    response.end(data);
  });
});

server.on('error', (error) => {
  console.error('Preview server failed:', error);
  process.exitCode = 1;
});
server.listen(4173, '127.0.0.1', () => {
  console.log('Static preview at http://127.0.0.1:4173 (device APIs require the CoreS3 web server)');
});
