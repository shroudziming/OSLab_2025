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
void shell_cd();
void set_cmdline();

char input_buffer[INPUT_BUFFER_SIZE];
char args[ARG_NUM][ARG_LEN];
char *arg_ptr[ARG_NUM];
char current_dir[30];
char cmd_line[50];

char * prefix = "> root@UCAS_OS:~";
int arg_count = 0;
int input_buffer_index = 0;
int main(void)
{
    sys_move_cursor(0, SHELL_BEGIN);
    set_cmdline();
    printf("------------------- COMMAND -------------------\n");
    printf(cmd_line);
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
                printf(cmd_line);
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
            printf(cmd_line);
        }else{
            input_buffer[input_buffer_index++] = temp;
        }
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
    else if(strcmp("mkfs", args[0])==0)
        if(strcmp(args[1],"-f")==0)
            sys_mkfs(1);
        else
            sys_mkfs(0);
    else if(strcmp("mkdir",args[0])==0){
        if(sys_mkdir(args[1]))
            printf("\nInfo: mkdir %s failed\n",args[1]);
    }
    else if(strcmp("rmdir",args[0])==0)
        sys_rmdir(args[1]);
    else if(strcmp("ls",args[0])==0){
        int ret;
        if(strcmp(args[1],"-al")==0){
            ret = sys_ls(args[2],1);
        }else{
            ret = sys_ls(args[1],0);
        }
        if(ret){
            printf("\nInfo: ls failed\n");
        }
    }else if(strcmp("statfs",args[0])==0)
        sys_statfs();
    else if(strcmp("cd",args[0])==0)
        shell_cd();
    else if(strcmp("touch",args[0])==0)
        sys_touch(args[1]);
    else if(strcmp("cat",args[0])==0)
        sys_cat(args[1]);
    else if(strcmp("ln",args[0])==0)
        sys_ln(args[1],args[2]);
    else if(strcmp("rm",args[0])==0)
        sys_rm(args[1]);
    else
        shell_unknown();
}
void shell_ps(){
    sys_ps();
}

void shell_exec(int argc){
    int wait = strcmp("&", args[argc]);
    pid_t pid = sys_exec(args[1], argc - (wait ? 0: 1), arg_ptr + 1);
    if(pid == -1){
        printf("\nexec failed\n");
    }else{
        printf("\nInfo: Execute %s success,pid = %d\n",args[1],pid);
        if(wait){
            sys_waitpid(pid);
        }
    }
}

void shell_kill(){
    int pid = atoi(args[1]);
    if(sys_kill(pid) == 0){
        printf("\nInfo: can not find process with pid %d\n",pid);
    }else{
        printf("\nInfo: kill %d success\n",pid);
    }
}

void shell_waitpid(){
    int pid = atoi(args[1]);
    if(sys_waitpid(pid) == 0){
        printf("\nInfo: can not find process with pid %d\n",pid);
    }else{
        printf("\nInfo: excute waitpid success, pid = %d\n",pid);
    }
}

void shell_unknown(){
    printf("\nInfo: Unknown command! %s\n",args[0]);
}


void shell_clear(){
    sys_clear();
    sys_move_cursor(0, SHELL_BEGIN);
    printf("------------------- COMMAND -------------------\n");
}

void shell_cd(){
    if(sys_cd(args[1])){
        printf("\nInfo: change directory to %s failed\n",args[1]);
        return;
    }
    int i=0;
    while(i < strlen(args[1])){
        char name[16];
        int j=0;
        for(j=0;i < strlen(args[1]);j++){
            if(args[1][i] == '/'){
                i++;
                break;
            }
            name[j]=args[1][i++];
        }
        name[j]='\0';
        if(strcmp(name,".")==0)
            continue;
        else if(strcmp(name,"..")==0){
            //go back
            if(strlen(current_dir)==0)
                continue;
            int k;
            for(k=strlen(current_dir)-1;k>=0 && current_dir[k] != '/';k--);
            current_dir[k]='\0';
        }else{
            strcat(current_dir,"/");
            strcat(current_dir,name);
        }
    }
    printf("\nInfo: change directory to %s\n",current_dir);
    set_cmdline();
}

void set_cmdline(){
    strcpy(cmd_line,prefix);
    strcat(cmd_line,current_dir);
    strcat(cmd_line,"$ ");
}