#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <pthread.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <error.h>
#include <mysql/mysql.h>
#include <ctype.h>
#include <termios.h>
#include <unistd.h>
#include "list.h"


#define LOGIN               1
#define REGISTER            2
#define DOWNLINE            3
#define ONLINE              4
#define FRIEND_ADD_APPLY    5   
#define FRIEND_ADD_RESP     6
#define CHAT_ONE			7
#define FRIEND_STATU		8
#define GROUP_CREATE		9
#define	GROUP_JOIN			10
#define	GROUP_QUIT			11
#define CHAT_GROUP			12
#define FILE_REQUEST		13
#define FILE_APPLY_RESP		14
#define FILE_SEND			15
#define FILE_FINISH			16
#define FRIEND_DEL			17
#define HISTORY_READ		18
#define G_M_L				19
#define GROUP_DIS			20
#define EXIT                -1


#define ECHOFLAGS (ECHO | ECHOE | ECHOK | ECHONL)
#define MAX_CHAR 20
#define MAXLEN 510
typedef struct friendinfor{
		int statu;
		int mes_num;
		char username[20];
}FRIEND;

typedef struct onlineuser{
		char username[20];
		int friend_num;
		int group_num;
		char group[100][20];
		FRIEND friends[100];
}C_USER;

typedef struct packdata{
		int type;
		char send_name[20];
		char recv_name[20];
		int send_fd;
		int recv_fd;
		char words[MAXLEN];
}PACK_DATA;
typedef struct package{
		PACK_DATA data;
		struct package *prev;
		struct package *next;
}PACK;


//声明
int login_menu(void);
int login(void);
void registe(void);
int get_choice(char *choice_t);
void my_err(const char * err_string,int line);
void friend_list(void);
void server_news_list(void);
void print_main_menu(void);
int main_menu(void);
void friend_add_resp(void);
void friend_add_apply(void);
void recv_pack(PACK*type,PACK*recv_pack_p);
void *recv_pack_thread(void *arg);
void send_pack(int type,char *send_name,char *recv_name,char *mes);
void* chatstart(void);
void chat_to_one(void);
void get_status(void);
void change_statu(PACK *pack_t);
void* chatstart_group(void);
void chat_to_group(void);
int group_list(void);
void group_create(void);
void group_join(void);
void group_quit(void);
void file_request(void);
void file_apply_deal(void);
void getpasswd(char passwd[]);
int set_disp_mode(int fd,int option);
void *pthread_send_file(void);
void *pthread_recv_file(PACK*recv_pack_p);
void file_apply_deal(void);
void file_request(void);
int get_file_size(char *file_name);
int my_recv(int fd,void*words,int len);
int my_send(int fd,void*words,int len);
void friend_del(void);
void history_read(void);
void group_member_list(void);
void group_dis(void);






//全局变量
PACK *f_recv_pack_apply = NULL;
PACK *f_recv_pack_resp = NULL;
PACK *all_recv_news = NULL;
PACK *all_recv_group_news = NULL;
PACK *all_file_pack = NULL;
PACK *all_file_apply = NULL;
char server_news[100][20];
int server_news_num;
int file_apply_num;
int SUM;
int FD;


int flag_group_create;
int flag_group_join;
int flag_group_quit;
int file_resp_flag;
int FILE_SIZE;
char file_recv_username[20];
char file_path[50];

C_USER my_infor;
int sock_fd;
char *IP = "127.0.0.1";
short PORT = 1234;
typedef struct sockaddr SA;
pthread_mutex_t  mutex;
pthread_mutex_t  mutex_recv_file;
pthread_t server_tid;
pthread_t file_send_tid;
pthread_t file_recv_tid;
int EX ;//退出标志
char chatuser[20];//正在聊天的用户名
char chatgroup[20];//正在聊天的群组名
char FILENAME[50];


