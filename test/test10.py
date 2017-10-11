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
import errno

source = "/dev/shm/source"
origdir = "/mnt/lxfs/"
mandir = "/mnt/lxfs/"
testdir = "test2/"
numfiles = 2000
#numfiles = 40000
size = 32768
numprocs = 2000
#numprocs = 200
iterations = 8000000
#iterations = 1000000
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

    try:
        os.remove("/var/run/test")
    except Exception:
        print("/var/run/test probably does not exist")
    cntfile = open("/var/run/test", 'w+')
    cntfile.write("0")
    cntfile.close();

    if os.system("ltfsdm stop") != 0:
        print("unable to stop OpenLTFS, perhaps already stopped")

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

    if os.system("umount /ltfs") != 0:
        print("unable to unmount ltfs")
        exit(-1)

    while os.system("pidof ltfs > /dev/null 2>&1") == 0:
        print("... wait for termination")
        time.sleep(1)

    time.sleep(1)

    if os.system("ltfs /ltfs -o changer_devname=/dev/IBMchanger0 -o sync_type=unmount") != 0:
        print("unable to start ltfs")
        exit(-1)

    time.sleep(1)

    if os.system("ltfsdm start") != 0:
    #if os.system("valgrind --leak-check=full  ltfsdmd -f > valgrind.out 2>&1 &") != 0:
        print("unable to start OpenLTFS")

    # time.sleep(10)
    print("creating the files")
    # time.sleep(10)

    crfiles()

    print("finished creating files")

    try:
        proc = os.popen("ltfsdm migrate -P pool1,pool2 -f -", 'w')
        for i in range(0, numfiles):
            if i%2==0:
                proc.write(mandir + testdir + "file." + str(i) + "\n")
        proc.close()
    except Exception:
        print("unable to migrate files to pool1")
        exit(-1)

    try:
        proc = os.popen("ltfsdm migrate -P pool2,pool1 -f -", 'w')
        for i in range(0, numfiles):
            if i%2==1:
                proc.write(mandir + testdir + "file." + str(i) + "\n")
        proc.close()
    except Exception:
        print("unable to migrate files to pool2")
        exit(-1)


def getNumMigProcs():
    numpr = 0
    for content in os.listdir("/proc"):
        fname = "/proc/" + content + "/cmdline"
        try:
            file = open(fname, "r")
        except Exception:
            continue

        try:
            for line in file:
                args = line.split('\x00')
                if args[0] == "ltfsdm" and args[1] == "migrate" and args[2] == "-P":
                    numpr = numpr + 1
                elif args[0] == "ltfsdm" and args[1] == "recall":
                    numpr = numpr + 1
        except Exception:
            pass

        file.close()

    return numpr


def finish():
    fd = os.open("/tmp/test2.lck", os.O_RDWR | os.O_CREAT)
    fcntl.lockf(fd, fcntl.LOCK_EX)

    cntfile = open("/var/run/test", 'r+')
    num = int(cntfile.readline())
    num+=1
    cntfile.seek(0)
    cntfile.write(str(num))
    cntfile.close();

    if num%numprocs == 0:
        print(time.strftime("%I:%M:%S") + ": " + str(num) + " finished")

    os.close(fd)

def test2(cycle):
    try:
        syncfd = os.open(sys.argv[0], os.O_RDWR)
    except IOError as e:
        print("unabale to open " + sys.argv[0] + " errno: " + str(e.errno))
        raise Exception("open failed")

    try:
        fcntl.lockf(syncfd, fcntl.LOCK_EX)
    except IOError as e:
        os.close(syncfd)
        print("unabale to lock " + sys.argv[0])
        raise  Exception("lock failed")

    while True:
        count = random.randint(0, numfiles)
        filenum = numfiles-(count%numfiles)-1
        filename = mandir + testdir + "file." + str(filenum)
        copyname = filename + ".cpy"
        lockname = origdir + testdir + "file." + str(filenum) + ".cpy"
        random.seed(None)
        nummcmds = 0

        try:
            lfd = os.open(lockname, os.O_RDWR)
        except IOError as e:
            print("unabale to open " + lockname + "errno: " + str(e.errno))
            raise Exception("open failed")

        try:
            fcntl.lockf(lfd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except IOError as e:
            os.close(lfd)
            if e.errno == errno.EACCES or e.errno == errno.EAGAIN:
                continue
            print("unabale to lock " + lockname)
            raise  Exception("lock failed")

        break

    fcntl.lockf(syncfd, fcntl.LOCK_UN)

    if subprocess.call(["cmp",  filename,  copyname], stdout=open(os.devnull, 'wb')) != 0:
        print("compare failed for file " + filename)
        raise  Exception("cmp failed")

    try:
        nummcmds = getNumMigProcs()
    except Exception as e:
        print("error determining the number of processes: " + str(e))
        raise  Exception("determining the number of migration processes failed: " + str(e))

    #age = int(time.time()) - int(os.stat(filename).st_mtime) + 1

    step = 0

    output1 = ""

    #if nummcmds > numprocs/200 or random.randint(0, int(100/age)+1) != 0:
    if nummcmds > numprocs/10 or random.randint(0, numprocs) != 0:
        step = 1
        try:
             proc1 = subprocess.Popen(["ltfsdm", "migrate",  filename], stdout=subprocess.PIPE)
             output1 = str(proc1.communicate())
        except Exception:
            print("migration failed for file " + filename)
            raise  Exception("stubbing failed")
    else:
        step = 2
        if subprocess.call(["ltfsdm", "recall",  "-r", filename], stdout=open(os.devnull, 'wb')) != 0:
            print("recall failed for file " + filename)
            raise  Exception("recall failed")
        if subprocess.call(["ltfsdm", "migrate", "-P", "pool" + str(filenum % 2 +1) + ",pool" + str((filenum+1) % 2 +1), filename], stdout=open(os.devnull, 'wb')) != 0:
            print("migration failed for file " + filename)
            raise  Exception("migration failed")

    try:
        proc = subprocess.Popen(["ltfsdm", "info", "files", filename], stdout=subprocess.PIPE)
        output = proc.communicate()[0]
    except Exception:
        print("unable to determine the migration state of file " + filename)
        raise  Exception("info files failed")

    res=re.search(".*\n(.?).*" + filename, output)
    if res == None:
        print("unable to determine migration state for file " + filename)
        raise  Exception("searching output failed")

    if res.group(1) != "m":
        print("file is not in migrated state, file " + filename + " - " + str(step) + " - " + output1 + " - " + output)
        raise  Exception("file not migrated")

    try:
        fcntl.lockf(lfd, fcntl.LOCK_UN)
    except Exception:
        print("unabale to unlock " + lockname)
        raise  Exception("unlock failed")

    os.close(lfd)
    finish()

def main(argv):
    prepare()

    with contextlib.closing(multiprocessing.Pool(processes=numprocs)) as pool:
        try:
            list(pool.imap(test2, range(iterations)))
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
