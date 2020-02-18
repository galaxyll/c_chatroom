#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<pthread.h>
#include<mysql/mysql.h>
#include<fcntl.h>
#include<sys/epoll.h>
#include<unistd.h>
#include<netinet/in.h>
#include<errno.h>
#include<arpa/inet.h>
#include<signal.h>
#include<time.h>
#include"list.h"

#define LOGIN				1
#define REGISTER			2
#define	DOWNLINE			3
#define ONLINE				4
#define FRIEND_ADD_APPLY	5
#define FRIEND_ADD_RESP		6
#define CHAT_ONE			7
#define FRIEND_STATU		8
#define GROUP_CREATE		9
#define	GROUP_JOIN			10
#define	GROUP_QUIT			11
#define CHAT_GROUP			12
#define FILE_REQUEST        13
#define FILE_APPLY_RESP     14
#define	FILE_SEND			15
#define FILE_FINISH			16
#define FRIEND_DEL			17
#define HISTORY_READ		18
#define	G_M_L				19
#define GROUP_DIS			20
#define EXIT				-1

#define MAXLEN 510

typedef struct userdata{
		char username[20];
		char password[20];
		int statu;
		int fd;
		int friend_num;
		char friends[100][20];//100个好友
		int group_num;
		char group[100][20];//
}USER_DATA;
typedef struct userinfor{
		USER_DATA data;
		struct userinfor*prev;
		struct userinfor*next;
}USER;

typedef struct groupdata{
		char groupname[20];
		int group_num;
		char group_owner[20];
		char group_member[100][20];
}GROUP_DATA;
typedef struct groupinfor{
		GROUP_DATA data;
		struct groupinfor *prev;
		struct groupinfor *next;
}GROUP;

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
		struct package*prev;
		struct package*next;
}PACK;



typedef struct filedata{
		char filename[50];
		char send_name[20];
		char recv_name[20];
		int now_size;
		int all_size;
		int flag;
}FILE_DATA;
typedef struct filepack{
		FILE_DATA data;
		struct filepack*prev;
		struct filepack*next;
}FILE_PACK;




PACK *all_send_pack = NULL;
USER *all_sign_user = NULL;
GROUP * all_create_group = NULL;
FILE_PACK * all_send_file = NULL;
USER *userf = NULL;
int number;

		
//声明
void *pack_send_thread(void *arg);
void send_pack(PACK *pack_send_p);
void login(PACK *recv_pack_p);
void registe(PACK *recv_pack_p);
void my_err(const char * err_string,int line);
void *deal_pack(void *recv_pack_p_p);
void init_mysql(void);
USER* find_user(char*username);
void friend_add_deal(PACK*recv_pack_p);
void chat_to_one(PACK*recv_pack_p);
void send_pack_par(int type,char *send_name,char *recv_name,char *mes);
void send_statu(PACK *recv_pack_p);
void group_create(PACK *recv_pack_p);
void group_quit(PACK *recv_pack_p);
void group_join(PACK *recv_pack_p);
void chat_to_group(PACK*recv_pack_p);
void mysql_table_del(char * tablename,char*table_s1,char*table_s2,char *firstname,char *lastname);
void mysql_utable_ins(char*username,char*groupname);
void mysql_gtable_ins(char*firstname,char*mdname,char*lastname);
void file_request(PACK*recv_pack_p);
void file_send(PACK*recv_pack_p);
int my_send(int fd,void*words,int len);
int my_recv(int fd,void*words,int len);
void friend_del(PACK*recv_pack_p);
void history_read(PACK*recv_pack_p);
void group_member_list(PACK*recv_pack_p);
void group_dis(PACK*recv_pack_p);


MYSQL mysql;
int listen_fd,epoll_fd;
short PORT = 1234;
pthread_mutex_t mutex;

