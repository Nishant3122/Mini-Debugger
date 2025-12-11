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
    if (pid == -1) {
        return -1;
    }
    if (pid == 0) {
        // Child
        if (ptrace(PTRACE_TRACEME, 0, nullptr, nullptr) == -1) {
            std::perror("ptrace TRACEME");
            _exit(1);
        }
        // Exec: use execvp for PATH lookup convenience
        execvp(program_path, argv);
        // If execvp returns, it's an error
        std::perror("execvp");
        _exit(1);
    }
    // Parent: return child's pid
    return pid;
}

int wait_for_child(pid_t child) {
    // TODO: waitpid(child, &status, 0) and decode whether child stopped/exited.
    // Return 0 on normal stop, <0 on error/exit.
    int status = 0;
    pid_t w = waitpid(child, &status, 0);
    if (w == -1) {
        return -1;
    }
    if (WIFEXITED(status)) {
        std::fprintf(stderr, "Child %d exited, status=%d\n", child, WEXITSTATUS(status));
        return -1;
    }
    if (WIFSIGNALED(status)) {
        std::fprintf(stderr, "Child %d killed by signal %d\n", child, WTERMSIG(status));
        return -1;
    }
    if (WIFSTOPPED(status)) {
        int sig = WSTOPSIG(status);
        std::printf("Child %d stopped by signal %d\n", child, sig);
        return 0;
    }
    return -1;
}

int continue_execution(pid_t child, int sig_to_deliver) {
    // TODO: call ptrace(PTRACE_CONT, child, 0, sig_to_deliver).
    // return 0 on success, -1 on failure
    if (ptrace(PTRACE_CONT, child, nullptr, (void*)(intptr_t)sig_to_deliver) == -1) {
        return -1;
    }
    return 0;
}

int single_step(pid_t child, int sig_to_deliver) {
    // TODO: ptrace(PTRACE_SINGLESTEP, child, 0, sig_to_deliver)
    if (ptrace(PTRACE_SINGLESTEP, child, nullptr, (void*)(intptr_t)sig_to_deliver) == -1) {
        return -1;
    }
    return 0;
}

int insert_breakpoint(pid_t child, void* addr) {
    // TODO:
    // - read machine long at addr using read_mem
    // - save original word
    // - write new word with 0xCC at first byte using write_mem
    // - create and add Breakpoint to bp_list_head
    // return 0 on success, -1 on failure
    long word = 0;
    if (read_mem(child, addr, &word) < 0) return -1;
    // If a breakpoint already exists, error
    if (find_breakpoint(addr) != nullptr) return -1;
    unsigned long uw = static_cast<unsigned long>(word);
    unsigned long modified = (uw & ~0xFFUL) | 0xCCUL;
    if (write_mem(child, addr, static_cast<long>(modified)) < 0) return -1;
    Breakpoint* bp = new Breakpoint(child, addr, word);
    add_bp_to_list(bp);
    return 0;
}

int remove_breakpoint(pid_t child, void* addr) {
    // TODO:
    // - find breakpoint in list
    // - restore original word at addr
    // - remove from list and free
    Breakpoint* bp = find_breakpoint(addr);
    if (!bp) return -1;
    if (write_mem(child, addr, bp->orig_data) < 0) return -1;
    remove_bp_from_list(bp);
    delete bp;
    return 0;
}

int handle_breakpoint(pid_t child) {
    // TODO:
    // - get regs
    // - adjust RIP (instruction pointer) to point back at original instruction
    // - restore original byte(s) for the breakpoint
    // - optionally single-step to execute original instruction
    // - reinsert breakpoint if necessary
    // - return 0 on success
    struct user_regs_struct regs{};
    if (get_regs(child, &regs) < 0) return -1;

    // On x86_64, RIP points to addr+1 after INT3
    void* bp_addr = reinterpret_cast<void*>(regs.rip - 1);

    Breakpoint* bp = find_breakpoint(bp_addr);
    if (!bp) {
        // Not our software breakpoint; maybe single-step or other trap
        std::printf("SIGTRAP at 0x%llx but no bp found\n", (unsigned long long)regs.rip);
        return 0;
    }

    // Restore original byte(s)
    if (write_mem(child, bp_addr, bp->orig_data) < 0) return -1;

    // Move RIP back by 1
    regs.rip = reinterpret_cast<uint64_t>(bp_addr);
    if (set_regs(child, &regs) < 0) return -1;

    // Single-step to execute original instruction
    if (single_step(child, 0) < 0) return -1;
    if (wait_for_child(child) < 0) return -1;

    // Re-insert breakpoint
    if (insert_breakpoint(child, bp_addr) < 0) {
        std::fprintf(stderr, "Warning: failed to reinsert breakpoint at %p\n", bp_addr);
        // Not fatal; continue
    }

    return 0;
}

int read_mem(pid_t child, void* addr, long* out_word) {
    // TODO: use ptrace(PTRACE_PEEKDATA, child, addr, NULL)
    errno = 0;
    long data = ptrace(PTRACE_PEEKDATA, child, addr, nullptr);
    if (data == -1 && errno != 0) {
        return -1;
    }
    *out_word = data;
    return 0;
}

