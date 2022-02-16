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

/** @page client_code Client Code

 # Options

 Some commands do an additional option processing. Each option has a particular
 meaning even it is used by different commands. The reason for doing so is to
 make it easier for users to switch between the commands. The option processing
 is performed within the single method LTFSDMCommand::processOptions. The
 following is a list of all options:

 option | meaning
 ---|---
 -h                    | show the usage
 -p                    | perform a premigration instead to fully migrate (no stubbing)
 -r                    | recall to resident state instead to premigrated state
 -n @<request number@> | the request number
 -f @<file name@>      | the name of a file that contains a list of file names
 -m @<mount point@>    | the mount point of a file system to be managed
 -P @<pool list@>      | a list of up to three tape storage pools (separated by commas)
 -t @<tape id@>        | the id of a cartridge
 -x                    | indicates a forced operation
 -F                    | format a cartridge when added to a tape storage pool
 -C                    | check a cartridge when added to a tape storage pool

 The LTFSDMCommand::checkOptions method checks if the number
 of arguments is correct and the request number is not set.

 # Command processing

 In general the command processing within the client code is performed
 in the following way:

 @dot
 digraph client_processing {
     compound=true;
     rankdir=LR;
     fontname="courier";
     fontsize=11;
     labeljust=l;
     node [shape=record, width=2, fontname="courier", fontsize=11, fillcolor=white, style=filled];
     do_command [ label="doCommand" ];
     subgraph cluster_do_command {
         label="doCommand";
         process_options [ fontname="courier bold", fontcolor=dodgerblue4, label="processOptions", URL="@ref LTFSDMCommand::processOptions" ];
         check_options [ fontname="courier bold", fontcolor=dodgerblue4, label="checkOptions", URL="@ref LTFSDMCommand::checkOptions" ];
         talk_to_backend [ label="talkToBackend" ];
     }
     subgraph cluster_talk_to_backend {
         label="talkToBackend";
         node [width=3];
         connect_1 [ fontname="courier bold", fontcolor=dodgerblue4, label="LTFSDMCommand::connect", URL="@ref LTFSDMCommand::connect" ];
         create_message [ label="create message" ];
         send [ fontname="courier bold", fontcolor=dodgerblue4, label="LTFSDmCommClient::send", URL="@ref LTFSDmCommClient::send" ];
         receive [ fontname="courier bold", fontcolor=dodgerblue4, label="LTFSDmCommClient::recv", URL="@ref LTFSDmCommClient::recv" ];
         eval_result [ label="evaluate result" ];
     }

     subgraph cluster_connect_i {
         label="LTFSDMCommand::connect";
         node [ width=3.5 ];
         connect_2 [ fontname="courier bold", fontcolor=dodgerblue4, label="LTFSDmCommClient::connect", URL="@ref LTFSDmCommClient::connect" ];
         get_reqnum [ fontname="courier bold", fontcolor=dodgerblue4, label="LTFSDMCommand::getRequestNumber", URL="@ref LTFSDMCommand::getRequestNumber" ];
     }
     do_command -> check_options [lhead=cluster_do_command];
     talk_to_backend -> send [lhead=cluster_talk_to_backend];
     connect_1 -> connect_2 [lhead=cluster_connect_i];
 }
 @enddot

 <BR>

 Not all items that are listed in the following are performed for all commands.
 The [ltfsdm info files](@ref ltfsdm_info_files) command e.g does not
 communicate to the backend since it is not necessary to do so for the file
 state evaluation. The following list gives a description of code components
 of the LTFS Data Management client:

 item | description | same implementation for all commands
 ---|---|:---:
 doCommand | performs all required operations for a certain command | no
 LTFSDMCommand::processOptions | option processing | yes
 LTFSDMCommand::checkOptions | checks the number of arguments | yes
 talkToBackend | performs the communication with the backend | no
 LTFSDMCommand::connect | connects to the backend and retrieves the request number | yes
 create message | assembles a Protocol Buffer message to send to the backend | no
 LTFSDmCommClient::send | sends a Protocol Buffer message | yes
 LTFSDmCommClient::recv | receives a Protocol Buffer message | yes
 evaluate result | checks the information that has been sent from the backend | no
 LTFSDmCommClient::connect | connects to the backend | yes
 LTFSDMCommand::getRequestNumber | retrieves the request number | yes

 <BR>

 For the following four commands the processing is described in more detail:

 - @subpage start_processing          "start"
 - @subpage stop_processing           "stop"
 - @subpage migrate_processing        "migrate"
 - @subpage recall_processing         "recall"

 */

class LTFSDMCommand
{
protected:
    LTFSDMCommand(std::string command_, std::string optionStr_) :
            preMigrate(false), recToResident(false), requestNumber(
                    Const::UNSET), fileList(""), command(command_), optionStr(
                    optionStr_), fsName(""), mountPoint(""), startTime(
                    time(NULL)), poolNames(""), tapeList( { }), forced(false), format(
                    false), check(false), key(Const::UNSET), commCommand(
                    Const::CLIENT_SOCKET_FILE), resident(0), transferred(0), premigrated(
                    0), migrated(0), failed(0), not_all_exist(false)
    {
    }
       bool autoMig;
    bool preMigrate;
    bool recToResident;
    long requestNumber;
    std::string fileList;
    std::string command;
    std::string optionStr;
    std::string fsName;
    std::string mountPoint;
    std::ifstream fileListStrm;
    time_t startTime;
    std::string poolNames;
    std::list<std::string> tapeList;
    bool forced;
    bool format;
    bool check;
    long key;
    LTFSDmCommClient commCommand;
    long resident;
    long transferred;
    long premigrated;
    long migrated;
    long failed;
    bool not_all_exist;

    void getRequestNumber();
    void queryResults();

    void checkOptions(int argc, char **argv);
    virtual void talkToBackend(std::stringstream *parmList)
    {
    }

public:
    virtual ~LTFSDMCommand();
    virtual void printUsage() = 0;
    virtual void doCommand(int argc, char **argv) = 0;

    // non-virtual methods
    void processOptions(int argc, char **argv);
    void traceParms();
    bool compare(std::string name)
    {
        return !command.compare(name);
    }
    std::string getCommand()
    {
        return command;
    }
    void connect();
    void sendObjects(std::stringstream *parmList);
    void isValidRegularFile();
};
