/**
    @page receiver_and_message_processing Receiver and message processing

    Within the Receiver::run method the backend is listening for messages
    sent by a client. If a new message is received further processing is
    performed within another thread to keep the Receiver listening. For these
    threads a ThreadPool wqm is available calling MessageParser::run with
    message specific parameters: the key number, the LTFSDmCommServer command,
    and a pointer to the Connector.

    @copydoc messaging

    For selective recall and migration the file names need to be transferred
    from the client to the backend. This is handled within the MessageParser::getObjects
    method. Sending the objects and querying the migration and recall status
    is performed over same connection like the initial migration and recall
    requests to be followed and not processed by the Receiver.

    This gives the following picture:

    @dot
    digraph message_processing {
        compound=yes;
        fontname="fixed";
        fontsize=11;
        labeljust=l;
        node [shape=record, width=1, fontname="fixed", fontsize=11, fillcolor=white, style=filled];
        listen [fontname="fixed bold", fontcolor=dodgerblue4, label="command.listen", URL="@ref LTFSDmCommServer::listen"];
        subgraph cluster_loop {
            label="while not terminated"
            accept [fontname="fixed bold", fontcolor=dodgerblue4, label="command.accept", URL="@ref LTFSDmCommServer::accept"];
            //enqueue [fontname="fixed bold", fontcolor=dodgerblue4, label="wqm.enqueue", URL="@ref ThreadPool::enqueue"];
            run [fontname="fixed bold", fontcolor=dodgerblue4, label="MessageParser::run", URL="@ref MessageParser::run"];
            msg [label="MessageParser::...Message"];
            mig_msg [fontname="fixed bold", fontcolor=dodgerblue4, label="MessageParser::migrationMessage", URL="@ref MessageParser::migrationMessage"];
            rec_msg [fontname="fixed bold", fontcolor=dodgerblue4, label="MessageParser::selRecallMessage", URL="@ref MessageParser::selRecallMessage"];
            get_objects [fontname="fixed bold", fontcolor=dodgerblue4, label="MessageParser::getObjects", URL="@ref MessageParser::getObjects"];
            req_status_msg [fontname="fixed bold", fontcolor=dodgerblue4, label="MessageParser::reqStatusMessage", URL="@ref MessageParser::reqStatusMessage"];
        }
        listen -> accept [lhead=cluster_loop, minlen=2];
        accept -> run [fontname="fixed bold", fontsize=8, fontcolor=dodgerblue4, label="wqm.enqueue", URL="@ref ThreadPool::enqueue"];
        run -> msg [];
        run -> mig_msg [];
        run -> rec_msg [];
        mig_msg -> get_objects [fontsize=8, label="1st"];
        mig_msg -> req_status_msg [fontsize=8, label="2nd"];
        rec_msg -> get_objects [fontsize=8, label="1st"];
        rec_msg -> req_status_msg [fontsize=8, label="2nd"];
    }
    @enddot

 */

#include "ServerIncludes.h"

std::atomic<long> globalReqNumber;

void Receiver::run(long key, std::shared_ptr<Connector> connector)

{
    MessageParser mproc;
    std::unique_lock<std::mutex> lock(Server::termmtx);
    ThreadPool<long, LTFSDmCommServer, std::shared_ptr<Connector>> wqm(&MessageParser::run,
            Const::MAX_RECEIVER_THREADS, "msg-wq");
    LTFSDmCommServer command(Const::CLIENT_SOCKET_FILE);

    TRACE(Trace::full, __PRETTY_FUNCTION__);

    globalReqNumber = 0;

    try {
        command.listen();
    } catch (const std::exception& e) {
        TRACE(Trace::error, e.what());
        MSG(LTFSDMS0004E);
        THROW(Error::GENERAL_ERROR);
    }

    while (Server::finishTerminate == false) {
        try {
            command.accept();
        } catch (const std::exception& e) {
            TRACE(Trace::error, e.what());
            MSG(LTFSDMS0005E);
            break;
        }

        try {
            wqm.enqueue(Const::UNSET, key, command, connector);
        } catch (const std::exception& e) {
            TRACE(Trace::error, e.what());
            MSG(LTFSDMS0010E);
            continue;
        }

        if (Server::finishTerminate == false)
            Server::termcond.wait_for(lock, std::chrono::seconds(30));
    }

    MSG(LTFSDMS0075I);

    TRACE(Trace::always, (bool) Server::finishTerminate);

    lock.unlock();

    wqm.waitCompletion(Const::UNSET);

    command.closeRef();

    connector->terminate();

    MSG(LTFSDMS0076I);
}
