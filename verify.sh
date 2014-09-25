#!/bin/bash

PROG=./pi-calc
OUTFILE=verify_out.txt

rm -rf $OUTFILE

for i in 10000 12000 15000 20000 25000 40000 50000 75000 100000 120000 150000 200000 250000
do
	OUTPUT=$($PROG -d $i)
	printf "%6d %s\n" $i $(echo "$OUTPUT" | grep -P '\t' | tr -d '\t') >> $OUTFILE
	printf "%6d digits calculations done in %s s\n" $i $(echo "$OUTPUT" | grep "Run time:" | cut -f 3 -d ' ' | cut -c 1-7)
done
