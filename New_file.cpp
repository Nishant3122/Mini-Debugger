// mini-debugger.cpp


#define _GNU_SOURCE
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/user.h>    // user_regs_struct
#include <sys/stat.h>
#include <sys/personality.h>
#include <sys/syscall.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cinttypes>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <sstream>
#include <iomanip>
#include<unordered_map>
using namespace std;

// ---- Simple breakpoint structure ----
/*struct Breakpoint {
    pid_t pid;              // pid of debuggee
    void* addr;             // address of breakpoint
    long orig_data;         // original machine word at addr
    bool enabled;           // true = set
    Breakpoint* next;
    Breakpoint(pid_t p, void* a, long d)
      : pid(p), addr(a), orig_data(d), enabled(true), next(nullptr) {}
};
*/
struct Breakpoint{
    long addr;
    long original_byte;
    bool enabled;
};
unordered_map<long,Breakpoint> breakpoints;
// ---- Global head of breakpoint list ----


// ---- Function prototypes to implement ----
pid_t launch_target(const char* program_path, char* const argv[]);
int wait_for_child(pid_t child);
int continue_execution(pid_t child, int sig_to_deliver);
int single_step(pid_t child, int sig_to_deliver);
bool insert_breakpoint(pid_t pid, long addr);
bool remove_breakpoint(pid_t pid, long addr);
void handle_breakpoint(pid_t pid,long addr);
int read_mem(pid_t child, void* addr, long* out_word);
int write_mem(pid_t child, void* addr, long word);
int get_regs(pid_t child, struct user_regs_struct* regs);
int set_regs(pid_t child, const struct user_regs_struct* regs);
void print_regs(struct user_regs_struct* reg1);
void list_breakpoints(void);
/*Breakpoint* find_breakpoint(void* addr);
void add_bp_to_list(Breakpoint* bp);
void remove_bp_from_list(Breakpoint* bp);*/

// ---- Helper / minimal CLI to demo usage ----
void repl(pid_t child);

// ---- main: parse args and start debugger ----
int main(int argc, char* argv[]) {
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <program> [args...]\n";
        cerr << "Example: " << argv[0] << " ./test_program arg1 arg2\n";
        return 1;
    }
    
    cout << "=== Mini Debugger ===\n";
    cout << "Target: " << argv[1] << "\n\n";
    
    // Launch the target program under ptrace
    pid_t child = launch_target(argv[1], argv + 1);
    if (child == -1) {
        return 1;
    }
    
    // Enter the debugger REPL
    repl(child);
    
    cout << "\n[Debugger] Session ended.\n";
    return 0;
}

/* ========== Functions to implement ========== */

pid_t launch_target(const char* program_path, char* const argv[]) {
    // TODO: fork -> child does ptrace(PTRACE_TRACEME) -> execve(program_path, argv, environ)
    // parent returns child's pid
    // Return -1 on failure.
    pid_t pid = fork();

    if (pid == -1) {
        perror("fork");
        return -1;
    }
    
    if (pid == 0) {
        // Child
        if (ptrace(PTRACE_TRACEME, 0, nullptr, nullptr) == -1) {
            perror("ptrace PTRACE_TRACEME");
            exit(1);
        }
        
        // Execute the target program
        execv(program_path, argv);
        
        perror("execv returned means it failed");
        exit(1);
    }
    
    // Parent process
    int status;
    if (waitpid(pid, &status, 0) == -1) {
        perror("waitpid");
        return -1;
    }
    
    if (WIFSTOPPED(status)) {
        cout << "[Debugger] Target launched (PID: " << pid << ")\n";
        cout << "[Debugger] Stopped with signal: " << WSTOPSIG(status) << "\n";
        return pid;
    }
    
    cout<< "[Error] Child did not stop as expected\n";
    return -1;
    
}

int wait_for_child(pid_t child) {
    // TODO: waitpid(child, &status, 0) and decode whether child stopped/exited.
    // Return 0 on normal stop, <0 on error/exit.
    int status;
    if (waitpid(child, &status, 0) == -1) {
        perror("waitpid");
        return -1;
    }
    
    if (WIFEXITED(status)) {
        cout << "[Debugger] Target exited with status " << WEXITSTATUS(status) << "\n";
        return 0;
    }
    
    if (WIFSIGNALED(status)) {
        cout << "[Debugger] Target killed by signal " << WTERMSIG(status) << "\n";
        return 0;
    }
    
    if (WIFSTOPPED(status)) {
        int sig = WSTOPSIG(status);
        //cout << "[Debugger] Target stopped by signal " << sig;
        if (sig == SIGTRAP) {
            cout << " (SIGTRAP - breakpoint or single-step)";
        }
        cout << "\n";
        return sig;
    }
    
    return -1;
    
}

