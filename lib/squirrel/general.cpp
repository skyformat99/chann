#include "general.h"

void changeState(struct Thread* t, char statebit, bool op){
	char truebit = statebit;

	if(op){ // add a state
		if(t->state & truebit){} // already has the state
		else
			t->state += truebit;
	}else{
		if(t->state & truebit)
			t->state -= truebit;
		else{} // already lose the state
	}
}

char * resolveState(char state){
	char *ret = new char[64];
	strcpy(ret, "[");
	strcat(ret, state & NORMAL_DISPLAY ? "Normal|" : "Deleted,");
	strcat(ret, state & MAIN_THREAD ? "Thread|" : "");
	strcat(ret, state & THREAD_REPLY ? "Reply|" : "");
	strcat(ret, state & SAGE_THREAD ? "Sage|" : "");
	strcat(ret, state & LOCKED_THREAD ? "Locked|" : "");

	return ret;
}

cclong nextCounter(unqlite *pDb){
	cclong v = readcclong(pDb, "global_counter") + 1;
	writecclong(pDb, "global_counter", v, false);
	return v;
}

int resetDatabase(unqlite *pDb){
	int rc = writecclong(pDb, "global_counter", 0, true);
	if (!rc) return 0;

	//construct the root thread
	cclong *key = new cclong(0);
	struct Thread *t = new Thread();
	t->threadID = 0;
	//t->state = 'r';
	t->nextThread = 0;
	t->childCount = 0;

	strcpy(t->content, "slogan");

	rc = unqlite_kv_store(pDb, key, 4, t, sizeof(struct Thread));
	if (rc != UNQLITE_OK)
		return 0;
	else{
		unqlite_commit(pDb);
		return 1;
	}
}

// cclong findParent(unqlite *pDb, cclong startID){
cclong findParent(unqlite *pDb, struct Thread* b){
	// struct Thread* b = readThread_(pDb, startID);
	cclong startID = b->threadID;
	if (!(b->state & NORMAL_DISPLAY)) return -1;
	if (b->state & MAIN_THREAD) return 0;
	if (b->parentThread) return b->parentThread;

	while (b->nextThread != startID){
		b = readThread_(pDb, b->nextThread);
		if (b->parentThread) return b->parentThread;
	}

	return 0;
}

int deleteThread(unqlite *pDb, cclong tid){
	// cchan will never truely delete a thread, it just hides it
	struct Thread* t = readThread_(pDb, tid);
	struct Thread* tp = readThread_(pDb, t->prevThread);
	struct Thread* tn = readThread_(pDb, t->nextThread);

	if (!(t->state & NORMAL_DISPLAY)) return 0;

	if (t->parentThread == 0 && t->state & MAIN_THREAD){
		tn->prevThread = t->prevThread;
		tp->nextThread = t->nextThread;

		if (t->prevThread == t->nextThread){
			//this happens only if the thread has one child reply
			tn->nextThread = tn->threadID;
			writeThread(pDb, tn->threadID, tn, false);
		}
		else{
			writeThread(pDb, tn->threadID, tn, false);
			writeThread(pDb, tp->threadID, tp, false);
		}

		//t->state = 'd';
		//t->state -= NORMAL_DISPLAY;
		changeState(t, NORMAL_DISPLAY, false);
		writeThread(pDb, t->threadID, t, false);
	}

	if (!(t->state & MAIN_THREAD)){
		//char contentkey[16];
		//unqlite_util_random_string(pDb, contentkey, 15);
		//contentkey[15] = 0;
		//writeString(pDb, contentkey, "<font color='red'>Deleted</font>", false);

		//strncpy(t->content, contentkey, 16);
		//t->state = 'd';
		//t->state -= NORMAL_DISPLAY;
		changeState(t, NORMAL_DISPLAY, false);
		writeThread(pDb, t->threadID, t, false);
	}

	unqlite_commit(pDb);

	return 1;
}

int newThread(unqlite *pDb, const char* content, char* author, const char* email, char* ssid, char* imgSrc, bool sega){
	struct Thread *t = new Thread();

	char contentkey[16];
	unqlite_util_random_string(pDb, contentkey, 15);
	contentkey[15] = 0;
	writeString(pDb, contentkey, content, false);

	strncpy(t->content, contentkey, 16);
	strncpy(t->author, author, 64);
	strncpy(t->email, email, 64);
	strncpy(t->ssid, ssid, 10);
	strncpy(t->imgSrc, imgSrc, 16);

	unqlite_commit(pDb);
	t->state = MAIN_THREAD + NORMAL_DISPLAY;
	if (sega) t->state += SAGE_THREAD;

	time_t rawtime;
	time(&rawtime);
	t->date = rawtime;

	struct Thread *r = readThread_(pDb, 0); // get the root thread
	cclong oriNextThread = r->nextThread;

	//insert
	t->nextThread = r->nextThread;
	t->prevThread = 0; //point to the root
	t->childThread = 0; //no replies yet
	t->parentThread = 0;

	cclong newkey = ++r->childCount; //nextCounter(pDb);
	t->threadID = newkey;

	writeThread(pDb, newkey, t, false);

	r->nextThread = newkey;
	writeThread(pDb, 0, r, false);

	if (oriNextThread){
		struct Thread *rn = readThread_(pDb, oriNextThread);
		rn->prevThread = newkey;
		writeThread(pDb, oriNextThread, rn, false);
	}

	unqlite_commit(pDb);

	delete t;
	delete r;

	return 1;
}

