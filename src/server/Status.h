#pragma once

class Status {
private:
	struct singleState {
		long resident = 0;
		long premigrated = 0;
		long migrated= 0;
		long failed = 0;
	};
	std::map<int, singleState> allStates;
	std::mutex mtx;

	static const std::string STATUS;
public:
	Status() {}
	void add(int reqNumber);
	void remove(int reqNumber);
	void updateSuccess(int reqNumber, FsObj::file_state from, FsObj::file_state to);
	void updateFailed(int reqNumber, FsObj::file_state from);
	void get(int reqNumber, long *resident, long *premigrated, long *migrated, long *failed);
};

extern Status mrStatus;