int main(void)
{
	List_Init(all_send_pack,PACK);
	List_Init(all_sign_user,USER);
	List_Init(all_create_group,GROUP);
	List_Init(all_send_file,FILE_PACK);
	PACK recv_pack_t;
	USER * cu;
	init_mysql();
	pthread_mutex_init(&mutex,NULL);
	pthread_t pack_send_tid;
	if(pthread_create(&pack_send_tid,NULL,pack_send_thread,NULL))
			my_err("pthread_create",__LINE__);
	printf("服务器启动中...\n");
	epoll_fd = epoll_create(10000);
	listen_fd = socket(AF_INET,SOCK_STREAM,0);
	int opt = 1;
	setsockopt(listen_fd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(int));
	struct epoll_event ev,events[2000];
	ev.data.fd = listen_fd;
	ev.events = EPOLLIN;
	epoll_ctl(epoll_fd,EPOLL_CTL_ADD,listen_fd,&ev);
	struct sockaddr_in sock_addr;
	sock_addr.sin_family = AF_INET;
	sock_addr.sin_port = htons(PORT);
	sock_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	if(bind(listen_fd,(struct sockaddr*)&sock_addr,sizeof(sock_addr))<0)
	{
		printf("绑定失败!\n");
		exit(-1);
	}
	printf("绑定成功:listen_fd:%d\n",listen_fd);
	if(listen(listen_fd,20)<0)
	{
		printf("监听失败\n");
		exit(-1);
	}
	printf("监听中...\n");
	printf("服务器初始化完成\n");

	int evs,cli_fd;
	socklen_t	cli_len = sizeof(struct sockaddr_in);
	while(1)
	{
		evs = epoll_wait(epoll_fd,events,2000,1000);
//		printf("当前循环有%d个事件\n",evs);
		for(int i=0;i<evs;i++)
		{
			if(events[i].data.fd == listen_fd)
			{
				cli_fd = accept(listen_fd,(struct sockaddr*)&sock_addr,&cli_len);
				printf("套接字%d连接到服务器\n",cli_fd);
				ev.data.fd = cli_fd;
				ev.events = EPOLLIN;
				epoll_ctl(epoll_fd,EPOLL_CTL_ADD,cli_fd,&ev);
			}
			else if(events[i].events&EPOLLIN)
			{
				int ret;
				ret = recv(events[i].data.fd,&(recv_pack_t.data),sizeof(PACK_DATA),0);
				recv_pack_t.data.send_fd = events[i].data.fd;
//				printf("接受了%d字节\n",ret);

				if(ret<0)
				{
					close(events[i].data.fd);
					printf("收包错误\n");
					continue;
				}
				else if(ret==0)
				{
						USER *cu;
					List_ForEach(all_sign_user,cu)
					{
						if(cu->data.fd==events[i].data.fd)
						{
							printf("用户%s已下线\n",cu->data.username);
							cu->data.statu = DOWNLINE;
							break;
						}
					}
							ev.data.fd = events[i].data.fd;
							epoll_ctl(epoll_fd,EPOLL_CTL_DEL,events[i].data.fd,&ev);
							close(events[i].data.fd);
							continue;
					
				}

					PACK *recv_pack_p = (PACK*)malloc(sizeof(PACK));
					memset(recv_pack_p,0,sizeof(PACK));
					memcpy(&(recv_pack_p->data),&(recv_pack_t.data),sizeof(PACK_DATA));
					pthread_t back_tid;
					
				printf("sendname:%s\n",recv_pack_t.data.send_name);
				printf("recvname:%s\n",recv_pack_t.data.recv_name);
				printf("send_fd:%d\n",recv_pack_t.data.send_fd);
				printf("recv_fd%d\n",recv_pack_t.data.recv_fd);
				printf("words:%s\n",recv_pack_t.data.words);
//					if(pthread_create(&back_tid,NULL,deal_pack,(void*)recv_pack_p)!=0)
//							my_err("pthread_create",__LINE__);
				deal_pack((void*)recv_pack_p);
				
			}
		}
	}

	return 0;

}