int main(void)
{
	List_Init(f_recv_pack_apply,PACK);
	List_Init(f_recv_pack_resp,PACK);
	List_Init(all_recv_news,PACK);
	List_Init(all_recv_group_news,PACK);
	List_Init(all_file_apply,PACK);
	List_Init(all_file_pack,PACK);
	printf("客户端启动中...\n");
	sock_fd = socket(AF_INET,SOCK_STREAM,0);
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(PORT);
	addr.sin_addr.s_addr = inet_addr(IP);
	if(connect(sock_fd,(SA*)&addr,sizeof(addr))<0)
	{
		my_err("connect",__LINE__);
	}
	printf("成功连接到服务器\n");
	if(login_menu()==0)
			return 0;
	pthread_create(&server_tid,NULL,recv_pack_thread,NULL);
//	pthread

	main_menu();

}
void group_dis()
{
	printf("输入你想解散的群名:");
	char groupname[20];
	int flag = 0;
	scanf("%s",groupname);
	for(int i=1;i<=my_infor.group_num;i++)
		if(strcmp(my_infor.group[i],groupname)==0)
				flag = 1;
	if(flag == 0)
	{
		printf("未添加该群组!\n");
		return;
	}
	send_pack(GROUP_DIS,my_infor.username,"server",groupname);
	printf("请求已发送,反馈结果发送至消息盒子查看!\n");

}
int login_menu()
{
    char choice_t[100];
    int chioce;
    do
    {
        printf("\n\t************************\n");
        printf("\t1.login in \n");
        printf("\t2.register \n");
        printf("\t0.exit	 \n");
        printf("\t************************\n");
        printf("\t\tchoice：");
        scanf("%s",choice_t);
        chioce = get_choice(choice_t);
        switch(chioce)
        {
            case 1:
                if(login() == 1)
                    return 1;
                break;
            case 2:
                registe();
                break;
            default:
                break;
        }
    }while(chioce!=0);
    return 0;
}

int login(void)
{
    char username [20];
    char password [20];
    printf("please input username:\n");
    scanf("%s",username);
    printf("please input password:\n");
	scanf("%s",password);
//    getpasswd(password);

	PACK send_login_t;
	send_login_t.data.type = LOGIN;
	strcpy(send_login_t.data.send_name,username);
	strcpy(send_login_t.data.words,password);
	strcpy(send_login_t.data.recv_name,"server");
	send_login_t.data.recv_fd = sock_fd;

    if(send(sock_fd,&(send_login_t.data),sizeof(PACK_DATA),0) < 0)
        my_err("words_send",__LINE__);
	if(read(sock_fd,&(send_login_t.data),sizeof(PACK_DATA)) < 0)
		my_err("words_read",__LINE__);

    int flag = send_login_t.data.words[0] - 48;

    if(flag ==  1){
        printf("无该用户!\n");
        return 0;
    }
    if(flag ==  2 ){
        printf("该用户已经上线\n");
        return 0;
    }
    if(flag == 3) {
        printf("password is not right!\n");
        return 0;
    }
    strcpy(my_infor.username,username);
    printf("login success!\n");
    return 1;
}
void registe(void)
{
    int flag = 0;
    flag = REGISTER;
    char username[MAX_CHAR];
    char password[MAX_CHAR];

    printf("please input new username:\n");
    scanf("%s",username);
    printf("please input your password:\n");
    scanf("%s",password);

    PACK send_registe_t;
	send_registe_t.data.type = REGISTER;
	strcpy(send_registe_t.data.send_name,username);
	strcpy(send_registe_t.data.recv_name,"server");
	strcpy(send_registe_t.data.words,password);
	send_registe_t.data.recv_fd = sock_fd;
	if(send(sock_fd,&(send_registe_t.data),sizeof(PACK_DATA),0) < 0)
        my_err("words_send",__LINE__);
	printf("ar\n");
	if(read(sock_fd,&(send_registe_t.data),sizeof(PACK_DATA)) < 0)
		my_err("words_read",__LINE__);
	printf("ne\n");

    if(send_registe_t.data.words[0] - 48)
        printf("register success,please login again!\n");
    else
        printf("username had live!\n");
}
/*
void getpasswd(char passwd[])
{
	char c;
    int i = 0;
	set_disp_mode(STDIN_FILENO,0);

    while ((c=getchar())!='\r')
    {
        if (isprint(c))
        {
            passwd[i++] = c;
            putchar('*');
        }
        else if (i>0 && c=='\b')
        {
            --i;
            putchar('\b');
            putchar(' ');
            putchar('\b');
        }
    }
	set_disp_mode(STDIN_FILENO,1);  
    putchar('\n');
    passwd[i] = '\0';
}

*/

//添加好友功能
void friend_add(void)
{
	char username[20];
	char words[200];
	int flag = 0;
	printf("#######添加好友#######\n");
	printf("请输入好友名:");
	scanf("%s",username);
	while(strcmp(username,"quit")!=0){
	for(int i=1;i<=my_infor.friend_num;i++)
	{
		if(strcmp(my_infor.friends[i].username,username)==0)
		{
			flag = 1;
			break;
		}
	}
	if(flag == 1)
	{	
		printf("该好友已存在!(输入quit退出)\n");
		scanf("%s",username);
	}
		
	else
	{
		printf("请输入验证消息:");
		scanf("%s",words);
		send_pack(FRIEND_ADD_APPLY,my_infor.username,username,words);
		printf("消息已发送\n");
		break;
	}
	}
}

