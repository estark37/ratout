#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <time.h>
#include <string.h>

#define REQ_FILE_PATH "/Users/estark37/Dropbox/Documents/school/cs294s-2/ratout-demo"
#define DOWNLOAD_URL "https://www.box.net/api/1.0/download/<auth token>/%s"
#define UPLOAD_URL "https://upload.box.net/api/upload/<auth token>/80421720"
#define SET_DESC_URL "https://www.box.net/api/1.0/rest?action=set_description&api_key=<api key>&auth_token=<auth token>&target=file&target_id=%s&description=%s"
#define GET_INFO_URL "https://www.box.net/api/1.0/rest?action=get_file_info&api_key=<api key>&auth_token=<auth token>&file_id=%s"

int upload_resp_size;

struct resp_buffer {
  int len;
  int size;
  char* buf;
};

void download_response(CURL* curl, char* file_id, struct resp_buffer* download_resp) {
  printf("Downloading response for file id %s\n", file_id);

  char download_url[1024];
  sprintf(download_url, DOWNLOAD_URL, file_id);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, NULL);
  curl_easy_setopt(curl, CURLOPT_URL, download_url);
  curl_easy_setopt(curl, CURLOPT_HTTPPOST, NULL);
  curl_easy_setopt(curl, CURLOPT_HTTPGET, 1);
  CURLcode res = curl_easy_perform(curl);
  printf("Downloading from %s\n", download_url);
  printf("%s\n", download_resp->buf);
}

void poll_for_response(CURL* curl, char* file_id, struct resp_buffer* poll_resp) {
  CURLcode res;
  char desc_url[512];
  int keep_polling = 1;
  char id[512];
  strcpy(id, file_id);
  sprintf(desc_url, GET_INFO_URL, id);

  curl_easy_setopt(curl, CURLOPT_URL, desc_url);
  while (keep_polling) {
    poll_resp->len = 0;
    printf("Checking for response... ");
    res = curl_easy_perform(curl);
    printf(" done\n");
    char* description = strstr(poll_resp->buf, "<description>") + strlen("<description>");
    if (description != NULL) {
      char *end_desc = strstr(description, "</description>");
      end_desc[0] = '\0';
      if (strcmp(description, "Processed") == 0) {
	keep_polling = 0;
	poll_resp->len = 0;
	download_response(curl, id, poll_resp);
      }
    }
  }

}

size_t read_api_response(void* ptr, size_t size, size_t nmemb, struct resp_buffer* resp) {
  if (!resp) return 0;
  while (size*nmemb > resp->size - resp->len) {
    // resize buffer
    char* new_buf = malloc(2*resp->size);
    if (!new_buf) {
      printf("Could not allocate new buffer.\n");
      return 0;
    }
    memcpy(new_buf, resp->buf, resp->len);
    free(resp->buf);
    resp->buf = new_buf;
    resp->size = 2*resp->size;
  }

  memcpy(resp->buf+resp->len, ptr, size*nmemb);
  resp->len += size*nmemb;
  resp->buf[resp->len] = '\0';
  return size*nmemb;
}

void create_request_file(char* url, char* fname) {
  FILE* file;
  sprintf(fname, "%s/ratout_%d", REQ_FILE_PATH, rand());
  printf("File name: %s\n", fname);
  file = fopen(fname, "w+");
  if (file) {
    fputs("test", file);
    fclose(file);
  } else printf("Could not create request file\n");
}

int main(int argc, char* argv[]) {

  if (argc != 2) {
    printf("Usage: ./ratout http://url-to-fetch\n");
    return(0);
  }

  char* url = argv[1];
  CURL *curl;
  CURLcode res;
  char filename[512];
  srand(time(NULL));

  curl_global_init(CURL_GLOBAL_ALL);
  printf("URL: %s\n", url);

  create_request_file(url, filename);

  struct curl_httppost *filepost = NULL;
  struct curl_httppost *lastptr = NULL;

  struct resp_buffer upload_resp;
  upload_resp.size = 4096;
  upload_resp.buf = malloc(4096);
  upload_resp.len = 0;

  curl_formadd(&filepost, &lastptr, CURLFORM_COPYNAME, "file", CURLFORM_FILE, filename, CURLFORM_END);
    
  curl = curl_easy_init();
  struct curl_slist *headerlist = NULL;
  headerlist = curl_slist_append(headerlist, "Expect:");
  if (curl) {
    curl_easy_setopt(curl, CURLOPT_URL, UPLOAD_URL);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerlist);
    curl_easy_setopt(curl, CURLOPT_HTTPPOST, filepost);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, read_api_response);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &upload_resp);
    printf("Uploading file... ");
    res = curl_easy_perform(curl);
    printf(" done.\n");

    printf("Got data response:\n");
    printf("%s\n\n", upload_resp.buf);
    upload_resp.len = 0;

    char* id_index = strstr(upload_resp.buf, "id=");
    char* space_index = strstr(id_index, " ");
    space_index[-1] = '\0';
    
    char desc_url[1024];
    sprintf(desc_url, SET_DESC_URL, id_index+strlen("id=\""), url);
    curl_easy_setopt(curl, CURLOPT_URL, desc_url);    
    printf("Setting description... ");
    res = curl_easy_perform(curl);
    printf(" done.\n");
    upload_resp.len = 0;

    poll_for_response(curl, id_index+strlen("id=\""), &upload_resp);

    curl_easy_cleanup(curl);
    curl_formfree(filepost);
    curl_slist_free_all(headerlist);

  }

  if (upload_resp.buf) free(upload_resp.buf);


}
