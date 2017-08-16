#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/resource.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <set>
#include <mutex>

#include "src/common/const/Const.h"
#include "src/common/errors/errors.h"
#include "src/common/util/util.h"
#include "src/common/exception/OpenLTFSException.h"

#include "Message.h"

Message messageObject;

Message::~Message()

{
    if (fd != Const::UNSET)
        close(fd);

    fd = Const::UNSET;
}

void Message::init(std::string extension)

{
    if (extension.compare("") != 0)
        fileName.append(extension);

    fd = open(fileName.c_str(),
    O_RDWR | O_CREAT | O_APPEND | O_CLOEXEC | O_SYNC, 0644);

    if (fd == Const::UNSET) {
        MSG(LTFSDMX0001E);
        exit((int) Error::LTFSDM_GENERAL_ERROR);
    }
}

void Message::writeOut(std::string msgstr)

{
    mtx.lock();
    std::cout << msgstr << std::flush;
    mtx.unlock();
}

void Message::writeLog(std::string msgstr)

{
    if (write(fd, msgstr.c_str(), msgstr.size()) != (long) msgstr.size()) {
        std::cerr << messages[LTFSDMX0004E];
        exit((int) Error::LTFSDM_GENERAL_ERROR);
    }
}