void friend_del(void)
{
	char username[20];
	int flag = 0;
	char words[40];
	printf("#######删除好友#######");
	printf("请输入好友名:");
	scanf("%s",username);
	while(strcmp(username,"quit")!=0){
	for(int i=1;i<=my_infor.friend_num;i++)
	{
		if(strcmp(my_infor.friends[i].username,username)==0)
		{
			flag = 1;
			break;
		}
	}
	if(flag == 0)
	{	
		printf("该好友不存在!请重新输入:(输入quit退出)\n");
		scanf("%s",username);
	}
	else 
	{
		printf("请求已告知\n");
		sprintf(words,"%s had delete you",my_infor.username);
		send_pack(FRIEND_DEL,my_infor.username,username,words);
		printf("删除好友成功!\n");
		break;
	}
	}

}





int get_choice(char *choice_t)
{
    int choice =0;
    for(int i=0;i<strlen(choice_t) ;i++)
        if(choice_t[i]<'0' || choice_t[i]>'9')
            return -1;
    for(int i=0;i<strlen(choice_t);i++)
    {
        int t=1;
        for(int j=1;j<strlen(choice_t)-i;j++)
        {
            t *=10;
        }
        choice += t*(int)(choice_t[i] - 48);
    }
    return choice;
}
void send_pack(int type,char *send_name,char *recv_name,char *mes)
{
    PACK send_pack;
	memset(send_pack.data.words,0,MAXLEN);
    send_pack.data.type = type;
	strcpy(send_pack.data.send_name,send_name);
    strcpy(send_pack.data.recv_name,recv_name);
    memcpy(send_pack.data.words,mes,MAXLEN);
	//printf("<%s>\n",mes+10);
	//for(int i=0;i<10;i++)
	//	printf("最终发包长度2:%d\n",send_pack.data.words[i]);
    if(my_send(sock_fd,(void*)&(send_pack.data),sizeof(PACK_DATA)) < 0){
        my_err("send",__LINE__);
    }
}



