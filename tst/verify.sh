#!/bin/bash

PROG=$1
OUT=$2

for i in 10000 12000 15000 20000 25000 40000 50000 75000 100000 120000 150000 200000 250000
do
	printf "%6d %s\n" $i $($PROG -d $i | grep -P '\t' | tr -d '\t') > $OUT
	echo "$i calculation done"
done
