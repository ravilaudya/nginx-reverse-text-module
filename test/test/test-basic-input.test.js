const request = require('supertest');
const assert = require('assert');

describe('GET /reverse', function() {
  it('respond with reverse text, text/plain', function(done) {
    request('http://localhost:8080')
    .post('/reverse')
    .set('Content-Type', 'text/plain')
    .send('abcd efgh ijkl mnop qrst uvwx yz')
    .expect('Content-Type', 'text/plain')
    .expect('Content-Length', '32')
    .expect(200, "zy xwvu tsrq ponm lkji hgfe dcba")
    .end(done)
  });
});

describe('GET /reverse', function() {
  it('respond with proper content type', function(done) {
    request('http://localhost:8080')
    .post('/reverse')
    .set('Content-Type', 'text/html')
    .send('abcd efgh ijkl mnop qrst uvwx yz')
    .expect(400)
    .end(done)
  });
});
