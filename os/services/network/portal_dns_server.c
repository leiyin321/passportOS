#include "portal_dns_server.h"
#include "portal_dns.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "esp_log.h"
#include <stdatomic.h>
#include <unistd.h>
static TaskHandle_t worker;
static SemaphoreHandle_t done;
static atomic_bool stopping;
static int dns_socket=-1;
static void serve(void *unused){
 (void)unused;uint8_t packet[512];
 while(!atomic_load(&stopping)){
  struct sockaddr_in client;socklen_t size=sizeof(client);
  int n=recvfrom(dns_socket,packet,sizeof(packet),0,(struct sockaddr*)&client,&size);
  if(n<=0)continue;
  /* AP-only binding plus source-subnet restriction prevents STA DNS hijack. */
  if((ntohl(client.sin_addr.s_addr)&0xffffff00u)!=0xc0a80400u)continue;
  size_t bytes=ps_portal_dns_reply(packet,(size_t)n,packet,sizeof(packet));
  if(bytes)sendto(dns_socket,packet,bytes,0,(struct sockaddr*)&client,size);
 }
 close(dns_socket);dns_socket=-1;
 ESP_LOGI("passport","Portal DNS closed stack_free=%u",(unsigned)uxTaskGetStackHighWaterMark(NULL));
 xSemaphoreGive(done);vTaskSuspend(NULL);
}
bool ps_portal_dns_start(void){
 if(worker)return true;
 dns_socket=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);if(dns_socket<0)return false;
 struct timeval timeout={.tv_sec=0,.tv_usec=200000};
 if(setsockopt(dns_socket,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout))!=0){close(dns_socket);dns_socket=-1;return false;}
 struct sockaddr_in address={.sin_family=AF_INET,.sin_port=htons(53),.sin_addr.s_addr=inet_addr("192.168.4.1")};
 if(bind(dns_socket,(struct sockaddr*)&address,sizeof(address))!=0){close(dns_socket);dns_socket=-1;return false;}
 done=xSemaphoreCreateBinary();atomic_store(&stopping,false);
 if(!done||xTaskCreate(serve,"portal_dns",3072,NULL,3,&worker)!=pdPASS){if(done)vSemaphoreDelete(done);done=NULL;close(dns_socket);dns_socket=-1;worker=NULL;return false;}
 return true;
}
void ps_portal_dns_stop(void){
 if(!worker)return;
 atomic_store(&stopping,true);xSemaphoreTake(done,portMAX_DELAY);
 vTaskDelete(worker);worker=NULL;vSemaphoreDelete(done);done=NULL;
}
