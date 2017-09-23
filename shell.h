//
// Created by gesq on 17-7-24.
//

#ifndef UNTITLED_SHELL_H
#define UNTITLED_SHELL_H
int check_redirect();//检查是否为重定向明码
int exec_redirect();    // 执行重定向命令
int is_internal_command();  // 检查是否为内部命令
void ClearList();
void ReadforList(char *buf);
void CleanList();
int normal_cmd();
void setpath(char* defaultpath); // set the path: defaultpath
void Add_Environment(); //设置环境变量
int redcommand(); // read command
void init(); //一些初始化操作
int is_internal_command(); //执行内部指令
void CleanList();   // 清空链表，slist，重新set的时候原链表清空
void ReadforList(char *tpath);  // set先是执行指令重定向到一个临时文件，然后需要读回来
int check_intercommand(char * cmd); // 检查是否为内部指令
int check_redirect(); // 检查是否为重定向指令
int exec_redirect();    // 执行重定向
int check_ispipe();     // 检查是否为管道命令
int is_pipe();      // 执行管道命令
int normal_cmd();   // 执行普通外部指令void back_exec(){
int check_equal();  // 检查输入是否有等号
int check_back();   // 检查是否需要后台执行
void back_exec();   // 执行后台指令

#endif //UNTITLED_SHELL_H
