/** @page receiver_and_message_processing Receiver and client message parsing

    When a client send a message to the backend there are two components
    that are processing such a message:

    - The Receiver listens on a socket and provides the information sent to
    - a MessageParser object that is evaluating the message in a separate thread.

    For details about parsing client messages see @subpage message_parsing.

    The Receiver is started by calling the Receiver::run method. This method
    is executed within a separate thread and it is listening for messages
    sent by a client. If a new message is received further processing is
    performed within another thread to keep the Receiver listening for further
    messages. For these threads a ThreadPool wqm is available calling
    MessageParser::run with message specific parameters: the key number,
    the LTFSDmCommServer command, and a pointer to the Connector.

    @dot
    digraph receiver {
        compound=yes;
        fontname="courier";
        fontsize=11;
        labeljust=l;
        node [shape=record, width=1, fontname="courier", fontsize=11, fillcolor=white, style=filled];
        listen [fontname="courier bold", fontcolor=dodgerblue4, label="command.listen", URL="@ref LTFSDmCommServer::listen"];
        subgraph cluster_loop {
            label="while not terminated"
            accept [fontname="courier bold", fontcolor=dodgerblue4, label="command.accept", URL="@ref LTFSDmCommServer::accept"];
            subgraph cluster_thread_pool {
                fontname="courier bold";
                fontcolor=dodgerblue4;
                label="ThreadPool wqm";
                URL="@ref ThreadPool";
                wqm [label="...|...|<mpo> MessageParser::run|...|..."];
            }
        }
        listen -> accept [lhead=cluster_loop, minlen=2];
        accept -> wqm:mpo[fontname="courier bold", fontsize=8, fontcolor=dodgerblue4, label="wqm.enqueue", URL="@ref ThreadPool::enqueue"];
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
