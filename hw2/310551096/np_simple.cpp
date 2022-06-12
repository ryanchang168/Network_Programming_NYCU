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
using namespace std;

struct pipe_info{
	int fd_in;   // command -> pipe
	int fd_out;  // pipe -> command
	int cnt;    // behind which command
	int type;   // 1:normal pipe  2:number pipe  3:number pipe with error   4:file output direction
	int line;   // remaining line for piping (only for number pipe)
};

vector<pipe_info> pipe_table;

void pipe_init(struct pipe_info &p){
	p.fd_in = -1;
	p.fd_out = -1;
	p.cnt = -1;
	p.line = -1;
  p.type = 0;
}

void childHandler(int signo){
	int status;
	while(waitpid(-1, &status, WNOHANG) > 0){
		// do nothing
	}
}

void process_fork(vector<vector<string>> cmd_table, bool line_ends_with_pipeN){
  signal(SIGCHLD, childHandler);
  	
	pid_t pid;
	vector<pid_t> pids;

	for(int i=0;i<cmd_table.size();i++){
		for(int j=0;j<pipe_table.size();j++){
			// pipe for command i
			if(pipe_table[j].cnt == i){			 	  
				bool new_pipe = true;
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
				if(new_pipe){
					// create a new pipe
					int pip[2];
					int tmp = pipe(pip);
					
					// the order is important !!!!!
					pipe_table[j].fd_in = pip[1];  // for processes to read
					pipe_table[j].fd_out = pip[0];  // for processes to write
				}
				break;
			}
		}
		
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
				// pipe in front of the command i
				if(i && pipe_table[j].cnt==i-1){
        	if(pipe_table[j].type == 1)
            	dup2(pipe_table[j].fd_out, 0);   // pipe -> command's stdin
            else if(pipe_table[j].type == 4)
                file_name = true;   // this command is a file name
         }
				
				// pipe behind the command
				if(pipe_table[j].cnt == i){
					switch(pipe_table[j].type){
						case 1:
							dup2(pipe_table[j].fd_in, 1);  // command's stdout -> pipe
							break;
						case 2:
							dup2(pipe_table[j].fd_in, 1);  // command's stdout -> pipe
							break;
						case 3:{
							dup2(pipe_table[j].fd_in, 1);  // command's stdout -> pipe
							dup2(pipe_table[j].fd_in, 2);  // command's stderr -> pipe
						       }
							break;
						case 4:{
							int fd = open((cmd_table[i+1][0]).c_str(), O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
							dup2(fd, 1);   // command's stdout -> file
						       }
					}
				}
				
				// number pipe's line remaining is 0
				// pipe into this line's first command
				if(!i && (pipe_table[j].type==2||pipe_table[j].type==3) && !pipe_table[j].line)
					dup2(pipe_table[j].fd_out, 0);
			}
      //cout<<i<<endl;
			// close the fd in child process
			for(vector<pipe_info>::iterator p = pipe_table.begin(); p != pipe_table.end(); p++){
				close(p->fd_in);
				close(p->fd_out);
			}
			
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
          int err = execvp(f, arg);
          if(err == -1){
              cerr << "Unknown command: [" << cmd_table[i][0].c_str() << "]." << endl;
              exit(EXIT_FAILURE);
          }
          exit(EXIT_SUCCESS);
      }
		}
		
		for(vector<pipe_info>::iterator p = pipe_table.begin(); p != pipe_table.end(); ) {
			if((i && p->cnt==i-1) || (!i && !(p->line))){   // already dup
				close(p->fd_in);
				close(p->fd_out);
				pipe_table.erase(p);    // erase unnecessary pipe information
			}
			else
				p++;
		}
	}
	
	// keep the order if the output is to be generated
	if(!line_ends_with_pipeN){
		for(pid_t p : pids){
			int status;
			waitpid(p, &status, 0);
      		pids.erase(pids.begin());
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

vector<vector<string>> parse(string cmd_input, bool &line_ends_with_pipeN){
	stringstream cmd_ss(cmd_input); // processing "getline"
	vector<string> tokens;
	vector<string> cmd;
	string token;
	vector<vector<string>> cmd_table;
	struct pipe_info p;
	int cnt = 0;

	while(getline(cmd_ss, token, ' '))
	       tokens.push_back(token);
	
	for(string t : tokens){
		pipe_init(p);
		
		// normal pipe
		if(t == "|"){
			cmd_table.push_back(cmd);
            		cmd.clear();
			p.type = 1;
			p.cnt = cnt;
			pipe_table.push_back(p);
		}
		
		// number pipe
		else if(t[0]=='|' && isdigit(t[1])){
			cmd_table.push_back(cmd);
            		cmd.clear();
			p.type = 2;
			p.cnt = cnt;
			p.line = string_to_num(t);
			pipe_table.push_back(p);
		}
		
		// number pipe with stderr
		else if(t[0]=='!' && isdigit(t[1])){
			cmd_table.push_back(cmd);
            		cmd.clear();
			p.type = 3;
			p.cnt = cnt;
			p.line = string_to_num(t);
			pipe_table.push_back(p);
		}
		
		// output file redirection
		else if(t == ">"){
			cmd_table.push_back(cmd);
            		cmd.clear();
			p.type = 4;
			p.cnt = cnt;
			pipe_table.push_back(p);
		}
		
		// other commands or arguments
		else{
			cmd.push_back(t);
			cnt--;   // don't need to add the cnt
		}
		cnt++;
	}

	// not a number pipe or a number pipe with stderr
	if(!((tokens[tokens.size()-1].length()>=2) && (tokens[tokens.size()-1][0]=='|'||tokens[tokens.size()-1][0]=='!') && isdigit(tokens[tokens.size()-1][1]))){
		cmd_table.push_back(cmd);
		cmd.clear();
		line_ends_with_pipeN = false;
	}
	return cmd_table;
}

void Begin(){
	while(1){
		cout << "% " << flush; 	// print propmt
		string cmd_input;
		vector<vector<string>> cmd_table;
		
		getline(cin, cmd_input);  // read one line input
		if(cin.eof())
			exit(EXIT_SUCCESS);
    
		if(!cmd_input.length())
			continue;
		// if the stdin is typed, there would have '\r' in the end of line
    else if(cmd_input.back() == '\r')
      cmd_input.pop_back();
      
		bool line_ends_with_pipeN = true;  // check if ending with number pipe
		cmd_table = parse(cmd_input, line_ends_with_pipeN);  // parse one-line command
		
		// built-in command
		if(cmd_table[0][0] == "exit")
			exit(EXIT_SUCCESS);
		else if(cmd_table[0][0] == "printenv"){
			if(cmd_table[0].size() != 2)
				cerr << "Usage: printenv [var]" << endl;
			else{
				char *env_name = getenv(cmd_table[0][1].c_str());
				if(env_name != NULL)
					cout << env_name << endl;
			}
		}
		else if(cmd_table[0][0] == "setenv"){
			if(cmd_table[0].size() != 3)
        cerr << "Usage: setenv [var] [value]" << endl;
			else
				setenv(cmd_table[0][1].c_str(), cmd_table[0][2].c_str(), true);
		}
		// other commands
		else
			process_fork(cmd_table, line_ends_with_pipeN);
	}
  return;
}

void TCP_server(int p){
	// master socket
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if(sockfd < 0){
    cerr << "Socket create error !" << endl;
    exit(EXIT_FAILURE);
  }

  int flag = 1;
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(int));
  // set flag, but I don't know the purpose indeed
  
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
  
  signal(SIGCHLD, childHandler);
  
  while(1){
    socklen_t len = sizeof(cli);

    // slave socket
    int ssockfd = accept(sockfd, (struct sockaddr*)&cli, &len);
    if(ssockfd < 0){
      cerr << "Accept error !" << endl;
      exit(EXIT_FAILURE);
    }
    
    pid_t pid = fork();
        if(pid < 0) {
            cerr << "Fork error !" << endl;
            sleep(1000);
            continue;
        }
        else if(pid == 0) {  // child
            close(sockfd);

           	// receiving command and print the result, in the specific client
            dup2(ssockfd, 0);
            dup2(ssockfd, 1);
            dup2(ssockfd, 2);
            close(ssockfd);
            Begin();
        }
        else
            close(ssockfd);
  }
    return;
}

  

int main(int argc, char **argv) {
  clearenv();
 	setenv("PATH", "bin:.", true);  
   
  if(argc != 2){
    cerr << "Usage : ./np_simple [port]" << endl;
    exit(EXIT_FAILURE);
  }
  
  int port_num = atoi(argv[1]);
  TCP_server(port_num);
  
  return 0;
}