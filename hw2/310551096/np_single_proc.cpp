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
#include <sys/types.h>
#include<arpa/inet.h>
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
  int sockfd;
  string name;
  string ip;
  int port;
  map<string, string> env;
  vector<pipe_info> pipe_table;
};

struct user_pipe_info {
  int sock_in;
  int sock_out;
  int uid_in;
  int uid_out;

  int fd_in;
  int fd_out;
};

vector<user_info> user(1024);
vector<user_pipe_info> user_pipe;

void user_init(struct user_info &u, int ssockfd){
  user[ssockfd].uid = 0;
  user[ssockfd].sockfd = -1;
  user[ssockfd].name = "(no name)";
  user[ssockfd].env.clear();
  user[ssockfd].pipe_table.clear();
  user[ssockfd].ip = "";
  user[ssockfd].port = -1;
}
void pipe_init(struct pipe_info &p){
  p.fd_in = -1;
  p.fd_out = -1;
  p.cnt = -1;
  p.line = -1;
  p.type = 0;
  p.uid = -1;
}

void user_pipe_init(struct user_pipe_info &u){
  u.sock_in = -1;
  u.sock_out = -1;
  u.uid_in = -1;
  u.uid_out = -1;
  u.fd_in = -1;
  u.fd_out = -1;
}

void childHandler(int signo){
  int status;
  while(waitpid(-1, &status, WNOHANG) > 0){
    // do nothing
  }
}

void broadcast(int ssockfd, string msg) {
  for(vector<user_info>::iterator it = user.begin(); it != user.end(); it++) {
    if(it->uid > 0){
      dup2(it->sockfd, 0);
      dup2(it->sockfd, 1);
      dup2(it->sockfd, 2);
      cout << msg << endl;
    }
  }

  dup2(ssockfd, 0);
  dup2(ssockfd, 1);
  dup2(ssockfd, 2);
  return;
}

