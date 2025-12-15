#include <iostream>
#include <unistd.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <unordered_map>
#include <sstream>
#include <iomanip>
#include <errno.h>
#include <cstring>
#include <cstdint>   


using namespace std;

/* ---------------- BREAKPOINT STRUCT ---------------- */

struct Breakpoint {
    uint64_t addr;
    uint8_t original_byte;
    bool enabled;
};

unordered_map<uint64_t, Breakpoint> breakpoints;

/* ---------------- UTILS ---------------- */

uint64_t parse_addr(const string &s) {
    uint64_t v;
    stringstream ss;
    ss << hex << s;
    ss >> v;
    return v;
}

/* ---------------- BREAKPOINT OPS ---------------- */

bool set_breakpoint(pid_t pid,long addr){

    
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
       bp.original_byte=original;
       bp.enabled=true;
       breakpoints[addr]=bp;

   return true;
 }



bool remove_breakpoint(pid_t pid,long addr){
    if(!breakpoints.count(addr)){
        return false;
    }
    Breakpoint &bp=breakpoints[addr];
       if(!bp.enabled){
        return false;
       }
        
        errno=0;
        long data=ptrace(PTRACE_PEEKDATA,pid,bp.addr,0);
        if(data==-1&& errno!=0){
            return false;
        }
        long new_data1=(data & ~0xFF)| bp.original_byte;
        errno=0;
        long new_data=ptrace(PTRACE_POKEDATA,pid,bp.addr,new_data1);
        if(new_data==-1&&errno!=0){
            return false;
        }
        breakpoints.erase(addr);
        return true;

    
}
void print_reg(user_regs_struct reg1){
    /*
    regs.rip
regs.rsp
regs.rbp
regs.eflags

regs.rax
regs.rbx
regs.rcx
regs.rdx
regs.rsi
regs.rdi

regs.r8
regs.r9
regs.r10
regs.r11
regs.r12
regs.r13
regs.r14
regs.r15

regs.cs
regs.ss
regs.ds
regs.es
regs.fs
regs.gs

    */
   cout<< "RIP : 0x" << hex << reg1.rip << endl;
   cout<< "RSP : 0x" << hex << reg1.rsp << endl;
   cout<< "RBP : 0x" << hex << reg1.rbp << endl;
   cout<< "EFLAGS : 0x" << hex << reg1.eflags << endl;
   cout<< "RAX : 0x" << hex << reg1.rax << endl;
   cout<< "RBX : 0x" << hex << reg1.rbx << endl;
   cout<< "RCX : 0x" << hex << reg1.rcx << endl;
   cout<< "RDX : 0x" << hex << reg1.rdx << endl;
   cout<< "RSI : 0x" << hex << reg1.rsi << endl;
   cout<< "RDI : 0x" << hex << reg1.rdi << endl;
   cout<< "R8 : 0x" << hex << reg1.r8 << endl;
   cout<< "R9 : 0x" << hex << reg1.r9 << endl;
   cout<< "R10 : 0x" << hex << reg1.r10 << endl;
   cout<< "R11 : 0x" << hex << reg1.r11 << endl;
   cout<< "R12 : 0x" << hex << reg1.r12 << endl;
   cout<< "R13 : 0x" << hex << reg1.r13 << endl;
   cout<< "R14 : 0x" << hex << reg1.r14 << endl;
   cout<< "R15 : 0x" << hex << reg1.r15 << endl;
   cout<< "CS : 0x" << hex << reg1.cs << endl;
   cout<< "SS : 0x" << hex << reg1.ss << endl;
   cout<< "DS : 0x" << hex << reg1.ds << endl;
   cout<< "ES : 0x" << hex << reg1.es << endl;
   cout<< "FS : 0x" << hex << reg1.fs << endl;
   cout<< "GS : 0x" << hex << reg1.gs << endl;

}
/* -------------------------Breakpoint handler------------------------------------------------------*/
void Breakpoint_handler(pid_t pid,long address){
    Breakpoint &bp=breakpoints[address];
    user_regs_struct reg;
    ptrace(PTRACE_GETREGS,pid,0,&reg);
    
    long hit_address=reg.rip-1;
    cout<<"BreakPoint Hit 0x"<<hit_address;
     if(bp.enabled){
        long data_get=ptrace(PTRACE_PEEKDATA,pid,hit_address,0);
        long original_data=data_get&~0xFF|bp.original_byte;
        ptrace(PTRACE_POKEDATA,pid,hit_address,original_data);
        bp.enabled=false;
        }
        reg.rip=hit_address;
        ptrace(PTRACE_SETREGS,pid,0,&reg);
        ptrace(PTRACE_SINGLESTEP,pid,0,0);
      //  waitpid(pid,NULL,0);
         
            // reinsert(pid, hit_addr);
            if(!bp.enabled){
                long data_get=ptrace(PTRACE_PEEKDATA,pid,hit_address,0);
                long original_data=data_get&~0xFF|0xCC;
                ptrace(PTRACE_POKEDATA,pid,hit_address,original_data);
                 bp.enabled=true;
                }

}
/*--------------- DEBUGGER LOOP ---------------- */

void parent_call(pid_t pid) {
    int status;
   

    while (true) {
        waitpid(pid, &status, 0);

        if (WIFEXITED(status)) {
            cout << "[child exited]" << endl;
            break;
        }

        if (!WIFSTOPPED(status))
            continue;

        int sig = WSTOPSIG(status);
                  


        if (sig == SIGTRAP) {
            user_regs_struct regs;
            ptrace(PTRACE_GETREGS, pid, 0, &regs);

            long hit_addr = regs.rip - 1;

            if (breakpoints.count(hit_addr)) {
                cout << "BREAKPOINT HIT";
                 user_regs_struct reg1;
                 if(ptrace(PTRACE_GETREGS,pid,0,&reg1)==0)
                     print_reg(reg1);
                    Breakpoint_handler(pid,hit_addr);
                 }
        }

        while (true) {
           
            cout << "Command "<<endl;
            string s;
            getline(cin, s);

            if (s == "c") {
                ptrace(PTRACE_CONT, pid, 0, 0);
                break;
            }
            else if (s== "s") {
                ptrace(PTRACE_SINGLESTEP, pid, 0, 0);
                break;
            }
            else if (s=="b") {
                 cout<<"Adress"<<endl;
                 string add="";
                 getline(cin,add);
                 if(set_breakpoint(pid, parse_addr(add))){
                    cout<<"Breakpoint set"<<endl;
                 }
                 else{
                    cout<<"Breakpoint is not set this address"<<endl;
                 }
            }
            else if (s=="rb") {
                cout<<"Adress "<<endl;
                string s1="";
                getline(cin,s1);
                long addr = parse_addr(s1);
                if (remove_breakpoint(pid, addr))
                    cout << "Breakpoint Removed\n";
                else
                    cout << "Breakpoint not removed\n";
            }
            else if(s=="q"){
                ptrace(PTRACE_KILL,pid,0,0);
             
                return;

            }
        
            else {
                cout << "Wrong command \n";
            }

        }
    }
}

/* ---------------- MAIN ---------------- */

int main() {
    pid_t pid = fork();

    if (pid == 0) {
        setbuf(stdout, NULL);   
        ptrace(PTRACE_TRACEME, 0, 0, 0);
        execl("./test", "test", NULL);
        perror("exec");
    }
    else {
        parent_call(pid);
    }
    return 0;
}
