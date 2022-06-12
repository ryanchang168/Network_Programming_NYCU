#include<stdlib.h>
#include<iostream>
#include<sstream>
#include<string>
#include<vector>
#include<unistd.h>
#include<fcntl.h>
#include<signal.h>
#include<sys/wait.h>
#include<string.h>
#include<pwd.h>
#include<fstream>
#include<bits/stdc++.h>
#include<errno.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<sys/types.h>
#include<arpa/inet.h>
#include<sys/ipc.h>
#include<sys/shm.h>
#include<sys/stat.h>
#include<signal.h>
using namespace std;

struct pipe_info{
  int fd_in;   // command -> pipe
  int fd_out;  // pipe -> command
  int cnt;    // behind which command
  int type;   // 1:normal pipe  2:number pipe  3:number pipe with error   4:file output direction
  int line;   // remaining line for piping (only for number pipe)
  int uid;
};

struct user_info {
  int uid;
  int pid;
  char name[20];
  char ip[INET_ADDRSTRLEN];
  char port[10];
  //map<string, string> env;
};

struct user_pipe_info {
  int fifo[31];
  int exist[31];
};

//vector<user_info> user(1024);
//vector<user_pipe_info> user_pipe;
int shared_u, shared_msg, shared_pipe;
char *buf;
struct user_info *u;
struct user_pipe_info *up;
vector<pipe_info> pipe_table;

/*void user_init(struct user_info &u, int ssockfd){
  user[ssockfd].uid = 0;
  user[ssockfd].sockfd = -1;
  user[ssockfd].name = "(no name)";
  user[ssockfd].env.clear();
  user[ssockfd].pipe_table.clear();
  user[ssockfd].ip = "";
  user[ssockfd].port = -1;
}*/
void pipe_init(struct pipe_info &p){
  p.fd_in = -1;
  p.fd_out = -1;
  p.cnt = -1;
  p.line = -1;
  p.type = 0;
  p.uid = -1;
}

/*void user_pipe_init(struct user_pipe_info &u){
  u.sock_in = -1;
  u.sock_out = -1;
  u.uid_in = -1;
  u.uid_out = -1;
  u.fd_in = -1;
  u.fd_out = -1;
}*/

void sigHandler(int signo){
  int status;
  if(signo == SIGCHLD){
    while(waitpid(-1, &status, WNOHANG) > 0){
      // do nothing
    }
  }
  else if(signo == SIGUSR1)
    cout<< buf <<endl;
  else if(signo == SIGUSR2){
    char fifo_name[20];
    int fifo_out;
    for(int i=1;i<31;i++){
      for(int j=1;j<31;j++){
        if(up[i].exist[j]){
          sprintf(fifo_name, "user_pipe/%d to %d", i, j);
          // ready to receive
          fifo_out = open(fifo_name, O_RDONLY);
          up[i].fifo[j] = fifo_out;
          up[i].exist[j] = 0;
          break;
        }
      }
    }
  }
  else if(signo == SIGINT){
    shmdt(u);
    shmctl(shared_u, IPC_RMID,0);     
    shmdt(buf);
    shmctl(shared_msg, IPC_RMID,0); 
    shmdt(up);
    shmctl(shared_pipe, IPC_RMID,0); 
    exit(1);
  }
}

void broadcast(struct user_info *u, string msg) {
  memset(buf, '\0' ,sizeof(buf));
  strcpy(buf, msg.c_str());

  for(int i=1;i<31;i++){
    if(u[i].pid > 0)
      kill(u[i].pid, SIGUSR1);
  }
}