int newReply(unqlite *pDb, cclong id, const char* content, char* author, const char* email, char* ssid, char* imgSrc, bool sega){
	struct Thread *r = readThread_(pDb, id);
	struct Thread *root = readThread_(pDb, 0);
	struct Thread *self = r;
	struct Thread *t = new Thread();
	bool reply2reply = false;

	if (r->state & THREAD_REPLY) reply2reply = true;

	char contentkey[16];
	unqlite_util_random_string(pDb, contentkey, 15);
	contentkey[15] = 0;
	writeString(pDb, contentkey, content, false);

	strncpy(t->content, contentkey, 16);
	strncpy(t->author, author, 64);
	strncpy(t->email, email, 64);
	strncpy(t->ssid, ssid, 10);
	strncpy(t->imgSrc, imgSrc, 16);

	unqlite_commit(pDb);
	t->state = THREAD_REPLY + NORMAL_DISPLAY;
	if (sega) t->state += SAGE_THREAD;

	time_t rawtime;
	time(&rawtime);
	t->date = rawtime;

	if (r->childThread == 0){ //no reply yet
		cclong newkey = ++root->childCount;// nextCounter(pDb);
		t->nextThread = newkey;
		t->prevThread = newkey;
		t->childThread = 0;
		t->parentThread = r->threadID;
		t->threadID = newkey;
		//it's strange and wired but we update the count of children here
		t->childCount = 1;
		writeThread(pDb, newkey, t, false);
		r->childThread = newkey;
		writeThread(pDb, r->threadID, r, false);
	}
	else{ // construct a circled linked list of threads
		r = readThread_(pDb, r->childThread);
		r->childCount++;
		
		struct Thread *rp;
		if (r->prevThread == r->threadID && r->nextThread == r->threadID) // only one child
			rp = r;
		else
			rp = readThread_(pDb, r->prevThread);

		cclong newkey = ++root->childCount; //nextCounter(pDb);

		t->prevThread = r->prevThread;
		t->nextThread = r->threadID;
		t->childThread = t->parentThread = 0;
		t->threadID = newkey;

		r->prevThread = newkey;
		rp->nextThread = newkey;

		writeThread(pDb, newkey, t, false);
		writeThread(pDb, r->threadID, r, false);
		writeThread(pDb, rp->threadID, rp, false);

	}

	
	if (self->prevThread == 0 || self->state & SAGE_THREAD || t->state & SAGE_THREAD || reply2reply){
		//under certain circumstance the thread won't be pushed to the top
		//1. already at top
		//2. is a sega thread
		//3. has a sega reply
		//4. what it replies to is a comment itself
	}
	else{//push the replied thread to the top
		if (self->prevThread) { 
			struct Thread* sp = readThread_(pDb, self->prevThread);
			sp->nextThread = self->nextThread;
			writeThread(pDb, sp->threadID, sp, false);

			if (self->nextThread){
				struct Thread* sn = readThread_(pDb, self->nextThread);
				sn->prevThread = sp->threadID;
				writeThread(pDb, sn->threadID, sn, false);
			}
		}

		self->nextThread = root->nextThread;
		self->prevThread = 0;
		root->nextThread = self->threadID;

		if (self->nextThread){
			struct Thread* sn = readThread_(pDb, self->nextThread);
			sn->prevThread = self->threadID;
			writeThread(pDb, sn->threadID, sn, false);
		}

		writeThread(pDb, self->threadID, self, false);

	}

	writeThread(pDb, 0, root, false);
	unqlite_commit(pDb);

	
	delete root;
	if (self != r) delete self;
	delete r;
	delete t;

	return 1;
}

int displayReply(unqlite *pDb, struct Thread *t){
	cclong ct = t->childThread;
	struct Thread *r = readThread_(pDb, ct); // beginning of the circle
	cclong rid = r->threadID; //the ID

	struct tm * timeinfo;
	timeinfo = localtime(&(r->date));
	fprintf(stdout, "  %d Reply(s)\n", r->childCount);
	fprintf(stdout, "  Reply-ID: %d, Author: %s, Date: %s  %s\n", rid, r->author, asctime(timeinfo), readString(pDb, r->content));

	while (r->prevThread != rid){
		r = readThread_(pDb, r->prevThread);
		timeinfo = localtime(&(r->date));
		fprintf(stdout, "  Reply-ID: %d, Author: %s, Date: %s  %s\n", r->threadID, r->author, asctime(timeinfo), readString(pDb, r->content));
	}

	return 1;
}