void init_mysql(void)
{
	//mysql相关
	if(NULL == mysql_init(&mysql))
				printf("init mysql failed\n");
	if(NULL == mysql_real_connect(&mysql,"localhost","debian-sys-maint","jOE9r7pWBUZLvHIY","chatroom",0,NULL,0))
				printf("connect %s\n",mysql_error(&mysql));
	//获取用户
	char * sql_login = "SELECT username,password FROM userlogin";

	if(0 != mysql_real_query(&mysql,sql_login,strlen(sql_login)))
				printf("query:%s\n",mysql_error(&mysql));

	MYSQL_RES * res = mysql_store_result(&mysql);
	MYSQL_ROW row;
	USER * user = NULL;
	while((row = mysql_fetch_row(res)))
	{
		user = (USER*)malloc(sizeof(USER));
		strcpy(user->data.username,row[0]);
		strcpy(user->data.password,row[1]);
		user->data.statu = DOWNLINE;
		user->data.friend_num = 0;
		user->data.group_num = 0;
		List_AddTail(all_sign_user,user);
		printf("user:%s\npassword:%s\n",user->data.username,user->data.password);
	}
	mysql_free_result(res);

	//获取好友
	char * sql_friend = "SELECT username,friendname FROM userfriend";
	if(0 != mysql_real_query(&mysql,sql_friend,strlen(sql_friend)))
				printf("query:%s\n",mysql_error(&mysql));
	MYSQL_RES * res2 = mysql_store_result(&mysql);
	MYSQL_ROW row2;
	while((row2 = mysql_fetch_row(res2)))
	{
		user = all_sign_user;
		while(user->next!=all_sign_user)
		{
			user = user->next;
			if(strcmp(user->data.username,row2[0])==0)
			{
				strcpy(user->data.friends[++(user->data.friend_num)],row2[1]);
				break;
			}
		}
	}
	mysql_free_result(res2);

	char * sql_group = "SELECT username,groupname FROM usergroup";
	if(0 != mysql_real_query(&mysql,sql_group,strlen(sql_group)))
				printf("query:%s\n",mysql_error(&mysql));
	MYSQL_RES * res3 = mysql_store_result(&mysql);
	MYSQL_ROW row3;
	while((row3 = mysql_fetch_row(res3)))
	{
		user = all_sign_user;
		while(user->next!=all_sign_user)
		{
			user = user->next;
			if(strcmp(user->data.username,row3[0])==0)
			{
				strcpy(user->data.group[++(user->data.group_num)],row3[1]);
				break;
			}
		}
	}
	mysql_free_result(res3);

	char * sql_groupuser = "SELECT groupname,username,group_owner FROM groupuser";
	if(0 != mysql_real_query(&mysql,sql_groupuser,strlen(sql_groupuser)))
				printf("query:%s\n",mysql_error(&mysql));
	GROUP * group = NULL;
	MYSQL_RES * res4 = mysql_store_result(&mysql);
	MYSQL_ROW row4;
	int flag = 0;
	while((row4 = mysql_fetch_row(res4)))
	{
		group = NULL;
		flag = 0;
		if(all_create_group->next==all_create_group)
		{
			group = (GROUP*)malloc(sizeof(GROUP));
			strcpy(group->data.groupname,row4[0]);
			group->data.group_num = 0;
			strcpy(group->data.group_member[++(group->data.group_num)],row4[1]);
			strcpy(group->data.group_owner,row4[2]);
			List_AddTail(all_create_group,group);
		}
		else
		{
			List_ForEach(all_create_group,group)
			{
				if(strcmp(group->data.groupname,row4[0])==0)
				{
					flag = 1;
					strcpy(group->data.group_member[++(group->data.group_num)],row4[1]);
				}
			}

			if(flag == 0)
			{
				group = (GROUP*)malloc(sizeof(GROUP));
				strcpy(group->data.groupname,row4[0]);
				group->data.group_num = 0;
				strcpy(group->data.group_member[++(group->data.group_num)],row4[1]);
				strcpy(group->data.group_owner,row4[2]);
			}

		}

	}
	mysql_free_result(res4);
}


void *deal_pack(void *recv_pack_p_p)
{
    int i;
    PACK *recv_pack_p = (PACK *)recv_pack_p_p;
    printf("包类型：%d\n", recv_pack_p->data.type);

	if(number==0&&recv_pack_p->data.type==FILE_SEND)
	{
		userf = find_user(recv_pack_p->data.recv_name);
		printf("第一次进入\n");
		number++;
	}
	
    switch(recv_pack_p->data.type)
    {
        case LOGIN:
            login(recv_pack_p);
            break;
        case REGISTER:
            registe(recv_pack_p);
            break;
		case FRIEND_ADD_APPLY:
			friend_add_deal(recv_pack_p);
			break;
		case FRIEND_ADD_RESP:
			friend_add_deal(recv_pack_p);
			break;
		case CHAT_ONE:
			chat_to_one(recv_pack_p);
			break;
		case CHAT_GROUP:
			chat_to_group(recv_pack_p);
			break;
		case FRIEND_STATU:
			send_statu(recv_pack_p);
			break;
		case GROUP_CREATE:
			group_create(recv_pack_p);
			break;
		case GROUP_JOIN:
			group_join(recv_pack_p);
			break;
		case GROUP_QUIT:
			group_quit(recv_pack_p);
			break;
		case FILE_REQUEST:
			send_pack(recv_pack_p);
			break;
		case FILE_SEND:
			send(userf->data.fd,&recv_pack_p->data,sizeof(PACK_DATA),0);
			break;
		case FILE_APPLY_RESP:
			send_pack(recv_pack_p);
			break;
		case FRIEND_DEL:
			friend_del(recv_pack_p);
			break;
		case FILE_FINISH:
			number = 0;
			break;
		case HISTORY_READ:
			history_read(recv_pack_p);
			break;
		case G_M_L:
			group_member_list(recv_pack_p);
			break;
		case GROUP_DIS:
			group_dis(recv_pack_p);
			break;
        case EXIT:

            break;
    }
}
void group_dis(PACK*recv_pack_p)
{
	GROUP *group = NULL;
	int n;
	PACK*pack = (PACK*)malloc(sizeof(PACK));
	strcpy(pack->data.words,recv_pack_p->data.words);
	List_ForEach(all_create_group,group)
	{
		printf("该群全部成员:(%d)\n",group->data.group_num);
		for(int i=1;i<=group->data.group_num;i++)
				printf("%d:%s\n",i,group->data.group_member[i]);
		printf("-------------\n");
		if(strcmp(group->data.groupname,recv_pack_p->data.words)==0)
		{
			if(strcmp(recv_pack_p->data.send_name,group->data.group_owner)==0)
			{
				n = group->data.group_num;
				char g[n+1][20];
				for(int i=1;i<=n;i++)
						strcpy(g[i],group->data.group_member[i]);
				for(int i=1;i<=n;i++)
				{
					printf("<<%d>>\n",group->data.group_num);
					strcpy(pack->data.send_name,g[i]);
					printf("{%s}\n",pack->data.send_name);
					printf("<%d:%s>\n",i,group->data.group_member[i]);
					group_quit(pack);
				}
				send_pack_par(GROUP_DIS,"server",recv_pack_p->data.send_name,"delet group success!");
			}
			else
				send_pack_par(GROUP_DIS,"server",recv_pack_p->data.send_name,"you is not group owner!");
			break;
		}
	}		
	free(recv_pack_p);
}

