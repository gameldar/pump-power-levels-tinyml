#!/usr/bin/env bash

SRC=$1
prefix=$2


split -b 32000 "$SRC" "${prefix}_"
if [ ! -d "raw/$prefix/" ]; then mkdir "raw/$prefix/"; fi
if [ ! -d "wav/$prefix/" ]; then mkdir "wav/$prefix/"; fi
for f in `ls ${prefix}_*`
do
  mv "$f" "raw/$prefix/${f}.raw"
  sox -r 16000 -e signed -b 16 -c 1 "raw/$prefix/${f}.raw" "wav/$prefix/${f}.wav"
done 
