//  Copyright 2013 Google Inc. All Rights Reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>	/* strerror(errno) */
#include "jsmn/jsmn.h"	/* JSON */

const long long max_size = 2000;         // max length of strings
const long long N = 100;                 // number of closest words that will be shown
const long long max_w = 100;             // max length of vocabulary entries
//const float secondary_link_threshold = 0.7f;
FILE *flog;

typedef struct query_t {
	char *string;
	int search_type;
} query_t;

/*
 * JSMN
 */
static int jsoneq(const char *json, jsmntok_t *tok, const char *s)
{
	if(tok->type == JSMN_STRING && (int) strlen(s) == tok->end - tok->start &&
		strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
		return 0;
	}
	return -1;
}

/*
 * JSMN
 */
int func_jsmn_json(query_t *query, char *strin, int len)
{
	int i;
	int r;
	char *ptr;
	char buf[max_size];

	jsmn_parser p;
	jsmntok_t t[128]; // We expect no more than 128 tokens

	jsmn_init(&p);
	r = jsmn_parse(&p, strin, len, t, sizeof(t)/sizeof(t[0]));
	if(r < 0) {
		fprintf(flog, "Failed to parse JSON: %d, %s\n", r, strin);
		return 1;
	}

	/* Assume the top-level element is an object */
	if(r < 1 || t[0].type != JSMN_OBJECT) {
		fprintf(flog, "JSON object expected: %s\n", strin);
		return 2;
	}

	/* Loop over all keys of the root object */
	for (i = 1; i < r; i++) {
		// Word
		if(jsoneq(strin, &t[i], "word") == 0) {
			fprintf(flog, "Word: %.*s / ", t[i+1].end - t[i+1].start, strin + t[i+1].start);
			query->string = (char *) malloc(t[i+1].end - t[i+1].start);
			strncpy(query->string, strin + t[i+1].start, t[i+1].end - t[i+1].start);
			query->string[t[i+1].end - t[i+1].start] = '\0';
			fprintf(flog, "%s", query->string);
			++i;
		} else if(jsoneq(strin, &t[i], "stype") == 0) {
			fprintf(flog, ", stype: %.*s / ", t[i+1].end - t[i+1].start, strin + t[i+1].start);
			strncpy(buf, strin + t[i+1].start, t[i+1].end - t[i+1].start);
			buf[t[i+1].end - t[i+1].start] = '\0';
			errno = 0;
			query->search_type = strtol(buf, &ptr, 10);
			fprintf(flog, "%d\n", query->search_type);
			++i;
		// Not recognized
		} else {
			fprintf(flog, "***ERROR: Unexpected key: %.*s\n", t[i].end-t[i].start, strin + t[i].start);
			return 3;
		}
	}

	return 0;
}

int similarity() {
	
	return 0;
}

int word_analogy() {
	
	return 0;
}

int most_similar() {
	
	return 0;
}

