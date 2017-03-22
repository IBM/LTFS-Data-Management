#ifndef _STATUS_H
#define _STATUS_H

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
	void add(int reqNumber);
public:
	Status() {}
	void remove(int reqNumber);
	void updateSuccess(int reqNumber, FsObj::file_state from, FsObj::file_state to);
	void updateFailed(int reqNumber, FsObj::file_state from);
	void get(int reqNumber, long *resident, long *premigrated, long *migrated, long *failed);
};

extern Status mrStatus;

#endif /* _STATUS_H */
