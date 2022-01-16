/*******************************************************************************
 * Copyright 2018 IBM Corp. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *******************************************************************************/
#pragma once

#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/syscall.h>
#include <fstream>
#include <iomanip>
#include <atomic>
#include <mutex>

#include "src/common/Message.h"
#include "src/common/LTFSDMException.h"
#include "src/common/errors.h"

/**
    @page tracing_system Tracing

    The tracing system is used to print out values of variables of the
    source code into a trace file.

    @todo enable tracing on the client side

    At the moment tracing only is enabled on the server side.

    There are five different trace levels:

    trace level | numeric value | description
    ---|:---:|---
    none |0| no tracing
    always |1| print trace message always
    error |2| print trace message in an error case
    normal |3| standard tracing
    full |4| perform a full tracing

    @todo trace levels need to be reworked, do not make sense

    The standard trace level is "normal". Different trace levels can be enabled
    by starting the backend directly: see @ref server_code "server code" how to start the backend
    in this way.

    @todo use rsyslog facility to write trace output

    The trace information is written to /var/rum/ltfsdm/LTFSDM.trc*.

    In the following there is some sample output:

    @verbatim
    2017-12-07T15:42:46.366490:[004502:012271]:------TransRecall.cc(0063): filename('/mnt/lxfs/test2/file.262')
    2017-12-07T15:42:46.366508:[004502:012271]:------TransRecall.cc(0067): tapeId(D01302L5)
    2017-12-07T15:42:46.366653:[004502:031034]:--------Migration.cc(0113): fileName(/mnt/lxfs/test2/file.945), replNum,(0), pool()
    @endverbatim

    The trace output has the following structure:

    <tt>
    @b date T @b time :[ @b pid  : @b tid  ]:---- @b file_name( @b line_number ): @b variable_name_1 ( @b value_1 ),@b variable_name_2 ( @b value_2 ),...
    </tt>

    For each process there exists a tracing object @ref traceObject that
    never should be used directly used but is used internally as part of
    the TRACE() macro. The macro TRACE() automatically adds the corresponding
    file name and the line number to the output.

    The usage is the following:

    @verbatim
    TRACE(tracelevel, var1, var2, ...)
    @endverbatim

    The following gives an overview about the internal processing of tracing:

    @dot
    digraph tracing {
        compound=true;
        fontname="courier";
        fontsize=11;
        labeljust=l;
        node [shape=record, width=2, fontname="courier", fontsize=11, fillcolor=white, style=filled];
        macro [ fontname="courier bold", fontcolor=dodgerblue4, label="TRACE(tracelevel, args ...)", URL="@ref TRACE()"];
        trace [ fontname="courier bold", fontcolor=dodgerblue4, label="traceObject.trace", URL="@ref Trace::trace"];
        add_parameter [ fontname="courier bold", fontcolor=dodgerblue4, label="Trace::processParms", URL="@ref Trace::processParms"];
        subgraph cluster_trace {
            label="traceObject.trace";
            write_msg [ label="create message|<addparamemeter> add parameter|write message" ];
        }
        macro -> trace;
        trace -> write_msg [lhead=cluster_trace,minlen=2];
        write_msg:addparamemeter -> add_parameter [];
        add_parameter -> add_parameter [];
    }
    @enddot


 */

class Trace
{
private:
    std::mutex mtx;
    int fd;
    std::string fileName;
public:
    enum traceLevel
    {
        none, always, error, normal, full
    };
private:
    std::atomic<Trace::traceLevel> trclevel;

    void processParms(std::stringstream *stream)
    {
        *stream << "";
    }

    template<typename T>
    void processParms(std::stringstream *stream, std::string varlist, T s)
    {
        *stream
                << varlist.substr(varlist.find_first_not_of(" "),
                        varlist.size()) << "(" << s << ")";
    }

    template<typename T, typename ... Args>
    void processParms(std::stringstream *stream, std::string varlist, T s,
            Args ... args)
    {
        *stream
                << varlist.substr(varlist.find_first_not_of(" "),
                        varlist.find(',')) << "(" << s << "), ";
        processParms(stream,
                varlist.substr(varlist.find(',') + 1, varlist.size()),
                args ...);
    }
public:
    Trace() :
            fd(Const::UNSET), fileName(Const::TRACE_FILE), trclevel(error)
    {
    }
    ~Trace();

    void init(std::string extension = "");
    void rotate();

    void setTrclevel(traceLevel level);
    int getTrclevel();

    template<typename ... Args>
    void trace(const char *filename, int linenr, traceLevel tl,
            std::string varlist, Args ... args)

    {
        struct timeval curtime;
        struct tm tmval;
        std::stringstream stream;
        char curctime[26];

        if (getTrclevel() > none && tl <= getTrclevel()) {
            try {
                gettimeofday(&curtime, NULL);
                localtime_r(&(curtime.tv_sec), &tmval);
                strftime(curctime, sizeof(curctime) - 1, "%Y-%m-%dT%H:%M:%S",
                        &tmval);
                stream << curctime << "." << std::setfill('0') << std::setw(6)
                        << curtime.tv_usec << ":[" << std::setfill('0')
                        << std::setw(6) << getpid() << ":" << std::setfill('0')
                        << std::setw(6) << syscall(SYS_gettid) << "]:"
                        << std::setfill('-') << std::setw(20)
                        << basename((char *) filename) << "("
                        << std::setfill('0') << std::setw(4) << linenr << "): ";
                processParms(&stream, varlist, args ...);
                stream << std::endl;

                std::lock_guard<std::mutex> lock(mtx);
                rotate();
                if (write(fd, stream.str().c_str(), stream.str().size())
                        != (unsigned) stream.str().size())
                    THROW(Error::GENERAL_ERROR, errno, fd);
            } catch (const std::exception& e) {
                MSG(LTFSDMX0002E, e.what());
                exit((int) Error::GENERAL_ERROR);
            }
        }
    }
};

extern Trace traceObject;

#define TRACE(tracelevel, args ...) traceObject.trace(__FILE__, __LINE__, tracelevel, #args, args)
