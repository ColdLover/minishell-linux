/**************************************************8
 * authour:
 * sid: 3150102193
 * time
 * purpose
 */
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "unistd.h"
#include "pwd.h"
#include <time.h>
#include<sys/types.h>
#include<sys/wait.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "shell.h"
#include "sys/mman.h"


// Syntax Highlighting
#define GREEN         "\033[0;32;32m"
#define BLUE          "\033[0;32;34m"
#define WHITE         "\033[1;37m"
#define RED           "\033[1;31;40m"

#define MAXLINE 80 /* The maximum length command */

typedef struct node *Vnode;
typedef struct setnode *Snode;
typedef struct cmdnode *Cnode;
/*******************
 * 该结构为链表节点，用来存储echo用的变量
 */
struct node
{
    char variable[50];
    char value[50];
    Vnode next;
};
/*******************
* 该结构用来存储set指令后的返回值
*/
struct setnode
{
    char strvalue[1000];
    Snode next;
};
/*******************
* 该结构用来存储 后台指令
*/
struct cmdnode{
    int state;  // 0 working 1 stop
    int paraflag;
    char cmd[20];
    char para[50];
    int pid;
    Cnode next;
};

char PATH[80];      // current path
char buf[100];      // buffer
char copybuf[100];
char copybuf2[100]; // 这三个buf一开始内容都是相同的，后面的用处不大一样
char destination[100];
char source[100];
char *symbol;       // one part of command，用于读取的中间变量
char *leftcommand;
Cnode clist = NULL;
Vnode vlist = NULL;
int vcount=0;
Snode slist = NULL;

char tmpfilepath[100];      // 过程中比如重定向，管道，我需要一个临时文件来存储
char initPath[100];         // 记录c文件的初始路径
int fgAvoidRepeated = 0;    // 标记，用于避免命令提示符重复


void setpath(char* defaultpath) // set the path: defaultpath
{
    chdir(defaultpath);
}
/***********************************
 * hell 的环境变量应该包含shell=<pathname>/myshell，其中<pathname>/myshell 是可执行程序shell 的完整路径
 */
void Add_Environment()
{
    char * path = getenv("PATH");
    char tmp[80];
    getcwd(tmp,sizeof(tmp));    // get current
    //puts(tmp);
    //  printf("%s\n",path);
    //  printf("%s\n",tmp);
    strcat(path,":");
    strcat(path,tmp);
    //   printf("%s\n",path);
    setenv("PATH",path,1);
    char * t = getenv("PATH");
    //   printf("%s\n",t);
}

/**************************************
 * 读取指令
 * @return
 */
int redcommand()				/*读取用户输入*/
{
    fgets(buf,90,stdin);
    leftcommand = strtok(buf,"\n");
    strcpy(copybuf,buf);
    strcpy(copybuf2,buf);
    symbol = strtok(leftcommand," "); //处理字符串，删除最后的换行符
}


/********************************
 *
 */
void init(){
    Add_Environment(); // shell 的环境变量应该包含shell=<pathname>/myshell，其中<pathname>/myshell 是可执行程序shell 的完整路径
/*设置命令提示符*/

    fflush(stdout);

    //chdir("/home/gesq/shtest");  //////////////////////////////// MUST!!!! MODIFY LAST

    char buffer[100];
    getcwd(buffer, sizeof(buffer));
    strcpy(initPath,buffer);
    strcpy(tmpfilepath,buffer);
    strcat(tmpfilepath,"/ShellSetTmpFile");
    //puts(tmpfilepath);
    //getchar();

/*设置默认的搜索路径*/
    setpath("/usr/bin");        // set initial path
    strcpy(PATH,"/usr/bin");    // store path information in the $PATH

}

/********************************************
 * 该函数内均为内部指令，用if隔开
 * @return
 */
