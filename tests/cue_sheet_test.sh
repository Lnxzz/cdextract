#!/bin/sh

echo "sample_single.cue"
../build/util/cdextract-util -v -l../sample/sample_single.cue

echo "sample_single_pregap.cue"
../build/util/cdextract-util -v -l../sample/sample_single_pregap.cue

echo "sample_multi.cue"
../build/util/cdextract-util -v -l../sample/sample_multi.cue

echo "sample_multi_pregap.cue"
../build/util/cdextract-util -v -l../sample/sample_multi_pregap.cue

echo "sample_multi_gap.cue"
../build/util/cdextract-util -v -l../sample/sample_multi_gap.cue

echo "sample_multi_eac.cue"
../build/util/cdextract-util -v -l../sample/sample_multi_eac.cue

echo done.