void group_member_list(PACK*recv_pack_p)
{
	GROUP *group = NULL;
	char str[20];
	int count = 0;
	List_ForEach(all_create_group,group)
	{
		if(strcmp(group->data.groupname,recv_pack_p->data.words)==0)
		{
				for(int i=1;i<=group->data.group_num;i++)
				{
					strcpy(str,group->data.group_member[i]);
					send_pack_par(G_M_L,"server",recv_pack_p->data.send_name,str);
				}
				break;
		}
	}
	free(recv_pack_p);
}
void registe(PACK *recv_pack_p)
{
    char words[2];

    if(find_user(recv_pack_p->data.send_name)==NULL){
        words[0] = '1';
		USER * cu = (USER*)malloc(sizeof(PACK));
        strcpy(cu->data.username,recv_pack_p->data.send_name);
        strcpy(cu->data.password,recv_pack_p->data.words);
		cu->data.fd = recv_pack_p->data.send_fd;

        printf(" registe success!\n");
        printf(" username:%s\n", cu->data.username);
        cu->data.statu = DOWNLINE;
		List_AddTail(all_sign_user,cu);
		char  sql[2200];
		sprintf(sql,"INSERT INTO userlogin(username,password) VALUES('%s','%s')",recv_pack_p->data.send_name,recv_pack_p->data.words);
		if(0 != mysql_real_query(&mysql,sql,strlen(sql)))
				printf("query:%s\n",mysql_error(&mysql));
    }
    else
		words[0] = '0';
    words[1] = '\0';
    //包信息赋值
    strcpy(recv_pack_p->data.recv_name,recv_pack_p->data.send_name);
    strcpy(recv_pack_p->data.send_name,"server");
    strcpy(recv_pack_p->data.words,words);
    recv_pack_p->data.recv_fd = recv_pack_p->data.send_fd;
    recv_pack_p->data.send_fd = listen_fd;

    send_pack(recv_pack_p);
    free(recv_pack_p);
}


USER* find_user(char *username)
{

    int i;
	USER *cu;

    if(all_sign_user->next == all_sign_user)
	{	
		printf("无用户注册!\n");
		return NULL;
	}
	printf("%s\n",username);
	printf("%s\n",all_sign_user->next->data.username);
    List_ForEach(all_sign_user,cu) 
    {
        if(strcmp(cu->data.username,username) == 0)
            return cu;
    }
        return NULL;
}

void history_read(PACK*recv_pack_p)
{
	char sql[1000];
	sprintf(sql,"SELECT send_name,recv_name,words FROM history");
	if(0!=mysql_real_query(&mysql,sql,strlen(sql)))
	{
		printf("query:%s\n",mysql_error(&mysql));
	}
	MYSQL_RES * res = mysql_store_result(&mysql);
	MYSQL_ROW row;
	while((row = mysql_fetch_row(res)))
	{
		if(strcmp(recv_pack_p->data.send_name,row[0])==0&&strcmp(recv_pack_p->data.recv_name,row[1])==0)
			send_pack_par(HISTORY_READ,recv_pack_p->data.send_name,recv_pack_p->data.send_name,row[2]);
		else if(strcmp(recv_pack_p->data.send_name,row[1])==0&&strcmp(recv_pack_p->data.recv_name,row[0])==0)
			send_pack_par(HISTORY_READ,recv_pack_p->data.recv_name,recv_pack_p->data.send_name,row[2]);
	}
}