int is_internal_command() {

    int result;
    // 退出
    if (strcmp(symbol, "quit") == 0 || strcmp(symbol, "exit") == 0) {
        exit(0);
        return 0;
    }
    // 显示当前路径
    if (strcmp(symbol, "pwd") == 0) {
        puts(PATH);
        return 0;
    }

    // 显示时间
    if (strcmp(symbol, "time") == 0) {
        time_t rawtime;
        struct tm *timeinfo;
        time(&rawtime);
        timeinfo = localtime(&rawtime);
        printf("%s", asctime(timeinfo));
        return 0;
    }

    //echo <comment>  ——在屏幕上显示<comment>并换行（多个空格和制表符可能被缩减为一个空格）。

    if (strcmp(symbol, "p") == 0) {
        Vnode p = vlist;
        while (p != NULL) {
            printf("%s = %s\n", p->variable, p->value);
            p = p->next;
        }
        return 0;
    }
/********************************************8
 * echo 显示信息
 * 1. echo + comment
 * 2. echo $variable
 * 3. echo $num
 */
    if (strcmp(symbol, "echo") == 0) {
        char *t = strtok(NULL, " ");
        if (t == NULL)
            return 0;
//探测是否是查询set返回值的命令
        // 如果是，搜索链表slist，我把set的返回值切割后存在这个链表当中
        if(*t == '$' && (*(t+1) == '1' || *(t+1) == '2'||*(t+1) == '3'||*(t+1) == '3'||*(t+1) == '4' || *(t+1) == '5' || *(t+1) == '6' || *(t+1) == '7' || *(t+1) == '8' || *(t+1) == '9'))
        {
            t++;
            int num=0;
            while(*t != '\0')
            {
                num = 10 * num + *t - '0';
                t++;
            }
            int count=0;
            Snode p = slist;
            while(p != NULL)
            {
                count++;
                if(count == num)
                {
                    puts(p->strvalue);
                    return 0;
                }
                p = p->next;
            }
            if(num > count)
            {
                putchar('\n');
                return 0;
            }


            return 0;
        }

    // 查询 $变量的情况
        if (*t == '$' && *(t + 1) != ' ') {
            char *variable = t + 1;

            //printf("%s",variable);
            Vnode p = vlist;
            int flag = 0;
            while (p) {

                // puts(p->variable);
                if (strcmp(p->variable, variable ) == 0) {
                    puts(p->value);
                    flag = 1;
                    break;
                }
                p = p->next;
            }
            if (flag == 0)
                putchar('\n');
            return 0;
        }


        while (t) {
            printf("%s ", t);
            //puts("1");
            t = strtok(NULL, " ");
        }
        putchar('\n');
        return 0;
    }

/*************************************************
 * ls 非本次作业要求的内部指令，我用exec调用了外部的ls指令
 */
    if(strcmp(symbol,"ls") == 0)
    {
        char *p = copybuf2;

        while(*p != '-' && *p != '\0')
            p++;

        char cmd[10]="ls";
        char para[100];

        //puts(copybuf2);
        //printf("%s,,%d",copybuf2,strlen(copybuf));

        //puts(copybuf2);

        if(strcmp(copybuf2,"ls") != 0) {
            strcpy(para, p);
            //puts("hellp");
        }

        // puts(para);

        int result = fork();
        if (result < 0) {
            printf("fail to fork in ls\n");
            exit(1);
        }
        else if(result == 0)
        {
            if(strcmp(copybuf2,"ls") == 0)
                execlp("ls","ls",NULL);
            else
                execlp("ls","ls",para,NULL);
            exit(0);
        }
        else{
            waitpid(result,NULL,0);
        }
        return  0;
    }



    // Keyfunction: cd . Parameter: address; .. return last dir;
    if (strcmp(symbol, "cd") == 0) {
        char *t;
        char tpath[80];
        int flag;
        t = strtok(NULL, " ");
        //printf("%d\n",t[0]);

        if(strcmp("cd",copybuf) == 0)
        {
            chdir("/usr/bin");
            strcpy(PATH,"/usr/bin");
            return 0;
        }

        if (t[0] == '/' && strlen(t) > 1) {
            strcpy(tpath, t);
            flag = chdir(tpath);
            //puts(tpath);
            if (flag == 0) {
                strcpy(PATH, tpath);
            } else {
                printf("Fail to enter %s\n", tpath);
            }
        } else if (strcmp("~", t) == 0)      // return default dir address
        {
            setpath("/usr/bin");
            strcpy(PATH, "/usr/bin");
            return 0;
        } else if (strcmp(t, "..") == 0) {
            char *lastpath;
            char *p = PATH;
            char *last;
            int cnum = 0;
            int tnum = 0;
            char tmp[80];
            while (*p != '\0') {
                if (*p == '/') {
                    last = p;
                    cnum += tnum;
                    tnum = 0;
                }
                tnum++;
                p++;
            }
            strncpy(tmp, PATH, cnum);
            tmp[cnum] = '\0';
            int flag;
            flag = chdir(tmp);
            if (flag == 0) {
                strcpy(PATH, tmp);
            } else {
                puts(tmp);
                puts("fail to exec cd ..");
            }
            return 0;
        } else if (strcmp("/", t) == 0) // return root dir
        {
            flag = chdir("/");
            if (flag == 0) {
                strcpy(PATH, "/");
            } else {
                printf("fail\n");
            }
        } else {   // cd set address
            strcpy(tpath, PATH);
            if (strcmp(PATH, "/") != 0)
                strcat(tpath, "/");
            strcat(tpath, t);

            flag = chdir(tpath);
            if (flag == 0) {
                strcpy(PATH, tpath);
            } else {
                printf("Fail to enter %s\n", tpath);
            }
            return 0;
        }
        return  0;
    }

/**************************************************
 * 取消变量绑定
 */
    if (strcmp(symbol, "unset") == 0) {
        char *t;
        char tmp[80];
        Vnode p = vlist;
        Vnode last = p;
        t = strtok(NULL, " ");
        if (t[0] == '$') {                  // 遍历链表，查找是否有此变量
            strcpy(tmp, t + 1);
            while (p) {
                if (strcmp(tmp, p->variable) == 0) {
                    if (p == vlist && p->next == NULL) {
                        free(p);
                        vlist = NULL;
                    } else if (p == vlist && p->next != NULL) {
                        vlist = vlist->next;
                        free(p);
                    } else {
                        last->next = p->next;
                        free(p);
                    }
                }
                last = p;
                p = p->next;
            }
        }
        return 0;
    }
/***************************************************
 * dir 显示当前目录
 * dir + 路径 显示路径下目录
 */
    // show files in a dictionary. you can input address as a parameter
    if (strcmp(symbol, "dir") == 0) {
        DIR *dir;
        struct dirent *ptr;
        char *t;
        char tpath[80];
        int result;
        int status;


        t = strtok(NULL, " ");


        if (t == NULL) {
            dir = opendir(PATH);
        } else {
            dir = opendir(t);
        }


        int count = 0;
        while ((ptr = readdir(dir)) != NULL) {
            printf("%s   ", ptr->d_name);
            count++;
            if (count % 8 == 0)
                putchar('\n');
        }
        putchar('\n');
        closedir(dir);
        return 0;
    }
    // show environment variable

    if (strcmp("environ", symbol) == 0) {
        char *pathvar = getenv("PATH");
        printf("%s\n", pathvar);
        return 0;
    }
/****************************************
 *  修改权限掩码，初始默认权限是066
 */
    if(strcmp("umask",symbol) == 0){
        char *para = strtok(NULL," ");
        if(para == NULL)
        {
            puts("please input : umask para");
            return 1;
        }
        int m = 0;
        int len = strlen(para);
        for(int i=0;i<len; i++)
        {
            int t = *(para+i) - '0';
            if( t < 0 || t >= 8){
                puts("Range : 000 ~ 777");
                return 1;
            }
            m = 8*m + t;
        }
        umask(m);
        puts("系统一开始默认掩码是066,更新后，我们建个文件看看，找文件名为ABCDEF的，看它权限");
        if(creat("ABCDEFG",S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH|O_TRUNC)<0){
            perror("creat2");
            return -1;
        }
        int result = fork();
        if(result == 0)
        {
            execlp("ls","ls","-l",NULL);
            exit(0);
        }
        waitpid(result,NULL,0);
        remove("ABCDEFG");
        return 0;

    }
/***********************************************
 * test A -para B，比较后返回结果
 */
    if(strcmp("test",symbol) == 0)
    {
        puts("我的shell中没有可以很好应用test的地方，所以我直接返回结果");
        char *left = strtok(NULL," ");
        char *algo_symbol = strtok(NULL," ");
        char *right = strtok(NULL," ");

        if(left == NULL || algo_symbol == NULL || right == NULL)
        {
            puts("请输入较为标准的格式 eg： test A -eq B");
            return 1;
        }

        if(strcmp(algo_symbol,"-eq") == 0)
        {
            if(strcmp(left,right) == 0)
                puts("True");
            else
                puts("False");
        }
        else if(strcmp(algo_symbol,"-ge") == 0)
        {
            if(strcmp(left,right) >= 0)
                puts("True");
            else
                puts("False");
        }
        else if(strcmp(algo_symbol,"-gt") == 0)
        {
            if(strcmp(left,right) > 0)
                puts("True");
            else
                puts("False");
        }
        else if(strcmp(algo_symbol,"-le") == 0)
        {
            if(strcmp(left,right) <= 0)
                puts("True");
            else
                puts("False");
        }
        else if(strcmp(algo_symbol,"-lt") == 0)
        {
            if(strcmp(left,right) < 0)
                puts("True");
            else
                puts("False");
        }
        else if(strcmp(algo_symbol,"-ne") == 0)
        {
            if(strcmp(left,right) != 0)
                puts("True");
            else
                puts("False");
        }
        else {
            puts("该shell的test只支持 -lt,-le,-ne,-gt,-ge,-eq");
        }
        return 0;
    }

    /*****************************************
     * exec 调用另一个进程来替换当前进程
     */
    if(strcmp("exec",symbol) == 0)
    {
        char *cmd = strtok(NULL," ");
        if(cmd == NULL)                 // 单纯的exec无效
            return 0;
        char *para = strtok(NULL," ");   // 没有参数的情况
        if(para == NULL){
            execlp(cmd,cmd,NULL);
            return 0;
        }
        char *p = strstr(copybuf2,para);    // 有参数的情况
        execlp(cmd,cmd,p,NULL);
        return 0;
    }

/**********************************************8
 *  function: set $(cmd) in shell
 *  redirect first to a temporary file and excute cmd
 *  read the output from the file to construct a list and delete the file
 *  set不能在home这些目录下使用，因为权限的问题，我需要创建一个临时文件
 */
    if(strcmp("set",symbol) == 0)
    {
        CleanList();

        //puts("hellp");

        char incmd[100];
        char tmp[100];
        char *start;
        int cstart = 0;
        int cend = 0;
        char *p=copybuf;
        int step0 = 0;
        int step_1 = 0;
        int step2 = 0;
        int flag = 0;
        int lbracket = 0, rbracket = 0;

        // puts(copybuf);
        for(int i=0;i<100;i++)          // 这里都是一些规范括号的检查，但是不能穷尽所有的错误输入
        {
            if(copybuf[i] == '\0')
                break;
            if(step2 == 1)
            {
                if(copybuf[i] == '(')
                {
                    lbracket++;
                    start = &copybuf[i+1];
                    cstart = i+1;
                }
                else if( copybuf[i] == ')')
                {
                    rbracket++;
                    cend = i-1;
                }
            }
            else if(step_1 == 1)
            {
                if(copybuf[i] == '$') {
                    step2 = 1;
                }
                else if(copybuf[i] == ' ')
                    ;
                else{
                    puts("Error syntex in set");
                    return 1;
                }
            }
                // 把set $（）里的命令提取出来
            else if(i >= 2 && copybuf[i-2] == 's' && copybuf[i-1] == 'e' && copybuf[i] == 't')
            {
                //puts("BBBBBb");
                step_1 = 1;
            }

        }

        if(lbracket != rbracket)
        {
            printf("Not match (),( %d ; ) %d\n",lbracket,rbracket);
            return 1;
        }

        int length = cend - cstart + 1;

        char tcopy[100];
        memset(tcopy,0,sizeof(tcopy));
        strncpy(tcopy,start,length);

        strcpy(source,tcopy);
        char tpath[100] ;
        strcpy(tpath,PATH);
        if(strcmp(PATH,"/") != 0)
            strcat(tpath,"/");


        strcat(tpath,"settmphello");

        strcpy(copybuf,tcopy);
        strcpy(copybuf2,tcopy);

        //puts(tcopy);
        //puts(tpath);

        int result;     // 分出一个进程取执行命令，关闭标准输出，将输出定向到一个临时文件
        result = fork();
        if(result < 0)
        {
            puts("fail to fork in set");
        }
        else if(result == 0)
        {
            symbol=strtok(tcopy," ");

            int fd = open(tmpfilepath,O_WRONLY | O_CREAT |O_TRUNC ,0644);
            //int fd = open("/home/settmp",O_WRONLY | O_CREAT |O_TRUNC ,0644);
            close(1);   // 关闭标准输出
            dup(fd);
            close(fd);

            is_internal_command();

            exit(0);
        }
        else
        {
            int *status;

            waitpid(result,status,0);
        }

        //puts("hello");

        ReadforList(tmpfilepath); // 从文件中读取，存到链表中

        remove(tmpfilepath);    // 删除临时文件

        return 0;
    }

    if(strcmp("continue",symbol) == 0 )
    {
        puts("我的shell中没有很合适适合返回continue的地方");
        return 0;
    }
    /***************************************8
     * shift 左移参数，实际上是改变链表头，然后释放节点。
     */
    if(strcmp("shift",symbol) == 0 )
    {
        Snode  t;
        char *num = strtok(NULL," ");
        int shiftnum;
        if(num != NULL){
        shiftnum = atoi(num);
       // printf("%d\n",shiftnum);
        }
        else
            shiftnum = 1;
        for(int i=0;i<shiftnum;i++){
            if(slist == NULL)
                return 0;
            else if(slist->next == NULL)
            {
                t = slist;
                slist = NULL;
                free(t);
            }
            else
            {
                t = slist;
                slist = slist->next;
                free(t);
            }
        }
        return  0;
    }
/*****************************
 *  清屏函数
 */
    if(strcmp("clr",symbol) == 0 ) {
        printf("\033[H\033[J");
        return 0;
    }
/**********************************
 *
 * 把后台指令移到前台来执行，思路是读取数组获得pid，然后在主进程使用waitpid函数
 */
    if(strcmp("fg",symbol) == 0 ){
        symbol = strtok(NULL," ");
        int id = atoi(symbol);
        int pid;
        int count = 0;
        Cnode p = clist;
        while(p != NULL){
            if(count == id){
                pid = p->pid;
                //printf("pid  %d\n",p->pid);
                break;
            }
            count++;
            //printf("%d\n",p->pid);
            p = p->next;
        }
        if(p == NULL) {
            puts("无此任务");
            return 1;
        }
        fgAvoidRepeated = 1;
        waitpid(pid,NULL,0);
        return 0;
    }
    /*************************************
     * 显示工作状态，读取链表
     */
    if(strcmp("jobs",symbol) == 0 ){
        printf("ID     PID        situation   command\n");
        int total = 0;
        Cnode p = clist;
        while(p != NULL){
            printf("%d      %d     ",total,p->pid);
            if(p->state == 0)
                printf("工作中       ");
            else
                printf("已完成       ");
            printf("%s ",p->cmd);
            if(p->paraflag == 1)
                printf("%s",p->para);
            putchar('\n');
            p = p->next;
            total++;
        }
        return 0;
    }

    // help + commandname : get instructions of this command
    if(strcmp("help",symbol) == 0 )
    {
        char *t;
        t = strtok(NULL," ");
        if(strcmp("cd",t) == 0)
        {
            printf("help");
        }
        else if(strcmp("bg",t) == 0)
        {
            printf("help");
        }
        else if(strcmp("continue",t) == 0)
        {
            printf("help");
        }
        else if(strcmp("echo",t) == 0)
        {
            printf("help");
        }
        else if(strcmp("exec",t) == 0)
        {
            printf("help");
        }
        else if(strcmp("exit",t) == 0)
        {
            printf("help");
        }
        else if(strcmp("fg",t) == 0)
        {
            printf("help");
        }
        else if(strcmp("jobs",t) == 0)
        {
            printf("help");
        }
        else if(strcmp("pwd",t) == 0)
        {
            printf("help");
        }
        else if(strcmp("set",t) == 0)
        {
            printf("help");
        }
        else if(strcmp("shift",t) == 0)
        {
            printf("help");
        }
        else if(strcmp("test",t) == 0)
        {
            printf("help");
        }
        else if(strcmp("time",t) == 0)
        {
            printf("help");
        }
        else if(strcmp("umask",t) == 0)
        {
            printf("help");
        }
        else if(strcmp("unset",t) == 0)
        {
            printf("help");
        }
        else if(strcmp("time",t) == 0)
        {
            printf("help");
        }
        else if(strcmp("dir",t) == 0)
        {
            printf("help");
        }
        else if(strcmp("environ",t) == 0)
        {
            printf("help");
        }
        else if(strcmp("help",t) == 0)
        {
            printf("help");
        }
        else if(strcmp("quit",t) == 0)
        {
            printf("help");
        }
        else
        {
            printf("No this interior commands\n");
        }
        return 0;
    }

    puts("Not support this command");
    return 0;
}

