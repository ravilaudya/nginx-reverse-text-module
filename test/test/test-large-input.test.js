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

describe('Large Input Text', function() {
  const random_number = chance.natural({ min: 10000, max: 20000 });
  const random_text = chance.sentence({ words:  random_number});
  const reverse_random_text = reverse_string(random_text);
  it('with random large text, text/plain', function(done) {
    request(endpoint)
    .post('/reverse')
    .set('Content-Type', 'text/plain')
    .send(random_text)
    .expect('Content-Type', 'text/plain')
    .expect('Content-Length', random_text.length + "")
    .expect(200, reverse_random_text)
    .end(done)
  });
});
