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

source = "/dev/shm/source"
origdir = "/mnt/lxfs/"
mandir = "/mnt/lxfs.managed/"
testdir = "test1/"
numfiles = 40000
size = 32768
numprocs = 1000
iterations = 20000000
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
        print("unable to stop LTFS Data Management, perhaps already stopped")

    try:
        shutil.rmtree("/var/run/ltfsdm")
    except Exception:
        print("unable to delete /var/run/ltfsdm")

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

    if os.system("ltfsdm start") != 0:
        print("unable to start LTFS Data Management")

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


def test1(count):
    filenum = str(numfiles-(count%numfiles)-1)
    filename = mandir + testdir + "file." + filenum
    copyname = filename + ".cpy"

    if subprocess.call(["cmp",  filename,  copyname], stdout=open(os.devnull, 'wb')) != 0:
        print("compare failed for file #" + filenum)
        raise  Exception("cmp failed")

    if subprocess.call(["ltfsdm", "migrate",  filename], stdout=open(os.devnull, 'wb')) != 0:
        print("migration failed for file #" + filenum)
        raise  Exception("stubbing failed")

    try:
        proc = subprocess.Popen(["ltfsdm", "info", "files", filename], stdout=subprocess.PIPE)
        output = proc.communicate()[0]
    except Exception:
        print("unable to determine the migration state of file #" + filenum)
        raise  Exception("info files failed")

    res=re.search(".*\n(.?).*" + filename, output)
    if res == None:
        print("unable to determine migration state for file #" + filenum)
        raise  Exception("searching output failed")

    if res.group(1) != "m":
        print("file is not in migrated state, file# " + filenum)
        raise  Exception("file not migrated")

    if count%numprocs == 0:
        print(time.strftime("%I:%M:%S") + ": " + str(count) + " done")

    if count%200000 == 0:
         proc = os.popen("mail -s '" + str(count) + " tests started' MAP@zurich.ibm.com", 'w')
         proc.write("---")
         proc.close()



def main(argv):
    prepare()

    with contextlib.closing(multiprocessing.Pool(processes=numprocs)) as pool:
        try:
            list(pool.imap(test1, range(iterations)))
        except Exception as e:
            print("a test failed (" + str(e) +  "), stopping ...")
            pool.close()
            pool.terminate()
        else:
            pool.close()
            pool.join()
            print("== test finished ==")


if __name__ == "__main__":
    main(sys.argv[1:])
