#include<iostream>
#include<unistd.h>//fork
#include<sys/ptrace.h>//ptrace
#include<sys/wait.h>//waitpid
#include<sys/user.h>//Linuax provide structure contain all register which are used in ptrace  that is user_regs_struct
using namespace std;
struct Breakpoint{
    long addr=0;
    long original=0;
    bool set=false;
};
long set_breakpoint(pid_t pid,long addr){
    long base_add= ptrace(PTRACE_PEEKDATA,pid,addr,0);
    if(base_add==-1){
     perror("error");
     exit(1);}
    long original= base_add& 0xFF;
    
    long new_data=(base_add&~0xFF)|0XCC;
    int p=ptrace(PTRACE_POKEDATA,pid,addr,new_data);
    if(p==-1){
        perror("Not update value");
        exit(1);
    }
   return original;
 }
 void remove_breakpoint(pid_t pid,long original_data,long addr){
    struct user_regs_struct reg;
    ptrace(PTRACE_GETREGS,pid,0,&reg);
    long rip=reg.rip;
    rip=rip-1;
    long data=ptrace(PTRACE_PEEKDATA,pid,rip,0);
    if(data==-1)
     {
        perror("Error");
        exit(1);
     }
     long new_data=(data&~0xFF)|original_data;
     int p=ptrace(PTRACE_POKEDATA,pid,rip,new_data);
     if(p==-1){
        perror("Not update value");
        exit(1);
     }
 }
//Parent_waitid
void parent_wait(pid_t pid){
    Breakpoint bp;
    while(true){
    
    int status;
    waitpid(pid,&status,0);
    if(WIFEXITED(status)){
        cout<<"Exit";
        break;
    }
    if(WIFSTOPPED(status)){//child stopped or not
          
          int sig=WSTOPSIG(status);
          if(sig==SIGTRAP){

          }
          else{
            string command;
            getline(cin,command);
            if(command=="c"){
               ptrace(PTRACE_CONT,pid,0,0);
            }
            else if(command=="s"){
                ptrace(PTRACE_SINGLESTEP,pid,0,0);
            }
            else if(command=="b"){
                 string add_r="";
                 if(add_r.empty()){
                     cout<<"addrss not valid";
                     continue;
                 }
                 cin>>add_r;
                long addr= (long)strtoul(add_r.c_str(),NULL,16);//this convert string to integer rom hexa to decimal llong long  strtoul is c function o convert c++ we use .c-str()

                bp.addr=addr;
                set_breakpoint(pid,addr);
                bp.set=true;
                
            }
            else if(command=="rb"){
                 if(bp.set==true){
                      remove_breakpoint(pid,bp.original,bp.addr);
                      bp.set=false;
                 }
                 else{
                    cout<<"No Break point to remove";
                 }
            }
       
        }
    } 
    }
}
 
 
int main(){
    char *args[]={(char*)"./testprog",NULL};
    pid_t pid=fork();
    
    if(pid==0){
       long res=ptrace(PTRACE_TRACEME,0,NULL,NULL);
       if(res==-1)
       {
        perror("Error to give control parent");
        exit(1);
       }
       execvp(args[0],args);
       perror("Eroor to execute child proceess");
       exit(1);
        
    }
    else{
        int status;
        waitpid(pid,&status,0);
        parent_wait(pid);
    }

}