void login(PACK *recv_pack_p)
{
	char words[2];
	USER *user = NULL;
	printf("进入登录函数\n");
	printf("%s\n",recv_pack_p->data.send_name);
    if((user = find_user(recv_pack_p->data.send_name)) == NULL){
        words[0] = '1';//do not have the user
    }
    else if (user->data.statu == ONLINE)
    {
        words[0] = '2';//user had online
    }
    else if(strcmp(user->data.password,recv_pack_p->data.words)!=0){
        words[0] = '3';//password is wrong

    }
    else
		words[0] = '0';
    words[1] = '\0';
        user->data.fd = recv_pack_p->data.send_fd;//niupi
        printf("\n\n\033[;45m********l登录包*********\033[0m\n");
        printf("\033[;45m*\033[0m %s get online!\n",user->data.username);
        printf("\033[;45m*\033[0m statu:    %d\n", user->data.statu);
        printf("\033[;45m*\033[0m socket_id:%d\n\n",user->data.fd);

    //包信息赋值
    strcpy(recv_pack_p->data.recv_name,recv_pack_p->data.send_name);
    strcpy(recv_pack_p->data.send_name,"server");
    strcpy(recv_pack_p->data.words,words);
    recv_pack_p->data.recv_fd = recv_pack_p->data.send_fd;
    recv_pack_p->data.send_fd = listen_fd;
		printf("sendname:%s\n",recv_pack_p->data.send_name);
		printf("recvname:%s\n",recv_pack_p->data.recv_name);
		printf("send_fd:%d\n",recv_pack_p->data.send_fd);
		printf("recv_fd%d\n",recv_pack_p->data.recv_fd);
		printf("words:%s\n",recv_pack_p->data.words);

    send_pack(recv_pack_p);

	usleep(1);
    if(words[0] =='0')
	{
		user->data.statu = ONLINE;
		user->data.fd = recv_pack_p->data.recv_fd;
	}

    free(recv_pack_p);
}
void send_pack(PACK *pack_send_p)
{
	PACK * cu = (PACK*)malloc(sizeof(PACK));
    memcpy(&(cu->data),&(pack_send_p->data),sizeof(PACK_DATA));
    pthread_mutex_lock(&mutex);
	List_AddTail(all_send_pack,cu);
    pthread_mutex_unlock(&mutex);
}
void my_err(const char * err_string,int line)
{
    fprintf(stderr, "line:%d ",line);
    perror(err_string);
    exit(-1);
}

void *pack_send_thread(void *arg)
{
	printf("后台线程:::\n");
    int statu ;
    PACK * cu;
    USER * user;
    int i,fd_t,fd_online;
	int flag = 0;
	PACK * tmp;
    while(1)
    {
        pthread_mutex_lock(&mutex);//线程锁
		flag = 0;
        statu = DOWNLINE;
        List_ForEach(all_send_pack,cu)
        {

            List_ForEach(all_sign_user,user)
            {

            if(strcmp(cu->data.recv_name,user->data.username) == 0)
                {
                    statu = user->data.statu;
                    if(statu == ONLINE)
                        fd_online = user->data.fd;
                    break;
                }
			}
            //上线，则发送包
            if(statu == ONLINE || cu->data.type == LOGIN || cu->data.type == REGISTER)
            {
                if(statu == ONLINE)
                    fd_t = fd_online;
                else
                    fd_t = cu->data.recv_fd;


                if(my_send(fd_t,&(cu->data),sizeof(PACK_DATA)) < 0){
                    my_err("send",__LINE__);
                }
				flag = 1;
				tmp = cu;
			//	for(int i=0;i<10;i++)
			//		printf("%d>-",cu->data.words[i]);
                break;
            }
        }
		if(flag==1)	
			List_FreeNode(tmp);
        pthread_mutex_unlock(&mutex);
        //usleep(1);
    }
}

