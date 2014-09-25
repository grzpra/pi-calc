#!/bin/bash

PROG=./pi-calc
OUTFILE=verify_out.txt
REFFILE=tst/reference.txt

if [ ! -f $PROG ]; then
	echo "Cannot find $PROG"
	exit 1
fi


rm -rf $OUTFILE

for i in 10000 12000 15000 20000 25000 40000 50000 75000 100000 120000 150000
do
	OUTPUT=$($PROG -d $i)
	printf "%6d %s\n" $i $(echo "$OUTPUT" | grep -P '\t' | tr -d '\t') >> $OUTFILE
	printf "%6d digits calculations done in %s s\n" $i $(echo "$OUTPUT" | grep "Run time:" | cut -f 3 -d ' ' | cut -c 1-7)
done

diff $OUTFILE $REFFILE > /dev/null
RES=$?
if [ $RES -ne 0 ]; then
	echo "Results do not match reference."
else
	echo "Results seem OK."
fi
