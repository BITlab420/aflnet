#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

#include "alloc-inl.h"
#include "aflnet.h"

// Protocol-specific functions for extracting requests and responses

region_t* extract_requests_rtsp(unsigned char* buf, unsigned int buf_size, unsigned int* region_count_ref)
{
  char *mem;
  unsigned int byte_count = 0;
  unsigned int mem_count = 0;
  unsigned int mem_size = 1024;
  unsigned int region_count = 0;
  region_t *regions = NULL;
  char terminator[4] = {0x0D, 0x0A, 0x0D, 0x0A};

  mem=(char *)ck_alloc(mem_size);

  unsigned int cur_start = 0;
  unsigned int cur_end = 0;
  while (byte_count < buf_size) { 
    
    memcpy(&mem[mem_count], buf + byte_count++, 1);

    //Check if the last four bytes are 0x0D0A0D0A
    if ((mem_count > 3) && (memcmp(&mem[mem_count - 3], terminator, 4) == 0)) {
      region_count++;
      regions = (region_t *)ck_realloc(regions, region_count * sizeof(region_t));
      regions[region_count - 1].start_byte = cur_start;
      regions[region_count - 1].end_byte = cur_end;
      regions[region_count - 1].state_sequence = NULL;
      regions[region_count - 1].state_count = 0;
        
      mem_count = 0;  
      cur_start = cur_end + 1;
      cur_end = cur_start;
    } else {
      mem_count++; 
      cur_end++;  

      //Check if the last byte has been reached
      if (cur_end == buf_size - 1) {
        region_count++;
        regions = (region_t *)ck_realloc(regions, region_count * sizeof(region_t));
        regions[region_count - 1].start_byte = cur_start;
        regions[region_count - 1].end_byte = cur_end;
        regions[region_count - 1].state_sequence = NULL;
        regions[region_count - 1].state_count = 0;
        break;
      }

      if (mem_count == mem_size) {
        //enlarge the mem buffer
        mem_size = mem_size * 2;
        mem=(char *)ck_realloc(mem, mem_size);
      }
    }
  }
  if (mem) ck_free(mem);

  //in case region_count equals zero, it means that the structure of the buffer is broken 
  //hence we create one region for the whole buffer
  if ((region_count == 0) && (buf_size > 0)) {
    regions = (region_t *)ck_realloc(regions, sizeof(region_t));
    regions[0].start_byte = 0;
    regions[0].end_byte = buf_size - 1;
    regions[0].state_sequence = NULL;
    regions[0].state_count = 0;

    region_count = 1;
  }
  
  *region_count_ref = region_count;
  return regions;
}

region_t* extract_requests_ftp(unsigned char* buf, unsigned int buf_size, unsigned int* region_count_ref)
{
   char *mem;
  unsigned int byte_count = 0;
  unsigned int mem_count = 0;
  unsigned int mem_size = 1024;
  unsigned int region_count = 0;
  region_t *regions = NULL;
  char terminator[2] = {0x0D, 0x0A};

  mem=(char *)ck_alloc(mem_size);

  unsigned int cur_start = 0;
  unsigned int cur_end = 0;
  while (byte_count < buf_size) { 
    
    memcpy(&mem[mem_count], buf + byte_count++, 1);

    //Check if the last two bytes are 0x0D0A
    if ((mem_count > 1) && (memcmp(&mem[mem_count - 1], terminator, 2) == 0)) {
      region_count++;
      regions = (region_t *)ck_realloc(regions, region_count * sizeof(region_t));
      regions[region_count - 1].start_byte = cur_start;
      regions[region_count - 1].end_byte = cur_end;
      regions[region_count - 1].state_sequence = NULL;
      regions[region_count - 1].state_count = 0;
        
      mem_count = 0;  
      cur_start = cur_end + 1;
      cur_end = cur_start;
    } else {
      mem_count++; 
      cur_end++;  

      //Check if the last byte has been reached
      if (cur_end == buf_size - 1) {
        region_count++;
        regions = (region_t *)ck_realloc(regions, region_count * sizeof(region_t));
        regions[region_count - 1].start_byte = cur_start;
        regions[region_count - 1].end_byte = cur_end;
        regions[region_count - 1].state_sequence = NULL;
        regions[region_count - 1].state_count = 0;
        break;
      }

      if (mem_count == mem_size) {
        //enlarge the mem buffer
        mem_size = mem_size * 2;
        mem=(char *)ck_realloc(mem, mem_size);
      }
    }
  }
  if (mem) ck_free(mem);

  //in case region_count equals zero, it means that the structure of the buffer is broken 
  //hence we create one region for the whole buffer
  if ((region_count == 0) && (buf_size > 0)) {
    regions = (region_t *)ck_realloc(regions, sizeof(region_t));
    regions[0].start_byte = 0;
    regions[0].end_byte = buf_size - 1;
    regions[0].state_sequence = NULL;
    regions[0].state_count = 0;

    region_count = 1;
  }
  
  *region_count_ref = region_count;
  return regions;
}

