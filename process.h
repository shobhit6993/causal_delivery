//setbase - cout << setbase (16); cout << 100 << endl; Prints 64
//setfill - cout << setfill ('x') << setw (5); cout << 77 << endl; prints xxx77
//setprecision - cout << setprecision (4) << f << endl; Prints x.xxxx
#include <iostream>
#include <string>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <utility>
#include <algorithm>
#include <map>
#include <list>
#include <cstring>
#include <ctime>
#include "fstream"
#include "sstream"
#include "pthread.h"
#include "time.h"

#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

using namespace std;

#define PR(x) cout << #x " = " << x << "\n";
#define N 5
#define CONFIG_FILE "config"

#define LISTEN_PORT0 "10666"  // the port on which p0 listens for incoming connections
#define LISTEN_PORT1 "11666"  // the port on which p1 listens for incoming connections
#define LISTEN_PORT2 "12666"  // the port on which p2 listens for incoming connections
#define LISTEN_PORT3 "13666"  // the port on which p3 listens for incoming connections
#define LISTEN_PORT4 "14666"  // the port on which p4 listens for incoming connections

#define SEND_PORT0 "10777"        // the port from which p0 sends messages
#define SEND_PORT1 "11777"        // the port from which p1 sends messages
#define SEND_PORT2 "12777"        // the port from which p2 sends messages
#define SEND_PORT3 "13777"        // the port from which p3 sends messages
#define SEND_PORT4 "14777"        // the port from which p4 sends messages

#define MAXDATASIZE 100 // max number of bytes we can get at once 
#define BACKLOG 10   // how many pending connections queue will hold
#define LOG_FILE "log"

extern int PID;

typedef enum
{
    SEND, RECEIVE, DELIVER
} MsgObjType;

struct MsgObj
{
    string msg;
    std::vector<std::vector<int> > sent_trace;
    int source_pid;
    int dest_pid;
    time_t send_time;
    time_t recv_time;
    time_t delv_time;

    MsgObjType type;

    MsgObj( string msg, 
            MsgObjType type, 
            int source_pid = -1, 
            int dest_pid = -1, 
            time_t send_time = -1, 
            time_t recv_time = -1, 
            time_t delv_time = -1, 
            std::vector<std::vector<int> > sent_trace = std::vector<std::vector<int> > (N, std::vector<int> (N, -1)))
            : msg(msg), type(type), source_pid(source_pid), dest_pid(dest_pid), send_time(send_time), 
                recv_time(recv_time), delv_time(delv_time), sent_trace(sent_trace) {};
};

class Process
{
private:
    std::vector<time_t> delay;
    std::vector<std::vector<time_t> > unicast_time;
    std::vector<int> fd;
    std::vector<string> listen_port_no;
    std::vector<string> send_port_no;
    std::map<int, int> port_pid_map;
    std::vector<int> recv_trace;
    std::vector<std::vector<int> > sent_trace;
    std::list<MsgObj> delv_buf;


public:
    std::map<time_t, std::vector<MsgObj> > recv_buf;
    std::map<time_t, std::vector<MsgObj> > log_buf;
    time_t start_time;

    Process();
    void set_fd(int incoming_port, int new_fd);
    void set_fd_by_pid(int _pid, int new_fd);

    string get_listen_port_no(int _pid);
    time_t get_unicast_time(int i, int j);
    int get_fd(int _pid);
    int get_unicast_size(int i);
    int get_port_pid_map(int port);
    string get_send_port_no(int _pid);
    time_t get_delay(int _pid);


    int return_port_no(struct sockaddr * sa);
    void read_config(string filename = CONFIG_FILE);
    void initiate_connections();
    int client(int);
    void print();

    void msg_handler(string msg, MsgObjType type, int source_pid, int dest_pid, time_t send_time, time_t recv_time, time_t delv_time);
    void extract_sent_trace(string msg, string &body, std::vector<std::vector<int> > &sent_trace_msg);
    string construct_msg(int _pid, int msg_counter, string &msg_body);


    void sent_trace_update_send(int s_pid, int r_pid);
    void sent_trace_update_recv(std::vector<std::vector<int> > &sent_trace_msg);
    void delay_receipt(string msg, MsgObjType type, int source_pid, int dest_pid, time_t send_time, time_t recv_time, time_t delv_time, std::vector<std::vector<int> > &sent_trace_msg);


    void add_to_delv_buf(string msg, MsgObjType type, int source_pid, int dest_pid, time_t send_time, time_t recv_time, time_t delv_time, std::vector<std::vector<int> > &sent_trace_msg);
    bool can_deliver(std::vector<std::vector<int> > &sent_trace_msg, int source_pid);

    void deliver(MsgObj& M);
    void causal_delv_handler();

};

struct Arg
{
    Process * P;
    int pid;
};

struct L
{
    string msg;
    int s_pid;
    int d_pid;
    time_t t;
    MsgObjType type;

    L(string msg, int s_pid, int d_pid, time_t t, MsgObjType type): msg(msg), s_pid(s_pid), d_pid(d_pid), t(t), type(type) {};
};

void sigchld_handler(int s);
void* start_unicast(void*);
void* server(void*);
void* receive(void* _P);
void* logger(void* _P);
void self_send(const char buf[MAXDATASIZE], int pid, Process* P);
void write_to_log(string msg, int s_pid, int d_pid, time_t t, MsgObjType type);
void* recv_buf_poller(void* _P);
void usage();

// msg are of the following format (without quotes)
// "P0:21 10 12 1 0"
// message body, followed by a space, followed by sent_trace of the send event of the message serialized
// message body format -- 'P' followed by processID followed by ':' followed by message index (local to each process)
// sent_trace is a space separated list of whole numbers (row0 row1), where each row is space separated list of elems
