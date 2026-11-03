#!/bin/bash
# run.sh NAME DATA_DIR HOURS CPUS [extra world_check args...]
S=/tmp/claude-1000/-home-martinb-Documents-GitHub-MUDProject/da6a1181-05f7-491a-a334-c3e7343d5540/scratchpad/farms_sim
OUT=/home/martinb/Documents/GitHub/MUDProject/artifacts/economy/r2
EXPORT=$OUT/farms_export
name=$1; data=$2; hours=$3; cpus=$4; shift 4
export RATW_DATA_DIR=$data
cd $S
/usr/bin/time -v taskset -c $cpus $S/${BIN:-world_check2} $EXPORT --simulate 6 $hours --no-check --deterministic --threads 4 --no-profile \
    --economy $OUT/$name.jsonl --loop-state $OUT/${name}_loop.jsonl "$@" > $OUT/$name.out 2> $S/$name.err
echo "exit $?" >> $S/$name.err