static char dtls_version[2] = {0xFE, 0xFD};

void hexdump(unsigned char *msg, unsigned char * buf, int start, int end) {
  printf("%s : ", msg);
  for (int i=start; i<=end; i++) {
      printf("%02x", buf[i]);
    }
    printf("\n");
}

region_t *extract_requests_dtls(unsigned char* buf, unsigned int buf_size, unsigned int* region_count_ref) {
  unsigned int byte_count = 0;
  unsigned int region_count = 0;
  region_t *regions = NULL;

  unsigned int cur_start = 0;

   while (byte_count < buf_size) { 
     //Check if the first three bytes are <valid_content_type><dtls-1.2>
     if ((byte_count > 3 && buf_size - byte_count > 1) && 
     ( (buf[byte_count] == HS_CONTENT_TYPE || buf[byte_count] == CCS_CONTENT_TYPE || buf[byte_count] == ALERT_CONTENT_TYPE)  && 
     memcmp(&buf[byte_count+1], dtls_version, 2) == 0)) {
       region_count++;
       regions = (region_t *)ck_realloc(regions, region_count * sizeof(region_t));
       regions[region_count - 1].start_byte = cur_start;
       regions[region_count - 1].end_byte = byte_count-1;
       regions[region_count - 1].state_sequence = NULL;
       regions[region_count - 1].state_count = 0;
       cur_start = byte_count;
       //hexdump("region", buf, regions[region_count - 1].start_byte, regions[region_count - 1].end_byte );
     } else { 

      //Check if the last byte has been reached
      if (byte_count == buf_size - 1) {
        region_count++;
        regions = (region_t *)ck_realloc(regions, region_count * sizeof(region_t));
        regions[region_count - 1].start_byte = cur_start;
        regions[region_count - 1].end_byte = byte_count;
        regions[region_count - 1].state_sequence = NULL;
        regions[region_count - 1].state_count = 0;
        break;
      }
     }

     byte_count ++;
  }

  //in case region_count equals zero, it means that the structure of the buffer is broken 
  //hence we create one region for the whole buffer
  if ((region_count == 0) && (buf_size > 0)) {
    regions = (region_t *)ck_realloc(regions, sizeof(region_t));
    regions[0].start_byte = 0;
    regions[0].end_byte = buf_size - 1;
    regions[0].state_sequence = NULL;
    regions[0].state_count = 0;

    region_count = 1;
  }
  
  *region_count_ref = region_count;
  return regions;
}

unsigned int read_bytes(unsigned char* buf, unsigned int offset, int num_bytes) {
  if (num_bytes > 0 && num_bytes < 5) {
    unsigned int val = 0;
    for (int i=0; i<num_bytes; i++) {
      val = (val << 8) + buf[i+offset];
    }
    return val;
  } else {
    exit(-1);
    return 0;
  }
}


