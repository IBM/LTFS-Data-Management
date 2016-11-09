class Updater {
protected:
	static std::mutex updmutex;
	static std::condition_variable updcond;
	static std::atomic<bool> done;
	long reqNum;
	struct updateInfo {
		int start;
		int end;
		FsObj::file_state state;
	};
public:
	MigrationUpdater(int _reqNum) : reqNum(_reqNum), done(false) {}
	virtual void run() {}
};

class  MigrationUpdater : public Updater {
private:
	std::map<int, updateInfo> statusMap;
public:
	void setUpdate(int colGrp, int end, FsObj::file_state state);
	void setStart(int colGrp, int start);
	updateInfo getUpdate(int colGrp);
	void run();
};

class  RecallUpdater : public Updater {
private:
	std::map<std::string, updateInfo> statusMap;
public:
	void setUpdate(std::string tapeId, int end, FsObj::file_state state);
	void setStart(std::string tapeId, int start);
	updateInfo getUpdate(std::string tapeId);
	void run();
};
