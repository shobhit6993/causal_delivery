#include "process.h"
#define PID 0
pthread_mutex_t lock;

Process::Process(): vc(), delay(N, 0), fd(N, -1), send_port_no(N), listen_port_no(N)
{
    port_pid_map.insert(make_pair(atoi(SEND_PORT0), 0));
    port_pid_map.insert(make_pair(atoi(SEND_PORT1), 1));
    port_pid_map.insert(make_pair(atoi(SEND_PORT2), 2));
    // port_pid_map.insert(make_pair(atoi(SEND_PORT3), 3));
    // port_pid_map.insert(make_pair(atoi(SEND_PORT4), 4));

    send_port_no[0] = SEND_PORT0;
    send_port_no[1] = SEND_PORT1;
    send_port_no[2] = SEND_PORT2;
    // send_port_no[3] = SEND_PORT3;
    // send_port_no[4] = SEND_PORT4;

    listen_port_no[0] = LISTEN_PORT0;
    listen_port_no[1] = LISTEN_PORT1;
    listen_port_no[2] = LISTEN_PORT2;
    // listen_port_no[3] = LISTEN_PORT3;
    // listen_port_no[4] = LISTEN_PORT4;
}

// get port number
int Process::return_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return (((struct sockaddr_in*)sa)->sin_port);
    }

    return (((struct sockaddr_in6*)sa)->sin6_port);
}

string Process::get_listen_port_no(int _pid)
{
    return listen_port_no[_pid];
}

int Process::get_port_pid_map(int port)
{
    return port_pid_map[port];
}

void Process::set_fd(int incoming_port, int new_fd)
{

    cout<<"out_set_fd"<<incoming_port<<" "<<new_fd<<" "<<port_pid_map[incoming_port]<<" "<<fd[port_pid_map[incoming_port]]<<endl;
    pthread_mutex_lock(&lock);
    if (fd[port_pid_map[incoming_port]] == -1)
    {
        cout<<"in_set_fd"<<incoming_port<<" "<<new_fd<<endl;
        fd[port_pid_map[incoming_port]] = new_fd;
    }
    pthread_mutex_unlock(&lock);
}

void Process::set_fd_by_pid(int _pid, int new_fd)
{

    // cout<<"out_set_fd"<<incoming_port<<" "<<new_fd<<" "<<port_pid_map[incoming_port]<<" "<<fd[port_pid_map[incoming_port]]<<endl;
    pthread_mutex_lock(&lock);
    if (fd[_pid] == -1)
    {
        // cout<<"in_set_fd"<<incoming_port<<" "<<new_fd<<endl;
        fd[_pid] = new_fd;
    }
    pthread_mutex_unlock(&lock);
}

time_t Process::get_br_time(int i)
{
    return br_time[i];
}

int Process::get_fd(int _pid)
{
    int ret;
    pthread_mutex_lock(&lock);
    ret = fd[_pid];
    pthread_mutex_unlock(&lock);
    return ret;
}

int Process::get_br_time_size()
{
    return br_time.size();
}

void Process::read_config(string filename)
{
    ifstream fin;
    fin.exceptions ( ifstream::failbit | ifstream::badbit );
    try
    {
        fin.open(filename.c_str());

        int i = 0, d;
        while (i < N && !fin.eof())
        {
            for (int j = 0; j < N; ++j)
            {
                fin >> d;
                if (i == PID)
                {
                    delay[j] = d;
                }
            }

            i++;
        }

        i = 0;
        int p, t;
        string garbage, line;

        while (i < N && !fin.eof())
        {
            fin >> p;
            fin >> garbage >> garbage;
            std::getline(fin, line);

            if (p == PID)
            {
                std::istringstream iss(line);
                while (iss >> t)
                {
                    br_time.push_back(t);
                }
                break;
            }

            i++;
        }

        sort(br_time.begin(), br_time.end());

    }
    catch (ifstream::failure e)
    {
        cerr << e.what();
    }

    fin.close();
}