/**************************
 *  Another set command. Before executing, clean the list for this command
 */
void CleanList()
{
    if(slist == NULL)
    {
        return;
    }
    else
    {
        Snode t = slist;
        Snode last;
        while(t!=NULL)
        {
            last = t;
            t = t->next;
            free(last);
        }
        slist = NULL;
        return;
    }
}

/***********************************
 * set 命令中，关闭标准输出，把输出内容输出到临时文件上。然后要用此函数来读取该文件
 * @param tpath
 */
void ReadforList(char *tpath)
{
    char *pBuf;

    FILE *pFile=fopen(tpath,"r"); //获取文件的指针

    fseek(pFile,0,SEEK_END); //把指针移动到文件的结尾 ，获取文件长度

    int len=ftell(pFile); //获取文件长度

    pBuf = (char*)malloc((len+1)*sizeof(char));

    rewind(pFile); //把指针移动到文件开头 因为我们一开始把指针移动到结尾，如果不移动回来 会出错
    fread(pBuf,1,len,pFile); //读文件
    pBuf[len]=0; //把读到的文件最后一位 写为0 要不然系统会一直寻找到0后才结束
    fclose(pFile); // 关闭文件

    /*读取临时文件的内容，开辟一个链表来存储文件的内容*/

    //puts(pbu);

    //用链表把读进来的东西存起来
    char *word = strtok(pBuf," ");
    int i = 0;
    Snode listend;
    while( word != NULL)
    {
        if(slist == NULL)
        {
            slist = (Snode)malloc((sizeof(struct setnode)));
            slist->next = NULL;
            strcpy(slist->strvalue,word);
            listend = slist;
            //puts(slist->strvalue);
        }
        else
        {
            Snode tmp = (Snode)malloc((sizeof(struct setnode)));
            tmp->next = NULL;
            strcpy(tmp->strvalue,word);
            listend->next = tmp;
            listend=tmp;
            //puts(tmp->strvalue);
        }
        word = strtok(NULL," ");
        i++;
    }
    Snode t = slist;

    free(pBuf); // free memory
}