int continue_execution(pid_t child, int sig_to_deliver) {
    // TODO: call ptrace(PTRACE_CONT, child, 0, sig_to_deliver).
    // return 0 on success, -1 on failure
    if (ptrace(PTRACE_CONT, child, nullptr, sig_to_deliver) == -1) {
        perror("ptrace PTRACE_CONT");
        return -1;
    }
    return 0;
    
}

int single_step(pid_t child, int sig_to_deliver) {
    // TODO: ptrace(PTRACE_SINGLESTEP, child, 0, sig_to_deliver)
    if (ptrace(PTRACE_SINGLESTEP, child, nullptr, sig_to_deliver) == -1) {
        perror("ptrace PTRACE_SINGLESTEP");
        return -1;
    }
    return 0;
    
}

bool insert_breakpoint(pid_t pid, long addr) {
    // TODO:
    // - read machine long at addr using read_mem
    // - save original word
    // - write new word with 0xCC at first byte using write_mem
    // - create and add Breakpoint to bp_list_head
    // return 0 on success, -1 on failure
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
        return false;
    }
     Breakpoint bp;
    bp.addr=addr;
    bp.original_byte=original;
    bp.enabled=true;
    if (breakpoints.count(addr)) {
        cout << "Breakpoint already exists\n";
        return false;
    }

    breakpoints[addr]=bp;

   return true;
    
}

