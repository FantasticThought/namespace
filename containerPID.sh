#!/bin/bash

containers=`docker ps | awk '{ if (FNR > 1) print $1 }'`
for container in $containers; do
	pid=`docker top $container | awk '{ if (FNR>1) print $2}'`
	echo "$container $pid"
done
