#!/bin/bash

echo "Updating project..."
git pull || exit 1

echo "Building project..."
cmake -S . -B build || exit 1

cmake --build build || exit 1

echo "Running program..."
./build/HelloWorld