void process_fork(vector<vector<string>> cmd_table, bool line_ends_with_pipe, int idx, string s){
  //cout<<pipe_table[0].fd_in<<pipe_table[0].fd_out<<"first"<<endl;
  //cout<<line_ends_with_pipe<<endl;
  //cout<<pipe_table.size()<<endl;
  int fifo_out, fifo_out2;
  pid_t pid;
  vector<pid_t> pids;
  bool new_pipe = true;
  //cout<<cmd_table.size()<<endl;
  // cout<<pipe_table[0].type<<endl;
  // cout<<pipe_table[0].line<<endl;
  for(int i=0;i<cmd_table.size();i++){
    //cout<<i<<endl;
    //user_pipe_init(u);
    bool err = false;
    for(int j=0;j<pipe_table.size();j++){
      //cout<<pipe_table[j].type<<endl;
      // pipe for command i
      if(pipe_table[j].cnt == i){         
        new_pipe = true;
        // if belonging to number pipe or number pipe with stderr
        if(pipe_table[j].type==2 || pipe_table[j].type==3){
          for(int k=0;k<j;k++){
            // if the remaning line count is equal,then use the same pipe
            if(pipe_table[k].line == pipe_table[j].line){
              pipe_table[j].fd_in = pipe_table[k].fd_in;
              pipe_table[j].fd_out = pipe_table[k].fd_out;
              new_pipe = false;
              break;
            }
          }
        }
        
        bool user_found = false, pipe_found = false;

        // '<'
        if(pipe_table[j].type == 5){
            new_pipe = false;
            for(int k=1;k<31;k++){
                if((u[k].uid == pipe_table[j].uid)){
                    user_found = true;
                    break;
                }
            }
          
          if(user_found){
            //cout<<up[pipe_table[j].uid].fifo[idx];
            if(up[pipe_table[j].uid].fifo[idx]){
              string msg = "*** " + string(u[idx].name) + " (#" + to_string(u[idx].uid) + ") just received from " + string(u[pipe_table[j].uid].name) + " (#" + to_string(pipe_table[j].uid) + ") by '" + s + "' ***";
              broadcast(u, msg);
              pipe_found = true;
              fifo_out2 = up[pipe_table[j].uid].fifo[idx];
              up[pipe_table[j].uid].fifo[idx] = 0;
            }
        
            if(!pipe_found){
                cerr << "*** Error: the pipe #" << pipe_table[j].uid << "->#" << u[idx].uid << " does not exist yet. ***" << endl;
                err = true;
            }
          }
          else{
            cerr << "*** Error: user #" << pipe_table[j].uid << " does not exist yet. ***" << endl;
            err = true;
          }
        }
          
        if(new_pipe){
            // create a new pipe
            int pip[2];
            int tmp = pipe(pip);
            //cout<<pip[1]<<endl;
            // the order is important !!!!!
            pipe_table[j].fd_in = pip[1];  // for processes to read
            pipe_table[j].fd_out = pip[0];  // for processes to write
            //cout<<pipe_table[j].fd_in<<pipe_table[j].fd_out<<endl;
          
          // '>'                     
          if(pipe_table[j].type == 6){
            //cout<<"wrong?"<<endl;
            user_found = false;
            for(int k=1;k<31;k++){
              //cout<<u[k].uid<<endl;
              if(u[k].uid == pipe_table[j].uid){
                user_found = true;
                break;
              }
            }

            if(user_found){
              if(up[idx].fifo[pipe_table[j].uid]){
                cout << "*** Error: the pipe #"+ to_string(u[idx].uid) +"->#"+ to_string(pipe_table[j].uid) +" already exists. ***" << endl;
                err = true;
              }

              if(!err){
                //cout<<"begin"<<endl;
                up[idx].exist[pipe_table[j].uid] = 1;
                char fifo_name[20];
                sprintf(fifo_name, "user_pipe/%d to %d", idx, pipe_table[j].uid);      
                mkfifo(fifo_name, 0666);
                kill(u[pipe_table[j].uid].pid, SIGUSR2);   
                string msg = "*** " + string(u[idx].name) + " (#" + to_string(u[ idx].uid) + ") just piped '" + s + "' to " + string(u[pipe_table[j].uid].name) + " (#" + to_string(pipe_table[j].uid) + ") ***";
                broadcast(u, msg);
              }
            }
            else{
              cerr << "*** Error: user #" << pipe_table[j].uid << " does not exist yet. ***" << endl;
              err = true;
            }
          }
        }
      }
    }
    
    //cout<<pipe_table[0].fd_in<<pipe_table[0].fd_out<<"second"<<endl;
    //cout<<"test"<<endl;
    pid = fork();
    pids.push_back(pid);
  
    // too many process...
    if(pid < 0){
      pids.pop_back();
      usleep(1000);
      i--;
      continue;
    }
    else if(pid == 0){
       //cout<<i<<endl;
       //cout<<pipe_table.size()<<endl;
      bool file_name = false;
      for(int j=0;j<pipe_table.size();j++){
        //cout<<i<<endl;
        // pipe in front of the command i
        if(i && pipe_table[j].cnt==i-1){
          //cout<<pipe_table[j].fd_out<<endl;
          if(pipe_table[j].type == 1)
              dup2(pipe_table[j].fd_out, 0);   // pipe -> command's stdin
            else if(pipe_table[j].type == 4)
                file_name = true;   // this command is a file name
        }
        
        // pipe behind the command
        if(pipe_table[j].cnt == i){
          //cout<<pipe_table[j].type<<endl;
          //cout<<pipe_table[j].fd_in<<endl;
          if(pipe_table[j].type == 1){
            //cout<<i<<endl;
            dup2(pipe_table[j].fd_in, 1);  // command's stdout -> pipe
            //cout<<errno<<endl;
          }
          else if(pipe_table[j].type == 2)
            dup2(pipe_table[j].fd_in, 1);  // command's stdout -> pipe
          else if(pipe_table[j].type == 3){
            dup2(pipe_table[j].fd_in, 1);  // command's stdout -> pipe
            dup2(pipe_table[j].fd_in, 2);  // command's stderr -> pipe
          }
          else if(pipe_table[j].type == 4){
            int fd = open((cmd_table[i+1][0]).c_str(), O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
            dup2(fd, 1);   // command's stdout -> file
          }
          else if(pipe_table[j].type == 5){
            if(err){
              int blackhole = open("/dev/null", O_RDONLY, 0);
              dup2(blackhole, 0);
            }
            else{
              dup2(fifo_out2, 0);
              //up[pipe_table[j].uid].fifo[idx] = 0;
              //close(fifo_out2);
              char fifo_name[20];
              sprintf(fifo_name, "user_pipe/%d to %d", pipe_table[j].uid, idx);
              unlink(fifo_name);
            }
          }
          else if(pipe_table[j].type == 6){
            if(err){
              int blackhole = open("/dev/null", O_RDWR, 0);
              dup2(blackhole, 1);
            }
            else{
              //cout<<fifo_out<<endl; 
              char fifo_name[20];
              sprintf(fifo_name, "user_pipe/%d to %d", idx, pipe_table[j].uid);           
              fifo_out = open(fifo_name, O_WRONLY);
              //cout<<fifo_out<<endl;
              dup2(fifo_out, 1);
              //close(fifo_out);  
            }
          }
          //cout<<i<<"2"<<endl;
        }
        //cout<<i<<endl;
        
        // number pipe's line remaining is 0
        // pipe into this line's first command
        if(!i && (pipe_table[j].type==2||pipe_table[j].type==3) && !pipe_table[j].line){
          //cout<<"test"<<endl;
          dup2(pipe_table[j].fd_out, 0);
        }
      } 
      //cout<<i<<endl;
  
      // close the fd in child process
      for(vector<pipe_info>::iterator p = pipe_table.begin(); p != pipe_table.end(); p++){
        //cout<<p->fd_in<<endl;
        close(p->fd_in);
        close(p->fd_out);
        //cout<<i<<endl;
      }
      
      //cout<<pipe_table[0].fd_in<<pipe_table[0].fd_out<<"third"<<endl;
      //cout<<i<<endl;
      // nothing important
      if(file_name)
        exit(EXIT_SUCCESS);
      // execute the command
      else{
        // change to the C-style
        // for the argument of "execvp"
        vector<char*> s;
        const char* f = cmd_table[i][0].c_str();
        // cout<<f<<endl;
        for(int j=0;j<cmd_table[i].size();j++){
          const char* a = cmd_table[i][j].c_str();
          s.push_back(strdup(a));
        }
        s.push_back(NULL);
        char **arg = &s[0];
        //cout<<f<<endl;
        int e = execvp(f, arg);
        //cout<<e<<endl;
        if(e == -1){
            cerr << "Unknown command: [" << cmd_table[i][0].c_str() << "]." << endl;
            exit(EXIT_FAILURE);
        }
        exit(EXIT_SUCCESS);
      }
    }
    else{
      //close(fifo_out);
      //close(fifo_out2);
    }
  
    //cout<<"test"<<endl;
    //cout<<err<<endl;
    for(vector<pipe_info>::iterator p = pipe_table.begin(); p != pipe_table.end(); ) {
      /*bool fin = false;
      //cout<<p->line<<endl;
      if(p->cnt == i){
        if(!err){
          //int fd = -1;
          if(p->type == 5){
            fin = true;
            for(int i=1;i<31;i++){
              if(u->uid_in == p->uid && u->uid_out == user[ssockfd].uid){
                fd = u->sock_in;
                close(u->fd_in);
                close(u->fd_out);
                user_pipe.erase(u);
                break;
              }
            }
            for(vector<pipe_info>::iterator p2 = user[fd].pipe_table.begin(); p2 != user[fd].pipe_table.end(); p2++){
                if(p2->type == 6 && p2->uid == user[ssockfd].uid){
                    user[fd].pipe_table.erase(p2);
                    break;
                }
            }
            pipe_table.erase(p);
          }
        }
      }*/
      if(((i && p->cnt==i-1)||(!i && !(p->line))) && p->type<=4){   // already dup
        //fin = true;
        //cout<<"test"<<p->line<<i<<endl;
        close(p->fd_in);
        close(p->fd_out);
        //cout<<i<<endl;
        pipe_table.erase(p);    // erase unnecessary pipe information
      }
      
      else
        p++;
    }
  }
  
  //cout<<"test2"<<endl;
  // keep the order if the output is to be generated
  if(!line_ends_with_pipe){
    for(vector<pid_t>::iterator it = pids.begin(); it != pids.end();){
      int status;
      waitpid((*it), &status, 0);
      it = pids.erase(it);
    }
  }

  for(int i=0; i<pipe_table.size();i++){
        pipe_table[i].cnt = -1;
        if(pipe_table[i].line != -1)
              pipe_table[i].line--;
  }
  return;
}

int string_to_num(string s){
  return atoi(s.substr(1, s.length()).c_str());
}

vector<vector<string>> parse(string cmd_input, bool &line_ends_with_pipe){
  stringstream cmd_ss(cmd_input); // processing "getline"
  vector<string> tokens;
  vector<string> cmd;
  string token;
  vector<vector<string>> cmd_table;
  struct pipe_info p;
  int cnt = 0;
  bool flag = false;
  vector<string> cmds = {"exit", "setenv", "printenv", "who", "tell", "yell", "name"};

  while(getline(cmd_ss, token, ' '))
    tokens.push_back(token);

  if(find(cmds.begin(), cmds.end(), tokens[0]) != cmds.end()){
      for(string t : tokens)
          cmd.push_back(t);
      cmd_table.push_back(cmd);
      return cmd_table;
  }
  
  for(string t : tokens){
    line_ends_with_pipe = true;
    pipe_init(p);
    
    // normal pipe
    if(t == "|"){
      if(cmd.size()){
        cmd_table.push_back(cmd);
        cmd.clear();
      }
      p.type = 1;
      p.cnt = cnt;
      pipe_table.push_back(p);
      cnt++;
    }
    
    // number pipe
    else if(t[0]=='|' && isdigit(t[1])){
      if(cmd.size()){
        cmd_table.push_back(cmd);
        cmd.clear();
      }
      p.type = 2;
      p.cnt = cnt;
      p.line = string_to_num(t);
      pipe_table.push_back(p);
    }
    
    // number pipe with stderr
    else if(t[0]=='!' && isdigit(t[1])){
      if(cmd.size()){
        cmd_table.push_back(cmd);
        cmd.clear();
      }
      p.type = 3;
      p.cnt = cnt;
      p.line = string_to_num(t);
      pipe_table.push_back(p);
    }
    
    // output file redirection
    else if(t == ">"){
      if(cmd.size()){
        cmd_table.push_back(cmd);
        cmd.clear();
      }
      p.type = 4;
      p.cnt = cnt;
      pipe_table.push_back(p);
      cnt++;
    }
    
    else if((t[0]=='<' || t[0]=='>') && isdigit(t[1])){
      if(cmd.size()){
        cmd_table.push_back(cmd);
        cmd.clear();
      }
      p.cnt = cnt;
      p.uid = string_to_num(t);
      if(t[0] == '<')
        p.type = 5;
      else{
        p.type = 6;
        flag = true;
      }
      
      if(flag && pipe_table.back().type==6 && p.type==5)
        pipe_table.insert(pipe_table.end()-1, p);
      else
        pipe_table.push_back(p);
    }
    
    // other commands or arguments
    else{
      cmd.push_back(t);
      //cnt--;   // don't need to add the cnt
      line_ends_with_pipe = false;
    }
  }

  if(! line_ends_with_pipe){
    cmd_table.push_back(cmd);
    cmd.clear();
  }

  if(pipe_table.size() && pipe_table.back().type == 5) 
    line_ends_with_pipe = false;

  return cmd_table;
}

void Begin(int idx){
  /*clearenv();
  setenv("PATH", "bin:.", true);
  map<string, string> env = u[idx].env;
  for(map<string, string>::iterator it = env.begin(); it != env.end(); it++) 
    setenv(it->first.c_str(), it->second.c_str(), true);*/

  while(1){
    cout << "% " << flush;
      
    string cmd_input;
    vector<vector<string>> cmd_table;
    
    getline(cin, cmd_input);
    if(cin.eof())
      exit(EXIT_SUCCESS);

    while(cmd_input.back() == '\r' || cmd_input.back() == '\n')
      cmd_input.pop_back();
    if(!cmd_input.length())
      return;

    //cout<<cmd_input<<endl;
    bool line_ends_with_pipe = true;  // check if ending with number pipe
    cmd_table = parse(cmd_input, line_ends_with_pipe);  // parse one-line command
      
    // built-in command
    if(cmd_table[0][0] == "exit"){
      string msg = "*** User '"+ string(u[idx].name) +"' left. ***";
      broadcast(u, msg);
      char fifo_name[20];
      for(int i=1;i<31;i++){
        //close(up[idx].fifo[i]);
        sprintf(fifo_name, "user_pipe/%d to %d", idx, i);
        unlink(fifo_name);
        sprintf(fifo_name, "user_pipe/%d to %d", i, idx);
        unlink(fifo_name);

        up[idx].fifo[i] = 0;
        up[idx].exist[i] = 0;
        up[i].fifo[idx] = 0;
        up[i].exist[idx] = 0;
      }
      u[idx].uid = 0;
      exit(1);
    }
    else if(cmd_table[0][0] == "printenv"){
      if(cmd_table[0].size() != 2)
        cerr << "Usage: printenv [var]" << endl;
      else{
        char *env_name = getenv(cmd_table[0][1].c_str());
        if(env_name != NULL)
          cout<<env_name<<endl;
      }
    }
    else if(cmd_table[0][0] == "setenv"){
      if(cmd_table[0].size() != 3)
        cerr << "Usage: setenv [var] [value]" << endl;
      else
        setenv(cmd_table[0][1].c_str(), cmd_table[0][2].c_str(), true);
    }
    else if(cmd_table[0][0] == "who"){
      if(cmd_table[0].size() != 1)
        cerr << "Usage: who" << endl;
      else{
        cout << "<ID>\t<nickname>\t<IP:port>\t<indicate me>" << endl;
        for(int i=0;i<31;i++){
          if(u[i].uid){
            string me = "";
            if(i == idx)
                me = "<-me";
            cout << u[i].uid <<  "\t" << u[i].name << "\t" << u[i].ip << ":" << u[i].port << "\t" << me << endl;
          }
        }
      }
    }
    else if(cmd_table[0][0] == "tell"){
      if(cmd_table[0].size() < 3)
        cerr << "Usage: tell [userID] [message]" << endl;
      else{
        if(atoi(cmd_table[0][1].c_str())<1 || atoi(cmd_table[0][1].c_str())>30) {
            cerr << "*** Error: user #" << atoi(cmd_table[0][1].c_str()) << " does not exist yet. ***" << endl;
            continue;
        }
        
        string msg = "";
        for(int i=2;i<cmd_table[0].size();i++)
          msg += cmd_table[0][i] + " ";
        msg.pop_back();
        
        bool find_user = false;
        for(int i=0;i<31;i++) {
          if(u[i].uid == atoi(cmd_table[0][1].c_str())) {
            msg = "*** " + string(u[idx].name) + " told you ***: " + msg;
            memset(buf, '\0', sizeof(buf));
            strcpy(buf,msg.c_str());
            kill(u[i].pid, SIGUSR1);
            find_user = true;
            break;
          }
        }

        if(!find_user){
          msg = "";
          msg = "*** Error: user #" + cmd_table[0][1] + " does not exist yet. ***" + msg;
          memset(buf, '\0' ,sizeof(buf));
          strcpy(buf,msg.c_str());
          kill(u[idx].pid, SIGUSR1);
        }
      }
    }
    else if(cmd_table[0][0] == "yell"){
      if(cmd_table[0].size() < 2)
        cerr << "Usage: yell [message]" << endl;
      else{
        string msg = "";
        for(int i=1;i<cmd_table[0].size();i++)
          msg += cmd_table[0][i] + " ";
        msg.pop_back();
        
        msg = "*** " + string(u[idx].name) + " yelled ***: " + msg;
        broadcast(u, msg);
      }
    }
    else if(cmd_table[0][0] == "name"){
      bool err = false;
      if(cmd_table[0].size() != 2)
        cerr << "Usage: name [New_name]" << endl;
      else{
        for(int i=1;i<31;i++) {
          if(u[i].name == cmd_table[0][1]) {
              cerr << "*** User '" <<  cmd_table[0][1] << "' already exists. ***" << endl;
              err = true;
              break;
          }
        }
        if(err)
          continue;
        string msg = "*** User from " + string(u[idx].ip) + ":" + string(u[idx].port) + " is named '" + cmd_table[0][1] + "'. ***";
        strcpy(u[idx].name, cmd_table[0][1].c_str());
        broadcast(u, msg);
      }
    }
    // other commands
    else
      process_fork(cmd_table, line_ends_with_pipe, idx, cmd_input);
  }

  return;
}

void TCP_server(int p){
  int sockfd = socket(PF_INET, SOCK_STREAM, 0), ssockfd;
  //cout<<sockfd<<endl;
  if(sockfd < 0){
    cerr << "Socket create error !" << endl;
    exit(EXIT_FAILURE);
  }
  
  int flag = 1;
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(int));
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &flag, sizeof(int));

  struct sockaddr_in servaddr, cli;  
  bzero(&servaddr, sizeof(servaddr));
  
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = INADDR_ANY;
  servaddr.sin_port = htons(p);
  
  if ((bind(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr))) < 0){
    cerr << "Bind error !" << endl;
    exit(EXIT_FAILURE);
  }
  
  if ((listen(sockfd, 30)) < 0) {
    cerr << "Listen error !" << endl;
    exit(EXIT_FAILURE);
  }

  // creating shared memory
  shared_u = shmget((key_t) p , 31 * sizeof(struct user_info), 0644|IPC_CREAT);
  shared_msg = shmget((key_t) p+1, sizeof(buf), 0644|IPC_CREAT);
  shared_pipe = shmget((key_t) p+2, 31 * sizeof(struct user_pipe_info), 0644|IPC_CREAT);
  
  u = (user_info*)shmat(shared_u, 0, 0);
  buf = (char*)shmat(shared_msg, 0, 0);
  up = (user_pipe_info*)shmat(shared_pipe, 0, 0);
  
  // initializing the user information
  for(int i=1;i<31;i++){
    u[i].uid = 0;
    u[i].pid = 0;
    memset(u[i].name,'\0',sizeof(u[i].name)); 
    memset(u[i].ip,'\0',sizeof(u[i].ip)); 
    memset(u[i].port,'\0',sizeof(u[i].port)); 
  } 
  
  // initializing the buffer
  memset(buf,'\0',sizeof(buf)); 

  // initializing the user pipe information
  for(int i=1;i<31;i++) {
    for(int k=1;k<31;k++){
      up[i].fifo[k] = 0;
      up[i].exist[k] = 0;     
    }
  }
  while(1){
    socklen_t len = sizeof(cli);
    ssockfd = accept(sockfd, (struct sockaddr *)&cli, &len);
    //cout<<ssockfd<<endl;
    if(ssockfd < 0) {
      cerr << "Accept error !" << endl;
      cout << errno << endl;
      continue;
    }

    int idx = 1;

    // the minimal available index
    while(u[idx].uid)
      idx++;
      
    pid_t pid_user = fork();
    if(pid_user < 0){
      usleep(1000);
      continue;
    }
    else if(!pid_user){
      close(sockfd);
      dup2(ssockfd, 0);
      dup2(ssockfd, 1);
      dup2(ssockfd, 2);
      close(ssockfd);

      cout << "****************************************" << endl;
      cout << "** Welcome to the information server. **" << endl;
      cout << "****************************************" << endl; 

      string msg = "*** User '" + string(u[idx].name) + "' entered from " + string(u[idx].ip) + ":" + string(u[idx].port) + ". ***";
      broadcast(u, msg);

      Begin(idx); 
    }

    else{
      // handle the user information
      close(ssockfd);
      u[idx].pid = pid_user;
      u[idx].uid = idx;   
      strcpy(u[idx].name,"(no name)");
      char s1[INET_ADDRSTRLEN];
      strcpy(u[idx].ip,inet_ntop(PF_INET, &(cli.sin_addr), s1, INET_ADDRSTRLEN));   
      stringstream s2;
      s2<<htons(cli.sin_port);
      string s;
      s2>>s;
      strcpy(u[idx].port,s.c_str());           
    }
  }
  return;
}

  

int main(int argc, char **argv) {
  clearenv();
  setenv("PATH", "bin:.", true);  
   
  if(argc != 2){
    cerr << "Usage : ./np_single_proc [port]" << endl;
    exit(EXIT_FAILURE);
  }
  
  int port_num = atoi(argv[1]);

  // add the signal handler of different type of signal
  signal(SIGCHLD, sigHandler);
  signal(SIGUSR1, sigHandler);
  signal(SIGUSR2, sigHandler);
  signal(SIGINT, sigHandler);

  TCP_server(port_num);
  
  return 0;
}
