/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 *               Author : Han Shukai (email : hanshukai@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *                  The shell acts as a task running in user mode.
 *       The main function is to make system calls through the user's output.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#define SHELL_BEGIN 20
#define INPUT_BUFFER_SIZE 128
#define ARG_NUM 6
#define ARG_LEN 32

void parse_command(char *input);
void handle_shell_command(char *input);
void shell_ps();
void shell_exec(int argc);
void shell_kill();
void shell_waitpid();
void shell_unknown();
void shell_clear();

char input_buffer[INPUT_BUFFER_SIZE];
char args[ARG_NUM][ARG_LEN];
char *arg_ptr[ARG_NUM];
int arg_count = 0;
int input_buffer_index = 0;
int main(void)
{
    sys_move_cursor(0, SHELL_BEGIN);
    printf("------------------- COMMAND -------------------\n");
    printf("> root@UCAS_OS: ");
    int temp;
    for(int i = 0; i < ARG_NUM; i++){
        arg_ptr[i] = args[i];
    }
    while (1)
    {
        // TODO [P3-task1]: call syscall to read UART port
        while((temp = sys_getchar()) == -1);
        if(input_buffer_index > 0 || !(temp == '\b' || temp == 127)){
            sys_putchar(temp);
        }
        // TODO [P3-task1]: parse input
        // note: backspace maybe 8('\b') or 127(delete)
        if(temp == '\b' || temp == 127){
            if(input_buffer_index > 0){
                input_buffer_index--;
            }
            input_buffer[input_buffer_index] = '\0';
        }else if(temp == '\n' || temp == '\r'){
            if(input_buffer_index == 0){
                sys_putchar('\n');
                printf("> root@UCAS_OS: ");
                continue;
            }
            input_buffer[input_buffer_index] = '\0';
            //parse input
            // printf("your command is %s\n",input_buffer);
            parse_command(input_buffer);
            // printf("your args are %s %s %s %s %s %s\n",args[0],args[1],args[2],args[3],args[4],args[5]);
            //handle command
            handle_shell_command(input_buffer);
            //clear input buffer
            input_buffer_index = 0;
            bzero(input_buffer, INPUT_BUFFER_SIZE);
            for(int i = 0;i < ARG_NUM; i++){
                for(int j = 0; j < ARG_LEN; j++){
                    args[i][j] = '\0';
                }
            }
            printf("> root@UCAS_OS: ");
        }else{
            input_buffer[input_buffer_index++] = temp;
        }

        // TODO [P3-task1]: ps, exec, kill, clear    

        /************************************************************/
        /* Do not touch this comment. Reserved for future projects. */
        /************************************************************/    
    }

    return 0;
}

void parse_command(char *input){
    arg_count = 0;
    int input_len = strlen(input);
    int i = 0;

    for(int j = 0;j < ARG_NUM;j++){
        bzero(args[j], ARG_LEN);
        arg_ptr[j] = args[j];
    }
    while(i < input_len && input[i] == ' '){
        i++;
    }
    while (i < input_len && arg_count < ARG_NUM) {
        int j = 0;
        
        //copy an arg
        while (i < input_len && input[i] != ' ' && input[i] != '\n' && input[i] != '\r' && j < ARG_LEN - 1) {
            args[arg_count][j++] = input[i++];
        }
        args[arg_count][j] = '\0';
        
        if (j > 0) {
            arg_count++;
        }
        //space
        while (i < input_len && input[i] == ' ') {
            i++;
        }
    }
}

void handle_shell_command(char *input){
    if(arg_count == 0){
        return;
    }
    if(strcmp("ps", args[0])==0)
        shell_ps();
    else if(strcmp("exec", args[0])==0)
        shell_exec(arg_count-1);
    else if(strcmp("kill", args[0])==0)
        shell_kill();
    else if(strcmp("clear", args[0])==0)
        shell_clear();
    else if(strcmp("waitpid", args[0])==0)
        shell_waitpid();
    // else if(strcmp("taskset", args[0])==0)
    //     shell_taskset();
    else
        shell_unknown();
}
void shell_ps(){
    sys_ps();
}

void shell_exec(int argc){
    int wait = strcmp("&", args[argc])==0;
    pid_t pid = sys_exec(args[1], argc - (wait ? 0: 1), arg_ptr + 1);
    if(pid == 0){
        printf("exec failed\n");
    }else{
        printf("execute %s success,pid = %d\n",args[1],pid);
        if(wait){
            sys_waitpid(pid);
        }
    }
}

void shell_kill(){
    int pid = atoi(args[1]);
    if(sys_kill(pid) == 0){
        printf("can not find process with pid %d\n",pid);
    }else{
        printf("kill %d success\n",pid);
    }
}

void shell_waitpid(){
    int pid = atoi(args[1]);
    if(sys_waitpid(pid) == 0){
        printf("can not find process with pid %d\n",pid);
    }else{
        printf("Excute waitpid success, pid = %d\n",pid);
    }
}

void shell_unknown(){
    printf("\n");
    printf("[Error] Unknown command! %s\n",args[0]);
}


void shell_clear(){
    sys_clear();
    sys_move_cursor(0, SHELL_BEGIN);
    printf("------------------- COMMAND -------------------\n");
}