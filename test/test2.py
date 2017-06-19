#!/usr/bin/python

import sys
import multiprocessing
import contextlib
import os.path
import shutil
import random
import time
import re
import threading
import subprocess
import resource
import fcntl

source = "/dev/shm/source"
origdir = "/mnt/lxfs/"
mandir = "/mnt/lxfs.managed/"
testdir = "test2/"
numfiles = 1000
size = 32768
numprocs = 100
iterations = 100000
tape1 = "D01301L5"
tape2 = "D01302L5"

def crfiles():
    if os.path.isfile(source) == 0:
        try:
            ifd = os.open("/dev/urandom", os.O_RDONLY)
        except Exception:
            print("unable to open /dev/urandom")
            exit(-1)
        try:
            ofd = os.open(source, os.O_RDWR | os.O_CREAT)
        except Exception:
            print("unable to open " + source)
            exit(-1)
        for i in range(4096):
            data = os.read(ifd, 1024*1024)
            if not data:
                print("unable to read random data to create source file")
                exit(-1)
            numbytes = os.write(ofd, data)
            if numbytes <= 0:
                print("unable to write source file")
                exit(-1)
        os.close(ofd)
        os.close(ifd)

    try:
        shutil.rmtree(origdir + testdir)
    except Exception:
        print("unable to delete " + origdir + testdir);

    try:
        os.mkdir(origdir + testdir)
    except Exception:
        print("unable to create test directory")
        exit(-1)

    try:
        sfd = os.open(source, os.O_RDONLY)
    except Exception:
        printf("unable to open " + source)
        exit(-1)

    random.seed(None)
    for i in range(0, numfiles):
        filename = origdir + testdir + "file." + str(i)
        copyname = filename + ".cpy"
        offset = random.randint(0, int(4*1024*1024*1024-size))

        try:
            os.lseek(sfd, offset, os.SEEK_SET)
        except Exception:
            printf("unable to seek within source file")
            exit(-1)

        data = os.read(sfd, size)
        if not data:
            print("unable to read reandom data to create test files")
            exit(-1)

        try:
            tfd = os.open(filename, os.O_RDWR | os.O_CREAT)
        except Exception:
            print("unable to open data file")
            exit(-1)

        numbytes = os.write(tfd, data)
        if numbytes <= 0:
            print("unable to write data file")
            exit(-1)

        os.close(tfd)

        try:
            shutil.copyfile(filename, copyname)
        except Exception:
            print("unable to copy data file")
            exit(-1)

    os.close(sfd)


def prepare():
    resource.setrlimit(resource.RLIMIT_CORE, (resource.RLIM_INFINITY, resource.RLIM_INFINITY))
    resource.setrlimit(resource.RLIMIT_NOFILE, (1028493, 1028493))
    resource.setrlimit(resource.RLIMIT_STACK, (resource.RLIM_INFINITY, resource.RLIM_INFINITY))

    if os.system("ltfsdm stop") != 0:
        print("unable to stop OpenLTFS, perhaps already stopped")

    if os.system("ltfsadmintool -t " + tape1 + "," + tape2 + " -f -- --force") != 0:
        print("unable to format tapes")
        exit(-1)

    if os.system("ltfsadmintool -t " + tape1 + " -m homeslot") != 0:
        print("unable to move " + tape1 + " to homeslot")
        exit(-1)

    if os.system("ltfsadmintool -t " + tape2 + " -m homeslot") != 0:
        print("unable to move " + tape2 + " to homeslot")
        exit(-1)

    if os.system("umount /mnt/ltfs") != 0:
        print("unable to unmount ltfs")
        exit(-1)

    while os.system("pidof ltfs > /dev/null 2>&1") == 0:
        print("... wait for termination")
        time.sleep(1)

    time.sleep(1)

    if os.system("ltfs /mnt/ltfs -o changer_devname=/dev/IBMchanger0 -o sync_type=unmount") != 0:
        print("unable to start ltfs")
        exit(-1)

    time.sleep(1)

    #if os.system("ltfsdmd -d 4") != 0:
    if os.system("ltfsdm start") != 0:
        print("unable to start OpenLTFS")

    crfiles()

    try:
        proc = os.popen("ltfsdm migrate -P pool1 -f -", 'w')
        for i in range(0, numfiles):
            if i%2==0:
                proc.write(mandir + testdir + "file." + str(i) + "\n")
        proc.close()
    except Exception:
        print("unable to migrate files to pool1")
        exit(-1)

    try:
        proc = os.popen("ltfsdm migrate -P pool2 -f -", 'w')
        for i in range(0, numfiles):
            if i%2==1:
                proc.write(mandir + testdir + "file." + str(i) + "\n")
        proc.close()
    except Exception:
        print("unable to migrate files to pool2")
        exit(-1)


def test2(count):
    filenum = numfiles-(count%numfiles)-1
    filename = mandir + testdir + "file." + str(filenum)
    copyname = filename + ".cpy"
    lockname = origdir + testdir + "file." + str(filenum) + ".cpy"
    random.seed(None)

    if count%numprocs == 0:
        print(time.strftime("%I:%M:%S") + ": " + str(count) + " started")

    if count%1000 == 0:
         proc = os.popen("mail -s '" + str(count) + " tests started' MAP@zurich.ibm.com", 'w')
         proc.write("---")
         proc.close()

    try:
        lfd = os.open(lockname, os.O_RDWR)
    except Exception:
        print("unabale to open " + lockname)
        raise -1

    try:
        fcntl.flock(lfd, fcntl.LOCK_EX)
    except Exception:
        print("unabale to lock " + lockname)
        raise -1

    if subprocess.call(["cmp",  filename,  copyname], stdout=open(os.devnull, 'wb')) != 0:
        print("compare failed for file " + filename)
        raise -1

    if random.randint(0, 10) != 0:
        if subprocess.call(["ltfsdm", "migrate",  filename], stdout=open(os.devnull, 'wb')) != 0:
            print("migration failed for file " + filename)
            raise -1
    else:
        if subprocess.call(["ltfsdm", "recall",  "-r", filename], stdout=open(os.devnull, 'wb')) != 0:
            print("recall failed for file " + filename)
            raise -1
        if subprocess.call(["ltfsdm", "migrate", "-P", "pool" + str(filenum % 2 +1), filename], stdout=open(os.devnull, 'wb')) != 0:
            print("migration failed for file " + filename)
            raise -1

    try:
        proc = subprocess.Popen(["ltfsdm", "info", "files", filename], stdout=subprocess.PIPE)
        output = proc.communicate()[0]
    except Exception:
        print("unable to determine the migration state of file " + filename)
        raise -1

    res=re.search(".*\n(.?).*" + filename, output)
    if res == None:
        print("unable to determine migration state for file " + filename)
        raise -1

    if res.group(1) != "m":
        print("file is not in migrated state, file " + filename)
        raise -1

    os.close(lfd)

def main(argv):
    prepare()

    with contextlib.closing(multiprocessing.Pool(processes=numprocs)) as pool:
        try:
            list(pool.imap(test2, range(iterations)))
        except Exception:
            print("a test failed, stopping ...")
            pool.close()
            pool.terminate()
        else:
            pool.close()
            pool.join()
            print("== test finished ==")


if __name__ == "__main__":
    main(sys.argv[1:])
