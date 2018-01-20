#pragma once

/** @page standard_thread SubServer

    The SubServer class is designated as a facility to start threads
    and wait for their completion. It has the following capabilities:

    - It is possible to set a maximum number of threads. Further thread
      creation is blocked if this limit is hit.
    - A name can be specified for each thread to be created by calling
      the SubServer::enqueue method.
    - The method SubServer::waitAllRemaining blocks until all threads
      are finished.

    For each thread to be executed a waiter thread is created. This waiter
    joins joins the following threads:

    - the thread that has been newly started
    - if it is not the first thread it waits for the previous waiter to
      complete.

    This additional waiter was necessary because of this upper limit of
    number of threads. A simpler implementation would be to have a
    vector of threads and join all of them at the when waiting for
    completion. In this case there would be no control about when
    a thread finished his work.

    @dot
    digraph sub_server {
        compound=yes;
        fontname="courier";
        fontsize=11;
        labeljust=l;
        node [shape=record, width=1, fontname="courier", fontsize=11, fillcolor=white, style=filled];
        style=solid;
        sub_server [fontname="courier bold", fontcolor=dodgerblue4, label="SubServer", URL="@ref SubServer"];
        subgraph cluster_1 {
            thread_1 [label="thread 1"];
            waiter_1 [label="waiter 1"];
        }
        subgraph cluster_2 {
            thread_2 [label="thread 2"];
            waiter_2 [label="waiter 2"];
        }
        subgraph cluster_3 {
            thread_3 [label="thread 3"];
            waiter_3 [label="waiter 3"];
        }
        subgraph cluster_n {
            thread_n [label="..."];
            waiter_n [label="..."];
        }
        thread_1 -> waiter_1 -> thread_2 -> waiter_2 -> thread_3 -> waiter_3 -> thread_n -> waiter_n [style=invis]; // work around
        sub_server:s -> thread_1:n [lhead=cluster_1, fontname="courier", fontsize=8, fontcolor=dodgerblue4, label="enqueue", URL="@ref SubServer::enqueue"];
        sub_server:s -> thread_2:n [lhead=cluster_2, fontname="courier", fontsize=8, fontcolor=dodgerblue4, label="enqueue", URL="@ref SubServer::enqueue"];
        sub_server:s -> thread_3:n [lhead=cluster_3, fontname="courier", fontsize=8, fontcolor=dodgerblue4, label="enqueue", URL="@ref SubServer::enqueue"];
        sub_server:s -> thread_n:n [lhead=cluster_n, style=dotted];
        waiter_1 -> thread_1 [label="join"];
        waiter_2 -> thread_2 [label="1st join"];
        waiter_3 -> thread_3 [label="1st join"];
        waiter_n -> thread_n [style=dotted];
        waiter_n -> waiter_3 [style=dotted, minlen=2];
        waiter_3 -> waiter_2 -> waiter_1 [label="2nd join",minlen=2];
    }
    @enddot


 */

class SubServer
{
private:
    std::atomic<int> count;
    std::atomic<int> maxThreads;
    std::mutex emtx;
    std::condition_variable econd;
    std::mutex bmtx;
    std::condition_variable bcond;
    std::shared_future<void> prev_waiter;
    void waitThread(std::string label, std::shared_future<void> thrd,
            std::shared_future<void> prev_waiter);
public:
    SubServer() :
            count(0), maxThreads(INT_MAX)
    {
    }
    SubServer(int _maxThreads) :
            count(0), maxThreads(_maxThreads)
    {
    }

    void waitAllRemaining();

    template<typename Function, typename ... Args>
    void enqueue(std::string label, Function&& f, Args ... args)
    {
        int countb;
        char threadName[64];
        std::unique_lock < std::mutex > lock(bmtx);

        countb = ++count;

        if (countb >= maxThreads) {
            bcond.wait(lock);
        }

        memset(threadName, 0, 64);
        pthread_getname_np(pthread_self(), threadName, 63);
        pthread_setname_np(pthread_self(), label.c_str());
        std::shared_future<void> task = std::async(std::launch::async, f,
                args ...);
        pthread_setname_np(pthread_self(), (std::string("w:") + label).c_str());
        std::shared_future<void> waiter = std::async(std::launch::async,
                &SubServer::waitThread, this, label, task, prev_waiter);
        pthread_setname_np(pthread_self(), threadName);
        prev_waiter = waiter;
    }
};