void *recv_pack_thread(void *arg)
{
    int i;
    PACK *recv_pack_p = NULL;
    while(1)
    {
		pthread_mutex_lock(&mutex);
		recv_pack_p = (PACK*)malloc(sizeof(PACK));
		memset(recv_pack_p,0,sizeof(PACK));
        if(my_recv(sock_fd,(void*)&(recv_pack_p->data),sizeof(PACK_DATA)) < 0){
            my_err("recv",__LINE__);
        }
		//pthread_mutex_lock(&mutex);
		if(recv_pack_p->data.type == FILE_SEND)
		{
			int t1 ;
			int len ;
			int r;
			len = 0;
			for(int j=0 ;j<10;j++)
			{
				if(recv_pack_p->data.words[j] == -1)
					break;
				t1 = 1;
				for(int l=0;l<j;l++)
					t1*=10;
				len += (int)recv_pack_p->data.words[j]*t1;
			}
			//printf("%s\n",recv_pack_p->data.words+10);
			r = 0;
			while(1)
			{
				r += write(FD,((char*)recv_pack_p->data.words)+10+r,len-r);
				if(r==len)
						break;
			}
			SUM += len;
			printf("---------文件已写入%d-->%d(%d)字节----------\n",FILE_SIZE,len,SUM);
			if(SUM >= FILE_SIZE)
			{
				FILE_SIZE = 0;
				SUM = 0;
				close(FD);
				printf("文件接收完成!!\n");
				send_pack(FILE_FINISH,my_infor.username,"server","ah");
			}
			free(recv_pack_p);
			pthread_mutex_unlock(&mutex);
			continue;
		}
        for(int i=1 ;i<= my_infor.friend_num;i++)
        {
            if(strcmp(my_infor.friends[i].username,recv_pack_p->data.send_name) == 0)
            {
                my_infor.friends[i].mes_num++;
                break;
            }
        }

        switch(recv_pack_p->data.type)
        {
			case FRIEND_ADD_APPLY:
				recv_pack(f_recv_pack_apply,recv_pack_p);
				break;
			case FRIEND_ADD_RESP:
				recv_pack(f_recv_pack_resp,recv_pack_p);
				friend_add_resp();
				break;
			case CHAT_ONE:
				recv_pack(all_recv_news,recv_pack_p);
				break;
			case CHAT_GROUP:
				recv_pack(all_recv_group_news,recv_pack_p);
				break;
			case FRIEND_STATU:
				change_statu(recv_pack_p);
				break;
			case GROUP_CREATE:
				flag_group_create = recv_pack_p->data.words[0];//-48;
				break;
			case GROUP_JOIN:
				flag_group_join = recv_pack_p->data.words[0];//-48;
				break;
			case GROUP_QUIT:
				flag_group_quit = recv_pack_p->data.words[0];//-48;
				break;
			case FILE_REQUEST:
				file_apply_num++;
				recv_pack(all_file_apply,recv_pack_p);
				break;
			case FILE_APPLY_RESP:
				file_resp_flag = recv_pack_p->data.words[0];
				break;
			//case FILE_SEND:
			//	recv_pack(all_file_pack,&recv_pack_t);
			//	pthread_recv_file(recv_pack_p);
			//	printf("%s\n",recv_pack_t.data.words);
			//	break;
			case FRIEND_DEL:
				strcpy(server_news[++server_news_num],recv_pack_p->data.words);
				break;
			case HISTORY_READ:
				printf("%s->%s\n",recv_pack_p->data.send_name,recv_pack_p->data.words);
				break;
			case G_M_L:
				printf("%s\n",recv_pack_p->data.words);
				break;
			case GROUP_DIS:
				strcpy(server_news[++server_news_num],recv_pack_p->data.words);
				break;
			default:
				break;
        }
        pthread_mutex_unlock(&mutex);
		//sleep(1);
    }
}
//将包存到相应链表
void recv_pack(PACK*type,PACK*recv_pack_p)
{
	PACK * pack = (PACK*)malloc(sizeof(PACK));
	memcpy(&(pack->data),&(recv_pack_p->data),sizeof(PACK_DATA));
	//for(int i=0;i<10;i++)
	//printf("收包长度解析:%d\n",recv_pack_p->data.words[i]);
	List_AddTail(type,pack);
}
//处理好友申请(新朋友功能)
void friend_add_apply(void)
{
	PACK * pack = f_recv_pack_apply->next,*tmp;
	char words[20] = "ahhhhhhhh";
		printf("*********好友申请*********\n");
		if(f_recv_pack_apply->next == f_recv_pack_apply)
		{
			printf("没有好友申请\n");
			return;
		}
		while(f_recv_pack_apply->next!=f_recv_pack_apply)
		{
			pack = f_recv_pack_apply->next;
			printf("%s申请加你为好友\n",pack->data.send_name);
			printf("验证消息是:%s\n",pack->data.words);
		
			printf("同意或拒绝:(enter quit to quit)");
			scanf("%s",words);
			if(strcmp(words,"quit")==0)
				break;
			if(words[0]=='y')
			{
				printf("你同意了好友申请\n");
				send_pack(FRIEND_ADD_RESP,my_infor.username,pack->data.send_name,words);
				strcpy(my_infor.friends[++(my_infor.friend_num)].username,pack->data.send_name);
			}
			else
			{
				printf("你拒绝了好友申请\n");
				send_pack(FRIEND_ADD_RESP,my_infor.username,pack->data.send_name,words);
			}
			printf("消息已发送\n");
			
			printf("申请加你为好友%s的包已清理\n",pack->data.send_name);
			List_FreeNode(pack);
		}	

}

//处理加好友请求回应
void friend_add_resp(void)
{

	PACK *pack = NULL;	
	List_ForEach(f_recv_pack_resp,pack)
	{
		printf("处理回应\n");
		if(pack->data.words[0]-48==0)
		{
			sprintf(server_news[++server_news_num],"do not have the user:%s!\n",pack->data.send_name);
			continue;
			
		}
		if(pack->data.words[0]=='y')
		{
			printf("%s已同意了您的请求\n",pack->data.send_name);
			sprintf(server_news[++server_news_num],"user %s agree your request!\n",pack->data.send_name);
			strcpy(my_infor.friends[++(my_infor.friend_num)].username,pack->data.send_name);
		}
		else
		{
			sprintf(server_news[++server_news_num],"user %s refused your request!\n",pack->data.send_name);
		}
	}
	List_Free(f_recv_pack_resp,PACK);

}

int main_menu()
{
    char choice_t[100];
    int chioce;
    do
    {
        get_status();
        //printf("pack num_chat:%d\n", m_recv_num_chat);
		sleep(1);
        print_main_menu();
        scanf("%s",choice_t);
        chioce = get_choice(choice_t);
        switch(chioce)
        {
            case 1:
                friend_add_apply();
                break;
            case 2:
                friend_add();
                break;
            case 3:
                friend_list();
                break;
            case 4:
                server_news_list();
                break;
            case 5:
                chat_to_one();
                break;
			case 6:
				group_create();
				break;
			case 7:
				group_join();
				break;
			case 8:
				group_quit();
				break;
			case 9:
				chat_to_group();
				break;
			case 10:
				file_request();
				break;
			case 11:
				file_apply_deal();
				break;
			case 12:
				friend_del();
				break;
			case 13:
				history_read();
				break;
			case 14:
				group_member_list();
				break;
			case 15:
				group_dis();
				break;
            default:
                  break;
        }
    }while(chioce!=0);
    return 0;
}