void process_fork(vector<vector<string>> cmd_table, bool line_ends_with_pipe, int ssockfd, vector<pipe_info> &pipe_table, string s){
  pid_t pid;
  vector<pid_t> pids;
  struct user_pipe_info u;
  bool new_pipe = true;

  for(int i=0;i<cmd_table.size();i++){
    bool err = false;
    for(int j=0;j<pipe_table.size();j++){
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
            for(vector<user_info>::iterator it = user.begin(); it != user.end(); it++){
                if((it->uid == pipe_table[j].uid)){
                    user_found = true;
                    break;
                }
            }
          
          if(user_found){
            for(vector<user_pipe_info>::iterator it = user_pipe.begin(); it != user_pipe.end(); it++){
                if(it->uid_in==pipe_table[j].uid && it->uid_out==user[ssockfd].uid){
                    pipe_found = true;
                    pipe_table[j].fd_in = it->fd_in;
                    pipe_table[j].fd_out = it->fd_out;
            
                    string msg = "*** " + user[it->sock_out].name + " (#" + to_string(it->uid_out) + ") just received from " + user[it->sock_in].name + " (#" + to_string(it->uid_in) + ") by '" + s + "' ***";
                    broadcast(ssockfd, msg);
                    break;
                }
            }
            if(!pipe_found){
                cerr << "*** Error: the pipe #" << pipe_table[j].uid << "->#" << user[ssockfd].uid << " does not exist yet. ***" << endl;
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
          
          // '>'                     
          if(pipe_table[j].type == 6){
            user_found = false;
            int fd = 0;
            for(int i=0;i<user.size();i++){
              if(user[i].uid == pipe_table[j].uid){
                fd = i;
                user_found = true;
                break;
              }
            }
            if(user_found){
              for(vector<user_pipe_info>::iterator it = user_pipe.begin(); it != user_pipe.end(); it++){
                if(it->uid_in==user[ssockfd].uid && it->uid_out==pipe_table[j].uid){
                  cerr << "*** Error: the pipe #" << it->uid_in << "->#" << it->uid_out << " already exists. ***" << endl;
                  err = true;
                  break; 
                }
              }
              if(!err){
                user_pipe_init(u);
                u.sock_in = ssockfd;
                u.sock_out = fd;
                u.uid_in = user[ssockfd].uid;
                u.uid_out = pipe_table[j].uid;
                u.fd_in = pip[1];
                u.fd_out = pip[0];
                user_pipe.push_back(u);
                
                string msg = "*** " + user[u.sock_in].name + " (#" + to_string(u.uid_in) + ") just piped '" + s + "' to " + user[u.sock_out].name + " (#" + to_string(u.uid_out) + ") ***";
                broadcast(ssockfd, msg);
              }
            }
            else{
              cerr << "*** Error: user #" << pipe_table[j].uid << " does not exist yet. ***" << endl;
              err = true;
            }
          }
        }
        if(err && pipe_table[j].type==6)
          line_ends_with_pipe = false;
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
      bool file_name = false;
      for(int j=0;j<pipe_table.size();j++){
        //cout<<j<<endl;
        // pipe in front of the command i
        if(i && pipe_table[j].cnt==i-1){
          //cout<<pipe_table[j].fd_out<<endl;
          if(pipe_table[j].type == 1 || pipe_table[j].type == 6)
              dup2(pipe_table[j].fd_out, 0);   // pipe -> command's stdin
            else if(pipe_table[j].type == 4)
                file_name = true;   // this command is a file name
        }
        
        // pipe behind the command
        if(pipe_table[j].cnt == i){
          //cout<<pipe_table[j].type<<endl;
          //cout<<pipe_table[j].fd_in<<endl;
          if(pipe_table[j].type == 1){
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
            else
              dup2(pipe_table[j].fd_out, 0);
          }
          else if(pipe_table[j].type == 6){
            if(err){
              int blackhole = open("/dev/null", O_RDWR, 0);
              dup2(blackhole, 1);
            }
            else
              dup2(pipe_table[j].fd_in, 1);
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

    }
    
    //cout<<"test"<<endl;
    //cout<<err<<endl;
    for(vector<pipe_info>::iterator p = pipe_table.begin(); p != pipe_table.end(); ) {
      bool fin = false;
      //cout<<p->line<<endl;
      if(p->cnt == i){
        if(err){
          if(p->type >= 5){
            fin = true;
            pipe_table.erase(p);
          }
        }
        else{
          int fd = -1;
          if(p->type == 5){
            fin = true;
            for(vector<user_pipe_info>::iterator u = user_pipe.begin(); u != user_pipe.end(); u++){
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
      }
      else if(((i && p->cnt==i-1)||(!i && !(p->line))) && p->type<=4){   // already dup
        fin = true;
        //cout<<"test"<<p->line<<i<<endl;
        close(p->fd_in);
        close(p->fd_out);
        //cout<<i<<endl;
        pipe_table.erase(p);    // erase unnecessary pipe information
      }
      
      if(!fin)
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

vector<vector<string>> parse(string cmd_input, bool &line_ends_with_pipe, vector<pipe_info> &pipe_table){
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
    
    // user pipe
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
      
      // "sending" user pipe is in front of "receiving" user pipe
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

  // should wait for the output if the last pipe is "receiving" user pipe
  if(pipe_table.size() && pipe_table.back().type == 5) 
    line_ends_with_pipe = false;

  return cmd_table;
}

int Begin(int ssockfd){
  clearenv();
  setenv("PATH", "bin:.", true);
  map<string, string> env = user[ssockfd].env;
  for(map<string, string>::iterator it = env.begin(); it != env.end(); it++) 
    setenv(it->first.c_str(), it->second.c_str(), true);
    
  string cmd_input;
  vector<vector<string>> cmd_table;
  
  getline(cin, cmd_input, '\n');

  // remove the '\r' and '\n' in the one line command
  while(cmd_input.back() == '\r' || cmd_input.back() == '\n')
    cmd_input.pop_back();
  if(!cmd_input.length())
    return 0;
      
  bool line_ends_with_pipe = true;  // check if ending with number pipe
  cmd_table = parse(cmd_input, line_ends_with_pipe, user[ssockfd].pipe_table);  // parse one-line command
    
  // built-in command
  if(cmd_table[0][0] == "exit")
    return 1;
  else if(cmd_table[0][0] == "printenv"){
    if(cmd_table[0].size() != 2)
      cerr << "Usage: printenv [var]" << endl;
    else
      cout << user[ssockfd].env[cmd_table[0][1]] << endl;
  }
  else if(cmd_table[0][0] == "setenv"){
    if(cmd_table[0].size() != 3)
      cerr << "Usage: setenv [var] [value]" << endl;
    else
      user[ssockfd].env[cmd_table[0][1]] = cmd_table[0][2];
  }
  else if(cmd_table[0][0] == "who"){
    if(cmd_table[0].size() != 1)
      cerr << "Usage: who" << endl;
    else{
      cout << "<ID>\t<nickname>\t<IP:port>\t<indicate me>" << endl;
      for(vector<user_info>::iterator it = user.begin(); it != user.end(); it++){
        string me = "";
        if(ssockfd == it->sockfd)    // itself
            me = "<-me";
        if(it->uid)
          cout << it->uid <<  "\t" << it->name << "\t" << it->ip << ":" << to_string(it->port) << "\t" << me << endl;
      }
    }
  }
  else if(cmd_table[0][0] == "tell"){
    if(cmd_table[0].size() < 3)
      cerr << "Usage: tell [userID] [message]" << endl;
    else{
      if(atoi(cmd_table[0][1].c_str())<1 || atoi(cmd_table[0][1].c_str())>30) {
          cerr << "*** Error: user #" << atoi(cmd_table[0][1].c_str()) << " does not exist yet. ***" << endl;
          return 0;
      }
      
      string msg = "";
      for(int i=2;i<cmd_table[0].size();i++)
        msg += cmd_table[0][i] + " ";    // if the message has space among
      msg.pop_back();
      
      for(vector<user_info>::iterator it = user.begin(); it != user.end(); it++) {
        if(it->uid == atoi(cmd_table[0][1].c_str())) {
          dup2(it->sockfd, 0);
          dup2(it->sockfd, 1);
          dup2(it->sockfd, 2);
          cout << "*** " + user[ssockfd].name + " told you ***: " + msg << endl;

          dup2(ssockfd, 0);
          dup2(ssockfd, 1);
          dup2(ssockfd, 2);
          return 0;
        }
      }
      cout << "*** Error: user #" + cmd_table[0][1] + " does not exist yet. ***" << endl;
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
      
      string msgg = "*** " + user[ssockfd].name + " yelled ***: " + msg;
      broadcast(ssockfd, msgg);
    }
  }
  else if(cmd_table[0][0] == "name"){
    if(cmd_table[0].size() != 2)
      cerr << "Usage: name [New_name]" << endl;
    else{
      for(vector<user_info>::iterator it = user.begin(); it != user.end(); it++) {
        if(it->name == cmd_table[0][1]) {
            cerr << "*** User '" <<  it->name << "' already exists. ***" << endl;
            return 0;
        }
      }
      string msg = "*** User from " + user[ssockfd].ip + ":" + to_string(user[ssockfd].port) + " is named '" + cmd_table[0][1] + "'. ***";
      user[ssockfd].name = cmd_table[0][1];
      broadcast(ssockfd, msg);
    }
  }
  // other commands
  else
    process_fork(cmd_table, line_ends_with_pipe, ssockfd, user[ssockfd].pipe_table, cmd_input);

  return 0;
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
  
  if ((listen(sockfd, 1)) < 0) {
    cerr << "Listen error !" << endl;
    exit(EXIT_FAILURE);
  }
  
  fd_set rfds, afds;
  int nfds = FD_SETSIZE;
  FD_ZERO(&afds);
  FD_SET(sockfd, &afds);

  while(1){
    //cout<<"test"<<endl;
    memcpy(&rfds, &afds, sizeof(rfds));
    if(select(nfds, &rfds, NULL, NULL, (struct timeval *)0) < 0) {
      if(errno != EINTR)   // nothing happened
        cerr << "Select failed !" << endl;
      continue;
    }

    if(FD_ISSET(sockfd, &rfds)) {
      socklen_t len = sizeof(cli);
      ssockfd = accept(sockfd, (struct sockaddr *)&cli, &len);
      //cout<<ssockfd<<endl;
      if(ssockfd < 0) {
        cerr << "Accept error !" << endl;
        continue;
      }
      FD_SET(ssockfd, &afds);
      // open this clientFD
    
      string ip = inet_ntoa(cli.sin_addr);
      int port = ntohs(cli.sin_port), idx = -1;
      
      vector<bool> list(1024, false);
      for(vector<user_info>::iterator u = user.begin(); u != user.end(); u++){
        if(u->uid > 0)
          list[u->uid] = true;
      }
      for(int i=1;i<list.size();i++){
        if(!list[i]){
          idx = i;    // the miniest avaliable index
          break;
        }
      }
      //cout<<idx<<endl;
      if(idx == -1){
        cerr << "Too many clients !" << endl;
        continue;
      }
      
      dup2(ssockfd, 0);
      dup2(ssockfd, 1);
      dup2(ssockfd, 2);

      cout << "****************************************" << endl;
      cout << "** Welcome to the information server. **" << endl;
      cout << "****************************************" << endl; 
      
      user[ssockfd].uid = idx;
      user[ssockfd].sockfd = ssockfd;
      user[ssockfd].name = "(no name)";
      user[ssockfd].ip = ip;
      user[ssockfd].port = port;
      user[ssockfd].env["PATH"] = "bin:.";
      user[ssockfd].pipe_table.clear();
      
      string msg = "*** User '" + user[ssockfd].name + "' entered from " + ip + ":" + to_string(port) + ". ***";
      broadcast(ssockfd, msg);      
      
      cout << "% " << flush;
    }
    
    for(int fd=0;fd<nfds;fd++) {
      if(fd!=sockfd && FD_ISSET(fd, &rfds)) {
        ssockfd = fd;
        dup2(ssockfd, 0);
        dup2(ssockfd, 1);
        dup2(ssockfd, 2);
        
        // indicate that the command is "exit"
        if(Begin(ssockfd) == 1) {    

          // doing the logout procedure
          string msg = "*** User '" + user[ssockfd].name + "' left. ***";
          broadcast(ssockfd, msg);

          for(vector<user_pipe_info>::iterator u = user_pipe.begin(); u != user_pipe.end();){
            //cout<<"test"<<endl;
            if(u->sock_in == ssockfd){
                close(u->fd_in);
                close(u->fd_out);
                user_pipe.erase(u);
            }
            else if(u->sock_out == ssockfd){
              for(vector<pipe_info>::iterator p = user[u->sock_in].pipe_table.begin(); p != user[u->sock_in].pipe_table.end();){
                  if(p->uid == u->uid_out){
                    user[u->sock_in].pipe_table.erase(p);
                    close(u->fd_in);
                    close(u->fd_out);
                    user_pipe.erase(u);
                  }
                  else
                    p++; 
              }          
            }
            else
              u++;
          }
          

          struct user_info u;
          user_init(u, ssockfd);
         
          dup2(sockfd, 0);
          dup2(sockfd, 1);
          dup2(sockfd, 2);
          
          close(ssockfd);
          FD_CLR(ssockfd, &afds);
        }
        else
          cout << "% " << flush;
          // the command is executed successfully, continue printing next "%"
      }
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
  signal(SIGCHLD, childHandler);

  TCP_server(port_num);
  
  return 0;
}
