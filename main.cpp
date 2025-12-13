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

// ---- Simple breakpoint structure ----
struct Breakpoint {
    pid_t pid;              // pid of debuggee
    void* addr;             // address of breakpoint
    long orig_data;         // original machine word at addr
    bool enabled;           // true = set
    Breakpoint* next;
    Breakpoint(pid_t p, void* a, long d)
      : pid(p), addr(a), orig_data(d), enabled(true), next(nullptr) {}
};

// ---- Global head of breakpoint list ----
static Breakpoint* bp_list_head = nullptr;

// ---- Function prototypes to implement ----
pid_t launch_target(const char* program_path, char* const argv[]);
int wait_for_child(pid_t child);
int continue_execution(pid_t child, int sig_to_deliver);
int single_step(pid_t child, int sig_to_deliver);
int insert_breakpoint(pid_t child, void* addr);
int remove_breakpoint(pid_t child, void* addr);
int handle_breakpoint(pid_t child);
int read_mem(pid_t child, void* addr, long* out_word);
int write_mem(pid_t child, void* addr, long word);
int get_regs(pid_t child, struct user_regs_struct* regs);
int set_regs(pid_t child, const struct user_regs_struct* regs);
void print_regs(const struct user_regs_struct* regs);
void list_breakpoints(void);
Breakpoint* find_breakpoint(void* addr);
void add_bp_to_list(Breakpoint* bp);
void remove_bp_from_list(Breakpoint* bp);

// ---- Helper / minimal CLI to demo usage ----
void repl(pid_t child);

// ---- main: parse args and start debugger ----
int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::fprintf(stderr, "Usage: %s <program> [args...]\n", argv[0]);
        return 1;
    }

    char** child_argv = &argv[1];

    pid_t child = launch_target(argv[1], (char* const*)child_argv);
    if (child < 0) {
        std::perror("launch_target");
        return 1;
    }

    if (wait_for_child(child) < 0) {
        std::fprintf(stderr, "Failed waiting for child initial stop\n");
        return 1;
    }

    repl(child);

    return 0;
}

/* ========== Functions to implement ========== */

pid_t launch_target(const char* program_path, char* const argv[]) {
    // TODO: fork -> child does ptrace(PTRACE_TRACEME) -> execve(program_path, argv, environ)
    // parent returns child's pid
    // Return -1 on failure.
    pid_t pid = fork();

    if (child_pid == -1) {
        cout<<"fork error";
        return -1;
    }
    
    if (child_pid == 0) {
        // Child
        if (ptrace(PTRACE_TRACEME, 0, nullptr, nullptr) == -1) {
            cout<<"ptrace PTRACE_TRACEME error";
            exit(1);
        }
        
        // Execute the target program
        execv(program_path, argv);
        
        cout<<"execv returned means it failed";
        exit(1);
    }
    
    // Parent process
    int status;
    if (waitpid(child_pid, &status, 0) == -1) {
        cout<<"waitpid error";
        return -1;
    }
    
    if (WIFSTOPPED(status)) {
        cout << "[Debugger] Target launched (PID: " << child_pid << ")\n";
        cout << "[Debugger] Stopped with signal: " << WSTOPSIG(status) << "\n";
        return child_pid;
    }
    
    cout<< "[Error] Child did not stop as expected\n";
    return -1;
    
}

int wait_for_child(pid_t child) {
    // TODO: waitpid(child, &status, 0) and decode whether child stopped/exited.
    // Return 0 on normal stop, <0 on error/exit.
    
}

int continue_execution(pid_t child, int sig_to_deliver) {
    // TODO: call ptrace(PTRACE_CONT, child, 0, sig_to_deliver).
    // return 0 on success, -1 on failure
    
}

int single_step(pid_t child, int sig_to_deliver) {
    // TODO: ptrace(PTRACE_SINGLESTEP, child, 0, sig_to_deliver)
    
}

int insert_breakpoint(pid_t child, void* addr) {
    // TODO:
    // - read machine long at addr using read_mem
    // - save original word
    // - write new word with 0xCC at first byte using write_mem
    // - create and add Breakpoint to bp_list_head
    // return 0 on success, -1 on failure
    
}

int remove_breakpoint(pid_t child, void* addr) {
    // TODO:
    // - find breakpoint in list
    // - restore original word at addr
    // - remove from list and free
    
}

int handle_breakpoint(pid_t child) {
    // TODO:
    // - get regs
    // - adjust RIP (instruction pointer) to point back at original instruction
    // - restore original byte(s) for the breakpoint
    // - optionally single-step to execute original instruction
    // - reinsert breakpoint if necessary
    // - return 0 on success
    
}

int read_mem(pid_t child, void* addr, long* out_word) {
    // TODO: use ptrace(PTRACE_PEEKDATA, child, addr, NULL)
    
}

int write_mem(pid_t child, void* addr, long word) {
    // TODO: use ptrace(PTRACE_POKEDATA, child, addr, word)
    
}

int get_regs(pid_t child, struct user_regs_struct* regs) {
    // TODO: ptrace(PTRACE_GETREGS, child, 0, regs)
    
}

int set_regs(pid_t child, const struct user_regs_struct* regs) {
    // TODO: ptrace(PTRACE_SETREGS, child, 0, regs)
    
}

void print_regs(const struct user_regs_struct* regs) {
    // TODO: print RIP, RSP, RBP, RAX, RBX, RCX, RDX, RSI, RDI, EFLAGS
    
}

void list_breakpoints() {
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
    
}

/* ========== Simple REPL ========== */

void repl(pid_t child) {
    
}