void print_main_menu()
{
	printf("*************main menu****************\n");
	printf("\t\t1.新朋友\n");
	printf("\t\t2.添加朋友\n");
	printf("\t\t3.好友列表\n");
	printf("\t\t4.系统消息  %d\n",server_news_num);
	printf("\t\t5.私聊\n");
	printf("\t\t6.创建群组\n");
	printf("\t\t7.加入群组\n");
	printf("\t\t8.退出群组\n");
	printf("\t\t9.群聊\n");
	printf("\t\t10.发送文件\n");
	printf("\t\t11.文件发送请求  %d\n",file_apply_num);
	printf("\t\t12.删除好友\n");
	printf("\t\t13.查看历史记录\n");
	printf("\t\t14.查看群成员\n");
	printf("\t\t15.解散群\n");
	printf("**************************************\n");
	printf("your choice:");
}
void server_news_list(void)
{
	//pthread_mutex_lock(&mutex);
	for(int i=1;i<=server_news_num;i++)
			printf("%s\n",server_news[i]);
	//pthread_mutex_unlock(&mutex);
	server_news_num = 0;
}
void friend_list(void)
{
	//pthread_mutex_lock(&mutex);
	printf("*****好友列表*****\n");
	if(my_infor.friend_num==0)
	{
		printf("还未添加好友!\n");
		return;
	}
	for(int i=1;i<=my_infor.friend_num;i++)
	{	
		if(my_infor.friends[i].statu==4)
			printf("%10s    new news %d  ONLINE\n",my_infor.friends[i].username,my_infor.friends[i].mes_num);
		else if(my_infor.friends[i].statu==3)
			printf("%10s    new news %d  DOWNLINE\n",my_infor.friends[i].username,my_infor.friends[i].mes_num);

	}
	//pthread_mutex_unlock(&mutex);
		
}
void history_read(void)
{
	printf("请输入想要查看历史记录的用户名:");
	char username[20];
	scanf("%s",username);
	int flag = 0;
	char words[10] = "history";
	while(strcmp(username,"quit")!=0){
	for(int i=1;i<=my_infor.friend_num;i++)
	{
		if(strcmp(my_infor.friends[i].username,username)==0)
		{
			flag = 1;
			break;
		}
	}
	if(flag == 0)
	{	
		printf("该好友不存在!请重新输入:(输入quit退出)\n");
		scanf("%s",username);
	}
	else 
	{
		printf("\t\t***和%s的聊天记录***\n",username);
		send_pack(HISTORY_READ,my_infor.username,username,words);	
		break;
	}
	}

}

void chat_to_one()
{
	if(EX==1)
		EX=0;
	char username[20];
	int flag = 0;
	char words[100];
	pthread_t tid;
	friend_list();
	printf("输入对象用户名:");
	scanf("%s",username);
	for(int i=1;i<=my_infor.friend_num;i++)
			if(strcmp(my_infor.friends[i].username,username)==0)
					flag = 1;
	if(flag == 0)
	{
		printf("未添加该好友!\n");
		return;
	}
	strcpy(chatuser,username);
	pthread_create(&tid,NULL,(void*)chatstart,NULL);
	while(1)
	{
		scanf("%s",words);
		if(strcmp(words,"quit")==0)
		{
			EX = 1;
			break;
		}
		send_pack(CHAT_ONE,my_infor.username,username,words);
		printf("me->:%s\n",words);

	}
	//EX=0;
	printf("对话结束\n");
	for(int i=1;i<=my_infor.friend_num;i++)
	{
		if(strcmp(my_infor.friends[i].username,username)==0)
		{
			my_infor.friends[i].mes_num = 0;
		}
	}

}

void* chatstart(void)
{
	PACK* news = all_recv_news;
	while(EX!=1)
	{
		//printf("%s:%s\n",news->data.send_name,chatuser);
		if(news->next==news)
				continue;
		news = news->next;
		if(strcmp(chatuser,news->data.send_name)==0)
		{
			printf("%s->:%s\n",news->data.send_name,news->data.words);
			List_FreeNode(news);
			news = all_recv_news;
		}
		
	}

}


