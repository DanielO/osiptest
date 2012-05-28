/* 
 * File:   main.c
 * Author: Soufiane Tarari
 *
 * Created on May 24, 2012, 2:04 PM
 */

#ifdef ENABLE_MPATROL
#include <mpatrol.h>
#endif


#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#include <osip2/osip.h>
#include <osip2/osip_fifo.h>
//#include <osipparser2/osip_port.h>
#include <osip2/osip_mt.h>
#include <osipparser2/osip_const.h>
//#include <osipparser2/sdp_message.h>

#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <assert.h>

//extern osip_fifo *ff;
static const size_t BUFFSIZE = 1600;

typedef struct _udp_transport_layer_t {
  int in_port;
  int in_socket;
#ifdef OSIP_MT
  sthread_t *thread;
#endif

  int out_port;
  int out_socket;

  int control_fds[2];
} udp_transport_layer_t;

udp_transport_layer_t *_udp_tl;

int cb_udp_snd_message(osip_transaction_t *tr, osip_message_t *sip, char *host, int port, int out_socket)
{
  printf("cb_udp_snd_message\n");
  int num = 0;
  int i;
  char* message;
  
  struct sockaddr_in addr;
  unsigned long int  one_inet_addr;
  int sock;
  size_t bufLen;
  
  i = osip_message_to_str(sip, &message, &bufLen);

  if (i!=0) return -1;

  osip_via_t * via = (osip_via_t *) osip_list_get (&sip->vias, 0);
  addr.sin_port        = htons(atoi(via->port));
  addr.sin_family      = AF_INET;

  sock=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  inet_aton(via->host, &addr.sin_addr);
  
  if (0  > sendto (sock, message, BUFFSIZE, 0, (struct sockaddr *) &addr, sizeof(addr))) 
    {

      if (ECONNREFUSED==errno)
	{
	  OSIP_TRACE(osip_trace(__FILE__,__LINE__,OSIP_INFO3,stdout, "SIP_ECONNREFUSED - No remote server.\n"));
	  /* This can be considered as an error, but for the moment,
	     I prefer that the application continue to try sending
	     message again and again... so we are not in a error case.
	     Nevertheless, this error should be announced!
	     ALSO, UAS may not have any other options than retry always
	     on the same port.
	  */
	  return 1;
	}
      else
	{
	  OSIP_TRACE(osip_trace(__FILE__,__LINE__,OSIP_INFO3,stdout, "SIP_NETWORK_ERROR - Network error.\n"));
	  /* SIP_NETWORK_ERROR; */
	  return -1;
	}
    }
  
  printf("\n<--- Transmitting to %s:%s --->\n", via->host, via->port);
  //osip_message_to_str(response, &buf, &bufLen);
  printf("%s\n", message);
    
  if (strncmp(message, "INVITE", 6)==0)
    {
      num++;
      fprintf(stdout, "number of message sent: %i\n", num);
    }

  OSIP_TRACE(osip_trace(__FILE__,__LINE__,OSIP_INFO2, NULL, "SND UDP MESSAGE.\n"));
  printf("end cb_udp_snd_message\n");
  return 0;
}

void cb_RcvICTRes(int i, osip_transaction_t *tr, osip_message_t *message)
{
        printf("cb_RcvICTRes fired\n");
}

void cb_RcvNICTRes(int i, osip_transaction_t *tr, osip_message_t *message)
{
        printf("cb_RcvNICTRes fired\n");
}

void cb_RcvNot(int i, osip_transaction_t *tr, osip_message_t *message)
{
    //OSIP_TRACE (osip_trace(__FILE__, __LINE__, OSIP_INFO1, NULL, "psp_core_cb_rcvregister!\n"));   
    //psp_core_event_add_sfp_inc_traffic(tr);
    printf("NOTIFY SIP message received\n");
}

void cb_ISTTranKill(int i, osip_transaction_t *osip_transaction)
{
        printf("cb_ISTTranKill fired\n");
}