/***********************************88
 * 检查是否为题目要去的内部指令
 * @param cmd
 * @return
 */

int check_intercommand(char * cmd)
{
    if( strcmp(cmd,"bg") == 0 || strcmp(cmd,"ls") == 0 || strcmp(cmd,"cd") == 0 || strcmp(cmd,"continue") == 0 ||  strcmp(cmd,"echo") == 0 || strcmp(cmd,"exec") == 0 || strcmp(cmd,"cd") == 0 || strcmp(cmd,"exit") == 0 || strcmp(cmd,"fg") == 0|| strcmp(cmd,"jobs") == 0 || strcmp(cmd,"pwd") == 0 || strcmp(cmd,"set") == 0 || strcmp(cmd,"shift") == 0 || strcmp(cmd,"test") == 0 || strcmp(cmd,"time") == 0 || strcmp(cmd,"umask") == 0 || strcmp(cmd,"unset") == 0 || strcmp(cmd,"clr")  == 0 || strcmp(cmd,"environ") == 0 || strcmp(cmd,"dir") == 0 || strcmp(cmd,"help") == 0 || strcmp(cmd,"quit") == 0)
        return 0;
    else
    {
        return 1;
    }
}

/***********************************
 * 检查输入指令中有无重定向符号
 * @return
 */
