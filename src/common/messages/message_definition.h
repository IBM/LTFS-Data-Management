#ifndef _MESSAGE_DEFINITION_H
#define _MESSAGE_DEFINITION_H

typedef std::string message_t[];
typedef std::string msgname_t[];

enum msg_id {
	OLTFS000001,
	OLTFS000002,
	OLTFS000003,
	OLTFS000004,
	OLTFS000005
};

const message_t messages = {
	"die 1. Nachricht %d xxx",  // OLTFS000001
	"die 2. Nachricht %d xxx",  // OLTFS000002
	"die 3. Nachricht %d xxx",  // OLTFS000003
	"die 4. Nachricht %d xxx",  // OLTFS000004
	"die 5. Nachricht"          // OLTFS000005
};

const msgname_t msgname = {
	"OLTFS000001",
	"OLTFS000002",
	"OLTFS000003",
	"OLTFS000004",
	"OLTFS000005"
};

#endif /* _MESSAGE_DEFINITION_H */
