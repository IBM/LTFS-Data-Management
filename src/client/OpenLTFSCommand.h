#pragma once

/** @page client_code Client Code

 ## Class Hierarchy

 For each of the commands there exists a class derived from the
 @ref OpenLTFSCommand class. The class name corresponds to the command name
 that the command names are prepended to the word "Command".

 @dot
 digraph client {
     fontname=default;
     fontsize=11;
     rankdir=RL;
     node [shape=record, fontsize=11, fillcolor=white, style=filled];
     open_ltfs_command [fontname="fixed bold", fontcolor=dodgerblue4, width=2, label="OpenLTFSCommand" URL="@ref OpenLTFSCommand"];
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
     open_ltfs_command [label="OpenLTFSCommand" URL="@ref OpenLTFSCommand"];
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
 the OpenLTFSCommand::doCommand method. Any command needs to implement
 this method even there is no need to talk to the backend.

 ## Options

 Some commands do an additional option processing. Each option has a particular
 meaning even it is used by different commands. The reason for doing so is to
 make it easier for users to switch between the commands. The option processing
 is performed within the single method OpenLTFSCommand::processOptions. The
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

 The OpenLTFSCommand::checkOptions method checks the the number
 of arguments is correct and the request number is not set.

 ## Command processing

 In general the command processing within the client code is performed
 in the following way:

 @dot
 digraph client_processing {
     rankdir=LR;
     node [shape=record, fontname="fixed", fontsize=11, fillcolor=white, style=filled];
     do_command [ label="doCommand" ];
     process_options [ fontname="fixed bold", fontcolor=dodgerblue4, label="processOptions", URL="@ref OpenLTFSCommand::processOptions" ];
     check_options [ fontname="fixed bold", fontcolor=dodgerblue4, label="checkOptions", URL="@ref OpenLTFSCommand::checkOptions" ];
     talk_to_backend [ label="talkToBackend" ];
     connect_1 [ fontname="fixed bold", fontcolor=dodgerblue4, label="OpenLTFSCommand::connect", URL="@ref OpenLTFSCommand::connect" ];
     create_message [ label="create message" ];
     send [ fontname="fixed bold", fontcolor=dodgerblue4, label="LTFSDmCommClient::send", URL="@ref LTFSDmCommClient::send" ];
     receive [ fontname="fixed bold", fontcolor=dodgerblue4, label="LTFSDmCommClient::recv", URL="@ref LTFSDmCommClient::recv" ];
     eval_result [ label="evaluate result" ];
     connect_2 [ fontname="fixed bold", fontcolor=dodgerblue4, label="LTFSDmCommClient::connect", URL="@ref LTFSDmCommClient::connect" ];
     get_reqnum [ fontname="fixed bold", fontcolor=dodgerblue4, label="OpenLTFSCommand::getRequestNumber", URL="@ref OpenLTFSCommand::getRequestNumber" ¨];

     do_command -> process_options [];
     do_command -> check_options [];
     do_command -> talk_to_backend [];

     talk_to_backend -> connect_1 [];
     talk_to_backend -> create_message [];
     talk_to_backend -> send [];
     talk_to_backend -> receive [];
     talk_to_backend -> eval_result [];

     connect_1 -> connect_2 [];
     connect_1 -> get_reqnum [];
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
 OpenLTFSCommand::processOptions | processing of additional options | yes
 OpenLTFSCommand::checkOptions | checks the number of arguments | yes
 talkToBackend | performs the communication with the backend | no
 OpenLTFSCommand::connect | connects to the backend and retrieves the request number | yes
 create message | assembles the message to send to the backend | no
 LTFSDmCommClient::send | send a message | yes
 LTFSDmCommClient::recv | received a message | yes
 evaluate result | checks the information that has been sent back | no
 LTFSDmCommClient::connect | connects to the backend | yes
 OpenLTFSCommand::getRequestNumber | retrieves the request number | yes

 <BR>

 For the following four commands the processing is described in more detail:

 - @subpage start_processing          "start"
 - @subpage stop_processing           "stop"
 - @subpage migrate_processing        "migrate"
 - @subpage recall_processing         "recall"

 */

class OpenLTFSCommand
{
protected:
    OpenLTFSCommand(std::string command_, std::string optionStr_) :
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
    virtual ~OpenLTFSCommand();
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