void* Notify(void* arg)
{       
    printf("Notify\n");
        osip_transaction_t *tran = (osip_transaction_t*)arg;
        
        osip_message_t *response = NULL;        
        osip_event_t *evt = NULL;
        BuildResponse(tran->orig_request, &response);

        printf("incoming call from %s\n [a]nswer or [d]ecline?\n", (char *)tran->from->url);
        char act = 'd';
        scanf("%c", &act);
        if('a' == act)
        {//accept call
                osip_message_set_status_code(response, SIP_OK);
                osip_message_set_reason_phrase(response, osip_strdup("OK"));
                const char* mime = "application/sdp";
                osip_message_set_body_mime(response, mime, strlen(mime));
                osip_message_set_content_type(response, mime);
                const char* sdp = "v=0\r\n\
o=raymond 323456 654323 IN IP4 127.0.0.1\r\n\
s=A conversation\r\n\
c=IN IP4 127.0.0.1\r\n\
t=0 0\r\n\
m=audio 7078 RTP/AVP 111 110 0 8 101\r\n\
a=rtpmap:111 speex/16000/1\r\n\
a=rtpmap:110 speex/8000/1\r\n\
a=rtpmap:0 PCMU/8000/1\r\n\
a=rtpmap:8 PCMA/8000/1\r\n\
a=rtpmap:101 telephone-event/8000\r\n\
a=fmtp:101 0-11\r\n";
                
                osip_message_set_body(response, osip_strdup(sdp), strlen(sdp));
        }
        else
        {//decline call
                osip_message_set_status_code(response, SIP_DECLINE);
                osip_message_set_reason_phrase(response, osip_strdup("Decline\n"));
                osip_message_set_contact(response, NULL);
        }

        evt = osip_new_outgoing_sipmessage(response);
        evt->transactionid = tran->transactionid;
        osip_transaction_add_event(tran, evt);
        return NULL;
}

void cb_RcvSub(int i, osip_transaction_t *tran, osip_message_t *msg){
    printf("SUBSCRIBE SIP message received\n");
}

void cb_RcvRegisterReq(int i, osip_transaction_t *tran, osip_message_t *msg){
    osip_via_t * via = (osip_via_t *) osip_list_get (&msg->vias, 0);
    char* buf;
    size_t bufLen;
    osip_message_t *response = NULL;   
    osip_event_t *evt = NULL;
    
    printf("\n<--- Received from %s:%s --->\n", via->host, via->port);
    osip_message_to_str(msg, &buf, &bufLen);
    printf("%s\n", buf);
    
    BuildResponse(msg, &response);
    osip_message_set_status_code(response, SIP_TRYING);
    evt = osip_new_outgoing_sipmessage (response);   
    osip_message_set_reason_phrase(response, osip_strdup("Trying"));
    osip_transaction_add_event(tran, evt);
    
    /*BuildReq(msg, &response);
    osip_message_set_method(response, osip_strdup("OPTIONS"));
    //osip_call_id_init(&response->call_id);
    //osip_call_id_set_host(response->call_id, osip_strdup("172.16.156.1"));
    //osip_call_id_set_number(response->call_id, osip_strdup("1c6d6ad5155cc32b4407635d08c6ff13"));
    osip_message_set_content_length(response, 0);
    evt = osip_new_outgoing_sipmessage (response);
    osip_transaction_add_event(tran, evt);*/
    
    BuildResponse(msg, &response);
    osip_message_set_status_code(response, SIP_OK);
    evt = osip_new_outgoing_sipmessage (response);   
    osip_message_set_reason_phrase(response, osip_strdup("OK"));
    osip_transaction_add_event(tran, evt);
    
    printf("end RcvRegisterReq\n");
}


void cb_RcvISTReq(int i, osip_transaction_t *tran, osip_message_t *msg)
{       
    osip_via_t * via = (osip_via_t *) osip_list_get (&msg->vias, 0);
    char* buf;
    size_t bufLen;
    osip_message_t *response = NULL;        
    osip_event_t *evt = NULL;
    
    printf("<--- INVITE SIP message received from %s:%s --->\n",  via->host, via->port);
    osip_message_to_str(msg, &buf, &bufLen);
    printf("%s\n", buf);
        

        BuildResponse(msg, &response); //trying
        osip_message_set_status_code(response, SIP_TRYING);
        evt = osip_new_outgoing_sipmessage(response);   
        osip_message_set_reason_phrase(response, osip_strdup("Trying\n"));
        osip_transaction_add_event(tran, evt);
        
        BuildResponse(msg, &response); //dialog establishement
        osip_message_set_status_code(response, 101);
        evt = osip_new_outgoing_sipmessage(response);   
        osip_message_set_reason_phrase(response, osip_strdup("Dialog Establishement\n"));
        osip_transaction_add_event(tran, evt);

        BuildResponse(msg, &response); //ringing
        osip_message_set_status_code(response, SIP_RINGING);
        evt = osip_new_outgoing_sipmessage(response);   
        osip_message_set_reason_phrase(response, osip_strdup("Ringing\n"));
        osip_transaction_add_event(tran, evt);

        //osip_thread_create(0, Notify, tran); // start another thread to notify user the incoming call
}