int listThread(unqlite *pDb){
	struct Thread *r = readThread_(pDb, 0); // get the root thread
	//iterThread(pDb, r);

	while (r->nextThread){
		r = readThread_(pDb, r->nextThread);

		struct tm * timeinfo;
		timeinfo = localtime(&(r->date));
		fprintf(stdout, "ID: %d, Author: %s, Date: %s%s\n", r->threadID, r->author, asctime(timeinfo), readString(pDb, r->content));

		if (r->childThread) displayReply(pDb, r);
	}

	return 1;
}

int writeString(unqlite *pDb, char* key, const char* value, bool autoCommit){
	int rc;
	rc = unqlite_kv_store_fmt(pDb, key, -1, value); //simple
	if (rc != UNQLITE_OK)
		return 0;
	else{
		if (autoCommit) unqlite_commit(pDb);
		return 1;
	}
}

cclong writeString_(unqlite *pDb, char* value, bool autoCommit){
	int rc;
	cclong* k = new cclong(nextCounter(pDb));
	rc = unqlite_kv_store_fmt(pDb, k, 4, value); //simple
	if (rc != UNQLITE_OK)
		return 0;
	else{
		if (autoCommit) unqlite_commit(pDb);
		return *k;
	}
}

char* readString(unqlite *pDb, char* key){
	unqlite_int64 nBytes;
	int rc;
	rc = unqlite_kv_fetch(pDb, key, -1, NULL, &nBytes);
	if (rc != UNQLITE_OK)
		return 0;

	char *zBuf = new char[++nBytes];
	if (zBuf == NULL) 
		return 0;

	rc = unqlite_kv_fetch(pDb, key, -1, zBuf, &nBytes);
	if (rc != UNQLITE_OK)
		return 0;

	zBuf[nBytes] = 0;

	return zBuf;
}

char* readString_(unqlite *pDb, cclong key){
	unqlite_int64 nBytes;
	cclong *k = new cclong(key);
	int rc;
	rc = unqlite_kv_fetch(pDb, k, 4, NULL, &nBytes);
	if (rc != UNQLITE_OK)
		return 0;

	char *zBuf = new char[++nBytes];
	if (zBuf == NULL)
		return 0;

	rc = unqlite_kv_fetch(pDb, k, 4, zBuf, &nBytes);
	if (rc != UNQLITE_OK)
		return 0;

	zBuf[nBytes] = 0;

	return zBuf;
}

int writecclong(unqlite *pDb, char* key, cclong value, bool autoCommit){
	cclong* zzz = new cclong(value);
	int rc;
	rc = unqlite_kv_store(pDb, key, -1, zzz, 4);
	if (rc != UNQLITE_OK)
		return 0;
	else{
		if(autoCommit) unqlite_commit(pDb);
		return 1;
	}
}

cclong writecclong_(unqlite *pDb, cclong value, bool autoCommit){
	cclong* zzz = new cclong(value);
	cclong *key = new cclong(nextCounter(pDb));
	int rc;
	rc = unqlite_kv_store(pDb, key, 4, zzz, 4);
	if (rc != UNQLITE_OK)
		return 0;
	else{
		if (autoCommit) unqlite_commit(pDb);
		return *key;
	}
}

cclong readcclong(unqlite *pDb, char* key){
	unqlite_int64 nBytes = 4;
	int rc;
	cclong* zBuf = new cclong(0);

	rc = unqlite_kv_fetch(pDb, key, -1, zBuf, &nBytes);
	if (rc != UNQLITE_OK)
		return 0;

	return *zBuf;
}

cclong readcclong_(unqlite *pDb, cclong key){
	unqlite_int64 nBytes = 4;
	cclong *k = new cclong(key);
	int rc;
	cclong* zBuf = new cclong(0);

	rc = unqlite_kv_fetch(pDb, k, 4, zBuf, &nBytes);
	if (rc != UNQLITE_OK)
		return 0;

	return *zBuf;
}

struct Thread* readThread_(unqlite *pDb, cclong key){
	unqlite_int64 nBytes = sizeof(struct Thread);
	// cclong *k = new cclong(key);
	cclong k = key;

	int rc;
	struct Thread* zBuf = new Thread();

	rc = unqlite_kv_fetch(pDb, &k, 4, zBuf, &nBytes);
	
	// delete k;
	//even the fetch function failed, we return an empty structure

	return zBuf;
}

cclong writeNewThread(unqlite *pDb, struct Thread* t, bool autoCommit){
	cclong *key = new cclong(nextCounter(pDb));
	int rc;
	rc = unqlite_kv_store(pDb, key, 4, t, sizeof(struct Thread));
	if (rc != UNQLITE_OK){
		delete key;
		return 0;
	}
	else{
		if (autoCommit) unqlite_commit(pDb);
		return *key;
	}
}

int writeThread(unqlite *pDb, cclong key, struct Thread* t, bool autoCommit){
	// cclong *k = new cclong(key);
	cclong k = key;
	int rc;
	rc = unqlite_kv_store(pDb, &k, 4, t, sizeof(struct Thread));

	if (rc != UNQLITE_OK)
		return 0;
	else{
		if (autoCommit) unqlite_commit(pDb);
		return 1;
	}
}