int main(int argc, char **argv) {
	FILE *f;
	char json[max_size];
	char temp[max_size];
	char *output;
	char *bestw[N];
	char file_name[max_size], st[100][max_size];
	float dist, len, bestd[N], vec[max_size];
	long long words, size, a, b, c, d, cn, bi[100];
	long long besti[N]; // ids of found words
	float *M;
	char *vocab;
	float dot, denom_a, denom_b;
	int errors = 0;
	struct query_t query;
	query.search_type = 0;
	
	output = (char *)malloc(N * N * max_size * sizeof(char));
	
	// Start logging to a file
	if(access("distanceio.log", W_OK)) {
		fprintf(stderr, "ERROR: Log file 'distanceio.log' is not writable!");
		return -1;
	}
	flog = fopen("distanceio.log", "a+");
	
	if (argc < 2) {
		fprintf(stderr, "Usage: ./distance <FILE>\nwhere FILE contains word projections in the BINARY FORMAT\n");
		return -1;
	}
	
	// Load vectors.bin
	strcpy(file_name, argv[1]);
	f = fopen(file_name, "rb");
	if (f == NULL) {
		fprintf(flog, "Input file not found\n");
		return -1;
	}
	fscanf(f, "%lld", &words);
	fscanf(f, "%lld", &size);
	vocab = (char *)malloc((long long)words * max_w * sizeof(char));
	for (a = 0; a < N; a++) bestw[a] = (char *)malloc(max_size * sizeof(char));
	M = (float *)malloc((long long)words * (long long)size * sizeof(float));
	if (M == NULL) {
		fprintf(flog, "Cannot allocate memory: %lld MB, %lld, %lld\n", (long long)words * size * sizeof(float) / 1048576, words, size);
		return -1;
	}
	for (b = 0; b < words; b++) {
		a = 0;
		while (1) {
			vocab[b * max_w + a] = fgetc(f);
			//sprintf(temp, "%c", vocab[b * max_w + a]);
			//printf(temp);
			if (feof(f) || (vocab[b * max_w + a] == ' ')) break;
			if ((a < max_w) && (vocab[b * max_w + a] != '\n')) a++;
		}
		vocab[b * max_w + a] = 0;
		//for (a = 0; a < size; a++) fread(&M[a + b * size], sizeof(float), 1, f);
		fread(&M[b * size], sizeof(float), size, f);
		len = 0;
		for (a = 0; a < size; a++) len += M[a + b * size] * M[a + b * size];
		len = sqrt(len);
		for (a = 0; a < size; a++) M[a + b * size] /= len;
	}
	fclose(f);
	
	// Read STDIN
	a = 0;
	while (1) {
		json[a] = fgetc(stdin);
		if ((json[a] == '\n') || (a >= max_size - 1)) {
			json[a] = 0;
			break;
		}
		a++;
	}
	
	// Parse incoming JSON string
	if(func_jsmn_json(&query, json, a)) {
		sprintf(temp, "Bad JSON string: %s\n", json);
		fprintf(flog, json);
		//exit(1);
		return 1;
	}

	cn = 0;
	b = 0;
	c = 0;
	while (1) {
		st[cn][b] = query.string[c];
		b++;
		c++;
		st[cn][b] = 0;
		if (query.string[c] == 0) break;
		if (query.string[c] == ' ') {
			cn++;
			b = 0;
			c++;
		}
	}
	cn++;
	
	//memset(output, '\0', sizeof(output));
	sprintf(temp, "{\"stype\":%d", query.search_type);
	strcpy(output, temp);
	
	strcat(output, ",\"words\":[");
	for (a = 0; a < cn; a++) {
		if(a) strcat(output, ",");
		for (b = 0; b < words; b++) if (!strcmp(&vocab[b * max_w], st[a])) break;
		if (b == words) b = -1;
		if (b == -1) ++errors;
		bi[a] = b;
		// Word and its position in vocabulary
		sprintf(temp, "{\"word\":\"%s\",\"id\":%lld}", st[a], bi[a]);
		strcat(output, temp);
		if (b == -1) {
			fprintf(flog, "Out of dictionary word: '%s' (%lld)\n", st[a], bi[a]);
			break;
		}
	}
	strcat(output, "]");
	
	if(!errors) {
		if(query.search_type == 1) {
			// Cosine similarity of 2 vectors
			if (cn < 2) {
				fprintf(flog, "Only %lld words were entered. Two words are needed at the input to perform the calculation!\n", cn);
				strcat(output, ",\"error\":-1");
				strcat(output, "}\r\n");
				printf(output);
				return 1;
			}
			
			// Word1 + vector
			//printf("[%lld] %s ", bi[0], &vocab[bi[0] * max_w]);
			//sprintf(temp, "[%lld] %s ", bi[0], &vocab[bi[0] * max_w]);
			//strcat(output, temp);
			//printf(temp);
			//for(a = 0; a < size; a++) {
				//printf("%f ", M[a + bi[0] * size]);
				//sprintf(temp, "%f ", M[a + bi[0] * size]);
				//printf(temp);
			//}
			
			// Word2 + vector
			//printf("\n\n[%lld] %s ", bi[1], &vocab[bi[1] * max_w]);
			//sprintf(temp, "\n\n[%lld] %s ", bi[1], &vocab[bi[1] * max_w]);
			//printf(temp);
			//for(a = 0; a < size; a++) {
				//printf("%f ", M[a + bi[1] * size]);
				//sprintf(temp, "%f ", M[a + bi[1] * size]);
				//printf(temp);
			//}
			
			dot = 0.f;
			denom_a = 0.f;
			denom_b = 0.f;
			for(a = 0; a < size; a++) {
				dot += M[a + bi[0] * size] * M[a + bi[1] * size];
				denom_a += M[a + bi[0] * size] * M[a + bi[0] * size];
				denom_b += M[a + bi[1] * size] * M[a + bi[1] * size];
			}
			dot = dot / (sqrt(denom_a) * sqrt(denom_b));
			sprintf(temp, ",\"cosine\": %f", dot);
			strcat(output, temp);
			
		} else if(query.search_type == 2) {
			// Word analogy
			if (cn < 3) {
				fprintf(flog, "Only %lld words were entered. Three words are needed at the input to perform the calculation!\n", cn);
				strcat(output, ",\"error\":-1");
				strcat(output, "}\r\n");
				printf(output);
				return 1;
			}
			for (a = 0; a < size; a++) vec[a] = M[a + bi[1] * size]     - M[a + bi[0] * size]     + M[a + bi[2] * size];
			len = 0;
			for (a = 0; a < size; a++) len += vec[a] * vec[a];
			len = sqrt(len);
			for (a = 0; a < size; a++) vec[a] /= len;
			for (a = 0; a < N; a++) bestd[a] = 0;
			for (a = 0; a < N; a++) bestw[a][0] = 0;
			for (c = 0; c < words; c++) {
				if (c == bi[0]) continue;
				if (c == bi[1]) continue;
				if (c == bi[2]) continue;
				a = 0;
				for (b = 0; b < cn; b++) if (bi[b] == c) a = 1;
				if (a == 1) continue;
				dist = 0;
				for (a = 0; a < size; a++) dist += vec[a] * M[a + c * size];
				for (a = 0; a < N; a++) {
					if (dist > bestd[a]) {
						for (d = N - 1; d > a; d--) {
							bestd[d] = bestd[d - 1];
							strcpy(bestw[d], bestw[d - 1]);
						}
						bestd[a] = dist;
						strcpy(bestw[a], &vocab[c * max_w]);
						break;
					}
				}
			}
			strcat(output, ",\"word_analogy\":[");
			for (a = 0; a < N; a++) {
				if(a) strcat(output, ",");
				// cosine_distance + words
				sprintf(temp, "{\"cd\":%f,\"w\":\"%s\"}", bestd[a], bestw[a]);
				strcat(output, temp);
			}
			strcat(output, "]");
		} else {
			// Find similar words
			for (a = 0; a < size; a++) vec[a] = 0;
			for (b = 0; b < cn; b++) {
				if (bi[b] == -1) continue;
				for (a = 0; a < size; a++) vec[a] += M[a + bi[b] * size];
			}
			len = 0;
			for (a = 0; a < size; a++) len += vec[a] * vec[a];
			len = sqrt(len);
			for (a = 0; a < size; a++) vec[a] /= len;
			for (a = 0; a < N; a++) bestd[a] = -1;
			for (a = 0; a < N; a++) bestw[a][0] = 0;
			for (c = 0; c < words; c++) {
				a = 0;
				for (b = 0; b < cn; b++) if (bi[b] == c) a = 1;
				if (a == 1) continue;
				dist = 0;
				for (a = 0; a < size; a++) dist += vec[a] * M[a + c * size];
				for (a = 0; a < N; a++) {
					if (dist > bestd[a]) {
						for (d = N - 1; d > a; d--) {
							bestd[d] = bestd[d - 1];
							strcpy(bestw[d], bestw[d - 1]);
						}
						bestd[a] = dist;
						strcpy(bestw[a], &vocab[c * max_w]);
						break;
					}
				}
			}
			
			// Find ids of these similar words
			for (b = 0; b < N; b++) {
				for (d = 0; d < words; d++) if (!strcmp(&vocab[d * max_w], bestw[b])) break;
				besti[b] = d;
				//printf(bestw[b]);
			}
			
			strcat(output, ",\"similar_words\":[");
			for (a = 0; a < N; a++) {
				if(a) strcat(output, ",");
				// cosine_distance + words
				sprintf(temp, "{\"cd\":%f,\"id\":%lld,\"w\":\"%s\"}", bestd[a], besti[a], bestw[a]);
				strcat(output, temp);
			}
			strcat(output, "]");
			
			// Count cosine similarity (NOT distance) for each pair in our list
			strcat(output, ",\"pairs_similarities\":[");
			for (b = 0; b < N; b++) {
				if(b) strcat(output, ",");
				//for (d = 0; d < words; d++) if (!strcmp(&vocab[d * max_w], bestw[b])) break;
				//printf(bestw[b]);
				for (c = 0; c < N; c++) {
					if(c) strcat(output, ",");
					//for (e = 0; e < words; e++) if (!strcmp(&vocab[e * max_w], bestw[c])) break;
					//printf(bestw[c]);
					dot = 0.f;
					denom_a = 0.f;
					denom_b = 0.f;
					for(a = 0; a < size; a++) {
						dot += M[a + besti[b] * size] * M[a + besti[c] * size];
						denom_a += M[a + besti[b] * size] * M[a + besti[b] * size];
						denom_b += M[a + besti[c] * size] * M[a + besti[c] * size];
					}
					dot = dot / (sqrt(denom_a) * sqrt(denom_b));
					//if(dot > secondary_link_threshold) {
					sprintf(temp, "{\"cd\":%f,\"id1\":%lld,\"id2\":\"%lld\"}", dot, besti[b], besti[c]);
					strcat(output, temp);
					//}
				}
				//sprintf(temp, "\n[%lld (%lld %lld)] %s [%lld (%lld %lld)] %s : %f", b, d, besti[b], bestw[b], c, e, besti[c], bestw[c], dot);
				//printf(temp);
			}
			strcat(output, "]");
		}
	}
	strcat(output, "}\r\n");
	printf(output);
	return 0;
}