int BuildReq(const osip_message_t *RCVrequest, osip_message_t **SNDrequest){
    printf("BuildReq\n");
    osip_via_t * viaReq = (osip_via_t *) osip_list_get (&RCVrequest->vias, 0);
        osip_message_t *msg = NULL;
        osip_message_init(&msg);
        osip_uri_t *uri;
        osip_uri_t *uri2;
                        
        osip_from_init(&msg->from); //allocating and setting the From field
        osip_uri_init(&uri);
        osip_uri_set_host(uri, osip_strdup("translator@172.16.156.1"));
        osip_from_set_url(msg->from, uri);        
        osip_from_set_displayname(msg->from, osip_strdup("\"Translator\""));
        osip_from_set_tag(msg->from, osip_strdup("123456"));
                
        osip_to_clone(RCVrequest->to, &msg->to);
        osip_message_set_contact(msg, osip_strdup("<sip:translator@172.16.156.1>"));        

        osip_via_t *via;
        osip_via_init(&via);
        via->host = osip_strdup("172.16.156.1");
        via->protocol = osip_strdup("UDP");
        via->version = osip_strdup("2.0");
        via->port = osip_strdup("5040");
        osip_list_add (&(msg->vias), via, -1);
        
        osip_message_set_version(msg, osip_strdup("SIP/2.0"));
        
        osip_uri_init(&uri2);
        osip_uri_set_host(uri2, viaReq->host);
        osip_uri_set_port(uri2, viaReq->port);
        osip_message_set_uri(msg, uri2);
        
        osip_message_set_user_agent(msg, osip_strdup("Soufiane/1.0.0 PBX"));
        osip_message_set_status_code(msg, 0);
        osip_message_set_reason_phrase(msg, NULL);
        osip_message_set_max_forwards(msg, "70");
        
        int pos = 0;//copy vias from request to response
        while (!osip_list_eol (&RCVrequest->allows, pos))
        {
                osip_allow_t *allow;
                osip_allow_t *allow2;

                allow = (osip_allow_t *) osip_list_get (&RCVrequest->allows, pos);
                int i = osip_allow_clone (allow, &allow2);
                if (i != 0)
                {
                        osip_message_free (msg);
                        return i;
                }
                osip_list_add (&(msg->allows), allow2, -1);
                pos++;
        }
        
        struct tm *loctime;
        time_t curtime;
        time(&curtime);
        loctime=localtime(&curtime);
        osip_message_set_date(msg, asctime(loctime));
        
        osip_message_set_supported(msg, "replaces");
        
        *SNDrequest = msg;
        printf("end BuildReq\n");
        return 0;
}

int BuildResponse(const osip_message_t *request, osip_message_t **response)
{
    printf("BuildResponse\n");
        osip_message_t *msg = NULL;
        osip_message_init(&msg);

        osip_from_clone(request->from, &msg->from);
        osip_to_clone(request->to, &msg->to);
        osip_cseq_clone(request->cseq, &msg->cseq);
        osip_call_id_clone(request->call_id, &msg->call_id);

        int pos = 0;//copy vias from request to response
        while (!osip_list_eol (&request->vias, pos))
        {
                osip_via_t *via;
                osip_via_t *via2;

                via = (osip_via_t *) osip_list_get (&request->vias, pos);
                int i = osip_via_clone (via, &via2);
                if (i != 0)
                {
                        osip_message_free (msg);
                        return i;
                }
                osip_list_add (&(msg->vias), via2, -1);
                pos++;
        }
        
        osip_to_set_tag(msg->to, osip_strdup("4893693")); // set to tag in response
        osip_message_set_version(msg, osip_strdup("SIP/2.0"));
        osip_contact_t* contact;
        osip_contact_clone((osip_contact_t*)osip_list_get(&(request->contacts), 0), &contact);
        osip_list_add(&(msg->contacts), contact, 0);
        osip_message_set_user_agent(msg, osip_strdup("Soufiane/1.0.0 PBX"));
        *response = msg;
        
        printf("end BuildResponse\n");
        
        return 0;
}