void get_status(void)
{
    PACK get_friend_statu;
    get_friend_statu.data.type = FRIEND_STATU;
    strcpy(get_friend_statu.data.send_name,my_infor.username);
    strcpy(get_friend_statu.data.recv_name,"server");
    memset(get_friend_statu.data.words,0,sizeof(get_friend_statu.data.words));
    if(send(sock_fd,&(get_friend_statu.data),sizeof(PACK_DATA),0) < 0){
        my_err("send",__LINE__);
    }
}
void change_statu(PACK *pack_t)
{
    int count = 0;
    my_infor.friend_num = pack_t->data.words[count++];
    //更新好友信息
    for(int i=1; i <= my_infor.friend_num ;i++)
    {
        for(int j=0;j<20;j++)
        {
            if(j == 0)
                my_infor.friends[i].statu = pack_t->data.words[count+j] - 48;
            else
                my_infor.friends[i].username[j-1] = pack_t->data.words[count+j];
        }
        count += 20;
    }
	my_infor.group_num = pack_t->data.words[count++];
    for(int i=1 ;i <= my_infor.group_num ;i++)
    {
        for(int j=0;j<20;j++)
        {
            my_infor.group[i][j] = pack_t->data.words[count+j];
        }
        count += 20;
    }
}
void group_create(void)
{
    char group_name[20];
    printf("请输入群名:\n");
    scanf("%s",group_name);
    send_pack(GROUP_CREATE,my_infor.username,"server",group_name);
    while(!flag_group_create);
    //printf("m_flag_group_create=%d\n",flag_group_create);
    if(flag_group_create == 2)
        printf("群创建成功!\n");
    else if(flag_group_create == 1)
        printf("群已经存在!\n");
    flag_group_create = 0;
}

void group_join(void)
{

    char group_name[20];
    printf("输入你想要加入的群名:\n");
    scanf("%s",group_name);
    //判断是否已经加入该群
    for(int i=1;i <= my_infor.group_num ;i++)
    {
        if(strcmp(my_infor.group[i],group_name) == 0)
        {
            printf("你已经是该群群员!\n");
            return ;
        }
    }
    send_pack(GROUP_JOIN,my_infor.username,"server",group_name);
	//printf("---%d---\n",flag_group_join);
    while(!flag_group_join);
    if(flag_group_join == 2)
        printf("成功加入该群!\n");
    else if(flag_group_join == 1)
        printf("没有<%s>这个群\n",group_name);
    flag_group_join = 0;
}
void group_quit(void)
{
    char group_name[20];
    printf("输入你要退出的群名:\n");
    scanf("%s",group_name);
    //判断是否添加过该群组
    for(int i=1;i <= my_infor.group_num ;i++)
    {
        if(strcmp(my_infor.group[i],group_name) == 0)
        {
            send_pack(GROUP_QUIT,my_infor.username,"server",group_name);
            printf("成功退出群%s!\n",group_name);
            return ;
        }
    }

    printf("你没加这个群!\n");
}

int group_list(void)
{
	printf("******群组列表******\n");
	printf("群数:%d\n",my_infor.group_num);
	if(my_infor.group_num==0)
	{
		printf("你没有任何可水的群\n");
		return 0;
	}
	for(int i=1;i<=my_infor.group_num;i++)
			printf("%s\n",my_infor.group[i]);
	printf("********************\n");
	return 1;
}
void group_member_list()
{
	char groupname[20];
	int flag = 0;
	printf("请输入要查看的群:");
	scanf("%s",groupname);
	for(int i=1;i<=my_infor.group_num;i++)
		if(strcmp(my_infor.group[i],groupname)==0)
				flag = 1;
	if(flag == 0)
	{
		printf("未添加该群组!\n");
		return;
	}
	send_pack(G_M_L,my_infor.username,"server",groupname);
}

void chat_to_group()
{
	char groupname[20];
	int flag = 0;
	char words[100];
	pthread_t tid;
	if(group_list()==0)
			return;
	printf("输入对象群组名:");
	scanf("%s",groupname);
	for(int i=1;i<=my_infor.group_num;i++)
			if(strcmp(my_infor.group[i],groupname)==0)
					flag = 1;
	if(flag == 0)
	{
		printf("未添加该群组!\n");
		return;
	}
	strcpy(chatgroup,groupname);
	pthread_create(&tid,NULL,(void*)chatstart_group,NULL);
	char txt[100];
	while(1)
	{
		scanf("%s",words);
		sprintf(txt,"%s->%s",my_infor.username,words);
		if(strcmp(words,"quit")==0)
		{
			EX = 1;
			break;
		}
		send_pack(CHAT_GROUP,my_infor.username,groupname,txt);
		//printf("me->:%s\n",txt);

	}
	printf("群聊结束\n");
}

