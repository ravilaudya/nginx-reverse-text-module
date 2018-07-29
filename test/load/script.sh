#!/usr/bin/env bash
loadtest -n 10000 -c 100 -p ../test-800kb.txt http://localhost:8080/reverse

loadtest -n 10000 -c 500 -p ../10char.txt http://localhost:8080/reverse

