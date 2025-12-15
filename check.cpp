#include<iostream>
#include<unistd.h>//fork
#include<sys/ptrace.h>//ptrace
#include<sys/wait.h>//waitpid
#include<sys/user.h>//Linuax provide structure contain all register which are used in ptrace  that is user_regs_struct
#include<climits>
#include<unordered_map>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <sstream>   // for stringstream


using namespace std;
/*---------------------------------------------------------------------------*/
bool first_stop=true;
struct Breakpoint{
    long addr=0;
    long original=0;
    bool enabled=false;
};
/*-------------------------------------------------------------------------------------------------------*/
long parse_addr(const string &s) {
    long v;
    stringstream ss;
    ss << hex << s;
    ss >> v;
    return v;
}
unordered_map<long,Breakpoint> breakp;
/*-----------------------------------------------------------------------------------------------------------------------------------*/
 /*bool set_breakpoint(pid_t pid,long addr){
    cout<<"aslam";
    errno=0;
    long base_add= ptrace(PTRACE_PEEKDATA,pid,addr,0);
    if(base_add==-1&& errno!=0){
     perror("error");
     return false ;
    }
    long original= base_add& 0xFF;
    errno=0;
    long new_data=(base_add&~0xFF)|0XCC;
    int p=ptrace(PTRACE_POKEDATA,pid,addr,new_data);
    if(p==-1&&errno!=0){
        perror("Not update value");
        return -false;
    }
     Breakpoint bp;
       bp.addr=addr;
       bp.original=original;
       breakp[addr]=bp;
   return true;
 }*/
void set_breakpoint(pid_t pid, long addr) {
    long data = ptrace(PTRACE_PEEKDATA, pid, addr, 0);
    if (data == -1 && errno) {
        perror("PTRACE_PEEKDATA");
        return;
    }

    Breakpoint bp;
    bp.addr = addr;
    bp.original = data & 0xFF;
    bp.enabled = true;

    long int3 = (data & ~0xFF) | 0xCC;
    ptrace(PTRACE_POKEDATA, pid, addr, int3);

    breakp[addr] = bp;
    cout << "[BREAKPOINT SET] at 0x" << hex << addr << endl;
}
void reinsert_breakpoint(pid_t pid, long addr) {
    auto &bp = breakp[addr];
    if (bp.enabled) return;

    long data = ptrace(PTRACE_PEEKDATA, pid, addr, 0);
    long int3 = (data & ~0xFF) | 0xCC;
    ptrace(PTRACE_POKEDATA, pid, addr, int3);

    bp.enabled = true;
}
void restore_breakpoint(pid_t pid, long addr) {
    auto &bp = breakp[addr];
    if (!bp.enabled) return;

    long data = ptrace(PTRACE_PEEKDATA, pid, addr, 0);
    long restored = (data & ~0xFF) | bp.original;
    ptrace(PTRACE_POKEDATA, pid, addr, restored);

    bp.enabled = false;
}
 /*-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
 bool remove_breakpoint_permanent(pid_t pid,long addr){
    if(!breakp.count(addr)){
        return false;
    }
    else{
        Breakpoint &bp=breakp[addr];
        errno=0;
        long data=ptrace(PTRACE_PEEKDATA,pid,bp.addr,0);
        if(data==-1&& errno!=0){
            return false;
        }
        long new_data1=(data & ~0xFF)| bp.original;
        errno=0;
        long new_data=ptrace(PTRACE_POKEDATA,pid,bp.addr,new_data1);
        if(new_data==-1&&errno!=0){
            return false;
        }
        breakp.erase(addr);
        return true;

    }
}
//*------------------------------------------------------------------------------------------------------------------------------------------------
  void parent_CALL(pid_t pid){
    int status;
          // ✅ exec SIGTRAP
    
    while(true){
        waitpid(pid,&status,0);
        if(WIFEXITED(status)){
            cout<<"EXit "<<endl;
            break;
        }
        if(!WIFSTOPPED(status)){
            continue;
        }
        int sig=WSTOPSIG(status);
       /* if(sig==SIGTRAP){
         
            //check this interuupt from breakpoint 
            user_regs_struct regs;
            //find all register value;
         
            ptrace(PTRACE_GETREGS,pid,0,&regs);
          
            //hit address where break point exit
            long hit_address=regs.rip-1;
            if(breakp.count(hit_address)){
                Breakpoint & bp=breakp[hit_address];
            regs.rip=hit_address;
             
             ptrace(PTRACE_SETREGS,pid,0,&regs);
             
             //place original data on instruction
             long data=ptrace(PTRACE_PEEKDATA,pid,hit_address,0);
             
            
             long original_data=(data&~0xFF)| bp.original;
             
             ptrace(PTRACE_POKEDATA,pid,hit_address,original_data);
            
             
             ptrace(PTRACE_SINGLESTEP,pid,0,0);
            
             waitpid(pid,&status,0);
             // restor breakpoint hose instruction which i have placed 
             if(breakp.count(hit_address)){
             long  intruptdata=(original_data&~0xFF)|0xCC;
             ptrace(PTRACE_POKEDATA,pid,hit_address,intruptdata);}


            }
        }*/
        if (sig == SIGTRAP) {
            user_regs_struct regs;
            ptrace(PTRACE_GETREGS, pid, 0, &regs);

            long hit_addr = regs.rip - 1;

            if (breakp.count(hit_addr)) {
                cout << "[BREAKPOINT HIT] at 0x" << hex << hit_addr << endl;

                restore_breakpoint(pid, hit_addr);

                regs.rip = hit_addr;
                ptrace(PTRACE_SETREGS, pid, 0, &regs);

                ptrace(PTRACE_SINGLESTEP, pid, 0, 0);
                waitpid(pid, &status, 0);

                if (breakp.count(hit_addr)) {
                    reinsert_breakpoint(pid, hit_addr);
                }
            }
        }
        while(true){
            cout<<"Command ";
            string s="";
            getline(cin,s);
            if(s=="c"){
                ptrace(PTRACE_CONT,pid,0,0);
                break;
            }
            else if(s=="s"){
                ptrace(PTRACE_SINGLESTEP,pid,0,0);
                break;
            }
            else if(s=="b"){
                string s1="";
                cout<<"Address to place breakpoint ";
                getline(cin,s1);
               
                long address=parse_addr(s1);
               
                 /* if(set_breakpoint(pid,address)){
                   cout<<"Set breakpoint at address 0x"<<address;
                  }
                  else{
                    cout<<"Breakpoint Not set";
                  }*/
                 set_breakpoint(pid,address);
                 cout<<"aslam";
                break;
            }
            else if(s=="rb"){
                string s1="";
                cout<<"Address to remove breakpoint ";
                getline(cin,s1);
                long address=parse_addr(s1);
                if(remove_breakpoint_permanent(pid,address)){
                    cout<<"cout breakpoint remove 0x"<<address;
                }
                else{
                    cout<<"Breakpoint not possible";
                }
                break;
            }
        }
       
    }
  }
  //*---------------------------------------------------------------------------------------------------------------------------------*
  int main(){
    cout<<"Start"<<endl;
    cout.flush();
    pid_t pid=fork();
    
    if(pid==0){
       long res=ptrace(PTRACE_TRACEME,0,NULL,NULL);
       if(res==-1)
       {
        perror("Error to give control parent");
        exit(1);
       }
        execl("./test", "test", NULL);
       perror("Eroor to execute child proceess");
       exit(1);
        
    }
    else{
        parent_CALL(pid);
    }
}