void friend_add_deal(PACK*recv_pack_p)
{
	USER*user;
	PACK *send_pack_p = (PACK*)malloc(sizeof(PACK));
	if((user = find_user(recv_pack_p->data.recv_name))!=NULL)
	{
		memcpy(&(send_pack_p->data),&(recv_pack_p->data),sizeof(PACK_DATA));
		send_pack_p->data.recv_fd = user->data.fd;
		send_pack(send_pack_p);
		if(recv_pack_p->data.type == FRIEND_ADD_RESP&&recv_pack_p->data.words[0]=='y')
		{
			strcpy(user->data.friends[++(user->data.friend_num)],recv_pack_p->data.send_name);
			user = find_user(recv_pack_p->data.send_name);
			strcpy(user->data.friends[++(user->data.friend_num)],recv_pack_p->data.recv_name);
			char  sql[2200];
			sprintf(sql,"INSERT INTO userfriend(username,friendname) VALUES('%s','%s')",recv_pack_p->data.send_name,recv_pack_p->data.recv_name);
			if(0 != mysql_real_query(&mysql,sql,strlen(sql)))
				printf("query:%s\n",mysql_error(&mysql));
			sprintf(sql,"INSERT INTO userfriend(username,friendname) VALUES('%s','%s')",recv_pack_p->data.recv_name,recv_pack_p->data.send_name);
			if(0 != mysql_real_query(&mysql,sql,strlen(sql)))
				printf("query:%s\n",mysql_error(&mysql));
			
			printf("好友信息写入成功!\n");
		}
	}
	else
	{
		strcpy(send_pack_p->data.recv_name,send_pack_p->data.send_name);
		strcpy(send_pack_p->data.send_name,"server");
		send_pack_p->data.recv_fd,send_pack_p->data.send_fd;
		send_pack_p->data.words[0] = '0';
		send_pack_p->data.words[1] = '\0';
		printf("无该用户!\n");
		send_pack_p->data.type = FRIEND_ADD_RESP;
		send_pack(send_pack_p);
	}

}

void friend_del(PACK*recv_pack_p)
{
	USER* user;
	PACK * send_pack_p = (PACK*)malloc(sizeof(PACK));
	user = find_user(recv_pack_p->data.send_name);
	for(int i=1;i<=user->data.friend_num;i++)
	{
		if(strcmp(recv_pack_p->data.recv_name,user->data.friends[i])==0)
		{
			for(int j=i;j<user->data.friend_num;j++)
				strcpy(user->data.friends[j] , user->data.friends[j+1]);
			user->data.friend_num--;
			mysql_table_del("userfriend","username","friendname",user->data.username,recv_pack_p->data.recv_name);
			mysql_table_del("userfriend","username","friendname",recv_pack_p->data.recv_name,user->data.username);
			break;
		}
	}
	user = find_user(recv_pack_p->data.recv_name);
	for(int i=1;i<=user->data.friend_num;i++)
	{
		if(strcmp(recv_pack_p->data.send_name,user->data.friends[i])==0)
		{
			for(int j=i;j<user->data.friend_num;j++)
				strcpy(user->data.friends[j] , user->data.friends[j+1]);
			user->data.friend_num--;
			break;
		}
	}
	send_pack_par(FRIEND_DEL,recv_pack_p->data.send_name,recv_pack_p->data.recv_name,recv_pack_p->data.words);
}



	
void send_pack_par(int type,char *send_name,char *recv_name,char *mes)
{
	USER * user = find_user(recv_name);
    PACK pack_send_pack;
    pack_send_pack.data.type = type;
    strcpy(pack_send_pack.data.send_name,send_name);
    strcpy(pack_send_pack.data.recv_name,recv_name);
    strcpy(pack_send_pack.data.words,mes);
    if(send(user->data.fd,&(pack_send_pack.data),sizeof(PACK_DATA),0) < 0){
        my_err("send",__LINE__);
    }
}

void chat_to_one(PACK*recv_pack_p)
{
	char sql[1000];
	//send_pack_par(CHAT_ONE,recv_pack_p->data.send_name ,recv_pack_p->data.recv_name ,recv_pack_p->data.words);
	//free(recv_pack_p);
	sprintf(sql,"INSERT INTO history(send_name,recv_name,words) VALUES('%s','%s','%s')",recv_pack_p->data.send_name,recv_pack_p->data.recv_name,recv_pack_p->data.words);
	if(0 != mysql_real_query(&mysql,sql,strlen(sql)))
		printf("query:%s\n",mysql_error(&mysql));
	send_pack(recv_pack_p);
}

