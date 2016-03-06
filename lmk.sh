#!/bin/sh
git fetch https://github.com/andi34/android_kernel_samsung_espresso.git p-android-omap-3.0.101-dev-lmk
echo "Cherry-picking commits..."
for COMMIT in `cat lmk.txt | grep -v ^# | grep -v ^$`; do
    git cherry-pick $COMMIT
done
