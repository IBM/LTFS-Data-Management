#include "ServerIncludes.h"

std::atomic<long> globalReqNumber;

void Receiver::run(long key, Connector *connector)

{
    MessageParser mproc;
    std::unique_lock<std::mutex> lock(Server::termmtx);
    ThreadPool<long, LTFSDmCommServer, Connector*> wq(&MessageParser::run,
            Const::MAX_RECEIVER_THREADS, "msg-wq");
    LTFSDmCommServer command;

    TRACE(Trace::full, __PRETTY_FUNCTION__);

    globalReqNumber = 0;

    try {
        command.listen();
    } catch (const std::exception& e) {
        TRACE(Trace::error, e.what());
        MSG(LTFSDMS0004E);
        throw(EXCEPTION(Const::UNSET));
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
            wq.enqueue(Const::UNSET, key, command, connector);
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

    wq.waitCompletion(Const::UNSET);
    wq.terminate();

    command.closeRef();

    connector->terminate();

    MSG(LTFSDMS0076I);
}
