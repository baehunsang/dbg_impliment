#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>  
#include <unistd.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <stdint.h>

void usage(){
    puts("hello");
}
void debugger (pid_t pid){
    int wait_status;
	void* bp1 = 0x401273;
	void* bp2 = 0x401290;
	void* bp3 = 0x4012a5; 
	uint8_t int3 = 0xcc;
	uint64_t overwrite_data = 0x206f687700001338;
    unsigned int inst_count=0;
	uint64_t buffer_addr = 0;
	uint64_t overwrite_flag1 = 0x4541427b47414c46;
	uint64_t overwrite_flag2 = 0x4e41535f4e55485f;
	uint64_t overwrite_flag3 = 0x5f554f595f3a5f47;
	uint64_t overwrite_flag4 = 0x007d44454b434148;
    struct user_regs_struct regs;

    wait(&wait_status);

    //b* bp1 && c
	uint64_t inst1 = ptrace(PTRACE_PEEKTEXT, pid, bp1, NULL);
	printf("stored int : %x\n", inst1);
	ptrace(PTRACE_POKETEXT, pid, bp1, int3);
	ptrace(PTRACE_CONT, pid, NULL, NULL);
	wait(&wait_status);

	ptrace(PTRACE_GETREGS, pid, NULL, &regs);
	uint64_t inst = ptrace(PTRACE_PEEKTEXT, pid, regs.rip, NULL);
	printf("RIP: 0x%11x, opcode= %x\n", regs.rip, inst);

	//x $rbp - 0x74
	buffer_addr = regs.rbp - 0x74;
	uint64_t checked_int = ptrace(PTRACE_PEEKDATA, pid, buffer_addr, NULL);
	printf("checked_int : %x\n", checked_int);
		
	//set $rbp - 0x74 = 0x1338
	ptrace(PTRACE_POKEDATA, pid, regs.rbp - 0x74, overwrite_data);
	checked_int = ptrace(PTRACE_PEEKDATA, pid,buffer_addr, NULL);
	printf("checked_int : %x\n", checked_int);

	//c
	regs.rip -= 1;
	ptrace(PTRACE_SETREGS, pid, NULL, &regs);
	printf("%x\n", inst1);
	ptrace(PTRACE_POKETEXT, pid, bp1, inst1);
	inst1=ptrace(PTRACE_PEEKTEXT, pid, regs.rip, NULL);
	printf("recovered inst: %x\n", inst1);

	inst1 = ptrace(PTRACE_PEEKTEXT, pid, bp2, NULL);
	printf("stored int : %x\n", inst1);
	ptrace(PTRACE_POKETEXT, pid, bp2, int3);
	ptrace(PTRACE_CONT, pid, NULL, NULL);
	//ptrace(PTRACE_SINGLESTEP, pid, NULL, NULL);
    wait(&wait_status);

		
	ptrace(PTRACE_GETREGS, pid, NULL, &regs);
	inst = ptrace(PTRACE_PEEKTEXT, pid, regs.rip, NULL);
	printf("RIP: 0x%11x, opcode= %x\n", regs.rip, inst);
		
	printf("RAX : %x\n", regs.rax);
	regs.rax = 0x1;
	ptrace(PTRACE_SETREGS, pid, NULL, &regs);
	ptrace(PTRACE_GETREGS, pid, NULL, &regs);
	printf("RAX: %x\n", regs.rax);

		
   	//c
	regs.rip -= 1;
	ptrace(PTRACE_SETREGS, pid, NULL, &regs);
	printf("%x\n", inst1);
	ptrace(PTRACE_POKETEXT, pid, bp2, inst1);
	inst1=ptrace(PTRACE_PEEKTEXT, pid, regs.rip, NULL);
	printf("recovered inst: %x\n", inst1);


	inst1 = ptrace(PTRACE_PEEKTEXT, pid, bp3, NULL);
	printf("stored int : %x\n", inst1);
	ptrace(PTRACE_POKETEXT, pid, bp3, int3);
	ptrace(PTRACE_CONT, pid, NULL, NULL);
	//ptrace(PTRACE_SINGLESTEP, pid, NULL, NULL);
    wait(&wait_status);

	ptrace(PTRACE_GETREGS, pid, NULL, &regs);
	inst = ptrace(PTRACE_PEEKTEXT, pid, regs.rip, NULL);
	printf("RIP: 0x%11x, opcode= %x\n", regs.rip, inst);
	buffer_addr = regs.rbp - 0x70;
	ptrace(PTRACE_POKEDATA, pid, buffer_addr, overwrite_flag1);
	buffer_addr +=8;
	ptrace(PTRACE_POKEDATA, pid, buffer_addr, overwrite_flag2);
	buffer_addr += 8;
	ptrace(PTRACE_POKEDATA, pid, buffer_addr, overwrite_flag3);
	buffer_addr += 8;
	ptrace(PTRACE_POKEDATA, pid, buffer_addr, overwrite_flag4);


	//c
	regs.rip -= 1;
	ptrace(PTRACE_SETREGS, pid, NULL, &regs);
	printf("%x\n", inst1);
	ptrace(PTRACE_POKETEXT, pid, bp3, inst1);
	inst1=ptrace(PTRACE_PEEKTEXT, pid, regs.rip, NULL);
	printf("recovered inst: %x\n", inst1);
	ptrace(PTRACE_CONT, pid, NULL, NULL);
	wait(&wait_status);
}



int main(int argc, char*argv[]){
    //if (argc != 2){
      //  usage();
        //return -1;
    //}
    pid_t pid = fork();

    if (pid ==0){
        ptrace(PTRACE_TRACEME, 0, NULL,NULL);
        execl(argv[1],argv[1],NULL);
    }
    else{
        debugger(pid);
    }
}
