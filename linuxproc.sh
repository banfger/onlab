#!/bin/sh

mkdir linuxproc
cd linuxproc
file=/usr/include/x86_64-linux-gnu/bits/sched.h

if ! grep -q "CLONE_NEWCGROUP" $file
then
    if grep -q "CLONE_NEWUTS" $file
    then
      awk '/# define CLONE_NEWUTS	0x04000000/{print "# define CLONE_NEWCGROUP 0x02000000"}1' $file > schedtemp.h
      cat schedtemp.h > $file
      rm schedtemp.h
    fi
fi

wget https://github.com/banfger/onlab/raw/master/linuxproc/prog.c
wget https://github.com/banfger/onlab/raw/master/linuxproc/meres.c
wget https://github.com/banfger/onlab/raw/master/linuxproc/Makefile
wget https://github.com/banfger/onlab/raw/master/linuxproc/rootfs.tar
wget https://github.com/banfger/onlab/raw/master/linuxproc/tdiff.awk
make -f Makefile
mkdir rootfs
mv rootfs.tar rootfs
cd rootfs
tar xvf rootfs.tar
cd ..
./meres $1 $2
