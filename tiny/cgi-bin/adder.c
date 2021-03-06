/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"

int main(void) {
  char *buf, *p;
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
  int n1=0, n2=0;

  /* Extract the two arguments */
  if ((buf = getenv("QUERY_STRING")) != NULL){
    // strchr(str, character) : returns a pointer to the first occurence of 'character' in 'str'
    p = strchr(buf, '&'); 
    *p = '\0';
    strcpy(arg1, buf);     // arg1 : first=12
    strcpy(arg2, p+1);     // arg2 : second=23
    // atoi : converts a character into an integer
    // n1 = atoi(arg1); 
    // n2 = atoi(arg2);
    n1 = atoi(strchr(arg1,'=')+1);  //12 
    n2 = atoi(strchr(arg2,'=')+1);  //23
    // 수빈이 방법
    // sscanf(buf, "first=%d", &n1);
    // sscanf(p + 1, "second=%d", &n2);
  }

  /*
    sprintf(msg1, "Hello");
    printf("%s", msg1);   //Hello
    sprintf(msg2, "%s world!", msg1);
    printf("%s", msg2);   //Hello world!
  */

  /* Make the response body */
  sprintf(content, "QUERY_STRING=%s", buf);
  sprintf(content, "Welcome to add.com: ");
  sprintf(content, "%sTHE Internet addition portal.\r\n<p>", content);
  // sprintf(content, "%sThe answer is: %s + %s = args\r\n<p>", content, arg1, arg2);
  sprintf(content, "%sThe answer is: %d + %d = %d\r\n<p>", content, n1,n2,n1+n2);
  sprintf(content, "%sThanks for visiting!\r\n", content);


  /* Generate the HTTP response */
  printf("Connection: close\r\n");
  printf("Content-length: %d\r\n", (int)strlen(content));
  printf("Content-type: text/html\r\n\r\n");
  printf("%s", content);
  fflush(stdout);
  exit(0);

}