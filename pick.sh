#!/bin/sh
echo "Cherry-picking commits..."
for COMMIT in `cat commits.txt | grep -v ^# | grep -v ^$`; do
    git cherry-pick $COMMIT
done