// a status code comprises <content_type, message_type> tuples
unsigned int* extract_response_codes_dtls(unsigned char* buf, unsigned int buf_size, unsigned int* state_count_ref) 
{
  unsigned int byte_count = 0;
  unsigned int *state_sequence = NULL;
  unsigned int state_count = 0;
  unsigned int status_code = 0; // unknown status code

  unsigned int unknown_content_type = 0xFF; // when content type is not known, either due to bug in this method or in the server
  unsigned int unknown_msg_type = 0xFF; // when but message type cannot be determined (because the message is encrypted)
  unsigned int malformed_msg_type = 0xFE; // when message type appear malformed

  unsigned int hs_msg_type;

  // hexdump("Extracting status codes from from bytes:\n", buf, 0, buf_size-1);

  state_count++;
  state_sequence = (unsigned int *)ck_realloc(state_sequence, state_count * sizeof(unsigned int));
  state_sequence[state_count - 1] = 0;

  while (byte_count < buf_size) {
    if ( (buf_size - byte_count > 13) && (memcmp(&buf[byte_count+1], dtls_version, 2) == 0)) {
      unsigned int record_length = read_bytes(buf, byte_count+11, 2);
      unsigned char content_type = read_bytes(buf, byte_count, 1);
      switch(content_type) {
        case HS_CONTENT_TYPE:
          hs_msg_type = read_bytes(buf, byte_count+13, 1);
          if (buf_size - byte_count > 24) {
            unsigned int frag_length = read_bytes(buf, byte_count+22, 3);
            // assuming every record contains a single fragment, we can check if the handshake record is encrypted
            // comparing their respective lengths
            if (record_length - frag_length == 12) {
              // not encrypted
              status_code = (content_type << 8) + hs_msg_type;
            } else {
              // encrypted handshake message
              status_code = (content_type << 8) + unknown_msg_type;
            }
          } else {
              // encrypted, though unexpected. It means the response is malformed (either that, or our way of processing it)
              status_code = (content_type << 8) + malformed_msg_type;
          }
        break;
        case CCS_CONTENT_TYPE:
          if (record_length == 1) {
            // unencrypted CCS
            unsigned int ccs_msg_type = read_bytes(buf, byte_count+13, 1);
            status_code = (content_type << 8) + ccs_msg_type;
          } else {
            // encrypted CCS
            status_code = (content_type << 8) + unknown_msg_type;
          }
        break;
        case ALERT_CONTENT_TYPE:
          if (record_length == 2) {
            // unencrypted alert, the type is sufficient for determining which alert occurred
            unsigned int level = read_bytes(buf, byte_count+13, 1);
            unsigned int type = read_bytes(buf, byte_count+14, 1);
            status_code = (content_type << 8) + type;
          } else {
            // encrypted alert
            status_code = (content_type << 8) + unknown_msg_type;
          }
        break;
        default:
          // unknown message type
          status_code = (unknown_content_type << 8) + unknown_msg_type;
        break;
      }
      state_count++;
      state_sequence = (unsigned int *)ck_realloc(state_sequence, state_count * sizeof(unsigned int));
      state_sequence[state_count - 1] = status_code;
      byte_count += record_length;
    } else {
      // we shouldn't really be reaching this code
      byte_count ++;
    }
  }

  // printf("Responses received:\n");
  // for (int i=0; i<state_count; i++) {
  //   printf("%04X ", state_sequence[i]);
  // }
  // printf("\n");

  *state_count_ref = state_count;
  return state_sequence;
}

unsigned int* extract_response_codes_rtsp(unsigned char* buf, unsigned int buf_size, unsigned int* state_count_ref)
{
  char *mem;
  unsigned int byte_count = 0;
  unsigned int mem_count = 0;
  unsigned int mem_size = 1024;
  unsigned int *state_sequence = NULL;
  unsigned int state_count = 0;
  char terminator[2] = {0x0D, 0x0A};
  char rtsp[5] = {0x52, 0x54, 0x53, 0x50, 0x2f};

  mem=(char *)ck_alloc(mem_size);

  state_count++;
  state_sequence = (unsigned int *)ck_realloc(state_sequence, state_count * sizeof(unsigned int));
  state_sequence[state_count - 1] = 0;

  while (byte_count < buf_size) { 
    memcpy(&mem[mem_count], buf + byte_count++, 1);

    //Check if the last two bytes are 0x0D0A
    if ((mem_count > 0) && (memcmp(&mem[mem_count - 1], terminator, 2) == 0)) {
      if ((mem_count >= 5) && (memcmp(mem, rtsp, 5) == 0)) {
        //Extract the response code which is the first 3 bytes
        char temp[4];
        memcpy(temp, &mem[9], 4);
        temp[3] = 0x0;
        unsigned int message_code = (unsigned int) atoi(temp);

        if (message_code == 0) break;

        state_count++;
        state_sequence = (unsigned int *)ck_realloc(state_sequence, state_count * sizeof(unsigned int));
        state_sequence[state_count - 1] = message_code;
        mem_count = 0;
      } else {
        mem_count = 0;
      }
    } else {
      mem_count++;   
      if (mem_count == mem_size) {
        //enlarge the mem buffer
        mem_size = mem_size * 2;
        mem=(char *)ck_realloc(mem, mem_size);
      }
    }
  }
  if (mem) ck_free(mem);
  *state_count_ref = state_count;
  return state_sequence;
}

