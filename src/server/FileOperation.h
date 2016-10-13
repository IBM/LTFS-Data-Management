#ifndef _FILEOPERATION_H
#define _FILEOPERATION_H

class FileOperation {
protected:
	std::vector<std::string> fileList;
public:
	void addFileName(std::string fileName);
	virtual void start() {}
};


#endif /* _FILEOPERATION_H */
