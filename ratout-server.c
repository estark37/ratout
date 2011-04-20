#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <time.h>
#include <string.h>
#include <libxml/parser.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#define REQ_FILE_PATH "/Users/estark37/Dropbox/Documents/school/cs294s-2/ratout-demo/server-files"
#define GET_FILE_TREE_URL "https://www.box.net/api/1.0/rest?action=get_account_tree&api_key=<api key>&auth_token=<auth token>&folder_id=80421720&params[]=nozip"
#define UPLOAD_URL "https://upload.box.net/api/upload/<auth token>/80421720"
#define SET_DESC_URL "https://www.box.net/api/1.0/rest?action=set_description&api_key=<api key>&auth_token=<auth token>&target=file&target_id=%s&description=%s"

struct resp_buffer {
  int len;
  int size;
  char* buf;
};

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


void process_file(xmlNode* file, CURL* curl, struct resp_buffer* resp) {
  printf("Processing request %s\n", xmlGetProp(file, "file_name"));
  printf("Requesting URL %s...", xmlGetProp(file, "description"));
  curl_easy_setopt(curl, CURLOPT_URL, xmlGetProp(file, "description"));
  CURLcode res = curl_easy_perform(curl);
  printf(" done.\n");
  printf("Response from server: %s\n\n", resp->buf);

  char fname[512];
  sprintf(fname, "%s/%s", REQ_FILE_PATH, xmlGetProp(file, "file_name"));
  FILE* f = fopen(fname, "w+");
  if (f != NULL) {
    fputs(resp->buf, f);
    fclose(f);
  } else {
    printf("Could not create new file.\n");
    return;
  }

  // upload the new file

  struct curl_httppost *filepost = NULL;
  struct curl_httppost *lastptr = NULL;
  curl_formadd(&filepost, &lastptr, CURLFORM_COPYNAME, "file", CURLFORM_FILE, fname, CURLFORM_END);
  struct curl_slist *headerlist = NULL;
  headerlist = curl_slist_append(headerlist, "Expect:");
  curl_easy_setopt(curl, CURLOPT_URL, UPLOAD_URL);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerlist);
  curl_easy_setopt(curl, CURLOPT_HTTPPOST, filepost);
  printf("Uploading file... ");
  res = curl_easy_perform(curl);
  printf(" done.\n");

  char desc_url[1024];
  sprintf(desc_url, SET_DESC_URL, xmlGetProp(file, "id"), "Processed");
  curl_easy_setopt(curl, CURLOPT_URL, desc_url);
  curl_easy_setopt(curl, CURLOPT_HTTPPOST, NULL);
  printf("Setting description %s... ", desc_url);
  resp->len = 0;
  res = curl_easy_perform(curl);
  printf(" done.\n");
  printf("Description response: %s\n", resp->buf);
}


void traverse_and_find_file(xmlNode* root, CURL* curl, struct resp_buffer* resp) {
  if (!root) return;

  if (xmlStrEqual(root->name, "file") && !xmlStrEqual(xmlGetProp(root, "description"), "Processed") && !xmlStrEqual(xmlGetProp(root, "description"), "")) {
    process_file(root, curl, resp);
    return;
  }

  xmlNode* cur = NULL;
  for (cur = root->children; cur; cur = cur->next) {   
    traverse_and_find_file(cur, curl, resp);
  }
}

int main() {

  CURL *curl;
  CURLcode res;

  curl_global_init(CURL_GLOBAL_ALL);

  struct curl_httppost *filepost = NULL;
  struct curl_httppost *lastptr = NULL;

  struct resp_buffer resp;
  resp.len = 0;
  resp.size = 4096;
  resp.buf = malloc(resp.size);

  curl = curl_easy_init();
  while (curl) {
    resp.len = 0;
    curl_easy_setopt(curl, CURLOPT_URL, GET_FILE_TREE_URL);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, read_api_response);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);
    printf("Checking for new requests... ");
    res = curl_easy_perform(curl);
    printf(" done.\n");

    xmlDocPtr doc;
    doc = xmlReadMemory(resp.buf, resp.len, "noname.xml", NULL, 0);
    if (doc == NULL) {
      printf("Failed to parse XML\n");
      printf("Resp: %s\n", resp.buf);
    } else {
      traverse_and_find_file(xmlDocGetRootElement(doc), curl, &resp);
      xmlFreeDoc(doc);
    }

  }

  if (resp.buf) free(resp.buf);
  curl_easy_cleanup(curl);


}

