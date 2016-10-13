#ifndef _MIGRATION_H
#define _MIGRATION_H

class Migration : public FileOperation {
private:
	unsigned long pid;
	long reqNumber;
	int colFactor;
	LTFSDmProtocol::LTFSDmMigRequest::State targetState;
public:
	Migration(unsigned long _pid, long _reqNumber, int _colFactor, LTFSDmProtocol::LTFSDmMigRequest::State _targetState) :
		pid(_pid), reqNumber(_reqNumber), colFactor(_colFactor), targetState(_targetState) {}
	void start();
};

#endif /* _MIGRATION_H */