void send_statu(PACK *recv_pack_p)
{
    char str[MAXLEN];
    char name_t[MAXLEN];
    char send_statu_mes[MAXLEN];
    int count = 0;

    memset(send_statu_mes,0,sizeof(send_statu_mes));

    USER* user = find_user(recv_pack_p->data.send_name);

    send_statu_mes[count++] = user->data.friend_num;

    //字符串处理
	USER *cu;
    for(int i=1 ;i <= user->data.friend_num ;i++)
    {
        strcpy(name_t,user->data.friends[i]);
		List_ForEach(all_sign_user,cu)
        {
            if(strcmp(name_t,cu->data.username) == 0)
            {
                memset(str,0,sizeof(str));
                sprintf(str,"%d%s",cu->data.statu,cu->data.username);
                printf("str = %s\n",str);
                for(int j = 0 ;j< 20;j++)
                {
                    send_statu_mes[j+count] = str[j];
                }
                count += 20;
				break;
            }
        }
    }
	send_statu_mes[count++] = user->data.group_num;
    for(int i = 1;i <= user->data.group_num;i++)
    {
        memset(str,0,sizeof(str));
        strcpy(name_t,user->data.group[i]);
		printf("groupname = %s\n",user->data.group[i]);
        sprintf(str,"%s",name_t);
        for(int j = 0 ;j< 20;j++)
        {
            send_statu_mes[j+count] = str[j];
        }
        count += 20;
    }

    //包信息赋值
    strcpy(recv_pack_p->data.recv_name,recv_pack_p->data.send_name);
    strcpy(recv_pack_p->data.send_name,"server");
    memcpy(recv_pack_p->data.words,send_statu_mes,MAXLEN);
    recv_pack_p->data.recv_fd = recv_pack_p->data.send_fd;
    recv_pack_p->data.send_fd = listen_fd;

    send_pack(recv_pack_p);
    free(recv_pack_p);
}
void group_create(PACK *recv_pack_p)
{
    //判断群是否已经存在
	GROUP * group = NULL;
	USER * user = NULL;
    List_ForEach(all_create_group,group)
    {
        if(strcmp(group->data.groupname,recv_pack_p->data.words) == 0)
		{
				strcpy(recv_pack_p->data.recv_name,recv_pack_p->data.send_name);
            strcpy(recv_pack_p->data.send_name,"server");
            recv_pack_p->data.words[0] = 1;
            send_pack(recv_pack_p);
            //free(recv_pack_p);
			printf("群已存在!\n");
            return ;
        }
    }
    //群不存在则新建
	group = (GROUP*)malloc(sizeof(GROUP));
    strcpy(group->data.groupname,recv_pack_p->data.words);
	group->data.group_num = 0;
    strcpy(group->data.group_member[++(group->data.group_num)],recv_pack_p->data.send_name);//群主
    strcpy(group->data.group_owner,recv_pack_p->data.send_name);//群主
	List_AddTail(all_create_group,group);
	if((user = find_user(recv_pack_p->data.send_name))==NULL)
			my_err("find_user",__LINE__);
    strcpy(user->data.group[++(user->data.group_num)],recv_pack_p->data.words);
	//新信息存到群组数据库
	mysql_gtable_ins(group->data.groupname,recv_pack_p->data.send_name,recv_pack_p->data.send_name);
	mysql_utable_ins(recv_pack_p->data.send_name,recv_pack_p->data.words);	
	//end
	printf("新建群已存入数据库\n");

    printf("创建群%s成功!\n", recv_pack_p->data.words);
    strcpy(recv_pack_p->data.recv_name,recv_pack_p->data.send_name);
    strcpy(recv_pack_p->data.send_name,"server");


    recv_pack_p->data.words[0] = 2;
    send_pack(recv_pack_p);
    free(recv_pack_p);
}
void group_join(PACK *recv_pack_p)
{
    //找到该群，并加入
	GROUP *group = NULL;
	USER *user = NULL;
	List_ForEach(all_create_group,group)
    {
        if(strcmp(group->data.groupname,recv_pack_p->data.words) == 0)
        {
            strcpy(group->data.group_member[++(group->data.group_num)],recv_pack_p->data.send_name);
			if((user = find_user(recv_pack_p->data.send_name))==NULL)
					my_err("find_user",__LINE__);

            strcpy(user->data.group[++(user->data.group_num)],recv_pack_p->data.words);
            strcpy(recv_pack_p->data.recv_name,recv_pack_p->data.send_name);
            strcpy(recv_pack_p->data.send_name,"server");
			recv_pack_p->data.recv_fd = recv_pack_p->data.send_fd;
            printf(" %s 加入群 %s !\n",recv_pack_p->data.recv_name, recv_pack_p->data.words);
			char sql[2200];
			sprintf(sql,"INSERT INTO groupuser(groupname,username,group_owner) VALUES('%s','%s','%s')",group->data.groupname,recv_pack_p->data.recv_name,group->data.group_owner);
			if(0!=mysql_real_query(&mysql,sql,strlen(sql)))
			{
				printf("query:%s\n",mysql_error(&mysql));
			}
			printf("群信息已更新\n");
			sprintf(sql,"INSERT INTO usergroup(username,groupname) VALUES('%s','%s')",user->data.username,recv_pack_p->data.words);
			if(0!=mysql_real_query(&mysql,sql,strlen(sql)))
			{
				printf("query:%s\n",mysql_error(&mysql));
			}
			printf("用户群信息已更新\n");

			recv_pack_p->data.words[0] = 2;
            send_pack(recv_pack_p);
            free(recv_pack_p);
            return ;
        }
    }
    //未找到该群
    strcpy(recv_pack_p->data.recv_name,recv_pack_p->data.send_name);
    strcpy(recv_pack_p->data.send_name,"server");

    recv_pack_p->data.words[0] = 1;
    send_pack(recv_pack_p);
    free(recv_pack_p);
}
void group_quit(PACK *recv_pack_p)
{
	USER * user = NULL;
	if((user = find_user(recv_pack_p->data.send_name))==NULL)
	{
		printf("用户不存在!\n");
		return;
	}
	for(int i=1;i<=user->data.group_num;i++)
	{
		if(strcmp(user->data.group[i],recv_pack_p->data.words)==0)
		{
			for(int j = i;j<user->data.group_num;j++)
					strcpy(user->data.group[j],user->data.group[j+1]);
			user->data.group_num--;
			//数据库删除用户表相应行
			mysql_table_del("usergroup","username","groupname",user->data.username,recv_pack_p->data.words);
			//end
			break;
		}
	}
	GROUP * group = NULL;
	List_ForEach(all_create_group,group)
	{
		if(strcmp(group->data.groupname,recv_pack_p->data.words)==0)
		{
			for(int i=1;i<=group->data.group_num;i++)
			{
				if(strcmp(group->data.group_member[i],recv_pack_p->data.send_name)==0)
				{
					for(int j=i;j<group->data.group_num;j++)
						strcpy(group->data.group_member[j],group->data.group_member[j+1]);
					group->data.group_num--;
					//数据库删除群组表相应行
					mysql_table_del("groupuser","groupname","username",recv_pack_p->data.words,user->data.username);
					//end
					break;
				}
			}
		}
	}
}