int write_mem(pid_t child, void* addr, long word) {
    // TODO: use ptrace(PTRACE_POKEDATA, child, addr, word)
    if (ptrace(PTRACE_POKEDATA, child, addr, (void*)word) == -1) {
        return -1;
    }
    return 0;
}

int get_regs(pid_t child, struct user_regs_struct* regs) {
    // TODO: ptrace(PTRACE_GETREGS, child, 0, regs)
    if (ptrace(PTRACE_GETREGS, child, nullptr, regs) == -1) {
        return -1;
    }
    return 0;
}

int set_regs(pid_t child, const struct user_regs_struct* regs) {
    // TODO: ptrace(PTRACE_SETREGS, child, 0, regs)
    if (ptrace(PTRACE_SETREGS, child, nullptr, (void*)regs) == -1) {
        return -1;
    }
    return 0;
}

void print_regs(const struct user_regs_struct* regs) {
    // TODO: print RIP, RSP, RBP, RAX, RBX, RCX, RDX, RSI, RDI, EFLAGS
    std::printf("RIP = 0x%llx\n", (unsigned long long)regs->rip);
    std::printf("RSP = 0x%llx, RBP = 0x%llx\n", (unsigned long long)regs->rsp, (unsigned long long)regs->rbp);
    std::printf("RAX = 0x%llx, RBX = 0x%llx, RCX = 0x%llx, RDX = 0x%llx\n",
                (unsigned long long)regs->rax, (unsigned long long)regs->rbx,
                (unsigned long long)regs->rcx, (unsigned long long)regs->rdx);
    std::printf("RSI = 0x%llx, RDI = 0x%llx\n", (unsigned long long)regs->rsi, (unsigned long long)regs->rdi);
    std::printf("EFLAGS = 0x%llx\n", (unsigned long long)regs->eflags);
}

void list_breakpoints() {
    // TODO: iterate bp_list_head and print addresses + enabled flag
    Breakpoint* cur = bp_list_head;
    while (cur) {
        std::printf("BP at %p (enabled=%d)\n", cur->addr, cur->enabled ? 1 : 0);
        cur = cur->next;
    }
}

Breakpoint* find_breakpoint(void* addr) {
    // TODO: linear search in bp_list_head
    Breakpoint* cur = bp_list_head;
    while (cur) {
        if (cur->addr == addr) return cur;
        cur = cur->next;
    }
    return nullptr;
}

void add_bp_to_list(Breakpoint* bp) {
    // TODO: push to bp_list_head
    bp->next = bp_list_head;
    bp_list_head = bp;
}

void remove_bp_from_list(Breakpoint* bp) {
    // TODO: remove node from linked list and free it
    if (!bp_list_head) return;
    if (bp_list_head == bp) {
        bp_list_head = bp->next;
        return;
    }
    Breakpoint* prev = bp_list_head;
    while (prev->next && prev->next != bp) prev = prev->next;
    if (prev->next == bp) {
        prev->next = bp->next;
    }
}

/* ========== Simple REPL ========== */

void repl(pid_t child) {
    std::string line;
    while (true) {
        std::cout << "mdbg> " << std::flush;
        if (!std::getline(std::cin, line)) break;
        // trim
        if (line.empty()) continue;

        if (line == "q") {
            ptrace(PTRACE_DETACH, child, nullptr, nullptr);
            std::printf("Detached and exiting\n");
            break;
        } else if (line == "c") {
            if (continue_execution(child, 0) == 0) {
                wait_for_child(child);
            } else {
                std::perror("continue_execution");
            }
        } else if (line == "s") {
            if (single_step(child, 0) == 0) {
                wait_for_child(child);
            } else {
                std::perror("single_step");
            }
        } else if (line == "regs") {
            struct user_regs_struct regs{};
            if (get_regs(child, &regs) == 0) {
                print_regs(&regs);
            } else {
                std::perror("get_regs");
            }
        } else if (line.rfind("b ", 0) == 0) {
            // set breakpoint: b 0xADDRESS
            std::string addr_str = line.substr(2);
            void* addr = reinterpret_cast<void*>(std::stoull(addr_str, nullptr, 0));
            if (insert_breakpoint(child, addr) == 0) {
                std::printf("Breakpoint set at %p\n", addr);
            } else {
                std::perror("insert_breakpoint");
            }
        } else if (line.rfind("rb ", 0) == 0) {
            std::string addr_str = line.substr(3);
            void* addr = reinterpret_cast<void*>(std::stoull(addr_str, nullptr, 0));
            if (remove_breakpoint(child, addr) == 0) {
                std::printf("Breakpoint removed at %p\n", addr);
            } else {
                std::perror("remove_breakpoint");
            }
        } else if (line == "l") {
            list_breakpoints();
        } else {
            std::printf("Commands: b <addr>, rb <addr>, c, s, regs, l, q\n");
        }
    }
}