void* chatstart_group(void)
{
	PACK* news = all_recv_group_news;
	while(EX!=1)
	{
		if(news->next==news)
				continue;
		news = news->next;
		if(strcmp(chatgroup,news->data.send_name)==0)
		{
			printf("%s\n",news->data.words);
			List_FreeNode(news);
			news = all_recv_group_news;
		}
		
	}
}
void file_request(void)
{
	printf("请输入要发送文件的好友:");
	friend_list();
	char username[20];
	char filename[50];
	C_USER*user;
	scanf("%s",username);
	printf("请告知好友要发送的文件名:");
	scanf("%s",filename);
	int file_size = get_file_size(filename);
	if(file_size==0)
	{
		printf("请输入正确的文件名!\n");
		return;
	}
	printf("文件大小 :%d\n", file_size);
	int flag = 0;
	for(int i=1;i<=my_infor.friend_num;i++)
	{
		if(strcmp(my_infor.friends[i].username,username)==0)
		{
			flag = 1;
			break;
		}
	}
	if(flag == 0)
	{
		printf("你未添加该好友!\n");
		return;
	}
	int digit = 0;
	char words[100];
	memset(words,-1,sizeof(words));
    while(file_size != 0)
    {
        words[digit++] = file_size%10;
        file_size /= 10;
    }
    words[digit]  = -1;

    for(int i=0 ;i< 50 ;i++)
    {
        words[10+i] = filename[i];
    }
	printf(">>>>>>>>>>>>>>>>>>>>>文件名:%s\n",words+10);
	printf("信息已发送!");
	send_pack(FILE_REQUEST,my_infor.username,username,words);
	printf("等待对方同意中...\n");
	strcpy(file_recv_username,username);
	strcpy(file_path,filename);
	while(!file_resp_flag);
	if(file_resp_flag == 'y')
	{
		printf("对方已同意,文件%s开始发送!\n",filename);
		//send_pack(FILE_SEND,my_infor.username,file_recv_username,"testpack");
		pthread_send_file();
	}
	else
	{
		printf("请求被拒绝!\n");
	}
	file_resp_flag = 0;

}






void file_apply_deal(void)
{
	printf("********文件发送申请*********\n");
	if(List_IsEmpty(all_file_apply))
	{
		printf("无文件申请!\n");
		return;
	}
	PACK * pack;
	int file_size = 0;
	List_ForEach(all_file_apply,pack)
	{
		for(int i=0 ;i<10;i++)
		{
			if(pack->data.words[i] == -1)
				break;
			int t1 = 1;
			for(int l=0;l<i;l++)
				t1*=10;
		    file_size += (int)pack->data.words[i]*t1;
		}
		printf("****************************\n");
		printf("文件名:%s\n",pack->data.words+10);
		printf("发送者:%s\n",pack->data.send_name);
		printf("文件大小:%d\n",file_size);
		printf("****************************\n\n");
	}
	printf("请输入你选择的好友:");
	char username[20];
	char words[2];
	int flag = 0;
	scanf("%s",username);
	List_ForEach(all_file_apply,pack)
	{
		if(strcmp(pack->data.send_name,username)==0)
		{
			for(int i=0 ;i<10;i++)
			{
				if(pack->data.words[i] == -1)
					break;
				int t1 = 1;
				for(int l=0;l<i;l++)
					t1*=10;
				FILE_SIZE += (int)pack->data.words[i]*t1;
			}
			strcpy(FILENAME,pack->data.words+10);
			flag = 1;
			break;
		}
	}
	if(flag == 1)
	{
		List_FreeNode(pack);
	}
	printf("是否同意请求:<y/n>");
	scanf("%s",words);
	send_pack(FILE_APPLY_RESP,my_infor.username,username,words);
	printf("回应已发送\n");
	file_apply_num--;
	if(words[0]=='y')
	{
		puts("111");
//		pthread_mutex_lock(&mutex);
		printf("准备接收文件...\n");
		if((FD = open(FILENAME,O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR)) < 0)
		{
			my_err("open",__LINE__);
			return ;
		}
		printf("文件已打开!\n");
		//pthread_recv_file();
	}
	else
	{
		printf("请求已拒绝!\n");
	}
}