void chat_to_group(PACK*recv_pack_p)
{
	GROUP *group = NULL;
	//memcpy(&(recv_pack_p->data.words[50]),recv_pack_p->data.send_name,20);
	//memcpy(words+20,recv_pack_p->data.words,80);
	List_ForEach(all_create_group,group)
	{
		if(strcmp(group->data.groupname,recv_pack_p->data.recv_name)==0)
		{
			for(int i=1;i<=group->data.group_num;i++)
			{
				send_pack_par(CHAT_GROUP,group->data.groupname,group->data.group_member[i],recv_pack_p->data.words);
			}
			break;
		}
	}
	free(recv_pack_p);
}

void mysql_table_del(char * tablename,char*table_s1,char*table_s2,char *firstname,char *lastname)
{	
	char sql[2200];
	sprintf(sql,"DELETE FROM %s WHERE %s = '%s' and %s = '%s'",tablename,table_s1,firstname,table_s2,lastname);
	if(0!=mysql_real_query(&mysql,sql,strlen(sql)))
	{
				printf("query:%s\n",mysql_error(&mysql));
				exit(-1);
	}
	printf("删除行成功!\n");
	
}
void mysql_gtable_ins(char*firstname,char*mdname,char*lastname)
{
	char sql[2200] ;
	sprintf( sql,"INSERT INTO groupuser(groupname,username,group_owner) VALUES('%s','%s','%s')",firstname,mdname,lastname);
	if(0!=mysql_real_query(&mysql,sql,strlen(sql)))
	{
		printf("query:%s\n",mysql_error(&mysql));
	}
}

void mysql_utable_ins(char*username,char*groupname)
{
	char sql[2200] ;
	sprintf( sql,"INSERT INTO usergroup(username,groupname) VALUES('%s','%s')",username,groupname);
	if(0!=mysql_real_query(&mysql,sql,strlen(sql)))
	{
		printf("query:%s\n",mysql_error(&mysql));
	}
	
}


void file_request(PACK*recv_pack_p)
{
	send_pack(recv_pack_p);
	printf("文件名:%s\n",(recv_pack_p->data.words+10));
	free(recv_pack_p);
}
void file_send(PACK*recv_pack_p)
{
	send_pack(recv_pack_p);
//	free(recv_pack_p);	
}



int my_send(int fd,void*words,int len)
{
	int nleft = len;
	int nread = 0;
	while(len!=nread)
	{
		nread += send(fd,(char*)words+nread,nleft,0);
		nleft = len - nread;
	}
	return nread;

}
int my_recv(int fd,void*words,int len)
{
	int nleft = len;
	int nread = 0;
	while(len!=nread)
	{
		nread += recv(fd,(char*)words+nread,nleft,0);
		nleft = len - nread;
	}
	return nread;
	
}




