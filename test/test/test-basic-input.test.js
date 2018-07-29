const request = require('supertest');
const assert = require('assert');
const endpoint = require('./endpoint').url;

describe('Basic Input', function() {
  it('respond with reverse text, text/plain', function(done) {
    request(endpoint)
    .post('/reverse')
    .set('Content-Type', 'text/plain')
    .send('abcd efgh ijkl mnop qrst uvwx yz')
    .expect('Content-Type', 'text/plain')
    .expect('Content-Length', '32')
    .expect(200, "zy xwvu tsrq ponm lkji hgfe dcba")
    .end(done)
  });
});