int check_redirect(){
    char *t;
    char tchar[100];
    char *flag;
    //printf("%s\n",copybuf);
    if( strstr(copybuf,">") != NULL || strstr(copybuf,">>") != NULL)
    {

        if(strstr(copybuf,">>") != NULL)
        {
            flag = t = strstr(copybuf,">>");
            strcpy(tchar,t);
            t = strtok(tchar, " ");
            t = strtok(NULL," ");
        }
        if(strstr(copybuf,">") != NULL)
        {
            flag = t = strstr(copybuf,">");
            strcpy(tchar,t);
            t = strtok(tchar, " ");
            t = strtok(NULL," ");
        }
        char *p = copybuf;
        int count=0;
        while(p++ != flag)
            count++;
        strncpy(source,copybuf,count);
        strcpy(destination,t);
        symbol = strtok(source," ");
        //puts(source);
        //puts(destination);
        return 0;
    }
    else if( strstr(copybuf,"<") != NULL || strstr(copybuf,"<<") != NULL )
    {
        if(strstr(copybuf,"<<") != NULL)
        {
            flag = t = strstr(copybuf,"<<");
            strcpy(tchar,t);
            t = strtok(tchar, " ");
            t = strtok(NULL," ");
        }
        else if(strstr(copybuf,"<") != NULL )
        {
            flag = t = strstr(copybuf,"<");
            strcpy(tchar,t);
            t = strtok(tchar, " ");
            t = strtok(NULL," ");
        }
        char *p = copybuf;
        int count=0;
        while(p++ != flag)
            count++;
        strncpy(destination,copybuf,count);
        strcpy(source,t);
        //puts(source);
        //puts(destination);
        return 0;
    }
    else
        return 1;
}

