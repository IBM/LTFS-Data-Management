#include "ServerIncludes.h"

Status mrStatus;

void Status::add(int reqNumber)

{
    SQLStatement stmt;
    int num;
    FsObj::file_state migState;

    std::lock_guard<std::mutex> lock(Status::mtx);

    //assert( allStates.count(reqNumber) == 0 );
    if (allStates.count(reqNumber) != 0)
        return;

    singleState state;

    stmt(Status::STATUS) << reqNumber;
    stmt.prepare();
    while (stmt.step(&migState, &num)) {
        switch (migState) {
            case FsObj::RESIDENT:
            case FsObj::PREMIGRATING:
                state.resident = num;
                break;
            case FsObj::PREMIGRATED:
            case FsObj::STUBBING:
            case FsObj::RECALLING_PREMIG:
                state.premigrated = num;
                break;
            case FsObj::MIGRATED:
            case FsObj::RECALLING_MIG:
                state.migrated = num;
                break;
            case FsObj::FAILED:
                state.failed = num;
                break;
            default:
                TRACE(Trace::error, migState);
        }
    }
    stmt.finalize();
    allStates[reqNumber] = state;
}

void Status::remove(int reqNumber)

{
    std::lock_guard<std::mutex> lock(Status::mtx);

    allStates.erase(reqNumber);
}

void Status::updateSuccess(int reqNumber, FsObj::file_state from,
        FsObj::file_state to)

{
    std::lock_guard<std::mutex> lock(Status::mtx);

    assert(allStates.count(reqNumber) != 0);

    singleState state = allStates[reqNumber];

    switch (from) {
        case FsObj::RESIDENT:
            state.resident--;
            break;
        case FsObj::PREMIGRATED:
            state.premigrated--;
            break;
        case FsObj::MIGRATED:
            state.migrated--;
            break;
        default:
            break;
    }

    switch (to) {
        case FsObj::RESIDENT:
            state.resident++;
            break;
        case FsObj::PREMIGRATED:
            state.premigrated++;
            break;
        case FsObj::MIGRATED:
            state.migrated++;
            break;
        default:
            break;
    }

    allStates[reqNumber] = state;
}

void Status::updateFailed(int reqNumber, FsObj::file_state from)

{
    std::lock_guard<std::mutex> lock(Status::mtx);

    singleState state = allStates[reqNumber];

    switch (from) {
        case FsObj::RESIDENT:
            state.resident--;
            break;
        case FsObj::PREMIGRATED:
            state.premigrated--;
            break;
        case FsObj::MIGRATED:
            state.migrated--;
            break;
        default:
            break;
    }

    state.failed++;

    allStates[reqNumber] = state;
}

void Status::get(int reqNumber, long *resident, long *premigrated,
        long *migrated, long *failed)

{
    std::lock_guard<std::mutex> lock(Status::mtx);

    *resident = allStates[reqNumber].resident;
    *premigrated = allStates[reqNumber].premigrated;
    *migrated = allStates[reqNumber].migrated;
    *failed = allStates[reqNumber].failed;
}
