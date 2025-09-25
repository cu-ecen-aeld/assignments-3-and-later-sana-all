#!/bin/sh
cd $(dirname $0)
echo "Running test script"
echo $(ls)
echo "going directory home"
cd /home
echo $(ls)
./finder-test.sh
rc=$?
if [ ${rc} -eq 0 ]; then
    echo "Completed with success!!"
else
    echo "Completed with failure, failed with rc=${rc}"
fi
echo "finder-app execution complete, dropping to terminal"
/bin/sh
