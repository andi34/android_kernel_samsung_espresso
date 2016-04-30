#!/bin/sh
echo "Cherry-picking commits..."
git fetch https://github.com/andi34/android_kernel_samsung_espresso.git p-android-omap-3.0.101-dev-bcmdhd
for COMMIT in `cat bcmdhd.txt | grep -v ^# | grep -v ^$`; do
    git cherry-pick $COMMIT
done
