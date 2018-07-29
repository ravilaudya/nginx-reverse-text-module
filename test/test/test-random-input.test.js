const request = require('supertest');
const assert = require('assert');
const faker = require('faker');
const random_paragraph = require('random-paragraph');
const Chance = require('chance');
const chance = new Chance();

function reverse_string(str)  {
  return str.split("").reverse().join('');
}

describe('GET /reverse', function() {
  const random_number = chance.natural({ min: 1000, max: 2000 });
  const random_text = chance.sentence({ words:  random_number});
  const reverse_random_text = reverse_string(random_text);
  it('respond with reverse text, text/plain', function(done) {
    request('http://localhost:8080')
    .post('/reverse')
    .set('Content-Type', 'text/plain')
    .send(random_text)
    .expect('Content-Type', 'text/plain')
    .expect('Content-Length', random_text.length + "")
    .expect(200, reverse_random_text)
    .end(done)
  });
});