unsigned int* extract_response_codes_ftp(unsigned char* buf, unsigned int buf_size, unsigned int* state_count_ref)
{
  char *mem;
  unsigned int byte_count = 0;
  unsigned int mem_count = 0;
  unsigned int mem_size = 1024;
  unsigned int *state_sequence = NULL;
  unsigned int state_count = 0;
  char terminator[2] = {0x0D, 0x0A};

  mem=(char *)ck_alloc(mem_size);

  state_count++;
  state_sequence = (unsigned int *)ck_realloc(state_sequence, state_count * sizeof(unsigned int));
  state_sequence[state_count - 1] = 0;

  while (byte_count < buf_size) { 
    memcpy(&mem[mem_count], buf + byte_count++, 1);

    if ((mem_count > 0) && (memcmp(&mem[mem_count - 1], terminator, 2) == 0)) {
      //Extract the response code which is the first 3 bytes
      char temp[4];
      memcpy(temp, mem, 4);
      temp[3] = 0x0;
      unsigned int message_code = (unsigned int) atoi(temp);

      if (message_code == 0) break;

      state_count++;
      state_sequence = (unsigned int *)ck_realloc(state_sequence, state_count * sizeof(unsigned int));
      state_sequence[state_count - 1] = message_code;
      mem_count = 0;
    } else {
      mem_count++;   
      if (mem_count == mem_size) {
        //enlarge the mem buffer
        mem_size = mem_size * 2;
        mem=(char *)ck_realloc(mem, mem_size);
      }
    }
  }
  if (mem) ck_free(mem);
  *state_count_ref = state_count;
  return state_sequence;
}

// kl_messages manipulating functions

klist_t(lms) *construct_kl_messages(u8* fname, region_t *regions, u32 region_count) 
{
  FILE *fseed = NULL;
  fseed = fopen(fname, "rb"); 
  if (fseed == NULL) PFATAL("Cannot open seed file %s", fname);
  
  klist_t(lms) *kl_messages = kl_init(lms);
  u32 i;

  for (i = 0; i < region_count; i++) {
    //Identify region size
    u32 len = regions[i].end_byte - regions[i].start_byte + 1;

    //Create a new message
    message_t *m = (message_t *) ck_alloc(sizeof(message_t));
    m->mdata = (char *) ck_alloc(len);
    m->msize = len;  
    if (m->mdata == NULL) PFATAL("Unable to allocate memory region to store new message");          
    fread(m->mdata, 1, len, fseed);

    //Insert the message to the linked list
    *kl_pushp(lms, kl_messages) = m;
  }

  if (fseed != NULL) fclose(fseed);
  return kl_messages;
}

void delete_kl_messages(klist_t(lms) *kl_messages) 
{ 
  /* Free all messages in the list before destroying the list itself */
  message_t *m;
  
  int ret = kl_shift(lms, kl_messages, &m);
  while (ret == 0) {
    if (m) {
      ck_free(m->mdata);
      ck_free(m);
    }
    ret = kl_shift(lms, kl_messages, &m);
  }
  
  /* Finally, destroy the list */
	kl_destroy(lms, kl_messages);
}

kliter_t(lms) *get_last_message(klist_t(lms) *kl_messages) 
{
  kliter_t(lms) *it;
  it = kl_begin(kl_messages);
  while (kl_next(it) != kl_end(kl_messages)) {
    it = kl_next(it);
  }
  return it;
}