/*************************************
 * 执行重定向指令，有四种类型的重定向操作
 * @return
 */
int exec_redirect(){
    //printf(BLUE"hello\n");
    int result;
    int *status;

    // 用子进程，改变标准输出出口，来进行输出
    if(strstr(copybuf,">>") != NULL)
    {
        result = fork();
        if(result < 0)
        {
            puts("Fail to fork in redirect!");
        }
        else if( result == 0)
        {
            char nowpath[100];
            strcpy(nowpath,PATH);
            strcat(nowpath,"/");
            strcat(nowpath,destination);

            int fd = open(nowpath,O_WRONLY | O_CREAT |O_APPEND ,0644);
            close(1);
            dup(fd);
            close(fd);

            is_internal_command();

            exit(0);
        }
        else{
            waitpid(result,status,0);
            //puts("B");
        }
    }
    else if(strstr(copybuf,">") != NULL)
    {
        result = fork();
        if(result < 0)
        {
            puts("Fail to fork in redirect!");
        }
        else if( result == 0)
        {
            char nowpath[100];
            strcpy(nowpath,PATH);
            strcat(nowpath,"/");
            strcat(nowpath,destination);

            //puts(symbol);

            int fd = open(nowpath,O_WRONLY | O_CREAT |O_TRUNC ,0644);
            close(1);
            dup(fd);
            close(fd);

            is_internal_command();

            //puts(nowpath);
            exit(0);
        }
    }
    else if(strstr(copybuf,"<<") != NULL || strstr(copybuf,"<") != NULL)
        puts("要求说只要实现stdin或者stdout一种就行\n");

    //is_internal_command();
    return 0;
}

/*****************************************
 * 检查是否有管道标记
 * @return
 */
int check_ispipe()
{
    //puts(copybuf2);
    char *t = strstr(copybuf2,"|");
    if(t == NULL) {
        //puts("A");
        return 1;
    }
    else {
        //puts("B");
        return 0;
    }
}
/***************************************
 * 管道的思路就是把管道指令切割成左右两个部分，左边指令的执行结果，加到右边指令的后面重新执行依次
 * @return
 */

int is_pipe(){
    char leftcmd[100];
    char rightcmd[100];
    memset(leftcmd,0,100);
    memset(rightcmd,0,100);     // 清空数组
    char *p = copybuf2;
    int count = 0;
    while(*p != '|')
    {
        count++;
        p++;
    }
    strncpy(leftcmd,copybuf2,count);        // 处理左右指令
    p++;
    count++;
    while(*p == ' ' && count<=strlen(copybuf2)) {
        p++;
        count++;
    }
    strcpy(rightcmd,p);                     // 这一片都是把管道指令切成左右两个部分
    strcpy(buf,leftcmd);
    strcpy(copybuf2,leftcmd);
    symbol = strtok(buf," ");

    int result;
    int pipe_fd[2];
    char buf_r[1000],buf_w[1000];
    memset(buf_w,0,sizeof(buf_w));
    memset(buf_r,0,sizeof(buf_r));
    if(pipe(pipe_fd)<0)                 // 子进程来处理重定向的问题
    {
        printf("Fail to Pipe\n");
        return  -1;
    }
    result = fork();
    if(result < 0)
    {
        perror("Fail to build fork in Pipe");
        exit(0);
    }
    else if(result == 0)
    {
        close(pipe_fd[0]);
        int fd = open(tmpfilepath,O_WRONLY | O_CREAT |O_TRUNC ,0644);       //重定向
        close(1);
        dup(fd);
        close(fd);

        //puts(symbol);
        if (check_intercommand(symbol) == 0) {
            is_internal_command();
        }
        else{
            normal_cmd();
        }

        FILE *fp;
        fp=fopen(tmpfilepath,"rb");// localfile文件名
        fseek(fp,0L,SEEK_END); /* 定位到文件末尾 */
        int flen=ftell(fp); /* 得到文件大小 */

        fseek(fp,0L,SEEK_SET); /* 定位到文件开头 */
        fread(buf_w,flen,1,fp); /* 一次性读取全部文件内容 */
        buf_w[flen]=0; /* 字符串结束标志 */
        if( write(pipe_fd[1],buf_w,strlen(buf_w)) == -1 )
        {
            puts("Write fail in fork pipe");
        }

        close(pipe_fd[1]);
        exit(0);
    }
    waitpid(result,NULL,0);
    close(pipe_fd[1]);
    if(read(pipe_fd[0],buf_r,1000) <= 0)  //读取文件失败
    {
        puts("Fail to read in pipe");
        return 1;
    }

    close(pipe_fd[0]);              // 关闭读端口

    int len = strlen(buf_r);
    buf_r[len-1] = '\0';            //重定向写的东西最后一个字符有点问题，要人为修改一下

    strcat(rightcmd," ");
    strcat(rightcmd,buf_r);

    strcpy(buf,rightcmd);
    strcpy(copybuf2,rightcmd);      // 修改指令读取的全局数组，以便重新执行指令
    symbol = strtok(buf," ");

    if (check_intercommand(symbol) == 0) {      // 依次执行内部指令和外部指令
        is_internal_command();
    }
    else{
        normal_cmd();
    }

    return 0;
}


