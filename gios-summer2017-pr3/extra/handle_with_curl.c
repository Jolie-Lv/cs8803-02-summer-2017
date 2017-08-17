#include <stdlib.h>
#include <fcntl.h>
#include <curl/curl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "gfserver.h"

//Replace with an implementation of handle_with_curl and any other
//functions you may need.

typedef struct UserData {
	CURL* curl;
	gfcontext_t* ctx;
  int flag;
} UserData;

void handle_with_curl_init() {
	curl_global_init(CURL_GLOBAL_ALL);
}

void clean_up_curl() {
	curl_global_cleanup();
}

size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
	UserData* headerdata;

	headerdata = (UserData*)userdata;

	double cl;
	curl_easy_getinfo(headerdata->curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &cl);
	if(cl > 0 && headerdata->flag != -1) {
		gfs_sendheader(headerdata->ctx, GF_OK, (size_t)cl);
		headerdata->flag = -1;
	}

	return (size_t)gfs_send(headerdata->ctx, ptr, size * nmemb);
}

ssize_t handle_with_curl(gfcontext_t *ctx, char *path, void* arg){
  int res;
	CURL *curl;
	char curl_errbuf[CURL_ERROR_SIZE];

	curl = curl_easy_init();

	UserData userdata = {curl, ctx, 0};

	curl_easy_setopt(curl, CURLOPT_URL, path);
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)(&userdata));
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curl_errbuf);


	res = curl_easy_perform(curl);
	if(res != CURLE_OK) {
		size_t len = strlen(curl_errbuf);
		fprintf(stderr, "\nlibcurl: (%d) ", res);
		if(len)
			fprintf(stderr, "%s%s", curl_errbuf,
							((curl_errbuf[len - 1] != '\n') ? "\n" : ""));
		else
			fprintf(stderr, "%s\n", curl_easy_strerror(res));
	}

	curl_easy_cleanup(curl);

	if(res != CURLE_OK) {
    gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);

		return -1;
	}
	else {
		return 0;
	}
}