u32 save_kl_messages_to_file(klist_t(lms) *kl_messages, u8 *fname, u8 replay_enabled, u32 max_count) 
{
  u8 *mem = NULL;
  u32 len = 0, message_size = 0;
  kliter_t(lms) *it;

  s32 fd = open(fname, O_WRONLY | O_CREAT, 0600);
  if (fd < 0) PFATAL("Unable to create file '%s'", fname);

  u32 message_count = 0;
  //Iterate through all messages in the linked list
  for (it = kl_begin(kl_messages); it != kl_end(kl_messages) && message_count < max_count; it = kl_next(it)) {
    message_size = kl_val(it)->msize;
    if (replay_enabled) {
		  mem = (u8 *)ck_realloc(mem, 4 + len + message_size);

      //Save packet size first
      u32 *psize = (u32*)&mem[len];
      *psize = message_size;

      //Save packet content 
      memcpy(&mem[len + 4], kl_val(it)->mdata, message_size);
      len = 4 + len + message_size;
    } else {
      mem = (u8 *)ck_realloc(mem, len + message_size);

      //Save packet content 
      memcpy(&mem[len], kl_val(it)->mdata, message_size);
      len = len + message_size;
    }
    message_count++;
  }  

  //Write everything to file & close the file
  ck_write(fd, mem, len, fname);
  close(fd);

  //Free the temporary buffer
  ck_free(mem);

  return len;
}

region_t* convert_kl_messages_to_regions(klist_t(lms) *kl_messages, u32* region_count_ref, u32 max_count) 
{
  region_t *regions = NULL;
  kliter_t(lms) *it;

  u32 region_count = 1;
  s32 cur_start = 0, cur_end = 0;
  //Iterate through all messages in the linked list
  for (it = kl_begin(kl_messages); it != kl_end(kl_messages) && region_count <= max_count ; it = kl_next(it)) {
    regions = (region_t *)ck_realloc(regions, region_count * sizeof(region_t));

    cur_end = cur_start + kl_val(it)->msize - 1;
    if (cur_end < 0) PFATAL("End_byte cannot be negative");

    regions[region_count - 1].start_byte = cur_start;
    regions[region_count - 1].end_byte = cur_end;
    regions[region_count - 1].state_sequence = NULL;
    regions[region_count - 1].state_count = 0;
    
    cur_start = cur_end + 1;
    region_count++;
  }  
  
  *region_count_ref = region_count - 1; 
  return regions;
}

// Network communication functions

int net_send(int sockfd, struct timeval timeout, char *mem, unsigned int len) {
  unsigned int byte_count = 0;
  int n;
  struct pollfd pfd[1]; 
  pfd[0].fd = sockfd;
  pfd[0].events = POLLOUT;
  int rv = poll(pfd, 1, 1);  

  setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout));
  if (rv > 0) {
    if (pfd[0].revents & POLLOUT) {
      while (byte_count < len) {
        usleep(10);
        n = send(sockfd, &mem[byte_count], len - byte_count, MSG_NOSIGNAL);
        if (n == 0) return byte_count;
        if (n == -1) return -1;
        byte_count += n;
      }
    }
  }
  return byte_count;
}

int net_recv(int sockfd, struct timeval timeout, int poll_w, char **response_buf, unsigned int *len) {
  char temp_buf[1000];
  int n;
  struct pollfd pfd[1]; 
  pfd[0].fd = sockfd;
  pfd[0].events = POLLIN;
  int rv = poll(pfd, 1, poll_w);
  
  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
  if (rv > 0) {
    if (pfd[0].revents & POLLIN) {
      n = recv(sockfd, temp_buf, sizeof(temp_buf), 0);
      if ((n < 0) && (errno != 11)) {
        return 1;
      }
      while (n > 0) {
        usleep(10);
        *response_buf = (unsigned char *)ck_realloc(*response_buf, *len + n);
        memcpy(&(*response_buf)[*len], temp_buf, n);
        *len = *len + n;
        n = recv(sockfd, temp_buf, sizeof(temp_buf), 0);
        if ((n < 0) && (errno != 11)) {
          return 1;
        }
      }   
    }
  } else if (rv < 0) return 1;
  return 0;
}