void ProcessNewReq(osip_t* osip, osip_event_t *evt)
{
    printf("ProcessNewReq\n");
        osip_transaction_t *tran;
        int g_sock;
        osip_transaction_init(&tran, NIST, osip, evt->sip);
        osip_transaction_set_in_socket (tran, g_sock);
        //osip_transaction_set_out_socket (tran, g_sock);
        osip_transaction_set_your_instance(tran, osip);// store osip in transaction for later usage
        osip_transaction_add_event(tran, evt);
        printf("end ProcessNewReq\n");
}

void* TransportFun(void* arg)
{
    printf("TransportFun\n");
        int rc;
        osip_t* osip = (osip_t*)arg;
                
        printf("Initialize network\n");
        int g_sock = InitNet();     
        assert(0 < g_sock);
        char buf[BUFFSIZE];
        while(1)
        {
                struct sockaddr from;
                int addrSize = sizeof(struct sockaddr);
                int len = recvfrom(g_sock, buf, BUFFSIZE, 0, &from, &addrSize);
                if(len < 1){
                    continue;
                }
                buf[len] = 0;
                osip_event_t *evt = osip_parse(buf, len);
                rc = osip_find_transaction_and_add_event(osip, evt);
                if(0 != rc)
                {
                        printf("this event has no transaction, create a new one.\n");
                        ProcessNewReq(osip, evt);
                }
                printf("******** this is the osip after: %d, method: %s, tr id: %d\n\n", osip->osip_nist_transactions.nb_elt, evt->sip->sip_method, evt->transactionid);
        }
        return NULL;
}

int InitNet(){
    printf("InitNet\n");
    struct sockaddr_in param;
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    
    param.sin_family = AF_INET;
    param.sin_port = htons(5040);
    param.sin_addr.s_addr = inet_addr("0.0.0.0");
    
    if( bind (sock, (struct sockaddr*)&param, sizeof(param)) == -1){
        perror("bind");
    }
    printf("Network ready\n");
    return(sock);
}

void SetCallbacks(osip_t *osip)
{
    printf("SetCallbacks\n");
    osip_set_cb_send_message(osip, &cb_udp_snd_message);
    osip_set_message_callback(osip, OSIP_IST_INVITE_RECEIVED, &cb_RcvISTReq);
    osip_set_message_callback(osip, OSIP_IST_INVITE_RECEIVED_AGAIN, &cb_RcvISTReq);
    osip_set_message_callback(osip, OSIP_ICT_STATUS_1XX_RECEIVED, &cb_RcvICTRes);
    osip_set_message_callback(osip, OSIP_NICT_STATUS_1XX_RECEIVED, &cb_RcvNICTRes);
    osip_set_message_callback(osip, OSIP_NIST_NOTIFY_RECEIVED, &cb_RcvNot);
    osip_set_message_callback(osip, OSIP_NIST_SUBSCRIBE_RECEIVED, &cb_RcvSub);
    osip_set_message_callback(osip, OSIP_NIST_REGISTER_RECEIVED, &cb_RcvRegisterReq);
    osip_set_kill_transaction_callback(osip, OSIP_IST_KILL_TRANSACTION, &cb_ISTTranKill);
}

int main(int argc, char** argv) {
    /* initializing the parser and the state machine */
    osip_t *osip=NULL;
    if (osip_init(&osip) != 0){
        return -1;
    }
    
    /* Setting the callback functions */
    SetCallbacks(osip);
    
    osip_thread_create(0, TransportFun, osip); 
        while(0 == 0)
        {               
                osip_ict_execute(osip);
                osip_ist_execute(osip);
                osip_nict_execute(osip);
                osip_nist_execute(osip);
                osip_timers_ict_execute(osip);
                osip_timers_ist_execute(osip);
                osip_timers_nict_execute(osip);
                osip_timers_nist_execute(osip);
        }
    return (EXIT_SUCCESS);
}