/**********************************
 * 该函数用来调用外部指令，所有原shell的，并且非本shell内部指令的指令，都可通过该函数兼容。
 * 但是兼容的类型必须是 cdm -参数类型的，或者仅仅cmd，其余的不支持
 * @return
 */
int normal_cmd()
{
    int count = 0;
    char cmd[100];
    char para[100];
    char *p = copybuf2;
    while(*p == ' ' && (*p != '\0' || *p != '\n') ) {
        p++;
        count++;
    }

    if(*p == '\0' || *p == '\n')
    {
        return 0;
    }

    int start = count;
    while(*p != ' ' && (*p != '\0' || *p != '\n') && count <= strlen(copybuf2))
    {
        count++;
        p++;
    }

    int length = count - start;
    strncpy(cmd,copybuf2+start,length);

    int flag = 0;
    while(*p != '\n' && *p != '\0'  && count <= strlen(copybuf2) )
    {
        if(*p != ' ') {
            flag = 1;
            break;
        }
        p++;
        count++;
    }
                // 以上均为字符串的切割处理
    //puts(cmd);

    int result = 0;             // 在子进程中调用exec的外部指令
    result = fork();
    if(result < 0)
    {
        puts("fail to fork in normal cmd");
        return 1;
    }
    if(result == 0)
    {
        if(flag == 1) {
            strcpy(para,copybuf2+count);
            //puts(para);

            execlp(cmd,cmd, para, NULL);
        }
        else
            execlp(cmd,cmd,NULL);
        exit(0);
    }
    waitpid(result,NULL,0);
    return  0;
}
/*************************************************
 *  例如 a=9.需要把这个值给存起来，我用了链表来存
 *  先检查有无等号，有等号的处理，没有的先直接返回
 * @return
 */
int check_equal()
{
    char varibale[50];
    memset(varibale,0,50);
    char *next = strstr(leftcommand, "=");
    if (next != NULL) {

        int i;
        for (i = 0; i < 50; i++) {
            if (leftcommand[i] == '=') {
                break;
            }
            varibale[i] = leftcommand[i];
        }
        Vnode p = vlist;            // 先把左右给剥离
        int flag = 0;
        while (p) {                 // 先遍历，如果已经有了此变量，就更新一下，不然加个节点
            if (strcmp(varibale, p->variable) == 0) {
                strcpy(p->value, next + 1);
                flag = 1;
                break;
            }
            p = p->next;
        }
        if (flag == 1)
            return 0;
        p = vlist;
        if (vlist == NULL) {            // 第一个，开辟节点
            vlist = (Vnode) malloc(sizeof(struct node));        // 存到链表里面去
            if (vlist == NULL)
                puts("Fail to allocate memory");
            vlist->next = NULL;

            strcpy(vlist->variable, varibale);
            strcpy(vlist->value, next + 1);
            vcount++;
        } else {
            vcount++;
            while (p->next != NULL)
                p = p->next;
            Vnode newnode = (Vnode) malloc(sizeof(struct node));
            if (newnode == NULL)
                puts("Fail to allocate memory");
            p->next = newnode;
            newnode->next = NULL;
            strcpy(newnode->variable, varibale);
            strcpy(newnode->value, next + 1);
        }

        p=vlist;
        /*while(p!=NULL)
        {
            printf("%s  %s\n",p->variable,p->value);
            p = p->next;
        }*/

        return 0;
    }
    return  1;  // 没等记号，直接返回
}
/***********************************8
 * 检查是否后台运行程序&
 * @return
 */
int check_back()
{
    int len = strlen(copybuf2);
    //printf("%d\n",len);
    if(copybuf2[len-1] == '&')
        return 0;
    else return 1;
}