// Utility function

void save_regions_to_file(region_t *regions, unsigned int region_count, unsigned char *fname)
{
  int fd;
  FILE* fp;

  fd = open(fname, O_WRONLY | O_CREAT | O_EXCL, 0600);
  
  if (fd < 0) return;

  fp = fdopen(fd, "w");

  if (!fp) {
    close(fd);
    return;
  }

  int i;
  
  for(i=0; i < region_count; i++) {
     fprintf(fp, "Region %d - Start: %d, End: %d\n", i, regions[i].start_byte, regions[i].end_byte);
  }

  fclose(fp);
}

int str_split(char* a_str, const char* a_delim, char **result, int a_count)
{
	char *token;
	int count = 0;

	/* count number of tokens */
	/* get the first token */
	char* tmp1 = strdup(a_str);
	token = strtok(tmp1, a_delim);

	/* walk through other tokens */
	while (token != NULL)
	{
		count++;
		token = strtok(NULL, a_delim);
	}

	if (count != a_count)
	{
		return 1;
	}

	/* split input string, store tokens into result */
	count = 0;
	/* get the first token */
	token = strtok(a_str, a_delim);

	/* walk through other tokens */

	while (token != NULL)
	{
		result[count] = token;
		count++;
		token = strtok(NULL, a_delim);
	}

	free(tmp1);
	return 0;
}

void str_rtrim(char* a_str)
{
	char* ptr = a_str;
	int count = 0;
	while ((*ptr != '\n') && (*ptr != '\t') && (*ptr != ' ') && (count < strlen(a_str))) {
		ptr++;
		count++;
	}
	if (count < strlen(a_str)) {
		*ptr = '\0';
	}
}

int parse_net_config(u8* net_config, u8* protocol, u8** ip_address, u32* port) 
{
  char  buf[80];
  char **tokens;
  int tokenCount = 3;

  tokens = (char**)malloc(sizeof(char*) * (tokenCount));

  if (strlen(net_config) > 80) return 1;

  strncpy(buf, net_config, strlen(net_config));
   str_rtrim(buf);
      
  if (!str_split(buf, "/", tokens, tokenCount))
  {
      if (!strcmp(tokens[0], "tcp:")) {
        *protocol = PRO_TCP;
      } else if (!strcmp(tokens[0], "udp:")) {
        *protocol = PRO_UDP;
      } else return 1;

      //TODO: check the format of this IP address
      *ip_address = strdup(tokens[1]);

      *port = atoi(tokens[2]);
      if (*port == 0) return 1;
  }
  return 0;
}

u8* state_sequence_to_string(unsigned int *stateSequence, unsigned int stateCount) {
  u32 i = 0;
 
  u8 *out = NULL;
  
  char strState[10];
  int len = 0;
  for (i = 0; i < stateCount; i++) {
    //Limit the loop to shorten the output string
    if ((i >= 2) && (stateSequence[i] == stateSequence[i - 1]) && (stateSequence[i] == stateSequence[i - 2])) continue;
    unsigned int stateID = stateSequence[i];
    if (i == stateCount - 1) {
      sprintf(strState, "%d", (int) stateID);
    } else {
      sprintf(strState, "%d-", (int) stateID);
    }
    out = (u8 *)ck_realloc(out, len + strlen(strState) + 1);
    memcpy(&out[len], strState, strlen(strState) + 1);
    len=strlen(out);
    //As Linux limit the size of the file name
    //we set a fixed upper bound here
    if (len > 150 && (i + 1 < stateCount)) {
      sprintf(strState, "%s", "end-at-");
      out = (u8 *)ck_realloc(out, len + strlen(strState) + 1);
      memcpy(&out[len], strState, strlen(strState) + 1);
      len=strlen(out);

      sprintf(strState, "%d", (int) stateSequence[stateCount - 1]);
      out = (u8 *)ck_realloc(out, len + strlen(strState) + 1);
      memcpy(&out[len], strState, strlen(strState) + 1);
      len=strlen(out);
      break;
    }
  }
  return out;
}

