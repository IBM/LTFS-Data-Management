/*
 * test.cc
 *
 *  Created on: Aug 4, 2017
 *      Author: root
 */




#include "ServerIncludes.h"

void thrdf1(std::string filename)

{
    FsObj obj(filename);
    struct stat statbuf;

    for (int i=0;i<10000; i++)
        stat(filename.c_str(), &statbuf);
}

void thrdf2(int j, int n)

{
    if ( n>0 ) {
        SubServer subs;
        n--;
        for(int i=0; i<4; i++) {
            std::stringstream thrdname;
            thrdname << "test:" << n << "." << j << "." << i << std::endl;
            subs.enqueue(thrdname.str(), &thrdf2, i, n);
        }
        subs.waitAllRemaining();
    }


    //sleep(1);
}

void thrdf3(int j, int n)

{
    const std::string baseName = "/mnt/lxfs.managed/test3/x.";
    std::string fileName = "";
    struct stat statbuf;

    if ( n>0 ) {
        SubServer subs;
        n--;
        for(int i=0; i<4; i++) {
            fileName = baseName;
            fileName.append(std::to_string(random()%2000));
            //FsObj file(fileName);
            //std::cout << file.getINode() << std::endl;
            stat(fileName.c_str(), &statbuf);
            std::stringstream thrdname;
            thrdname << "test:" << n << "." << j << "." << i << std::endl;
            subs.enqueue(thrdname.str(), &thrdf3, i, n);
        }
        subs.waitAllRemaining();
    }


    //sleep(1);
}

int main(int argc, char **argv)

{
/*    ThreadPool<std::string> wq(&thrdf1, 1000, "wq");

    sleep(1);

    for(int i=0; i<1000; i++)
        wq.enqueue(Const::UNSET, std::string("x.").append(std::to_string(i)));

    wq.waitCompletion(Const::UNSET);
    wq.terminate();*/

/*
    SubServer subs;

    for(int i=0; i<4; i++)
        subs.enqueue(std::string("test:").append(std::to_string(i)), &thrdf2, i, 4);

    subs.waitAllRemaining();
*/

    FsObj fileSystem("/mnt/lxfs");
    SubServer subs;

    fileSystem.manageFs(true, (struct timespec) {0,0});

    srandom(time(NULL));

    for(int i=0; i<4; i++)
        subs.enqueue(std::string("test:").append(std::to_string(i)), &thrdf3, i, 4);

    subs.waitAllRemaining();



    return 0;
}
