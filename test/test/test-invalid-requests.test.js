const request = require('supertest');
const assert = require('assert');
const faker = require('faker');
const random_paragraph = require('random-paragraph');
const Chance = require('chance');
const chance = new Chance();
const endpoint = require('./endpoint').url;

function reverse_string(str)  {
  return str.split("").reverse().join('');
}

describe('GET Request', function() {
  const random_text = chance.sentence({ words:  10});
  it('respond with invalid get request', function(done) {
    request(endpoint)
    .get('/reverse')
    .set('Content-Type', 'text/plain')
    .expect(405)
    .end(done)
  });
});

describe('Invalid Content Type', function() {
  it('respond with invalid content type', function(done) {
    request(endpoint)
    .post('/reverse')
    .set('Content-Type', 'text/html')
    .send('abcd efgh ijkl mnop qrst uvwx yz')
    .expect(400)
    .end(done)
  });
});

describe('Empty body', function() {
  it('respond with invalid body', function(done) {
    request(endpoint)
    .post('/reverse')
    .set('Content-Type', 'text/plain')
    .expect(400)
    .end(done)
  });
});