bool remove_breakpoint(pid_t pid, long addr) {
    // TODO:
    // - find breakpoint in list
    // - restore original word at addr
    // - remove from list and free
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

void handle_breakpoint(pid_t pid,long address) {
    // TODO:
    // - get regs
    // - adjust RIP (instruction pointer) to point back at original instruction
    // - restore original byte(s) for the breakpoint
    // - optionally single-step to execute original instruction
    // - reinsert breakpoint if necessary
    // - return 0 on success
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
        waitpid(pid,NULL,0);
         
            // reinsert(pid, hit_addr);
            if(!bp.enabled){
                long data_get=ptrace(PTRACE_PEEKDATA,pid,hit_address,0);
                long original_data=data_get&~0xFF|0xCC;
                ptrace(PTRACE_POKEDATA,pid,hit_address,original_data);
                 bp.enabled=true;
                }
    
}



int get_regs(pid_t child, struct user_regs_struct* regs) {
    // TODO: ptrace(PTRACE_GETREGS, child, 0, regs)
    if(ptrace(PTRACE_GETREGS, child, 0, regs)==-1)
    {
        perror("ptrace GETREGS");
        return -1;
    }
    return 0;
    
}

int set_regs(pid_t child, const struct user_regs_struct* regs) {
    // TODO: ptrace(PTRACE_SETREGS, child, 0, regs)
    if(ptrace(PTRACE_SETREGS, child, 0, regs)==-1)
    {
        perror("ptrace SETREGS");
        return -1;
    }
    return 0;
    
}

void print_regs( struct user_regs_struct* reg1) {
    // TODO: print RIP, RSP, RBP, RAX, RBX, RCX, RDX, RSI, RDI, EFLAGS
    cout<< "RIP : 0x" << hex << reg1->rip << endl;
    cout<< "RSP : 0x" << hex << reg1->rsp << endl;
    cout<< "RBP : 0x" << hex << reg1->rbp << endl;
    cout<< "EFLAGS : 0x" << hex << reg1->eflags << endl;
    cout<< "RAX : 0x" << hex << reg1->rax << endl;
    cout<< "RBX : 0x" << hex << reg1->rbx << endl;
    cout<< "RCX : 0x" << hex << reg1->rcx << endl;
    cout<< "RDX : 0x" << hex << reg1->rdx << endl;
    cout<< "RSI : 0x" << hex << reg1->rsi << endl;
    cout<< "RDI : 0x" << hex << reg1->rdi << endl;
    cout<< "R8 : 0x" << hex << reg1->r8 << endl;
    cout<< "R9 : 0x" << hex << reg1->r9 << endl;
    cout<< "R10 : 0x" << hex << reg1->r10 << endl;
    cout<< "R11 : 0x" << hex << reg1->r11 << endl;
    cout<< "R12 : 0x" << hex << reg1->r12 << endl;
    cout<< "R13 : 0x" << hex << reg1->r13 << endl;
    cout<< "R14 : 0x" << hex << reg1->r14 << endl;
    cout<< "R15 : 0x" << hex << reg1->r15 << endl;
    cout<< "CS : 0x" << hex << reg1->cs << endl;
    cout<< "SS : 0x" << hex << reg1->ss << endl;
    cout<< "DS : 0x" << hex << reg1->ds << endl;
    cout<< "ES : 0x" << hex << reg1->es << endl;
    cout<< "FS : 0x" << hex << reg1->fs << endl;
    cout<< "GS : 0x" << hex << reg1->gs << endl;
    
}

/*void list_breakpoints() {
    // TODO: iterate bp_list_head and print addresses + enabled flag
    
}

Breakpoint* find_breakpoint(void* addr) {
    // TODO: linear search in bp_list_head
    
}

void add_bp_to_list(Breakpoint* bp) {
    // TODO: push to bp_list_head
    
}

void remove_bp_from_list(Breakpoint* bp) {
    // TODO: remove node from linked list and free it
    
}*/

/* ========== Simple REPL ========== */

void repl(pid_t child) {
    string line;
    bool flag = true;
    
    cout << "\n[Debugger] Entering interactive mode. Type 'h' for help.\n";
    
    while (flag) {
        cout << "mini-dbg ";
        
        if (!getline(cin, line)) {
            break;
        }
        
        istringstream iss(line);
        string command;
        iss >> command;
        
        if (command.empty()) {
            continue;
        }
        
        if (command == "h" || command == "help") {
           // print_help();
        }
        else if (command == "b") {
            // Set breakpoint
            string addr_str;
            if (!(iss >> addr_str)) {
                cout << "Enter input correctly: b <address>\n";
                continue;
            }
            
            //void* addr = (void*)stoull(addr_str, nullptr, 16);//converting string into address
            long addr = stoull(addr_str, nullptr, 16);
              if(insert_breakpoint(child, addr)){
                cout<<"Breat point inserted at 0x"<<addr<<endl;
              }
              else{
                cout<<"Breakpoint not able to insert at 0x"<<addr<<endl;
              }
        }
        else if (command == "rb") {
            // Remove breakpoint
            string addr_str;
            if (!(iss >> addr_str)) {
                cout << "Enter input correctly: rb <address>\n";
                continue;
            }
            
            //void* addr = (void*)stoull(addr_str, nullptr, 16);
            long addr = stoull(addr_str, nullptr, 16);
            if(remove_breakpoint(child, addr))
            {
                cout<<"Breakpoint removed at 0x:"<<addr<<endl;
            }
            else{
                cout<<"Breakpoint not removed(error)"<<endl;
            }
        }
        else if (command == "c") {
            // Continue execution
            cout << "[Debugger] Continuing\n";
            if (continue_execution(child, 0) == -1) {
                continue;
            }
            
            int sig = wait_for_child(child);
            if (sig == 0) {
                // Process exited
                flag = false;
            } else if (sig == SIGTRAP) {
                user_regs_struct regs;
                ptrace(PTRACE_GETREGS, child, 0, &regs);

                long hit_addr = regs.rip - 1;

                if (breakpoints.count(hit_addr)) {
                    cout << "BREAKPOINT HIT";
                    user_regs_struct reg1;
                    if(ptrace(PTRACE_GETREGS,child,0,&reg1)==0){
                        print_regs(&reg1);
                        handle_breakpoint(child,hit_addr);
                    }
                 }
            }
        }
        else if (command == "s") {
            // Single step
            cout << "[Debugger] Single stepping\n";
            if (single_step(child, 0) == -1) {
                continue;
            }
            
            int sig = wait_for_child(child);
            if (sig == 0) {
                flag = false;
            }
            
            // Show current RIP after 1 step
            struct user_regs_struct regs;
            if (get_regs(child, &regs) == 0) {
                cout << "Current RIP is at: 0x" <<hex<< regs.rip <<dec<< "\n";
            }
        }
        else if (command == "regs") {
            // Display registers
            struct user_regs_struct reg1;
            if (get_regs(child, &reg1) == 0) {
                print_regs(&reg1);
            }
        }
        else if (command == "l") {
            // List breakpoints
            //list_breakpoints();
        }
        else if (command == "q" || command == "quit") {
            cout << "[Debugger] Detaching and quitting...\n";
            ptrace(PTRACE_DETACH, child, nullptr, nullptr);
            flag = false;
        }
        else {
            cout << "Unknown command: " << command << "\n";
            cout << "Type 'h' for help.\n";
        }
    }
    
}