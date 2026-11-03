#!/bin/bash
# run.sh CPUS BINARY EXPORT HOURS NAME [extra args...]
# Runs an economy simulation; outputs go to the diag folder in the main repo's artifacts.
CPUS=$1; BIN=$2; EXPORT=$3; HOURS=$4; NAME=$5; shift 5
WT=/home/martinb/Documents/GitHub/MUDProject/.claude/worktrees/agent-afce1bb4c5ae0bf2f
OUT=/home/martinb/Documents/GitHub/MUDProject/artifacts/economy/r2/diag
export RATW_DATA_DIR=${RATW_DATA_DIR:-$WT/Data}
mkdir -p $OUT
taskset -c $CPUS $BIN $EXPORT --simulate 6 $HOURS --no-check --deterministic --threads ${THREADS:-4} --no-profile \
    --economy $OUT/$NAME.jsonl "$@" > $OUT/$NAME.out 2>&1
echo "done $NAME: $?"
