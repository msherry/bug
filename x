#!/bin/bash

clear

if [ -f ./bug.out ]
then
	cat bug.out
else
	cat ~/bug.out
fi
