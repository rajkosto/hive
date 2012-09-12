#!/bin/bash

gitVer=`git rev-list HEAD | head -n 1`
sedCommand="s/%GIT_VERSION%/${gitVer}/g" 
echo "Generating Git version file"
sed "$sedCommand" $1 > $2