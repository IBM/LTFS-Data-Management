#pragma once

/** @page client_code Client Code

 ## Class Hierarchy

 For each of the commands there exists a class derived from the
 @ref LTFSDMCommand class. The class name corresponds to the command name
 that the command names are prepended to the word "Command".

 @dot
 digraph client {
     fontname=default;
     fontsize=11;
     rankdir=RL;
     node [shape=record, fontsize=11, fillcolor=white, style=filled];
     open_ltfs_command [fontname="fixed bold", fontcolor=dodgerblue4, width=2, label="LTFSDMCommand" URL="@ref LTFSDMCommand"];
     command [fontname="fixed", fontcolor=black, label="ltfsdm X Y → class XYCommand"];
     command -> open_ltfs_command;
 }
 @enddot

 E.g. for the @ref ltfsdm_migrate "ltfsdm migrate" and for the @ref ltfsdm_recall "ltfsdm recall"
 commands there exist @ref MigrateCommand and @ref RecallCommand classes:

 @dot
 digraph client {
     fontname=default;
     fontsize=11;
     rankdir=RL;
     node [shape=record, fontname="fixed bold", fontcolor=dodgerblue4, fontsize=11, fillcolor=white, style=filled, width=2];
     open_ltfs_command [label="LTFSDMCommand" URL="@ref LTFSDMCommand"];
     migrate_command [label="MigrateCommand" URL="@ref MigrateCommand"];
     recall_command [label="RecallCommand" URL="@ref RecallCommand"];
     other_commands [fontname="fixed", fontcolor=black, label="..."];
     migrate_command -> open_ltfs_command;
     recall_command -> open_ltfs_command;
     other_commands -> open_ltfs_command;
 }
 @enddot

 Any new command should follow this rule of creating a corresponding class name.

 The actual processing of a command happens within virtual
 the LTFSDMCommand::doCommand method. Any command needs to implement
 this method even there is no need to talk to the backend.

 ## Options

 Some commands do an additional option processing. Each option has a particular
 meaning even it is used by different commands. The reason for doing so is to
 make it easier for users to switch between the commands. The option processing
 is performed within the single method LTFSDMCommand::processOptions. The
 following is a list of all options:

 option | meaning
 ---|---
 -h | show the usage
 -p | migrate to premigrate state instead to to fully migrate
 -r | recall to resident state instead to premigrated state
 -n | the request number
 -f | the name of a file that contains a list of file names
 -m | the mount point of a file system to be managed
 -P | a list of up to three tape storage pools (separated by commas)
 -t | the id of a cartridge
 -x | indicates a forced operation

 The LTFSDMCommand::checkOptions method checks the the number
 of arguments is correct and the request number is not set.

 ## Command processing

 In general the command processing within the client code is performed
 in the following way:

 @dot
 digraph client_processing {
     compound=true;
     rankdir=LR;
     fontname="fixed";
     fontsize=11;
     labeljust=l;
     node [shape=record, width=2, fontname="fixed", fontsize=11, fillcolor=white, style=filled];
     do_command [ label="doCommand" ];
     subgraph cluster_do_command {
         label="doCommand";
         process_options [ fontname="fixed bold", fontcolor=dodgerblue4, label="processOptions", URL="@ref LTFSDMCommand::processOptions" ];
         check_options [ fontname="fixed bold", fontcolor=dodgerblue4, label="checkOptions", URL="@ref LTFSDMCommand::checkOptions" ];
         talk_to_backend [ label="talkToBackend" ];
     }
     subgraph cluster_talk_to_backend {
         label="talkToBackend";
         node [width=3];
         connect_1 [ fontname="fixed bold", fontcolor=dodgerblue4, label="LTFSDMCommand::connect", URL="@ref LTFSDMCommand::connect" ];
         create_message [ label="create message" ];
         send [ fontname="fixed bold", fontcolor=dodgerblue4, label="LTFSDmCommClient::send", URL="@ref LTFSDmCommClient::send" ];
         receive [ fontname="fixed bold", fontcolor=dodgerblue4, label="LTFSDmCommClient::recv", URL="@ref LTFSDmCommClient::recv" ];
         eval_result [ label="evaluate result" ];
     }

     subgraph cluster_connect_i {
         label="LTFSDMCommand::connect";
         node [ width=3.5 ];
         connect_2 [ fontname="fixed bold", fontcolor=dodgerblue4, label="LTFSDmCommClient::connect", URL="@ref LTFSDmCommClient::connect" ];
         get_reqnum [ fontname="fixed bold", fontcolor=dodgerblue4, label="LTFSDMCommand::getRequestNumber", URL="@ref LTFSDMCommand::getRequestNumber" ¨];
     }
     do_command -> check_options [lhead=cluster_do_command];
     talk_to_backend -> send [lhead=cluster_talk_to_backend];
     connect_1 -> connect_2 [lhead=cluster_connect_i];
 }
 @enddot

 <BR>

 Not all items are performed for all commands. The [ltfsdm info files](@ref ltfsdm_info_files)
 command e.g does not communicate to the backend since it is not necessary to
 do so for the file state evaluation. The following gives a description of
 each item:

 item | description | same implementation for all commands
 ---|---|:---:
 doCommand | performs all required operations for a certain command | no
 LTFSDMCommand::processOptions | processing of additional options | yes
 LTFSDMCommand::checkOptions | checks the number of arguments | yes
 talkToBackend | performs the communication with the backend | no
 LTFSDMCommand::connect | connects to the backend and retrieves the request number | yes
 create message | assembles the message to send to the backend | no
 LTFSDmCommClient::send | send a message | yes
 LTFSDmCommClient::recv | received a message | yes
 evaluate result | checks the information that has been sent back | no
 LTFSDmCommClient::connect | connects to the backend | yes
 LTFSDMCommand::getRequestNumber | retrieves the request number | yes

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
                    time(NULL)), poolNames(""), tapeList( { }), forced(false), key(
                    Const::UNSET), commCommand(Const::CLIENT_SOCKET_FILE)
    {
    }
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
    long key;
    LTFSDmCommClient commCommand;

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
    std::string getCommand() { return command; }
    void connect();
    void sendObjects(std::stringstream *parmList);
    void isValidRegularFile();
};