/************************************************
 * 在后台分出一个子进程来执行程序，用链表在存储信息
 * 用共享内存的机制实现进程间的通信
 */
void back_exec(){
    char newcmd[100];
    for(int i=0;i<strlen(copybuf2);i++){
        if(copybuf2[i] == '&')
            newcmd[i]='\0';
        else
            newcmd[i]=copybuf2[i];
    }
    char* cmd = strtok(newcmd," ");
    char* para = strtok(NULL," ");
    //puts(cmd);
    //puts(para);

    Cnode a,a2;
    int count = 0;

    a = clist;
    if(a == NULL)   // 用共享内存的机制，来更新state
    {
        clist = a2 = (Cnode)mmap(NULL,sizeof(struct cmdnode),PROT_READ|PROT_WRITE,MAP_SHARED|MAP_ANONYMOUS,-1,0);
        clist->state = 0;
        a2->pid = 0;
        strcpy(clist->cmd,cmd); // 存命令
        if(para != NULL){
            clist->paraflag=1;
            strcpy(clist->para,para);
        }
        else{
            clist->paraflag=0;
        }
        count++;
    }
    else{           // 增加节点
        Cnode p = clist;
        a2 = (Cnode)mmap(NULL,sizeof(struct cmdnode),PROT_READ|PROT_WRITE,MAP_SHARED|MAP_ANONYMOUS,-1,0);
        while(p->next != NULL)
        {
            p = p->next;
        }
        p->next = a2;
        count++;
        a2->state = 0;
        strcpy(a2->cmd,cmd);
        a2->pid = 0;
        if(para != NULL){
            a2->paraflag=1;
            strcpy(a2->para,para);      // 存参数
        }
        else{
            a2->paraflag=0;
        }
    }


    int result = fork();        // 共享内存更新state，实现进程通信
    if(result < 0)
    {
        puts("Fail to fork in &");
        return;
    }
    if(result == 0){
        int subfork = fork();
        if(subfork == 0){
            //puts("Sleeping");
            execlp(cmd,cmd,para,NULL);      // 执行外部普通指令
            exit(0);
        }
        waitpid(subfork,NULL,0);
        //puts("ENDsleep");
        printf("已完成%s\n",copybuf);
        printf(GREEN"myshell>"BLUE"%s"WHITE"$ ", PATH); //命令行提示符
        a2->state = 1;
        exit(0);
    }
    a2->pid = result;
    //waitpid(result,NULL,0);

    return;

}

int main(int agrc, char** agrv) {
    char *args[MAXLINE / 2 + 1]; /* command line arguments */
    int should_run = 1; /* flag to determine when to exit program */
    init();             // initial parameter
    int i = 0;
    int result;
    FILE *fp;
    char StrLine[1024];
    //puts(initPath);
    if(agrc == 2)
    {
        if(strcmp(initPath,"/")!=0)     // initPath是c文件最初始的路径
            strcat(initPath,"/");
        strcat(initPath,agrv[1]);       // 做一些修改，使得initPath的路径为输入文件的路径
        if((fp = fopen(initPath,"r")) == NULL) //判断文件是否存在及可读
        {
            printf("不存在该文件，请确保文本文件和myshell.c在同一目录下!\n");
            return 1;
        }
    }
    while (should_run)   // infinite loop
    {
        if(agrc >= 3)   // 用来外部输出命令文件
        {
            puts("最多一个参数");
            return 1;
        }
        else if(agrc == 2 && !feof(fp))
        {
            fgets(buf,100,fp);  //读取一行
            leftcommand = strtok(buf,"\n");
            strcpy(copybuf,buf);
            strcpy(copybuf2,buf);
            symbol = strtok(leftcommand," "); //处理字符串，删除最后的换行符

            if(feof(fp))
                return 0;

            printf(GREEN"myshell>"BLUE"%s"WHITE"$ %s\n", PATH, copybuf); //命令行提示符

        }
        else {
            if(fgAvoidRepeated == 0)            //防止fg回来后一行两个命令提示符
                 printf(GREEN"myshell>"BLUE"%s"WHITE"$ ", PATH); //命令行提示符
            else
                fgAvoidRepeated = 0;
            redcommand();/*读取用户输入*/
        }

        if( check_back() == 0 ) // 检查是否要求后台执行
        {
            back_exec();
            continue;
        }

        if (strcmp(buf, "\n") == 0) {       // 命令为空
            //printf("myshell>");
            continue;
        }

        if(check_equal() == 0 ) // 检查是否有等号
        {
            continue;
        }

        if (check_redirect() == 0) {    // 检查是否需要重定向
            exec_redirect();        // 执行重定向
            continue;
        }

        if (check_ispipe() == 0)  // 0 means existing |
        {
            is_pipe();  // 执行管道指令
            continue;
        }

        if (check_intercommand(symbol) == 0) {  // 检查是否为内部指令
            is_internal_command();  //执行内部指令
            continue;
        }
        else
        {               // 执行普通的外部指令
            //puts("RIO");
            normal_cmd();
            //puts("ORI");
            continue;
        }


    }
    if(agrc == 2)
        fclose(fp);                     //关闭文件

}

