#!/usr/bin/bash

sleep 2
filtro=${1:-}
count=$(ls -la | grep ^- | awk '{print $9}' | grep "^$filtro" | wc -l)
echo "Número de ficheros encontrados: $count"

if [ "$count" -gt 0 ]; then
  exit 0
else
  exit 1
fi