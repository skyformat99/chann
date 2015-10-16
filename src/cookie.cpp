
#include "cookie.h"
#include "helper.h"

extern char md5Salt[64];
extern bool stopNewcookie;
extern unqlite *pDb;
extern FILE* log_file;

using namespace std;

void set_cookie(mg_connection *conn, const std::string c){
	char expire[100];
	time_t t = time(NULL) + 60 * 60 * 24 * 30;
	strftime(expire, sizeof(expire), "%a, %d %b %Y %H:%M:%S GMT", gmtime(&t));

	mg_printf(conn,
		"HTTP/1.1 200 OK\r\n"
		"Set-Cookie: ssid=%s; expires=%s; max-age=%d; path=/; http-only; HttpOnly;\r\n"
		"Content-type: text/html\r\n"
		"X-UA-Compatible: IE=edge\r\n"
		"Content-Length: 0\r\n",
		c.c_str(), expire, 60 * 60 * 24 * 30);
}

char* generateSSID(const char *user_name) {
	char *hash = new char[33];
	mg_md5(hash, user_name, ":", md5Salt, NULL);

	return hash;
}

string to_ssid(const char* username){
	char newssid[33];
	strcpy(newssid, generateSSID(username));
	char finalssid[64];// = new char[64];

	int len = sprintf(finalssid, "%s|%s", username, newssid);
	finalssid[len] = 0;

	std::string ret(finalssid);

	return ret;
}

string to_ssid(const string username){
	char newssid[33];
	strcpy(newssid, generateSSID(username.c_str()));
	char finalssid[64];// = new char[64];

	int len = sprintf(finalssid, "%s|%s", username.c_str(), newssid);
	finalssid[len] = 0;

	std::string ret(finalssid);

	return ret;
}


void destoryCookie(mg_connection *conn){
	mg_printf(conn,
		"HTTP/1.1 200 OK\r\n"
		"Content-type: text/html\r\n"
		"Set-Cookie: ssid=\"(none)\"; expires=\"Sat, 31 Jul 1993 00:00:00 GMT\"; path=/; http-only; HttpOnly;\r\n"
		"Content-Length: 0\r\n");
}

string random_9chars(){
	if (stopNewcookie) return NULL;

	char username[10];// = new char(10);
	unqlite_util_random_string(pDb, username, 9);
	username[9] = 0;

	string ret(username);

	return ret;
}

string verify_cookie(mg_connection* conn){
	// char ssid[128];
	string ssid = extract_ssid(conn);
	string username = "";
	// char *username = new char[10];
	// mg_parse_header(mg_get_header(conn, "Cookie"), "ssid", ssid, sizeof(ssid));
	vector<string> zztmp = split(ssid, string("|"));
	if (zztmp.size() != 2) return username;

	username = zztmp[0];

	string _ssid = to_ssid(username.c_str());
	// strcpy(testssid, generateSSID(username));

	return (_ssid == ssid) ? username : "";
	// if (strcmp(testssid, zztmp[1].c_str()) == 0)
	// 	return username;
	// else
	// 	return NULL;
}

char* verifyCookieStr(char* szSSID){
	char *username = new char[10];
	vector<string> zztmp = split(string(szSSID), string("|"));
	if (zztmp.size() != 2) return NULL;

	strncpy(username, zztmp[0].c_str(), 10);
	char testssid[33];
	strcpy(testssid, generateSSID(username));

	if (strcmp(testssid, zztmp[1].c_str()) == 0)
		return username;
	else
		return NULL;
}