void* server(void* _P)
{
    Process *P = (Process *)_P;

    int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    struct sigaction sa;
    int yes = 1;
    char s[INET6_ADDRSTRLEN];
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP
    if ((rv = getaddrinfo(NULL, (P->get_listen_port_no(PID)).c_str(), &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit (1);
    }

    // loop through all the results and bind to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                       sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    cout << "p" << PID << " server: waiting for connections...\n";
    cout << "p" << PID << "server sockfd=" << sockfd << endl;
    while (1)
    {   // main accept() loop
        sin_size = sizeof their_addr;

        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1)
        {
            perror("accept");
            continue;
        }

        // inet_ntop(their_addr.ss_family,
        //           return_in_addr((struct sockaddr *)&their_addr),
        //           s, sizeof s);
        int incoming_port = ntohs(P->return_in_addr((struct sockaddr *)&their_addr));
        printf("server %d: got connection from %d\n", PID, incoming_port);
        P->set_fd(incoming_port, new_fd);

        cout << "P" << PID << "accepting connection from P" << P->get_port_pid_map(incoming_port) << "using sockfd=" << new_fd << " but using:" << P->get_fd(P->get_port_pid_map(incoming_port)) << endl;

        // add the fd for this process to fd vector for future communication

        int child_pid = fork();
        if (child_pid == 0)
        {   // this is the child process
            // close(sockfd); // child doesn't need the listener
            // cout << "FD:A" << new_fd << endl;
            // if (send(new_fd, ST_BR_MSG.c_str(), ST_BR_MSG.size(), 0) == -1)
            //     perror("send");

            // cout << "FD:B" << new_fd << endl;

            // P->set_fd(incoming_port, new_fd);

            // cout << "FD:C" << P->get_fd(P->get_port_pid_map(incoming_port)) << endl;
            // usleep(100000 * 1000);

            // close(new_fd);
            exit(0);
        }
        // int returnStatus;
        // waitpid(child_pid, &returnStatus, 0);
        // char buf[MAXDATASIZE];
        //     int numbytes;
        // if ((numbytes = recv(P->get_fd(0), buf, MAXDATASIZE-1, 0)) == -1) {
        //         perror("recv");
        //         exit(1);
        //     }

        //     buf[numbytes] = '\0';

        //     printf("client: received '%s'\n",buf);
    }

    pthread_exit(NULL);
}

int Process::client(int _pid)
{
    int sockfd;  // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *clientinfo, *p;

    struct sigaction sa;
    int yes = 1;
    int rv;

    // set up addrinfo for client (i.e. self)
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP
    if ((rv = getaddrinfo(NULL, send_port_no[PID].c_str(), &hints, &clientinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit (1);
    }

    // loop through all the results and bind to the first we can
    for (p = clientinfo; p != NULL; p = p->ai_next)
    {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                       sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(clientinfo); // all done with this structure

    if (p == NULL)  {
        fprintf(stderr, "client: failed to bind\n");
        exit(1);
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }


    // set up addrinfo for server
    int numbytes;
    // char buf[MAXDATASIZE];
    struct addrinfo *servinfo;
    char s[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, listen_port_no[_pid].c_str(), &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and connect to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            cerr << "P" << PID << "client: connect error\n";
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }

    // inet_ntop(p->ai_family, return_in_addr((struct sockaddr *)p->ai_addr),
    //           s, sizeof s);
    int outgoing_port = ntohs(return_in_addr((struct sockaddr *)p->ai_addr));
    printf("client %d: connecting to %d\n", PID, outgoing_port);
    freeaddrinfo(servinfo); // all done with this structure
    set_fd_by_pid(_pid, sockfd);
    cout << "P" << PID << "initiating connection to P" << _pid << "using sockfd=" << sockfd << " but using:" << get_fd(_pid) << endl;

    // fd[port_pid_map[outgoing_port]] = sockfd;

    // if ((numbytes = recv(sockfd, buf, MAXDATASIZE - 1, 0)) == -1) {
    //     perror("recv");
    //     exit(1);
    // }

    // buf[numbytes] = '\0';

    // printf("client: received '%s'\n", buf);

    // close(sockfd);
}

void Process::print()
{
    cout << PID << "..";
    // PR(fd[0])
    // PR(fd[1])
    // PR(fd[2])
    // PR(fd[3])
    for (int i = 0; i < N; ++i)
    {
        cout << fd[i] << " ";
    }
    cout << endl;
    // for (int i = 0; i < N; ++i)
    // {
    //     cout << delay[i] << " ";
    // }
    // cout << endl;
    // for (int i = 0; i < br_time.size(); ++i)
    // {
    //     cout << br_time[i] << " ";
    // }
}

void Process::initiate_connections()
{
    // every process initiates connect to every process with pid < self.pid, including self
    for (int i = 0; i <= PID; ++i)
    {
        client(i);
    }
}

void Process::wait_for_st_br_msg()
{
    for (int i = 0; i < N; ++i)
    {
        if (i == PID)
            continue;

        char buf[MAXDATASIZE];
        int numbytes;
        cout << PID << "#" << "ST_BR Stuck at" << i << endl;
        // memset(buf, '\0', MAXDATASIZE);
        if ((numbytes = recv(get_fd(i), buf, MAXDATASIZE - 1, 0)) == -1)
        {
            cerr << PID << "#" << "error in receiving ST_BR for P" << i << endl;
            exit(1);
        }
        else if (numbytes == 0)     //connection closed
        {
            cerr << PID << "#" << "Connection closed by P" << i << endl;
            break;
        }
        else
        {
            buf[numbytes] = '\0';
            if (string(buf) == ST_BR_MSG)
            {
                cout << PID << "#" << "ST_BR_MSG rcvd from P" << i << "-" << buf << "on sockfd=" << get_fd(i) << endl;
            }
        }
        usleep(100 * 1000);
    }
    cout << PID << "#" << "Received ST_BR_MSG from all process" << endl;
}

void* start_broadcast(void* _P)
{
    Process *P = (Process *)_P;

    P->start_time = time(NULL);
    // cout<<"P"<<PID<<" start_time="<<P->start_time<<endl;
    int i = 0, msg_counter = 1;
    int n = P->get_br_time_size();
    while (i < n)
    {
        // cout<<"t="<<time(NULL) - (P->start_time)<<endl;
        if ((time(NULL) - (P->start_time)) == (P->get_br_time(i)))
        {
            stringstream ss;
            ss << PID;
            string msg = "P" + ss.str() + ":";
            ss.str("");
            ss << msg_counter;
            msg = msg + ss.str();

            for (int j = 0; j < N; ++j)
            {
                // if (j == PID)
                // {
                //     // cout << PID << "#"<< "Not sending to self..." << endl;
                //     continue;
                // }
                cout << PID << "#" << "P" << j << P->get_fd(j) << endl;
                if (send(P->get_fd(j), msg.c_str(), msg.size(), 0) == -1)
                {
                    cerr << "error sending to process P" << j << endl;
                }
                else
                {
                    cout << PID << "#" << "Sent msg from P" << PID << " to P" << j << "-" << msg << "using sockfd=" << P->get_fd(j) << endl;
                }
            }
            P->log_br(msg, PID, P->get_br_time(i));
            msg_counter++;
            i++;
        }
        usleep(100 * 1000);
    }
    cout << PID << "#" << "start_broadcast thread Sleeping" << endl;
    // usleep(100000 * 1000);

    pthread_exit(NULL);
}

void Process::log_br(string msg, int pid, time_t t)
{
    ofstream fout;
    stringstream ss;
    ss << pid;
    string logfile = LOG_FILE + ss.str() + ".txt";

    fout.open(logfile.c_str(), ios::app);
    fout << t << "\t";
    fout << "p" << pid << " BRC " << msg << endl;
    fout.close();
}

void Process::log_rcv(string msg, int pid, time_t t)
{
    ofstream fout;
    stringstream ss;
    ss << pid;
    string logfile = LOG_FILE + ss.str() + ".txt";

    fout.open(logfile.c_str(), ios::app);
    fout << t << "\t";
    fout << "p" << pid << " REC " << msg << endl;
    fout.close();
}

void* receive(void* argument)
{
    Arg *A = (Arg*) argument;
    Process *P = A->P;
    int pid = A->pid;

    int numbytes;
    while (true)
    {
        char buf[MAXDATASIZE];
        cout << PID << "#" << "Stuck at" << pid << endl;
        // memset(buf, '\0', MAXDATASIZE);
        if ((numbytes = recv(P->get_fd(pid), buf, MAXDATASIZE - 1, 0)) == -1)
        {
            cerr << PID << "#" << "error in receiving for P" << pid << endl;
            exit(1);
        }
        else if (numbytes == 0)     //connection closed
        {
            cerr << PID << "#" << "Connection closed by P" << pid << endl;
            break;
        }
        else
        {
            buf[numbytes] = '\0';
            cout << PID << "#" << "Msg rcvd from P" << pid << "-" << buf << "on sockfd=" << P->get_fd(pid) << endl;
            P->log_rcv(string(buf), PID, time(NULL) - (P->start_time));
        }
        usleep(100 * 1000);
    }
    cout << PID << "#" << "receive thread exiting for P" << pid << endl;
    pthread_exit(NULL);
}

void sigchld_handler(int s)
{
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while (waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}

int main(int argc, char const *argv[])
{
    Process *P = new Process;
    P->read_config(CONFIG_FILE);

    if (pthread_mutex_init(&lock, NULL) != 0)
    {
        printf("\n mutex init failed\n");
        return 1;
    }

    pthread_t server_thread;
    int rv = pthread_create(&server_thread, NULL, server, (void *)P);

    if (rv)
    {
        cout << "Error:unable to create thread for server:" << rv << endl;
        return 1;
    }

    cout << "Sleeping for 3 secs" << endl;
    usleep(3000 * 1000);
    cout << "Up and running once again" << endl;

    void *status;

    P->initiate_connections();
    usleep(5000 * 1000);
    P->print();

    pthread_t broadcast_thread;
    rv = pthread_create(&broadcast_thread, NULL, start_broadcast, (void *)P);

    if (rv)
    {
        cout << "Error:unable to create thread for broadcast:" << rv << endl;
        return 1;
    }


    pthread_t receive_thread[N];
    Arg **A = new Arg*[N];
    for (int i = 0; i < N; ++i)
    {
        A[i] = new Arg;
        A[i]->P = P;
    }
    // A->P = P;
    // usleep(2000 * 1000);
    for (int i = 0; i < N; ++i)
    {

        // if (i == PID)
        // {
        //     // cout << "Not creating thread for receving from self..." << endl;
        //     continue;
        // }
        A[i]->pid = i;
        rv = pthread_create(&receive_thread[i], NULL, receive, (void *)A[i]);

        if (rv)
        {
            cout << "Error:unable to create thread for receive for " << i << ":" << rv << endl;
            return 1;
        }
    }
    // pthread_join(broadcast_thread, &status);
    for (int i = 0; i < N; ++i)
    {
        // cout << "HERE1";
        pthread_join(receive_thread[i], &status);
        // cout << "HERE2";
    }

    pthread_join(broadcast_thread, &status);
    pthread_join(server_thread, &status);
    pthread_exit(NULL);
    // Pserver();
    // p.initiate_connections();
    return 0;
}