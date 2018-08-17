#include <stdio.h>
#include <strings.h>
#include <stdlib.h>
#include <string.h>
#include <mysql.h>


void insert_data(char* word)
{
    char mean[1024];
    char sql[1024];
    MYSQL* mysql_fd = mysql_init(NULL);
    if(mysql_fd == 0)
    {
        printf("init failed\n");
        return ;
    }

    if(mysql_real_connect(mysql_fd, "127.0.0.1","root","","mydict",3306,NULL,0) == NULL)
    {
        printf("connect failed\n");
        return;
    }

    mysql_query(mysql_fd, "set names utf8"); 
    sprintf(sql, "SELECT * FROM mydict WHERE word=\"%s\"", word);
    mysql_query(mysql_fd, sql);                                                                                    
    MYSQL_RES* res = mysql_store_result(mysql_fd);                                                                 
    int row = mysql_num_rows(res);                                                                                 
    int col = mysql_num_fields(res);               
    int i = 0;
    for(i = 0; i < row;i++)                                                                                        
    {                                                                                                              
        MYSQL_ROW rowData = mysql_fetch_row(res);                                                                  
        int j = 0;                                                                                                 
        for(; j < col;j++)                                                                                         
        {              
            if(j == 1)
            {
                strcpy(mean,rowData[j]);

            }
            printf("<td>%s</td>",rowData[j]);                                                                      
        }                                                                                                          
    }                                                                                                              

    char SQL[1024];


    sprintf(SQL, "INSERT INTO NewWord(word, mean) VALUES(\"%s\",\"%s\")",word, mean);
    printf("SQL: %s\n",SQL);
    //const char* sql = "INSERT INTO NewWord (word, mean) VALUES(\"haha\",\"哈哈\")";

    mysql_query(mysql_fd, SQL);

    mysql_close(mysql_fd);
}


int main()
{
    char data[1024];
    if(getenv("METHOD"))  
    {
        if(strcasecmp("GET",getenv("METHOD")) == 0)
        {
            strcpy(data, getenv("QUERY_STRING"));
        }
        else
        {
            int content_length = atoi(getenv("CONTENT_LENGTH"));
            int i = 0;
            for(; i < content_length;i++)
            {
                read(0, data+i, 1);
            }
            data[i] = 0;
        }

    }
    printf("arg :%s\n",data);
    // printf("client version: %s\n", mysql_get_client_info()); 
    char *word;
    //char *mean;
    strtok(data, "=&");
    word = strtok(NULL, "=&");
    //strtok(NULL, "=&");
    //mean = strtok(NULL, "=&");
    //    sscanf(data, "word=%s&mean=%s",word,mean);
    insert_data(word);
    return 0;
}