//接收文件函数2
void *pthread_recv_file(PACK*recv_pack_p)
{
		/*
	//int sum = 0;
	//int fd;
	//int len = 0;
	//int t1 = 0;
	//int flag = 0;
	//PACK recv_pack_t;
	//pthread_mutex_lock(&mutex);
    if((fd = open(FILENAME,O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR)) < 0)
    {
        my_err("open",__LINE__);
        return NULL;
    }
	while(1)
	{
        if(recv(sock_fd,&(recv_pack_t.data),sizeof(PACK_DATA),0) < 0)
            my_err("recv",__LINE__);
		if(recv_pack_t.data.type!=FILE_SEND)
				continue;
		len = 0;
	*/
		int t1 ;
		int len = 0;
        for(int j=0 ;j<10;j++)
        {
            if(recv_pack_p->data.words[j] == -1)
               break;
            t1 = 1;
            for(int l=0;l<j;l++)
                t1*=10;
            len += (int)recv_pack_p->data.words[j]*t1;
        }

        if(write(FD,recv_pack_p->data.words + 10,len) < 0)
            my_err("write",__LINE__);
		//lseek(FD,len,SEEK_CUR);
        SUM += len;
		printf("---------文件已写入%d-->%d(%d)字节----------\n",FILE_SIZE,len,SUM);
        if(SUM >= FILE_SIZE)
        {
			FILE_SIZE = 0;
			SUM = 0;
			close(FD);
            printf("文件接收完成!!\n");
		//	pthread_mutex_unlock(&mutex);
		//	break;
		}
}

void* pthread_send_file(void)
{
	printf("发送文件名:%s\n",file_path);
    int fd;
    int len;
    int file_size;
    int sum = 0;
    char words[MAXLEN];
    //打开文件
    if((fd = open(file_path,O_RDONLY)) == -1)
    {
        my_err("open",__LINE__);
        exit(-1);
    }
    file_size = lseek(fd, 0, SEEK_END);
    //文件指针移到开头
    lseek(fd ,0 ,SEEK_SET);
    bzero(words,MAXLEN);
	int digit = 0;
	printf("传输准备中...");
	//usleep(100000);
//	strcpy(words,"testpack");
//	send_pack(FILE_SEND,my_infor.username,file_recv_username,words);
	printf("传输开始!");
    // 每读取一段数据，便将其发送给客户端，循环直到文件读完为止
    while((len = read(fd,words+10, MAXLEN-10)) > 0)
    {

        sum += len;
		printf("-----------文件已读取%d-%d字节------------\n",file_size,sum);
        digit = 0;
        while(len != 0)
        {
            words[digit++] = len%10;
            len /= 10;
        }
        words[digit]  = -1;
		printf("%s",words);
		//getchar();
        send_pack(FILE_SEND,my_infor.username,file_recv_username,words);
		//usleep(100000);
        if(sum == file_size)
            break;
        bzero(words,MAXLEN);

    }
    // 关闭文件
    close(fd);
    printf("文件发送完成!!\n");
}





int my_send(int fd,void*words,int len)
{
	int nleft = len;
	int nread = 0;
	int ret;
	while(len!=nread)
	{
		ret = send(fd,(char*)words+nread,nleft,0);
		if(ret==0)
				break;
		else if(ret==SO_ERROR)
				break;
		else if(ret>0)
		{
			nread += ret;
			nleft = len - nread;
		}
	}
	return nread;

}
int my_recv(int fd,void*words,int len)
{
	int nleft = len;
	int nread = 0;
	int ret;
	while(len!=nread)
	{
		ret = recv(fd,(char*)words+nread,nleft,0);
		if(ret==0)
				break;
		else if(ret==SO_ERROR)
				break;
		else if(ret>0)
		{
			nread += ret;
			nleft = len - nread;
		}
	}
	return nread;
	
}

void my_err(const char * err_string,int line)
{
	fprintf(stderr, "line:%d ",line);
	perror(err_string);
	exit(-1);
}
int set_disp_mode(int fd,int option)
{
   int err;
   struct termios term;
   if(tcgetattr(fd,&term)==-1){
     perror("Cannot get the attribution of the terminal");
     return 1;
   }
   if(option)
        term.c_lflag|=ECHOFLAGS;
   else
        term.c_lflag &=~ECHOFLAGS;
   err=tcsetattr(fd,TCSAFLUSH,&term);
   if(err==-1 && err==EINTR){
        perror("Cannot set the attribution of the terminal");
        return 1;
   }
   return 0;
}
int get_file_size(char *file_name)
{
    int fd;
    int len;
    if((fd = open(file_name,O_RDONLY)) == -1)
    {
        my_err("open",__LINE__);
        return 0;
    }
    len = lseek(fd, 0, SEEK_END);
    close(fd);
    return len;
}
