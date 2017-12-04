#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/resource.h>
#include <blkid/blkid.h>

#include <string>
#include <list>
#include <map>
#include <set>
#include <sstream>
#include <exception>

#include "src/common/errors/errors.h"
#include "src/common/exception/OpenLTFSException.h"
#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"
#include "src/common/util/FileSystems.h"
#include "src/common/configuration/Configuration.h"

#include "src/common/comm/ltfsdm.pb.h"
#include "src/common/comm/LTFSDmComm.h"

#include "src/connector/Connector.h"

#include "OpenLTFSCommand.h"
#include "InfoFilesCommand.h"

/** @page ltfsdm_info_files ltfsdm info files
    The ltfsdm info files command provides information about the migration status of one or more files.

    <tt>@LTFSDMC0010I</tt>

    parameters | description
    ---|---
    \<file name\> … | a set of file names to get the migration status
    -f \<file list\> | the name of a file containing file names to get the migration status

    Example:

    @verbatim
    [root@visp ~]# ls /mnt/lxfs/bigfile* |ltfsdm info files -f -
    state             size               blocks              tape id  file name
    m          47049088000                    8             D01301L5  /mnt/lxfs/bigfile
    r          47049088000             91892752                    -  /mnt/lxfs/bigfile.1
    r          47049088000             91892752                    -  /mnt/lxfs/bigfile.1.cpy
    r          47049088000             91892752                    -  /mnt/lxfs/bigfile.cp
    @endverbatim

    The migration states are:

    state | description
    ---|---
    m | migrated
    p | premigrated
    r | resident

    The responsible class is @ref InfoFilesCommand.
 */

void InfoFilesCommand::printUsage()
{
    INFO(LTFSDMC0010I);
}

void InfoFilesCommand::talkToBackend(std::stringstream *parmList)

{
}

void InfoFilesCommand::doCommand(int argc, char **argv)
{
    std::stringstream parmList;
    Connector connector(false);
    struct stat statbuf;
    char migstate;
    std::istream *input;
    std::string line;
    char *file_name;
    std::stringstream tapeIds;
    FsObj::mig_attr_t attr;

    if (argc == 1) {
        INFO(LTFSDMC0018E);
        THROW(Error::GENERAL_ERROR);

    }

    processOptions(argc, argv);

    checkOptions(argc, argv);

    TRACE(Trace::normal, argc, optind);
    traceParms();

    if (!fileList.compare("")) {
        for (int i = optind; i < argc; i++) {
            parmList << argv[i] << std::endl;
        }
    }

    isValidRegularFile();

    if (fileList.compare("")) {
        fileListStrm.open(fileList);
        input = dynamic_cast<std::istream*>(&fileListStrm);
    } else {
        input = dynamic_cast<std::istream*>(&parmList);
    }

    INFO(LTFSDMC0047I);

    while (std::getline(*input, line)) {
        try {
            file_name = canonicalize_file_name(line.c_str());
            if (file_name == NULL) {
                continue;
            }
            FsObj fso(file_name);
            statbuf = fso.stat();
            attr = fso.getAttribute();
            tapeIds.str("");
            tapeIds.clear();
            if (attr.copies == 0) {
                tapeIds << "-";
            } else {
                for (int i = 0; i < attr.copies; i++) {
                    if (i != 0)
                        tapeIds << ",";
                    tapeIds << attr.tapeId[i];
                }
            }
            if (!S_ISREG(statbuf.st_mode)) {
                INFO(LTFSDMC0049I, '-', statbuf.st_size, statbuf.st_blocks,
                        tapeIds.str(), file_name);
            } else {
                switch (fso.getMigState()) {
                    case FsObj::MIGRATED:
                        migstate = 'm';
                        break;
                    case FsObj::PREMIGRATED:
                        migstate = 'p';
                        break;
                    case FsObj::RESIDENT:
                        migstate = 'r';
                        break;
                    default:
                        migstate = ' ';
                }
                INFO(LTFSDMC0049I, migstate, statbuf.st_size, statbuf.st_blocks,
                        tapeIds.str(), file_name);
            }
        } catch (const std::exception& e) {
            if (stat(file_name, &statbuf) == -1) {
                free(file_name);
                continue;
            }
            INFO(LTFSDMC0049I, '-', statbuf.st_size, statbuf.st_blocks, '-',
                    file_name);
        }
        free(file_name);
    }
}
