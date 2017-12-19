# Code Design

## Directories

In the following the internal LTFS Data Management design is described on a high
level. It describes the path it takes from the client side to start
the back end and to migrate, selectively ? and transparently migrate files.
 
LTFS Data Management is a client-server application where the server (sometimes also
referred as backend) is started initially and eventually performs all the
operations and the client - as an interface to LTFS Data Management - that initiates
these operations. A user only has to work with the client part of the
application. The code is structured within the following sub-directories:

path | description
----|----
[src/common](@ref src/common) | code that is used within other parts (client, server, connector, common)
<a href="../messages.cfg">messages.cfg</a> | @subpage messaging_system
[src/common/tracing](@ref src/common/tracing) | @subpage tracing_system
[src/client](@ref src/client) | @subpage client_code
[src/connector](@ref src/connector) | code for the connector library, see @subpage fuse_connector for more information
[src/server](@ref src/server) | @subpage server_code

The common code consists of the following:

path | description
----|----
[src/common/comm](@ref src/common/comm) | the communication between: client ↔ server,server ↔ Fuse overlay file system (transp. recalls)
[src/common/configuration](@ref src/common/configuration) | maintaining the configuration (storage pools, file systems)
[src/common/const](@ref src/common/const) | constants consolidated
[src/common/errors](@ref src/common/errors) | error values consolidated
[src/common/exception](@ref src/common/exception) | the LTFS Data Management exception class
[src/common/messages](@ref src/common/messages) | the LTFS Data Management messaging system
[src/common/msgcompiler](@ref src/common/msgcompiler) | the message compiler that transforms a text based message file into c++ code
[src/common/tracing](@ref src/common/tracing) | the LTFS Data Management tracing facility
[src/common/util](@ref src/common/util) | utility functions

There are two files within the main directory that are used to generate c++ code:

path |description
----|----
ltfsdm.proto | protocol buffers definition file for the communication
messages.cfg | the text based definition file for the messages

## Processes

The following main() function entry points exist:

path |description
----|----
[src/client/ltfsdm.cc](@ref src/client/ltfsdm.cc) | client entry point
[src/server/ltfsdmd.cc](@ref src/server/ltfsdmd.cc) | server entry point
[src/connector/fuse/ltfsdmd.ofs.cc](@ref src/connector/fuse/ltfsdmd.ofs.cc) | Fuse overlay file system entry point
[src/common/msgcompiler/msgcompiler.cc](@ref src/common/msgcompiler/msgcompiler.cc) | message compiler entry point

Each of the main() entry points corresponds to a process that will
be started. The message compiler only is used during the build. The
other three are launched during normal operation:

@dot
digraph start_sequence {
     node [shape=record, width=2 ];
     cli [ label="ltfsdm.cc → main()\n(ltfsdm start)" URL="@ref src/client/ltfsdm.cc" ];
     srv [ label="ltfsdmd.cc → main()\n(ltfsdmd ...)" URL="@ref src/server/ltfsdmd.cc" ];
     con1 [ label="ltfsdmd.ofs.cc → main()\n(ltfsdmd.ofs -m /mnt/fs1 ...)" URL="@ref src/connector/fuse/ltfsdmd.ofs.cc" ];
     con2 [ label="ltfsdmd.ofs.cc → main()\n(ltfsdmd.ofs -m /mnt/fs2 ...)" URL="@ref src/connector/fuse/ltfsdmd.ofs.cc" ];
     con3 [ label="ltfsdmd.ofs.cc → main()\n(ltfsdmd.ofs -m /mnt/fs3 ...)" URL="@ref src/connector/fuse/ltfsdmd.ofs.cc" ];
     conn [label="            "];
     cli -> srv [];
     srv -> con1 [];
     srv -> con2 [];
     srv -> con3 [];
     srv -> conn [ style="dashed" ]
 }
 @enddot

 The number of processes of the Fuse overlay file systems corresponds to
 the number of LTFS Data Management managed file systems. If there is only one file
 system managed there will only be one such process.

The following is an example for processes that are running in the
background if one file system (here <tt><b>/mnt/lxfs</b></tt>) is managed:

@verbatim
   [root\@visp ~]# ps -p $(pidof ltfsdmd) $(pidof ltfsdmd.ofs)\n
    PID TTY      STAT   TIME COMMAND\n
    32246 ?        Ssl  674:51 /root/LTFSDM/bin/ltfsdmd\n
    32263 ?        Sl   918:32 /root/LTFSDM/bin/ltfsdmd.ofs -m /mnt/lxfs -f /dev/sdc1 -S 1510663933 -N 49751364 -l 1 -t 2 -p 32246
@endverbatim

