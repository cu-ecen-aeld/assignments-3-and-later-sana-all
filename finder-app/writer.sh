#!/bin/sh

if [ $# -lt 2 ]
then

  echo put sufficient args
  exit 1 

else
  writefile=$1
  writestr=$2
  
  
  writedir=$(dirname "$writefile")
  
  mkdir -p "$writedir"
  if [ $? -ne 0 ]; then
    echo error: Failed to create directory path
    exit 1
  fi


  echo $writestr > $writefile
  if [ $? -ne 0 ]; then
    echo error: Failed to write to file
    exit 1
  fi
fi
